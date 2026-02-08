#include "DataExchanger.h"
#include "Logger.h"
#include <EEPROM.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

// Global pointer to the instance for the static MQTT callback
static DataExchanger* _exchangerInstance = nullptr;

void _mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_exchangerInstance) {
        _exchangerInstance->handleMqttMessage(topic, payload, length);
    }
}

struct DataExchangerConfig {
    unsigned long interval;
    char httpUrl[128];
    char mqttUrl[128];
    uint32_t magic;
};

DataExchanger::DataExchanger(const char* name, const char* deviceId, unsigned long interval, const char* httpUrl, const char* mqttUrl, WifiConnection& wifi, int eepromOffset)
    : _name(name), _deviceId(deviceId), _eepromOffset(eepromOffset), _interval(interval), _httpUrl(httpUrl), _mqttUrl(mqttUrl), _wifi(wifi), _lastExchangeTime(0), _lastMqttConnectionAttempt(0), _doc(2048) {
    
    _exchangerInstance = this;
    _mqttClient.setClient(_wifiClient);
    _mqttClient.setCallback(_mqttCallback);
    _mqttClient.setBufferSize(2560); // Increase buffer for JSON payloads
}

void DataExchanger::begin() {
    loadConfig();
}

void DataExchanger::loadConfig() {
    DataExchangerConfig config;
    EEPROM.get(_eepromOffset, config);
    
    if (config.magic == 0xCAFEBABE) {
        _interval = config.interval;
        // Ensure null termination
        config.httpUrl[sizeof(config.httpUrl) - 1] = 0;
        if (strlen(config.httpUrl) > 0) {
            _httpUrl = String(config.httpUrl);
        }
        config.mqttUrl[sizeof(config.mqttUrl) - 1] = 0;
        if (strlen(config.mqttUrl) > 0) {
            _mqttUrl = String(config.mqttUrl);
        }
    }
}

void DataExchanger::saveConfig() {
    DataExchangerConfig config;
    config.interval = _interval;
    strncpy(config.httpUrl, _httpUrl.c_str(), sizeof(config.httpUrl));
    config.httpUrl[sizeof(config.httpUrl) - 1] = 0;
    strncpy(config.mqttUrl, _mqttUrl.c_str(), sizeof(config.mqttUrl));
    config.mqttUrl[sizeof(config.mqttUrl) - 1] = 0;
    config.magic = 0xCAFEBABE;
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

void DataExchanger::addProvider(JsonProvider* provider) {
    _providers.push_back(provider);
}

bool DataExchanger::exchange(bool force, const char* reason) {
    unsigned long currentMillis = millis();

    if (_mqttUrl.length() > 0) {
        // Ensure MQTT connection
        if (!_mqttClient.connected()) {
            // Attempt connection if WiFi is connected and we haven't tried too recently (5s retry)
            if (_wifi.isConnected() && (force || (currentMillis - _lastMqttConnectionAttempt >= 5000))) {
                _lastMqttConnectionAttempt = currentMillis;
                
                // Parse URL for server (and optional port)
                String server = _mqttUrl;
                int port = 1883;

                // Strip scheme if present (e.g. mqtt://)
                int schemeEnd = server.indexOf("://");
                if (schemeEnd != -1) {
                    server = server.substring(schemeEnd + 3);
                }

                int colonIndex = server.indexOf(':');
                if (colonIndex != -1) {
                    String portStr = server.substring(colonIndex + 1);
                    server = server.substring(0, colonIndex);
                    port = portStr.toInt();
                }

                _mqttClient.setServer(server.c_str(), port);
                String clientId = _deviceId;
                if (_mqttClient.connect(clientId.c_str())) {
                    Log.info("MQTT Connected");
                    String topic = String("device/") + _deviceId + "/command";
                    _mqttClient.subscribe(topic.c_str());
                } else {
                    Log.error("MQTT Connect failed");
                }
            }
        } else {
            _mqttClient.loop();
        }
    }

    // Check if the interval has elapsed
    if (!force && (currentMillis - _lastExchangeTime < _interval)) {
        return true;
    }

    _lastExchangeTime = currentMillis;

    // Create JSON payload
    _doc.clear();
    JsonObject root = _doc.to<JsonObject>();

    addToJson(root);

    // Add the reason/trigger for the transmission to the exchanger data.
    if (root.containsKey(_name)) {
        JsonObject nested = root[_name];
        const char* actualReason = (reason && *reason) ? reason : (force ? "forced" : "scheduled");
        nested["trigger"] = actualReason;
    }

    // Add providers to JSON
    for (JsonProvider* provider : _providers) {
        provider->addToJson(root);
    }

    _requestBody = "";
    serializeJson(_doc, _requestBody);

    Log.info("Payload size:");
    Log.info(String(_requestBody.length()).c_str());

    if (_mqttUrl.length() > 0) {
        String topic = String("device/") + _deviceId + "/data";
        if (_mqttClient.publish(topic.c_str(), _requestBody.c_str())) {
            Log.info(("MQTT Publish successful: " + topic).c_str());
            _pendingAck = "";
            return true;
        } else {
            Log.error("MQTT Publish failed");
        }
    }

    // HTTP Fallback
    if (_httpUrl.length() == 0) {
        return false;
    }

    String response = _wifi.postJson(_httpUrl.c_str(), _requestBody);

    if (response.length() > 0) {
        Log.info("DataExchanger: Response:");
        Log.info(response.c_str());
        _pendingAck = "";

        // Parse the response
        StaticJsonDocument<1024> responseDoc;
        DeserializationError error = deserializeJson(responseDoc, response);

        if (!error) {
            // Call each of the provider's processJson methods so they can act on commands that may have come back.
            JsonObject root = responseDoc.as<JsonObject>();
            
            processJson(root);

            for (JsonProvider* provider : _providers) {
                provider->processJson(root);
            }
        } else {
            Log.error("DataExchanger: Failed to parse response JSON.");
        }
        return true;
    }
    return false;
}

void DataExchanger::addToJson(JsonObject& doc) {
    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = "DataExchanger";
    nested["interval"] = _interval;
    nested["httpUrl"] = _httpUrl;
    nested["mqttUrl"] = _mqttUrl;
    if (_pendingAck.length() > 0) {
        doc["_ack"] = _pendingAck;
    }
}

void DataExchanger::processJson(JsonObject& doc) {
    if (doc.containsKey("_ack")) {
        _pendingAck = doc["_ack"].as<String>();
    }

    if (doc.containsKey(_name)) {
        JsonObject config = doc[_name];

        if (config.containsKey("setInterval")) {
            // Explicitly extract as JsonVariant to handle both number and string input types safely
            JsonVariant intervalVar = config["setInterval"];
            unsigned long newInterval = intervalVar.as<unsigned long>();
            if (newInterval >= 10000 && newInterval <= 600000 && newInterval != _interval) {
                _interval = newInterval;
                saveConfig();
                Log.info("DataExchanger: Interval updated");
            }
        }

        if (config.containsKey("setHttpUrl")) {
            String newUrl = config["setHttpUrl"].as<String>();
            if (newUrl.length() < 128 && newUrl != _httpUrl) {
                _httpUrl = newUrl;
                saveConfig();
                Log.info("DataExchanger: HTTP URL updated");
            }
        }

        if (config.containsKey("setMqttUrl")) {
            String newUrl = config["setMqttUrl"].as<String>();
            if (newUrl.length() < 128 && newUrl != _mqttUrl) {
                _mqttUrl = newUrl;
                saveConfig();
                Log.info("DataExchanger: MQTT URL updated");
            }
        }
    }
}

const String& DataExchanger::getName() {
    return _name;
}

void DataExchanger::handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    Log.info(("MQTT Message received: " + String(topic)).c_str());

    // Create a null-terminated string from the payload for logging
    char* payloadStr = (char*)malloc(length + 1);
    memcpy(payloadStr, payload, length);
    payloadStr[length] = '\0';
    Log.info(payloadStr);
    free(payloadStr);

    StaticJsonDocument<1024> responseDoc;
    // Cast payload to (const byte*) to force ArduinoJson to copy the data.
    // Otherwise, it uses pointers to the MQTT buffer, which gets overwritten when we publish the Ack.
    DeserializationError error = deserializeJson(responseDoc, (const byte*)payload, length);

    if (!error) {
        JsonObject root = responseDoc.as<JsonObject>();
        processJson(root);

        for (JsonProvider* provider : _providers) {
            provider->processJson(root);
        }

        if (_pendingAck.length() > 0) {
            exchange(true, "commandAck");
        }
    } else {
        Log.error("DataExchanger: Failed to parse MQTT message.");
    }
}
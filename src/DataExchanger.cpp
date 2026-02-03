#include "DataExchanger.h"
#include "Logger.h"
#include <EEPROM.h>

struct DataExchangerConfig {
    unsigned long interval;
    char url[128];
    uint32_t magic;
};

DataExchanger::DataExchanger(const char* name, const char* deviceId, unsigned long interval, const char* url, WifiConnection& wifi, int eepromOffset)
    : _name(name), _deviceId(deviceId), _eepromOffset(eepromOffset), _interval(interval), _url(url), _wifi(wifi), _lastExchangeTime(0), _doc(1024) {
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
        config.url[sizeof(config.url) - 1] = 0;
        if (strlen(config.url) > 0) {
            _url = String(config.url);
        }
    }
}

void DataExchanger::saveConfig() {
    DataExchangerConfig config;
    config.interval = _interval;
    strncpy(config.url, _url.c_str(), sizeof(config.url));
    config.url[sizeof(config.url) - 1] = 0;
    config.magic = 0xCAFEBABE;
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

void DataExchanger::addProvider(JsonProvider* provider) {
    _providers.push_back(provider);
}

void DataExchanger::exchange(bool force, const char* reason) {
    unsigned long currentMillis = millis();

    // Check if the interval has elapsed
    if (!force && (currentMillis - _lastExchangeTime < _interval)) {
        return;
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

    for (JsonProvider* provider : _providers) {
        provider->addToJson(root);
    }

    _requestBody = "";
    serializeJson(_doc, _requestBody);

    String response = _wifi.postJson(_url.c_str(), _requestBody);

    if (response.length() > 0) {
        Log.info("DataExchanger: Response:");
        Log.info(response.c_str());

        // Parse the response
        StaticJsonDocument<512> responseDoc;
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
    }
}

void DataExchanger::addToJson(JsonObject& doc) {
    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = "DataExchanger";
    nested["interval"] = _interval;
    nested["url"] = _url;
}

void DataExchanger::processJson(JsonObject& doc) {
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

        if (config.containsKey("setUrl")) {
            String newUrl = config["setUrl"].as<String>();
            if (newUrl.length() > 0 && newUrl != _url && newUrl.length() < 128) {
                _url = newUrl;
                saveConfig();
                Log.info("DataExchanger: URL updated");
            }
        }
    }
}

const String& DataExchanger::getName() {
    return _name;
}
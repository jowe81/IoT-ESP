#ifndef DATA_EXCHANGER_H
#define DATA_EXCHANGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include "JsonProvider.h"
#include "WifiConnection.h"

class DataExchanger : public JsonProvider {
public:
    DataExchanger(const char* name, const char* deviceId, unsigned long interval, const char* httpUrl, const char* mqttUrl, WifiConnection& wifi, int eepromOffset);
    void begin();
    void addProvider(JsonProvider* provider);
    bool exchange(bool force = false, const char* reason = "");
    void addToJson(JsonObject& doc) override;
    void processJson(JsonObject& doc) override;
    void handleMqttMessage(char* topic, byte* payload, unsigned int length);

private:
    String _name;
    String _deviceId;
    int _eepromOffset;
    unsigned long _interval;
    String _httpUrl;
    String _mqttUrl;
    WifiConnection& _wifi;
    std::vector<JsonProvider*> _providers;
    unsigned long _lastExchangeTime;
    unsigned long _lastMqttConnectionAttempt;
    DynamicJsonDocument _doc;
    String _requestBody;
    String _pendingReason;
    String _pendingAck;
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    void loadConfig();
    void saveConfig();
    const String& getName();
};

#endif
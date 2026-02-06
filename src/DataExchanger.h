#ifndef DATA_EXCHANGER_H
#define DATA_EXCHANGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

#include "JsonProvider.h"
#include "WifiConnection.h"

class DataExchanger : public JsonProvider {
public:
    DataExchanger(const char* name, const char* deviceId, unsigned long interval, const char* url, WifiConnection& wifi, int eepromOffset);
    void begin();
    void addProvider(JsonProvider* provider);
    bool exchange(bool force = false, const char* reason = "");
    void addToJson(JsonObject& doc) override;
    void processJson(JsonObject& doc) override;

private:
    String _name;
    String _deviceId;
    int _eepromOffset;
    unsigned long _interval;
    String _url;
    WifiConnection& _wifi;
    std::vector<JsonProvider*> _providers;
    unsigned long _lastExchangeTime;
    DynamicJsonDocument _doc;
    String _requestBody;
    String _pendingReason;
    void loadConfig();
    void saveConfig();
    const String& getName();
};

#endif
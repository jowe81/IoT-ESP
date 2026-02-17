#ifndef JSON_PROVIDER_H
#define JSON_PROVIDER_H

#include <Arduino.h>
#include <ArduinoJson.h>

class JsonProvider {
public:
    virtual void addToJson(JsonArray& doc) = 0;
    virtual void processJson(JsonObject& doc) {}
    virtual ~JsonProvider() {}
};

#endif
#ifndef TEMPERATURE_READER_H
#define TEMPERATURE_READER_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "JsonProvider.h"

class TemperatureReader : public JsonProvider {
    private:
        OneWire _oneWire;
        DallasTemperature _sensors;
        String _name;
        int _sensorIndex;
        float _lastGoodTemp;
        int _badReadingCount;
        int _maxBadReadings;

    public:
        TemperatureReader(int pin, String name, int sensorIndex = 0);
        void begin();
        float getTemperature();
        void addToJson(JsonObject& doc) override;
        const String& getName();
};

#endif

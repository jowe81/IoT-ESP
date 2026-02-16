#ifndef DS18B20_H
#define DS18B20_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Device.h"
#include <EEPROM.h>

class DS18B20 : public Device {
    private:
        OneWire _oneWire;
        DallasTemperature _sensors;
        String _name;
        int _sensorIndex;
        float _lastGoodTemp;
        int _badReadingCount;
        int _maxBadReadings;
        unsigned long _lastUpdateTime;
        float _offset;
        int _eepromOffset;

        struct Config {
            float offset;
            uint32_t magic;
        };
        void loadConfig();
        void saveConfig();

    public:
        DS18B20(int pin, String name, int sensorIndex = 0, int eepromOffset = -1);
        void begin();
        void update() override;
        float getTemperature();
        void addToJson(JsonObject& doc) override;
        void processJson(JsonObject& doc) override;
        const String& getName();
};

#endif
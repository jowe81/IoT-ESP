#ifndef BME280_H
#define BME280_H

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Device.h"
#include <EEPROM.h>

class BME280Reader : public Device {
    private:
        Adafruit_BME280 _bme;
        String _name;
        uint8_t _address;
        unsigned long _interval;
        unsigned long _lastUpdateTime;
        int _eepromOffset;
        
        float _temperature;
        float _humidity;
        float _pressure;
        float _tempSum;
        float _humSum;
        float _pressSum;
        int _readingsCount;
        bool _available;

        struct Config {
            unsigned long interval;
            uint32_t magic;
        };

        void loadConfig();
        void saveConfig();

    public:
        BME280Reader(String name, uint8_t address = 0x76, unsigned long interval = 60000, int eepromOffset = -1);
        void begin() override;
        void update() override;
        void addToJson(JsonArray& doc) override;
        void processJson(JsonObject& doc) override;
        const String& getName() override;
};

#endif
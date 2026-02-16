#ifndef SHT31_H
#define SHT31_H

#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include "Device.h"
#include <EEPROM.h>

class SHT31 : public Device {
    private:
        Adafruit_SHT31 _sht;
        String _name;
        uint8_t _address;
        unsigned long _interval;
        unsigned long _lastUpdateTime;
        int _eepromOffset;
        
        float _temperature;
        float _humidity;
        
        float _tempSum;
        float _humSum;
        int _readingsCount;
        bool _available;
        bool _heaterOn;

        struct Config {
            unsigned long interval;
            bool heaterOn;
            uint32_t magic;
        };

        void loadConfig();
        void saveConfig();

    public:
        SHT31(String name, uint8_t address = 0x44, unsigned long interval = 60000, int eepromOffset = -1);
        void begin() override;
        void update() override;
        void addToJson(JsonObject& doc) override;
        void processJson(JsonObject& doc) override;
        const String& getName() override;
};

#endif
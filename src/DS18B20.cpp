#include "DS18B20.h"
#include <ArduinoJson.h>

DS18B20::DS18B20(int pin, String name, int sensorIndex, int eepromOffset) 
    : _oneWire(pin), _sensors(&_oneWire), _name(name), _sensorIndex(sensorIndex), _lastGoodTemp(NAN), _badReadingCount(0), _maxBadReadings(0), _lastUpdateTime(0), _offset(0.0), _eepromOffset(eepromOffset) {
}

void DS18B20::begin() {
    if (_eepromOffset >= 0) {
        loadConfig();
    }
    _sensors.begin();
    // Ensure the first update happens immediately
    _lastUpdateTime = millis() - 60000;
}

void DS18B20::update() {
    if (millis() - _lastUpdateTime >= 60000) {
        _lastUpdateTime = millis();
        _sensors.requestTemperatures();
        float tempC = _sensors.getTempCByIndex(_sensorIndex);
        
        if (tempC >= -70 && tempC <= 70) {
            _lastGoodTemp = tempC + _offset;
            _badReadingCount = 0;
        } else {
            _badReadingCount++;
            if (_badReadingCount > _maxBadReadings) {
                _maxBadReadings = _badReadingCount;
            }
        }
    }
}

float DS18B20::getTemperature() {
    update();
    if (_badReadingCount <= 5 && !isnan(_lastGoodTemp)) {
        return _lastGoodTemp;
    }
    return NAN;
}

void DS18B20::addToJson(JsonObject& doc) {
    float tempC = getTemperature();
    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = "DS18B20";
    nested["tempC"] = tempC;
    nested["tempF"] = DallasTemperature::toFahrenheit(tempC);
    nested["maxBadReadings"] = _maxBadReadings;
    nested["tempCOffset"] = serialized(String(_offset, 2));
    nested["isStale"] = (_badReadingCount > 0);
}

void DS18B20::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        JsonObject config = doc[_name];
        if (config.containsKey("setTempCOffset")) {
            float newOffset = config["setTempCOffset"].as<float>();
            if (newOffset != _offset) {
                _offset = newOffset;
                saveConfig();
                // Invalidate last reading so next update reflects the offset immediately
                _lastUpdateTime = millis() - 60000; 
            }
        }
    }
}

void DS18B20::loadConfig() {
    Config config;
    EEPROM.get(_eepromOffset, config);
    if (config.magic == 0x18B2018) {
        _offset = config.offset;
    }
}

void DS18B20::saveConfig() {
    if (_eepromOffset < 0) return;
    Config config = { _offset, 0x18B2018 };
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

const String& DS18B20::getName() {
    return _name;
}
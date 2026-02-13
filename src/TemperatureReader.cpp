#include "TemperatureReader.h"
#include <ArduinoJson.h>

TemperatureReader::TemperatureReader(int pin, String name, int sensorIndex) 
    : _oneWire(pin), _sensors(&_oneWire), _name(name), _sensorIndex(sensorIndex), _lastGoodTemp(NAN), _badReadingCount(0), _maxBadReadings(0), _lastUpdateTime(0) {
}

void TemperatureReader::begin() {
    _sensors.begin();
    // Ensure the first update happens immediately
    _lastUpdateTime = millis() - 60000;
}

void TemperatureReader::update() {
    if (millis() - _lastUpdateTime >= 60000) {
        _lastUpdateTime = millis();
        _sensors.requestTemperatures();
        float tempC = _sensors.getTempCByIndex(_sensorIndex);
        
        if (tempC >= -70 && tempC <= 70) {
            _lastGoodTemp = tempC;
            _badReadingCount = 0;
        } else {
            _badReadingCount++;
            if (_badReadingCount > _maxBadReadings) {
                _maxBadReadings = _badReadingCount;
            }
        }
    }
}

float TemperatureReader::getTemperature() {
    update();
    if (_badReadingCount <= 5 && !isnan(_lastGoodTemp)) {
        return _lastGoodTemp;
    }
    return NAN;
}

void TemperatureReader::addToJson(JsonObject& doc) {
    float tempC = getTemperature();
    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = "TemperatureReader";
    nested["tempC"] = tempC;
    nested["tempF"] = DallasTemperature::toFahrenheit(tempC);
    nested["maxBadReadings"] = _maxBadReadings;
    nested["isStale"] = (_badReadingCount > 0);
}

const String& TemperatureReader::getName() {
    return _name;
}

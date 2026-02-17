#include "SHT31.h"
#include "Logger.h"

SHT31::SHT31(String name, uint8_t address, unsigned long interval, int eepromOffset) 
    : _name(name), _address(address), _interval(interval), _lastUpdateTime(0), _eepromOffset(eepromOffset),
      _temperature(NAN), _humidity(NAN),
      _tempSum(0), _humSum(0), _readingsCount(0), _available(false), _heaterOn(false), _tempOffset(0.0), _humOffset(0.0) {
}

void SHT31::begin() {
    if (_eepromOffset >= 0) {
        loadConfig();
    }

    // Initialize SHT31
    bool found = _sht.begin(_address);
    
    if (found) {
        _available = true;
        Log.info(("SHT31 " + _name + " found at 0x" + String(_address, HEX)).c_str());
        _sht.heater(_heaterOn);
    } else {
        Log.error(("SHT31 " + _name + " not found at 0x" + String(_address, HEX)).c_str());
    }
    
    // Force immediate update on next loop
    _lastUpdateTime = millis() - _interval;
}

void SHT31::loadConfig() {
    Config config;
    EEPROM.get(_eepromOffset, config);
    // Magic number to validate EEPROM data
    if (config.magic == 0xDEADBEE1) {
        if (config.interval >= 1000) {
            _interval = config.interval;
        }
        _heaterOn = config.heaterOn;
        _tempOffset = config.tempOffset;
        _humOffset = config.humOffset;
    }
}

void SHT31::saveConfig() {
    if (_eepromOffset < 0) return;
    Config config = { _interval, _heaterOn, _tempOffset, _humOffset, 0xDEADBEE1 };
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

void SHT31::update() {
    if (!_available) return;

    if (millis() - _lastUpdateTime >= _interval) {
        _lastUpdateTime = millis();
        
        float t = _sht.readTemperature();
        float h = _sht.readHumidity();

        if (!isnan(t) && !isnan(h)) {
            t += _tempOffset;
            h += _humOffset;
            
            _temperature = t;
            _humidity = h;
            _tempSum += t;
            _humSum += h;
            _readingsCount++;
        }
    }
}

void SHT31::addToJson(JsonArray& doc) {
    JsonObject nested = doc.createNestedObject();
    nested["type"] = "Sensor";
    nested["subtype"] = "SHT31";
    nested["name"] = _name;
    nested["interval"] = _interval;
    
    if (_available) {
        nested["heater"] = _heaterOn;
        nested["tempCOffset"] = serialized(String(_tempOffset, 2));
        nested["humOffset"] = serialized(String(_humOffset, 2));

        float t = _temperature;
        float h = _humidity;

        if (_readingsCount > 0) {
            t = _tempSum / _readingsCount;
            h = _humSum / _readingsCount;
            _tempSum = 0; _humSum = 0; _readingsCount = 0;
        }

        if (!isnan(t)) nested["tempC"] = t;
        if (!isnan(h)) nested["humidity"] = h;
    } else {
        nested["available"] = false;
        nested["error"] = "Sensor not found";
    }
}

void SHT31::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        JsonObject config = doc[_name];
        bool changed = false;

        if (config.containsKey("setInterval")) {
            unsigned long newInterval = config["setInterval"].as<unsigned long>();
            if (newInterval >= 1000 && newInterval != _interval) {
                _interval = newInterval;
                changed = true;
            }
        }

        if (config.containsKey("setHeater")) {
            bool newHeater = config["setHeater"].as<bool>();
            if (newHeater != _heaterOn) {
                _heaterOn = newHeater;
                if (_available) _sht.heater(_heaterOn);
                changed = true;
            }
        }

        if (config.containsKey("setTempCOffset")) {
            _tempOffset = config["setTempCOffset"].as<float>();
            changed = true;
        }

        if (config.containsKey("setHumOffset")) {
            _humOffset = config["setHumOffset"].as<float>();
            changed = true;
        }

        if (changed) saveConfig();
    }
}

const String& SHT31::getName() {
    return _name;
}
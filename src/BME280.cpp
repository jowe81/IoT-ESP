#include "BME280.h"
#include "Logger.h"

BME280Reader::BME280Reader(String name, uint8_t address, unsigned long interval, int eepromOffset) 
    : _name(name), _address(address), _interval(interval), _lastUpdateTime(0), _eepromOffset(eepromOffset),
      _temperature(NAN), _humidity(NAN), _pressure(NAN),
      _tempSum(0), _humSum(0), _pressSum(0), _readingsCount(0), _available(false) {
}

void BME280Reader::begin() {
    if (_eepromOffset >= 0) {
        loadConfig();
    }

    // Initialize BME280
    // Note: Adafruit_BME280::begin() returns true on success
    bool found = _bme.begin(_address);
    
    // If not found, try the alternative address (swap 0x76 <-> 0x77)
    if (!found && (_address == 0x76 || _address == 0x77)) {
        uint8_t altAddress = (_address == 0x76) ? 0x77 : 0x76;
        Log.warn(("BME280 " + _name + " not found at 0x" + String(_address, HEX) + ", trying 0x" + String(altAddress, HEX)).c_str());
        if (_bme.begin(altAddress)) {
            _address = altAddress;
            found = true;
        }
    }

    if (found) {
        _available = true;
        Log.info(("BME280 " + _name + " found at 0x" + String(_address, HEX)).c_str());
    } else {
        Log.error(("BME280 " + _name + " not found").c_str());
    }
    
    // Force immediate update on next loop
    _lastUpdateTime = millis() - _interval;
}

void BME280Reader::loadConfig() {
    Config config;
    EEPROM.get(_eepromOffset, config);
    // Magic number to validate EEPROM data
    if (config.magic == 0xCAFEBABE) {
        if (config.interval >= 1000) {
            _interval = config.interval;
        }
    }
}

void BME280Reader::saveConfig() {
    if (_eepromOffset < 0) return;
    Config config = { _interval, 0xCAFEBABE };
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

void BME280Reader::update() {
    if (!_available) return;

    if (millis() - _lastUpdateTime >= _interval) {
        _lastUpdateTime = millis();
        
        float t = _bme.readTemperature();
        float p = _bme.readPressure() / 100.0F; // Convert Pa to hPa
        float h = _bme.readHumidity();

        if (!isnan(t) && !isnan(p) && !isnan(h)) {
            _temperature = t;
            _pressure = p;
            _humidity = h;
            _tempSum += t;
            _pressSum += p;
            _humSum += h;
            _readingsCount++;
        }
    }
}

void BME280Reader::addToJson(JsonObject& doc) {
    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = "BME280Reader";
    nested["available"] = _available;
    nested["interval"] = _interval;
    
    if (_available) {
        float t = _temperature;
        float h = _humidity;
        float p = _pressure;

        if (_readingsCount > 0) {
            t = _tempSum / _readingsCount;
            h = _humSum / _readingsCount;
            p = _pressSum / _readingsCount;
            _tempSum = 0; _humSum = 0; _pressSum = 0; _readingsCount = 0;
        }

        if (!isnan(t)) nested["temperature"] = t;
        if (!isnan(h)) nested["humidity"] = h;
        if (!isnan(p)) nested["pressure"] = p;
    } else {
        nested["error"] = "Sensor not found";
    }
}

void BME280Reader::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        JsonObject config = doc[_name];
        if (config.containsKey("setInterval")) {
            unsigned long newInterval = config["setInterval"].as<unsigned long>();
            if (newInterval >= 1000 && newInterval != _interval) {
                _interval = newInterval;
                saveConfig();
            }
        }
    }
}

const String& BME280Reader::getName() {
    return _name;
}
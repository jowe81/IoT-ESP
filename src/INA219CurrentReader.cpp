#include "INA219CurrentReader.h"
#include <EEPROM.h>

struct INA219Config {
    int intervalMs;
    int calibrationMode;
    int averagingSamples;
    uint32_t magic;
};

INA219CurrentReader::INA219CurrentReader(String name, uint8_t addr, int intervalMs, int eepromOffset, int averagingSamples)
    : _name(name), _addr(addr), _intervalMs(intervalMs), _eepromOffset(eepromOffset),
      _calibrationMode(0), _averagingSamples(averagingSamples), _ina(addr), _wire(&Wire), _currentSum(0.0), _readingsCount(0), _lastReadingTime(0) {
    if (_name.length() == 0) {
        _name = "ina219";
    }
}

void INA219CurrentReader::begin() {
    begin(&Wire);
}

void INA219CurrentReader::begin(TwoWire *wire) {
    _wire = wire;
    if (_eepromOffset >= 0) {
        loadConfig();
    }
    // Initialize the INA219 library with the specific Wire instance
    if (!_ina.begin(wire)) {
        Serial.printf("INA219CurrentReader: Failed to find INA219 chip at address 0x%02X\n", _addr);
    }
    applyCalibration();
}

void INA219CurrentReader::update() {
    if (millis() - _lastReadingTime >= (unsigned long)_intervalMs) {
        _lastReadingTime = millis();
        
        // Read current in mA
        float current = _ina.getCurrent_mA();
        
        // Accumulate readings
        _currentSum += current;
        _readingsCount++;
    }
}

float INA219CurrentReader::getAverageCurrent() {
    if (_readingsCount == 0) {
        return 0.0;
    }
    return (float)(_currentSum / _readingsCount);
}

void INA219CurrentReader::addToJson(JsonArray& doc) {
    JsonObject nested = doc.createNestedObject();
    nested["type"] = "Sensor";
    nested["subtype"] = "INA219";
    nested["name"] = _name;
    nested["current_mA"] = getAverageCurrent();
    nested["shunt_mV"] = _ina.getShuntVoltage_mV();
    nested["voltage_V"] = _ina.getBusVoltage_V();
    nested["power_mW"] = _ina.getPower_mW();
    nested["readingsCount"] = _readingsCount;
    nested["interval"] = _intervalMs;
    nested["calibrationMode"] = _calibrationMode;
    nested["averagingSamples"] = _averagingSamples;

    // Reset accumulators after reporting
    _currentSum = 0;
    _readingsCount = 0;
}

const String& INA219CurrentReader::getName() {
    return _name;
}

void INA219CurrentReader::loadConfig() {
    INA219Config config;
    EEPROM.get(_eepromOffset, config);
    
    // Magic updated to 0xDEADBEF1 to force reset of config structure if upgrading from old version
    if (config.magic == 0xDEADBEF1) {
        if (config.intervalMs > 0) {
            _intervalMs = config.intervalMs;
        }
        if (config.calibrationMode >= 0 && config.calibrationMode <= 2) {
            _calibrationMode = config.calibrationMode;
        }
        if (config.averagingSamples > 0) {
            _averagingSamples = config.averagingSamples;
        }
    }
}

void INA219CurrentReader::saveConfig() {
    if (_eepromOffset < 0) return;
    INA219Config config = { _intervalMs, _calibrationMode, _averagingSamples, 0xDEADBEF1 };
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

void INA219CurrentReader::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        JsonObject config = doc[_name];
        bool changed = false;

        if (config.containsKey("setInterval")) {
            _intervalMs = config["setInterval"].as<int>();
            changed = true;
        }
        if (config.containsKey("setCalibrationMode")) {
            int mode = config["setCalibrationMode"].as<int>();
            // 0: 32V 2A (Default)
            // 1: 32V 1A
            // 2: 16V 400mA
            if (mode >= 0 && mode <= 2) {
                _calibrationMode = mode;
                applyCalibration();
                changed = true;
            }
        }
        if (config.containsKey("setAveragingSamples")) {
            int samples = config["setAveragingSamples"].as<int>();
            if (samples == 1 || samples == 2 || samples == 4 || samples == 8 || samples == 16 || samples == 32 || samples == 64 || samples == 128) {
                _averagingSamples = samples;
                applyCalibration(); // Re-apply calibration and averaging
                changed = true;
            }
        }

        if (changed) {
            saveConfig();
        }
    }
}

void INA219CurrentReader::applyCalibration() {
    switch (_calibrationMode) {
        case 1: _ina.setCalibration_32V_1A(); break;
        case 2: _ina.setCalibration_16V_400mA(); break;
        default: _ina.setCalibration_32V_2A(); break;
    }
    // Reset buffer to avoid mixing readings from different scales
    applyAveraging();
    _currentSum = 0;
    _readingsCount = 0;
}

void INA219CurrentReader::applyAveraging() {
    // The Adafruit library doesn't expose a method to set averaging directly,
    // so we modify the configuration register (0x00) manually.
    
    // 1. Read current config
    _wire->beginTransmission(_addr);
    _wire->write(0x00); // Config register address
    _wire->endTransmission();
    
    _wire->requestFrom(_addr, (uint8_t)2);
    if (_wire->available() >= 2) {
        uint16_t config = _wire->read() << 8;
        config |= _wire->read();
        
        // 2. Mask out Bus ADC (bits 7-10) and Shunt ADC (bits 3-6) settings
        // 0000 0111 1111 1000 = 0x07F8
        config &= ~0x07F8;
        
        // 3. Determine new bits based on samples
        uint16_t mask = 0x3; // Default 1 sample (12-bit)
        if (_averagingSamples == 2) mask = 0x9;
        else if (_averagingSamples == 4) mask = 0xA;
        else if (_averagingSamples == 8) mask = 0xB;
        else if (_averagingSamples == 16) mask = 0xC;
        else if (_averagingSamples == 32) mask = 0xD;
        else if (_averagingSamples == 64) mask = 0xE;
        else if (_averagingSamples == 128) mask = 0xF;
        
        // 4. Set new bits for both Bus and Shunt ADC
        config |= (mask << 7) | (mask << 3);
        
        _wire->beginTransmission(_addr);
        _wire->write(0x00);
        _wire->write((config >> 8) & 0xFF);
        _wire->write(config & 0xFF);
        _wire->endTransmission();
    }
}
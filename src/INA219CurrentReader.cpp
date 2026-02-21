#include "INA219CurrentReader.h"
#include <EEPROM.h>
#include "Logger.h"

struct INA219Config {
    int intervalMs;
    int calibrationMode;
    int averagingSamples;
    uint32_t magic;
};

INA219CurrentReader::INA219CurrentReader(String name, uint8_t addr, int intervalMs, int eepromOffset, int averagingSamples)
    : _name(name), _addr(addr), _intervalMs(intervalMs), _eepromOffset(eepromOffset),
      _calibrationMode(0), _averagingSamples(averagingSamples), _ina(addr), _wire(&Wire), _available(false), _currentSum(0.0), _readingsCount(0), _lastReadingTime(0), _lastReconnectAttempt(0),
      _isExternalShunt(false), _shuntOhms(0.0f), _maxAmps(0.0f), _currentLSB(0.0f), _calValue(0) {
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
    if (_ina.begin(wire)) {
        _available = true;
        Log.info(("INA219 " + _name + " found at 0x" + String(_addr, HEX)).c_str());
        applyCalibration();
    } else {
        _available = false;
        Log.error(("INA219 " + _name + " not found at 0x" + String(_addr, HEX)).c_str());
    }
}

void INA219CurrentReader::update() {
    if (!_available) {
        if (millis() - _lastReconnectAttempt >= 120000) {
            _lastReconnectAttempt = millis();
            if (_ina.begin(_wire)) {
                _available = true;
                Log.info(("INA219 " + _name + " is available again.").c_str());
                applyCalibration();
            }
        }
        return;
    }

    if (millis() - _lastReadingTime >= (unsigned long)_intervalMs) {
        _lastReadingTime = millis();

        // Check if device is still present
        _wire->beginTransmission(_addr);
        if (_wire->endTransmission() != 0) {
            Log.warn(("INA219 " + _name + " reading failed. Marking as unavailable.").c_str());
            _available = false;
            _lastReconnectAttempt = millis();
            return;
        }

        float current = 0;
        if (_isExternalShunt) {
            // To protect against brown-out resets on the INA219, we rewrite the
            // calibration register before each reading. This is what the Adafruit
            // library does in its getCurrent_raw() method.
            _wire->beginTransmission(_addr);
            _wire->write(0x05); // Calibration register
            _wire->write((_calValue >> 8) & 0xFF);
            _wire->write(_calValue & 0xFF);
            _wire->endTransmission();

            // For external shunts, we must read the raw value and apply our own LSB
            _wire->beginTransmission(_addr);
            _wire->write(0x04); // Current register
            _wire->endTransmission();

            _wire->requestFrom(_addr, (uint8_t)2);
            if (_wire->available() >= 2) {
                int16_t raw_current = (int16_t)((_wire->read() << 8) | _wire->read());
                // There appears to be a subtle interaction with the Adafruit library's
                // default power multiplier of 2, which causes the raw reading to be halved.
                // We multiply by the same factor to correct it.
                float multiplier = 2.0f;
                current = raw_current * _currentLSB * 1000.0f * multiplier; // convert to mA and correct
            }
        } else {
            // Read current in mA using the library for internal shunts
            current = _ina.getCurrent_mA();
        }

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
    nested["interval"] = _intervalMs;
    if (_isExternalShunt) {
        nested["shuntType"] = "external";
        nested["shuntOhms"] = _shuntOhms;
        nested["maxAmps"] = _maxAmps;
    } else {
        nested["shuntType"] = "internal";
        nested["calibrationMode"] = _calibrationMode;
    }
    nested["averagingSamples"] = _averagingSamples;
    nested["available"] = _available;

    if (_available) {
        float current_mA = getAverageCurrent();
        float bus_voltage_V = _ina.getBusVoltage_V();
        nested["current_mA"] = current_mA;
        nested["shunt_mV"] = _ina.getShuntVoltage_mV();
        nested["voltage_V"] = bus_voltage_V;
        // For external shunt, power must be calculated manually
        nested["power_mW"] = _isExternalShunt ? (bus_voltage_V * current_mA) : _ina.getPower_mW();
        nested["readingsCount"] = _readingsCount;
    } else {
        nested["error"] = "Sensor not found";
    }

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

void INA219CurrentReader::setExternalShunt(float shuntOhms, float maxAmps) {
    _isExternalShunt = true;
    _shuntOhms = shuntOhms;
    _maxAmps = maxAmps;
}

void INA219CurrentReader::applyCalibration() {
    if (!_available) return;

    if (_isExternalShunt) {
        // --- Custom Calibration for External Shunt ---

        // 1. Select PGA gain based on the max expected shunt voltage.
        // V_SHUNT_MAX = I_MAX * R_SHUNT
        // Gains: /1 (40mV), /2 (80mV), /4 (160mV), /8 (320mV)
        uint16_t config_gain_bits;
        float max_shunt_voltage = _maxAmps * _shuntOhms;

        if (max_shunt_voltage <= 0.04) { config_gain_bits = 0x0000; } // Gain /1, 40mV
        else if (max_shunt_voltage <= 0.08) { config_gain_bits = 0x0800; } // Gain /2, 80mV
        else if (max_shunt_voltage <= 0.16) { config_gain_bits = 0x1000; } // Gain /4, 160mV
        else { config_gain_bits = 0x1800; } // Gain /8, 320mV

        // 2. Calculate calibration value.
        // Cal = 0.04096 / (Current_LSB * R_SHUNT)
        // For best resolution, we use the smallest possible LSB: Current_LSB = MaxExpected_I / 32767
        _currentLSB = _maxAmps / 32767.0;
        _calValue = (uint16_t)(0.04096 / (_currentLSB * _shuntOhms));

        // 3. Recalculate the LSB with the truncated calValue for better accuracy in future calculations.
        _currentLSB = 0.04096 / (_calValue * _shuntOhms);

        // 4. Manually set gain in the config register.
        _wire->beginTransmission(_addr);
        _wire->write(0x00); // Config register address
        _wire->endTransmission();
        _wire->requestFrom(_addr, (uint8_t)2);
        uint16_t config = 0;
        if (_wire->available() >= 2) {
            config = (_wire->read() << 8) | _wire->read();
        }
        config &= ~0x1800; // Mask out old gain bits (PG1, PG0 @ 12,11)
        config |= config_gain_bits; // Set new gain bits
        _wire->beginTransmission(_addr);
        _wire->write(0x00);
        _wire->write((config >> 8) & 0xFF);
        _wire->write(config & 0xFF);
        _wire->endTransmission();

        // 5. Manually write the new calibration value.
        _wire->beginTransmission(_addr);
        _wire->write(0x05); // Calibration register
        _wire->write((_calValue >> 8) & 0xFF);
        _wire->write(_calValue & 0xFF);
        _wire->endTransmission();

        Log.info(("INA219 " + _name + " calibrated for external shunt: " + String(_shuntOhms, 4) + " Ohm, " + String(_maxAmps) + " A. CalVal: " + String(_calValue)).c_str());
    } else {
        // Use standard library calibrations for internal shunt
        switch (_calibrationMode) {
            case 1: _ina.setCalibration_32V_1A(); break;
            case 2: _ina.setCalibration_16V_400mA(); break;
            default: _ina.setCalibration_32V_2A(); break;
        }
    }
    // Reset buffer to avoid mixing readings from different scales
    applyAveraging();
    _currentSum = 0;
    _readingsCount = 0;
}

void INA219CurrentReader::applyAveraging() {
    if (!_available) return;

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
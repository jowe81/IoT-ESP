#include <Arduino.h>
#include "BatteryMonitor.h"
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "Logger.h"

struct BatteryConfig {
    float low;
    float critical;
    int bufferSize;
    float adjustment;
    uint32_t magic;
};

// Constructor.
BatteryMonitor::BatteryMonitor(String name, int pin, float ratio, float lowThreshold, float criticalThreshold, int eepromOffset, int readingsBufferSize) 
    : _pin(pin), _eepromOffset(eepromOffset), _name(name), _ratio(ratio), _lowThreshold(lowThreshold), _criticalThreshold(criticalThreshold),
      _voltageAdjustment(1.0), _readingsBufferSize(readingsBufferSize),
      _readingsIndex(0), _readingsCount(0), _lastReadingTime(0), _lastAcceptedReadingTime(0),
      _lastKnownVoltage(0.0),
      _lowState(false), _criticalState(false), _lowEvent(false), _criticalEvent(false) {
    if (_name.length() == 0) {
        _name = "_battery";
    }
    pinMode(_pin, INPUT);
    _readings.resize(_readingsBufferSize * READINGS_PER_CYCLE, 0.0);
}

void BatteryMonitor::begin() {
    loadConfig();
}

void BatteryMonitor::loadConfig() {
    BatteryConfig config;
    EEPROM.get(_eepromOffset, config);
    
    // Check for magic number (0xCAFEBABE) to verify valid data exists
    if (config.magic == 0xCAFEBABE) {
        // Sanity check values
        if (config.low > MIN_SANITY_VOLTAGE && config.low < MAX_SANITY_VOLTAGE && config.critical > MIN_SANITY_VOLTAGE && config.critical < MAX_SANITY_VOLTAGE) {
            _lowThreshold = config.low;
            _criticalThreshold = config.critical;
            
            if (config.bufferSize > 0 && config.bufferSize < 1000) {
                if (_readingsBufferSize != config.bufferSize) {
                    _readingsBufferSize = config.bufferSize;
                    _readings.assign(_readingsBufferSize * READINGS_PER_CYCLE, 0.0);
                    _readingsCount = 0;
                    _readingsIndex = 0;
                }
            }

            if (config.adjustment > 0.0) {
                _voltageAdjustment = config.adjustment;
            }
        }
    }
}

void BatteryMonitor::saveConfig() {
    BatteryConfig config = { _lowThreshold, _criticalThreshold, _readingsBufferSize, _voltageAdjustment, 0xCAFEBABE };
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

void BatteryMonitor::update() {
    // Save a set of readings up to once every 900ms (in reality this should work out to about a second
    // with the lightsleep delay bein 1000ms also)
    int totalReadings = _readingsBufferSize * READINGS_PER_CYCLE;

    if (millis() - _lastReadingTime >= 900) {
        _lastReadingTime = millis();
        
        // Safety check: If we haven't accepted a valid reading in 30 seconds,
        // assume the voltage has shifted significantly (e.g. charger connected)
        // and reset the buffer to start a new average.
        if (millis() - _lastAcceptedReadingTime > 30000) {
            Log.warn("BatteryMonitor: Stuck rejecting outliers. Resetting buffer.");
            _readingsCount = 0;
            _readingsIndex = 0;
            _lastAcceptedReadingTime = millis();
            _lastKnownVoltage = 0.0; // Invalidate cache so we don't report phantom voltage
        }

        float currentAverage = getVoltage();

        for (int i = 0; i < READINGS_PER_CYCLE; i++) {
            int raw = analogRead(_pin);
            delay(2);
            
            // Convert 10-bit ADC (0-1023) to voltage based on the divider
            float voltage = (raw / 1023.0) * _ratio * _voltageAdjustment;

            bool isOutlier = false;

            // Check absolute sanity limits
            if (voltage < MIN_SANITY_VOLTAGE || voltage > MAX_SANITY_VOLTAGE) {
                isOutlier = true;
            }

            if (!isOutlier && _readingsCount == totalReadings && currentAverage > 0) {
                // Check if reading is within 10% of average to reject noise spikes
                float margin = currentAverage * 0.10;
                if (abs(voltage - currentAverage) > margin) {
                    isOutlier = true;
                }
            }

            if (!isOutlier) {
                _readings[_readingsIndex] = voltage;
                _readingsIndex = (_readingsIndex + 1) % totalReadings;
                if (_readingsCount < totalReadings) _readingsCount++;
                _lastAcceptedReadingTime = millis();
            }
        }

        // Update states with hysteresis
        float v = getVoltage();
        bool connected = (v > MIN_SANITY_VOLTAGE);

        if (connected) {
            if (!_lowState && v < _lowThreshold) {
                _lowState = true;
                _lowEvent = true;
            } else if (_lowState && v > (_lowThreshold + HYSTERESIS)) {
                _lowState = false;
            }

            if (!_criticalState && v < _criticalThreshold) {
                _criticalState = true;
                _criticalEvent = true;
            } else if (_criticalState && v > (_criticalThreshold + HYSTERESIS)) {
                _criticalState = false;
            }
        } else {
            _lowState = false;
            _criticalState = false;
        }
    }
}

float BatteryMonitor::getVoltage() {
    int totalReadings = _readingsBufferSize * READINGS_PER_CYCLE;

    if (_readingsCount < totalReadings) {
        // Return last known good voltage while refilling
        return _lastKnownVoltage;
    }
    float sum = 0;
    for (int i = 0; i < totalReadings; i++) {
        sum += _readings[i];
    }
    _lastKnownVoltage = sum / (float)totalReadings;
    return _lastKnownVoltage;
}

bool BatteryMonitor::batteryIsConnected() {
    return getVoltage() > MIN_SANITY_VOLTAGE;
}


bool BatteryMonitor::isLow() {
    return _lowState;
}

bool BatteryMonitor::isCritical() {
    return _criticalState;
}

bool BatteryMonitor::gotLow() {
    if (_lowEvent) {
        _lowEvent = false;
        return true;
    }
    return false;
}

bool BatteryMonitor::gotCritical() {
    if (_criticalEvent) {
        _criticalEvent = false;
        return true;
    }
    return false;
}

void BatteryMonitor::addToJson(JsonObject& doc) {
    int totalReadings = _readingsBufferSize * READINGS_PER_CYCLE;

    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = TYPE;
    float voltage = getVoltage();
    if (voltage > 0) {
        nested["voltage"] = getVoltage();
        nested["thresholdLow"] = _lowThreshold;
        nested["thresholdCritical"] = _criticalThreshold;
        nested["bufferSize"] = _readingsBufferSize;
        nested["adjustment"] = _voltageAdjustment;
        nested["isLow"] = isLow();
        nested["isCritical"] = isCritical();
        nested["isStale"] = (_readingsCount < totalReadings);
        nested["isBuffering"] = false;
    } else {
        nested["isBuffering"] = true;
    }
}

void BatteryMonitor::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        JsonObject config = doc[_name];
        if (config.containsKey("low")) {
            _lowThreshold = config["low"];
        }
        if (config.containsKey("critical")) {
            _criticalThreshold = config["critical"];
        }
        if (config.containsKey("bufferSize")) {
            int newSize = config["bufferSize"];
            if (newSize > 0 && newSize != _readingsBufferSize) {
                _readingsBufferSize = newSize;
                _readings.assign(_readingsBufferSize * READINGS_PER_CYCLE, 0.0);
                _readingsCount = 0;
                _readingsIndex = 0;
            }
        }
        if (config.containsKey("adjustment")) {
            _voltageAdjustment = config["adjustment"];
        }
        saveConfig();
    }
}

const String& BatteryMonitor::getName() {
    return _name;
}
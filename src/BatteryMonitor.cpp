#include <Arduino.h>
#include "BatteryMonitor.h"
#include "TemperatureReader.h"
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "Logger.h"

struct BatteryConfig {
    float low;
    float critical;
    int bufferSize;
    float adjustment;
    float temperature;
    float batteryVoltage;
    char batteryType[20];
    uint32_t magic;
};

// Constructor.
BatteryMonitor::BatteryMonitor(String name, int pin, float ratio, float lowThreshold, float criticalThreshold, int eepromOffset, int readingsBufferSize, TemperatureReader* tempReader, float temperature) 
    : _pin(pin), _eepromOffset(eepromOffset), _name(name), _ratio(ratio), _lowThreshold(lowThreshold), _criticalThreshold(criticalThreshold),
      _voltageSensorAdjustmentFactor(1.0), _temperature(temperature), _tempReader(tempReader), _batteryType("flooded"), _batteryVoltage(12.0),
      _readingsBufferSize(readingsBufferSize),
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
                _voltageSensorAdjustmentFactor = config.adjustment;
            }

            _temperature = config.temperature;
            _batteryVoltage = config.batteryVoltage;
            // Ensure null termination
            config.batteryType[sizeof(config.batteryType) - 1] = 0;
            _batteryType = String(config.batteryType);
        }
    }
}

void BatteryMonitor::saveConfig() {
    BatteryConfig config = { _lowThreshold, _criticalThreshold, _readingsBufferSize, _voltageSensorAdjustmentFactor, _temperature, _batteryVoltage, "", 0xCAFEBABE };
    strncpy(config.batteryType, _batteryType.c_str(), sizeof(config.batteryType) - 1);
    config.batteryType[sizeof(config.batteryType) - 1] = 0;
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

float BatteryMonitor::applyAdjustment(float voltage, bool reverse) {
    if (_batteryType == "flooded") {
        float adjustment = (25.0 - _temperature) * 0.024;
        return reverse ? (voltage - adjustment) : (voltage + adjustment);
    }
    return voltage;
}

void BatteryMonitor::update() {
    // Save a set of readings up to once every 900ms (in reality this should work out to about a second
    // with the lightsleep delay bein 1000ms also)
    int totalReadings = _readingsBufferSize * READINGS_PER_CYCLE;

    if (millis() - _lastReadingTime >= 900) {
        _lastReadingTime = millis();

        if (_tempReader != nullptr) {
            float t = _tempReader->getTemperature();
            if (!isnan(t)) {
                _temperature = t;
            }
        }
        
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
            #ifdef ESP32
                //float voltage = (raw / 4095.0) * _ratio * _voltageSensorAdjustmentFactor;
                float voltage = raw * _ratio * _voltageSensorAdjustmentFactor;
            #else
                float voltage = (raw / 1023.0) * 3.3 * _ratio * _voltageSensorAdjustmentFactor;
            #endif

            voltage = applyAdjustment(voltage);

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

    float voltage = getVoltage();

    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = TYPE;
    nested["bufferSize"] = _readingsBufferSize;
    
    if (voltage > 0) {
        nested["voltage"] = voltage;
        nested["voltageRaw"] = applyAdjustment(voltage, true);

        nested["thresholdLow"] = _lowThreshold;
        nested["thresholdCritical"] = _criticalThreshold;
        nested["adjustment"] = _voltageSensorAdjustmentFactor;
        nested["temperature"] = _temperature;
        nested["batteryType"] = _batteryType;
        nested["batteryVoltage"] = _batteryVoltage;
        nested["isLow"] = isLow();
        nested["isCritical"] = isCritical();
        nested["isStale"] = (_readingsCount < totalReadings);
        nested["isBuffering"] = false;
    } else {
        int raw = analogRead(_pin);
        nested["isBuffering"] = true;
        nested["raw"] = raw;
        nested["momentary"] = raw * _ratio * _voltageSensorAdjustmentFactor;
        nested["readingsCount"] = _readingsCount;
    }
}

void BatteryMonitor::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        JsonObject config = doc[_name];
        if (config.containsKey("setLow")) {
            _lowThreshold = config["setLow"].as<float>();
        }
        if (config.containsKey("setCritical")) {
            _criticalThreshold = config["setCritical"].as<float>();
        }
        if (config.containsKey("setBufferSize")) {
            int newSize = config["setBufferSize"].as<int>();
            if (newSize > 0 && newSize != _readingsBufferSize) {
                _readingsBufferSize = newSize;
                _readings.assign(_readingsBufferSize * READINGS_PER_CYCLE, 0.0);
                _readingsCount = 0;
                _readingsIndex = 0;
            }
        }
        if (config.containsKey("setAdjustment")) {
            _voltageSensorAdjustmentFactor = config["setAdjustment"].as<float>();
        }
        if (config.containsKey("setTemperature")) {
            _temperature = config["setTemperature"].as<float>();
        }
        if (config.containsKey("setBatteryType")) {
            _batteryType = config["setBatteryType"].as<String>();
        }
        if (config.containsKey("setBatteryVoltage")) {
            _batteryVoltage = config["setBatteryVoltage"].as<float>();
        }
        saveConfig();
    }
}

const String& BatteryMonitor::getName() {
    return _name;
}
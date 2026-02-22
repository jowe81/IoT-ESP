#include <Arduino.h>
#include "BatteryMonitor.h"
#include "DS18B20.h"
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "Logger.h"
#include <vector>

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
BatteryMonitor::BatteryMonitor(String name, int pin, float ratio, float lowThreshold, float criticalThreshold, int eepromOffset, int readingsBufferSize, DS18B20* tempReader, float temperature) 
    : _pin(pin), _eepromOffset(eepromOffset), _name(name), _ratio(ratio), _lowThreshold(lowThreshold), _criticalThreshold(criticalThreshold),
      _voltageSensorAdjustmentFactor(1.0), _temperature(temperature), _tempReader(tempReader), _batteryType("flooded"), _batteryVoltage(12.0),
      _readingsBufferSize(readingsBufferSize),
      _smoothedVoltage(-1.0), _alpha(0.1), _lastReadingTime(0),
      _lowState(false), _criticalState(false), _lowEvent(false), _criticalEvent(false) {
    if (_name.length() == 0) {
        _name = "_battery";
    }
    pinMode(_pin, INPUT);
    
    // Calculate Alpha based on buffer size to mimic previous behavior roughly.
    // Alpha ~= 2 / (N + 1). For N=60, Alpha is approx 0.03.
    if (_readingsBufferSize > 0) {
        _alpha = 2.0 / (_readingsBufferSize + 1.0);
    }
}

void BatteryMonitor::begin() {
    loadConfig();
#ifdef ESP32
    // Set attenuation to 11dB to allow reading up to 3.3V.
    analogSetPinAttenuation(_pin, ADC_11db);
#endif
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
                    _alpha = 2.0 / (_readingsBufferSize + 1.0);
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

float BatteryMonitor::rawToVoltage(int raw) {
    #ifdef ESP32
        return (raw / 4095.0) * 3.3 * _ratio * _voltageSensorAdjustmentFactor;
    #else
        return (raw / 1023.0) * 1.0 * _ratio * _voltageSensorAdjustmentFactor;
    #endif
}

void BatteryMonitor::update() {
    // Save a set of readings up to once every 900ms (in reality this should work out to about a second
    // with the lightsleep delay bein 1000ms also)

    if (millis() - _lastReadingTime >= 900) {
        _lastReadingTime = millis();

        if (_tempReader != nullptr) {
            float t = _tempReader->getTemperature();
            if (!isnan(t)) {
                _temperature = t;
            }
        }
        
        // 1. Take a burst of readings
        std::vector<float> burstSamples;
        for (int i = 0; i < READINGS_PER_CYCLE; i++) {
            int raw = analogRead(_pin);
            delay(2);
            
            // Convert ADC reading to voltage. We assume a 3.3V reference for the ADC.
            // _ratio is now the multiplier for the voltage divider (e.g. 6.0 for a 1/6 divider).
            float voltage = rawToVoltage(raw);
            voltage = applyAdjustment(voltage);

            // Basic sanity check only (0V is allowed for disconnected)
            if (voltage >= 0.0 && voltage <= MAX_SANITY_VOLTAGE) {
                burstSamples.push_back(voltage);
            }
        }

        if (burstSamples.empty()) return;

        // 2. Median Filter: Sort to find the middle value.
        // This effectively removes spark noise/glitches without complex logic.
        // Simple bubble sort is fast enough for 5 items.
        for (size_t i = 0; i < burstSamples.size() - 1; i++) {
            for (size_t j = 0; j < burstSamples.size() - i - 1; j++) {
                if (burstSamples[j] > burstSamples[j + 1]) {
                    float temp = burstSamples[j];
                    burstSamples[j] = burstSamples[j + 1];
                    burstSamples[j + 1] = temp;
                }
            }
        }
        float medianVoltage = burstSamples[burstSamples.size() / 2];

        // 3. Exponential Moving Average (EMA)
        // If this is the first reading, initialize immediately.
        if (_smoothedVoltage < 0) {
            _smoothedVoltage = medianVoltage;
        } else {
            // Apply smoothing
            _smoothedVoltage = (_smoothedVoltage * (1.0 - _alpha)) + (medianVoltage * _alpha);
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
    if (_smoothedVoltage < 0) {
        return 0.0;
    }
    return _smoothedVoltage;
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

void BatteryMonitor::addToJson(JsonArray& doc) {
    float voltage = getVoltage();

    JsonObject nested = doc.createNestedObject();
    nested["type"] = "System";
    nested["subtype"] = "BatteryMonitor";
    nested["name"] = _name;
    nested["bufferSize"] = _readingsBufferSize;
    
    if (voltage > 0) {
        nested["voltage"] = serialized(String(voltage, 2));
        nested["voltageRaw"] = serialized(String(applyAdjustment(voltage, true), 2));

        nested["thresholdLow"] = serialized(String(_lowThreshold, 2));
        nested["thresholdCritical"] = serialized(String(_criticalThreshold, 2));
        nested["adjustment"] = serialized(String(_voltageSensorAdjustmentFactor, 3));
        nested["temperature"] = serialized(String(_temperature, 2));
        nested["batteryType"] = _batteryType;
        nested["batteryVoltage"] = serialized(String(_batteryVoltage, 2));
        nested["isLow"] = isLow();
        nested["isCritical"] = isCritical();
        nested["isBuffering"] = false;
    } else {
        int raw = analogRead(_pin);
        nested["isBuffering"] = true;
        nested["raw"] = raw;
        float momentary = rawToVoltage(raw);
        nested["momentary"] = serialized(String(applyAdjustment(momentary), 2));
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
                _alpha = 2.0 / (_readingsBufferSize + 1.0);
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
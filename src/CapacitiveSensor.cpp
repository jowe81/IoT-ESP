#include "CapacitiveSensor.h"

// Magic number for EEPROM config validation
#define CAP_SENSOR_MAGIC 0xCAFECA01

CapacitiveSensor::CapacitiveSensor(String name, int pin, int threshold, unsigned long interval, bool triggerOnStateChange, int eepromOffset)
    : _name(name), _pin(pin), _threshold(threshold), _interval(interval), _triggerOnStateChange(triggerOnStateChange), _eepromOffset(eepromOffset),
      _lastAverageValue(0.0), _isTouched(false), _lastReportedTouchedState(false), _triggerExchange(false), // _triggerExchange is set by update()
      _lastUpdateTime(0), _bufferIndex(0), _runningSum(0) {
}

void CapacitiveSensor::begin() {
    if (_eepromOffset >= 0) {
        loadConfig();
    }

    // ESP32 touch pins are configured automatically by touchRead()
    // No explicit pinMode needed for touch pins.

    // Initial read
    int initialVal = _readSensor();
    _readingsBuffer.assign(BUFFER_SIZE, initialVal);
    _runningSum = (long)initialVal * BUFFER_SIZE;
    _lastAverageValue = (float)initialVal;

    _isTouched = (_lastAverageValue < _threshold);
    _lastReportedTouchedState = _isTouched;

    Log.info(("CapacitiveSensor " + _name + " initialized on pin " + String(_pin) + " with threshold " + String(_threshold)).c_str());
}

void CapacitiveSensor::update() {
    if (millis() - _lastUpdateTime >= _interval) {
        _lastUpdateTime = millis();

        // Take burst readings
        for (int i = 0; i < BURST_SIZE; i++) {
            int val = _readSensor();
            
            // Update running sum and buffer
            _runningSum -= _readingsBuffer[_bufferIndex];
            _readingsBuffer[_bufferIndex] = val;
            _runningSum += val;
            
            _bufferIndex = (_bufferIndex + 1) % BUFFER_SIZE;
        }

        _lastAverageValue = getAverage();
        bool currentTouchedState = (_lastAverageValue < _threshold);

        // Check for state change
        if (currentTouchedState != _isTouched) {
            _isTouched = currentTouchedState;
            // Only trigger exchange if the *reported* state changes
            if (_triggerOnStateChange && _isTouched != _lastReportedTouchedState) {
                _triggerExchange = true;
                _lastReportedTouchedState = _isTouched;
                Log.info(("CapacitiveSensor " + _name + " state changed to " + (_isTouched ? "TOUCHED" : "NOT TOUCHED")).c_str());
            }
        }
    }
}

int CapacitiveSensor::_readSensor() {
    #ifdef ESP32
        // ESP32 has built-in touch sensors
        return touchRead(_pin);
    #else
        // For ESP8266 or other boards, this would require an external library
        // or a different approach (e.g., using a dedicated capacitive touch IC).
        // For now, return a dummy value or error for non-ESP32.
        Log.warn("CapacitiveSensor: touchRead() is ESP32 specific. Returning 0 for non-ESP32.");
        return 0;
    #endif
}

float CapacitiveSensor::getAverage() {
    return (float)_runningSum / BUFFER_SIZE;
}

bool CapacitiveSensor::isTouched() {
    return _isTouched;
}

bool CapacitiveSensor::shouldTriggerExchange() {
    return _triggerExchange;
}

void CapacitiveSensor::resetTriggerExchange() {
    _triggerExchange = false;
}

void CapacitiveSensor::addToJson(JsonArray& doc) {
    JsonObject nested = doc.createNestedObject();
    nested["type"] = "Sensor";
    nested["subtype"] = "Capacitive";
    nested["name"] = _name;
    nested["pin"] = _pin;
    nested["threshold"] = _threshold;
    nested["value"] = serialized(String(_lastAverageValue, 2));
    nested["isTouched"] = _isTouched;
    nested["interval"] = _interval;
    nested["triggerOnStateChange"] = _triggerOnStateChange;
}

void CapacitiveSensor::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        JsonObject config = doc[_name];
        bool changed = false;

        if (config.containsKey("setThreshold")) {
            int newThreshold = config["setThreshold"].as<int>();
            if (newThreshold > 0 && newThreshold != _threshold) {
                _threshold = newThreshold;
                changed = true;
                Log.info(("CapacitiveSensor " + _name + " threshold updated to " + String(_threshold)).c_str());
            }
        }
        if (config.containsKey("setInterval")) {
            unsigned long newInterval = config["setInterval"].as<unsigned long>();
            if (newInterval >= 50 && newInterval != _interval) { // Minimum interval to avoid excessive reads
                _interval = newInterval;
                changed = true;
                Log.info(("CapacitiveSensor " + _name + " interval updated to " + String(_interval)).c_str());
            }
        }
        if (config.containsKey("setTriggerOnStateChange")) {
            bool newTrigger = config["setTriggerOnStateChange"].as<bool>();
            if (newTrigger != _triggerOnStateChange) {
                _triggerOnStateChange = newTrigger;
                changed = true;
                Log.info(("CapacitiveSensor " + _name + " triggerOnStateChange updated to " + String(_triggerOnStateChange)).c_str());
            }
        }

        if (changed) saveConfig();
    }
}

void CapacitiveSensor::loadConfig() {
    Config config = {false, 50, 0}; // Default values for safety
    EEPROM.get(_eepromOffset, config);
    if (config.magic == CAP_SENSOR_MAGIC) {
        _threshold = config.threshold;
        _triggerOnStateChange = config.triggerOnStateChange;
    }
}

void CapacitiveSensor::saveConfig() {
    if (_eepromOffset < 0) return;
    Config config = { _triggerOnStateChange, _threshold, CAP_SENSOR_MAGIC };
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

const String& CapacitiveSensor::getName() {
    return _name;
}
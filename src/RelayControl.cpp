#include "RelayControl.h"
#include <EEPROM.h>

#ifdef ESP32
int RelayControl::_nextLedcChannel = 0;
#endif

struct RelayConfig {
    unsigned long autoOffTimer;
    int fadeDuration;
    int percentage;
    uint32_t magic;
};

RelayControl::RelayControl(String name, int pin, bool activeLow, bool pwm, int frequency, int eepromOffset) 
    : DeviceControl(name), pin(pin), _activeLow(activeLow), _pwm(pwm), _percentage(100), _frequency(frequency), _on(false), _autoOffTimer(0), _turnOnTime(0), _eepromOffset(eepromOffset), _fadeDuration(0), _lastHardwareDuty(0) {
    pinMode(pin, OUTPUT);

    if (_pwm) {
        #ifdef ESP32
            _ledcChannel = _nextLedcChannel++;
            ledcSetup(_ledcChannel, _frequency, 8);
            ledcAttachPin(pin, _ledcChannel);
        #else
            // This will change frequency for all pins on the ES08266, as it can only be globally set.
            analogWriteFreq(_frequency);
        #endif
    }

    // Init
    turnOff();
}

void RelayControl::begin() {
    if (_eepromOffset >= 0) {
        loadConfig();
    }
}

void RelayControl::loadConfig() {
    RelayConfig config;
    EEPROM.get(_eepromOffset, config);
    
    if (config.magic == 0xCAFEBABE) {
        _autoOffTimer = config.autoOffTimer;
        _fadeDuration = config.fadeDuration;
        if (config.percentage >= 0 && config.percentage <= 100) {
            _percentage = config.percentage;
        }
    }
}

void RelayControl::saveConfig() {
    if (_eepromOffset < 0) return;
    RelayConfig config = { _autoOffTimer, _fadeDuration, _percentage, 0xCAFEBABE };
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

void RelayControl::turnOn() {
    _on = true;
    _turnOnTime = millis();
    _updateHardware();
}

void RelayControl::turnOff() {
    _on = false;
    _updateHardware();
}

void RelayControl::toggle() {
    isOn() ? turnOff() : turnOn();
}

bool RelayControl::isOn() {
    return _on;
}

void RelayControl::setPercentage(int percentage) {
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    
    if (_percentage != percentage) {
        _percentage = percentage;
        saveConfig();
    }

    // Only update hardware if we are currently ON.
    // This allows setting the dim level without turning the light on.
    if (_on) {
        _updateHardware();
    }
}

void RelayControl::_updateHardware() {
    int effectivePercentage = _on ? _percentage : 0;

    if (_pwm) {
        int targetDuty = 0;
        #ifdef ESP32
            int maxDuty = 255;
        #else
            int maxDuty = 1023;
        #endif

        targetDuty = map(effectivePercentage, 0, 100, 0, maxDuty);
        if (_activeLow) targetDuty = maxDuty - targetDuty;

        if (_fadeDuration > 0 && _lastHardwareDuty != targetDuty) {
            int diff = targetDuty - _lastHardwareDuty;
            int steps = abs(diff);
            if (steps > 0) {
                int stepDelay = _fadeDuration / steps;
                for (int i = 1; i <= steps; i++) {
                    int d = _lastHardwareDuty + (diff * i / steps);
                    #ifdef ESP32
                        ledcWrite(_ledcChannel, d);
                    #else
                        analogWrite(pin, d);
                    #endif
                    if (stepDelay > 0) delay(stepDelay);
                }
            }
        } else {
            #ifdef ESP32
                ledcWrite(_ledcChannel, targetDuty);
            #else
                analogWrite(pin, targetDuty);
            #endif
        }
        _lastHardwareDuty = targetDuty;
    } else {
        bool on = effectivePercentage > 0;
        digitalWrite(pin, _activeLow ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
    }
}

void RelayControl::setFrequency(int frequency) {
    _frequency = frequency;
    if (_pwm) {
        #ifdef ESP32
            ledcSetup(_ledcChannel, _frequency, 8);
        #else
            analogWriteFreq(_frequency);
        #endif
        // Re-apply percentage to ensure duty cycle is correct
        _updateHardware();
    }
}

void RelayControl::setAutoOffTimer(unsigned long duration) {
    if (_autoOffTimer != duration) {
        _autoOffTimer = duration;
        saveConfig();
    }
}

void RelayControl::setFadeDuration(int duration) {
    if (_fadeDuration != duration) {
        _fadeDuration = duration;
        saveConfig();
    }
}

void RelayControl::update() {
    if (_on && _autoOffTimer > 0 && (millis() - _turnOnTime >= _autoOffTimer)) {
        turnOff();
    }
}

void RelayControl::refreshState() {
    _updateHardware();
}

void RelayControl::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        if (doc[_name].is<JsonObject>()) {
            JsonObject command = doc[_name].as<JsonObject>();

            // Process settings first to ensure they apply before any state change
            if (command.containsKey("setPercentage")) {
                setPercentage(command["setPercentage"].as<int>());
            }
            if (command.containsKey("setFrequency")) {
                setFrequency(command["setFrequency"].as<int>());
            }
            if (command.containsKey("setAutoOffTimer")) {
                setAutoOffTimer(command["setAutoOffTimer"].as<unsigned long>());
            }
            if (command.containsKey("setFadeDuration")) {
                setFadeDuration(command["setFadeDuration"].as<int>());
            }

            // Process state changes
            if (command.containsKey("toggleState") && command["toggleState"].as<bool>()) {
                toggle();
            }
            if (command.containsKey("setState") && command["setState"].is<bool>()) {
                // Only act if setState is explicitly true or false, ignore null/string/etc.
                bool state = command["setState"].as<bool>();
                state ? turnOn() : turnOff();
            }
        }
    }
}

void RelayControl::addToJson(JsonObject& doc) {
    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = "RelayControl";
    nested["isOn"] = isOn();
    nested["percentage"] = _percentage;
    nested["frequency"] = _frequency;
    nested["autoOffTimer"] = _autoOffTimer;
    nested["fadeDuration"] = _fadeDuration;

    unsigned long remaining = 0;
    if (_on && _autoOffTimer > 0) {
        unsigned long elapsed = millis() - _turnOnTime;
        if (elapsed < _autoOffTimer) {
            remaining = _autoOffTimer - elapsed;
        }
    }
    nested["autoOffRemaining"] = remaining;
}

const String& RelayControl::getName() {
    return _name;
}

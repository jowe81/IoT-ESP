#include "BistableRelayControl.h"
#include <EEPROM.h>

struct BistableRelayConfig {
    unsigned long autoOffTimer;
    uint32_t magic;
};

BistableRelayControl::BistableRelayControl(String name, int pinOn, int pinOff, int eepromOffset) 
    : DeviceControl(name), pinOn(pinOn), pinOff(pinOff), _isOn(false), _autoOffTimer(0), _turnOnTime(0), _eepromOffset(eepromOffset) {
    pinMode(pinOn, OUTPUT);
    pinMode(pinOff, OUTPUT);
    
    // Init.
    digitalWrite(pinOn, LOW);
    digitalWrite(pinOff, LOW);
}

void BistableRelayControl::begin() {
    if (_eepromOffset >= 0) {
        loadConfig();
    }
}

void BistableRelayControl::loadConfig() {
    BistableRelayConfig config;
    EEPROM.get(_eepromOffset, config);
    
    if (config.magic == 0xCAFEBABE) {
        _autoOffTimer = config.autoOffTimer;
    }
}

void BistableRelayControl::saveConfig() {
    if (_eepromOffset < 0) return;
    BistableRelayConfig config = { _autoOffTimer, 0xCAFEBABE };
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

void BistableRelayControl::turnOn() {
    if (pinOn == pinOff && _isOn) return;
    digitalWrite(pinOn, HIGH);
    delay(100); // 100ms pulse to latch the relay
    digitalWrite(pinOn, LOW);
    _isOn = true;
    _turnOnTime = millis();
}

void BistableRelayControl::turnOff() {
    if (pinOn == pinOff && !_isOn) return;
    digitalWrite(pinOff, HIGH);
    delay(100); // 100ms pulse to unlatch the relay
    digitalWrite(pinOff, LOW);
    _isOn = false;
}

void BistableRelayControl::toggle() {
    if (isOn()) {
        turnOff();
    } else {
        turnOn();
    }
}

void BistableRelayControl::toggleInternalState() {
    _isOn = !_isOn;
}

bool BistableRelayControl::isOn() {
    return _isOn;
}

void BistableRelayControl::setAutoOffTimer(unsigned long duration) {
    if (_autoOffTimer != duration) {
        _autoOffTimer = duration;
        saveConfig();
    }
}

void BistableRelayControl::update() {
    if (_isOn && _autoOffTimer > 0 && (millis() - _turnOnTime >= _autoOffTimer)) {
        turnOff();
    }
}

void BistableRelayControl::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        if (doc[_name].is<JsonObject>()) {
            JsonObject command = doc[_name].as<JsonObject>();

            if (command.containsKey("toggleState") && command["toggleState"].as<bool>()) {
                toggle();
            }
            if (command.containsKey("setState") && command["setState"].is<bool>()) {
                // Only act if setState is explicitly true or false, ignore null/string/etc.
                bool state = command["setState"].as<bool>();
                state ? turnOn() : turnOff();
            }
            if (command.containsKey("toggleInternalState") && command["toggleInternalState"].as<bool>()) {
                toggleInternalState();
            }
            if (command.containsKey("setAutoOffTimer")) {
                setAutoOffTimer(command["setAutoOffTimer"].as<unsigned long>());
            }
        }
    }
}

void BistableRelayControl::addToJson(JsonObject& doc) {
    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = "DeviceControl";
    nested["subType"] = "BistableRelayControl";
    nested["isOn"] = isOn();
    nested["autoOffTimer"] = _autoOffTimer;

    unsigned long remaining = 0;
    if (_isOn && _autoOffTimer > 0) {
        unsigned long elapsed = millis() - _turnOnTime;
        if (elapsed < _autoOffTimer) {
            remaining = _autoOffTimer - elapsed;
        }
    }
    nested["autoOffRemaining"] = remaining;
}

const String& BistableRelayControl::getName() {
    return _name;
}

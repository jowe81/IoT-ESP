#include "BistableRelayControl.h"

BistableRelayControl::BistableRelayControl(String name, int pinOn, int pinOff) 
    : DeviceControl(name), pinOn(pinOn), pinOff(pinOff), _isOn(false) {
    pinMode(pinOn, OUTPUT);
    pinMode(pinOff, OUTPUT);
    
    // Init.
    digitalWrite(pinOn, LOW);
    digitalWrite(pinOff, LOW);
}

void BistableRelayControl::turnOn() {
    if (pinOn == pinOff && _isOn) return;
    digitalWrite(pinOn, HIGH);
    delay(100); // 100ms pulse to latch the relay
    digitalWrite(pinOn, LOW);
    _isOn = true;
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

void BistableRelayControl::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        if (doc[_name].is<JsonObject>()) {
            JsonObject command = doc[_name].as<JsonObject>();

            if (command.containsKey("toggleState") && command["toggleState"].as<bool>()) {
                toggle();
            } else if (command.containsKey("setState") && command["setState"].is<bool>()) {
                // Only act if setState is explicitly true or false, ignore null/string/etc.
                bool state = command["setState"].as<bool>();
                state ? turnOn() : turnOff();
            }
            if (command.containsKey("toggleInternalState") && command["toggleInternalState"].as<bool>()) {
                toggleInternalState();
            }
        }
    }
}

const String& BistableRelayControl::getName() {
    return _name;
}

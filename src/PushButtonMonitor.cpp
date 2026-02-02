#include "PushButtonMonitor.h"

PushButtonMonitor::PushButtonMonitor(String name, int pin, bool activeLow) 
    : _pin(pin), _name(name), _activeLow(activeLow), _lastReading(false), _state(false), _lastDebounceTime(0), _localAction(true) {
    if (_activeLow) {
        pinMode(_pin, INPUT_PULLUP);
    } else {
        pinMode(_pin, INPUT);
    }
}

bool PushButtonMonitor::isPressed() {
    int state = digitalRead(_pin);
    return _activeLow ? (state == LOW) : (state == HIGH);
}

bool PushButtonMonitor::checkPressed() {
    bool reading = isPressed();

    // If the switch changed, due to noise or pressing:
    if (reading != _lastReading) {
        _lastDebounceTime = millis();
    }
    _lastReading = reading;

    if ((millis() - _lastDebounceTime) > 50) {
        // If the state has changed:
        if (reading != _state) {
            _state = reading;
            // Only return true if the new state is PRESSED
            if (_state) {
                return true;
            }
        }
    }
    return false;
}

void PushButtonMonitor::addToJson(JsonObject& doc) {
    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = TYPE;
    nested["isPressed"] = isPressed();
    nested["localAction"] = _localAction;
}

void PushButtonMonitor::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        JsonObject config = doc[_name];
        if (config.containsKey("localAction")) {
            _localAction = config["localAction"].as<bool>();
        }
    }
}

bool PushButtonMonitor::localAction() {
    return _localAction;
}

const String& PushButtonMonitor::getName() {
    return _name;
}
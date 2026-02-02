#include "RelayControl.h"

RelayControl::RelayControl(int pin, String name, bool activeLow) 
    : DeviceControl(name), pin(pin), _activeLow(activeLow) {
    pinMode(pin, OUTPUT);

    // Init
    turnOff();
}

void RelayControl::turnOn() {
    digitalWrite(pin, _activeLow ? LOW : HIGH);    
}

void RelayControl::turnOff() {
    digitalWrite(pin, _activeLow ? HIGH : LOW);
}

void RelayControl::toggle() {
    isOn() ? turnOff() : turnOn();
}

bool RelayControl::isOn() {
    int state = digitalRead(pin);
    return _activeLow ? (state == LOW) : (state == HIGH);
}

void RelayControl::processJson(JsonObject& doc) {
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
        }
    }
}

const String& RelayControl::getName() {
    return _name;
}

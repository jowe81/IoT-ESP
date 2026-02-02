#ifndef BISTABLE_RELAIS_CONTROL_H
#define BISTABLE_RELAIS_CONTROL_H

#include <Arduino.h>
#include "DeviceControl.h"

class BistableRelayControl : public DeviceControl {
    private:
        int pinOn;
        int pinOff;
        bool _isOn;

    public:
        // Works for both single pin and dual pin bistable relays.
        BistableRelayControl(String name, int pinOn, int pinOff);
        void turnOn() override;
        void turnOff() override;
        void toggle() override;
        void toggleInternalState();
        bool isOn() override;
        void processJson(JsonObject& doc) override;
        const String& getName();
};

#endif
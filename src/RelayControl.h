#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include <Arduino.h>
#include "DeviceControl.h"

class RelayControl : public DeviceControl {
    private:
        int pin;
        bool _activeLow;

    public:
        RelayControl(int pin, String name, bool activeLow = false);
        void turnOn() override;
        void turnOff() override;
        void toggle() override;
        bool isOn() override;
        void processJson(JsonObject& doc) override;
        const String& getName();
};

#endif
#ifndef BISTABLE_RELAIS_CONTROL_H
#define BISTABLE_RELAIS_CONTROL_H

#include <Arduino.h>
#include "DeviceControl.h"

class BistableRelayControl : public DeviceControl {
    private:
        int pinOn;
        int pinOff;
        bool _isOn;
        unsigned long _autoOffTimer;
        unsigned long _turnOnTime;
        int _eepromOffset;

    public:
        // Works for both single pin and dual pin bistable relays.
        BistableRelayControl(String name, int pinOn, int pinOff, int eepromOffset = -1);
        void begin();
        void turnOn() override;
        void turnOff() override;
        void toggle() override;
        void toggleInternalState();
        bool isOn() override;
        void setAutoOffTimer(unsigned long duration);
        void update();
        void processJson(JsonObject& doc) override;
        void addToJson(JsonArray& doc) override;
        const String& getName();

    private:
        void loadConfig();
        void saveConfig();
};

#endif
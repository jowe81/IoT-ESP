#ifndef PUSH_BUTTON_MONITOR_H
#define PUSH_BUTTON_MONITOR_H

#include <Arduino.h>
#include "Device.h"
#include "DeviceControl.h"

class PushButtonMonitor : public Device {
    private:
        int _pin;
        String _name;
        bool _activeLow;
        bool _lastReading;
        bool _state;
        unsigned long _lastDebounceTime;
        bool _localAction;
        DeviceControl* _targetDevice;
        bool _triggerExchange;

    public:
        static constexpr const char* TYPE = "PushButtonMonitor";
        PushButtonMonitor(String name, int pin, bool activeLow = true);
        void setTarget(DeviceControl* target);
        void update() override;
        bool shouldTriggerExchange() override;
        void resetTriggerExchange() override;
        bool isPressed();
        bool checkPressed();
        void addToJson(JsonObject& doc) override;
        void processJson(JsonObject& doc) override;
        bool localAction();
        const String& getName();
};

#endif
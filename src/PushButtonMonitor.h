#ifndef PUSH_BUTTON_MONITOR_H
#define PUSH_BUTTON_MONITOR_H

#include <Arduino.h>
#include "JsonProvider.h"

class PushButtonMonitor : public JsonProvider {
    private:
        int _pin;
        String _name;
        bool _activeLow;
        bool _lastReading;
        bool _state;
        unsigned long _lastDebounceTime;
        bool _localAction;

    public:
        static constexpr const char* TYPE = "PushButtonMonitor";
        PushButtonMonitor(String name, int pin, bool activeLow = true);
        bool isPressed();
        bool checkPressed();
        void addToJson(JsonObject& doc) override;
        void processJson(JsonObject& doc) override;
        bool localAction();
        const String& getName();
};

#endif
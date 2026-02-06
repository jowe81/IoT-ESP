#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include <Arduino.h>
#include "DeviceControl.h"

class RelayControl : public DeviceControl {
    private:
        int pin;
        bool _activeLow;
        bool _pwm;
        int _percentage;
        int _frequency;
        bool _on;
        unsigned long _autoOffTimer;
        unsigned long _turnOnTime;
        int _eepromOffset;
        int _fadeDuration;
        int _lastHardwareDuty;
#ifdef ESP32
        int _ledcChannel;
        static int _nextLedcChannel;
#endif

    public:
        RelayControl(String name, int pin, bool activeLow = false, bool pwm = false, int frequency = 1000, int eepromOffset = -1);
        void begin();
        void turnOn() override;
        void turnOff() override;
        void toggle() override;
        bool isOn() override;
        void setPercentage(int percentage);
        void setFrequency(int frequency);
        void setAutoOffTimer(unsigned long duration);
        void setFadeDuration(int duration);
        void update();
        void refreshState() override;
        void processJson(JsonObject& doc) override;
        void addToJson(JsonObject& doc) override;
        const String& getName();

    private:
        void _updateHardware();
        void loadConfig();
        void saveConfig();
};

#endif
#ifndef RGB_CONTROL_H
#define RGB_CONTROL_H

#include <Arduino.h>
#include "DeviceControl.h"

class RGBControl : public DeviceControl {
    private:
        int _pinR, _pinG, _pinB;
        bool _activeLow;
        int _percentage;
        int _frequency;
        bool _on;
        unsigned long _autoOffTimer;
        unsigned long _turnOnTime;
        int _eepromOffset;
        int _fadeDuration;
        
        // Configured color (0-255)
        int _targetR;
        int _targetG;
        int _targetB;

        // Last hardware state (0-MaxDuty)
        int _lastHardwareDutyR;
        int _lastHardwareDutyG;
        int _lastHardwareDutyB;

#ifdef ESP32
        int _ledcChannelR;
        int _ledcChannelG;
        int _ledcChannelB;
        static int _nextLedcChannel;
#endif

    public:
        RGBControl(String name, int pinR, int pinG, int pinB, bool activeLow = false, int frequency = 1000, int eepromOffset = -1);
        void begin() override;
        void turnOn() override;
        void turnOff() override;
        void toggle() override;
        bool isOn() override;
        
        void setPercentage(int percentage);
        void setRGB(int r, int g, int b);
        void setFrequency(int frequency);
        void setAutoOffTimer(unsigned long duration);
        void setFadeDuration(int duration);
        
        void update() override;
        void refreshState() override;
        void processJson(JsonObject& doc) override;
        void addToJson(JsonObject& doc) override;
        const String& getName() override;

    private:
        void _updateHardware();
        void loadConfig();
        void saveConfig();
};

#endif
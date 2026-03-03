#ifndef CAPACITIVE_SENSOR_H
#define CAPACITIVE_SENSOR_H

#include <Arduino.h>
#include "Device.h"
#include "Logger.h" // For logging
#include <EEPROM.h> // For saving/loading config
#include <vector>

class CapacitiveSensor : public Device {
private:
    String _name;
    int _pin; // GPIO pin for touch sensor
    int _threshold; // Touch threshold
    float _lastAverageValue; // Last calculated average
    bool _isTouched; // Current touch state
    bool _lastReportedTouchedState; // Last state reported to JSON/exchange
    bool _triggerExchange; // Flag to trigger data exchange on state change
    bool _triggerOnStateChange; // New flag: if true, trigger exchange on touch state change
    unsigned long _interval; // Reading interval
    unsigned long _lastUpdateTime; // Last update time
    int _eepromOffset; // EEPROM offset for config

    std::vector<int> _readingsBuffer;
    int _bufferIndex;
    long _runningSum;
    static const int BUFFER_SIZE = 300;
    static const int BURST_SIZE = 15;

    // EEPROM configuration structure
    struct Config {
        bool triggerOnStateChange;
        int threshold;
        uint32_t magic;
    };

    void loadConfig();
    void saveConfig();
    int _readSensor(); // Helper to read raw sensor value
    
public:
    CapacitiveSensor(String name, int pin, int threshold = 50, unsigned long interval = 100, bool triggerOnStateChange = true, int eepromOffset = -1);
    void begin() override;
    void update() override;
    void addToJson(JsonArray& doc) override;
    void processJson(JsonObject& doc) override;
    const String& getName() override;

    float getAverage();
    bool isTouched();
    bool shouldTriggerExchange() override;
    void resetTriggerExchange() override;
};

#endif
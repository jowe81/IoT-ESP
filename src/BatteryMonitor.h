#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>
#include "Device.h"
#include <vector>

class BatteryMonitor : public Device {
  private:
    static constexpr float MIN_SANITY_VOLTAGE = 9.0;
    static constexpr float MAX_SANITY_VOLTAGE = 20.0;
    static constexpr int READINGS_PER_CYCLE = 5;
    static constexpr float HYSTERESIS = 0.5;
    int _pin;
    int _eepromOffset;
    String _name;
    float _ratio;
    float _lowThreshold;
    float _criticalThreshold;
    float _voltageAdjustment;
    int _readingsBufferSize;
    std::vector<float> _readings;
    int _readingsIndex;
    int _readingsCount;
    unsigned long _lastReadingTime;
    unsigned long _lastAcceptedReadingTime;
    float _lastKnownVoltage;
    bool _lowState;
    bool _criticalState;
    bool _lowEvent;
    bool _criticalEvent;
    void loadConfig();
    void saveConfig();

  public:
    static constexpr const char* TYPE = "BatteryMonitor";
    BatteryMonitor(String name, int pin, float ratio, float lowThreshold, float criticalThreshold, int eepromOffset, int readingsBufferSize);
    void begin();
    void update();
    float getVoltage();
    bool batteryIsConnected();
    bool isLow();
    bool isCritical();
    bool gotLow();
    bool gotCritical();
    void addToJson(JsonObject& doc) override;
    void processJson(JsonObject& doc) override;
    const String& getName();
};

#endif
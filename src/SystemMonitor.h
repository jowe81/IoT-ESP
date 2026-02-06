#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>
#include "Device.h"

class SystemMonitor : public Device {
private:
    String _deviceId;
    String _name;
    int _loopDelay;

public:
    static constexpr const char* TYPE = "SystemMonitor";
    SystemMonitor(String name, String deviceId);
    void begin() override {}
    void update() override {}
    void addToJson(JsonObject& doc) override;
    uint32_t getFreeHeap();
    uint32_t getLargestBlock();
    bool fragmentationIsCritical();
    unsigned long getUptime();
    void processJson(JsonObject& doc) override;
    int getLoopDelay();
    const String& getName() override;
};

#endif
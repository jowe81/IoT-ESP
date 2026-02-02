#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>
#include "JsonProvider.h"

class SystemMonitor : public JsonProvider {
private:
    String _deviceId;
    String _name;

public:
    static constexpr const char* TYPE = "SystemMonitor";
    SystemMonitor(String name, String deviceId);
    void addToJson(JsonObject& doc) override;
    uint32_t getFreeHeap();
    uint32_t getLargestBlock();
    bool fragmentationIsCritical();
    unsigned long getUptime();
    void processJson(JsonObject& doc) override;
};

#endif
#include "SystemMonitor.h"

SystemMonitor::SystemMonitor(String name, String deviceId) : _deviceId(deviceId), _name(name) {
    if (_name.length() == 0) {
        _name = "_system";
    }
}

void SystemMonitor::addToJson(JsonObject& doc) {
    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = TYPE;
    nested["deviceId"] = _deviceId;
    nested["freeHeap"] = getFreeHeap();
    nested["largestBlock"] = getLargestBlock();
    nested["uptime"] = getUptime();
}

uint32_t SystemMonitor::getFreeHeap() {
    return ESP.getFreeHeap();
}

uint32_t SystemMonitor::getLargestBlock() {
    return ESP.getMaxFreeBlockSize();
}

unsigned long SystemMonitor::getUptime() {
    return millis();
}

bool SystemMonitor::fragmentationIsCritical() {
    return getLargestBlock() < 4096;
}

void SystemMonitor::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        if (doc[_name].is<JsonObject>()) {
            JsonObject command = doc[_name].as<JsonObject>();

            if (command.containsKey("reboot") && command["reboot"].as<bool>()) {
                ESP.restart();
            }

            if (command.containsKey("sleep") && command["sleep"].is<unsigned long>()) {
                // Use 1000ULL to force 64-bit arithmetic, preventing overflow when converting ms to us
                ESP.deepSleep(command["sleep"].as<unsigned long>() * 1000ULL);
            }
        }
    }
}

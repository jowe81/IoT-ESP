#include "SystemMonitor.h"

SystemMonitor::SystemMonitor(String name, String deviceId) : _deviceId(deviceId), _name(name), _loopDelay(20) {
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
    nested["loopDelay"] = _loopDelay;
}

uint32_t SystemMonitor::getFreeHeap() {
    return ESP.getFreeHeap();
}

uint32_t SystemMonitor::getLargestBlock() {
#ifdef ESP32
    return ESP.getMaxAllocHeap();
#else
    return ESP.getMaxFreeBlockSize();
#endif
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
                uint64_t sleepTime = command["sleep"].as<unsigned long>() * 1000ULL;
                #ifdef ESP32
                    esp_deep_sleep(sleepTime);
                #else
                    ESP.deepSleep(sleepTime);
                #endif
            }

            if (command.containsKey("setLoopDelay")) {
                int newDelay = command["setLoopDelay"].as<int>();
                if (newDelay >= 0) {
                    _loopDelay = newDelay;
                }
            }
        }
    }
}

int SystemMonitor::getLoopDelay() {
    return _loopDelay;
}

const String& SystemMonitor::getName() {
    return _name;
}

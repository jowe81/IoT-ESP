#include "Configuration.h"
#include "DS18B20.h"
#include "RelayControl.h"

#ifdef CONFIG_KITCHEN

// This is an ESP32 configuration.
// --- Identity ---
const char* DEVICE_ID = "kitchen_01";
DataExchanger dataExchanger("dataExchanger", DEVICE_ID, 60000, "http://server.wnet.wn:8101/automation_api", "mqtt://server.wnet.wn:1883", wifi, 2);

// --- Devices ---
static SystemMonitor sysMon("systemMonitor", DEVICE_ID);

// 1 Temperature Reader on D25
static DS18B20 tempKitchen(25, "tempKitchen", 0, 300);

// 2 PWM Relays
static RelayControl counterLights("counterLights", std::vector<int>{D7, 21}, false, true, 1000, 310); // D7 is GPIO19 on ESP32

void setupConfiguration() {
    // 1. Assign specific pointers for main loop logic
    systemMonitor = &sysMon;
    systemBattery = nullptr;
    statusIndicator = nullptr;

    // 2. Configure wiring
    // No local relay targets for these buttons in this config

    // 3. Populate generic device list (for update loop)
    allDevices.push_back(&sysMon);
    allDevices.push_back(&tempKitchen);
    allDevices.push_back(&counterLights);

    switchableDevices.push_back(&counterLights);

    // 4. Register providers to DataExchanger
    dataExchanger.addProvider(&sysMon);
    dataExchanger.addProvider(&tempKitchen);
    dataExchanger.addProvider(&counterLights);
}

#endif
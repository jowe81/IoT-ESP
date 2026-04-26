#include "Configuration.h"
#include "RelayControl.h"
#include "DS18B20.h"

#ifdef CONFIG_FURNACE_CLOSET

// --- Identity ---
const char* DEVICE_ID = "furnace_closet_01";
// Using GPIO 2 (Built-in LED on NodeMCU) for status indication
DataExchanger dataExchanger("dataExchanger", DEVICE_ID, 60000, "http://server.wnet.wn:8101/automation_api", "mqtt://server.wnet.wn:1883", wifi, 2);

// --- Devices ---
static SystemMonitor sysMon("systemMonitor", DEVICE_ID);

// Relay Control on Pins 26, 25
// Parameters: name, pin, activeLow, pwm, frequency, eepromOffset
static RelayControl furnaceRelay("furnace", 26, true, false, 1000, 300);
static RelayControl fanRelay("fan", 25, true, false, 1000, 400);
// Temperature Reader on 19
static DS18B20 temp1(19, "recroom", 0, 500);


void setupConfiguration() {
    // 1. Assign specific pointers for main loop logic
    systemMonitor = &sysMon;
    systemBattery = nullptr;
    statusIndicator = nullptr;

    // 2. Configure wiring
    // No local relay targets for these buttons in this config

    // 3. Populate generic device list (for update loop)
    allDevices.push_back(&sysMon);
    allDevices.push_back(&furnaceRelay);
    allDevices.push_back(&fanRelay);
    allDevices.push_back(&temp1);

    // 4. Populate switchable list
    switchableDevices.push_back(&furnaceRelay);
    switchableDevices.push_back(&fanRelay);

    // 5. Register providers to DataExchanger
    dataExchanger.addProvider(&sysMon);
    dataExchanger.addProvider(&furnaceRelay);
    dataExchanger.addProvider(&fanRelay);
    dataExchanger.addProvider(&temp1);
}

#endif
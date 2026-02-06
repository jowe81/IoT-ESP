#include "Configuration.h"
#include "RelayControl.h"
#include "PushButtonMonitor.h"
#include "TemperatureReader.h"

#ifdef CONFIG_WOODSHED

// --- Identity ---
const char* DEVICE_ID = "woodshed_01";
DataExchanger dataExchanger("dataExchanger", DEVICE_ID, 60000, "http://server.wnet.wn:8101/automation_api", wifi, 32);

// --- Devices ---
static SystemMonitor sysMon("systemMonitor", DEVICE_ID);
static BatteryMonitor batMon("batteryMonitor", A0, 0.00484, 11.9, 11.5, 0, 60);
static RelayControl lightInside("lightInside", D1, false, true, 200, 200);
static RelayControl lightOutside("lightOutside", D2, false, true, 200, 220);
static RelayControl nightLight("nightLight", D6, false, false, 200, 240);
static PushButtonMonitor lightSwitchForOutside("lightSwitchOutside", D3, true);
static PushButtonMonitor lightSwitchForInside("lightSwitchInside", D7, true);
static TemperatureReader tempSensor(D5, "tempOutside");
static RelayControl statusLed("statusLed", LED_BUILTIN, false);

void setupConfiguration() {
    // 1. Assign specific pointers for main loop logic
    systemMonitor = &sysMon;
    systemBattery = &batMon;

    // 2. Configure wiring (Buttons -> Relays)
    lightSwitchForOutside.setTarget(&lightOutside);
    lightSwitchForInside.setTarget(&lightInside);

    // 3. Populate generic device list (for update loop)
    allDevices.push_back(&batMon);
    allDevices.push_back(&tempSensor);
    allDevices.push_back(&sysMon);
    allDevices.push_back(&lightInside);
    allDevices.push_back(&lightOutside);
    allDevices.push_back(&nightLight);
    allDevices.push_back(&lightSwitchForOutside);
    allDevices.push_back(&lightSwitchForInside);
    allDevices.push_back(&statusLed);

    // 4. Populate switchable list (for group operations like turnOffLights)
    switchableDevices.push_back(&lightInside);
    switchableDevices.push_back(&lightOutside);
    switchableDevices.push_back(&nightLight);

    // 5. Register providers to DataExchanger
    dataExchanger.addProvider(&batMon);
    dataExchanger.addProvider(&tempSensor);
    dataExchanger.addProvider(&sysMon);
    dataExchanger.addProvider(&lightInside);
    dataExchanger.addProvider(&lightOutside);
    dataExchanger.addProvider(&nightLight);
    dataExchanger.addProvider(&lightSwitchForOutside);
    dataExchanger.addProvider(&lightSwitchForInside);
    dataExchanger.addProvider(&statusLed);
}

#endif
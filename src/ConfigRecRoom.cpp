#include "Configuration.h"
#include "PushButtonMonitor.h"
#include "TemperatureReader.h"

#ifdef CONFIG_RECROOM

// --- Identity ---
const char* DEVICE_ID = "recroom_01";
// Using GPIO 2 (Built-in LED on NodeMCU) for status indication
DataExchanger dataExchanger("dataExchanger", DEVICE_ID, 60000, "http://server.wnet.wn:8101/automation_api", wifi, 2);

// --- Devices ---
static SystemMonitor sysMon("systemMonitor", DEVICE_ID);

// 3 Push Buttons (D1, D2, D3)
static PushButtonMonitor btn1("btn1", D1, true);
static PushButtonMonitor btn2("btn2", D3, true);
static PushButtonMonitor btn3("btn3", D7, true);

// 2 Temperature Readers (D5, D6)
static TemperatureReader temp1(D5, "temp1", 0);
static TemperatureReader temp2(D5, "temp2", 1);

void setupConfiguration() {
    // 1. Assign specific pointers for main loop logic
    systemMonitor = &sysMon;
    // systemBattery is not used in this config (remains nullptr)

    // 2. Configure wiring
    // No local relay targets for these buttons in this config

    // 3. Populate generic device list (for update loop)
    allDevices.push_back(&sysMon);
    allDevices.push_back(&btn1);
    allDevices.push_back(&btn2);
    allDevices.push_back(&btn3);
    allDevices.push_back(&temp1);
    allDevices.push_back(&temp2);

    // 4. Register providers to DataExchanger
    dataExchanger.addProvider(&sysMon);
    dataExchanger.addProvider(&btn1);
    dataExchanger.addProvider(&btn2);
    dataExchanger.addProvider(&btn3);
    dataExchanger.addProvider(&temp1);
    dataExchanger.addProvider(&temp2);
}

#endif
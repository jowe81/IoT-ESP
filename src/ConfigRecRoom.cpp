#include "Configuration.h"
#include "PushButtonMonitor.h"
#include "DS18B20.h"
#include "RelayControl.h"
#include "SHT31.h"

#ifdef CONFIG_RECROOM

// --- Identity ---
const char* DEVICE_ID = "recroom_01";
// Using GPIO 2 (Built-in LED on NodeMCU) for status indication
DataExchanger dataExchanger("dataExchanger", DEVICE_ID, 60000, "http://server.wnet.wn:8101/automation_api", "mqtt://server.wnet.wn:1883", wifi, 2);

// --- Devices ---
static SystemMonitor sysMon("systemMonitor", DEVICE_ID);

// Push Button (D3; D7 available if another needed)
static PushButtonMonitor btn1("btn1", D3, true);

// SHT31 Sensor (I2C: D2=SDA, D1=SCL)
static SHT31 shtSensor("shtSensor", 0x44, 60000, 400);

// 2 Temperature Readers (D5, D6)
static DS18B20 temp1(D5, "woodstove", 0, 300);

// Status LED (Built-in LED is usually GPIO 2, Active Low)
static RelayControl statusLed("statusLed", 2, true);

void setupConfiguration() {
    // 1. Assign specific pointers for main loop logic
    systemMonitor = &sysMon;
    // systemBattery is not used in this config (remains nullptr)
    statusIndicator = &statusLed;

    // 2. Configure wiring
    // No local relay targets for these buttons in this config

    // 3. Populate generic device list (for update loop)
    allDevices.push_back(&sysMon);
    allDevices.push_back(&shtSensor);
    allDevices.push_back(&btn1);
    allDevices.push_back(&temp1);
    allDevices.push_back(&statusLed);

    // 4. Register providers to DataExchanger
    dataExchanger.addProvider(&sysMon);
    dataExchanger.addProvider(&shtSensor);
    dataExchanger.addProvider(&btn1);
    dataExchanger.addProvider(&temp1);
}

#endif
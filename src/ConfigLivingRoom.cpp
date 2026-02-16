#include "Configuration.h"
#include "PushButtonMonitor.h"
#include "TemperatureReader.h"
#include "RelayControl.h"
#include "RGBControl.h"
#include "SHT31.h"
#include <Wire.h>

#ifdef CONFIG_LIVINGROOM

// --- Identity ---
const char* DEVICE_ID = "livingroom_01";
// Using GPIO 2 (Built-in LED on NodeMCU) for status indication
DataExchanger dataExchanger("dataExchanger", DEVICE_ID, 60000, "http://server.wnet.wn:8101/automation_api", "mqtt://server.wnet.wn:1883", wifi, 2);

// --- Devices ---
static SystemMonitor sysMon("systemMonitor", DEVICE_ID);

// Push Button (D3) - GPIO0 must be HIGH at boot (Button is Normally Open)
static PushButtonMonitor btn1("btn1", D3, true);

// Temperature Reader (D5)
static TemperatureReader temp1(D5, "temp1", 0);

// SHT31 Sensor (I2C: D2=SDA, D1=SCL)
static SHT31 shtSensor("shtSensor", 0x44, 60000, 400);

// RGB Strip (D8, D6, D7) - Moved Red to D8 to free D1/D2 for I2C
// Note: D8 (GPIO15) has a built-in pulldown on NodeMCU. D6 & D7 need external 10k pulldowns.
static RGBControl rgbStrip("woodRackLights", D8, D6, D7, false, 1000, 300);

// Status LED (Built-in LED is usually GPIO 2, Active Low)
static RelayControl statusLed("statusLed", 2, true);

void setupConfiguration() {
    // 1. Assign specific pointers for main loop logic
    systemMonitor = &sysMon;
    // systemBattery is not used in this config (remains nullptr)
    statusIndicator = &statusLed;

    // 2. Configure wiring
    btn1.setTarget(&rgbStrip);

    // Initialize I2C on D2 (SDA) and D1 (SCL)
    Wire.begin(D2, D1);

    // 3. Populate generic device list (for update loop)
    allDevices.push_back(&sysMon);
    allDevices.push_back(&btn1);
    allDevices.push_back(&temp1);
    allDevices.push_back(&shtSensor);
    allDevices.push_back(&rgbStrip);
    allDevices.push_back(&statusLed);

    switchableDevices.push_back(&rgbStrip);

    // 4. Register providers to DataExchanger
    dataExchanger.addProvider(&sysMon);
    dataExchanger.addProvider(&btn1);
    dataExchanger.addProvider(&temp1);
    dataExchanger.addProvider(&shtSensor);
    dataExchanger.addProvider(&rgbStrip);
    dataExchanger.addProvider(&statusLed);
}

#endif
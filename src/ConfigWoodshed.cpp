#include "Configuration.h"
#include "RelayControl.h"
#include "PushButtonMonitor.h"
#include "DS18B20.h"
#include "INA219CurrentReader.h"
#include "RGBControl.h"
#include "BME280.h"

#ifdef CONFIG_WOODSHED

// --- Identity ---
const char* DEVICE_ID = "woodshed_01";
DataExchanger dataExchanger("dataExchanger", DEVICE_ID, 60000, "http://server.wnet.wn:8101/automation_api", "mqtt://server.wnet.wn:1883", wifi, 32);

// --- Devices ---
static SystemMonitor sysMon("systemMonitor", DEVICE_ID);
static DS18B20 tempOutside(D5, "tempOutside", 0, 530);
static DS18B20 tempControlBox(D5, "controlBox", 1, 540);
static BatteryMonitor batMon("batteryMonitor", A0, 6.0, 11.9, 11.5, 420, 60, &tempOutside);

static RGBControl rgbStrip("rgbStrip", D6, D2, D1, false, 1000, 500);
static RelayControl lightInside("lightInside", 32, false, true, 200, 300);
static RelayControl lightOutside("lightOutside", 33, false, true, 200, 320);
static RelayControl statusLed("statusLed", LED_BUILTIN, false);

static PushButtonMonitor lightSwitchForOutside("lightSwitchOutside", D3, true);
static PushButtonMonitor lightSwitchForInside("lightSwitchInside", D7, true);
static INA219CurrentReader loadMeter("loadMeter", 0x40, 1000, 360, 128);
static INA219CurrentReader chargeMeter("chargeMeter", 0x41, 1000, 390, 128);
static BME280Reader bmeSensor("bmeSensor", 0x76, 60000, 480);

void setupConfiguration() {
    // 1. Assign specific pointers for main loop logic
    systemMonitor = &sysMon;
    systemBattery = &batMon;
    statusIndicator = &statusLed;

    // 2. Configure devices
    lightSwitchForOutside.setTarget(&lightOutside);
    lightSwitchForInside.setTarget(&lightInside);

    // Configure the 'chargeMeter' to use a 10A/75mV external shunt.
    // Resistance = 0.075V / 10A = 0.0075 Ohms.
    chargeMeter.setExternalShunt(0.0075, 10.0);

    // 3. Populate generic device list for the main update loop
    allDevices.push_back(&batMon);
    allDevices.push_back(&tempOutside);
    allDevices.push_back(&tempControlBox);
    allDevices.push_back(&sysMon);
    allDevices.push_back(&rgbStrip);
    allDevices.push_back(&lightInside);
    allDevices.push_back(&lightOutside);
    allDevices.push_back(&statusLed);
    allDevices.push_back(&lightSwitchForOutside);
    allDevices.push_back(&lightSwitchForInside);
    allDevices.push_back(&loadMeter);
    allDevices.push_back(&chargeMeter);
    allDevices.push_back(&bmeSensor);

    // 4. Populate switchable list (for group operations like turnOffLights)
    switchableDevices.push_back(&lightInside);
    switchableDevices.push_back(&lightOutside);
    switchableDevices.push_back(&rgbStrip);

    // 5. Register providers to DataExchanger
    dataExchanger.addProvider(&batMon);
    dataExchanger.addProvider(&tempOutside);
    dataExchanger.addProvider(&tempControlBox);
    dataExchanger.addProvider(&sysMon);
    dataExchanger.addProvider(&rgbStrip);
    dataExchanger.addProvider(&lightInside);
    dataExchanger.addProvider(&lightOutside);    
    dataExchanger.addProvider(&lightSwitchForOutside);
    dataExchanger.addProvider(&lightSwitchForInside);
    dataExchanger.addProvider(&loadMeter);
    dataExchanger.addProvider(&chargeMeter);
    dataExchanger.addProvider(&bmeSensor);
}

#endif
#include "Configuration.h"
#include "SHT31.h"
#include "RelayControl.h"
#include <Wire.h>
#include "CapacitiveSensor.h" // New include

#ifdef CONFIG_OFFICE_JOHANNES

// This is an ESP32 configuration.
// --- Identity ---
const char* DEVICE_ID = "office_johannes_01";
DataExchanger dataExchanger("dataExchanger", DEVICE_ID, 60000, "http://server.wnet.wn:8101/automation_api", "mqtt://server.wnet.wn:1883", wifi, 2);

// --- Devices ---
static SystemMonitor sysMon("systemMonitor", DEVICE_ID);

// SHT31 Sensor (I2C: SDA=21, SCL=22)
static SHT31 shtSensor("shtSensor", 0x44, 60000, 400);

// Relay Control on D5
static RelayControl humidifier("humidifier", D5, true);
static RelayControl fan("fan", 27, true);

// Capacitive Sensor on D1 (GPIO4) - ESP32 Touch Pin T0
static CapacitiveSensor capacitiveSensor("humidifierTank", D1, 50, 100, false, 450); // Example threshold 50, interval 100ms, triggerOnStateChange=true, EEPROM offset 450

void setupConfiguration() {
    // 1. Assign specific pointers for main loop logic
    systemMonitor = &sysMon;
    systemBattery = nullptr;
    statusIndicator = nullptr;

    // 2. Configure wiring
    // Initialize I2C on ESP32 pins (SDA=21, SCL=22)
    Wire.begin(21, 22);

    // 3. Populate generic device list (for update loop)
    allDevices.push_back(&sysMon);
    allDevices.push_back(&shtSensor);
    allDevices.push_back(&humidifier);
    allDevices.push_back(&fan);
    allDevices.push_back(&capacitiveSensor); // Add new sensor

    switchableDevices.push_back(&humidifier);
    switchableDevices.push_back(&fan);

    // 4. Register providers to DataExchanger
    dataExchanger.addProvider(&sysMon);
    dataExchanger.addProvider(&shtSensor);
    dataExchanger.addProvider(&humidifier);
    dataExchanger.addProvider(&fan);
    dataExchanger.addProvider(&capacitiveSensor); // Register new sensor
}

#endif
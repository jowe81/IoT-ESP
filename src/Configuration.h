#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <Arduino.h>
#include <vector>
#include "WifiConnection.h"
#include "DataExchanger.h"
#include "Device.h"
#include "DeviceControl.h"
#include "BatteryMonitor.h"
#include "SystemMonitor.h"

#ifdef ESP32
// Define pin mappings for ESP32 so they are available in all config files
#ifndef D1
#define D1 4
#define D2 16
#define D3 18
#define D5 26
#define D6 17
#define D7 19
#endif
#ifndef A0
#define A0 34 
#endif
#ifndef LED_BUILTIN
#define LED_BUILTIN 2 
#endif
#endif

// Global Objects
extern WifiConnection wifi;
extern DataExchanger dataExchanger;
extern const char* DEVICE_ID;

// Specific pointers for critical system logic (can be null)
extern BatteryMonitor* systemBattery;
extern SystemMonitor* systemMonitor;

// Lists for generic iteration
extern std::vector<Device*> allDevices;
extern std::vector<DeviceControl*> switchableDevices;

// Setup function to initialize the configuration (instantiate objects)
void setupConfiguration();

#endif
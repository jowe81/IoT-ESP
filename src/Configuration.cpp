#include "Configuration.h"

// Global Instances
WifiConnection wifi("jjnet_automation", "2023-02-18!a", WIFI_LIGHT_SLEEP);

// Pointers exposed to main
// These will be assigned by the specific config file's setupConfiguration()
BatteryMonitor* systemBattery = nullptr;
SystemMonitor* systemMonitor = nullptr;
DeviceControl* statusIndicator = nullptr;

// Lists for generic iteration
// These will be populated by the specific config file's setupConfiguration()
std::vector<Device*> allDevices;
std::vector<DeviceControl*> switchableDevices;
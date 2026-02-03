#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>


#include "BatteryMonitor.h"
#include "WifiConnection.h"
#include "Logger.h"
#include "RelayControl.h"
#include "BistableRelayControl.h"
#include "PushButtonMonitor.h"
#include "TemperatureReader.h"
#include "DataExchanger.h"
#include "SystemMonitor.h"

// Instantiate objects.
WifiConnection wifi("jjnet_automation", "2023-02-18!a", WIFI_LIGHT_SLEEP);

const char* DEVICE_ID = "woodshed_01";
SystemMonitor systemMonitor("systemMonitor", DEVICE_ID);

// I/O
BatteryMonitor battery("batteryMonitor", A0, 48.198, 11.9, 11.5, 0, 60); 
BistableRelayControl lightInside("lightInside", D1, D1);
BistableRelayControl lightOutside("lightOutside", D2, D2);
BistableRelayControl nightLight("nightLight", D6, D6);
PushButtonMonitor lightSwitchForOutside("lightSwitchOutside", D3, true);
PushButtonMonitor lightSwitchForInside("lightSwitchInside", D7, true);
TemperatureReader tempSensor(D5, "tempOutside");
RelayControl statusLed(LED_BUILTIN, "statusLed", true);

// Data Exchanger (Send every 60 seconds)
// Eventually this will go to: http://jj-auto.wnet.wn:3000/auto/remote/remoteID=<deviceId>
DataExchanger dataExchanger("dataExchanger", DEVICE_ID, 60000, "http://server.wnet.wn:8101/automation_api", wifi, 32);

void turnOffLights() {
    lightInside.turnOff();
    lightOutside.turnOff();
}

void setup() {
    Log.begin();
    Log.info("Starting up...");
    // Reserve 512 bytes for config (BatteryMonitor uses 0-12, DataExchanger uses 32+)
    EEPROM.begin(512);
    wifi.begin();
    battery.begin();
    tempSensor.begin();
    dataExchanger.begin();

    // Register sensors to the data exchanger
    dataExchanger.addProvider(&battery);
    dataExchanger.addProvider(&tempSensor);
    dataExchanger.addProvider(&systemMonitor);
    dataExchanger.addProvider(&lightInside);
    dataExchanger.addProvider(&lightOutside);
    dataExchanger.addProvider(&nightLight);
    dataExchanger.addProvider(&lightSwitchForOutside);
    dataExchanger.addProvider(&lightSwitchForInside);
    dataExchanger.addProvider(&statusLed);

    // Turn off lights on startup.
    turnOffLights();

    Log.info("Setup done.");
}

void loop() {
    // Turn on the internal LED during network activity.
    statusLed.turnOn();
    // Check connectivity and attempt to (re)connect if needed.
    wifi.update();
    dataExchanger.exchange();
    // Leave the LED on if there's no connection.
    if (wifi.isConnected()) {
        statusLed.turnOff();
    }

    battery.update();

    // Turn lights off if the battery is low, but only
    // force a data exchange if it got low since the last reading.
    if (battery.isLow()) {
        // This does nothing if the lights are already off.
        turnOffLights();

        if (battery.gotLow()) {
            // Only exchange data once.
            Log.warn("Low Battery - turning off lights.");
            dataExchanger.exchange(true, "low_battery");
        }
    }

    // Go to deep sleep if the battery is critically low.
    if (battery.isCritical()) {
        Log.error("Critical Battery - shutting down.");
        turnOffLights();
        dataExchanger.exchange(true, "critical_battery_shutdown");
        // 3600e6 is 3,600,000,000 microseconds (1 hour)
        ESP.deepSleep(3600e6);        
    }

    // Restart the chip if fragmentation has reached a critical level.
    if (systemMonitor.fragmentationIsCritical()) {
        Log.error("Fragmentation is critical - rebooting.");
        dataExchanger.exchange(true, "critical_fragmentation_reboot");
        ESP.restart();
    }
    
    // Check the push buttons and take action if needed.
    if (lightSwitchForOutside.checkPressed()) {
        if (lightSwitchForOutside.localAction()) {
            lightOutside.toggle();
        }
        dataExchanger.exchange(true, lightSwitchForOutside.getName().c_str());
    }
    if (lightSwitchForInside.checkPressed()) {
        if (lightSwitchForInside.localAction()) {
            lightInside.toggle();
        }
        dataExchanger.exchange(true, lightSwitchForInside.getName().c_str());
    }

    // Allow the chip to go to light sleep.
    delay(1000);
}

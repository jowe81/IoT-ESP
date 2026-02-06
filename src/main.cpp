#include <Arduino.h>
#include <EEPROM.h>
#ifdef ESP32
#include <WiFi.h>
#include "esp_pm.h"
// Define pin mappings for ESP32 (Adjust these GPIOs to match your actual wiring)
#ifndef D1
#define D1 4
#define D2 16
#define D3 18
#define D5 26
#define D6 17
#define D7 19
#endif
#ifndef A0
#define A0 34 // Must use an ADC1 pin (GPIO 32-39) when WiFi is active
#endif
#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Standard built-in LED pin for ESP32 Dev Kits
#endif
#else
#include <ESP8266WiFi.h>
#endif


#include "BatteryMonitor.h"
#include "WifiConnection.h"
#include "Logger.h"
#include "RelayControl.h"
#include "PushButtonMonitor.h"
#include "TemperatureReader.h"
#include "DataExchanger.h"
#include "SystemMonitor.h"

// Instantiate objects.
WifiConnection wifi("jjnet_automation", "2023-02-18!a", WIFI_LIGHT_SLEEP);

const char* DEVICE_ID = "woodshed_01";
SystemMonitor systemMonitor("systemMonitor", DEVICE_ID);

// I/O
BatteryMonitor battery("batteryMonitor", A0, 0.00484, 11.9, 11.5, 0, 60); 
RelayControl lightInside("lightInside", D1, false, true, 200, 200);
RelayControl lightOutside("lightOutside", D2, false, true, 200, 220);
RelayControl nightLight("nightLight", D6, false, false, 200, 240);
PushButtonMonitor lightSwitchForOutside("lightSwitchOutside", D3, true);
PushButtonMonitor lightSwitchForInside("lightSwitchInside", D7, true);
TemperatureReader tempSensor(D5, "tempOutside");
RelayControl statusLed("statusLed", LED_BUILTIN, false);

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

#ifdef ESP32
    // Configure power management for automatic light sleep.
    // This makes delay() behave more like on the ESP8266, where it enters light sleep.
    // This requires specific build flags in platformio.ini (see explanation).
    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 80, // Run at 80MHz to save power, instead of default 240MHz
        .min_freq_mhz = 40,
        .light_sleep_enable = true
    };
    esp_err_t err = esp_pm_configure(&pm_config);
    if (err == ESP_OK) {
        Log.info("ESP32 Power Management configured for automatic light sleep.");
    } else {
        Log.error("ESP32 Power Management configuration FAILED. Check build flags.");
    }
#endif

    wifi.begin();
    battery.begin();
    tempSensor.begin();
    dataExchanger.begin();
    lightInside.begin();
    lightOutside.begin();
    nightLight.begin();
    statusLed.begin();

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

    // Send initial transmission.
    dataExchanger.exchange(true, "startup");

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
    
    // Update relays to handle auto-off timers
    lightInside.update();
    lightOutside.update();
    nightLight.update();
    statusLed.update();

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
        #ifdef ESP32
            esp_deep_sleep(3600e6);
        #else
            ESP.deepSleep(3600e6);
        #endif
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
    delay(systemMonitor.getLoopDelay());
}

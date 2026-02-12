#include <Arduino.h>
#include <EEPROM.h>
#ifdef ESP32
#include <esp_wifi.h>
#endif

#include "Logger.h"
#include "Configuration.h"

void turnOffLights() {
    for (auto* device : switchableDevices) {
        device->turnOff();
    }
}

void setup() {
    Log.begin();
    Log.info("Starting up...");
    // Reserve 512 bytes for config (BatteryMonitor uses 0-12, DataExchanger uses 32+)
    EEPROM.begin(512);
    
    // Initialize configuration (instantiate objects, wire them up)
    setupConfiguration();

    wifi.begin();

#ifdef ESP32
    // Enable Modem Sleep (Automatic Radio Sleep).
    // This saves power by turning off the radio between WiFi beacons.
    // It is supported by the standard Arduino framework.
    // Note: Must be called AFTER WiFi is initialized (wifi.begin).
    esp_err_t err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (err == ESP_OK) {
        Log.info("ESP32 Power Management: Modem Sleep ENABLED.");
    } else {
        Log.error("ESP32 Power Management configuration FAILED.");
    }
#endif

    dataExchanger.begin();
    
    // Initialize all generic devices
    for (auto* device : allDevices) {
        device->begin();
    }

    // Turn off lights on startup.
    turnOffLights();

    Log.info("Setup done.");
}

void loop() {
    // Turn on the internal LED during network activity.
    if (statusIndicator) statusIndicator->turnOn();

    // Check connectivity and attempt to (re)connect if needed.
    wifi.update();
    if (!dataExchanger.exchange()) {
        Log.warn("Data exchange failed. Refreshing device states.");
        for (auto* device : allDevices) {
            device->refreshState();
        }
    }

    // Leave the LED on if there's no connection, otherwise turn it off (creating a blink)
    if (statusIndicator && wifi.isConnected()) {
        statusIndicator->turnOff();
    }

    // Generic Device Update Loop
    for (auto* device : allDevices) {
        device->update();
        
        if (device->shouldTriggerExchange()) {
            dataExchanger.exchange(true, device->getName().c_str());
            device->resetTriggerExchange();
        }
    }

    // Turn lights off if the battery is low, but only
    // force a data exchange if it got low since the last reading.
    if (systemBattery && systemBattery->isLow()) {
        // This does nothing if the lights are already off.
        turnOffLights();

        if (systemBattery->gotLow()) {
            // Only exchange data once.
            Log.warn("Low Battery - turning off lights.");
            dataExchanger.exchange(true, "low_battery");
        }
    }

    // Go to deep sleep if the battery is critically low.
    if (systemBattery && systemBattery->isCritical()) {
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
    if (systemMonitor && systemMonitor->fragmentationIsCritical()) {
        Log.error("Fragmentation is critical - rebooting.");
        dataExchanger.exchange(true, "critical_fragmentation_reboot");
        ESP.restart();
    }
    
    // Allow the chip to go to light sleep.
    if (systemMonitor) {
        delay(systemMonitor->getLoopDelay());
    } else {
        delay(20);
    }
}

#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include <Arduino.h>
#include <ESP8266WiFi.h>

class WifiConnection {
  private:
    const char* _ssid;
    const char* _password;
    unsigned long _lastReconnectAttempt;
    WiFiSleepType _sleepMode;
    const unsigned long _reconnectInterval = 10000; // Retry every 10 seconds

  public:
    WifiConnection(const char* ssid, const char* password, WiFiSleepType sleepMode = WIFI_NONE_SLEEP);
    void begin();
    void update();
    bool isConnected();
    String postJson(const char* endpoint, const String& jsonBody);
};

#endif

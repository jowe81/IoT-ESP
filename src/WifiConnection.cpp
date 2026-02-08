#include "WifiConnection.h"
#include "Logger.h"
#ifdef ESP32
#include <HTTPClient.h>
#else
#include <ESP8266HTTPClient.h>
#endif
#include <WiFiClient.h>

WifiConnection::WifiConnection(const char* ssid, const char* password, WiFiSleepType sleepMode) 
    : _ssid(ssid), _password(password), _lastReconnectAttempt(0), _sleepMode(sleepMode), _wasConnected(false) {
}

void WifiConnection::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid, _password);

    if (_sleepMode) {
#ifdef ESP32
        WiFi.setSleep(_sleepMode);
#else
        WiFi.setSleepMode(_sleepMode);
#endif
    
    }
    Log.info("Connecting to WiFi:");
    Log.info(_ssid);
}

void WifiConnection::update() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!_wasConnected) {
            Log.info("WiFi Connected!");
            Log.info(WiFi.localIP().toString().c_str());
            _wasConnected = true;
        }
        return;
    }
    
    _wasConnected = false;

    // If not connected, check if it's time to retry
    unsigned long currentMillis = millis();
    if (currentMillis - _lastReconnectAttempt >= _reconnectInterval) {
        _lastReconnectAttempt = currentMillis;
        Log.warn("WiFi disconnected. Attempting to reconnect...");
        WiFi.disconnect(); 
        WiFi.begin(_ssid, _password);
    }
}

bool WifiConnection::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WifiConnection::postJson(const char* endpoint, const String& jsonBody) {
    if (WiFi.status() != WL_CONNECTED) {
        Log.warn("WifiConnection: Cannot POST, WiFi not connected.");
        return "";
    }

    WiFiClient client;
    HTTPClient http;

    Log.info("WifiConnection: Posting to");
    Log.info(endpoint);

    if (http.begin(client, endpoint)) {
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.POST(jsonBody);
        String response = "";

        if (httpCode > 0) {
            response = http.getString();
            Log.info("WifiConnection: POST response code:");
            Log.info(String(httpCode).c_str());
        } else {
            Log.error("WifiConnection: POST failed, error:");
            Log.error(http.errorToString(httpCode).c_str());
        }
        http.end();
        return response;
    }
    return "";
}

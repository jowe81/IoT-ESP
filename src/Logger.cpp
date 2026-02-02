#include "Logger.h"

Logger Log;

void Logger::begin(long baudRate) {
    Serial.begin(baudRate);
}

void Logger::info(const char* msg) {
    Serial.print("[INFO]  ");
    Serial.println(msg);
}

void Logger::error(const char* msg) {
    Serial.print("[ERROR] ");
    Serial.println(msg);
}

void Logger::warn(const char* msg) {
    Serial.print("[WARN]  ");
    Serial.println(msg);
}
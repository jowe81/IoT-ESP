#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

class Logger {
  public:
    void begin(long baudRate = 115200);
    void info(const char* msg);
    void error(const char* msg);
    void warn(const char* msg);
};

extern Logger Log;

#endif
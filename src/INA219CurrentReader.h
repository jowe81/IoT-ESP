#ifndef INA219CurrentReader_h
#define INA219CurrentReader_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_INA219.h>
#include <Wire.h>
#include "Device.h"

class INA219CurrentReader : public Device {
public:
    // Constructor
    // name: The key used in the JSON output
    // addr: I2C address of the INA219 (default 0x40)
    // intervalMs: How often to take a reading in milliseconds
    // averagingSamples: Number of samples to average (1, 2, 4, 8, 16, 32, 64, 128)
    INA219CurrentReader(String name, uint8_t addr = 0x40, int intervalMs = 1000, int eepromOffset = -1, int averagingSamples = 1);

    // Initialize the sensor. Pass a TwoWire pointer if using a non-default I2C bus.
    void begin(TwoWire *wire);
    void begin() override;
    
    void update() override;
    void addToJson(JsonObject& doc) override;
    void processJson(JsonObject& doc) override;
    const String& getName() override;

private:
    String _name;
    uint8_t _addr;
    int _intervalMs;
    int _eepromOffset;
    int _calibrationMode;
    int _averagingSamples;
    Adafruit_INA219 _ina;
    TwoWire* _wire;
    
    double _currentSum;
    int _readingsCount;
    unsigned long _lastReadingTime;
    
    float getAverageCurrent();
    void loadConfig();
    void saveConfig();
    void applyCalibration();
    void applyAveraging();
};

#endif
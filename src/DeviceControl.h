#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include <Arduino.h>
#include "Device.h"

class DeviceControl : public Device {
protected:
    String _name;

public:
    static constexpr const char* TYPE = "DeviceControl";
    
    DeviceControl(String name) : _name(name) {}
    virtual ~DeviceControl() {}

    virtual void turnOn() = 0;
    virtual void turnOff() = 0;
    virtual void toggle() = 0;
    virtual bool isOn() = 0;

    void addToJson(JsonObject& doc) override {
        JsonObject nested = doc.createNestedObject(_name);
        nested["type"] = TYPE;
        nested["isOn"] = isOn();
    }
};

#endif
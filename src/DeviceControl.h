#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include <Arduino.h>
#include "Device.h"

class DeviceControl : public Device {
protected:
    String _name;

public:
    
    DeviceControl(String name) : _name(name) {}
    virtual ~DeviceControl() {}

    virtual void turnOn() = 0;
    virtual void turnOff() = 0;
    virtual void toggle() = 0;
    virtual bool isOn() = 0;

    void addToJson(JsonArray& doc) override {
        JsonObject nested = doc.createNestedObject();
        nested["type"] = "DeviceControl";
        nested["name"] = _name;
        nested["isOn"] = isOn();
    }
};

#endif
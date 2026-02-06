#ifndef DEVICE_H
#define DEVICE_H

#include <Arduino.h>
#include "JsonProvider.h"

class Device : public JsonProvider {
public:
    virtual void begin() {}
    virtual void update() {}
    virtual void refreshState() {}
    virtual bool shouldTriggerExchange() { return false; }
    virtual void resetTriggerExchange() {}
    virtual const String& getName() = 0;
    virtual ~Device() {}
};

#endif
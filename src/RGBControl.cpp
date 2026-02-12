#include "RGBControl.h"
#include <EEPROM.h>

#ifdef ESP32
// Start from 8 to avoid conflict with RelayControl (which usually starts at 0)
int RGBControl::_nextLedcChannel = 8;
#endif

struct RGBConfig {
    unsigned long autoOffTimer;
    int fadeDuration;
    int percentage;
    int r;
    int g;
    int b;
    uint32_t magic;
};

RGBControl::RGBControl(String name, int pinR, int pinG, int pinB, bool activeLow, int frequency, int eepromOffset) 
    : DeviceControl(name), _pinR(pinR), _pinG(pinG), _pinB(pinB), _activeLow(activeLow), _percentage(100), _frequency(frequency), 
      _on(false), _autoOffTimer(0), _turnOnTime(0), _eepromOffset(eepromOffset), _fadeDuration(0),
      _targetR(255), _targetG(255), _targetB(255),
      _lastHardwareDutyR(0), _lastHardwareDutyG(0), _lastHardwareDutyB(0) {
    
    pinMode(_pinR, OUTPUT);
    pinMode(_pinG, OUTPUT);
    pinMode(_pinB, OUTPUT);

    #ifdef ESP32
        _ledcChannelR = _nextLedcChannel++;
        _ledcChannelG = _nextLedcChannel++;
        _ledcChannelB = _nextLedcChannel++;
        
        ledcSetup(_ledcChannelR, _frequency, 8);
        ledcSetup(_ledcChannelG, _frequency, 8);
        ledcSetup(_ledcChannelB, _frequency, 8);
        
        ledcAttachPin(_pinR, _ledcChannelR);
        ledcAttachPin(_pinG, _ledcChannelG);
        ledcAttachPin(_pinB, _ledcChannelB);
    #else
        analogWriteFreq(_frequency);
    #endif

    // Init
    turnOff();
}

void RGBControl::begin() {
    if (_eepromOffset >= 0) {
        loadConfig();
    }
}

void RGBControl::loadConfig() {
    RGBConfig config;
    EEPROM.get(_eepromOffset, config);
    
    if (config.magic == 0xDEADBEEF) {
        _autoOffTimer = config.autoOffTimer;
        _fadeDuration = config.fadeDuration;
        if (config.percentage >= 0 && config.percentage <= 100) {
            _percentage = config.percentage;
        }
        if (config.r >= 0 && config.r <= 255) _targetR = config.r;
        if (config.g >= 0 && config.g <= 255) _targetG = config.g;
        if (config.b >= 0 && config.b <= 255) _targetB = config.b;
    }
}

void RGBControl::saveConfig() {
    if (_eepromOffset < 0) return;
    RGBConfig config = { _autoOffTimer, _fadeDuration, _percentage, _targetR, _targetG, _targetB, 0xDEADBEEF };
    EEPROM.put(_eepromOffset, config);
    EEPROM.commit();
}

void RGBControl::turnOn() {
    _on = true;
    _turnOnTime = millis();
    _updateHardware();
}

void RGBControl::turnOff() {
    _on = false;
    _updateHardware();
}

void RGBControl::toggle() {
    isOn() ? turnOff() : turnOn();
}

bool RGBControl::isOn() {
    return _on;
}

void RGBControl::setPercentage(int percentage) {
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    
    if (_percentage != percentage) {
        _percentage = percentage;
        saveConfig();
    }

    if (_on) {
        _updateHardware();
    }
}

void RGBControl::setRGB(int r, int g, int b) {
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;

    if (_targetR != r || _targetG != g || _targetB != b) {
        _targetR = r;
        _targetG = g;
        _targetB = b;
        saveConfig();
    }

    if (_on) {
        _updateHardware();
    }
}

void RGBControl::_updateHardware() {
    int effectiveR = _on ? (_targetR * _percentage / 100) : 0;
    int effectiveG = _on ? (_targetG * _percentage / 100) : 0;
    int effectiveB = _on ? (_targetB * _percentage / 100) : 0;

    #ifdef ESP32
        int maxDuty = 255;
    #else
        int maxDuty = 1023;
    #endif

    int targetDutyR = map(effectiveR, 0, 255, 0, maxDuty);
    int targetDutyG = map(effectiveG, 0, 255, 0, maxDuty);
    int targetDutyB = map(effectiveB, 0, 255, 0, maxDuty);

    if (_activeLow) {
        targetDutyR = maxDuty - targetDutyR;
        targetDutyG = maxDuty - targetDutyG;
        targetDutyB = maxDuty - targetDutyB;
    }

    if (_fadeDuration > 0 && (_lastHardwareDutyR != targetDutyR || _lastHardwareDutyG != targetDutyG || _lastHardwareDutyB != targetDutyB)) {
        int diffR = targetDutyR - _lastHardwareDutyR;
        int diffG = targetDutyG - _lastHardwareDutyG;
        int diffB = targetDutyB - _lastHardwareDutyB;
        
        int steps = max(abs(diffR), max(abs(diffG), abs(diffB)));
        if (steps > 0) {
            int stepDelay = _fadeDuration / steps;
            for (int i = 1; i <= steps; i++) {
                int dR = _lastHardwareDutyR + (diffR * i / steps);
                int dG = _lastHardwareDutyG + (diffG * i / steps);
                int dB = _lastHardwareDutyB + (diffB * i / steps);
                
                #ifdef ESP32
                    ledcWrite(_ledcChannelR, dR);
                    ledcWrite(_ledcChannelG, dG);
                    ledcWrite(_ledcChannelB, dB);
                #else
                    analogWrite(_pinR, dR);
                    analogWrite(_pinG, dG);
                    analogWrite(_pinB, dB);
                #endif
                
                if (stepDelay > 0) delay(stepDelay);
            }
        }
    } else {
        #ifdef ESP32
            ledcWrite(_ledcChannelR, targetDutyR);
            ledcWrite(_ledcChannelG, targetDutyG);
            ledcWrite(_ledcChannelB, targetDutyB);
        #else
            analogWrite(_pinR, targetDutyR);
            analogWrite(_pinG, targetDutyG);
            analogWrite(_pinB, targetDutyB);
        #endif
    }
    
    _lastHardwareDutyR = targetDutyR;
    _lastHardwareDutyG = targetDutyG;
    _lastHardwareDutyB = targetDutyB;
}

void RGBControl::setFrequency(int frequency) {
    _frequency = frequency;
    #ifdef ESP32
        ledcSetup(_ledcChannelR, _frequency, 8);
        ledcSetup(_ledcChannelG, _frequency, 8);
        ledcSetup(_ledcChannelB, _frequency, 8);
    #else
        analogWriteFreq(_frequency);
    #endif
    _updateHardware();
}

void RGBControl::setAutoOffTimer(unsigned long duration) {
    if (_autoOffTimer != duration) {
        _autoOffTimer = duration;
        saveConfig();
    }
}

void RGBControl::setFadeDuration(int duration) {
    if (_fadeDuration != duration) {
        _fadeDuration = duration;
        saveConfig();
    }
}

void RGBControl::update() {
    if (_on && _autoOffTimer > 0 && (millis() - _turnOnTime >= _autoOffTimer)) {
        turnOff();
    }
}

void RGBControl::refreshState() {
    _updateHardware();
}

void RGBControl::processJson(JsonObject& doc) {
    if (doc.containsKey(_name)) {
        if (doc[_name].is<JsonObject>()) {
            JsonObject command = doc[_name].as<JsonObject>();

            if (command.containsKey("setPercentage")) {
                setPercentage(command["setPercentage"].as<int>());
            }

            int r = _targetR;
            int g = _targetG;
            int b = _targetB;
            bool updateColor = false;

            if (command.containsKey("setRGB")) {
                JsonObject rgb = command["setRGB"];
                r = rgb["r"];
                g = rgb["g"];
                b = rgb["b"];
                updateColor = true;
            }
            if (command.containsKey("setR")) { r = command["setR"].as<int>(); updateColor = true; }
            if (command.containsKey("setG")) { g = command["setG"].as<int>(); updateColor = true; }
            if (command.containsKey("setB")) { b = command["setB"].as<int>(); updateColor = true; }

            if (updateColor) {
                setRGB(r, g, b);
            }

            if (command.containsKey("setFrequency")) {
                setFrequency(command["setFrequency"].as<int>());
            }
            if (command.containsKey("setAutoOffTimer")) {
                setAutoOffTimer(command["setAutoOffTimer"].as<unsigned long>());
            }
            if (command.containsKey("setFadeDuration")) {
                setFadeDuration(command["setFadeDuration"].as<int>());
            }

            if (command.containsKey("toggleState") && command["toggleState"].as<bool>()) {
                toggle();
            }
            if (command.containsKey("setState") && command["setState"].is<bool>()) {
                bool state = command["setState"].as<bool>();
                state ? turnOn() : turnOff();
            }
        }
    }
}

void RGBControl::addToJson(JsonObject& doc) {
    JsonObject nested = doc.createNestedObject(_name);
    nested["type"] = "DeviceControl";
    nested["subType"] = "RGBControl";
    nested["isOn"] = isOn();
    nested["percentage"] = _percentage;
    
    nested["r"] = _targetR;
    nested["g"] = _targetG;
    nested["b"] = _targetB;

    nested["frequency"] = _frequency;
    nested["autoOffTimer"] = _autoOffTimer;
    nested["fadeDuration"] = _fadeDuration;

    unsigned long remaining = 0;
    if (_on && _autoOffTimer > 0) {
        unsigned long elapsed = millis() - _turnOnTime;
        if (elapsed < _autoOffTimer) {
            remaining = _autoOffTimer - elapsed;
        }
    }
    nested["autoOffRemaining"] = remaining;
}

const String& RGBControl::getName() {
    return _name;
}
#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include <IClock.h>

class ArduinoClock : public IClock {
public:
    uint32_t millis() override { return ::millis(); }
};
#endif

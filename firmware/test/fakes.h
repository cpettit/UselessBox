#pragma once
#include <stdint.h>
#include <IClock.h>
#include <ISwitchInput.h>
#include <IServoOutput.h>

class FakeClock : public IClock {
public:
    uint32_t now = 0;
    uint32_t millis() override { return now; }
    void advance(uint32_t ms) { now += ms; }
};

class FakeSwitchInput : public ISwitchInput {
public:
    bool states[7] = {false,false,false,false,false,false,false};
    bool isOn(uint8_t ch) override { return states[ch]; }
    void set(uint8_t ch, bool on) { states[ch] = on; }
};

class RecordingServoOutput : public IServoOutput {
public:
    uint8_t last[7] = {255,255,255,255,255,255,255};
    int writes = 0;
    void writeAngle(uint8_t ch, uint8_t deg) override { last[ch] = deg; writes++; }
};

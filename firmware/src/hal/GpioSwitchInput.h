#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include <ISwitchInput.h>

class GpioSwitchInput : public ISwitchInput {
public:
    GpioSwitchInput(const uint8_t* pins, uint8_t count)
        : pins_(pins), count_(count) {}
    void begin() {
        for (uint8_t i = 0; i < count_; i++) pinMode(pins_[i], INPUT_PULLUP);
    }
    bool isOn(uint8_t channel) override {
        if (channel >= count_) return false;
        return digitalRead(pins_[channel]) == LOW; // active-low
    }
private:
    const uint8_t* pins_;
    uint8_t count_;
};
#endif

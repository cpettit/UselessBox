#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>
#include <IServoOutput.h>

class Pca9685ServoOutput : public IServoOutput {
public:
    Pca9685ServoOutput(uint16_t minUs = 500, uint16_t maxUs = 2500)
        : pwm_(), minUs_(minUs), maxUs_(maxUs) {}
    void begin() {
        pwm_.begin();
        pwm_.setOscillatorFrequency(27000000);
        pwm_.setPWMFreq(50); // 50 Hz for analog servos
    }
    void writeAngle(uint8_t channel, uint8_t degrees) override {
        if (channel > 15) return;  // PCA9685 has 16 channels (0..15)
        if (degrees > 180) degrees = 180;
        uint16_t us = minUs_ + (uint32_t)(maxUs_ - minUs_) * degrees / 180;
        pwm_.writeMicroseconds(channel, us);
    }
private:
    Adafruit_PWMServoDriver pwm_;
    uint16_t minUs_;
    uint16_t maxUs_;
};
#endif

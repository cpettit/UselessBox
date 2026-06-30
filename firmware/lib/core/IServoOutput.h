#pragma once
#include <stdint.h>
class IServoOutput {
public:
    virtual ~IServoOutput() {}
    virtual void writeAngle(uint8_t channel, uint8_t degrees) = 0;
};

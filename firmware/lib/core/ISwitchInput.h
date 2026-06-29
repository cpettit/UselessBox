#pragma once
#include <stdint.h>
class ISwitchInput {
public:
    virtual ~ISwitchInput() {}
    virtual bool isOn(uint8_t channel) = 0; // true when toggle is ON
};

#pragma once
#include <stdint.h>
class IClock {
public:
    virtual ~IClock() {}
    virtual uint32_t millis() = 0;
};

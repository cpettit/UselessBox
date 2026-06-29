#pragma once
#include <stdint.h>

class Debouncer {
public:
    explicit Debouncer(uint32_t debounceMs = 0);
    bool update(bool raw, uint32_t nowMs);
    bool stable() const;
private:
    uint32_t debounceMs_;
    bool stable_;
    bool lastRaw_;
    uint32_t lastChangeMs_;
    bool initialized_;
};

#include "Debouncer.h"

Debouncer::Debouncer(uint32_t debounceMs)
    : debounceMs_(debounceMs), stable_(false), lastRaw_(false),
      lastChangeMs_(0), initialized_(false) {}

bool Debouncer::update(bool raw, uint32_t nowMs) {
    if (!initialized_) {
        initialized_ = true;
        stable_ = raw;
        lastRaw_ = raw;
        lastChangeMs_ = nowMs;
        return stable_;
    }
    if (raw != lastRaw_) {
        lastRaw_ = raw;
        lastChangeMs_ = nowMs;
    }
    if (raw != stable_ && (nowMs - lastChangeMs_) >= debounceMs_) {
        stable_ = raw;
    }
    return stable_;
}

bool Debouncer::stable() const { return stable_; }

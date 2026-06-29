#include "ChannelController.h"

ChannelController::ChannelController(ISwitchInput& input, IServoOutput& output,
                                     IClock& clock, Channel* channels,
                                     uint8_t count, uint8_t maxConcurrent,
                                     uint32_t debounceMs)
    : input_(input), output_(output), clock_(clock), channels_(channels),
      count_(count), maxConcurrent_(maxConcurrent) {
    for (uint8_t i = 0; i < count_ && i < kMaxChannels; i++) {
        debouncers_[i] = Debouncer(debounceMs);
    }
}

void ChannelController::begin() {
    for (uint8_t i = 0; i < count_; i++) {
        output_.writeAngle(i, channels_[i].desiredAngle());
    }
}

uint8_t ChannelController::busyCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < count_; i++) {
        if (channels_[i].isBusy()) n++;
    }
    return n;
}

void ChannelController::tick() {
    uint32_t now = clock_.millis();

    // 1. read + debounce switches, advance channel timers
    for (uint8_t i = 0; i < count_; i++) {
        debouncers_[i].update(input_.isOn(i), now);
        channels_[i].update(now);
    }

    // 2. grant reactions to idle channels whose switch is ON, staggered
    uint8_t busy = busyCount();
    for (uint8_t i = 0; i < count_; i++) {
        if (!channels_[i].isBusy() && debouncers_[i].stable()) {
            if (busy < maxConcurrent_) {
                channels_[i].trigger(now);
                busy++;
            }
        }
    }

    // 3. drive servos to desired angles
    for (uint8_t i = 0; i < count_; i++) {
        output_.writeAngle(i, channels_[i].desiredAngle());
    }
}

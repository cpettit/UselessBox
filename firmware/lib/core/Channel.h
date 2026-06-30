#pragma once
#include <stdint.h>

enum class ChannelState : uint8_t { Idle, Reacting, Returning };

struct ChannelConfig {
    uint8_t homeAngle;     // arm retracted (resting)
    uint8_t outAngle;      // arm extended to flip the toggle OFF
    uint32_t reactHoldMs;  // time held at outAngle
    uint32_t returnMs;     // time allotted for the arm to return home
};

class Channel {
public:
    explicit Channel(const ChannelConfig& cfg);
    void trigger(uint32_t nowMs);
    void update(uint32_t nowMs);
    bool isBusy() const;
    uint8_t desiredAngle() const;
    ChannelState state() const;
private:
    ChannelConfig cfg_;
    ChannelState state_;
    uint32_t phaseStartMs_;
};

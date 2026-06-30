#include "Channel.h"

Channel::Channel(const ChannelConfig& cfg)
    : cfg_(cfg), state_(ChannelState::Idle), phaseStartMs_(0) {}

void Channel::trigger(uint32_t nowMs) {
    if (state_ == ChannelState::Idle) {
        state_ = ChannelState::Reacting;
        phaseStartMs_ = nowMs;
    }
}

void Channel::update(uint32_t nowMs) {
    if (state_ == ChannelState::Reacting) {
        if (nowMs - phaseStartMs_ >= cfg_.reactHoldMs) {
            state_ = ChannelState::Returning;
            phaseStartMs_ = nowMs;
        }
    } else if (state_ == ChannelState::Returning) {
        if (nowMs - phaseStartMs_ >= cfg_.returnMs) {
            state_ = ChannelState::Idle;
        }
    }
}

bool Channel::isBusy() const { return state_ != ChannelState::Idle; }

uint8_t Channel::desiredAngle() const {
    return state_ == ChannelState::Reacting ? cfg_.outAngle : cfg_.homeAngle;
}

ChannelState Channel::state() const { return state_; }

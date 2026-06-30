#pragma once
#include <stdint.h>
#include "IClock.h"
#include "ISwitchInput.h"
#include "IServoOutput.h"
#include "Channel.h"
#include "Debouncer.h"

class ChannelController {
public:
    static const uint8_t kMaxChannels = 7;
    ChannelController(ISwitchInput& input, IServoOutput& output, IClock& clock,
                      Channel* channels, uint8_t count,
                      uint8_t maxConcurrent, uint32_t debounceMs);
    void begin();
    void tick();
    uint8_t busyCount() const;
private:
    ISwitchInput& input_;
    IServoOutput& output_;
    IClock& clock_;
    Channel* channels_;
    uint8_t count_;
    uint8_t maxConcurrent_;
    Debouncer debouncers_[kMaxChannels];
};

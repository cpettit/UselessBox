# Phase 1 — Breadboard Prototype + Firmware v0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a module-based breadboard of the Angry Octopus controller and firmware v0 that runs the core "flip the toggle back OFF" loop for 1 channel, then all 5, with current-bounding stagger.

**Architecture:** Firmware is split into a hardware-independent **core** (debounce, per-channel state machine, multi-channel staggering controller) behind abstract interfaces, and a thin **HAL** (PCA9685 servo output, GPIO switch input, Arduino clock) that adapts real hardware to those interfaces. Core logic is unit-tested on the host via PlatformIO's `native` environment + Unity; the HAL is validated manually on the breadboard (Humble Object pattern). Personality (eyes/sound) is explicitly out of scope until Phase 2.

**Tech Stack:** PlatformIO, Arduino-ESP32 framework, ESP32-WROOM-32 DevKit, PCA9685 16-ch PWM breakout (Adafruit PWM Servo Driver library), MG90S servos, SPDT toggle switches, Unity test framework, C++17.

## Global Constraints

- Brain is ESP32-WROOM-32 (DevKit module on the breadboard); servo PWM via PCA9685 over I²C. (verbatim from spec §2)
- Board capacity is **7** switch/servo pairs; this build uses **5**. Code must support up to 7. (spec §2)
- Servos are MG90S on standard 3-pin headers; signal from PCA9685. (spec §2)
- Triggers are SPDT toggle switches read on ESP32 GPIO with pull-ups. (spec §2)
- **Firmware must stagger servo moves** so peak current stays bounded — never actuate more than `maxConcurrent` servos at once. (spec §4)
- Core logic must be **hardware-independent** (no `Arduino.h`) so it compiles and unit-tests in the `native` environment. (spec §1, this plan's architecture)
- Personality (WS2812 eyes, DFPlayer sound, mood engine) is **out of scope for Phase 1** — Phase 2. (spec §8)
- All firmware lives under `firmware/`; wiring docs under `docs/wiring/`.

---

### Task 1: PlatformIO project skeleton + native test harness

**Files:**
- Create: `firmware/platformio.ini`
- Create: `firmware/lib/core/.gitkeep`
- Create: `firmware/src/.gitkeep`
- Test: `firmware/test/test_sanity/test_sanity.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: a working `pio test -e native` toolchain that later tasks add tests to; an `esp32` build env for firmware.

- [ ] **Step 1: Write the failing test**

`firmware/test/test_sanity/test_sanity.cpp`:
```cpp
#include <unity.h>

void test_toolchain_runs(void) {
    TEST_ASSERT_EQUAL_INT(4, 2 + 2);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_toolchain_runs);
    return UNITY_END();
}
```

- [ ] **Step 2: Create the PlatformIO config**

`firmware/platformio.ini`:
```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    adafruit/Adafruit PWM Servo Driver Library@^3.0.0
monitor_speed = 115200

[env:native]
platform = native
test_framework = unity
build_flags = -std=c++17
```

Create empty `firmware/src/.gitkeep` and `firmware/lib/core/.gitkeep` so the dirs exist.

- [ ] **Step 3: Run the test to verify it passes**

Run: `cd firmware && pio test -e native -f test_sanity`
Expected: PASS — `test_toolchain_runs` passes, `1 Tests 0 Failures`.

(If `pio` is missing: install with `pip install platformio` or via the PlatformIO IDE extension. This is the one environment prerequisite for the whole plan.)

- [ ] **Step 4: Commit**

```bash
git add firmware/platformio.ini firmware/src/.gitkeep firmware/lib/core/.gitkeep firmware/test/test_sanity/test_sanity.cpp
git commit -m "chore: scaffold PlatformIO project with native test harness"
```

---

### Task 2: Debouncer (core)

**Files:**
- Create: `firmware/lib/core/Debouncer.h`
- Create: `firmware/lib/core/Debouncer.cpp`
- Test: `firmware/test/test_debouncer/test_debouncer.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `class Debouncer` with `explicit Debouncer(uint32_t debounceMs = 0)`, `bool update(bool raw, uint32_t nowMs)`, `bool stable() const`. First `update` call initializes the stable state to `raw` immediately. A change in `raw` only flips the stable state after it has been held for `>= debounceMs`. Copy-assignable (used in an array by Task 4).

- [ ] **Step 1: Write the failing test**

`firmware/test/test_debouncer/test_debouncer.cpp`:
```cpp
#include <unity.h>
#include <Debouncer.h>

void test_initializes_to_first_reading(void) {
    Debouncer d(50);
    TEST_ASSERT_TRUE(d.update(true, 0));
    TEST_ASSERT_TRUE(d.stable());
}

void test_transient_bounce_is_ignored(void) {
    Debouncer d(50);
    d.update(false, 0);            // stable = false
    d.update(true, 10);            // raw goes high...
    bool s = d.update(false, 30);  // ...but drops before 50ms window
    TEST_ASSERT_FALSE(s);          // never became stable-high
}

void test_sustained_change_flips_after_window(void) {
    Debouncer d(50);
    d.update(false, 0);
    d.update(true, 10);            // raw high at t=10
    TEST_ASSERT_FALSE(d.update(true, 59));  // 49ms held: not yet
    TEST_ASSERT_TRUE(d.update(true, 60));   // 50ms held: flips
}

void test_boundary_is_inclusive(void) {
    Debouncer d(50);
    d.update(false, 0);
    d.update(true, 100);
    TEST_ASSERT_TRUE(d.update(true, 150)); // exactly 50ms -> flips
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_initializes_to_first_reading);
    RUN_TEST(test_transient_bounce_is_ignored);
    RUN_TEST(test_sustained_change_flips_after_window);
    RUN_TEST(test_boundary_is_inclusive);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd firmware && pio test -e native -f test_debouncer`
Expected: FAIL — `Debouncer.h: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`firmware/lib/core/Debouncer.h`:
```cpp
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
```

`firmware/lib/core/Debouncer.cpp`:
```cpp
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
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd firmware && pio test -e native -f test_debouncer`
Expected: PASS — 4 tests, 0 failures.

- [ ] **Step 5: Commit**

```bash
git add firmware/lib/core/Debouncer.h firmware/lib/core/Debouncer.cpp firmware/test/test_debouncer/test_debouncer.cpp
git commit -m "feat: add debouncer for toggle switch inputs"
```

---

### Task 3: Channel state machine (core)

**Files:**
- Create: `firmware/lib/core/Channel.h`
- Create: `firmware/lib/core/Channel.cpp`
- Test: `firmware/test/test_channel/test_channel.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `enum class ChannelState : uint8_t { Idle, Reacting, Returning }`
  - `struct ChannelConfig { uint8_t homeAngle; uint8_t outAngle; uint32_t reactHoldMs; uint32_t returnMs; }`
  - `class Channel` with `explicit Channel(const ChannelConfig&)`, `void trigger(uint32_t nowMs)` (starts a reaction only when `Idle`), `void update(uint32_t nowMs)` (advances timing), `bool isBusy() const` (true while `Reacting` or `Returning`), `uint8_t desiredAngle() const` (`outAngle` while `Reacting`, else `homeAngle`), `ChannelState state() const`.
  - Lifecycle: `trigger` → `Reacting` for `reactHoldMs` → `Returning` for `returnMs` → `Idle`.

- [ ] **Step 1: Write the failing test**

`firmware/test/test_channel/test_channel.cpp`:
```cpp
#include <unity.h>
#include <Channel.h>

static ChannelConfig cfg() {
    ChannelConfig c;
    c.homeAngle = 10; c.outAngle = 100;
    c.reactHoldMs = 300; c.returnMs = 200;
    return c;
}

void test_starts_idle_at_home(void) {
    Channel ch(cfg());
    TEST_ASSERT_EQUAL(ChannelState::Idle, ch.state());
    TEST_ASSERT_EQUAL_UINT8(10, ch.desiredAngle());
    TEST_ASSERT_FALSE(ch.isBusy());
}

void test_trigger_starts_reaction_out(void) {
    Channel ch(cfg());
    ch.trigger(1000);
    TEST_ASSERT_EQUAL(ChannelState::Reacting, ch.state());
    TEST_ASSERT_EQUAL_UINT8(100, ch.desiredAngle());
    TEST_ASSERT_TRUE(ch.isBusy());
}

void test_holds_then_returns_then_idles(void) {
    Channel ch(cfg());
    ch.trigger(1000);
    ch.update(1299);                                   // 299ms: still reacting
    TEST_ASSERT_EQUAL(ChannelState::Reacting, ch.state());
    ch.update(1300);                                   // 300ms: -> returning
    TEST_ASSERT_EQUAL(ChannelState::Returning, ch.state());
    TEST_ASSERT_EQUAL_UINT8(10, ch.desiredAngle());
    ch.update(1499);                                   // 199ms returning
    TEST_ASSERT_EQUAL(ChannelState::Returning, ch.state());
    ch.update(1500);                                   // 200ms: -> idle
    TEST_ASSERT_EQUAL(ChannelState::Idle, ch.state());
    TEST_ASSERT_FALSE(ch.isBusy());
}

void test_trigger_ignored_while_busy(void) {
    Channel ch(cfg());
    ch.trigger(1000);
    ch.trigger(1100);                                  // ignored
    ch.update(1300);
    TEST_ASSERT_EQUAL(ChannelState::Returning, ch.state()); // timed off first trigger
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_starts_idle_at_home);
    RUN_TEST(test_trigger_starts_reaction_out);
    RUN_TEST(test_holds_then_returns_then_idles);
    RUN_TEST(test_trigger_ignored_while_busy);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd firmware && pio test -e native -f test_channel`
Expected: FAIL — `Channel.h: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`firmware/lib/core/Channel.h`:
```cpp
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
```

`firmware/lib/core/Channel.cpp`:
```cpp
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
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd firmware && pio test -e native -f test_channel`
Expected: PASS — 4 tests, 0 failures.

- [ ] **Step 5: Commit**

```bash
git add firmware/lib/core/Channel.h firmware/lib/core/Channel.cpp firmware/test/test_channel/test_channel.cpp
git commit -m "feat: add per-channel reaction state machine"
```

---

### Task 4: ChannelController with stagger (core) + test fakes

**Files:**
- Create: `firmware/lib/core/IClock.h`
- Create: `firmware/lib/core/ISwitchInput.h`
- Create: `firmware/lib/core/IServoOutput.h`
- Create: `firmware/lib/core/ChannelController.h`
- Create: `firmware/lib/core/ChannelController.cpp`
- Create: `firmware/test/fakes.h`
- Test: `firmware/test/test_controller/test_controller.cpp`

**Interfaces:**
- Consumes: `Channel`, `ChannelConfig`, `Debouncer` (Tasks 2–3).
- Produces:
  - `class IClock { virtual uint32_t millis() = 0; }`
  - `class ISwitchInput { virtual bool isOn(uint8_t channel) = 0; }` — true when that channel's toggle is in the ON position.
  - `class IServoOutput { virtual void writeAngle(uint8_t channel, uint8_t degrees) = 0; }`
  - `class ChannelController` with `static const uint8_t kMaxChannels = 7;`, ctor `ChannelController(ISwitchInput&, IServoOutput&, IClock&, Channel* channels, uint8_t count, uint8_t maxConcurrent, uint32_t debounceMs)`, `void begin()` (writes every channel's home angle), `void tick()` (one control cycle), `uint8_t busyCount() const`.
  - tick() contract each cycle: read+debounce all switches, advance all channels, then grant `trigger()` to idle channels whose debounced switch is ON up to `maxConcurrent` busy total, then write every channel's `desiredAngle()` to the servo output.

- [ ] **Step 1: Write the failing test**

`firmware/test/fakes.h`:
```cpp
#pragma once
#include <stdint.h>
#include <IClock.h>
#include <ISwitchInput.h>
#include <IServoOutput.h>

class FakeClock : public IClock {
public:
    uint32_t now = 0;
    uint32_t millis() override { return now; }
    void advance(uint32_t ms) { now += ms; }
};

class FakeSwitchInput : public ISwitchInput {
public:
    bool states[7] = {false,false,false,false,false,false,false};
    bool isOn(uint8_t ch) override { return states[ch]; }
    void set(uint8_t ch, bool on) { states[ch] = on; }
};

class RecordingServoOutput : public IServoOutput {
public:
    uint8_t last[7] = {255,255,255,255,255,255,255};
    int writes = 0;
    void writeAngle(uint8_t ch, uint8_t deg) override { last[ch] = deg; writes++; }
};
```

`firmware/test/test_controller/test_controller.cpp`:
```cpp
#include <unity.h>
#include <ChannelController.h>
#include <Channel.h>
#include "../fakes.h"

static ChannelConfig cfg() {
    ChannelConfig c;
    c.homeAngle = 10; c.outAngle = 100;
    c.reactHoldMs = 300; c.returnMs = 200;
    return c;
}

// helpers shared across tests
static FakeClock clk;
static FakeSwitchInput sw;
static RecordingServoOutput servo;

void setUp(void) {
    clk.now = 0;
    for (int i = 0; i < 7; i++) { sw.states[i] = false; servo.last[i] = 255; }
    servo.writes = 0;
}

void test_begin_writes_home_to_all(void) {
    Channel chs[2] = { Channel(cfg()), Channel(cfg()) };
    ChannelController c(sw, servo, clk, chs, 2, 1, 50);
    c.begin();
    TEST_ASSERT_EQUAL_UINT8(10, servo.last[0]);
    TEST_ASSERT_EQUAL_UINT8(10, servo.last[1]);
}

void test_switch_on_drives_servo_out(void) {
    Channel chs[1] = { Channel(cfg()) };
    ChannelController c(sw, servo, clk, chs, 1, 1, 50);
    c.begin();
    c.tick();                 // t=0: debouncer initializes OFF, no reaction
    sw.set(0, true);
    clk.advance(60);          // hold ON past 50ms debounce
    c.tick();                 // debounced ON -> trigger
    TEST_ASSERT_EQUAL_UINT8(100, servo.last[0]); // arm out
}

void test_stagger_limits_concurrency(void) {
    Channel chs[2] = { Channel(cfg()), Channel(cfg()) };
    ChannelController c(sw, servo, clk, chs, 2, 1, 50);  // maxConcurrent = 1
    c.begin();
    c.tick();
    sw.set(0, true); sw.set(1, true);
    clk.advance(60);
    c.tick();                 // only ONE channel may actuate
    TEST_ASSERT_EQUAL_UINT8(1, c.busyCount());
    bool oneOut = (servo.last[0] == 100) != (servo.last[1] == 100);
    TEST_ASSERT_TRUE(oneOut); // exactly one arm out
}

void test_second_channel_runs_after_first_returns(void) {
    Channel chs[2] = { Channel(cfg()), Channel(cfg()) };
    ChannelController c(sw, servo, clk, chs, 2, 1, 50);
    c.begin();
    c.tick();
    sw.set(0, true); sw.set(1, true);
    clk.advance(60);
    c.tick();                 // ch A actuates
    clk.advance(600);         // let A fully finish (300 hold + 200 return + margin)
    c.tick();                 // A returns to idle; B may now actuate
    c.tick();
    TEST_ASSERT_EQUAL_UINT8(1, c.busyCount()); // now B is the busy one
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_writes_home_to_all);
    RUN_TEST(test_switch_on_drives_servo_out);
    RUN_TEST(test_stagger_limits_concurrency);
    RUN_TEST(test_second_channel_runs_after_first_returns);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd firmware && pio test -e native -f test_controller`
Expected: FAIL — `ChannelController.h: No such file or directory`.

- [ ] **Step 3: Write the interfaces**

`firmware/lib/core/IClock.h`:
```cpp
#pragma once
#include <stdint.h>
class IClock {
public:
    virtual ~IClock() {}
    virtual uint32_t millis() = 0;
};
```

`firmware/lib/core/ISwitchInput.h`:
```cpp
#pragma once
#include <stdint.h>
class ISwitchInput {
public:
    virtual ~ISwitchInput() {}
    virtual bool isOn(uint8_t channel) = 0; // true when toggle is ON
};
```

`firmware/lib/core/IServoOutput.h`:
```cpp
#pragma once
#include <stdint.h>
class IServoOutput {
public:
    virtual ~IServoOutput() {}
    virtual void writeAngle(uint8_t channel, uint8_t degrees) = 0;
};
```

- [ ] **Step 4: Write the controller**

`firmware/lib/core/ChannelController.h`:
```cpp
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
```

`firmware/lib/core/ChannelController.cpp`:
```cpp
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
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd firmware && pio test -e native -f test_controller`
Expected: PASS — 4 tests, 0 failures.

- [ ] **Step 6: Run the whole native suite**

Run: `cd firmware && pio test -e native`
Expected: PASS — all tests across `test_sanity`, `test_debouncer`, `test_channel`, `test_controller`.

- [ ] **Step 7: Commit**

```bash
git add firmware/lib/core/IClock.h firmware/lib/core/ISwitchInput.h firmware/lib/core/IServoOutput.h firmware/lib/core/ChannelController.h firmware/lib/core/ChannelController.cpp firmware/test/fakes.h firmware/test/test_controller/test_controller.cpp
git commit -m "feat: add staggered multi-channel controller"
```

---

### Task 5: Hardware layer (HAL) + firmware entry point

**Files:**
- Create: `firmware/src/hal/ArduinoClock.h`
- Create: `firmware/src/hal/GpioSwitchInput.h`
- Create: `firmware/src/hal/Pca9685ServoOutput.h`
- Create: `firmware/src/main.cpp`

**Interfaces:**
- Consumes: `IClock`, `ISwitchInput`, `IServoOutput`, `Channel`, `ChannelConfig`, `ChannelController` (Tasks 3–4).
- Produces: the `esp32` firmware binary. HAL classes are header-only and guarded by `#ifdef ARDUINO` so they never compile in the `native` test build. (PlatformIO does not build `src/` during `pio test` by default, so `main.cpp` is also excluded from native automatically.)

**Note on testing:** the HAL is the Humble Object — thin pass-through to vendor APIs, validated on real hardware in Step 5, not unit-tested.

- [ ] **Step 1: Write the Arduino clock adapter**

`firmware/src/hal/ArduinoClock.h`:
```cpp
#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include <IClock.h>

class ArduinoClock : public IClock {
public:
    uint32_t millis() override { return ::millis(); }
};
#endif
```

- [ ] **Step 2: Write the GPIO switch input adapter**

Toggles wire pin→GND when ON, using the ESP32 internal pull-up, so ON reads LOW (active-low).

`firmware/src/hal/GpioSwitchInput.h`:
```cpp
#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include <ISwitchInput.h>

class GpioSwitchInput : public ISwitchInput {
public:
    GpioSwitchInput(const uint8_t* pins, uint8_t count)
        : pins_(pins), count_(count) {}
    void begin() {
        for (uint8_t i = 0; i < count_; i++) pinMode(pins_[i], INPUT_PULLUP);
    }
    bool isOn(uint8_t channel) override {
        if (channel >= count_) return false;
        return digitalRead(pins_[channel]) == LOW; // active-low
    }
private:
    const uint8_t* pins_;
    uint8_t count_;
};
#endif
```

- [ ] **Step 3: Write the PCA9685 servo output adapter**

`firmware/src/hal/Pca9685ServoOutput.h`:
```cpp
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
```

- [ ] **Step 4: Write the firmware entry point**

`firmware/src/main.cpp`:
```cpp
#include <Arduino.h>
#include <Wire.h>
#include "hal/ArduinoClock.h"
#include "hal/GpioSwitchInput.h"
#include "hal/Pca9685ServoOutput.h"
#include <Channel.h>
#include <ChannelController.h>

// 5 active channels for the octopus build (board supports up to 7).
// Switch GPIOs: all support internal pull-ups; avoid input-only/strapping pins.
static const uint8_t kCount = 5;
static const uint8_t SWITCH_PINS[kCount] = {32, 33, 25, 26, 27};

// Per-channel motion. Tune homeAngle/outAngle on the bench so the arm flips
// the toggle OFF at outAngle and rests clear at homeAngle.
static ChannelConfig makeCfg() {
    ChannelConfig c;
    c.homeAngle = 20;
    c.outAngle = 110;
    c.reactHoldMs = 350;
    c.returnMs = 300;
    return c;
}

static Channel channels[kCount] = {
    Channel(makeCfg()), Channel(makeCfg()), Channel(makeCfg()),
    Channel(makeCfg()), Channel(makeCfg())
};

static ArduinoClock clock_;
static GpioSwitchInput switches(SWITCH_PINS, kCount);
static Pca9685ServoOutput servos;

// maxConcurrent = 1 keeps peak servo current bounded (spec §4).
static ChannelController controller(switches, servos, clock_, channels, kCount,
                                    /*maxConcurrent=*/1, /*debounceMs=*/30);

void setup() {
    Serial.begin(115200);
    Wire.begin();          // ESP32 default SDA=21, SCL=22
    servos.begin();
    switches.begin();
    controller.begin();
}

void loop() {
    controller.tick();
    delay(5);
}
```

- [ ] **Step 5: Build, flash, and validate on the breadboard**

Build: `cd firmware && pio run -e esp32`
Expected: compiles cleanly.

Flash (board connected): `cd firmware && pio run -e esp32 -t upload`

Bench validation (wire per the Task 6 diagram; bench supply on the 5 V servo rail):
1. Power up: all 5 servo arms move to ~home angle and hold steady.
2. Flip toggle on channel 0 ON → arm 0 sweeps to `outAngle`, holds, returns home; the sweep physically knocks the toggle back OFF.
3. Repeat for channels 1–4.
4. Flip two toggles ON at once → only one arm moves at a time (stagger); the second moves after the first finishes.
5. Hold a toggle ON → the arm keeps fighting it back (re-triggers after each return).
Expected: all five behaviors observed. Adjust `homeAngle`/`outAngle` per channel if an arm overshoots or fails to flip the toggle.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/hal/ArduinoClock.h firmware/src/hal/GpioSwitchInput.h firmware/src/hal/Pca9685ServoOutput.h firmware/src/main.cpp
git commit -m "feat: add ESP32 HAL and firmware v0 entry point"
```

---

### Task 6: Breadboard wiring document

**Files:**
- Create: `docs/wiring/breadboard-phase1.md`

**Interfaces:**
- Consumes: pin assignments from `firmware/src/main.cpp` (Task 5) — switch GPIOs `{32,33,25,26,27}`, I²C `SDA=21/SCL=22`, PCA9685 servo channels `0–4`.
- Produces: the canonical Phase 1 wiring reference (pin table + connection list + ASCII layout). The formal KiCad schematic is deferred to Phase 3.

- [ ] **Step 1: Write the wiring document**

`docs/wiring/breadboard-phase1.md`:
````markdown
# Phase 1 Breadboard Wiring — Angry Octopus Useless Box

Module-based prototype. Mirrors `firmware/src/main.cpp` exactly.

## Modules
- ESP32-WROOM-32 DevKit
- PCA9685 16-channel PWM servo driver breakout
- 5 V buck converter module (≥5 A), fed from a 6–12 V adapter or battery pack
- 5× MG90S servos
- 5× SPDT toggle switches

## Power
- Source (6–12 V) → buck module IN+ / IN−.
- Buck OUT (5 V) → **servo rail**: PCA9685 `V+` terminal **and** ESP32 `5V`/`VIN` pin.
- **Common ground:** buck OUT−, ESP32 `GND`, PCA9685 `GND` all tied together.
- PCA9685 `VCC` (logic) → ESP32 `3V3`.
- Add bulk capacitance (≥470 µF) across the 5 V servo rail near the PCA9685.

> ⚠️ Do not power the servos from the ESP32's onboard regulator. Servos pull
> amps; they must run off the buck's 5 V rail. `maxConcurrent=1` in firmware
> bounds peak current to a single servo at a time.

## I²C (ESP32 ↔ PCA9685)
| ESP32 | PCA9685 |
|-------|---------|
| GPIO21 (SDA) | SDA |
| GPIO22 (SCL) | SCL |

## Switch inputs (active-low, internal pull-up)
Each toggle: one leg → ESP32 GPIO, other leg → GND. ON = closed = reads LOW.

| Channel | ESP32 GPIO |
|---------|-----------|
| 0 | GPIO32 |
| 1 | GPIO33 |
| 2 | GPIO25 |
| 3 | GPIO26 |
| 4 | GPIO27 |

## Servo outputs
MG90S signal → PCA9685 channel header (S pin); servo V+ and GND → PCA9685
channel header V+/GND (fed by the 5 V rail).

| Channel | PCA9685 ch |
|---------|-----------|
| 0 | 0 |
| 1 | 1 |
| 2 | 2 |
| 3 | 3 |
| 4 | 4 |

## ASCII layout
```
 [6-12V] --> [BUCK 5V] --+--> ESP32 5V
                         |
                         +--> PCA9685 V+ --(per-ch)--> MG90S power
   GND common: BUCK- = ESP32 GND = PCA9685 GND = switch commons

   ESP32 3V3 --> PCA9685 VCC
   ESP32 SDA(21)/SCL(22) --> PCA9685 SDA/SCL
   ESP32 GPIO 32,33,25,26,27 --> toggle -> GND   (x5)
   PCA9685 ch0..4 signal --> MG90S signal        (x5)
```
````

- [ ] **Step 2: Commit**

```bash
git add docs/wiring/breadboard-phase1.md
git commit -m "docs: add Phase 1 breadboard wiring diagram"
```

---

## Self-Review

**Spec coverage (Phase 1 scope):**
- Switch→servo flip loop, 1 then 5 channels → Tasks 3–5. ✓
- ESP32 + PCA9685 over I²C → Tasks 4–6. ✓
- Up to 7 channels supported → `kMaxChannels = 7`, count-parameterized. ✓
- SPDT toggles with pull-ups → `GpioSwitchInput` + wiring doc. ✓
- Servo-move staggering to bound current → `maxConcurrent`, Task 4 tests + Task 5. ✓
- Hardware-independent, host-tested core → Tasks 2–4 native tests. ✓
- Breadboard wiring diagram (pin table) → Task 6. ✓ (formal KiCad schematic intentionally deferred to Phase 3.)
- Personality (eyes/sound/mood) correctly **excluded** (Phase 2). ✓

**Placeholder scan:** No TBD/TODO; every code/test step contains complete content.

**Type consistency:** `ChannelConfig` fields (`homeAngle/outAngle/reactHoldMs/returnMs`), `ChannelState` enum, `isOn`/`writeAngle`/`millis` signatures, and `ChannelController` ctor signature are identical across Tasks 3–6 and the fakes. ✓

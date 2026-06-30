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

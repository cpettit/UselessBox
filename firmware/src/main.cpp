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

// Per-arm tunables: adjust homeAngle/outAngle on the bench so each arm flips
// its toggle OFF at outAngle and rests clear at homeAngle.
static ChannelConfig channelCfgs[kCount] = {
    {20, 110, 350, 300},  // arm 0
    {20, 110, 350, 300},  // arm 1
    {20, 110, 350, 300},  // arm 2
    {20, 110, 350, 300},  // arm 3
    {20, 110, 350, 300},  // arm 4
};

static Channel channels[kCount] = {
    Channel(channelCfgs[0]), Channel(channelCfgs[1]), Channel(channelCfgs[2]),
    Channel(channelCfgs[3]), Channel(channelCfgs[4])
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
    // A toggle left ON at power-up is treated as immediately stable (Debouncer
    // initialises its stable state to the first reading), so the octopus will
    // flip off any switch that is already ON at boot — this is intended behaviour.
    controller.begin();
}

void loop() {
    controller.tick();
    delay(5);
}

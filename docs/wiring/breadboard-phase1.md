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

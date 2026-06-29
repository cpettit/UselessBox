# Angry Octopus Useless Box — Design Spec

**Date:** 2026-06-29
**Status:** Approved (design); pending implementation plan
**Reference product:** ROBOTSZU "Don't Touch — Octopus Treasure Guardian" useless box

## 1. Summary

A personality-driven "useless box": the user flips a toggle switch ON, and an
angry octopus retaliates — a tentacle-arm whips out to flip the switch back OFF,
the eyes glow red, and it growls. The first physical build is a **5-arm octopus**
(5 toggles, 5 servo-driven tentacles). The electronics are a **generalized
control board** that supports **up to 7 switch/servo pairs**, so the same board
can drive other useless-box builds.

Two distinct deliverables:

1. **Generalized control board** — reusable PCB + firmware for up to 7
   switch/servo pairs, with sound and addressable-LED ("angry eyes") support.
2. **The octopus build** — the specific 5-channel mechanical build (3D-printed
   mechanism + plush shell) that uses the board.

## 2. Key decisions (locked)

| Area | Decision |
|---|---|
| Control brain | ESP32-WROOM-32E |
| Servo PWM | PCA9685 16-channel I²C PWM driver |
| Channel capacity | 7 switch/servo pairs (octopus uses 5; 2 spare) |
| Power input | Wide input ≈6–12 V; barrel jack **and** screw terminal/JST; accepts wall adapter or battery pack. Reverse-polarity protection + input fuse. Onboard charging deferred to v2. |
| 5 V rail | Synchronous buck, rated ≥5 A; firmware staggers servo moves to bound peak current. |
| Servos | MG90S (9 g metal gear) baseline; channels are standard 3-pin headers so SG90 / MG996R can be substituted. |
| Triggers | SPDT toggle switches (classic "flip it back off"); inputs are pull-up headers that can also accept buttons/sensors later. |
| Personality | Sound (DFPlayer Mini + microSD + speaker), angry eyes (WS2812 addressable RGB), randomized/escalating mood engine in firmware. |
| Octopus channels | 5 toggles + 5 MG90S tentacle-arms. |
| Construction | 3D-printed mechanism + enclosure; plush octopus shell sourced/sewn separately. |
| PCB tooling | KiCad (schematic + layout as version-controlled files) → Gerbers → JLCPCB PCBA. All parts LCSC-stocked. (No EasyEDA / no MCP dependency.) |
| PCB integration | Fully integrated SMD production board. Breadboard prototype uses equivalent breakout **modules**; final board integrates the SMD parts. Electrically identical. |
| 3D CAD | OpenSCAD → STL, fully parametric. |
| Firmware | PlatformIO + Arduino-ESP32. |

## 3. System architecture

Per-channel data flow:

```
SPDT toggle → ESP32 GPIO (debounced) → mood engine selects reaction
  → ESP32 commands PCA9685 over I²C → servo arm sweeps out, flips toggle OFF
  → in parallel: WS2812 eyes go red, DFPlayer plays an angry SFX
  → arm returns → channel idle
```

Block diagram:

```
[Power in: barrel jack + screw term/JST]
        │
        ▼
[Reverse-polarity P-FET + fuse]
        │
        ▼
[5 V synchronous buck ≥5 A] ─┬─ 5 V ─→ servo rail (7×3-pin headers)
                             │         WS2812 eyes, speaker amp
                             └─ [3.3 V LDO] ─→ ESP32-WROOM-32E

ESP32 ─ I²C ───────→ PCA9685 ─ PWM ×7 ─→ servo signal pins
ESP32 ─ GPIO ×7 (pull-ups) ←──────────── toggle switch headers
ESP32 ─ UART ─────→ DFPlayer Mini ─────→ speaker
ESP32 ─ GPIO ─→ level shifter (3.3→5 V) ─→ WS2812 "eyes" data
```

### Pin budget (ESP32-WROOM, ~25 usable GPIO)
- I²C to PCA9685: 2 (SDA/SCL)
- 7× switch inputs: 7 (use input-only pins 34–39 where possible)
- DFPlayer UART: 2 (TX/RX)
- WS2812 data: 1
- Headroom: spare GPIO for status LED, future expansion

## 4. Power detail

- Input range ≈6–12 V so a 9–12 V adapter or a 2S LiPo / multi-cell pack works.
- Reverse-polarity P-channel MOSFET on input; input fuse (or polyfuse).
- Bulk capacitance on the 5 V servo rail to absorb servo inrush.
- Synchronous buck to 5 V, **≥5 A** continuous target. Worst-case all-servo
  stall exceeds this, so firmware **staggers** servo actuation (no simultaneous
  multi-stall) to keep peak current within budget. This constraint is documented
  for the firmware mood engine.
- Separate 3.3 V LDO for the ESP32 (clean rail, isolated from servo noise).

## 5. The octopus build (5 channels)

3D-printed, parametric (OpenSCAD). Parts:

- **Enclosure base** — PCB standoffs, battery bay, ventilation.
- **Top plate** — 5 toggle-switch mounts + 5 tentacle exit slots.
- **Servo brackets** ×5 — MG90S mounts positioned so the horn arc flips the
  adjacent toggle OFF.
- **Tentacle "bone" arms** ×5 — mount to servo horn; curved spine that (a) flips
  the toggle and (b) is the armature the plush tentacle sleeve slides over.
- **Eye dome + diffuser** — houses WS2812 eyes.
- **Speaker grille** + **cable/electronics tray**.

Plush octopus body and tentacle sleeves are **sourced or sewn separately** — not
printed.

Parametric variables: box dimensions, servo spacing, switch spacing, arm length
and throw angle.

## 6. Personality / firmware

- Non-blocking per-channel state machine:
  `IDLE → TRIGGERED → REACT (sweep out, flip off) → RETURN → IDLE`.
- **Mood engine:** randomized reaction patterns, fake-outs, escalating anger with
  repeated pokes, occasional multi-tentacle flurries (subject to current
  staggering).
- **Eyes:** WS2812 calm idle color → red flashing when angry; intensity scales
  with mood.
- **Sound:** DFPlayer Mini plays growls/SFX from microSD; sounds easily swapped
  on the card.
- Per-channel configurable servo endpoints (off-angle / out-angle), debounce
  timing, and reaction library.

## 7. Toolchain & deliverables

- **Firmware:** PlatformIO + Arduino-ESP32.
- **Schematic / PCB:** KiCad → Gerbers + JLCPCB PCBA files (CPL + BOM); LCSC parts.
- **3D parts:** OpenSCAD → STL, fully parametric.
- **Breadboard wiring diagram:** KiCad schematic (canonical) **plus** a
  plain-text pin-mapping table for the module-based breadboard. (Optional
  Fritzing-style SVG picture on request.)
- **BOM with cost estimates:** electronics + PCB fab/assembly + filament + plush +
  misc.

## 8. Build phases (each becomes its own implementation plan)

1. **Breadboard prototype + firmware v0** — module-based breadboard; switch→servo
   flip loop working for 1 channel, then all 5. Deliver wiring diagram + pin table.
2. **Personality on breadboard** — add WS2812 eyes, DFPlayer sound, mood engine.
3. **KiCad schematic** — integrated SMD design of the 7-channel board.
4. **KiCad PCB layout** — route, generate Gerbers + JLCPCB PCBA (CPL/BOM).
5. **OpenSCAD 3D parts** — enclosure, mounts, tentacle arms, eye dome → STLs.
6. **Full costed BOM** — finalized once parts are pinned.
7. **Firmware v1 polish** — tune reactions, sound mapping, current staggering.

## 9. Out of scope (v1)

- Onboard battery charging circuit (adapter/battery input only; charge externally).
- WiFi/BT app control (ESP32 supports it; not a v1 feature).
- Sewing/CAD of the plush shell (sourced separately).
- Fully hand-solderable board variant (production board is SMD/assembled).

## 10. Open items / defaults to confirm during implementation

- Exact ESP32 module variant footprint and 3.3 V LDO part (PCB phase).
- Specific ≥5 A buck IC (PCB phase; must be LCSC-stocked).
- WS2812 eye count (2 minimum; possibly more for effect).
- Audio path on the SMD board: place a DFPlayer Mini as a module footprint
  (simplest, microSD sound swapping) vs. integrate MAX98357A I²S amp + SD
  (more "integrated", needs audio storage). Breadboard uses DFPlayer module
  either way.
- Whether to add a Fritzing-style breadboard picture in addition to the table.

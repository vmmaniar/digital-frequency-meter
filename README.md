# Digital Frequency Meter (1 Hz – 1 MHz) with Regulated Power Supply

A hybrid analog/digital frequency meter. The original lab build uses dual NE555 timers to generate a 1-second gating window and CD4584 Schmitt triggers for input conditioning; this repository documents that classic design and adds a modern ESP-IDF firmware that uses the ESP32 PCNT (pulse counter) peripheral with a hardware-timer-driven gate.

## Two implementations

| Variant | Timebase             | Counter        | Display     |
|---------|---------------------|----------------|-------------|
| Classic | NE555 monostable    | CD4040 + decoders | 7-segment |
| MCU     | ESP32 gptimer (1 ppm xtal) | ESP32 PCNT | OLED / UART |

Both implement the same measurement principle:

```
input ─► Schmitt (CD4584 / GPIO) ─► counter ─► gate ENABLES counter for 1.000 s ─► latch ─► display
                                              ▲
                                              └── precise 1 Hz timebase
```

## Why both?

* The discrete NE555 design demonstrates analog timing, monostable behavior, and basic combinational logic.
* The ESP32 variant is what you would actually ship — a single SoC handles signal conditioning input, counting, gating, and display, with far tighter timebase accuracy (~10 ppm vs. ~5 % for an NE555).

## Building the firmware

```bash
cd firmware
idf.py set-target esp32
idf.py build flash monitor
```

Connect the signal under test to `GPIO 34` (input-only ADC pin, also usable as a PCNT input). Readings appear on serial at 115200 baud.

## Repository layout

```
firmware/        ESP-IDF project — PCNT + gptimer frequency counter
hardware/        Schematic notes for the NE555/CD4584 classic build
sim/             Python simulation of the NE555 gating timebase
docs/            Theory, accuracy budget, calibration
```

## Specifications (MCU variant)

| Parameter     | Value             |
|---------------|-------------------|
| Range         | 1 Hz – 40 MHz     |
| Resolution    | 1 Hz at 1 s gate  |
| Gate accuracy | ±10 ppm (xtal)    |
| Input        | 0–3.3 V TTL       |

For higher frequencies, gate 1 s; for low frequencies, switch to period-measurement mode (firmware auto-selects below 100 Hz).

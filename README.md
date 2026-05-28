# Digital Frequency Meter (1 Hz – 1 MHz) with Regulated Power Supply

A hybrid analog/digital frequency meter. The original lab build uses dual NE555 timers to generate a 1-second gating window and CD4584 Schmitt triggers for input conditioning; this repository documents that classic design and adds a modern ESP-IDF firmware that uses the ESP32 PCNT (pulse counter) peripheral with a hardware-timer-driven gate.

## Two implementations

| Variant | Timebase             | Counter        | Display     |
|---------|---------------------|----------------|-------------|
| Classic | NE555 monostable    | CD4040 + decoders | 7-segment |
| MCU     | ESP32 gptimer (~10 ppm xtal) | ESP32 PCNT | UART / OLED |

Both implement the same measurement principle:

```
input ─► Schmitt (CD4584 / GPIO clamp) ─► counter ─► gate ENABLES counter for 1.000 s ─► latch ─► display
                                                     ▲
                                                     └── precise 1 Hz timebase
```

## Why both?

* The discrete NE555 design demonstrates analog timing, monostable behavior, and basic combinational logic.
* The ESP32 variant is what you would actually ship — a single SoC handles input conditioning, counting, gating, and display, with far tighter timebase accuracy (~10 ppm vs. ~0.5–1 % for an NE555).

## Quick start (no hardware required)

```bash
# Python simulation + tests
cd sim
pip install -r requirements.txt
pytest -v                                     # 13 tests, all green
python ne555_gate_sim.py --duration 5 --plot  # plots the analog timebase

# Host-side serial display, replayed from a captured log
cd ../host_tools
pip install -r requirements.txt
pytest -v                                     # 6 tests, all green
python serial_display.py --replay sample_data/capture.log
```

## With hardware

### Build the firmware

```bash
cd firmware
idf.py set-target esp32
idf.py build flash monitor
```

Connect the signal under test to **GPIO 4** through a 1 kΩ series resistor and a 1N4148 clamp to 3V3. Readings appear on serial at 115200 baud as `DATA …` lines. Send single keystrokes over the UART to control the device:

| Key | Effect |
|-----|--------|
| `g` | cycle gate window (1 s → 5 s → 10 s → 1 s) |
| `c` | calibrate — treat the current input as exactly 1.000 MHz, save offset |
| `r` | reset calibration offset to 0 |
| `h` | print the help banner |

The calibration offset and the selected gate window survive a reboot (stored in NVS).

### Live display on a laptop

```bash
cd host_tools
pip install -r requirements.txt
python serial_display.py --port COM7         # Windows
python serial_display.py --port /dev/ttyUSB0 # Linux
```

## Repository layout

```
firmware/             ESP-IDF v5.4 project — PCNT + gptimer frequency counter
  main/
    app_main.c        Boot, NVS calibration, gate cycling, UART command UX
    freq_counter.c    PCNT + gptimer driver with overflow accounting
host_tools/           Python serial parser + replay test harness
  serial_display.py   Live display reading "DATA …" lines from the firmware
  test_serial_display.py
  sample_data/        Captured log for replay-mode testing
hardware/             Schematic notes + BOM for the NE555/CD4584 classic build
sim/                  Python simulation + pytest of the NE555 timebase math
  ne555_gate_sim.py   Astable + monostable model with plot output
  test_ne555_math.py  pytest suite locking in the timing equations
docs/                 Theory, accuracy budget, calibration
.github/workflows/    GitHub Actions CI (Python tests + ESP-IDF build)
BUILD_PLAN.md         Full 4-week, two-board build plan with India-sourced BOM
```

## Specifications (MCU variant)

| Parameter     | Value             |
|---------------|-------------------|
| Range         | 1 Hz – ~30 MHz    |
| Resolution    | 1 Hz at 1 s gate  |
| Gate accuracy | ±10 ppm (xtal)    |
| Input         | 0 – 3.3 V TTL (1 kΩ + 1N4148 protection) |
| Modes         | direct count (≥ 100 Hz), period (< 100 Hz, auto-engaged) |
| Gate windows  | 1 s / 5 s / 10 s, cycle with `g` |
| Calibration   | single-key `c`, ppm offset stored in NVS |

## CI

GitHub Actions runs two jobs on every push:

1. **python-tests** — `sim/` + `host_tools/` pytest suites.
2. **esp-idf-build** — compiles `firmware/` inside the official `espressif/idf:v5.4` container and reports `idf.py size`.

## License

MIT — see [LICENSE](LICENSE).

# Accuracy budget

## Classic NE555 build

| Source                       | Drift          |
|------------------------------|----------------|
| NE555 thermal drift          | ~50 ppm/°C     |
| R timing 1 % metal film      | ±100 ppm/°C    |
| C timing C0G/NP0             | ±30 ppm/°C     |
| Supply variation             | ±100 ppm/V     |
| Total (room temp, regulated) | **~0.5 – 1 %** |

This is acceptable for resolving signals to ~10 Hz at 1 kHz, but not for precision lab work.

## ESP32 PCNT + gptimer build

| Source                       | Drift          |
|------------------------------|----------------|
| ESP32 main xtal (40 MHz)     | ±10 ppm        |
| Temperature                  | ±5 ppm/°C      |
| Quantization (1 s gate)      | ±1 count       |
| Total                        | **~10 ppm**    |

At 1 MHz that is ±10 Hz, two orders of magnitude better than the NE555 design.

## Calibration

1. Drive the input with a known reference (GPS-disciplined or rubidium OXCO).
2. Measure offset over a 60-second average.
3. Store correction factor in NVS for the MCU build, or trim the NE555 pot for the classic build.

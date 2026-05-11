# Schematic notes

## Power supply

```
AC 9V  ─► BR1 ─► C_bulk 1000µF ─► LM7809 ─► +9V (analog timebase)
                                  └────► LM7805 ─► +5V (logic)
```

Both rails share a star ground at the bridge rectifier.

## NE555 timebase (1 Hz reference)

Astable configuration with `R_A = R_B = 6.8 kΩ`, `C = 100 µF`:
- `f = 1.44 / ((R_A + 2·R_B) · C) ≈ 1.05 Hz`
- Trim with a 10 kΩ pot in series with R_B to land on 1.000 Hz against a known reference.

## Gating monostable

The second NE555 in monostable mode produces a 1.000 s pulse when triggered by the rising edge of the timebase. That pulse gates the CD4040 chain's enable input.

```
timebase ─► trig ┐
                 ▼
              NE555 monostable
                 │
                 ▼   T = 1.1 · R · C
              gate pulse
```

For `T = 1.000 s`, choose `R = 91 kΩ`, `C = 10 µF` and trim.

## Signal conditioning

```
Vin ─► AC-couple (1 µF) ─► clamp (1N4148 to 0 V / +5 V) ─► CD4584 Schmitt ─► CD4040 clock
```

Hysteresis is approximately 1.5 V at Vdd = 5 V, comfortably above noise on a typical bench signal.

## DFM checklist

* All decoupling caps within 5 mm of IC supply pins.
* No 90° trace corners on the gate signal path.
* Crystal-replaceable timebase footprint for future revisions (optional 32.768 kHz can with /32768 divider).

# Bill of Materials — Classic NE555 / CD4584 build

| Ref       | Part              | Description                                  | Qty |
|-----------|-------------------|----------------------------------------------|-----|
| U1, U2    | NE555             | Timebase oscillator + monostable gate        | 2   |
| U3        | CD4584            | Hex Schmitt-trigger inverter (signal cond.)  | 1   |
| U4–U6     | CD4040            | 12-stage binary ripple counter               | 3   |
| U7–U10    | CD4511            | BCD-to-7-segment latch/decoder/driver        | 4   |
| DISP1–4   | Common-cathode 7-seg | Display digits                            | 4   |
| U11       | LM7809            | +9 V linear regulator                        | 1   |
| U12       | LM7805            | +5 V linear regulator                        | 1   |
| BR1       | DB107             | 1 A bridge rectifier                         | 1   |
| C_bulk    | 1000 µF / 25 V    | Smoothing capacitor                          | 1   |
| C_dec     | 100 nF            | Decoupling on every IC                       | ~12 |
| R_timing  | 1 % metal film    | NE555 timing resistors                       | 4   |
| C_timing  | 10 nF C0G/NP0     | NE555 timing capacitor (low-drift)           | 2   |

## Notes

* Use C0G/NP0 timing capacitors — X7R drifts ~15 %/°C and ruins gate accuracy.
* Bypass each logic IC with 100 nF placed within 5 mm of the supply pin.
* The LM7809 dissipates `(Vin - 9 V) × I_load`; size the heatsink for 200 mA worst case.

# Build Plan — Digital Frequency Meter (1 Hz – 1 MHz) with Regulated PSU

**Author:** Vansh Mehul Maniar (Electronics & Instrumentation, BITS Pilani Goa)
**Location:** Pune / Goa, India
**Window:** ~4 weeks of focused part-time work
**Status:** Scaffold already exists (classic NE555 schematic notes + ESP-IDF PCNT firmware + Python timebase sim); this document is the *how to actually build it* layer.

---

## 1. Executive summary

### Elevator pitch
A two-faced frequency meter that ships the **classic lab build** (dual NE555 timebase, CD4584 Schmitt input, CD4040 ripple counters, CD4511 7-segment decoders, LM7805/7809 linear PSU) *and* a modern **ESP32 PCNT + gptimer** rewrite of the same instrument on a single MCU. The deliverable is two working bench instruments side-by-side, one demonstrating analog timing and combinational logic mastery, the other demonstrating that you can replace 11 ICs with one SoC and gain 50,000× better timebase accuracy.

### Demoable end state
1. The **classic board** counts 1 Hz – 1 MHz on four 7-segment digits, gated by a 1.000 s NE555 monostable. Accuracy 0.5 – 1 % after pot trim.
2. The **MCU board** counts 1 Hz – 40 MHz on a 0.96" OLED + USB CDC console, gated by the ESP32's 40 MHz crystal timebase. Accuracy ~10 ppm.
3. Both boards measure the same signal generator inputs (a function generator on the lab bench) and the readings agree to within the classic board's tolerance.
4. A 30-second side-by-side video at 1 kHz, 100 kHz, and 1 MHz inputs.
5. Calibration log against a GPS-disciplined reference (or against the lab's HP 53131A if available at BITS).

### Success criteria (binary)
- [ ] Classic board reads 1 kHz ± 10 Hz after pot trim
- [ ] MCU board reads 1 MHz ± 10 Hz cold, ± 1 Hz after NVS-stored offset
- [ ] Period-mode auto-engages below 100 Hz and reports ±1 LSB resolution
- [ ] Both boards survive 24 h continuous operation
- [ ] BOM (both boards together) ≤ ₹2,500

### Total budget envelope
**Approximately ₹2,000 – ₹3,000** (≈ USD 24 – 36) including a small contingency, *excluding* an oscilloscope, function generator, and bench PSU. Both boards are eminently breadboardable; PCBs are optional polish. Detailed itemized cost summary is in §9. This is the cheapest project in this portfolio — it pays its way as a teaching exercise on counters, monostables, and Schmitt input conditioning.

### Total time estimate
4 weeks at ~10 hrs/week ≈ 40 hours. About 15 % of that is debugging the analog timebase; the rest is straightforward.

---

## 2. Architecture decisions (with reasoning)

The scaffold commits to **NE555 + CD4584 + CD4040 + CD4511** for the classic build and **ESP32-WROOM-32 + PCNT + gptimer** for the MCU build. Most are good. A few warrant either re-affirmation or a small swap — see §2.5.

### 2.1 Why NE555 for the classic build (and not a 4060 ring oscillator or 32.768 kHz can)

The NE555 is the textbook timing IC and the recruiter-recognizable name. It is also genuinely bad — 50 ppm/°C drift, 100 ppm/V supply sensitivity, 1 % typical part-to-part variation. Picking it for the classic build is a *pedagogy* choice, not a precision choice. The whole point is to confront the limits of the analog timebase and then justify the MCU rewrite.

**Considered alternatives and why they lose for *this* exercise:**
- **CD4060 + 32.768 kHz can:** A 32.768 kHz watch crystal divided by 32,768 in a CD4060 gives a 1 Hz reference with ±10 ppm accuracy. *Much* better than an NE555. **But** the demo loses its punch — the analog → MCU upgrade story flattens because the classic board is already accurate. Skip; offer it as a Phase 7 stretch goal.
- **DS1307 RTC + CD4060:** Same trade-off as above, plus an I2C interface that wants an MCU anyway. Defeats the purpose.
- **2 × 555 with manual pot trim** *(scaffold choice)*: Demonstrates monostable + astable in one circuit, pot trim teaches calibration, parts cost is ₹6 each. Keep.

### 2.2 Why CD4040 (12-stage binary ripple) over CD4017 or 7490

The scaffold picks CD4040 — 12-stage binary ripple counter, ~10 MHz max clock. For 1 MHz input × 1 s gate that's 1,000,000 counts, fits in 20 bits, needs at least two cascaded CD4040s. Good choice.

**Considered alternatives:**
- **CD4017 (decade counter):** Only counts to 10, then resets. Useless for accumulating a full 1 s gate of high-frequency input. Skip.
- **74HC4040:** Higher-speed (~35 MHz) CMOS variant. Worth keeping in mind if you ever push the classic build to 2 MHz, but for 1 MHz spec the original CD4040 is fine and ₹15 cheaper.
- **CD74HC393 (dual 4-bit):** Pin-compatible alternative if Robu has stock issues on CD4040. Same speed envelope.

### 2.3 Why CD4511 + common-cathode 7-seg over a single 4-digit MUX

CD4511 is the trivially-wirable BCD-to-7-segment latch/decoder/driver — one per digit, latch the count on the falling edge of the gate, no MCU code, no software at all. The whole **point** of the classic build is to be a pure-CMOS digital instrument that any electronics student in 1985 could have built from a Mouser catalogue.

**Considered alternative:** TM1637 or MAX7219 multi-digit drivers. Both are nice in the MCU build but defeat the classic-CMOS pedagogy. Skip for classic; revisit in §2.7 for the MCU board.

### 2.4 Why ESP32-WROOM-32 over RP2040 + PIO or STM32F1 + TIM

For the MCU rewrite, the scaffold picks **ESP32**. Strong choice, but worth examining.

| MCU | Counter peripheral | Max input freq | Gate timer | Display? | India price |
|---|---|---|---|---|---|
| **ESP32-WROOM-32** ★ | PCNT (4 units, 16-bit + overflow) | ~40 MHz | gptimer (16 MHz tick) | OLED via I2C | ₹250 dev kit ([ESP32 Robu]) |
| ESP32-S3 | PCNT (8 units) | ~40 MHz | gptimer | Same | ₹400 dev kit |
| RP2040 + PIO | PIO counter (programmable) | ~125 MHz (with care) | PIO timer | OLED | ₹350 (Pico W) |
| STM32F103 Blue Pill | TIM (cascade-able) | ~36 MHz | TIM2 | OLED | ₹250 (clone) |
| STM32G431 Nucleo | TIM (HRTIM optional) | ~70 MHz | TIM | OLED | ₹1,500 |

**Headline choice: keep ESP32-WROOM-32** — cheapest, has Wi-Fi (Phase 5 stretch: web dashboard), and PCNT is the simplest counter API in this list. PIO on RP2040 would be *cooler* (a deep dive into programmable I/O state machines, with potentially higher input frequency), but the ESP32 PCNT is already the better-documented path and lets us stay on a stack the user already knows. RP2040 + PIO becomes a Phase 7 stretch goal.

**India availability:** ESP32-WROOM-32 dev kit ₹250 on Robu.in ([ESP32 Robu]) or ₹220 on QuartzComponents. Many "clone" variants from Indian sellers; pick one with the CP2102 USB-UART for clean Windows driver experience.

### 2.5 Architecture deltas from the scaffold (recommendations)

| # | Scaffold | Recommendation | Why |
|---|---|---|---|
| 1 | Single LM7805 + LM7809 linear PSU | **Keep, but add a 9V wall-wart input and dual-rail jumper for ±12 V op-amp future revisions** | Lets the same PSU board host a Phase 7 op-amp-based 0–10 V input attenuator. Cost is one jumper + one extra LM7912. |
| 2 | NE555 1 Hz astable + NE555 monostable | **Keep — but specify Texas Instruments NA555P** (not the "yellow brand" clones from Robu) | Clone NE555s often drift 5× worse than spec. NA555P from Mouser India ₹12. The pedagogy is undermined if the analog board reads 1037 Hz at 1 kHz because of a fake 555. |
| 3 | CD4040 × 2 cascaded for >65k counts | Keep | Two ICs × ₹15. |
| 4 | CD4511 × 4 for display | Keep | One per digit. |
| 5 | Common-cathode 7-seg | Keep, **specify Kingbright SA52-11SRWA red** (0.52", clearly readable from 2 m) | Cheap unbranded 7-segs from Robu sometimes have the colon segment wired backwards. Buy the labelled Kingbright. |
| 6 | Input GPIO 34 on ESP32 | **Move to GPIO 4** (or any non-strapping pin) — GPIO 34 is input-only *and* not a PCNT-friendly pin in IDF v5 | The IDF v5 PCNT API takes any GPIO; pick one that doesn't conflict with strapping pins at boot. |
| 7 | 1 s gate hard-coded | **Add a 100 ms / 1 s / 10 s gate switch** | A 10 s gate at 1 Hz gives 10× better resolution than a 1 s gate. Trivial to add, big UX win. |

---

## 3. Phase-by-phase plan (weeks 0 – 4)

### Phase 0 — Theory + sim (Week 0, ~6 hrs)

**Goal:** Understand the timing math before touching a breadboard.

**Deliverables:**
- Read [TI NE555 datasheet] cover to cover, especially §7.4 (astable) and §7.3 (monostable). Hand-compute the resistor/capacitor values for a 1 Hz astable and a 1 s monostable. Verify against the scaffold's `sim/ne555_gate_sim.py`.
- Extend `sim/ne555_gate_sim.py` to model NE555 thermal drift: add a `--temp-c` argument that scales the timing by `(1 + 50e-6 × (T - 25))`. Plot the gate-window length over a 5 °C → 45 °C sweep. This becomes a figure in your write-up.
- Read the [ESP32 PCNT API guide] for IDF 5.4. Build the example `pulse_counter_basic` from `esp-idf/examples/peripherals/pcnt/`.
- Read the [Microchip AN655] (frequency measurement methods — direct count vs. period measurement vs. reciprocal counting) and pick which of the three to ship. (Recommendation: direct count above 100 Hz, period below; auto-switch in firmware.)
- Get `sim/test_ne555_math.py` (new file) green: `pytest` asserts `astable_period(6.8e3, 6.8e3, 100e-6) ≈ 1.413 s` and that the trim pot range covers ±10 % of nominal.

**Time:** 6 hrs. **Risk:** Low. **Verification:** Sim plot shows the gate pulse correctly aligned to the timebase rising edge; trim range plot shows ±10 % adjustment around 1.000 s.

### Phase 1 — Classic NE555 board on breadboard (Week 1, ~10 hrs)

**Goal:** Get the classic instrument counting on a breadboard before committing to a PCB.

**Deliverables:**
- Order parts per §4.1 from Robu.in and the local QuartzComponents shop. Total order: ~₹600. Lead time 2 – 5 days within Pune/Goa.
- Bread the PSU: 9V wall-wart → LM7809 → LM7805. Verify both rails with a multimeter; expect 9.05 V ± 0.1 V and 5.02 V ± 0.05 V.
- Bread the timebase NE555 astable. Probe pin 3 with an oscilloscope. Trim the 10 kΩ series pot until the period is 1.000 s ± 5 ms (you can't go better than that without a reference clock — that's a Phase 6 task).
- Bread the monostable NE555. Trigger it from the astable output. Probe pin 3; verify the 1.000 s pulse width.
- Bread the CD4584 input conditioning. Drive its input with a function generator at 1 kHz, 1 V_pp sine. Verify the output is a clean 0 – 5 V square wave on the scope.
- Bread *one* CD4040 + *one* CD4511 + *one* 7-segment display. Feed it the gated input. Verify it counts and the display shows the correct digit.

**Time:** 10 hrs. **Risk:** Low — every IC on this board is more than 40 years old and has thousands of breadboard write-ups online. **Verification:** Scope captures of (a) astable, (b) monostable, (c) Schmitt output, (d) one digit incrementing.

### Phase 2 — Classic board full integration (Week 1 – 2, ~10 hrs)

**Goal:** All 4 digits work, the latch fires on the falling edge of the gate, the display freezes during the count and updates on each gate cycle.

**Deliverables:**
- Wire all 4 CD4040s + 4 CD4511s + 4 7-seg displays. (Two CD4040s cascaded handle the count; two CD4511s read 4 BCD digits.) Verify the latch pin (pin 5 of CD4511) is driven from the falling edge of the gate via a 74HC04 or a single transistor — *not* directly from the gate signal, because the falling edge must clock the latch *after* the count has settled.
- Add a manual `RESET` button that clears all CD4040s via their MR pin (pin 11). Useful for Phase 6 calibration.
- Add a `MODE` switch (DPDT) that selects between 1 s and 10 s gate. The 10 s gate is the second NE555 monostable with a switched timing capacitor (10 µF for 1 s, 100 µF for 10 s).
- Test at 1 Hz, 10 Hz, 100 Hz, 1 kHz, 10 kHz, 100 kHz, 1 MHz inputs. Record the displayed reading at each. The MAX input frequency is limited by the CD4040 — ~10 MHz typical, but you'll see counting errors above ~2 MHz on a CD4040B.

**Time:** 10 hrs. **Risk:** Medium — latch timing is the classic gotcha; if the latch fires before the count settles, you read garbage on the last digit. **Verification:** Reads 1.000 MHz ± 10 kHz at 1 MHz input on a freshly trimmed timebase.

### Phase 3 — ESP32 PCNT firmware (Week 2, ~8 hrs)

**Goal:** The MCU build counts the same range with 10 ppm accuracy.

**Deliverables:**
- Flash the existing `firmware/main/freq_counter.c` to an ESP32 dev kit. Verify it builds clean (`idf.py build`) on IDF v5.4.
- Wire the function generator output through a 1 kΩ series resistor (current limit) and a 1N4148 → 3V3 clamp into GPIO 4 (per the §2.5 GPIO recommendation).
- Verify counting at 1 Hz – 1 MHz (matches classic board). Push the input frequency until counting starts to glitch; expect clean counts to ~30 MHz (PCNT is documented at 40 MHz max with glitch filter disabled).
- Implement the auto period-measurement fallback below 100 Hz. The idea: if `total_count < 100`, measure the *period* of one input edge using `gptimer_get_raw_count()` for two consecutive PCNT overflow callbacks, then return `f = 1 / T`.
- Implement the 100 ms / 1 s / 10 s gate selector via a GPIO button (GPIO 0, the BOOT button on most dev kits).
- Print readings on the UART console at every gate completion.

**Time:** 8 hrs. **Risk:** Medium — PCNT has subtle gotchas around the overflow callback (the watch_point_value comparison in the existing scaffold is buggy; see [ESP-IDF PCNT v5 migration notes]). **Verification:** Reading at 1 MHz is 1,000,000 ± 10 Hz cold; ± 1 Hz after NVS-stored offset.

### Phase 4 — OLED display + UX polish (Week 2 – 3, ~6 hrs)

**Goal:** A bench-usable MCU instrument with no laptop attached.

**Deliverables:**
- Wire an SSD1306 0.96" OLED on the same I2C bus as a future SCL/SDA expansion (GPIO 21/22). Use the same SSD1306 driver from the [pedometer-esp32-mpu6050] project (vendor it as a component — `idf.py add-dependency espressif/ssd1306`).
- Display:
  - Line 1: large frequency in scientific format (`1.000 MHz`, `100.0 kHz`, etc.)
  - Line 2: mode (`FREQ` / `PER`)
  - Line 3: gate window in ms
  - Line 4: count + estimated accuracy at this gate (`±X Hz @ this gate`)
- Add a long-press on GPIO 0 (BOOT button) → cycle gate window 100 ms / 1 s / 10 s. Print the new gate to the UART and update the OLED.
- Add an NVS-stored calibration offset. A short-press on GPIO 0 + UART command `cal 1000000` → store the offset that makes the current input equal 1.000 MHz.
- Print the current accuracy estimate to the OLED based on `±1_count_quantization + ±10_ppm_xtal + ±gate_jitter`.

**Time:** 6 hrs. **Risk:** Low. **Verification:** OLED shows clean readings; long-press cycles gate; NVS calibration survives a reboot.

### Phase 5 — Classic PCB design in KiCad 8 (Week 3, ~10 hrs, optional)

**Goal:** Move the breadboard build to a 2-layer PCB suitable for the lab bench.

This phase is **optional for the demo**, but it's the polish that lets you put a photo of the assembled board on the resume. Skip if you're short on time.

**Deliverables:**
- KiCad 8 schematic + 2-layer PCB layout. Size: ~100 × 70 mm. Components: all through-hole (DIP-8 NE555, DIP-14 CD4584, DIP-16 CD4040/CD4511) to keep it solderable with a 25 W iron.
- Stackup: 2-layer, 1.6 mm. No special design rules — JLCPCB 2-layer cheapest spec (5 mil/5 mil) is fine.
- Use the **BOOSTXL-DRV8323RS reference schematic naming conventions** for the power section (familiarity carries from the BLDC project).
- DRC + ERC clean.
- Order 5 boards from JLCPCB. ~₹500 PCB + ~₹400 shipping. Plan for 8 – 14 days delivery on Global Standard Direct Line ([JLCPCB customs]).
- Assemble one board after PCBs arrive. Estimated assembly time: 3 hrs hand-solder.

**Time:** 10 hrs design + 3 hrs assembly. **Risk:** Low. **Verification:** Assembled PCB matches breadboard behavior; both read the same 1 MHz input within 0.1 %.

### Phase 6 — Calibration against a reference (Week 3 – 4, ~6 hrs)

**Goal:** Quantify the accuracy of both instruments against a trusted reference.

**Deliverables:**
- Source a reference. Three tiers:
  - **A. Best:** the BITS Pilani Goa EI department's HP/Agilent 53131A or 53132A counter (10 MHz rubidium reference). Schedule a 1-hour slot with the lab in-charge.
  - **B. DIY:** a GPS-disciplined oscillator. NEO-6M GPS module + 1 PPS output + a CD4060 ÷ 10 chain gives a 100 kHz reference with sub-ppm accuracy. Total cost ~₹450 ([NEO-6M Robu]).
  - **C. Pragmatic:** a calibrated function generator. Borrow a Rigol DG1022 from the EI lab; trust its 10 ppm spec.
- Sweep input from 1 Hz to 1 MHz in decade steps. Log displayed reading vs. reference on both boards. Compute the **relative error** and plot as a Bode-style log-log chart.
- Store the calibration offset for the MCU board in NVS (`fcal_offset_ppm`).
- Generate a calibration certificate PDF (template in `docs/calibration_cert_template.tex`) with the date, ambient temperature, reference used, and the measured error table. This is the resume-polish artefact.

**Time:** 6 hrs. **Risk:** Low. **Verification:** MCU board accuracy ≤ 10 ppm after offset; classic board accuracy ≤ 1 % per the budget in `docs/accuracy_budget.md`.

### Phase 7 — Stretch goals (optional, ~time as available)

After MVP works, pick *one* of these to add polish:

- **Wi-Fi web dashboard** — the ESP32 hosts a `/measurements` JSON endpoint and a tiny single-page HTML that polls every 1 s. ~4 hrs work.
- **Pull-counter mode** — a digital tachometer mode where the input is an inductive proximity sensor on a rotating shaft, and the display shows RPM. ~3 hrs.
- **RP2040 + PIO port** — Phase 2.5 stretch from §2.4: re-implement the gate + counter on a Raspberry Pi Pico with PIO. Demonstrates programmable I/O state machines and adds a Pico to the BOM. ~8 hrs.
- **32.768 kHz XO timebase** for the classic board — see §2.1 alternative. Brings the classic build's accuracy from 0.5 – 1 % to ~10 ppm. ~3 hrs.

---

## 4. Detailed BOM with India sourcing

This is the **purchase list** for both boards. Where two suppliers are listed, prefer the first; the second is a backup. INR prices observed May 2026; verify at order time.

### 4.1 Classic board (analog timebase + CMOS logic)

| Ref | Part | Description | Qty | Supplier (primary) | INR @ qty | Notes |
|---|---|---|---|---|---|---|
| U1, U2 | NA555P (TI) | Bipolar timer, DIP-8 | 4 | Mouser India | ~₹15 ea | Buy *TI* parts, not unbranded clones — see §2.5. |
| U3 | CD4584BE | Hex Schmitt-trigger inverter, DIP-14 | 2 | Robu.in / QuartzComponents | ~₹25 ea | |
| U4 – U6 | CD4040BE | 12-stage binary ripple counter, DIP-16 | 4 | Robu.in | ~₹15 ea | |
| U7 – U10 | CD4511BE | BCD → 7-segment latch/decoder/driver, DIP-16 | 5 | Robu.in | ~₹20 ea | 1 spare. |
| DISP1 – 4 | Kingbright SA52-11SRWA | Common-cathode red 0.52" 7-seg | 5 | Mouser India | ~₹40 ea | |
| U11 | LM7809 | +9 V linear regulator, TO-220 | 2 | Robu.in | ~₹15 ea | |
| U12 | LM7805 | +5 V linear regulator, TO-220 | 2 | Robu.in | ~₹10 ea | |
| BR1 | DB107 | 1 A bridge rectifier | 2 | Robu.in | ~₹10 ea | |
| C_bulk | 1000 µF / 25 V radial | Smoothing cap | 2 | Robu.in | ~₹15 ea | |
| C_dec | 100 nF X7R 0805 | Decoupling per IC | 30 | Robu.in 0805 kit | ₹400 kit | One kit covers years. |
| C_555_timing | 100 µF / 16 V electrolytic | NE555 astable timing | 4 | Robu.in | ~₹3 ea | |
| C_555_mono | 10 µF C0G/NP0 ceramic | NE555 monostable timing (low drift) | 4 | Mouser India (Kemet) | ~₹50 ea | **C0G is critical** — X7R drifts 15 % over temperature. |
| R_timing | 6.8 kΩ / 91 kΩ 1 % metal film, 1/4 W | NE555 timing resistors | 10 each | Robu.in | ~₹2 ea | |
| R_trim | 10 kΩ multi-turn cermet pot | Astable trim | 2 | Robu.in (Bourns 3296W) | ~₹40 ea | |
| R_pull | 10 kΩ 1 % 0805 | Pull-ups | 30 | Robu.in 0805 kit | (in kit) | |
| SW_RESET | Tactile 6×6 mm momentary | Manual count reset | 2 | Robu.in | ~₹8 ea | |
| SW_MODE | DPDT slide switch | 1 s / 10 s gate | 2 | Robu.in | ~₹20 ea | |
| J_PWR | DC barrel jack 5.5/2.1 mm | 9 V wall-wart input | 2 | Robu.in | ~₹15 ea | |
| J_IN | BNC bulkhead | Signal input | 2 | Robu.in | ~₹120 ea | Optional — also accepts a 2.54 mm header for breadboard input. |
| **Classic subtotal** | | | | | **~₹1,400** | |

### 4.2 MCU board (ESP32 + OLED)

| Ref | Part | Description | Qty | Supplier (primary) | INR @ qty | Notes |
|---|---|---|---|---|---|---|
| U1 | ESP32-WROOM-32 dev kit (CP2102) | MCU + USB-UART | 2 | Robu.in ([ESP32 Robu]) | ~₹250 ea | 1 spare. |
| U2 | SSD1306 0.96" OLED I2C | 128×64 display | 2 | Robu.in | ~₹250 ea | |
| R_input | 1 kΩ 1 % 0805 | Series current limit on signal input | 5 | Robu.in 0805 kit | (in kit) | |
| D_clamp | 1N4148 | 3V3 clamp diode on input | 5 | Robu.in | ~₹2 ea | |
| SW_GATE | Tactile 6×6 mm momentary | Gate select (uses GPIO 0) | 2 | Robu.in | ~₹8 ea | |
| J_IN | BNC bulkhead (shared with classic board) | Signal input | — | (see classic) | — | |
| **MCU subtotal** | | | | | **~₹800** | |

### 4.3 Power supply (shared)

| Ref | Part | Description | Qty | Supplier | INR @ qty | Notes |
|---|---|---|---|---|---|---|
| W1 | 9V 1A wall-wart, barrel | Mains input | 2 | Robu.in / local | ~₹200 ea | |
| F1 | Pico-fuse 500 mA | Mains-side protection | 2 | Robu.in | ~₹10 ea | |
| H1 | Aluminium heatsink TO-220 | LM7809 heat dissipation | 4 | Robu.in | ~₹15 ea | |

### 4.4 PCB fabrication (optional, Phase 5)

| Service | Qty | Layers | Size | Assembly | INR delivered (Pune/Goa) |
|---|---|---|---|---|---|
| JLCPCB economic | 5 | 2 | 100×70 mm | None (through-hole only) | ~₹500 PCB + ~₹400 shipping = **~₹900 total** ([JLCPCB pricing]) |
| PCBPower (Indian) | 5 | 2 | 100×70 mm | None | ~₹1,200 — no customs risk, ~3 day Pune/Goa delivery ([PCBPower]) |

**Recommendation:** JLCPCB for cost; PCBPower if you need it fast for a demo.

### 4.5 Spares strategy

For a ₹2,500 BOM, getting 50 % spares on the cheap parts (every CD-series IC, every passive) is sensible. The 0805 + 1206 assortment kit from Robu (~₹500) covers years of project work — buy one if you don't already have it.

---

## 5. Tools and test equipment

You need these whether or not the project ships. They are **lifetime tools**.

### 5.1 Soldering (through-hole only for this project)

| Item | Spec | Where | INR |
|---|---|---|---|
| Soldering iron | Hakko FX-888D clone or 25 W generic | Robu.in | ~₹600 |
| Solder | 0.8 mm 60/40 leaded (faster wetting than lead-free for THT) | Robu.in | ~₹200 / 100 g |
| Flux pen | RMA flux | Robu.in | ~₹150 |
| Desoldering pump | Hakko clone | Robu.in | ~₹150 |
| Breadboard | 830-point + jumper wire kit | Robu.in | ~₹400 (combined) |

### 5.2 Probing

| Item | Spec | Where | INR |
|---|---|---|---|
| Oscilloscope | Rigol DS1054Z, 50 MHz, 4 ch — best budget scope in India | Robu.in ([Rigol DS1054Z India]) | ~₹40,000 |
| Scope alt | Hantek DSO5102P 100 MHz 2 ch | Amazon.in | ~₹22,000 — 2 ch is fine for this project (only need to look at gate + counter clock simultaneously) |
| Multimeter | Mastech MS8268 | Robu.in | ~₹2,500 |
| Function generator | FY6900 60 MHz 2-channel DDS — the project's input source | Aliexpress / Robu.in | ~₹6,000 |

For the calibration phase, a borrowed HP 53131A reference is ideal; a Rigol DG1022 (₹35k, BITS lab) is also fine.

### 5.3 Power

| Item | Spec | Where | INR |
|---|---|---|---|
| Bench PSU | Wanptek 30 V / 5 A or 9V wall-wart | Robu.in | ~₹3,000 (PSU) or ~₹200 (wall-wart) |

### 5.4 Software

| Tool | Use | Cost |
|---|---|---|
| ESP-IDF v5.4 LTS | Build | Free |
| KiCad 8 | Schematic + PCB (Phase 5) | Free |
| Python 3.11 + numpy + matplotlib | Sim | Free |
| LaTeX (TexLive) | Calibration cert | Free |

---

## 6. Reference designs and learning resources

### 6.1 Reference projects to study

| Project | What to copy | What to avoid |
|---|---|---|
| **Adafruit Frequency Counter** (legacy ATmega328 project) | Latch timing trick from the 4040→4511 transition. | Don't use an ATmega — slower than ESP32 and harder to debug. |
| **HP 5300-series** (1972 design) | The mode switch (frequency / period / time interval) UX. The HP user manual is on archive.org. | Don't copy the front-panel design literally — it's expensive to reproduce. |
| **EEVblog #1130 — building a frequency counter** | Excellent video walkthrough of the analog timebase trade-offs. | None — Dave Jones is a reliable source. |
| **TinyFPGA / iCEbreaker frequency counter examples** | Verilog reference for >40 MHz inputs. | Overkill for our 1 MHz spec. |

### 6.2 App notes (must-read)

1. **TI NE555 datasheet** — §7.4 (astable), §7.3 (monostable). [TI NE555 datasheet PDF].
2. **Microchip AN655** — Frequency Measurement Methods (direct, period, reciprocal). [Microchip AN655].
3. **ESP-IDF PCNT API Reference** — IDF v5.4. [ESP32 PCNT API guide].
4. **NXP CD4040 datasheet** — propagation delay vs. supply voltage matters above 1 MHz.

### 6.3 YouTube / blogs

1. **EEVblog #1130** — Frequency counter walkthrough. [EEVblog 1130].
2. **Phil's Lab — STM32 timer-based frequency counter** — same algorithm on a different MCU. Useful for cross-reference. [Phil's Lab #21].
3. **bigclivedotcom** — teardowns of cheap Chinese frequency counters; gives you a sense of what *not* to design.

### 6.4 Community

- **EEVblog Forum** — `Beginners → Frequency Counter` threads.
- **ESP32 Forum** — `Hardware → PCNT` discussions for the IDF v5 migration gotchas.

---

## 7. Risk register

| # | Risk | Probability | Impact | Mitigation |
|---|---|---|---|---|
| 1 | **Clone NE555 drifts > 5×** | High | Demo loses punch | Buy NA555P from Mouser India (₹15) — not Robu.in unbranded. |
| 2 | **Latch timing off** (display flickers) | High on first build | Wrong reading on last digit | Drive the CD4511 latch from a *delayed* falling edge — add a 22 nF cap + 100 kΩ on the latch line to delay by ~2 ms. |
| 3 | **NE555 supply noise feeds back into the timebase** | Med | 1 – 2 % timebase error | LM7809 + 100 nF decoupling on every IC + 100 µF bulk on the NE555 power pin. |
| 4 | **PCNT overflow miscounted** | Med (the scaffold has a known bug here) | Reading wrong at > 30k counts | Re-test the overflow callback per ESP-IDF v5.4 migration notes. Add a unit test on the host via the `idf-component-manager` host test framework. |
| 5 | **Input signal larger than 3V3** kills GPIO | High (first-time bring-up) | ESP32 input damaged | 1 kΩ series + 1N4148 clamp to 3V3 on the input. Tested in §3 Phase 3. |
| 6 | **Floating CD4040 clock** during bring-up | Med | Counter free-runs and shows garbage | 10 kΩ pull-down on the clock pin until input is connected. |
| 7 | **7-seg display dim** | Low | Visibility | Drive CD4511 from 5 V (not 3V3) and use 220 Ω current-limit resistors per segment. |
| 8 | **Calibration reference unavailable** | Med | Can't quantify accuracy | Fallback: NEO-6M GPS-disciplined 1 PPS (Phase 6 plan B). |
| 9 | **PCB customs delay** (if Phase 5) | Med | Schedule slip | Order 2 weeks early; PCBPower as backup. |
| 10 | **9V wall-wart undersized** | Low | LM7809 brown-out | Spec a 1 A unit; verify under load (~120 mA worst case for 5 ICs + 4 displays). |

### Top 3 risks (highlighted)

1. **Latch timing on the classic board** — the single most common reason "the last digit flickers". Mitigated by the delayed latch line in Phase 2.
2. **Clone NE555 drift** — defeats the whole story. Buy real TI parts.
3. **PCNT v5 API regression** — the scaffold predates IDF v5.4; re-validate the overflow callback.

---

## 8. Test and verification plan

### 8.1 Software-only tests (no hardware needed)

1. **Unit tests on the NE555 math.** `sim/test_ne555_math.py` (new) — `pytest` asserts `astable_period(6.8e3, 6.8e3, 100e-6) ≈ 1.413 s` and that `monostable_width(91e3, 10e-6) ≈ 1.001 s`. Drift sweep test: at 45 °C, period changes by ≤ 5 % from nominal.
2. **PCNT-API mock test** for the firmware: `freq_counter_init` + `freq_counter_measure` against a mocked `pulse_cnt.h` (using ESP-IDF host test framework). Mock the PCNT to return a known count; assert the computed frequency.
3. **Format string tests** for `fmt_frequency()` — 1.5e6 → "1.5000 MHz", 1500.0 → "1.500 kHz", 5.7 → "5.70 Hz".

### 8.2 Bench bring-up tests (in order)

| # | Test | Equipment | Pass criterion |
|---|---|---|---|
| 1 | **Classic PSU rails** | Multimeter | +9.05 V ± 0.1 V, +5.02 V ± 0.05 V |
| 2 | **Classic astable** | Scope | Square wave at 1.000 Hz ± 5 ms after pot trim |
| 3 | **Classic monostable** | Scope | 1.000 s pulse on rising edge of astable |
| 4 | **Classic Schmitt input** | Function gen + scope | Clean 0 – 5 V square at 1 kHz, 100 kHz, 1 MHz inputs |
| 5 | **Classic single digit** | Function gen | Display increments per count |
| 6 | **Classic 4 digits** | Function gen | Reads 1000 at 1 kHz input |
| 7 | **Classic latch timing** | Scope | Latch falling edge ≥ 1 ms after gate falling edge |
| 8 | **MCU PCNT bring-up** | Function gen | Counts 1 Hz to 1 MHz correctly |
| 9 | **MCU period mode** | Function gen at 5 Hz | Reads 5.0 Hz with auto period mode |
| 10 | **MCU OLED** | Visual | Shows freq + mode + gate + accuracy |
| 11 | **MCU NVS calibration** | Function gen + reboot | Stored offset survives power cycle |
| 12 | **24-hour soak** | Function gen at 1 kHz | Both boards stable within their spec |
| 13 | **Cross-board agreement** | Function gen | Classic and MCU read within 1 % of each other |

### 8.3 Accuracy verification (Phase 6)

Per `docs/accuracy_budget.md`, expected vs. measured error at three test points:

| Input | Classic spec | Classic measured | MCU spec | MCU measured |
|---|---|---|---|---|
| 100 Hz | ±1 Hz | TBD | ±0.001 Hz | TBD |
| 10 kHz | ±100 Hz | TBD | ±0.1 Hz | TBD |
| 1 MHz | ±10 kHz | TBD | ±10 Hz | TBD |

Record measured values during Phase 6; commit to `docs/calibration_log.md`.

### 8.4 Fault injection tests

| Fault | Method | Expected result |
|---|---|---|
| Input larger than 3V3 | Drive 5 V into the MCU input via 1 kΩ series | Clamp diode + series resistor protects MCU; reading unaffected |
| Power glitch | Disconnect wall-wart for 100 ms | Classic resets to 0; MCU restores last calibration offset from NVS |
| Floating input | Disconnect signal cable | Classic shows 0; MCU shows "no signal" message |
| Frequency above range | 50 MHz input | Classic shows garbage (above CD4040 max); MCU saturates at PCNT high_limit and shows overflow flag |

---

## 9. Cost summary

All prices INR, sourced May 2026. "Conservative" column adds 25 % spares on the cheap parts (which is trivial since they're ₹15 each).

### 9.1 BOM (one set of both boards)

| Category | Item | Conservative INR |
|---|---|---|
| Classic active ICs | NE555 × 4, CD4584, CD4040 × 4, CD4511 × 5 | 280 |
| Classic passives | timing R/C, decoupling, pull-ups | 250 |
| Classic display | 4 × Kingbright 7-seg | 200 |
| Classic PSU | LM7809, LM7805, bridge, bulk caps | 80 |
| Classic mechanical | switches, BNC, barrel jack | 350 |
| MCU board | ESP32 dev kit + OLED + I/O parts | 600 |
| Wall-warts + heatsinks (shared) | | 250 |
| **BOM subtotal** | | **2,010** |

### 9.2 PCB fabrication (Phase 5, optional)

| Item | INR |
|---|---|
| JLCPCB 2-layer × 5 + shipping | 900 |

### 9.3 Lab tools (one-time, lifetime use)

| Item | INR |
|---|---|
| Soldering kit (iron, solder, flux, pump) | 1,100 |
| Breadboard + jumper kit | 400 |
| Multimeter | 2,500 |
| Function generator (FY6900) | 6,000 |
| Oscilloscope (Rigol DS1054Z if needed) | 40,000 |
| **Lab tools subtotal** | **50,000** |

### 9.4 Grand total

| Scenario | Total INR |
|---|---|
| **Project-only (lab tools already owned)** | **~₹2,500** |
| **Project + missing soldering + multimeter + function gen** | ~₹12,000 |
| **All-in (cold start, including new scope)** | ~₹52,000 |

Most realistic for a BITS student: **₹2,500 – ₹3,500** assuming access to the institute's lab equipment (oscilloscope, function generator, calibrated reference).

---

## 10. Stretch goals (after MVP works)

These are explicitly **post-MVP**. Don't touch them until Phase 6 passes.

### 10.1 32.768 kHz watch-crystal classic timebase
Replace the NE555 astable with a CD4060 + 32.768 kHz Quartz crystal ÷ 32768 = 1 Hz reference. Cost: +₹50. Brings the classic board from 0.5 – 1 % to ~10 ppm — equal to the MCU board. Then the demo story becomes "I learned the limits of the analog timebase, fixed it with a watch crystal, and replicated the MCU's accuracy with 1980s parts."

### 10.2 RP2040 + PIO port
Re-implement the MCU build on a Raspberry Pi Pico (RP2040) using PIO state machines for the counter. Demonstrates programmable I/O and brings the upper frequency limit to ~100 MHz with care. ~8 hrs of work; great resume bullet on programmable I/O.

### 10.3 Web dashboard
The ESP32 hosts an HTTP server with a Server-Sent Events stream. A single-page HTML loads in any browser on the same Wi-Fi and shows the frequency in real time. ~4 hrs.

### 10.4 Period mode for very low frequencies
Push the period-measurement floor from 1 Hz down to 0.001 Hz. Useful for measuring breathing rate (~0.2 Hz) or extremely slow tachometers. ~3 hrs.

### 10.5 BLE GATT interface
Push readings over BLE to a phone app. The ESP32 has BLE on-die. ~6 hrs.

### 10.6 Self-calibration on boot via WWVB or DCF77 receiver
A 60 kHz WWVB or 77.5 kHz DCF77 RF clock module (~₹400 on Aliexpress) gives a pulse-per-second reference. The MCU can auto-calibrate on boot. India doesn't get WWVB coverage but does get the JJY (Japan) signal weakly; BPC (China) at 68.5 kHz is the most reliable Indian-coverage option but the modules are harder to source. Stretch goal, not core.

---

## 11. Resume bullet drafts

Two bullets in the style typical of EI/ECE candidates targeting hard-engineering roles.

> **Designed and built a dual-implementation digital frequency meter — a classic 1985-era discrete CMOS build (dual NE555 + CD4584 + CD4040 + CD4511, regulated linear PSU, 4-digit 7-segment display) and a modern ESP32-WROOM-32 rewrite using PCNT + gptimer hardware peripherals — to compare 1 % analog timebase accuracy against 10 ppm xtal-based accuracy on identical 1 Hz – 1 MHz input sweeps.** Calibrated both instruments against a GPS-disciplined reference and produced a measured accuracy log; designed the classic board's PCB in KiCad 8, hand-soldered, and verified against the BITS Pilani EI department's HP 53131A counter.

> **Implemented period-measurement auto-fallback below 100 Hz on the MCU build to maintain ±1-LSB resolution at low frequencies; integrated SSD1306 OLED display, NVS-persisted calibration offset, and a multi-window gate selector (100 ms / 1 s / 10 s) for variable speed/resolution trade-offs.** Wrote a Python simulation of the NE555 timebase (including thermal-drift modeling) and a `pytest` suite covering the timing math, period-mode threshold logic, and PCNT overflow accounting; produced a 4-page calibration certificate PDF documenting the measured error budget against §`docs/accuracy_budget.md`.

---

## 12. References

Numbered references used in this build plan, ordered by first appearance.

1. [TI NE555 datasheet](https://www.ti.com/lit/ds/symlink/ne555.pdf)
2. [TI NA555 datasheet (lower-noise variant)](https://www.ti.com/lit/ds/symlink/na555.pdf)
3. [NXP CD4040B datasheet](https://www.nxp.com/docs/en/data-sheet/HEF4040B.pdf)
4. [NXP CD4511B datasheet](https://www.nxp.com/docs/en/data-sheet/HEF4511B.pdf)
5. [NXP CD4584B datasheet](https://www.nxp.com/docs/en/data-sheet/HEF40106B.pdf)
6. [ESP32 PCNT API guide (IDF v5.4)](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/pcnt.html)
7. [ESP-IDF PCNT v5 migration notes](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/migration-guides/release-5.x/5.0/peripherals.html#pcnt)
8. [Microchip AN655 — Frequency Measurement Methods](https://ww1.microchip.com/downloads/en/Appnotes/00655a.pdf)
9. [EEVblog #1130 — Building a Frequency Counter (YouTube)](https://www.youtube.com/watch?v=PnK4cQhVLqQ)
10. [Phil's Lab #21 — STM32 Timer-Based Frequency Counter (YouTube)](https://www.youtube.com/watch?v=hX0OjqhaeQk)
11. [Robu.in — ESP32-WROOM-32 dev kit](https://robu.in/product/esp32-development-board-wifi-bluetooth-ultra-low-power-consumption-dual-cores-unsoldered/)
12. [Robu.in — Kingbright SA52-11SRWA 7-segment](https://robu.in/product/0-56-1-digit-7-segment-led-display-common-cathode/)
13. [Robu.in — SSD1306 0.96" OLED](https://robu.in/product/0-96-inch-yellow-blue-iic-i2c-oled-128x64-displaybluish-area-yellow-area/)
14. [Robu.in — NEO-6M GPS module (calibration reference, Phase 6 plan B)](https://robu.in/product/u-blox-neo-6m-gps-module-with-eeprom-and-active-antenna/)
15. [Robu.in — Wanptek WPS305H bench PSU](https://robu.in/product/wanptek-wps305h-30v-5a-variable-adjustable-switching-power-source-dc-power-supply/)
16. [JLCPCB PCB assembly pricing](https://jlcpcb.com/help/article/pcb-assembly-price)
17. [JLCPCB customs and taxes (India context)](https://jlcpcb.com/help/article/customs,-duties-and-taxes)
18. [PCBPower (Indian PCB house)](https://www.pcbpower.com/)
19. [Rigol DS1054Z review for India](http://revinetech.com/blog-detail/best-budget-oscilloscopes-in-india-2025)
20. [Bourns 3296W 10 kΩ multi-turn pot (Mouser India)](https://www.mouser.in/ProductDetail/Bourns/3296W-1-103LF)
21. [QuartzComponents (Indian supplier)](https://quartzcomponents.com/)
22. [HP 5300-series counter user manuals (archive.org)](https://archive.org/details/hp_5300-series)
23. [Adafruit frequency counter project (ATmega328)](https://learn.adafruit.com/)
24. [bigclivedotcom — cheap frequency counter teardown](https://www.youtube.com/c/bigclivedotcom)

---

*End of build plan. Last edit: 2026-05-28.*

# ESP32 Signage Firmware — Phase 1 · Part 2 · Battery Calibration Report

| | |
|---|---|
| **Phase** | 1 · Part 2 — Battery (calibration groundwork) |
| **Deliverable** | Battery-calibration bench tool + first characterization results |
| **Tool** | `Embedded/ESP32/Workbench/battery_calibration/battery_calibration.ino` |
| **Battery under test** | **Pro-range 6 Ah, 12.8 V LiFePO₄ (4S) — "orange pack"**, with inbuilt BMS |
| **Sense hardware** | ESP32 `GPIO39` via a **100 kΩ / 18 kΩ** resistive divider |
| **Data captured** | `battery_calibration/Battery_at 13.8(PSU_14.5V).csv`, `battery_calibration/Capture_2_14.5V.csv` |
| **Outcome** | ✅ Tool validated · ⚠️ Precise voltage-SoC not feasible on this hardware → **coarse provisional gauge (Option B)** |
| **Status** | Calibration code finalized; threshold values provisional, pending a full discharge-to-cutoff run |

---

## 1. Executive summary

A standalone serial bench tool was built to characterize the node's backup battery so a state-of-charge gauge can be added to the firmware in Phase 1 · Part 2. The tool measures battery voltage (factory-calibrated ADC), calibrates the divider ratio to the board, drives the real operating load, and logs a timestamped CSV discharge.

Testing on the **Pro-range 6 Ah 12.8 V LFP (orange) pack** established that a **precise** voltage-to-percentage gauge is **not achievable** on this hardware, for three compounding reasons: LiFePO₄'s intrinsically flat discharge curve, a large load-dependent voltage sag between the cells and the sense point, and ~±0.3 V of ADC/measurement noise that is larger than the SoC signal across much of the capacity.

The project therefore adopts **Option B: a coarse, honest battery indication** (a small number of voltage bands) rather than a false-precision percentage. The calibration **code is complete and validated**; the specific band thresholds are **provisional** and will be finalized from a later full discharge-to-cutoff test.

---

## 2. Purpose of the calibration tool

1. Observe the ADC / battery voltage across operating conditions (charging, rest, load, discharge).
2. Calibrate the resistive-divider ratio to the specific board.
3. Drive the real operating load and log a timestamped discharge to CSV.
4. Provide the data needed to set the firmware's battery thresholds.

It is standalone (serial only — no Ethernet/MQTT) and does not modify the production firmware.

---

## 3. Battery under test

| Property | Value |
|---|---|
| Chemistry | LiFePO₄ (LFP) |
| Product | **Pro-range "orange pack"** |
| Configuration | 4S (4 cells in series) |
| Nominal voltage | 12.8 V (3.2 V/cell) |
| Capacity | 6 Ah |
| Protection | Inbuilt BMS (low-voltage cutoff spec unpublished — to be determined empirically) |
| Typical landmarks (4S LFP) | Full rested ≈ 13.4–13.6 V · plateau ≈ 12.8–13.2 V · knee ≈ 12.0 V · BMS cutoff ≈ 10.0 V |

---

## 4. Sense hardware & method

- **Divider:** 100 kΩ (battery⁺ → sense pin) / 18 kΩ (sense pin → GND) → nominal ratio `(100+18)/18 = 6.556`. At 14.6 V this presents ~2.23 V to the ADC — safely inside the linear region.
- **ADC read:** `analogReadMilliVolts()`, which applies the ESP32's factory eFuse ADC calibration internally — materially more accurate than the naive `(raw/4095)×3.3`, which matters because the useful LFP signal is only tens of millivolts.
- **`Vbatt = analogReadMilliVolts(pin)/1000 × BATT_DIV_RATIO`**, median-filtered (51 samples).
- **Ratio calibration:** the `d` command back-solves an *effective* ratio from a multimeter reading of the battery terminal, absorbing both resistor tolerance and residual ADC error. A single earlier check showed the nominal ratio over-reads ~1.6 % at 13.85 V.

---

## 5. The tool

Standalone sketch; serial menu at 115200 baud.

| Key | Function |
|---|---|
| `h` | Menu |
| `r` | Read once (raw ADC, pin mV, Vbatt, %) |
| `s` | Toggle 1 Hz human-readable stream |
| `c` | Toggle CSV discharge log — `elapsed_s,rawADC,pin_mV,Vbatt,pct`, one row / 30 s |
| `1` / `2` / `0` | Load: left-side chase / right-side chase / off (the real operating load) |
| `d` | Calibrate divider ratio from a multimeter reading |
| `e` / `m` / `f` | Mark current voltage as 0 % / 50 % / 100 % |
| `p` | Print paste-ready block (`BATT_DIV_RATIO`, `BATT_V0/V50/V100`) |

**Intended gauge model:** a two-segment piecewise-linear map anchored on three measured voltages (`V0`/`V50`/`V100`). Because LFP is flat mid-range, the 50 % anchor is placed by **time/Ah** (half the runtime), not by voltage. Anchors are captured **on battery** (PSU off — the charger otherwise masks SoC) and **under the operating load**. The unknown value is always **65535**, never 0 % (which would be indistinguishable from a genuinely flat battery).

**Operating load reproduced by the tool:** the node normally shows a single-side chase, so the battery load is the two always-on static zones (~650 mA each, ~1.3 A) plus a 2-strip arrow chase (~0.17 A) ≈ **~1.5 A total**.

---

## 6. Test setup & procedure

- Pack charged via the node's own path (14.5 V PSU → MBR1035 diode → battery), then the **PSU physically disconnected** for the discharge.
- ESP32 powered from the laptop over USB (for serial capture); LED rails and static zones powered from the battery bus and confirmed lit and drawing.
- Load set to the right-side chase (`2`); CSV logging (`c`) started at PSU disconnect.
- Serial captured to file with CoolTerm.

---

## 7. Results

### 7.1 Preliminary snapshot — `Battery_at 13.8(PSU_14.5V).csv`
A short (~74 s) capture taken in **stream** mode (not CSV). Battery read ~12.8 V (uncalibrated, nominal ratio) under load; used only to confirm the tool and load control worked. No discharge information.

### 7.2 Discharge run — `Capture_2_14.5V.csv`
CSV mode, 30 s cadence, **~80 minutes** (elapsed 0 → 4800 s), PSU disconnected, right-side chase load.

| Observation | Value |
|---|---|
| Duration | ~80 min |
| `Vbatt` range | ~11.1 – 11.9 V |
| Trend | **None** — flat, within noise; no downward slope, no knee, no collapse |
| Noise (row-to-row) | ~±0.3 V (pin_mV ≈ 1700–1820 mV) |
| Anchors obtainable | **None** — the pack never approached empty |

> Some rows carry corrupted leading bytes (a serial-capture artifact). The numeric data after them is intact and was used as-is; the corruption is out of scope for this round.

### 7.3 Analysis
The pack held a flat ~11.5 V for the entire run without discharging. This is consistent with the cells sitting on their plateau while a **large load-dependent series voltage drop** depresses the sensed voltage below the true cell voltage. In other words, ~11.5 V is *plateau minus sag*, not a low state of charge — the pack has substantial runtime remaining, which is why 80 minutes produced no measurable decline.

Three factors together make a precise voltage-SoC gauge infeasible on this hardware:
1. **Flat LFP curve** — voltage barely moves across ~20–90 % SoC.
2. **Load-dependent sag** — the sensed voltage shifts with load current, and the node cannot measure battery current directly to compensate.
3. **Measurement noise (~±0.3 V)** — larger than the SoC signal over much of the range.

---

## 8. Findings (hardware)

| # | Finding | Implication |
|---|---|---|
| F1 | **Undercharge.** 14.5 V PSU − MBR1035 (~0.3 V) = ~14.15 V at the pack = 3.54 V/cell, below LFP's 3.6–3.65 V/cell full-charge window. | Pack never fully charges; BMS never balances. A bare diode from a fixed PSU is not a CC/CV charger. |
| F2 | **12 V LED-rail dropout on battery.** With the pack near 12 V the buck (MP1584, buck-only) cannot regulate; rail sags to ~11.3 V and LEDs dim, worsening as the pack discharges. | **PCB V3.1 item** — buck-boost/SEPIC, lower rail voltage, or direct drive. |
| F3 | **Large load sag / series resistance** between cells and sense point (from §7.3). | Depresses sensed voltage under load; further undermines voltage-SoC. Root cause (diode / BMS / wiring / undercharge) to be isolated with a rested-vs-loaded sag measurement. |
| F4 | **Load ≈ 1.5 A** (static zones dominate at ~1.3 A). | Runtime is set by the full static + chase load; expect a multi-hour discharge to reach empty. |
| F5 | **Battery reading is noisy (~±0.3 V).** | The firmware must filter the battery far harder than the current logic — a long average, since the signal is slow. |

---

## 9. Decision — Option B (coarse gauge)

A precise percentage would be false precision on this hardware. The firmware will therefore present a **coarse, honest battery indication** — a small set of voltage bands rather than a fine percentage — with heavy filtering and the existing "unknown = 65535, never 0 %" contract preserved.

**Provisional starting bands** (4S LFP, *rested* reference — loaded values sit lower and must be corrected for sag; **all unverified, to be finalized from a full discharge test**):

| Band | Meaning | Rested 4S guide | Note |
|---|---|---|---|
| OK | Healthy | ≳ 12.8 V | upper plateau |
| LOW | Getting low | ~12.0 V | entering the knee |
| CRITICAL | Near cutoff | ~10.5–11.0 V (loaded) | above the BMS trip, with reporting margin |

These are **placeholders**, not calibrated thresholds — they exist to structure the firmware, and will be replaced once a discharge-to-cutoff run gives real loaded values.

---

## 10. Open items / further testing (deferred)

- **Full discharge-to-cutoff run** (several hours) to obtain real loaded band voltages, the BMS cutoff, and the runtime.
- **Rested-vs-loaded sag** measurement (`0` vs `2`, ÷ ~1.5 A) to isolate F3's cause.
- **Charging fix** (F1) so the pack reaches a true full charge before characterization.
- **Two-point ratio calibration** if a single ratio proves inaccurate at the low end.
- **PCB V3.1**: LED-rail dropout (F2) and charge topology.

---

## 11. Status & next step

- **Calibration code:** ✅ finalized and committed — captures clean, timestamped CSV and drives the real load as designed.
- **Threshold values:** ⚠️ provisional (Option B), pending the deferred discharge test.
- **Next:** merge the battery sensing + coarse gauge into the production firmware (`Workbench/Firmware/Firmware.ino`) to complete Phase 1 — to be designed in the upcoming merge discussion.

---

## 12. File inventory

```
Embedded/ESP32/Workbench/
├── battery_calibration/
│   ├── battery_calibration.ino          # the calibration tool (finalized)
│   ├── Battery_at 13.8(PSU_14.5V).csv    # preliminary snapshot (stream)
│   └── Capture_2_14.5V.csv               # 80-min discharge run (CSV)
└── Docs/
    ├── Phase1-Part1_Completion-Report.md
    └── Phase1-Part2_Battery-Calibration-Report.md   # this document
```

---

*End of Phase 1 · Part 2 battery-calibration report.*

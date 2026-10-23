# ESP32 Signage Controller — Workbench Build (Firmware v4.1.0-p2)

**Phase:** 1 · Part 2 — *hardcoded bench build, with battery*
**Role:** Hardware edge controller & network node
**Pairs with:** `RaspberryPi/Gateway/Pi.py` v2.0.0 · HMI `RaspberryPi/HMI/HMI_new/hmi_{light,dark}.py`
**Contract:** [`../../../esp32_contract.md`](../../../esp32_contract.md) — authoritative; wins on any conflict.
**Baseline:** v4.0.0-p1 (Phase 1 · Part 1). This document covers only what is **different** from that build — see the Part-1 `firmware_doc.md` and [`../../Docs/Phase1-Part1_Completion-Report.md`](../../Docs/Phase1-Part1_Completion-Report.md) for the full reference.
**Doc last reconciled against code:** 2026-09-17

---

## 1. What this build adds

v4.0.0-p1 (Ethernet + MQTT + command execution + intent-echo arrows + 2-LED wrapping chase) **plus**:

1. **A coarse battery gauge** — published as `0 / 50 / 100` on `battery_pct`.
2. **A watchdog-init fix** — the loopTask WDT reboot loop is gone.
3. **A separate `b` serial stream** for battery calibration.

Everything remains hardcoded (edit-the-constants-and-reflash); no runtime persistence (NVS is Phase 2).

---

## 2. Battery gauge (Phase 1 · Part 2)

### 2.1 Hardware & read path
- **Sense:** `GPIO39` via a **100 kΩ (batt⁺→pin) / 18 kΩ (pin→GND)** divider → nominal ratio `(100+18)/18 = 6.556` (`BATT_DIV_RATIO`, calibrate per board).
- **Read:** `analogReadMilliVolts()` (factory eFuse ADC calibration), median-of-51 — the **same method as the calibration tool**, so its thresholds transfer. Then a **heavy IIR** (`BATT_ALPHA = 0.10`) because the signal is slow and noisy (~±0.3 V raw).

### 2.2 Coarse mapping (Option B)
A precise percentage is **not achievable** on this hardware (flat LFP curve + load sag + ADC noise — see [`../../Docs/Phase1-Part2_Battery-Calibration-Report.md`](../../Docs/Phase1-Part2_Battery-Calibration-Report.md)). The node therefore publishes only three values, by threshold on the **loaded** voltage, with hysteresis (`BATT_HYST_V = 0.15 V`) to stop boundary chatter:

| Reading | Published |
|---|---|
| `Vbatt ≥ 11.5 V` (`BATT_FULL_V`) | **100** |
| `10.8 V ≤ Vbatt < 11.5 V` (`BATT_LOW_V`) | **50** |
| `Vbatt < 10.8 V` | **0** |

> **These thresholds are PROVISIONAL.** "50" here means *"on battery, discharging, not yet critical"* rather than exactly 50 % SoC. Refine after a full discharge-to-cutoff test.

### 2.3 Semantics
- **No charging-detection branch.** On mains the pack sits high (~13.8 V) → reads **100** naturally. When the PSU fails (`power=FAIL`, the critical alarm) the number simply follows the pack down as it takes over.
- **Unknown = 65535, never a bare 0.** The node publishes nothing until the first sample; before that the gateway shows `65535` (unknown). It only ever sends `0` as a genuine near-empty band.
- **Battery is orthogonal to the PSU alarm.** They use separate dividers (PSU on `GPIO36`, battery on `GPIO39`); a node with no battery and no PSU has no power and is simply *OFFLINE* — a different signal.

### 2.4 Downstream
- **Gateway:** no code change — `map_battery_to_int()` already passes the number to SCADA diagnostic register **+4**.
- **HMI:** `hmi_light.py` / `hmi_dark.py` now display the value (100 % green / 50 % amber / 0 % red; N/A when offline/unknown) instead of the old static "N/A · NOT FITTED".

**Battery under test:** Pro-range 6 Ah, 12.8 V, 4S LiFePO₄ ("orange pack") with inbuilt BMS.

---

## 3. Watchdog fix

**Symptom (v4.0.0-p1 and earlier):** periodic `task_wdt: loopTask (CPU 1) did not reset the watchdog` panics → reboot → Ethernet dropped, plus `esp_task_wdt_init(517): TWDT already initialized` on boot.

**Cause:** arduino-esp32 3.x already starts the Task Watchdog, so `esp_task_wdt_init()` returned `ESP_ERR_INVALID_STATE` — our intended **15 s never applied**, leaving the short default. A blocking network call (MQTT read, or the TCP connect handshake to an unreachable broker) then overran it.

**Fix (in `setup()`):**
- **`esp_task_wdt_reconfigure()`** (when init reports already-initialised) so 15 s actually takes effect; `idle_core_mask = 0` so only the tasks we add (loopTask + SensorTask) are watched, not the idle tasks.
- **`client.setSocketTimeout(2)`** — caps MQTT byte-stream waits.
- **`ethClient.setConnectionTimeout(5000)`** — caps the **TCP** connect (which `setSocketTimeout` does *not* govern; an unreachable broker would otherwise stall `client.connect()` ~30 s).

All three sit well under the 15 s WDT, so a network flap self-clears instead of rebooting.

---

## 4. Serial menu (115200 baud)

| Key | Action |
|---|---|
| `h` | Menu |
| `i` | Node info — now also prints battery `%` and `Vbatt` |
| `s` | Toggle current-sensor stream (1 Hz) |
| `b` | **Toggle battery stream** (1 Hz: raw ADC, pin mV, Vbatt, and the band `%`) |

`b` is deliberately separate from `s`: during commissioning the battery is calibrated while the node runs on the PSU. Watch the stream, hand-tune `BATT_DIV_RATIO` / thresholds, reflash. No blocking serial input (also good watchdog hygiene).

---

## 5. Unchanged from v4.0.0-p1

Network-first boot; MCP write-verify-recover; verified `state`; hold-last-command 2-LED wrapping chase; validated command parse; PSU hysteresis+debounce; **arrow current health = intent-echo** (`ON`/`OFF`, never `FAIL_*`); static zones = measured 4-state; dual-core FreeRTOS + queue.

---

## 6. Build & verify

```
Sketch uses 1014332 bytes (77%) of program storage space.
Global variables use 49296 bytes (15%) of dynamic memory.
```

```bash
Embedded/bin/arduino-cli compile --fqbn esp32:esp32:esp32 Embedded/ESP32/Workbench/v4.1.0/v4.1.0.ino
```

> **Bench verification pending:** flash, confirm no WDT reboots on an Ethernet flap, and confirm `battery_pct` reads 100 on mains and steps down on the PSU-off discharge.

---

## 7. Roadmap

- **Completes Phase 1** (firmware side). Remaining Phase-1 housekeeping: README/contract note that battery is now live; provisional thresholds to be finalized from a full discharge run.
- **Phase 2:** NVS persistence → NVS-backed auto-calibration → OTA over Ethernet → device identity (node ID + SW/PCB version).
- **Phase 3:** scheduled diagnostics + on-demand all-on self-test (the real arrow-health check).
- **PCB V3.1 (hardware):** 12 V LED-rail dropout on battery; charging voltage (undercharge via the MBR1035 path).

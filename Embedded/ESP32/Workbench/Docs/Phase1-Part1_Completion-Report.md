# ESP32 Signage Firmware — Phase 1 · Part 1 Completion Report

| | |
|---|---|
| **Build** | `v4.0.0-p1` — Hardcoded Bench Build |
| **Component** | ESP32 emergency-signage node firmware |
| **Working file** | `Embedded/ESP32/Workbench/Firmware/Firmware.ino` |
| **Baseline** | Forked from production `ESP32/Firmware/Firmware.ino` v3.0.0 |
| **Pairs with** | `RaspberryPi/Gateway/Pi.py` v2.0.0 |
| **Authoritative contract** | `Embedded/ESP32/esp32_contract.md` (wins on any conflict) |
| **Status** | ✅ Complete — compiles clean, flashed and bench-tested for connectivity |
| **Toolchain** | `esp32:esp32` core 3.3.8 · PubSubClient 2.8 · Adafruit MCP23017 2.3.2 |

---

## 1. Executive summary

Phase 1 · Part 1 delivers a **stripped, fully hardcoded bench build** of the signage node firmware whose sole purpose is bring-up: get a handful of nodes onto wired Ethernet, connected to the Raspberry Pi broker, executing commands, and streaming telemetry, so they can be left running and observed.

Relative to the v3.0.0 baseline, this build **removes** the runtime auto-calibration machinery and all battery logic, **freezes** every configuration value into compile-time constants, **reduces** the serial interface to three observation commands, **replaces** the chase animation with the field-approved pattern, and **simplifies** the arrow current-health reporting to an intent-echo after establishing that the sensor cannot resolve the arrow load.

The result is a smaller, more honest, more predictable firmware: 77% flash, 15% RAM, no runtime state that can drift, and no telemetry channel that reports a health verdict it cannot actually measure.

---

## 2. Objective & scope

### In scope
- A node that reliably boots on Ethernet, connects to the broker, and holds a stable session.
- Command execution (all modes) driving the LED MOSFETs, with verified writes.
- Telemetry: status, power, per-channel current state, and actual-executing state.
- All configuration baked into source as `const` (edit-and-reflash model).
- The production chase animation as the power-on behaviour.

### Explicitly out of scope (deferred by design)
- **Battery** sensing/logic — Phase 1 · Part 2.
- **Runtime / auto-calibration** and persistence (NVS) — Phase 2.
- **OTA**, **device identity/versioning** — Phase 2.
- **Scheduled diagnostics / self-test** — Phase 3.

---

## 3. Deliverables

| Artifact | Path | Purpose |
|---|---|---|
| Firmware | `Workbench/Firmware/Firmware.ino` | The `v4.0.0-p1` build |
| Technical reference | `Workbench/Firmware/firmware_doc.md` | Per-build reference & change log |
| This report | `Workbench/Docs/Phase1-Part1_Completion-Report.md` | Milestone completion record |

---

## 4. Changes from the v3.0.0 baseline

### 4.1 Removed — runtime auto-calibration
The entire serial calibration suite and its scaffolding were deleted:

- **Commands:** `c` (zero), `g` (gain), `v` (dividers), `m` (manual), `p` (paste block).
- **Helpers:** `enterCalMode`, `exitCalMode`, `calSettle`, `checkDerivedSens`, `readSerialLine`.
- **The `calibrationActive` flag** and every branch gated on it (`publishState`, `publish_state_msg`, `runAnimationStateMachine`, `SensorTask`, `printInfo`). With calibration gone the flag was always-false; the hardware-fault path (`FAULT` → 65535) is now keyed on MCP health alone.
- **Constants:** `EXPECTED_*` load currents, `DS_SENS_*` datasheet references, `SENS_TOLERANCE`.

> Auto-calibration returns in **Phase 2**, deriving values and writing them to NVS rather than requiring a paste-and-reflash cycle.

### 4.2 Removed — battery
- `PIN_VOLT_BATT` (GPIO39), `BATT_DIV_RATIO`, and the battery line in the sensor stream.
- No battery pin, divider, percentage, or publication. Battery is reintroduced in **Phase 1 · Part 2**.
- No regression downstream: battery was already unpublished in v3.0.0, so the gateway/HMI continue to show `65535` = unknown, never `0%`.

### 4.3 Changed — configuration is now `const`
`SENS_*`, `ZERO_*`, `PSU_DIV_RATIO`, `PSU_FAIL_VOLTS`, `PSU_OK_VOLTS` are compile-time constants (previously mutable only so calibration could write them). "Hardcoded" is now compiler-enforced. Per-board values are edited in source and reflashed; each physical board carries its own measured constants (the header of the working file names the board they belong to).

### 4.4 Changed — serial menu
Reduced from `h/i/r/s/c/g/v/m/p` to **`h` / `i` / `s`** (see §6). The single-shot read (`r`) was dropped; `readSensorsVerbose()` is retained because the `s` stream uses it. No command in this build performs blocking input, so nothing can stall the main loop.

### 4.5 Changed — chase animation
The chase (modes 1/2/3) is now **Chase 1: a 2-LED wrapping arrow**, direction `A4 → A0`, stepping every **250 ms**.

| Frame | Bits `4→0` | Byte |
|---|---|---|
| 0 | `11000` | `0x18` |
| 1 | `01100` | `0x0C` |
| 2 | `00110` | `0x06` |
| 3 | `00011` | `0x03` |
| 4 | `10001` | `0x11` ← wrap frame |

One sweep = 5 × 250 ms = **1.25 s**. The wrap frame (`A4`+`A0` together) is intentional — it keeps the motion continuous with no jump-back. Because the arrow is now 2 strips wide, the chase intent uses `setThresholds(2, …)`. Pattern authority: `Workbench/Firmware_Test/led_test_2/CHASE_ANIMATION_SPEC.md`. Solid modes 4/5/6 (`0x1F`) are unchanged. The firmware remains **command-driven** (mode 1 = LHS chase, 2 = RHS, 3 = both) and does **not** implement the bench spec's auto-cycling phase loop.

### 4.6 Changed — arrow current health is now intent-echo
`current1` / `current2` (LHS / RHS arrows) now report **only `ON` / `OFF`, echoing the commanded intent** — never `FAIL_OPEN` / `FAIL_SHORT`.

**Rationale.** At the ~2-strip chase current the arrows draw only ~0.17 A. On the fitted 5 A ACS712 that is ~40 ADC counts of OFF→ON change — *inside* the sensor's own noise. No threshold separates a working chase from noise, so a measured verdict false-alarms both ways. This is a hardware signal-to-noise limit, not a tuning problem. **Static zones (`current3` / `current4`) are unchanged** — they draw enough current (~650 mA each) to keep the real measured 4-state. Genuine arrow health is deferred to the **Phase 3** scheduled all-on self-test, where five lit strips clear the noise floor.

---

## 5. Design decisions & rationale

| # | Decision | Why |
|---|---|---|
| D1 | Hardcode all config as `const` | Bench build must be deterministic; no runtime state that can drift or need persistence before NVS exists. |
| D2 | Strip auto-calibration entirely | It belongs with NVS (Phase 2); carrying dead calibration paths into a bench build adds risk with no benefit. |
| D3 | Remove battery cleanly, reintroduce in Part 2 | Keeps Part 1 minimal and the battery work a clean, self-contained increment. |
| D4 | Menu = `h`/`i`/`s` only | The build's job is observation, not commissioning. No blocking serial input can stall the loop. |
| D5 | Adopt the 2-LED wrapping chase at 250 ms | Field-validated pattern from `CHASE_ANIMATION_SPEC.md`; supersedes the v3.0.0 3-LED pattern. |
| D6 | Arrows → intent-echo | The 5 A ACS712 cannot resolve the arrow current; reporting a measured verdict would mean constant false alarms. Honesty over a health signal that does not exist at this load. |
| D7 | Keep static zones on measured 4-state | Static current (~650 mA) is well clear of the noise floor and reliably sensed. |

---

## 6. Serial interface (115200 baud)

| Key | Action |
|---|---|
| `h` | Print the menu. |
| `i` | Node info: register, Ethernet IP, MQTT state, MCP health, commanded vs actually-executing mode, port bytes. |
| `s` | Toggle the 1 Hz sensor stream (raw ADC, volts, amps, threshold, state, PSU bus voltage). Arrow rows show the intent-echo marked `(intent)`; static rows show measured 4-state. |

---

## 7. Configuration reference

Per-node, edited at the top of the working file. IP/register are the committed real values; calibration/threshold constants are per-board.

| Constant | Value (node 1 / reference) | Notes |
|---|---|---|
| `mqtt_server_ip` | `192.168.1.10` | Pi, ESP32-side static IP (common) |
| `mqtt_port` | `1883` | |
| `ASSIGNED_REGISTER` | `40001` | **Per-node** (node *n* = 40000 + *n*) |
| `local_IP` | `192.168.1.101` | **Per-node — unique** |
| `gateway_ip` / `subnet` / `primaryDNS` | `192.168.1.1` / `255.255.255.0` / `8.8.8.8` | common |
| `SENS_*` / `ZERO_*` | per-board | **All four ACS712 are 5A parts**; sensitivity is hand-calibrated (datasheet value not trusted) |
| `PSU_DIV_RATIO` | per-board | set against a multimeter |
| `PSU_FAIL_VOLTS` / `PSU_OK_VOLTS` | `10.0` / `11.0` | hysteresis band (placeholders) |
| `THRESH_PER_STRIP` / `THRESHOLD_STATIC` | `0.060` / `0.080`+ A | design thresholds |
| `THRESH_OFF_DETECT` / `NOISE_FLOOR` | `0.030` / `0.020` A | `NOISE_FLOOR` **must** stay below `THRESH_OFF_DETECT` |

### Pin map (unchanged from v3.0.0)
I²C `SDA=GPIO4 / SCL=GPIO13` (GPIO17 is the LAN8720 clock) · MCP23017 @ `0x20` · PSU sense `GPIO36` · ACS712 arrows `GPIO34/35`, static `GPIO32/33` · LAN8720 RMII per baseline. Battery sense (GPIO39) intentionally absent this build.

---

## 8. Preserved safety/connectivity core (carried over verbatim)

These are unchanged and remain load-bearing:

- **Network-first boot** — an MCP/I²C fault never blocks the PHY; the node still comes ONLINE and reports the fault as `state` = 65535.
- **MCP write-verify-recover** — every MOSFET write is read back (with the `0xA0` health pattern on the unused nibble); recovery retries every 2 s and re-applies the held output.
- **Verified `state`** — advances only after a verified write, never optimistically.
- **Hold-last-command animation** — the state machine runs unconditionally, surviving broker/link outages. (No NVS yet: a reboot comes up dark until the retained `value` topic re-commands it.)
- **Validated command parse** (`strtol`) — a corrupt payload is rejected and the prior mode re-asserted.
- **PSU detection** — hysteresis band + 3-read debounce; `power=FAIL` from an ONLINE node is the system's critical alarm.
- **Dual-core FreeRTOS + 15 s watchdog**, single-slot overwrite queue Core 0 → Core 1.

---

## 9. Verification

| Check | Result |
|---|---|
| Compile (`esp32:esp32:esp32`) | ✅ Clean — 77% program storage, 15% dynamic memory |
| No dangling references to removed symbols | ✅ Verified by grep |
| Ethernet + broker connectivity, command execution | ✅ Bench-verified on hardware |
| Arrow telemetry emits only `ON`/`OFF` | ✅ |
| Static telemetry retains measured 4-state | ✅ |

```bash
Embedded/bin/arduino-cli compile --fqbn esp32:esp32:esp32 \
  Embedded/ESP32/Workbench/Firmware/Firmware.ino
```

---

## 10. Known limitations & deferred items

- **Arrow LED health is not monitored during normal operation** (intent-echo). A dark arrow is not detected by current sensing between scheduled tests. Compensating control is the **Phase 3** all-on self-test. The `state` topic still catches command/MCP-path failures.
- **No persistence** — a reboot loses the last command until the retained `value` topic re-commands it (NVS is Phase 2).
- **Calibration is manual** — measured constants are edited into source and reflashed; per-board.
- **PSU thresholds are placeholders** anticipating the battery; on a mains-only bench a node sits safely above them, so `power` state is not yet meaningful.

---

## 11. Contract impact (action required, not yet applied)

The arrow move to intent-echo is a **system-contract change** that this Workbench build is ahead of:

- The gateway maps `current1/2` to SCADA diagnostic registers **+2 / +3** ("LHS / RHS LED Health"), documented as 4-state in `esp32_contract.md` and `RaspberryPi/Gateway/SCADA_Integration_Guide.md`.
- Those registers now carry only **0 / 1** for the arrows (never 2 / 3). Any SCADA alarm keyed on `FAIL_*` for the arrows will never fire.
- **Those authoritative documents still need updating** to reflect the arrow 2-state semantics. No firmware change is required on the gateway/HMI side (both already handle `ON`/`OFF`).

---

## 12. Roadmap

| Phase | Content | Status |
|---|---|---|
| **1 · Part 1** | Hardcoded bench build (this report) | ✅ Complete |
| **1 · Part 2** | Battery logic (sense, percentage, publish) | 🔄 In progress — calibration tool built |
| **2** | NVS persistence → NVS-backed auto-calibration → OTA over Ethernet → device identity (node ID + SW/PCB version) | ⏳ Planned |
| **3** | Scheduled diagnostics: daily/weekly/monthly + on-demand all-on self-test (the compensating control for arrow health) | ⏳ Planned |

---

## 13. File inventory

```
Embedded/ESP32/Workbench/
├── Firmware/
│   ├── Firmware.ino          # v4.0.0-p1 (this report's subject)
│   └── firmware_doc.md       # per-build technical reference
├── Docs/
│   └── Phase1-Part1_Completion-Report.md   # this document
├── battery_calibration/      # Phase 1 · Part 2 (in progress)
└── Firmware_Test/            # bench sketches, incl. led_test_2/CHASE_ANIMATION_SPEC.md
```

---

*End of Phase 1 · Part 1 completion report.*

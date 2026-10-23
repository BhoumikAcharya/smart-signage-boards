# ESP32 Signage Controller — Firmware v4.1.0-p2

> ## ✅ THIS DOCUMENT IS AUTHORITATIVE
>
> **Promoted 2026-10-23.** This file is the source of truth for ESP32 node behaviour and supersedes
> [`../../../esp32_contract.md`](../../../esp32_contract.md), which is now a historical record.
> Where the two disagree, **this file wins**.
>
> It was previously a delta document covering only what changed from v4.0.0-p1. It has been made
> **self-sufficient**: §8–§14 below absorb the sections that used to live only in the contract, each
> re-verified against `v4.1.0.ino` rather than copied forward. The old inheritance chain
> (v4.1.0 → v4.0.0 → `Frimware_v3.0.0/` → a file that does not exist) is retired — nothing in this
> document depends on reading another build's doc.

**Phase:** 1 · Part 2 — *hardcoded bench build, with battery*
**Role:** Hardware edge controller & network node
**Source:** [`v4.1.0.ino`](v4.1.0.ino) — header `v4.1.0-p2`
**Pairs with:** `RaspberryPi/Gateway/Pi.py` v2.0.0 · HMI `RaspberryPi/HMI/HMI_new/hmi_{light,dark}.py`
**Wiring / build sheet:** [`PCB/PCB_V3/Connections.md`](../../../../../PCB/PCB_V3/Connections.md)
**Historical:** [`../../../esp32_contract.md`](../../../esp32_contract.md) (superseded) · [`v4.0.0/firmware_doc.md`](../v4.0.0/firmware_doc.md) (Part-1 delta)
**Doc last reconciled against code:** 2026-10-23

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
- **Sense:** `GPIO39` via a **100 kΩ (batt⁺→pin) / 18 kΩ (pin→GND)** divider → ratio `(100+18)/18 = 6.556` (`BATT_DIV_RATIO`). **Confirmed fitted 2026-10-23**; the board silkscreen prints `100k` and `18k`. Calibrate per board.
- **Tap point:** the divider sits on the **post-diode `MAIN_OP` bus**, where the battery ties in — not on the battery terminal in isolation. On mains it therefore reads the bus (~14.15 V); on PSU loss it follows the pack down.
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

> The watchdog is **internal hang recovery only**. It is not a network watchdog and does not implement any fail-safe. See §13.

---

## 4. Serial menu (115200 baud)

| Key | Action |
|---|---|
| `h` | Menu |
| `i` | Node info — net, MCP health, active mode, battery `%` and `Vbatt` |
| `s` | Toggle current-sensor stream (1 Hz) |
| `b` | **Toggle battery stream** (1 Hz: raw ADC, pin mV, Vbatt, and the band `%`) |

**These four keys are the whole menu.** The nine-key auto-calibration menu (`r c g v m p`) was removed in v4.0.0-p1 and returns in Phase 2 backed by NVS. Calibration today is observe-the-stream, hand-edit the constants, reflash.

`b` is deliberately separate from `s`: during commissioning the battery is calibrated while the node runs on the PSU. No blocking serial input (also good watchdog hygiene).

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
Embedded/bin/arduino-cli compile --fqbn esp32:esp32:esp32 Embedded/ESP32/Workbench/Firmware/v4.1.0/v4.1.0.ino
```

Toolchain: `arduino-cli` is **not** on PATH — use the repo copy at `Embedded/bin/arduino-cli`. Core `esp32:esp32` 3.3.8; libraries PubSubClient 2.8, Adafruit MCP23017 2.3.2.

> **Bench verification pending:** flash, confirm no WDT reboots on an Ethernet flap, and confirm `battery_pct` reads 100 on mains and steps down on the PSU-off discharge.

---

## 7. Roadmap

- **Completes Phase 1** (firmware side). Remaining Phase-1 housekeeping: provisional battery thresholds to be finalized from a full discharge run.
- **Phase 2:** NVS persistence → NVS-backed auto-calibration → OTA over Ethernet → device identity (node ID + SW/PCB version).
- **Phase 3:** scheduled diagnostics + on-demand all-on self-test (the real arrow-health check — see §11).
- **PCB V3.1 (hardware):** 12 V LED-rail dropout on battery; charging voltage (undercharge via the MBR1035 path).

---

# Authoritative reference — absorbed 2026-10-23

Everything below was previously held only in `esp32_contract.md`. Each section has been
re-verified line-by-line against `v4.1.0.ino`; where the contract and the code disagreed, **the
code is recorded here and the contract is wrong**. Those divergences are called out inline.

---

## 8. Hardware pin map

| Function | GPIO | Notes |
| --- | --- | --- |
| I²C SDA | **4** | 4.7 kΩ pull-up to 3V3 (**R21**, fitted — not DNP) |
| I²C SCL | **13** | 4.7 kΩ pull-up to 3V3 (**R22**, fitted — not DNP) |
| ACS712 #1 — LHS arrows | 34 | switched load |
| ACS712 #2 — RHS arrows | 35 | switched load |
| ACS712 #3 — Static Zone 1 | 32 | **always-on** load |
| ACS712 #4 — Static Zone 2 | 33 | **always-on** load |
| PSU voltage divider | 36 (VP) | R23/R24 = 100 k / 18 k, tapped **pre-diode**. See §14 |
| Battery voltage divider | 39 (VN) | R25/R26 = 100 k / 18 k, tapped on `MAIN_OP`. See §2.1 |

> **I²C must stay on 4/13.** GPIO17 is `ETH_CLOCK_GPIO17_OUT`, the 50 MHz RMII reference clock.
> Putting I²C there has the bus fighting the Ethernet clock. Earlier builds used 16/17.

**LAN8720 RMII:** MDC=23, MDIO=18, CRS_DV=27, RXD0=25, RXD1=26, TXD0=19, TXD1=22, TX_EN=21,
REFCLK=GPIO17 (`ETH_CLOCK_GPIO17_OUT`), `PHY_ADDR=1`, `PHY_POWER=-1` (PHY supply is tied
permanently to the 3.3 V rail).

> **GPIO0 is deliberately UNCONNECTED.** It appears in no net on PCB V3 or V3.1. The ESP32
> *generates* the RMII clock on GPIO17; it does not receive one. **Do not wire GPIO0 to the PHY** —
> it is a strapping pin, and a 50 MHz signal sampled low at reset drops the chip into
> firmware-download mode instead of booting, intermittently.
>
> The Olimex ESP32-PoE reference design in `PCB/Open Source Files/ESP32/` labels GPIO0 as
> `EMAC_TX_CLK` because it uses the opposite clock topology (PHY→ESP32). **Do not copy it.**

The six RMII data pins (TXD0/TXD1/TX_EN/RXD0/RXD1/CRS_DV) are fixed in ESP32 silicon and are not
configurable — that is why they do not appear in `ETH.begin()`.

**MCP23017** @ `0x20` (A0/A1/A2→GND, RESET→3V3). Port **A0–A4 = LHS** 5 MOSFETs, Port
**B0–B4 = RHS** 5 MOSFETs. 100 Ω gate series, 10 kΩ gate pull-down. All 16 pins set OUTPUT;
A5–A7 / B5–B7 unused and **must stay unconnected** (see §10).

**Network (per board, top of the `.ino`):** static IP, e.g. node 1 `192.168.1.101`; gateway
`192.168.1.1`; broker (Pi) `192.168.1.10:1883`; `ASSIGNED_REGISTER` 40001 for node 1.

**ADC:** 12-bit, `ADC_11db`, ADC1 only (ADC2 is unusable while the Ethernet/WiFi stack is active).
Current channels use median-of-51 raw reads then IIR `ALPHA = 0.15`; the battery channel uses
`analogReadMilliVolts()` (see §2.1), the PSU channel the raw path (see §14).

### ACS712 variants

**All four channels are 5 A parts**, `SENS = 0.185 V/A` on every channel (`v4.1.0.ino:72-75`).
Zero points are per-board and hand-calibrated.

> ⚠️ **Divergence from `esp32_contract.md` §1.** The contract recommends 20 A parts on the static
> zones with `SENS_STA1/STA2 = 0.100`. The shipped firmware uses 0.185 on all four. Confirm against
> the markings on the fitted parts and close bench item **B2**.

---

## 9. Command decode

Subscribe: `metro/signage/register/<40001+i>/value`, **QoS 1**. Payload is a decimal integer as ASCII.

> **`<address>` is the holding-register number (`40001 + i`), NOT the node index.** Node 1 is
> `register/40001/value`. Get this wrong and the node reads OFFLINE with no error logged anywhere.

**Commands control the moving arrows ONLY.** Static Zones 1 and 2 are hardwired always-on —
current-monitored, never switched. No command value affects them.

| Value | Action | Port A (LHS) | Port B (RHS) | Access |
| --- | --- | --- | --- | --- |
| `0` | Arrows OFF | `0x00` | `0x00` | Operator |
| `1` | LHS chase | chase frame | `0x00` | Operator |
| `2` | RHS chase | `0x00` | chase frame | Operator |
| `3` | Both chase | chase frame | chase frame | PIN |
| `4` | Solid ON left | `0x1F` | `0x00` | PIN |
| `5` | Solid ON right | `0x00` | `0x1F` | PIN |
| `6` | Solid ON all | `0x1F` | `0x1F` | PIN |
| `10000`–`11023` | Raw bitmask | `mask & 0x1F` | `(mask >> 5) & 0x1F` | PIN |

`mask = value − 10000`, 10 bits, low 5 = Port A, high 5 = Port B.

**The PIN is enforced in the HMI, not here.** Firmware executes any valid value it receives. Do not
add mode gating to the firmware.

**Technician modes latch.** No auto-revert timer.

### Chase animation — Chase-1, 2-LED wrapping

**Frames `{0x18, 0x0C, 0x06, 0x03, 0x11}`, 5 frames, `FRAME_MS = 250` (1.25 s per sweep).**
Each frame lights **2 of 5** strips, sweeping A4→A0. The final frame (`0x11`, A4+A0) is the
intentional wrap. Driven by a non-blocking `millis()` state machine — never `delay()`.
Spec: [`../../Firmware_Test/demo_mode_v2.0.0/CHASE_ANIMATION_SPEC.md`](../../Firmware_Test/demo_mode_v2.0.0/CHASE_ANIMATION_SPEC.md).

> ⚠️ **Divergence from `esp32_contract.md` §2.** The contract still specifies the old production
> table `{0x07, 0x0E, 0x1C, 0x19, 0x13}`, 300 ms, 3-of-5. That animation is not what ships.

**Parsing must distinguish "the number zero" from "not a number."** `atoi()` returns `0` for
unparseable input, and `0` is a valid command meaning arrows-off — so a corrupt payload would blank
the sign with no error anywhere. The firmware uses `strtol()` and checks the end pointer, then
re-publishes `state` on every rejection so the divergence is always visible.

---

## 10. MCP23017 write-verify and the health pattern

Every MOSFET write is a **verified** write: write both ports, read both back, compare. On top of
the 10 real bits the firmware ORs a fixed pattern `MCP_HEALTH_PATTERN = 0xA0` into the unused
A5–A7 / B5–B7 bits of both ports and expects it back.

**Why the pattern is necessary.** A dead or disconnected I²C bus reads back `0x00`. That exactly
matches a legitimate "all MOSFETs off" write — so without the pattern, mode `0` could never be
verified, and a node with a severed I²C bus sitting in mode 0 would report perfect health. Forcing
a non-zero expected value into every port in every mode means a silent bus always fails the
comparison.

> **This depends on A5–A7 / B5–B7 being genuinely unconnected.** Anything wired to those pins breaks
> the readback comparison and the node reports a permanent, un-clearable `FAULT`. Bench item **B7**.

A failed readback calls `setMcpHealth(false)`, which publishes `state = FAULT` and stops further
write attempts. `ensureMcpHealthy()` retries the whole I²C bring-up on a 2 s cadence and re-applies
the held output intent when it succeeds, so a node recovers by itself once the fault clears.

---

## 11. Telemetry — what the gateway subscribes to

Publish to `metro/signage/register/<40001+i>/<metric>`, all with **`retain=True`**.
**Publish on change, not on a timer.**

| Metric | Payload | Notes |
| --- | --- | --- |
| `status` | `ONLINE:<ip>` or `OFFLINE` | Gateway splits on the first `:` to extract the IP |
| `power` | `OK` \| `FAIL` | PSU divider on GPIO36 |
| `current1` | **`ON` \| `OFF` only** | LHS arrows — **intent-echo**, see below |
| `current2` | **`ON` \| `OFF` only** | RHS arrows — **intent-echo**, see below |
| `current3` | `ON` \| `FAIL_OPEN` | Static Zone 1 — measured |
| `current4` | `ON` \| `FAIL_OPEN` | Static Zone 2 — measured |
| `battery_pct` | `0` \| `50` \| `100` | Coarse band, §2. Omitted until first sample |
| `state` | integer **or** `FAULT` | The command this node is ACTUALLY executing |

### ⚠️ Arrow health is intent-echo — it is NOT measured

`current1` and `current2` report **what the side was commanded to do**, not what was measured.
They can never emit `FAIL_OPEN` or `FAIL_SHORT`.

Arrow diagnostics **were implemented and then deliberately withdrawn.** The measured values
fluctuated too much to be read reliably while the arrows were animating: a 2-strip chase draws
~0.17 A, about 40 ADC counts on the 5 A ACS712, which sits inside the part's own noise. Measuring
it continuously false-alarmed in both directions. Real arrow health moves to the **Phase-3
on-demand self-test**, where every strip is switched on together and the signal clears the noise
floor by roughly 5×.

**Consequences, which must not be "fixed" downstream:**
- `FAIL_SHORT` is currently **unreachable on every channel**.
- `FAIL_OPEN` originates **only** from the two static zones.
- SCADA diagnostic registers `+2` / `+3` carry only `0` or `1` for the arrows, never `2` or `3`.
  Any alarm keyed on `FAIL_*` for the arrows will never fire.

> ⚠️ **Divergence from `esp32_contract.md` §3 and `SCADA_Integration_Guide.md` §5.1.** Both still
> document the arrows as continuously measured 4-state. They are stale and must be corrected.

### Static zones — the asymmetry that matters

Static zones are **always intended ON**, so their only legal states are `ON` and `FAIL_OPEN`. They
can never legitimately report `OFF` or `FAIL_SHORT`. Their threshold is static
(`THRESHOLD_STATIC = 0.200 A`), not scaled by mode, because their load never changes. If a static
zone reports `OFF`, the fault is in the sensing chain or the calibration — not the load.

### `state` — the acknowledgment topic

Everything else in the system shows what a node was *commanded*. This topic is the only thing that
reports what it is *doing*. Publish it whenever the active command changes, in response to a PING,
and whenever MCP health changes — but **never optimistically**: only after the MOSFET write has
been written *and read back verified*. A write that fails verification does not advance
`activeCommand` at all.

| Cause of divergence | Payload | Meaning |
| --- | --- | --- |
| **Command rejected** (malformed or out of range) | the **previous numeric** value | Node is still faithfully running the old mode and *knows* it |
| **MCP unreachable / readback mismatch** | non-numeric — `FAULT` | Node does **not know** what its outputs are doing |

The gateway maps any non-numeric payload to `65535` and exposes it at diagnostic register `+7`.
`65535` can never equal a valid command, so the fault **cannot be cleared by commanding anything** —
it clears only when the hardware is fixed. That is intended and must not be filtered out at the
SCADA end.

### Gateway encoding

The gateway maps the strings to `OFF=0 / ON=1 / FAIL_OPEN=2 / FAIL_SHORT=3`; anything else becomes
`99`. **Spelling is load-bearing.**

```
                 measured >= threshold    measured < threshold
  intended ON          ON                     FAIL_OPEN
  intended OFF         FAIL_SHORT             OFF
```

### Suppression

The four `current*` topics are **not** published before the first sensor cycle completes
(`haveSensorData == false`). Publishing at boot would assert states no measurement supports. The
previously retained values stay on the broker, so the registers hold their last value rather than
going to a "no data" state. `status`, `power` and `state` publish throughout.

### Last Will and Testament

Connect with LWT on `.../status`, payload `OFFLINE`, **QoS 1, retained** (`v4.1.0.ino:831`). This is
what makes the gateway's offline-scrubbing work when a node drops without a clean disconnect.
Subscriptions: `.../value` at QoS 1, `metro/signage/scan` at QoS 0.

---

## 12. Thresholds and mode-change settle

`setThresholds(nLeft, nRight)` is called on every mode change, **re-arming both sides** — never
just the side that is turning on.

| Mode | Left strips | Right strips |
| --- | --- | --- |
| `0` | 0 | 0 |
| `1` | **2** (chase) | 0 |
| `2` | 0 | **2** (chase) |
| `3` | **2** | **2** |
| `4` | 5 | 0 |
| `5` | 0 | 5 |
| `6` | 5 | 5 |

> ⚠️ **Divergence from `esp32_contract.md` §5**, which scales chase at **3** strips (`× 2.5`). The
> chase is 2-of-5 in this build. Largely dormant while the arrows are intent-echo, but it governs
> arrow evaluation again from Phase 3.

Constants: `THRESH_PER_STRIP = 0.060 A`, `THRESHOLD_STATIC = 0.200 A`,
`THRESH_OFF_DETECT = 0.030 A`, `NOISE_FLOOR = 0.020 A`.

**Ordering constraint: `NOISE_FLOOR` must stay below `THRESH_OFF_DETECT`.** The noise gate snaps
small readings to zero; if it ever sat above the off-threshold it would zero out exactly the
currents `FAIL_SHORT` exists to catch.

**Mode-change settle (`MODE_SETTLE_MS = 1200`).** The IIR filter is deliberately slow, so without
this every off→on change would publish `FAIL_OPEN` for several seconds before clearing itself. On a
mode change the filter is snapped to the instantaneous reading, and state evaluation is suppressed
for 1200 ms while the load settles. `power` still evaluates and publishes during the window.

> **Only mode changes are stamped, not chase frames.** A chase holds a constant number of strips lit
> in every frame, so the load is steady across the animation. Stamping each frame would suppress
> evaluation permanently in modes 1–3, since the 250 ms frame interval is shorter than the window.

---

## 13. Network failsafe — HOLD LAST COMMAND

**There is no dead-man switch and no fail-safe timer.**

- On network loss the node **holds its current command** and keeps animating. The animation state
  machine runs unconditionally from `loop()` — outside the MQTT-connected branch and independent of
  MCP health.
- No forced Solid-ON, no forced OFF.
- `esp_task_wdt` (15 s, panic reboot) is **internal hang recovery only**, not a network watchdog.

### Hold-last-command survives a link outage but NOT a reboot

There is **no NVS persistence** in this build — no `Preferences`, no `nvs_*` calls. It is Phase 2.

| Scenario | Result |
| --- | --- |
| Link drops, node stays powered | Animation keeps running, command held. Works as intended |
| Node reboots, network **up** | Comes up at mode `0`, broker delivers the retained `value`, sign restores within seconds |
| Node reboots **during** a network outage | **The sign comes up dark and stays dark** until the broker is reachable |

**This is bench-acceptable and NOT site-acceptable.** A power blip that takes out the field switch
and the signs together brings the signs back blank, and the control system will report them as
correctly showing mode 0.

**PING** (`metro/signage/scan`, payload `PING`, QoS 0, every 60 s from the gateway): respond by
re-publishing the full telemetry set. That is its entire purpose — it lets the gateway rebuild state
without waiting for a value to change.

---

## 14. PSU failure detection

`power=FAIL` published by an **ONLINE** node is the system's critical alarm, not an edge case. On
PSU loss the node must stay connected and keep publishing; losing the link defeats the purpose of
the backup. All four MP1584 bucks tap the bus post-diode where the battery ties in, so the battery
carries the **whole** load including the three LED rails. LEDs stay lit, the current sensors keep
reading normally, and there is no false `FAIL_OPEN` cascade during an outage.

An **OFFLINE** node is a different fault entirely — network loss, dead board, or an exhausted
battery. It is **not** a PSU failure and must never be reported as one.

### Measurement

`psuV = adcVolts(GPIO36) * PSU_DIV_RATIO`, with hysteresis and debounce:
`PSU_FAIL_VOLTS = 10.0`, `PSU_OK_VOLTS = 11.0`, `PWR_DEBOUNCE_COUNT = 3` (1.5 s at the 500 ms
sensor cadence). Fail below, recover only above, hold in between.

### `PSU_DIV_RATIO = 6.9` is calibrated — do not "correct" it to 6.556

The fitted divider is 100 k / 18 k, nominal ratio **6.556**. The firmware uses **6.9**, obtained by
manual calibration against a multimeter, and that is the correct value to ship.

The two differ because the PSU channel reads through the **uncalibrated** `raw/4095*3.3` path
(`adcVolts()`), which under-reads on the 11 dB attenuation range. 6.9 / 6.556 = 1.052 — a ~5 % ADC
under-read, exactly the expected magnitude. **6.9 is a single combined correction factor doing two
jobs: undoing the divider and correcting the ADC.**

> **Do not switch the PSU channel to `analogReadMilliVolts()`.** That removes the ADC error 6.9
> compensates for, and the node would start over-reading the PSU by ~5 %. If that change is ever
> made, `PSU_DIV_RATIO` must drop to ~6.556 **in the same commit**.

### The measurement is pre-diode, and that matters

R23 taps `/14.5_VD` — post-fuse, **before** the isolation diode. The battery sits on `/MAIN_OP`,
after it. When the PSU dies the diode blocks back-feed and the measured node collapses toward zero,
so detection is clean and robust. Almost any threshold between ~1 V and ~13 V works.

> ⚠️ **Divergence from `esp32_contract.md` §6a**, which assumes the shared bus is measured and calls
> the threshold "the narrowest calibration window in the system". That is based on the wrong
> hardware model. The window is wide.

**Known latency.** The 470 µF bulk cap (C1) sits on `/14.5_VD`, and all four bucks draw from the
other side of the diode — so after a PSU loss the **only** discharge path is the 118 kΩ divider.
That is a ~55 s time constant, giving roughly **20 s before `power=FAIL` is published**. Calculated,
not yet measured. If too slow, the fix is a bleeder across C1 — hardware, not firmware.

---

## 15. Outstanding bench items

| # | Question |
| --- | --- |
| **B1** | Which physical arrow position does each of A0–A4 / B0–B4 drive? Needed to confirm the chase sweep direction |
| **B2** | Confirm the ACS712 markings on the fitted parts (§8 says all four are 5 A) |
| **B3** | Expected load currents — per arrow strip, per static zone |
| **B5** | `THRESH_OFF_DETECT` viability against real idle-channel noise |
| **B6** | `MODE_SETTLE_MS` viability against the real LED load |
| **B7** | Are MCP23017 A5–A7 / B5–B7 genuinely unconnected? (§10 depends on it) |
| **new** | Time how long `power=FAIL` takes to appear after the PSU is cut (§14) |
| **new** | Verify the J15 / J16 socket pin order against the LAN8720 module pinout |
| **new** | Full battery discharge-to-cutoff run to finalize `BATT_FULL_V` / `BATT_LOW_V` (§2.2) |

**B4 (PSU thresholds) is closed** — the window is wide, not narrow; see §14.

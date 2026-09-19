# HANDOFF — ESP32 Signage Firmware (Phase 1)

Machine-readable state capture. Repo: `smart-signage-boards`. Domain: metro emergency
signage — PLC/Modbus ⇄ Raspberry Pi gateway ⇄ MQTT ⇄ ESP32 nodes, all wired Ethernet.
Read `README.md` (root) + `Embedded/ESP32/esp32_contract.md` (authoritative ESP32 spec)
for system context.

## GOAL
Rework ESP32 node firmware in 3 phases. All work + docs in `Embedded/ESP32/Workbench/`.
- Phase 1: hardcoded bench build. Part 1 = strip to essentials, no battery. Part 2 = add battery. (ACTIVE)
- Phase 2 (later): NVS persistence → NVS-backed auto-calibration → OTA over Ethernet → NVS device identity (node id + SW/PCB version).
- Phase 3 (later): scheduled diagnostics + on-demand all-on self-test (the real arrow-health check).

## CURRENT STATUS
- DONE — Phase 1 Part 1 (`v4.0.0-p1`): stripped build, config hardcoded/const, auto-cal removed, battery removed, menu `h/i/s`, 2-LED wrapping chase, arrows intent-echo. Compiles, bench-tested for connectivity.
- DONE — Phase 1 Part 2 (`v4.1.0-p2`): battery gauge merged, watchdog bug fixed, `b` battery stream added. Compiles clean (77% flash). NOT yet flashed/verified on hardware.
- DONE — HMI (`hmi_light.py`/`hmi_dark.py`): battery value now displayed. py_compile clean. Not run on device.
- DONE — Gateway: no code change needed (already passes `battery_pct` through).
- IN PROGRESS / OPEN — battery thresholds are PROVISIONAL (see Decisions). Need a full discharge-to-cutoff run to finalize.
- UNTOUCHED — `esp32_contract.md` + `RaspberryPi/Gateway/SCADA_Integration_Guide.md` still document arrow current channels as 4-state and battery as unpublished; both now stale (see Next Steps).
- UNTOUCHED — PCB V3.1 hardware items (see Gotchas).

## KEY FILES (paths relative to repo root)
- `Embedded/ESP32/Workbench/Firmware/v4.1.0/v4.1.0.ino` — CURRENT firmware (merged, battery). Header `v4.1.0-p2`. Compile this.
- `Embedded/ESP32/Workbench/Firmware/v4.1.0/firmware_doc.md` — v4.1.0 technical reference.
- `Embedded/ESP32/Workbench/Firmware/v4.0.0/v4.0.0.ino` — Part-1 build (`v4.0.0-p1`), the baseline v4.1.0 was built from.
- `Embedded/ESP32/Workbench/Firmware/v4.0.0/firmware_doc.md` — Part-1 reference.
- `Embedded/ESP32/Workbench/battery_calibration/battery_calibration.ino` — standalone serial bench tool (divider cal `d`, mark anchors `e/m/f`, CSV log `c`, drive load `1/2/0`, stream `s`).
- `Embedded/ESP32/Workbench/battery_calibration/Capture_2_14.5V.csv` — 80-min discharge log (see Gotchas: flat, no curve).
- `Embedded/ESP32/Workbench/Docs/Phase1-Part1_Completion-Report.md` — Part-1 completion report.
- `Embedded/ESP32/Workbench/Docs/Phase1-Part2_Battery-Calibration-Report.md` — battery calibration report + Option-B decision.
- `Embedded/RaspberryPi/HMI/HMI_new/hmi_light.py` / `hmi_dark.py` — new HMIs (near-identical light/dark); battery display edited in both.
- `Embedded/RaspberryPi/Gateway/Pi.py` — Modbus↔MQTT bridge (unchanged; `map_battery_to_int` already handles the number).
- `Embedded/ESP32/esp32_contract.md` — AUTHORITATIVE ESP32 contract. Wins on conflicts.
- `Embedded/ESP32/Workbench/Firmware_Test/demo_mode_v2.0.0/CHASE_ANIMATION_SPEC.md` — chase pattern spec.

## DECISIONS & RATIONALE (do not relitigate)
- Battery = **coarse 0/50/100** on `battery_pct`, NOT a fine %. Reason: 4S LFP flat curve + large load sag + ~±0.3 V ADC noise make precise SoC unrecoverable ("Option B"). "50" means "on battery, discharging, not critical", not exactly half.
- Battery thresholds (loaded V, PROVISIONAL): `≥11.5 → 100`, `10.8–11.5 → 50`, `<10.8 → 0`. Hysteresis `±0.15 V`. Heavy IIR (`BATT_ALPHA=0.10`). Divider `BATT_DIV_RATIO=6.556` (100k/18k, per-board, calibrate).
- Battery read with `analogReadMilliVolts()` (factory eFuse ADC cal) + median-51, NOT `raw/4095*3.3` — matters at LFP mV-scale; matches the calibration tool so thresholds transfer.
- No charging-detection branch: on mains the pack sits high → reads 100 naturally; on PSU fail the number follows the pack down. PSU + battery have separate dividers (GPIO36 vs GPIO39).
- Arrows (`current1/2`) = **intent-echo** (`ON`/`OFF` only, never `FAIL_*`). Reason: ~2-strip chase current (~0.17 A) is ~40 ADC counts on the 5A ACS712 — inside its noise. Static zones (`current3/4`) keep measured 4-state (draw ~650 mA each, well-sensed). Real arrow health deferred to Phase-3 self-test.
- Chase = Chase-1 2-LED wrapping, A4→A0, 250 ms, frames `{0x18,0x0C,0x06,0x03,0x11}`. Wrap frame (A4+A0) intentional. `setThresholds(2,…)` for chase modes.
- All config is `const`, hardcoded, per-board; edit-and-reflash. No NVS (Phase 2).
- Battery reported only after first sample; absence = 65535 (unknown) at gateway; never a bare 0 except genuine near-empty.
- Versioning/folder org is USER-OWNED — do NOT rename/move/reorg files or version labels. Flag issues, wait for guidelines.

## CURRENT STATE
- git branch: `Bhoumik_Embedded`. HEAD: `d3d1377 Finished the merged code and reorganized the repository`.
- Working tree CLEAN except untracked `PCB/PCB_V3/PCB_V3.1/.history/` (IDE noise, unrelated). All firmware/HMI/doc edits are committed in `d3d1377`.
- Nothing running. No dev server.
- ENV/toolchain: `arduino-cli` NOT on PATH — use the repo copy `Embedded/bin/arduino-cli`. Core `esp32:esp32` 3.3.8; libs PubSubClient 2.8, Adafruit MCP23017 2.3.2. FQBN `esp32:esp32:esp32`.
- Compile: `Embedded/bin/arduino-cli compile --fqbn esp32:esp32:esp32 Embedded/ESP32/Workbench/Firmware/v4.1.0/v4.1.0.ino`
- Node net config (per-board, top of .ino): node1 `local_IP 192.168.1.101`, `ASSIGNED_REGISTER 40001`; broker (Pi) `192.168.1.10:1883`.
- Battery under test: Pro-range 6 Ah 12.8 V 4S LiFePO4 ("orange pack"), inbuilt BMS (low-cutoff spec unknown, ~10 V expected — determine empirically).
- Serial menu (v4.1.0): `h` help, `i` info(+battery), `s` current-sensor stream, `b` battery stream. 115200 baud.

## NEXT STEPS (ordered)
1. Flash `v4.1.0/v4.1.0.ino` to a node; verify no WDT reboots on an Ethernet flap; verify `battery_pct` reads 100 on mains and steps down on a PSU-off discharge.
2. Calibrate battery per board: run `battery_calibration.ino`, `d` (multimeter → `BATT_DIV_RATIO`); paste into `v4.1.0.ino` + reflash.
3. Run a FULL discharge-to-cutoff (hours, on battery, real ~1.5 A load) → finalize `BATT_FULL_V`/`BATT_LOW_V` from the real curve/knee. Replaces provisional thresholds.
4. Rested-vs-loaded sag test (`0` vs `2` on the tool ÷ ~1.5 A) → confirm undercharge vs lossy series path.
5. Update stale downstream docs: `esp32_contract.md` + `SCADA_Integration_Guide.md` — arrows now 2-state (0/1, never 2/3); battery now published (0/50/100). README note battery is live. (No gateway code change.)
6. Then Phase 2 (NVS) planning.

## GOTCHAS / DEAD ENDS
- WATCHDOG BUG (fixed in v4.1.0): `esp_task_wdt_init()` fails on arduino-esp32 3.x ("TWDT already initialized") → intended 15 s never applied → blocking MQTT/TCP connect tripped the short default → reboot loop, Ethernet dropped. FIX: `esp_task_wdt_reconfigure()` + `idle_core_mask=0` + `client.setSocketTimeout(2)` + `ethClient.setConnectionTimeout(5000)` (setSocketTimeout does NOT bound the TCP connect — that was the remaining hole). Same bug exists in v4.0.0-p1 / original v3.0.0.
- Discharge log `Capture_2_14.5V.csv`: 80 min flat ~11.1–11.9 V, NO downward trend, no knee — pack barely discharged. Cause: sensed V is plateau-minus-load-sag, not low SoC; pack has hours left. So NO V0/V50/V100 extractable from it. A real characterization needs a multi-hour run to actual collapse. Also: first capture attempt used stream (`s`) not CSV (`c`) and only caught ~74 s — use `c`, keep laptop awake (USB selective suspend off), CoolTerm capture running the whole time.
- Precise voltage→% SoC for this LFP is NOT achievable (flat curve + sag + noise). Do not attempt a fine gauge; coarse bands only.
- 12 V LED rail DROPOUT on battery: MP1584 (buck-only) can't hold 12 V from a ~12 V pack; rail sags to ~11.3 V, LEDs dim as pack discharges. HARDWARE issue → PCB V3.1 (buck-boost/SEPIC, or lower rail, or direct drive). Not fixable in firmware.
- Battery UNDERCHARGE: 14.45 V PSU − MBR1035 diode (~0.3 V) = ~14.15 V = 3.54 V/cell, below LFP full-charge (3.6–3.65 V/cell / 14.4–14.6 V). Bare diode ≠ CC/CV charger. Pack never fully charges. PCB V3.1 / charger item.
- Serial: no blocking input in production firmware (blocking readLine would starve the WDT). Calibration is observe-stream + hand-edit + reflash.
- `Embedded/ESP32/Firmware/Frimware_v3.0.0/` is MISLABELED: contains Part-1 (`v4.0.0-p1`) content, not v3.0.0. The `v4.0.0/firmware_doc.md` baseline link points there and is therefore circular. Genuine v3.0.0 is recoverable from git HEAD (`Embedded/ESP32/Firmware/firmware_doc.md`, `Firmware.ino`). User will reorganize; do not fix unprompted.

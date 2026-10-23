# Factual Extraction — Project Handover

Extracted 2026-10-23. Branch `Bhoumik_Embedded`, HEAD `d693441`.
Every fact below was verified against source files, not documentation. Where only documentation
exists, that is stated. Contradictions are flagged inline and collected in §6.

---

## 1. Raspberry Pi Gateway

### 1.1 Entry point

| Item | Value | Source |
| --- | --- | --- |
| Script | `Embedded/RaspberryPi/Gateway/Pi.py` | — |
| Version string | `v2.0.0` | `Pi.py:2`, `:210` |
| Main guard | `if __name__ == '__main__':` | `Pi.py:328` |
| Threads | Modbus server (`daemon=True`), paho network loop (`loop_start()`), bridge loop on main thread | `Pi.py:339`, `:348-353`, `:356` |
| Lines | 363 | — |

### 1.2 Venv creation / activation

**Not present as a script in the repo.** Documented only, identically, in
`Embedded/README.md:182-186`, `Embedded/RaspberryPi/pi_agent.md:20`,
`Embedded/RaspberryPi/Gateway/Gateway_doc.md:213`:

```bash
sudo apt update && sudo apt install mosquitto mosquitto-clients python3-venv python3-tk -y
python3 -m venv --system-site-packages ~/metro-gw/venv
source ~/metro-gw/venv/bin/activate
pip install paho-mqtt==2.1.0 pymodbus==3.11.3
```

`--system-site-packages` is required: `python3-tk` is apt-only and cannot be pip-installed.
PEP 668 is enforced on Debian 13, so a bare `pip3 install` against the system Python is refused.

### 1.3 pip freeze

**No `requirements.txt`, `Pipfile`, `pyproject.toml` or captured `pip freeze` exists in the repo.**
Verified by `find` across the whole tree. Pinned versions are documented only:

```
paho-mqtt==2.1.0
pymodbus==3.11.3
```

Source: `pi_agent.md:26-29`. Imports actually used by `Pi.py:16-23`:
`paho.mqtt.client`, `paho.mqtt.client.CallbackAPIVersion`, `pymodbus.server.StartTcpServer`,
`pymodbus.datastore.{ModbusSequentialDataBlock, ModbusDeviceContext, ModbusServerContext}`.

`ModbusDeviceContext` was renamed from `ModbusSlaveContext` in 3.11.3 (`Pi.py:21`), which is one
reason the pin is load-bearing.

### 1.4 Mosquitto configuration

**No `.conf` file exists in the repo.** Documented in two places, and **they contradict each
other** — see §6.1.

| Source | Path | Content |
| --- | --- | --- |
| `pi_agent.md:379-389` | `/etc/mosquitto/conf.d/metro.conf` | `listener 1883 127.0.0.1`<br>`listener 1883 192.168.1.10`<br>`allow_anonymous true`<br>`persistence true`<br>`persistence_location /var/lib/mosquitto/` |
| `HMI_new/SETUP.md:70-76` | `/etc/mosquitto/conf.d/lan.conf` | `listener 1883 0.0.0.0`<br>`allow_anonymous true` |

Broker version: Mosquitto 2.0.21 (`pi_agent.md:17`, verified via apt per that document).

### 1.5 systemd units

| Unit | Status |
| --- | --- |
| `Embedded/RaspberryPi/Gateway/deploy/metro-gateway.service` | **Present in repo** |
| HMI unit | **Does not exist.** Required by `pi_agent.md:411` and `Embedded/README.md:206`; `HMI_new/SETUP.md:205` states units are intentionally not included yet. See §6.2 |

Contents of `metro-gateway.service`, verbatim fields:

| Field | Value |
| --- | --- |
| `After` / `Wants` | `network-online.target mosquitto.service` |
| `Type` | `simple` |
| `User` / `Group` | `pi` / `pi` |
| `WorkingDirectory` | `/home/pi/smart-signage-boards/Embedded/RaspberryPi/Gateway` |
| `ExecStart` | `/home/pi/metro-gw/venv/bin/python3 Pi.py` |
| `AmbientCapabilities` | `CAP_NET_BIND_SERVICE` |
| `CapabilityBoundingSet` | `CAP_NET_BIND_SERVICE` |
| `Restart` / `RestartSec` | `always` / `5` |
| `Environment` | `PYTHONUNBUFFERED=1` |
| `StandardOutput` / `StandardError` | `null` / `journal` |
| Hardening | `NoNewPrivileges=yes`, `PrivateTmp=yes`, `ProtectSystem=strict`, `ProtectHome=read-only`, `ReadWritePaths=/home/pi/smart-signage-boards/Embedded/RaspberryPi/Gateway`, `ProtectKernelTunables=yes`, `ProtectControlGroups=yes`, `RestrictNamespaces=yes` |
| `WantedBy` | `multi-user.target` |

### 1.6 IPs, ports, interfaces

**Configured in code** (`Pi.py:26-31`):

| Constant | Value |
| --- | --- |
| `MODBUS_PORT` | `502` |
| `MODBUS_BIND_HOST` | `"0.0.0.0"` — wildcard bind, so a SCADA-side IP change needs no restart |
| `MQTT_HOST` | `"127.0.0.1"` |
| `MQTT_PORT` | `1883` |
| `MQTT_TOPIC_PREFIX` | `"metro/signage/register"` |
| `REGISTERS_TO_BRIDGE` | `100` |

**Documented network config** (`pi_agent.md:351-373`) — not a file in the repo:

| Link | Target | Port |
| --- | --- | --- |
| SCADA / PLC → Gateway | `10.45.2.50` | 502 |
| ESP32 → Gateway | `192.168.1.10` | 1883 |
| HMI → Gateway | `127.0.0.1` | 502 + 1883 |

Single `eth0` NIC carrying both addresses. NetworkManager profile `metro-gw`:
`ipv4.method: manual`, `ipv4.addresses: 10.45.2.50/24, 192.168.1.10/24`, no gateway,
`ipv4.never-default: yes`, `ipv6.method: disabled`. Isolation depends on
`net.ipv4.ip_forward = 0`.

### 1.7 Modbus register map — as implemented in code

Datastore: `ModbusSequentialDataBlock(0, [0] * 6000)`, `ModbusServerContext(devices=store,
single=True)` (`Pi.py:331-332`). `single=True` means every unit ID maps to the same space.

Base offsets (`Pi.py:34-40`):

| Constant | Index | PLC register | Zone | Access | Written by |
| --- | --- | --- | --- | --- | --- |
| `ADDR_ACTIVE` | `0` | `40001`–`40100` | Active execution zone (MUX output → MQTT) | read | Gateway |
| `ADDR_HMI` | `1000` | `41001`–`41100` | HMI manual buffer | read/write | HMI |
| `ADDR_SCADA` | `2000` | `42001`–`42100` | SCADA auto buffer | read/write | PLC |
| `ADDR_MUX_FLAG` | `3000` | `43001` | MUX control flag | read/write | HMI or PLC |
| `ADDR_DIAG` | `5000` | `45001`–`45800` | Diagnostic block | read only | Gateway |
| `HR_SIZE` | `6000` | — | Datastore size; diag ends at index 5799 | — | — |

**There is no register zone at `44001`.** Index 4000–4999 is allocated but unused.

MUX semantics (`Pi.py:284-289`): `43001 == 1` → copy `41001+` into `40001+`; anything else →
copy `42001+`. Evaluated once per second.

Command value encoding, published verbatim to MQTT (`Pi.py:298-303`):

| Value | Meaning |
| --- | --- |
| `0` | Arrows OFF |
| `1` | LHS chase |
| `2` | RHS chase |
| `3` | Both chase |
| `4` | Solid ON left |
| `5` | Solid ON right |
| `6` | Solid ON all |
| `10000`–`11023` | Raw 10-bit MOSFET bitmask, `value − 10000` |

Diagnostic block — 8 registers per node, node N base `45001 + (N−1) × 8`
(`Pi.py:186-204`, `DIAG_REGS_PER_NODE = 8`):

| Offset | Content | Encoding | Code |
| --- | --- | --- | --- |
| `+0` | Network status | `1` online / `0` offline | `tele["status"].startswith("ONLINE:")` |
| `+1` | Main power | `1` OK / `0` fail | `tele["power"] == "OK"` |
| `+2` | LHS arrows health | 4-state int | `map_discrepancy_to_int` |
| `+3` | RHS arrows health | 4-state int | `map_discrepancy_to_int` |
| `+4` | Battery | `0`/`50`/`100`, or `65535` | `map_battery_to_int` |
| `+5` | Static Zone 1 health | 4-state int | `map_discrepancy_to_int` |
| `+6` | Static Zone 2 health | 4-state int | `map_discrepancy_to_int` |
| `+7` | Actual executing mode | int `0`–`65534`, else `65535` | `map_state_to_int` |

4-state integer encoding (`Pi.py:82-87`): `OFF`→`0`, `ON`→`1`, `FAIL_OPEN`→`2`,
`FAIL_SHORT`→`3`, anything else →`99`.

`map_battery_to_int` (`Pi.py:90-95`): `int(payload)`, or `65535` on `TypeError`/`ValueError`.
`map_state_to_int` (`Pi.py:98-108`): `int(payload)` if `0 <= v <= 65534`, else `65535`.

Addressing: wire address = PLC register − 40001. Gateway writes and SCADA reads both pass through
the same `ModbusDeviceContext`, so any internal offset cancels. Stated verified against a live
server at `SCADA_Integration_Guide.md:84-90`.

Write cadence: `write_diagnostic_block` is called for all 100 nodes every second,
**unconditionally**, outside the `isatty()` presentation branch (`Pi.py:306-309`). `Pi.py:191-192`
records that in v1.0.1 this sat inside the TTY branch, so under systemd the PLC read zeros forever.

Startup: `perform_startup_cleanup` (`Pi.py:153-168`) publishes retained `OFFLINE` on `.../status`
and retained `"---"` on all seven metric topics for registers 40001–40100, then publishes one
`PING` and sleeps 2 s.

PING cadence: `metro/signage/scan` = `PING`, `retain=False`, every 60 s (`Pi.py:278-281`).

---

## 2. ESP32 Firmware

### 2.1 Current version

| Item | Value | Source |
| --- | --- | --- |
| Version | **v4.1.0-p2** | `Workbench/Firmware/v4.1.0/v4.1.0.ino:4` (header), `:634` (`printMenu`) |
| File | `Embedded/ESP32/Workbench/Firmware/v4.1.0/v4.1.0.ino`, 846 lines | — |
| Toolchain | `esp32:esp32` core 3.3.8, FQBN `esp32:esp32:esp32` | `HANDOFF.md` |
| Libraries | PubSubClient 2.8, Adafruit MCP23017 2.3.2 | `HANDOFF.md` |
| Build size | 1 014 332 B flash (77 %), 49 296 B RAM (15 %) | `v4.1.0/firmware_doc.md` |
| Compiler availability | `Embedded/bin/arduino-cli` is a **Mach-O arm64 (macOS)** binary — cannot run on Windows | `file(1)` |

### 2.2 MQTT topics

Topic strings built at `v4.1.0.ino:732-740`, all as
`metro/signage/register/<ASSIGNED_REGISTER>/<suffix>`.

**Subscribed** (`v4.1.0.ino:832-833`):

| Topic | QoS | Payload |
| --- | --- | --- |
| `metro/signage/register/<reg>/value` | 1 | Decimal integer ASCII. Valid: `0`–`6`, `10000`–`11023` (`isValidCommand`, `:385-389`). Parsed with `strtol` + end-pointer check (`:404-410`) |
| `metro/signage/scan` | 0 | `PING` → re-publishes full telemetry set |

**Published** (`v4.1.0.ino:353-376`), all `retain=true`:

| Topic | Payload | Notes |
| --- | --- | --- |
| `.../status` | `ONLINE:<ip>` | `snprintf("ONLINE:%s", ETH.localIP())` |
| `.../power` | `OK` \| `FAIL` | — |
| `.../current1` | **`ON` \| `OFF`** | Intent-echo, `:603`. Never `FAIL_*` |
| `.../current2` | **`ON` \| `OFF`** | Intent-echo, `:604`. Never `FAIL_*` |
| `.../current3` | `ON` \| `FAIL_OPEN` | Measured, `:614` |
| `.../current4` | `ON` \| `FAIL_OPEN` | Measured, `:615` |
| `.../battery_pct` | `0` \| `50` \| `100` | Only when `battery_pct >= 0` (`:369`) |
| `.../state` | integer, or `FAULT` | `FAULT` when `!mcpHealthy` (`:334-340`) |

The four `current*` and `battery_pct` topics are gated behind `haveSensorData` (`:360`).

**LWT** (`v4.1.0.ino:831`): `client.connect(clientId, topic_status, 1, true, "OFFLINE")` —
topic `.../status`, QoS 1, retained, payload `OFFLINE`.
Client ID: `ESP32-<ETH.macAddress()>` (`:828`). Reconnect attempt every 5000 ms (`:826`).

### 2.3 FreeRTOS tasks and core pinning

| Task | Core | Priority | Stack | Created at |
| --- | --- | --- | --- | --- |
| `SensorTask` | **0** | 1 | 10000 | `v4.1.0.ino:800` |
| `loopTask` (Arduino) | 1 (core default) | — | — | implicit |

IPC: `sensorQueue = xQueueCreate(1, sizeof(NodeStateMsg))` (`:777`), written with
`xQueueOverwrite` (`:624`), read non-blocking with `xQueueReceive(..., 0)` (`:841`).
`SensorTask` yields with `vTaskDelay(pdMS_TO_TICKS(10))` (`:626`).
`volatile bool haveSensorData` is written on core 0 and read on core 1 (`:204`).

### 2.4 Watchdog configuration

`v4.1.0.ino:789-797`:

```c
esp_task_wdt_config_t wdt_config = {
  .timeout_ms = 15000,
  .idle_core_mask = 0,
  .trigger_panic = true
};
if (esp_task_wdt_init(&wdt_config) == ESP_ERR_INVALID_STATE) {
  esp_task_wdt_reconfigure(&wdt_config);
}
esp_task_wdt_add(NULL);
```

`SensorTask` also registers itself (`:543`) and resets (`:554`). Supporting timeouts:
`client.setSocketTimeout(2)` and `ethClient.setConnectionTimeout(5000)` — the latter bounds the
TCP connect, which `setSocketTimeout` does not.

### 2.5 Thresholds and calibration constants

| Constant | Value | Line |
| --- | --- | --- |
| `SENS_LEFT` / `ZERO_LEFT` | `0.185f` / `2.20f` | `:72` |
| `SENS_RGHT` / `ZERO_RGHT` | `0.185f` / `2.450f` | `:73` |
| `SENS_STA1` / `ZERO_STA1` | `0.185f` / `2.437f` | `:74` |
| `SENS_STA2` / `ZERO_STA2` | `0.185f` / `2.28f` | `:75` |
| `PSU_DIV_RATIO` | `6.9f` | `:78` |
| `PSU_FAIL_VOLTS` | `10.0f` | `:86` |
| `PSU_OK_VOLTS` | `11.0f` | `:87` |
| `PWR_DEBOUNCE_COUNT` | `3` | `:88` |
| `BATT_DIV_RATIO` | `6.556f` | `:109` |
| `BATT_FULL_V` | `11.5f` | `:110` |
| `BATT_LOW_V` | `10.8f` | `:111` |
| `BATT_HYST_V` | `0.15f` | `:112` |
| `BATT_ALPHA` | `0.10f` | `:113` |
| `THRESH_PER_STRIP` | `0.060f` | `:118` |
| `THRESHOLD_STATIC` | `0.200f` | `:119` |
| `THRESH_OFF_DETECT` | `0.030f` | `:126` |
| `NOISE_FLOOR` | `0.020f` | `:127` |
| `ADC_REF` | `3.3f` | `:129` |
| `ALPHA` | `0.15f` | `:130` |
| `MODE_SETTLE_MS` | `1200` | `:136` |
| `MCP_HEALTH_PATTERN` | `0xA0` | `:166` |
| `chaseFrames[5]` | `{0x18, 0x0C, 0x06, 0x03, 0x11}` | `:211` |
| `numFrames` / `FRAME_MS` | `5` / `250` | `:212`, `:213` |

Per-unit config (`:56-62`): `mqtt_server_ip "192.168.1.10"`, `mqtt_port 1883`,
`ASSIGNED_REGISTER 40001`, `local_IP 192.168.1.101`, `gateway_ip 192.168.1.1`,
`subnet 255.255.255.0`, `primaryDNS 8.8.8.8`.

ADC setup (`:726-727`): `analogReadResolution(12)`, `analogSetAttenuation(ADC_11db)`.

### 2.6 TODO / FIXME comments

**None.** `grep -niE "\b(TODO|FIXME|XXX|HACK|WORKAROUND)\b"` across all `.ino` and `.py` files
under `Embedded/` (excluding `Version History/`) returns zero matches. Outstanding work is tracked
in prose in `HANDOFF.md`, `DOC_UPDATES_PENDING.md` and `v4.1.0/firmware_doc.md` §15 instead.

---

## 3. HMI

### 3.1 Entry points

Three Tkinter applications exist. All three end with
`if __name__ == "__main__": app = HMIApp(); app.mainloop()`.

| File | Lines | Classes | Status |
| --- | --- | --- | --- |
| `Embedded/RaspberryPi/HMI/HMI.py` | 1012 | 5 | Older build. Superseded |
| `Embedded/RaspberryPi/HMI/HMI_new/hmi_light.py` | 1451 | 8 | Current, light palette |
| `Embedded/RaspberryPi/HMI/HMI_new/hmi_dark.py` | 1450 | 8 | Current, dark palette |

### 3.2 Dependencies

Imports (`hmi_light.py:19-29`, `HMI.py:14-24`): `logging`, `sys`, `threading`, `tkinter`,
`tkinter.font`, `paho.mqtt.client`, `pymodbus.client.ModbusTcpClient`.

Runtime requirements, per `HMI_new/SETUP.md:37-44`: `paho-mqtt==2.1.0`, `pymodbus==3.11.3` (pip,
in venv), `python3-tk` (apt, system). Dependency check given there:

```bash
~/metro-gw/venv/bin/python -c "import tkinter, paho.mqtt.client, pymodbus; print('deps OK')"
```

Connection constants, identical in all three files: `MODBUS_IP = '127.0.0.1'`,
`MODBUS_PORT = 502`, `MQTT_BROKER = '127.0.0.1'`, `MQTT_PORT = 1883`, `MAX_NODES = 100`,
`SCREEN_W, SCREEN_H = 1280, 800`.

### 3.3 How it is launched

Documented only (`HMI_new/SETUP.md:120-128`):

```bash
~/metro-gw/venv/bin/python ~/metro-gw/hmi_dark.py
```

`--demo` seeds a mock fleet: `DEMO_MODE = "--demo" in sys.argv` (`hmi_light.py:50`,
`hmi_dark.py:50`). `HMI.py` has **no** `--demo` flag.

Fullscreen is **commented out** in all three files and must be enabled by hand:
`# self.attributes('-fullscreen', True)` — `HMI.py:271`, `hmi_light.py:364`, `hmi_dark.py:364`.
The new builds additionally bind `<Escape>` to leave fullscreen (`:365`).

### 3.4 Autostart files

**None exist.** No `.desktop` file, no HMI systemd unit, no autostart entry anywhere in the repo
(verified by `find` for `*.desktop` and `*.service` — the only hit is `metro-gateway.service`).

`HMI_new/SETUP.md:205` states this is deliberate: *"A start-all script / systemd units are
intentionally not included yet — they depend on the final Pi repository layout, which is still to
be decided."* This contradicts `pi_agent.md:411` and `Embedded/README.md:206`, which both require
the HMI to run as its own systemd unit. See §6.2.

---

## 4. Repository tree — two levels

| Path | Purpose |
| --- | --- |
| `Data Sheets/` | Vendor PDFs: ACS712, AMS1117-3.3, ESP32 DevKit V1 pinout, MCP23017, MP1584 |
| `docs/handover/` | Handover deliverables produced in this review (conflict resolutions, this file) |
| `Embedded/` | All firmware, gateway, HMI source and documentation |
| `Embedded/.claude/` | Local tool settings (`settings.local.json`) |
| `Embedded/bin/` | Vendored `arduino-cli` (**macOS arm64 binary**) |
| `Embedded/Builds/` | Compiled artefact archive (`Build_v1.0.0.zip`) |
| `Embedded/Docs/` | **Empty.** Contains only empty `MODBUS_Map/` and `WiringDiagram/` |
| `Embedded/Hardware/` | **Empty.** Contains only empty `BillOfMaterials/` and `PCBLayout/` |
| `Embedded/ESP32/` | Node firmware, contract, pending-docs checklist, workbench |
| `Embedded/RaspberryPi/` | Gateway, HMI, platform contract |
| `PCB/` | All KiCad design data |
| `PCB/Open Source Files/` | Third-party reference designs: AMS1117, MP1584 buck, ACS712, Olimex ESP32-PoE, 5 V relay |
| `PCB/Self Design Modules/` | In-house module designs: ACS712, AMS1117 |
| `PCB/PCB_V1_Plug_and_Play_Board/` | V1 board (GPB1/GPB2), superseded |
| `PCB/PCB_V2/` | V2 board, superseded |
| `PCB/PCB_V3/` | V3 board — `PCB_V3_Final/` (fabricated), `PCB_V3.1/` (in progress), `PCB_V3_JTAG/`, `Connections.md`, 3D models |

> **`Embedded/Hardware/BillOfMaterials/` is empty.** The BOM referenced throughout the
> knowledge-base conflicts (C1, C2, C5) does not exist in this repository.

---

## 5. Git history

No release tags. One tag exists, `Test`, unrelated to versioning.
Branches: `Bhoumik_Embedded` (current), `main`, `default`, `temp_bhoumik`,
`BhoumikAcharya-patch-1-Remote_Local`, plus remotes on `origin` and `BhoumikAcharya`.

### 5.1 `Embedded/RaspberryPi/Gateway`

```
8e66a48 Updated the Gateway and the HMI
38ce6c9 CODE UPDATE -- Pi Gateway script update with respect to the new architecture
44b48ca Added the Pi_agent document and created version control
96460e4 Updating the updated code for the ESP32 and the Raspberrypi
0611b28 Created version history
60d78aa Added version history for the ESP32 and the Raspberrypi codes along with the documentation in .md files
66b3491 Added Battery dict and displaying battery status
0d0224d Reconstruction of the Repo for Embedded Files
```

### 5.2 `Embedded/ESP32` (most recent 14)

```
b7f4697 The node details error has been fixed
0db8f83 Troubleshot the not rendering signage list page
6ec4274 Created handoff doc for coding agent
d3d1377 Finished the merged code and reorganized the repository
133acae Adding another capture file
546bcaa Updating documentation - Part 1
905657e Addind the CSV file for battery calibration with psu at 14.5V
91df590 Updated the battery_calibration code
9c8db4d Created a pcb 3.1 file
9de83f6 uploading the battery calibration code
1f8a66a Merge branch 'Bhoumik_Embedded' ...
25c7e6d Changed the firmware - The 4 state descripency for the moving arrows, removed
c62ee96 Updated the values for the left right arrows
7053c38 Updated the firmware doc.
```

### 5.3 `Embedded/RaspberryPi/HMI` (most recent 14)

```
b7f4697 The node details error has been fixed
0db8f83 Troubleshot the not rendering signage list page
d3d1377 Finished the merged code and reorganized the repository
b5d97eb Adding the first set of HMI drafts and documentation
00454c2 Updated the design for the HMI
594916e Updating the documentation.
8e66a48 Updated the Gateway and the HMI
38ce6c9 CODE UPDATE -- Pi Gateway script update with respect to the new architecture
44b48ca Added the Pi_agent document and created version control
e21aabf Created a repo for version history of the HMI and added V1.zip
815239f had pasted an empty file, changed that to the required file
416cb99 Adding the HMI documentation to teh HMI folder
00816b1 1. Fixed the sync error of the mode(SCADA/HMI) ...
967e11e 1.removed the dummy information, and changed the hmi data to real time data ...
```

### 5.4 `PCB` (most recent 14)

```
905657e Addind the CSV file for battery calibration with psu at 14.5V
91df590 Updated the battery_calibration code
9c8db4d Created a pcb 3.1 file
317d9d0 Firmware v4.0.0-p1
a4f76be Edited the PCBs and changes the structure of the rep
594916e Updating the documentation.
c04422b Commiting the LED test code.
4fa8299 ESP32 Code update
268c3e2 Updated the PCB
3c38ac4 Merge branch 'BhoumikAcharya-patch-1-Remote_Local' ...
19bbf2d Final PCB Upload
44b48ca Added the Pi_agent document and created version control
054f264 Created and finished coding the firmware_test.ino file
682237c Uploading the final PCB files
```

### 5.5 Major changes between firmware versions

| Version | Commit | Key changes (verified in source) |
| --- | --- | --- |
| v1.1.1 | — | Relay-era. `ETH.begin` already LAN8720/GPIO17. Malformed payloads logged, not reported |
| v2.0.0 | — | `BATT_R1/R2 = 42000/10000`. I²C on GPIO 16/17 (collided with the RMII clock) |
| v3.0.0 | `4fa8299` | I²C moved to 4/13. 4th ACS712 + `current4`. Modes 4/5/6 split. `state` topic with verified writes. `strtol` parsing. `PSU_DIV_RATIO = BATT_DIV_RATIO = 5.30f`. Arrows measured 4-state. Auto-cal menu `h i r s c g v m p` |
| v4.0.0-p1 | `317d9d0` | Auto-calibration **removed**; battery **removed**; all config `const`. Menu cut to `h i s`. Chase changed to `{0x18,0x0C,0x06,0x03,0x11}` @ 250 ms, 2-of-5. **Arrows changed to intent-echo.** `PSU_DIV_RATIO = 6.9f` |
| v4.1.0-p2 | `d3d1377` | Coarse battery gauge (`0/50/100`, `BATT_DIV_RATIO = 6.556f`, 100 k/18 k on GPIO39). Watchdog init fixed (`esp_task_wdt_reconfigure`, `idle_core_mask = 0`, socket + connect timeouts). Menu `h i s b` |

Gateway: v1.0.1 → v2.0.0 moved the diagnostic-block write out of the `isatty()` branch, replaced
`break` with `continue` in the bridge loop's exception handler, and adopted
`CallbackAPIVersion.VERSION2` (`Pi.py:191-192`, `:321-325`, `:334`).

---

## 6. Self-contradictions found in the code and configuration

### 6.1 Mosquitto bind address — security-relevant

| Source | Says |
| --- | --- |
| `pi_agent.md:379-392` | `/etc/mosquitto/conf.d/metro.conf`, two explicit listeners (`127.0.0.1`, `192.168.1.10`). *"The broker is deliberately NOT bound to `0.0.0.0`… Verify with `ss -tlnp \| grep 1883` — you must **not** see `0.0.0.0:1883`."* |
| `HMI_new/SETUP.md:70-76` | `/etc/mosquitto/conf.d/lan.conf`, `listener 1883 0.0.0.0` |

Different filenames, so both drop-ins could be installed simultaneously and the `0.0.0.0` listener
would win — exposing MQTT to the SCADA segment, which `pi_agent.md` explicitly designs against.
Neither file exists in the repo, so whichever was applied on the Pi is unverifiable from here.

### 6.2 HMI systemd unit — required but absent

`pi_agent.md:411` and `Embedded/README.md:206` both require the HMI to run as a separate systemd
unit. `HMI_new/SETUP.md:205` says units are intentionally not provided. No HMI unit exists.

### 6.3 Deployment path

| Source | Gateway path |
| --- | --- |
| `deploy/metro-gateway.service` | `/home/pi/smart-signage-boards/Embedded/RaspberryPi/Gateway/Pi.py` |
| `HMI_new/SETUP.md:117` | `~/metro-gw/Gateway/Pi.py` |

Two different install layouts. The venv is at `~/metro-gw/venv` in both.

### 6.4 Node_3 duplicates Node_2's identity — functional bug

```
Node_1.ino:59  ASSIGNED_REGISTER = 40001   :61  local_IP 192.168.1.101
Node_2.ino:59  ASSIGNED_REGISTER = 40002   :61  local_IP 192.168.1.102
Node_3.ino:59  ASSIGNED_REGISTER = 40002   :61  local_IP 192.168.1.102   <-- same as Node_2
```

`diff Node_1.ino Node_3.ino` differs **only** in those two lines; `diff Node_2.ino Node_3.ino`
differs only in `ZERO_RGHT` and `ZERO_STA1`. Node_3 is a copy of Node_1 with Node_2's identity
pasted in.

**Consequences if both are flashed:** IP address conflict on `192.168.1.0/24`; both nodes subscribe
to `metro/signage/register/40002/value` and act on the same command; both publish retained
telemetry to the same topics, so the gateway and HMI see one node flapping between two boards'
readings; node 3 never appears.

### 6.5 `Frimware_v3.0.0/` folder contents do not match its name

`Embedded/ESP32/Firmware/Frimware_v3.0.0/Firmware_v3.0.0/Firmware_v3.0.0.ino:4` reads
`Firmware v4.0.0-p1`. `Frimware_v3.0.0/firmware_doc.md:1` is titled `v4.0.0-p1` and duplicates
`Workbench/Firmware/v4.0.0/firmware_doc.md`. Genuine v3.0.0 exists only at git `4fa8299`.
Folder name is also misspelled. Flagged in-file; not renamed (owner-owned per `HANDOFF.md`).

### 6.6 Documentation vs code divergences

Resolved and recorded in `docs/handover/kb-conflict-resolutions.md` (items M1–M9). Still live in
`SCADA_Integration_Guide.md` §5.1 at the time of writing: arrow health documented as measured
4-state when it is intent-echo.

---

## Appendix A — Additional questions

### A1. Diagnostic LED on the 14.5 V rail

**There is no diagnostic LED on the board.** No LED component exists in either revision.

Complete component list, parsed from `PCB_V3_Final.kicad_sch` and `PCB_V3.1.kicad_sch`:
`C1`, `D1`, `F1`, `H1`–`H4`, `J1`–`J41` (`J42` in V3.1), `Q1`–`Q10`, `R1`–`R26`, `TP1`–`TP4`.
Footprint reference prefixes in both `.kicad_pcb` files: `C D F H J Q R TP`. **No `D` other than
`D1` (Schottky), and no LED footprint.**

Every string containing "LED" in the schematic is a net or connector name for the **external** LED
strips: `LHS_LED_1`–`5`, `RHS_LED_1`–`5`, `LHS_LED_IP+`, `RHS_LED_IP+`, `STATIC_LED_1`,
`STATIC_LED_2`, and the sheet note `"LED Screw Terminals"`.

**`power=FAIL` delay is therefore unchanged.** Discharge path for `C1` after PSU loss:

| Quantity | Value | Source |
| --- | --- | --- |
| `C1` | 470 µF | Silkscreen `470u` at (104.50, 66.00); `Firmware_Test/README.md:42` |
| Discharge path | R23 + R24 = 100 kΩ + 18 kΩ = **118 kΩ** | Silkscreen `100k`/`18k`; only components on `/14.5_VD` besides `D1` anode and `F1` |
| Other loads on `/14.5_VD` | **None** — all four bucks draw from `/MAIN_OP`, the far side of `D1` | Pad-net table |
| τ = RC | 118 000 × 470 × 10⁻⁶ = **55.5 s** | — |
| Trip point | `PSU_FAIL_VOLTS = 10.0 V` | `v4.1.0.ino:86` |
| t = τ · ln(14.5 / 10.0) | 55.5 × 0.3716 = **20.6 s** | — |
| Debounce | 3 × 500 ms = 1.5 s | `PWR_DEBOUNCE_COUNT = 3` |
| **Total** | **≈ 22 s** | — |

Calculated, not measured. A PSU with its own output bleeder will be faster; a blown fuse or
severed input gives the full figure. *(For reference, had an LED + 2.2 kΩ been fitted on
`/14.5_VD` it would have drawn ~5.7 mA and dominated the discharge, cutting this to under
2 s — but no such part exists.)*

### A2. ACS712 outputs → GPIO34/35/32/33

**No divider, no series resistor, no clamp. Direct connection.**

Each ACS output net carries exactly two pads:

| Net | Pads |
| --- | --- |
| `/D34` | `J3` (ACS1_Pins) pin 2 → `J1` (ESP_Left) pin 4 |
| `/D35` | `J7` (ACS2_pins) pin 2 → `J1` pin 5 |
| `/D32` | `J4` (ACS3_Pins) pin 2 → `J1` pin 6 |
| `/D33` | `J9` (ACS4_Pins) pin 2 → `J1` pin 7 |

The only resistors on the board are R1–R20 (MOSFET gate series + pull-down), R21/R22 (I²C
pull-ups) and R23–R26 (the two voltage dividers). None touches an ACS output net.

**Sensor supply is 5 V:** `J3/J4/J7/J9` pin 3 = `/5V`, the BC4 output. So each ACS712 output
swings 0–5 V with a nominal quiescent of VCC/2 = 2.5 V, wired straight to a 3.3 V ADC pin.

**The firmware assumes no divider.** `readCurrent()` (`v4.1.0.ino:288-290`) is
`fabs((adcVolts(pin) − zero) / sens)` with `adcVolts()` = `raw/4095 × 3.3` — the pin voltage is
treated as the raw sensor output, with no scale factor. The zero points
(`2.20`, `2.450`, `2.437`, `2.28` V) are consistent with an un-divided 5 V-powered ACS712 sitting
near 2.5 V. A divider would have put them near 1.2–1.6 V.

**Current at which the pin exceeds 3.3 V**, from `V = ZERO + I × 0.185`:

| Channel | `ZERO` | I at 3.3 V |
| --- | --- | --- |
| LHS arrows (GPIO34) | 2.20 V | **5.95 A** |
| RHS arrows (GPIO35) | 2.450 V | **4.59 A** |
| Static Zone 1 (GPIO32) | 2.437 V | **4.66 A** |
| Static Zone 2 (GPIO33) | 2.28 V | **5.51 A** |

At the nominal 2.5 V quiescent the figure is 4.32 A.

Two observations that follow from the direct connection:

1. **Headroom is set by the ACS712's own ±5 A range, not by the ESP32.** The parts saturate at
   about the same current that would over-drive the pin, so normal operation (arrows ~0.17 A,
   static zones ~0.65 A) stays far below. A hard short on a 12 V rail is the case that would
   exceed it.
2. **The quiescent point sits at the top of the ESP32's usable ADC range.** `ADC_11db` is
   non-linear above roughly 2.5 V, and every reading is referenced to a zero point of 2.2–2.45 V.
   This degrades small-signal accuracy on all four channels and is consistent with the measured
   noise that caused arrow diagnostics to be deferred to Phase 3.

### A3. Isolation diode D1 and fuse F1

**D1:**

| Source | Value |
| --- | --- |
| `PCB_V3_Final.kicad_sch`, `PCB_V3.1.kicad_sch` | Value field = `D_Schottky` (generic symbol). Footprint `Package_TO_SOT_THT:TO-220-2_Vertical`. **No part number** |
| `Workbench/HANDOFF.md:72` | `MBR1035` |
| `Workbench/Docs/Phase1-Part2_Battery-Calibration-Report.md:83`, `:122` | `MBR1035` (forward drop ~0.3 V) |
| `v4.1.0/firmware_doc.md:128` | `MBR1035` |

**`MBR1035` is the only part number in the repository.** `MBR1545` and `15SQ045` appear **nowhere**
— no match in any `.md`, `.ino`, `.py`, `.kicad_sch` or `.kicad_pcb` file. If the knowledge base
names either, it is unsupported here.

Note: MBR1035 is a TO-220-2 10 A / 35 V Schottky, consistent with the footprint.

**F1:**

| Source | Value |
| --- | --- |
| `PCB_V3_Final.kicad_sch`, `PCB_V3.1.kicad_sch` | Value field = `Fuse` (generic). Footprint `Fuse:Fuseholder_BF020_21.3mmm` (a 5×20 mm holder) |
| Silkscreen | `Fuse_Holder` |
| `Firmware_Test/README.md:42` | "Fuse Holder" — no rating |

**F1's current rating is not recorded anywhere in the repository.** No value, no BOM entry
(`Embedded/Hardware/BillOfMaterials/` is empty), no mention in any document. **Must be obtained
from the physical build.**

### A4. `NodeStateMsg` — verbatim, `v4.1.0.ino:195-202`

```c
struct NodeStateMsg {
  bool power_ok;
  const char* state_left;
  const char* state_right;
  const char* state_sta1;
  const char* state_sta2;
  int battery_pct;          // coarse 0/50/100; -1 = not sampled yet (=> not published)
};
```

Associated declarations (`:203-204`):

```c
QueueHandle_t sensorQueue;
NodeStateMsg networkState = {true, "OFF", "OFF", "OFF", "OFF", -1};
volatile bool haveSensorData = false;  // written on core 0, read on core 1
```

### A5. Which HMI is production

**Undetermined by the repository.** There is no autostart file, no systemd unit and no `.desktop`
entry, so nothing in the repo launches any HMI. The only launch instruction is documentation:
`HMI_new/SETUP.md:127` runs **`hmi_dark.py`**.

Supporting evidence that `HMI_new` supersedes `HMI.py`: `HANDOFF.md` lists
`HMI_new/hmi_light.py` / `hmi_dark.py` under KEY FILES as the HMIs that received the battery work,
and describes `HMI.py` nowhere as current.

**Functional differences:**

| | `HMI.py` | `hmi_light.py` | `hmi_dark.py` |
| --- | --- | --- | --- |
| Lines / classes | 1012 / 5 | 1451 / 8 | 1450 / 8 |
| Screens | `DashboardFrame`, `NodeDetailFrame`, `DiagnosticsFrame` | `FleetOverviewFrame`, `SignageListFrame`, `NodeDetailFrame`, `DiagnosticsFrame` | same as light |
| Per-LED bitmask control | **absent** | `PerLedDialog` (`:1185`) | present |
| Fleet health roll-up / alarm list | **absent** | `node_health`, `counts`, `build_alarms` (`:149-205`) | present |
| `--demo` flag | **absent** | `DEMO_MODE = "--demo" in sys.argv` (`:50`) | present |
| Battery display | **hardcoded `N/A` / `NOT FITTED`** (`:641-647`) — will not show the live value | live `100`/`50`/`0`, colour-coded (`:1155-1165`) | same as light |
| 4-state colour handling | `four_state_color` (`:118`) — correct | `four_state_health` (`:138`) — correct | correct |
| Static Zone 1/2 rows | present (`:620-621`) | present (`SENSOR_LABELS`, `:120`) | present |
| Escape to exit fullscreen | absent | `:365` | `:365` |
| Difference between light and dark | — | **Palette only.** A diff with hex colours stripped shows only the header text and the `# DESIGN SYSTEM` comment differing | — |

**`HMI.py` will not display battery state.** It predates the feature and hardcodes `N/A` /
`NOT FITTED`. If it is still deployed anywhere, that is a functional gap.

### A6. Uncommitted changes from the conflict-resolution session

```
 M Embedded/ESP32/Firmware/Frimware_v3.0.0/Firmware_v3.0.0/Firmware_v3.0.0.ino
 M Embedded/ESP32/Firmware/Frimware_v3.0.0/firmware_doc.md
 M Embedded/ESP32/Workbench/Docs/Phase1-Part1_Completion-Report.md
 M Embedded/ESP32/Workbench/Firmware/Nodes_Testing/Node_1/Node_1.ino
 M Embedded/ESP32/Workbench/Firmware/Nodes_Testing/Node_2/Node_2.ino
 M Embedded/ESP32/Workbench/Firmware/Nodes_Testing/Node_3/Node_3.ino
 M Embedded/ESP32/Workbench/Firmware/v4.0.0/firmware_doc.md
 M Embedded/ESP32/Workbench/Firmware/v4.0.0/v4.0.0.ino
 M Embedded/ESP32/Workbench/Firmware/v4.1.0/firmware_doc.md
 M Embedded/ESP32/Workbench/Firmware/v4.1.0/v4.1.0.ino
 M Embedded/ESP32/Workbench/HANDOFF.md
 M Embedded/ESP32/esp32_contract.md
 M Embedded/README.md
 M Embedded/RaspberryPi/Gateway/Gateway_doc.md
 M Embedded/RaspberryPi/Gateway/SCADA_Integration_Guide.md
 M Embedded/RaspberryPi/HMI/HMI_doc.md
 M Embedded/RaspberryPi/HMI/HMI_new/TECHNICAL.md
 M Embedded/RaspberryPi/HMI/hmi_contract.md
 M Embedded/RaspberryPi/pi_agent.md
 M PCB/PCB_V3/Connections.md
?? docs/
```

**20 modified, 1 untracked directory. 539 insertions, 54 deletions.** Nothing is committed.

Scope of the changes:

* **Documentation only**, except for the six `.ino` files.
* The six `.ino` edits are **comment-only**: all 30 changed lines begin with ` * ` and sit inside
  each file's opening `/* … */` header block, verified programmatically. No logic, no constants.
  **Not compiled** — `Embedded/bin/arduino-cli` is a macOS arm64 binary and will not run on the
  Windows host used for this review.
* **No KiCad file was modified.**
* `docs/` is new and contains `handover/kb-conflict-resolutions.md` and this file.

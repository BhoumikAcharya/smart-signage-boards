# Knowledge-Base Conflict Resolutions

Working record for the project handover. Each entry states the conflict as it appears in the
external knowledge base, the evidence found **in this repository** (with file paths and line
numbers), the decision taken by the project owner, and the files that need changing as a result.

**Repo state at time of review:** branch `Bhoumik_Embedded`, HEAD `d693441`.
**Review date:** 2026-10-23.

**Rule applied throughout:** no code or design file was modified while resolving these conflicts.
Where the repository holds no evidence, that is stated plainly rather than inferred.

---

## C1 — Buck converter count, rail assignment, and the AMS1117 feed

### Conflict (as stated in the knowledge base)

* BOM: **4x MP1584** — Buck 1–3 at 12 V for LEDs, Buck 4 at 5 V.
* PCB constraints doc: **5x MP1584**.
* BOM also states the AMS1117-3.3 is fed from **"Buck 5"**, while listing only Buck 4 as the 5 V unit.

### Evidence found in the repository

**Board netlist — `PCB/PCB_V3/PCB_V3_Final/PCB_V3_Final.kicad_pcb`** (identical in
`PCB/PCB_V3/PCB_V3.1/PCB_V3.1.kicad_sch`). Four MP1584 module positions exist, designated
**BC1, BC2, BC3 and BC5 — there is no BC4 on the board**:

| Module | Input net | Output net | Rail | Pin-socket refs |
| --- | --- | --- | --- | --- |
| BC1 | `/MAIN_OP` | `/12V_LR` | 12 V — LHS **and** RHS arrows (one shared rail) | J21/J22 → J26/J27 |
| BC2 | `/MAIN_OP` | `/12V_STATIC1` | 12 V — Static Zone 1 | J32/J33 ↔ J38/J39 |
| BC3 | `/MAIN_OP` | `/12V_STATIC2` | 12 V — Static Zone 2 | J23/J24 ↔ J28/J29 |
| BC5 | `/MAIN_OP` | `/5V` | 5.0 V | J34/J35 → J40/J41 |

Corroborated by the four test points: `TP1 12V_Arrows` on `/12V_LR`, `TP3 12V_Static_1` on
`/12V_STATIC1`, `TP2 12V_Static_2` on `/12V_STATIC2`, `TP4 5V` on `/5V`.

**AMS1117 feed.** `J20 AMS_IP_Pins` pin 1 = `/5V`, pin 2 = GND. `/5V` is the output of BC5
(`J40 BC5_OUT+`). `J25 AMS_OP_Pins` pin 1 = `/AMS_OP+`, the 3.3 V rail, which feeds the ESP32 3V3
pin (J2-15), the LAN8720 (J16-7), the MCP23017 (J17-1 and J17-7) and both I2C pull-ups
(R21-2, R22-2). The same `/5V` net also feeds all four ACS712 VCC pins (J3-3, J4-3, J7-3, J9-3).

**All four bucks tap the post-diode bus.** `D1` (Schottky, TO-220-2) anode = `/14.5_VD`,
cathode = `/MAIN_OP`; `J37 Battery_Terminal` pin 1 = `/MAIN_OP`. The battery therefore carries all
four rails, as asserted in `Embedded/ESP32/esp32_contract.md:254` and `PCB/PCB_V3/Connections.md:25`.

**Supporting documentation already in the repo:**

* `PCB/PCB_V3/Connections.md:12-17` — "Buck Converters (4x MP1584)"; Buck 1 = arrows,
  Buck 2 = Static Zone 1, Buck 3 = Static Zone 2, Buck 4 OUT (5.0 V) → ACS712 VCC + AMS1117 IN.
* `PCB/PCB_V3/Connections.md:21-23` — explicit 2026-08-07 correction: *"the count is 4x MP1584, not
  5x… The earlier revision listed five units with 'Buck 4 OUT (12V): Float/No Connection (Expansion
  buffer)'. The finalized hardware has no floating spare; the 5.0V unit is now Buck 4."*
* `Embedded/ESP32/DOC_UPDATES_PENDING.md:385-390` — records that "5x MP1584" appeared in three
  places and was corrected, *"and the 5.0V unit renumbered 5 → 4."*
* `Embedded/README.md:93` — "4x MP1584 buck converter (3x 12V LED rails + 1x 5V), AMS1117-3.3 for
  the 3.3V rail".
* `Embedded/ESP32/Workbench/Firmware_Test/README.md:49-56` — also says 4x, *"not 5; no floating
  spare"*, **but numbers them differently**: #1 = Static Zone 1, #2 = Static Zone 2, #3 = arrows,
  #4 = 5 V. This disagrees with `Connections.md` and with the board designators.

### Which side the evidence supports

| KB claim | Verdict | Basis |
| --- | --- | --- |
| BOM: 4x MP1584 | **Correct** | Four module footprints on the board |
| PCB constraints: 5x MP1584 | **Wrong** | Stale pre-2026-08-07 count, already retracted in `Connections.md`; the `BC5` designator makes four look like five |
| BOM: Buck 1–3 at 12 V for LEDs | **Correct** | BC1/BC2/BC3 → `/12V_LR`, `/12V_STATIC1`, `/12V_STATIC2` |
| BOM: Buck 4 is the 5 V unit | **Correct in substance** | It is the fourth converter; the board silkscreens it **BC5** |
| BOM: AMS1117 fed from "Buck 5" | **Correct as designated** | `J20` ← `/5V` ← `J40 BC5_OUT+`; same physical unit as "Buck 4" |

The BOM was never self-contradictory. It mixed the board **designator** (BC5) with the document
**sequence number** (Buck 4); both name the same single 5 V converter.

### DECISION (project owner, 2026-10-23)

**There are four buck converters. Documentation standardises on Buck 1–4 / BC1–BC4 — the 5 V unit
is renamed from BC5 to BC4 throughout the documentation.**

Canonical statement to use everywhere:

> **4x MP1584.** Buck 1 (BC1) = 12 V, LHS + RHS arrows on one shared rail. Buck 2 (BC2) = 12 V,
> Static Zone 1. Buck 3 (BC3) = 12 V, Static Zone 2. Buck 4 (BC4) = 5.0 V, feeding the four ACS712
> VCC pins and the AMS1117-3.3 input; the AMS1117 output is the 3.3 V rail for the ESP32, LAN8720,
> MCP23017 and the I2C pull-ups. All four bucks are fed from the post-diode `MAIN_OP` bus, so the
> battery carries the whole load including the three LED rails.

"Buck 5" is retired as a name. There is no floating spare and no fifth converter.

### Files changed or needing change

No file was modified while resolving this conflict.

| File | Needs | Status |
| --- | --- | --- |
| `PCB/PCB_V3/Connections.md` §1 | Already says 4x MP1584 with Buck 4 = 5.0 V → AMS1117. Matches the decision. | **No change needed** |
| `Embedded/README.md:93` | Already says 4x (3x 12 V + 1x 5 V). Matches. | **No change needed** |
| `Embedded/ESP32/esp32_contract.md:254` | Already says "All four MP1584 bucks". Matches. | **No change needed** |
| `Embedded/ESP32/Workbench/Firmware_Test/README.md:49-56` | Buck numbering disagrees with `Connections.md` (#1 = Static Zone 1 vs Buck 1 = arrows). Renumber to BC1 = arrows, BC2 = Static Zone 1, BC3 = Static Zone 2, BC4 = 5 V. | **CHANGE REQUIRED** |
| `PCB/PCB_V3/PCB_V3_Final/*` and `PCB/PCB_V3/PCB_V3.1/*` (KiCad) | **CORRECTED 2026-10-23 — no change needed.** The silkscreen carries no `BC` designators at all. It labels each buck position by its output: `LR_Arrows 12V`, `Static_LED_1 12V`, `Static_LED_2 12V`, `5V`, with `IN+ / IN- / OUT+ / OUT-` at the pads. Nothing on the board says "BC5" | **No change needed** |

### Silkscreen — checked 2026-10-23, and it is correct throughout

Extracted from the `F.Silkscreen` text of both `PCB_V3_Final.kicad_pcb` and `PCB_V3.1.kicad_pcb`.
**There are no `BC1`/`BC2`/`BC3`/`BC5` designators printed on the board.** Each buck position is
identified by what comes out of it:

| Silkscreen text | Position (x, y) | Buck |
| --- | --- | --- |
| `LR_Arrows` / `12V` | 206.00, 67.50 | Buck 1 — arrows |
| `Static_LED_1` / `12V` | 199.50, 72.50 | Buck 2 — Static Zone 1 |
| `Static_LED_2` / `12V` | 193.00, 67.50 | Buck 3 — Static Zone 2 |
| `5V` | 211.50, 67.50 | Buck 4 — 5 V rail |

The `IN+ / IN- / OUT+ / OUT-` markings at each module's pads were cross-checked against the pad
net assignments for all four positions. **Every one is correct**, including BC2 and BC3:

* Buck 2 — silk `IN+` sits at the `/MAIN_OP` pad and `IN-` at GND; silk `OUT+` at
  `/12V_STATIC1` and `OUT-` at GND.
* Buck 3 — silk `IN+` at `/MAIN_OP`, `IN-` at GND; silk `OUT+` at `/12V_STATIC2`, `OUT-` at GND.

> **Earlier finding retracted.** A previous draft of this entry flagged an assembly hazard on
> BC2/BC3, on the basis that their KiCad *reference designator names* (`J32 "BC2_IN+"`,
> `J39 "BC2_OUT-"`, and `J28` duplicating `J24`'s name) do not match the nets on those pads. That
> remains true **inside the KiCad file**, but those designator names are never printed on the
> board. The silkscreen an assembler actually reads is correct. **There is no assembly hazard.**
> The inconsistent internal names are cosmetic housekeeping for whoever next opens the project,
> nothing more.

### The silkscreen also records component values the schematic omits

Worth noting here because it bears on C2, C3 and C5, where the schematic symbols carry no values:

| Silkscreen text | Position (x, y) | What it is |
| --- | --- | --- |
| `100k` / `18k` | 132.50, 48.00 / 132.50, 51.50 | One voltage divider |
| `100k` / `18k` | 103.50, 148.50 / 103.50, 152.00 | The other voltage divider |
| `4.7k` × 2 | 177.00, 85.00 and 181.50, 85.00 | The I2C pull-ups (C3) |
| `470u` | 104.50, 66.00 | C1, the bulk cap (C5) |
| `100` × 10, `10k` × 10 | gate area | MOSFET gate series and pull-down resistors |

**The board itself independently confirms both dividers are 100k / 18k** (C2 and C5), that the
pull-ups are 4.7k and fitted (C3), and that C1 is 470 µF (C5). The silkscreen is better
documentation than the schematic here — the fix in those entries is to bring the schematic
symbol values up to what the board already prints.

---

## C2 — Battery voltage divider ratio

### Conflict (as stated in the knowledge base)

* BOM: **47k (R1) / 10k (R2)**, "confirmed in firmware calibration math".
* PCB constraints doc: voltage-monitoring dividers are **10k / 2.2k (16V -> 2.6V)**.

### What each file in the repository says

| File | What it says |
| --- | --- |
| `Embedded/ESP32/Workbench/Firmware/v4.1.0/v4.1.0.ino:109` (current firmware) | `BATT_DIV_RATIO = 6.556f;  // (100+18)/18 nominal` — **100k / 18k**, on GPIO39 |
| `Embedded/ESP32/Workbench/battery_calibration/battery_calibration.ino:4` and `:61` | "GPIO39 via a 100k/18k divider"; `BATT_DIV_RATIO = 6.556f` |
| `Embedded/ESP32/Workbench/Docs/Phase1-Part2_Battery-Calibration-Report.md:9` | "ESP32 `GPIO39` via a **100 kΩ / 18 kΩ** resistive divider" |
| `Embedded/ESP32/Workbench/HANDOFF.md` (Decisions) | "Divider `BATT_DIV_RATIO=6.556` (100k/18k, per-board, calibrate)" |
| `PCB/PCB_V3/PCB_V3_Final/PCB_V3_Final.kicad_sch` and `PCB/PCB_V3/PCB_V3.1/PCB_V3.1.kicad_sch` | R25 (`/MAIN_OP` -> `/BATT_VD`) and R26 (`/BATT_VD` -> GND) carry **no value** — the field reads the default `"R"` |
| `Embedded/ESP32/TestingCalibration/Battery/CalculatingBatteryADC/CalculatingBatteryADC.ino:8-10` | Comment says "47k and 10k"; the code immediately below uses `R1 = 42000.0, R2 = 10000.0`. Also on the wrong pin (GPIO34) and a 12V lead-acid curve. Obsolete |
| `Embedded/ESP32/TestingCalibration/Battery/ConvertingADCtoPercentage/ConvertingADCtoPercentage.ino:8` | Same self-contradicting comment, GPIO32. Obsolete |
| `Embedded/ESP32/Firmware/Version History/Firmware_2.0.0/Firmware.ino:56-57` | `BATT_R1 = 42000.0, BATT_R2 = 10000.0`. Superseded |

There is **no occurrence of 2.2k, 2200 or a 5.545 ratio anywhere in the repository**. The
10k/2.2k figure has no support here at all. The 10k/2.2k line is also wrong on its own terms:
that pair produces about 2.9 V from 16 V, not 2.6 V.

Routing is confirmed on the board: `J1 ESP_Left` pin 3 = `/BATT_VD` = GPIO39 (VN).

### Which side the evidence supports

Four current files say **100k / 18k**. Both knowledge-base figures are unsupported. The BOM's
"47k/10k" traces to a comment in an obsolete bench sketch whose own code contradicts it, so the
"confirmed in firmware calibration math" justification is false.

### DECISION (project owner, 2026-10-23)

**The fitted resistors are 100k and 18k. This is measured fact, not an assumption.** Ratio 6.556
on GPIO39. The current firmware is correct. Both knowledge-base documents are wrong and must be
corrected to 100k / 18k.

**Consequence check:** because firmware matches the fitted hardware, **the battery percentage is
NOT wrong on the PCB build.** The concern raised in the conflict does not apply.

### Files changed or needing change

No file was modified while resolving this conflict.

| File | Needs | Status |
| --- | --- | --- |
| KB **BOM** | 47k/10k -> **100k/18k**. Delete the phrase "confirmed in firmware calibration math" — it is false and is why the wrong value survived | **CHANGE REQUIRED (KB)** |
| KB **PCB constraints** | Remove "10k/2.2k (16V -> 2.6V)". Replace with 100k/18k on GPIO39 | **CHANGE REQUIRED (KB)** |
| `PCB/PCB_V3/PCB_V3_Final/PCB_V3_Final.kicad_sch`, `PCB/PCB_V3/PCB_V3.1/PCB_V3.1.kicad_sch` | Assign real values to **R25 = 100k, R26 = 18k**. An unvalued resistor on a measurement divider is the root cause of this conflict | **CHANGE REQUIRED (design file — not touched)** |
| `Embedded/ESP32/esp32_contract.md:294` (gap 9) | Currently says both divider ratios are open placeholders. Split it: **battery resolved (100k/18k)**, PSU still open | **CHANGE REQUIRED** |
| `Embedded/ESP32/TestingCalibration/Battery/CalculatingBatteryADC/CalculatingBatteryADC.ino`, `.../ConvertingADCtoPercentage/ConvertingADCtoPercentage.ino` | Delete, or mark clearly obsolete. These are the origin of the 47k/10k claim and they contradict themselves | **CHANGE REQUIRED** |
| `Embedded/ESP32/Workbench/Firmware/v4.1.0/v4.1.0.ino` | Already correct | **No change needed** |

### Carried forward

The **main-supply divider on GPIO36 is a separate divider with a separate ratio**
(`PSU_DIV_RATIO = 6.9f`, `v4.1.0.ino:78`), and no resistor pair is recorded for it anywhere.
Handled under **C5**.

---

## C3 — I2C pull-ups on SDA (GPIO4) and SCL (GPIO13)

### Conflict (as stated in the knowledge base)

* Pin map: external **4.7k pull-ups to 3.3V are MANDATORY** — the MCP23017 breakout was visually
  confirmed to have none.
* PCB constraints doc: the pull-ups are marked **DNP** on the schematic.
* Stated risk: if they are DNP, I2C to the MCP23017 will likely fail on the PCB build.

### What each file in the repository says

| File | What it says |
| --- | --- |
| `PCB/PCB_V3/PCB_V3_Final/PCB_V3_Final.kicad_sch` | **R21 = 4.7k**, `/D4` (GPIO4, SDA) -> `/AMS_OP+` (3.3V rail). **R22 = 4.7k**, `/D13` (GPIO13, SCL) -> `/AMS_OP+` |
| `PCB/PCB_V3/PCB_V3.1/PCB_V3.1.kicad_sch` | Identical — same refs, same values, same nets |
| `PCB/PCB_V3/PCB_V3_Final/PCB_V3_Final.kicad_pcb` | R21 and R22 placed on the board (x = 178.66 and 182.71, y = 82.66), both pads on live nets |
| `PCB/PCB_V3/PCB_V3.1/PCB_V3.1.kicad_pcb` | Identical placement |
| `Embedded/ESP32/esp32_contract.md:20-21` | SDA = GPIO4, SCL = GPIO13, "4.7k pull-up to 3V3" |
| `PCB/PCB_V3/Connections.md:71-72` | "SDA: Connect to ESP32 GPIO 4. Connect a 4.7kΩ pull-up resistor from this net to the 3.3V Rail." Same for SCL on GPIO 13 |

### DNP status — checked exhaustively

Every component in both schematics was parsed, not just the two resistors:

| File | `(dnp yes)` | `(dnp no)` | `(exclude_from_bom yes)` |
| --- | --- | --- | --- |
| `PCB_V3_Final.kicad_sch` | **0** | 140 | 0 |
| `PCB_V3.1.kicad_sch` | **0** | 142 | 0 |

**Nothing in either schematic is marked DNP.** No component is excluded from the BOM either.

### Which side the evidence supports

The **pin map is correct and the requirement is already met**. The PCB constraints doc is wrong:
the pull-ups are not DNP, they are fitted. R21 and R22 are present, correctly valued at 4.7k,
wired to the right pins and the right rail, and placed on the fabricated board in both V3 Final
and V3.1.

**The predicted failure does not apply.** I2C to the MCP23017 will not fail on the PCB for want of
pull-ups.

### DECISION (project owner, 2026-10-23)

**The pull-ups are fitted: R21 and R22, 4.7k, SDA and SCL to the 3.3V rail. Not DNP.** The PCB
constraints doc's DNP claim is retracted. The pin map's requirement is recorded as satisfied by
R21/R22 on the carrier board.

### Files changed or needing change

No file was modified while resolving this conflict.

| File | Needs | Status |
| --- | --- | --- |
| KB **PCB constraints** | Delete the DNP claim. Replace with: "R21 and R22, 4.7k, fitted — SDA and SCL pulled to the 3.3V rail" | **CHANGE REQUIRED (KB)** |
| KB **pin map** | Reword. "MANDATORY, breakout has none" reads as an open action item when it is done. State that the pull-ups are on the carrier board as R21/R22 — the breakout having none is precisely why they are there | **CHANGE REQUIRED (KB)** |
| KB **pin map** and KB **PCB constraints** | Add the reference designators **R21 / R22**. Neither document names them, which is part of why the question stayed open | **CHANGE REQUIRED (KB)** |
| `PCB/PCB_V3/Connections.md:71-72` | Already correct. Could optionally gain the R21/R22 designators for traceability | **No change needed** |
| `Embedded/ESP32/esp32_contract.md:20-21` | Already correct | **No change needed** |
| KiCad schematics and board files | Already correct | **No change needed** |

---

## C4 — ESP32 <-> LAN8720 RMII mapping, and the GPIO0 entry

### Conflict (as stated in the knowledge base)

* Pin map: finalized v2.0 table — CLK = GPIO17 out, MDIO = 18, MDC = 23, TX_EN = 21, TXD0 = 19,
  TXD1 = 22, CRS_DV = 27, RXD0 = 25, RXD1 = 26, **plus GPIO0 listed as "MAC"**.
* PCB constraints doc: this mapping is still **TBD**.
* Questions asked: (a) does firmware ETH init match the table, (b) does the schematic match the
  firmware, (c) what does GPIO0 actually connect to, and is there a boot-mode risk.

### What each file in the repository says

| File | What it says |
| --- | --- |
| `Embedded/ESP32/Workbench/Firmware/v4.1.0/v4.1.0.ino:154-159` and `:746` | `ETH_PHY_ADDR 1`, `ETH_PHY_POWER -1`, `ETH_PHY_MDC 23`, `ETH_PHY_MDIO 18`, `ETH_PHY_TYPE ETH_PHY_LAN8720`, `ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT`; `ETH.begin(TYPE, ADDR, MDC, MDIO, POWER, CLK_MODE)` |
| `Embedded/ESP32/Firmware/Frimware_v3.0.0/.../Firmware_v3.0.0.ino:168-173`, `:753` | Identical |
| `Embedded/ESP32/Firmware/Version History/Firmware_2.0.0/Firmware.ino:67-72` | Identical |
| `Embedded/ESP32/Firmware/Version History/Firmware_1.1.1/Firmware.ino:49-54` | Identical |
| `Embedded/ESP32/esp32_contract.md:33` | MDC=23, MDIO=18, CRS_DV=27, RXD0=25, RXD1=26, TXD0=19, TXD1=22, TX_EN=21, REFCLK=GPIO17, PHY_ADDR=1, PHY_POWER=-1. **No GPIO0** |
| `PCB/PCB_V3/Connections.md:37-40` | GPIO17->ETH_CLK, GPIO18->MDIO, GPIO23->MDC, and **"GPIO 0, 19, 21, 22, 25, 26, 27 -> Ethernet MAC Pins"** |
| `PCB/PCB_V3/PCB_V3_Final/PCB_V3_Final.kicad_pcb`, J16 (LAN8720 header 1) | pin1 unconnected, pin2 `/D19`, pin3 `/D26`, pin4 `/D27`, pin5 `/D23`, pin6 GND, pin7 `/AMS_OP+` (3.3V) |
| same file, J15 (LAN8720 header 2) | pin1 `/D22`, pin2 `/D21`, pin3 `/D25`, pin4 `/D17`, pin5 `/D18`, pins 6-7 unconnected |
| `PCB/Open Source Files/ESP32/ESP32-PoE_Rev_M1.kicad_sch:240` | Olimex reference design — GPIO0 is `EMAC_TX_CLK` |

### (a) Firmware vs the pin map — MATCHES

Every configurable value agrees: PHY address 1, MDC 23, MDIO 18, clock mode
`ETH_CLOCK_GPIO17_OUT`, no power pin (`-1`). Identical across all four firmware versions in the
repo, back to v1.1.1.

The six data pins (TXD0/19, TXD1/22, TX_EN/21, RXD0/25, RXD1/26, CRS_DV/27) do not appear in the
code because the ESP32 fixes them in silicon for RMII — they are not configurable. The firmware
therefore cannot disagree with the table on those six.

Power pin: firmware declares none, and the board ties the LAN8720 supply (J16 pin 7) directly to
the 3.3V rail. Consistent.

### (b) Schematic vs firmware — MATCHES, with one thing the repo cannot prove

All nine RMII signals reach the two LAN8720 headers, plus GND and 3.3V, and nothing else:
D19, D26, D27, D23 on J16; D22, D21, D25, D17, D18 on J15. No tenth signal, no spare.

**Unverifiable from the repo:** J15 and J16 are generic `Connector:Conn_01x07_Pin` symbols. Their
pins are `Pin_1`..`Pin_7` with no signal names, so the repository proves the right nine signals
reach the module but **cannot prove they land on the correct module pins**. That must be checked
against the LAN8720 module's own pinout. Recorded as a new bench item below.

**PHY address 1** also cannot be verified here — it is set by strapping resistors on the LAN8720
module, which is not in the repository.

### (c) GPIO0 — connects to NOTHING on this board

There is **no `/D0` net anywhere** in the schematic or the board file. Every pad on the two ESP32
headers (J1, J2) is either a named signal net, GND, 3.3V, or explicitly marked unconnected.
GPIO0 is not among them.

**Origin of the error.** The Olimex ESP32-PoE reference design held in this repo at
`PCB/Open Source Files/ESP32/ESP32-PoE_Rev_M1.kicad_sch:240` wires GPIO0 as `EMAC_TX_CLK` — that
board takes the 50 MHz clock *from* the PHY on GPIO0. This design does the opposite: the ESP32
*generates* the clock on GPIO17. The GPIO0 line was carried across into `Connections.md:40`, and
the knowledge-base pin map inherited it from there.

**Boot-mode risk — documentation only, but real.** GPIO0 is a strapping pin: held low at reset the
ESP32 enters download mode instead of booting. On the actual board there is **no risk**, because
GPIO0 is unconnected and the DevKit's onboard pull-up holds it high. The hazard is that **anyone
building a new board from the pin map as written would wire GPIO0 to a 50 MHz clock line**, and
get a board that boots unreliably or not at all. This is the main reason the conflict matters.

### Which side the evidence supports

The **pin map table is correct except for the GPIO0 entry, which must be deleted.** The PCB
constraints doc's "TBD" is simply stale — the mapping is confirmed and consistent across firmware,
schematic and board.

### DECISION (project owner, 2026-10-23)

The v2.0 RMII table is confirmed and final. **The GPIO0 entry is deleted** — GPIO0 is deliberately
unconnected, and the documentation must say so together with the reason. "TBD" in the PCB
constraints doc is replaced by the confirmed table.

### Files changed or needing change

No file was modified while resolving this conflict.

| File | Needs | Status |
| --- | --- | --- |
| KB **pin map** | Delete the GPIO0 row. Add: *"GPIO0 is deliberately unconnected. The 50 MHz RMII clock is generated by the ESP32 on GPIO17 (`ETH_CLOCK_GPIO17_OUT`). Do not wire GPIO0 to the PHY — it is a strapping pin and a clock on it breaks boot. The Olimex ESP32-PoE reference design in `PCB/Open Source Files/ESP32/` uses GPIO0 for the clock; that is a different topology, do not copy it."* | **CHANGE REQUIRED (KB)** |
| KB **pin map** | Note that TXD0 / TXD1 / TX_EN / RXD0 / RXD1 / CRS_DV are fixed by the ESP32 in silicon and not configurable, so their absence from the firmware is expected | **CHANGE REQUIRED (KB)** |
| KB **PCB constraints** | Replace "TBD" with the confirmed table | **CHANGE REQUIRED (KB)** |
| `PCB/PCB_V3/Connections.md:40` | Remove GPIO 0 from "GPIO 0, 19, 21, 22, 25, 26, 27 -> Ethernet MAC Pins". This line is the in-repo source of the error | **CHANGE REQUIRED** |
| `PCB/PCB_V3/Connections.md` §7 | Add a new bench item: **verify the J15 / J16 socket pin order against the LAN8720 module pinout.** The repo cannot answer it — the connectors are generic symbols with unnamed pins | **CHANGE REQUIRED** |
| `Embedded/ESP32/esp32_contract.md:33` | Already correct — no GPIO0, all values match | **No change needed** |
| Firmware (all versions) | Already correct | **No change needed** |

---

## C5 — Main supply monitor voltage range (GPIO36)

### Conflict (as stated in the knowledge base)

* Pin map labels GPIO36 "Main Power Supply Voltage **(24V/12V)** Monitor".
* Power architecture doc: **14.8V PSU** -> ~14.4V MAIN_OP bus.
* Questions asked: what does firmware compute and threshold against, and does the divider keep the
  maximum possible input (PSU fault, up to ~16V) below 3.3V at the ADC pin.

### What each file in the repository says

| File | What it says |
| --- | --- |
| `Embedded/ESP32/Workbench/Firmware/v4.1.0/v4.1.0.ino:78` | `PSU_DIV_RATIO = 6.9f` — comment: "Per-board — set with a multimeter" |
| `.../v4.1.0.ino:86-88` | `PSU_FAIL_VOLTS = 10.0f`, `PSU_OK_VOLTS = 11.0f`, `PWR_DEBOUNCE_COUNT = 3` |
| `.../v4.1.0.ino:561-564` | `psuV = adcVolts(PIN_VOLT_PSU) * PSU_DIV_RATIO;` then FAIL at or below 10.0, OK at or above 11.0, hold in between |
| `.../v4.1.0.ino:286` | `adcVolts()` uses the raw `raw/4095*3.3` path (the battery channel uses the factory-calibrated `analogReadMilliVolts`) |
| `PCB/PCB_V3/PCB_V3_Final/PCB_V3_Final.kicad_pcb` | R23 `/14.5_VD` -> `/PSU_VD`, R24 `/PSU_VD` -> GND, both **unvalued** in the schematic. `/PSU_VD` reaches J1 pin 2 = GPIO36 (VP) |
| same | `/14.5_VD` sits **post-fuse (F1), pre-diode (D1 anode)**. The battery is on `/MAIN_OP`, the diode's cathode side |
| `PCB/PCB_V3/Connections.md:10` | "Main Input: **14.5V** DC entering via terminal block" |
| `Embedded/README.md:99` | "**14.5V** PSU" |
| `Embedded/ESP32/Workbench/Firmware_Test/README.md:42` | "14.5V Input Terminal Block (+) -> Fuse Holder -> **470uF Electrolytic Cap** -> Diode -> Battery Terminal Block" |
| `Embedded/ESP32/Workbench/HANDOFF.md:71` | "14.45 V PSU - MBR1035 diode (~0.3 V) = ~14.15 V" |
| `Embedded/ESP32/esp32_contract.md:266` | Thresholds are placeholders and "must sit below the normal supply voltage but above the battery's loaded voltage"; calls it "the narrowest calibration window in the system" |

### DECISION (project owner, 2026-10-23)

**R23 = 100k and R24 = 18k — the same pair as the battery divider. The PSU is 14.5V.**

**`PSU_DIV_RATIO = 6.9f` is CORRECT and must NOT be changed.** It came from manual testing and
calibration on real hardware. The code stays as it is.

The two numbers are both right and describe different things:

| Number | What it is |
| --- | --- |
| **6.556** | The **nominal** divider ratio from the fitted parts — (100k + 18k) / 18k |
| **6.9** | The **bench-calibrated effective** ratio, measured against a multimeter. More accurate in practice |

**Wherever 6.9 appears, the documentation must state both:** the nominal divider is 100k/18k =
6.556, and manual calibration gives 6.9 as the more accurate figure in service.

**Why they differ, and why this matters for anyone tempted to "fix" it.** The PSU channel reads
through the uncalibrated `raw/4095*3.3` path at `v4.1.0.ino:286`, which under-reads on the ESP32's
11 dB attenuation range. The measured 6.9 therefore absorbs **both** the divider ratio **and** the
ADC's own error — 6.9 / 6.556 = 1.052, a ~5% ADC under-read, which is the expected magnitude.
It is a single combined correction factor, not a divider ratio on its own.

> **Consequence: do not switch the PSU channel to `analogReadMilliVolts()`.** Doing so would
> remove the ADC error that 6.9 compensates for, and the node would immediately start over-reading
> the PSU by ~5%. If that change is ever made, `PSU_DIV_RATIO` must be re-calibrated to ~6.556 in
> the same commit. Record this next to the constant so nobody harmonises the two channels by
> accident.

### What the firmware computes, and against what

GPIO36 is read, multiplied by `PSU_DIV_RATIO`, and compared against a 10.0 V / 11.0 V hysteresis
band with a 3-read debounce. The monitored node is the **pre-diode 14.5 V PSU rail**, not the
MAIN_OP bus. Nominal reading in service is ~14.5 V.

### Does the divider keep the maximum input below 3.3V at the pin — YES

Using the nominal 6.556 ratio, since the true pin voltage follows the physical divider:

| Input at `/14.5_VD` | Voltage at GPIO36 |
| --- | --- |
| 14.5 V nominal | 2.21 V |
| 16 V PSU fault | 2.44 V |
| 3.3 V reached at | 21.6 V |

Comfortable margin, and both readings sit inside the ESP32's usable ADC range. **No risk.**

### Two further findings

**1. The contract's threshold guidance is based on the wrong hardware model.**
`esp32_contract.md:266` assumes GPIO36 measures the shared bus, so the threshold must thread
between mains and loaded-battery voltage. It does not. R23 taps `/14.5_VD`, **before** diode D1;
the battery is on `/MAIN_OP`, **after** it. On PSU loss D1 blocks back-feed and the measured node
collapses toward zero. Detection is robust, not delicate — almost any threshold between roughly
1 V and 13 V works. Bench item B4 is far simpler than the contract implies.

**2. The alarm is slow, and the number is now calculable.**
C1 (470 uF, per `Firmware_Test/README.md:42`) sits on `/14.5_VD`. All four bucks draw from
`/MAIN_OP`, the other side of the diode. After a PSU loss **the only discharge path for C1 is the
118k divider itself** — a ~55 s time constant, giving roughly **20 seconds before `power=FAIL` is
published** (plus 1.5 s debounce). For the system's stated critical alarm this should be a known,
documented number. A PSU with its own output bleeder will be faster; a blown fuse or broken wire
gives the full delay.

### Files changed or needing change

No file was modified while resolving this conflict.

| File | Needs | Status |
| --- | --- | --- |
| Firmware — `Workbench/Firmware/v4.1.0/v4.1.0.ino:78`, `v4.0.0/v4.0.0.ino:98`, `Nodes_Testing/Node_1..3/*.ino:78`, `Firmware/Frimware_v3.0.0/.../Firmware_v3.0.0.ino:98` | **Leave `6.9f` as-is.** Expand the comment only: nominal divider 100k/18k = 6.556; 6.9 is the bench-calibrated effective ratio and also absorbs the uncalibrated ADC error; do not change it without re-calibrating | **COMMENT-ONLY CHANGE — no logic change** |
| `Workbench/Firmware/v4.1.0/v4.1.0.ino:286` and `:561` | Add a warning that the PSU channel must stay on the raw `adcVolts` path, because `PSU_DIV_RATIO = 6.9` is calibrated against it. Switching to `analogReadMilliVolts` requires re-calibrating to ~6.556 in the same change | **COMMENT-ONLY CHANGE** |
| KB **pin map** | "Main Power Supply Voltage (24V/12V) Monitor" -> "PSU rail monitor, 14.5 V nominal, measured pre-diode on GPIO36". There is no 24 V rail in this design | **CHANGE REQUIRED (KB)** |
| KB **power architecture** | 14.8 V PSU -> **14.5 V**. MAIN_OP is ~14.15–14.2 V after the MBR1035 drop | **CHANGE REQUIRED (KB)** |
| KB **BOM / PCB constraints** | Record R23 = 100k, R24 = 18k on GPIO36, pre-diode tap. State nominal ratio 6.556 **and** calibrated ratio 6.9 together, with the reason they differ | **CHANGE REQUIRED (KB)** |
| `Embedded/ESP32/esp32_contract.md:266` (section 6a) | Rewrite: the measurement is pre-diode and isolated from the battery. Drop the "narrowest calibration window in the system" framing — it is incorrect | **CHANGE REQUIRED** |
| `Embedded/ESP32/esp32_contract.md:294` (gap 9) | Both dividers are now known: 100k/18k nominal, battery ratio 6.556, PSU calibrated to 6.9. **Close the gap** — it is no longer an open placeholder | **CHANGE REQUIRED** |
| `Embedded/ESP32/DOC_UPDATES_PENDING.md` section 0 item **B4** | Simplify — the threshold window is wide, not narrow. Add a new bench item: **time how long `power=FAIL` takes to appear after the PSU is cut.** If ~20 s is too slow, the fix is a bleeder across C1, not firmware | **CHANGE REQUIRED** |
| `PCB/PCB_V3/PCB_V3_Final/PCB_V3_Final.kicad_sch`, `PCB/PCB_V3/PCB_V3.1/PCB_V3.1.kicad_sch` | Assign values: **R23 = 100k, R24 = 18k, C1 = 470uF**. Same root cause as C2 | **CHANGE REQUIRED (design file — not touched)** |

---

## C6 — Diagnostic register block 45001+

### Conflict (as stated in the knowledge base)

* Architecture doc: the gateway translates ESP32 4-state strings (ON / OFF / FAIL_OPEN /
  FAIL_SHORT) into Modbus integers in a diagnostic block starting at **45001**.
* HMI spec: its Modbus map lists only **40001-40100** (actual state), **41001-41100** (HMI
  command) and **43001** (MUX flag). No 45001.
* Questions asked: does the gateway implement 45001+, what is the exact integer encoding and
  per-node layout, is it 0- or 1-based relative to pymodbus addressing, and does the HMI read it.

### What each file in the repository says

| File | What it says |
| --- | --- |
| `Embedded/RaspberryPi/Gateway/Pi.py:38-40` | `ADDR_DIAG = 5000`, `DIAG_REGS_PER_NODE = 8`, `HR_SIZE = 6000` ("covers the diag block end at index 5799") |
| `Pi.py:186-204` | `write_diagnostic_block()` — builds the 8 values and writes them with `setValues(3, ADDR_DIAG + i*8, values)` |
| `Pi.py:306-309` | Called every second for all 100 nodes, **unconditionally** — explicitly outside the `isatty()` presentation branch |
| `Pi.py:82-87` | `map_discrepancy_to_int()` — the 4-state encoding |
| `Pi.py:90-95` | `map_battery_to_int()` — int, else 65535 |
| `Pi.py:98-108` | `map_state_to_int()` — int in 0..65534, else 65535 |
| `Pi.py:331` | `ModbusSequentialDataBlock(0, [0] * HR_SIZE)` — datablock starts at address 0 |
| `Gateway_doc.md:124-141` | Full per-node layout table |
| `SCADA_Integration_Guide.md:274-300` | Same layout plus the 4-state encoding table |
| `SCADA_Integration_Guide.md:84-90` | "Modbus wire address = PLC register number - 40001. Register 45001 is wire address 5000... Do not add a correction of your own" |
| `SCADA_Integration_Guide.md:464-466` | Commissioning check: power on node 2 only, confirm 45009 goes to 1 and 45008 does not |
| `RaspberryPi/HMI/hmi_contract.md:33` | **Does list it** — "Diagnostic Block, `45001 + i*8`, datastore index `5000 + i*8`, read (optional - MQTT is richer)" |
| `HMI.py:36-38`, `HMI_new/hmi_light.py:41-43`, `HMI_new/hmi_dark.py:41-43` | Only three constants: `REG_ACTUAL_BASE = 0`, `REG_HMI_BASE = 1000`, `REG_FLAG_MUX = 3000`. **No 5000 anywhere** |
| `Gateway/bench/scada_probe.py:21-26` | Independent reader: `ADDR_DIAG, DIAG_PER_NODE = 5000, 8`; field order `Network, Power, LHS, RHS, Battery, Static Z1, Static Z2, Actual` |

### Does the gateway implement 45001+ — YES, fully

All 8 registers are written for all 100 nodes once per second. `Pi.py:191-192` records that in
v1.0.1 this write sat inside `if sys.stdout.isatty():`, so under systemd the PLC read zeros from
every diagnostic register forever. It is now unconditional.

### Exact integer encoding

**4-state health** (`Pi.py:82-87`):

| String | Integer |
| --- | --- |
| `OFF` | 0 |
| `ON` | 1 |
| `FAIL_OPEN` | 2 |
| `FAIL_SHORT` | 3 |
| anything else, including `---` | 99 |

**Battery** (`Pi.py:90-95`): the published integer, or **65535** if missing or unparseable.
Never 0 — `0` would be indistinguishable from a genuinely flat battery.

**Actual executing state** (`Pi.py:98-108`): the integer if it parses and is within 0..65534,
otherwise **65535**. The firmware's non-numeric `FAULT` payload lands here as 65535.

### Per-node layout

**8 registers per node, contiguous.** Node N starts at `45001 + (N - 1) * 8`.
Node 1 = 45001-45008, node 2 = 45009-45016, node 100 = 45793-45800.

| Offset | Content | Encoding |
| --- | --- | --- |
| +0 | Network status | 1 = online, 0 = offline |
| +1 | Main power | 1 = OK, 0 = fail |
| +2 | LHS arrows health | 4-state |
| +3 | RHS arrows health | 4-state |
| +4 | Battery percentage | 0-100, or 65535 = unknown |
| +5 | Static Zone 1 health | 4-state |
| +6 | Static Zone 2 health | 4-state |
| +7 | Actual executing mode | mode number, or 65535 = unknown |

> **The ordering is not intuitive: battery sits at +4, between the two arrow channels and the two
> static zones.** This is historical — the block grew 6 -> 7 -> 8 registers (`Pi.py:39`) and new
> fields were appended rather than inserted in logical order. A PLC integrator must not assume
> current1-4 are consecutive.

### 0-based or 1-based relative to pymodbus

**Register 45001 is wire address 5000. Subtract 40001 from the PLC register number. There is no
off-by-one correction to apply.**

The gateway writes through `context[0].setValues(3, 5000 + i*8, ...)` and SCADA reads through the
same `ModbusDeviceContext`, so any internal addressing adjustment pymodbus applies acts on both
sides and cancels. `SCADA_Integration_Guide.md:84-90` states this was verified against a live
server with a live client, and warns explicitly against adding a correction. The commissioning
check at `:464-466` is the practical test.

### Does the HMI read it — NO

All three HMI files define only the three constants for 40001+, 41001+ and 43001. There is no
reference to `5000` or `45001` anywhere in HMI code. The HMI takes all telemetry straight from
MQTT (`metro/signage/register/+/+`), which carries the original strings rather than the integer
encoding.

This is deliberate, and `hmi_contract.md:33` says so: the diagnostic block is
"read (optional - MQTT is richer)".

### Which side the evidence supports

**The architecture doc is correct.** The HMI spec's register map is **incomplete rather than
wrong** — it omits both the diagnostic block at 45001 and the SCADA buffer at 42001. The repo's
own `hmi_contract.md` §2 already lists all five zones, so the knowledge-base HMI spec has simply
fallen behind it.

### DECISION (project owner, 2026-10-23)

The gateway implements 45001+ exactly as the architecture doc describes. The encoding, the
8-register layout and the addressing above are confirmed canonical. The HMI legitimately does not
read the block and does not need to. The HMI spec's Modbus map must be completed.

### Files changed or needing change

No file was modified while resolving this conflict.

| File | Needs | Status |
| --- | --- | --- |
| KB **HMI spec** | Add the two missing rows to the Modbus map: **42001-42100** (SCADA buffer, PLC owns it, HMI must not touch) and **45001-45800** (diagnostic block, 8 per node, gateway writes, read-only, optional for the HMI). The correct table already exists at `RaspberryPi/HMI/hmi_contract.md:25-35` — copy it | **CHANGE REQUIRED (KB)** |
| KB **HMI spec** | State explicitly that **the HMI does not read 45001+ and does not need to**, because MQTT carries richer data. Its silent absence is what made this look like a contradiction | **CHANGE REQUIRED (KB)** |
| KB **architecture doc** | Add the full 8-register layout and call out that **battery sits at +4**, not after the four current channels. This is the one detail that will catch out a PLC integrator | **CHANGE REQUIRED (KB)** |
| `Embedded/RaspberryPi/Gateway/bench/scada_probe.py:9` | Docstring says "node 1's **7** diagnostic registers"; the code uses 8 (`:21`). Stale docstring | **MINOR CHANGE REQUIRED** |
| `Embedded/RaspberryPi/Gateway/Pi.py` | Already correct | **No change needed** |
| `Embedded/RaspberryPi/Gateway/Gateway_doc.md`, `SCADA_Integration_Guide.md` | Already correct and consistent with the code | **No change needed** |
| `Embedded/RaspberryPi/HMI/hmi_contract.md` | Already correct — lists all five zones | **No change needed** |

---

## C7 — MQTT payload mismatch: HMI vs firmware

### Conflict (as stated in the knowledge base)

* HMI spec expects `current1` / `current2` payloads of **`OK` / `FAIL`** on
  `metro/signage/register/<address>/<metric>`.
* Firmware emits **4-state strings** (ON / OFF / FAIL_OPEN / FAIL_SHORT) and also has Load 3/4
  (always-on) channels.
* Questions asked: what are the actual topics and payloads on all three sides, how does the HMI
  currently display FAIL_OPEN / FAIL_SHORT, and are loads 3/4 shown anywhere.

### What each side actually does

**Firmware publishes** (`Workbench/Firmware/v4.1.0/v4.1.0.ino:735-740` for the topic strings,
`:356-374` for the payloads). All retained; the topic address is the holding-register number:

| Topic | Payload actually emitted |
| --- | --- |
| `metro/signage/register/<reg>/status` | `ONLINE:<ip>` or `OFFLINE` |
| `.../power` | `OK` or `FAIL` |
| `.../current1` | **`ON` or `OFF` only** |
| `.../current2` | **`ON` or `OFF` only** |
| `.../current3` | `ON` or `FAIL_OPEN` |
| `.../current4` | `ON` or `FAIL_OPEN` |
| `.../battery_pct` | `0`, `50` or `100` |
| `.../state` | mode integer, or `FAULT` |

**Gateway subscribes** to all of these (`Pi.py:65-73`, `:139-141`) and maps the strings to the
integers described in C6.

**HMI subscribes** to `metro/signage/register/+/+` (`HMI_new/hmi_light.py:235`) and maps all seven
metrics (`:114-117`). Same in `hmi_dark.py` and the older `HMI.py`.

### Both halves of the stated conflict are out of date

**1. The "HMI expects OK/FAIL" claim is obsolete — it was a real bug and it is fixed.**
`RaspberryPi/HMI/hmi_contract.md:117` lists it as bug #4: *"Compares current state to `OK` — every
current row shows an error colour permanently"*, closed 2026-07-27. All three HMI files now handle
the 4-state strings correctly.

**2. The firmware no longer emits 4 states on the arrows either — and the conflict does not
mention this.** `v4.1.0.ino:602-608`:

```
const char* sl = expectLeftOn  ? "ON" : "OFF";
const char* sr = expectRightOn ? "ON" : "OFF";
```

`current1` and `current2` are now **intent-echo**: they report what the side was commanded to do,
not what was measured. They can never say `FAIL_OPEN` or `FAIL_SHORT`. The reason is recorded in
`Workbench/HANDOFF.md`: a 2-strip chase draws about 0.17 A, roughly 40 ADC counts on the 5A
ACS712, which is inside its own noise — a measured verdict false-alarms in both directions. Real
arrow health is deferred to the Phase-3 self-test.

Only `current3` and `current4` are genuinely measured (`v4.1.0.ino:612-618`), and because the
static zones are hardwired always-on they can only ever report `ON` or `FAIL_OPEN`.

**Net effect: `FAIL_SHORT` is currently unreachable on every channel, and `FAIL_OPEN` only ever
originates from the two static zones.**

`Workbench/HANDOFF.md` already flags this: *"UNTOUCHED — `esp32_contract.md` +
`RaspberryPi/Gateway/SCADA_Integration_Guide.md` still document arrow current channels as 4-state
and battery as unpublished; both now stale."*

### How the HMI displays FAIL_OPEN / FAIL_SHORT — correctly, in three places

`HMI_new/hmi_light.py:138-145` (`four_state_health`):
`ON` -> ok colour, `OFF` -> dim (healthy, **not** a fault), `FAIL_OPEN` and `FAIL_SHORT` -> fault
colour, `---` -> dim.

* Node health roll-up (`:149-162`) marks the whole node FAULT if any of the four channels reads
  `FAIL_*`.
* Alarm list (`:197-199`) raises an entry reading e.g. `FAIL_OPEN · Static Zone 1`.
* The older `HMI.py:118-131` (`four_state_color`) behaves identically.

> **Caveat:** none of this has been run against hardware. `hmi_contract.md:108` says the fixes are
> *"Not yet executed against hardware or the live gateway"*, and `HANDOFF.md` says the new HMIs
> are py_compile-clean but *"Not run on device"*.

### Are loads 3 and 4 shown anywhere — YES, everywhere

| Where | How |
| --- | --- |
| New HMI (`hmi_light.py` / `hmi_dark.py`) | `SENSOR_LABELS` at `:120-121` — "Static Zone 1", "Static Zone 2"; rendered as node-detail rows at `:932`; included in the health roll-up and the alarm list |
| Old HMI (`HMI.py:620-621`) | Same two rows |
| Gateway terminal table (`Pi.py:219-220`) | "Static Z1" and "Static Z2" columns |
| SCADA | Diagnostic registers `+5` and `+6` |

**Naming:** the conflict calls them "Load 3/4". The project renamed them **Static Zone 1** and
**Static Zone 2** project-wide on 2026-08-07 (`PCB/PCB_V3/Connections.md:62`). "Load 3/4" is
retired terminology and should not appear in the handover.

### Why the arrows became intent-echo — project owner's account, 2026-10-23

> Arrow diagnostics **were implemented earlier**. The measured values fluctuated too much to be
> read reliably while the arrows were animating, so the owner moved arrow health into **Phase 3**,
> where all arrows can be switched on together and a steady, accurate reading taken.

This is a deliberate engineering decision, not an omission or a regression. It matches the numbers
recorded in `Workbench/HANDOFF.md`: a 2-strip chase draws roughly 0.17 A, about 40 ADC counts on
the 5A ACS712, which sits inside the part's own noise. Holding every strip on for a self-test
raises the signal well clear of that floor, which is why the Phase-3 approach works where
continuous monitoring during animation did not.

The consequence for the handover is unchanged: **the behaviour is correct, the documentation is
not.** `esp32_contract.md` and `SCADA_Integration_Guide.md` still describe arrow health as
continuously measured 4-state. They must say it is intent-echo today, and measured by on-demand
self-test from Phase 3.


### DECISION (project owner, 2026-10-23)

The HMI spec's `OK`/`FAIL` expectation is obsolete and is removed — the HMI already handles the
4-state strings correctly on all three files. Static Zones 1 and 2 are displayed throughout; the
"Load 3/4" naming is retired.

**The substantive issue is the opposite of the one stated:** the arrow channels are now **2-state
intent-echo**, and `esp32_contract.md` and `SCADA_Integration_Guide.md` still promise 4-state
measured health on them. Those documents must be corrected.

### Files changed or needing change

No file was modified while resolving this conflict.

| File | Needs | Status |
| --- | --- | --- |
| KB **HMI spec** | Delete the `OK`/`FAIL` expectation for `current1` / `current2`. Payloads are `ON` / `OFF` / `FAIL_OPEN` / `FAIL_SHORT`, and the HMI already handles them | **CHANGE REQUIRED (KB)** |
| `Embedded/ESP32/esp32_contract.md` §3 (lines 99-146) | **Highest priority of this conflict.** Still documents `current1` / `current2` as 4-state measured. They are now 2-state intent-echo (`ON`/`OFF` only). State plainly that **arrow health is not currently monitored** and arrives with the Phase-3 self-test. Keep `FAIL_OPEN` / `FAIL_SHORT` in the encoding tables as reserved values so nothing breaks when Phase 3 lands | **CHANGE REQUIRED** |
| `Embedded/RaspberryPi/Gateway/SCADA_Integration_Guide.md` §5.1 (around line 289) | Same correction. As written, a SCADA integrator would build a `FAIL_OPEN` alarm on the arrows that can never fire, and would believe arrow health is monitored when it is not. This is a safety-relevant documentation gap, not a cosmetic one | **CHANGE REQUIRED** |
| `Embedded/ESP32/esp32_contract.md` §7, `SCADA_Integration_Guide.md` §5.5, `Embedded/README.md` item 3.8 | All still say battery is never published and always reads `65535`. It now publishes coarse `0` / `50` / `100` | **CHANGE REQUIRED** |
| KB **HMI spec**, KB **architecture doc** | Rename "Load 3/4" to **Static Zone 1 / Static Zone 2** to match the rest of the project | **CHANGE REQUIRED (KB)** |
| `RaspberryPi/HMI/HMI.py`, `HMI_new/hmi_light.py`, `HMI_new/hmi_dark.py` | Already correct | **No change needed** |
| `RaspberryPi/Gateway/Pi.py` | Already correct — passes all four channels through unchanged | **No change needed** |

### Open item carried forward

The HMI's 4-state handling and the new `hmi_light.py` / `hmi_dark.py` builds have **never been run
against hardware or a live gateway**. The test plan exists at `hmi_contract.md:143-168` (§5.1) and
has not been executed. This is tracked there, not here.

---

## Final summary

All seven conflicts resolved 2026-10-23. No code or design file was modified.

| ID | Decision | Files changed or needing change | Knowledge-base doc to update |
| --- | --- | --- | --- |
| **C1** | 4x MP1584, not 5. BC1 = 12V arrows, BC2 = 12V Static Zone 1, BC3 = 12V Static Zone 2, BC4 = 5.0V -> ACS712 VCC + AMS1117 -> 3.3V rail. All four fed from the post-diode MAIN_OP bus. **Nomenclature switched from BC5 to BC4 in documentation**; "Buck 5" is retired | `Embedded/ESP32/Workbench/Firmware_Test/README.md:49-56` (buck numbering disagrees). **Silkscreen checked and correct — nothing to change on the board.** It carries no `BC` designators at all: each buck is labelled by output (`LR_Arrows 12V`, `Static_LED_1 12V`, `Static_LED_2 12V`, `5V`), and every `IN+/IN-/OUT+/OUT-` marking matches its net. Only the internal KiCad designator names are inconsistent — cosmetic, never printed | BOM; PCB constraints |
| **C2** | Battery divider is **100k / 18k, ratio 6.556, on GPIO39** — measured fact. Both KB figures (47k/10k and 10k/2.2k) are wrong. Firmware is correct, so **the battery percentage is NOT wrong on the PCB build** | `esp32_contract.md:294` (split gap 9); `TestingCalibration/Battery/CalculatingBatteryADC.ino` + `ConvertingADCtoPercentage.ino` (delete or mark obsolete — origin of the 47k/10k claim); KiCad R25 = 100k, R26 = 18k | BOM (also delete "confirmed in firmware calibration math" — it is false); PCB constraints |
| **C3** | Pull-ups are **fitted, not DNP**: R21 and R22, 4.7k, SDA/SCL to the 3.3V rail. Zero components are DNP in either schematic. **I2C will not fail for this reason** | None — schematic, board and both repo docs are already correct | PCB constraints (retract the DNP claim); pin map (reword "MANDATORY", add R21/R22 designators) |
| **C4** | RMII table confirmed and final across firmware, schematic and board. **GPIO0 entry deleted** — it connects to nothing; the clock is generated on GPIO17. The GPIO0 line was copied from the Olimex ESP32-PoE reference design, which uses the opposite topology | `PCB/PCB_V3/Connections.md:40` (remove GPIO 0 from the MAC pin list); `Connections.md` §7 (add bench item: verify J15/J16 socket order against the LAN8720 module pinout — the repo cannot answer it) | Pin map (delete GPIO0 row, add the boot-mode warning, note that the six data pins are fixed in silicon); PCB constraints (replace "TBD" with the confirmed table) |
| **C5** | R23 = 100k, R24 = 18k; PSU is **14.5V**. **`PSU_DIV_RATIO = 6.9f` stays — it is a bench-calibrated value, not an error.** Nominal ratio is 6.556; 6.9 also absorbs the uncalibrated ADC path's ~5% under-read. Document both together. Divider keeps a 16V fault at 2.44V — safe | Firmware: **comment-only** at `v4.1.0.ino:78`, `:286`, `:561` and the five sibling files — record both ratios and warn that switching to `analogReadMilliVolts` requires re-calibrating to ~6.556. `esp32_contract.md:266` (§6a rewrite: measurement is pre-diode, drop "narrowest calibration window"); `:294` close gap 9; `DOC_UPDATES_PENDING.md` B4 (simplify, add a bench item timing `power=FAIL` — ~20s via the 470µF/118k RC); KiCad R23, R24, C1 values | Pin map ("24V/12V" -> 14.5V pre-diode on GPIO36); power architecture (14.8V -> 14.5V); BOM / PCB constraints |
| **C6** | Gateway **does** implement 45001+, fully and unconditionally. 8 registers per node at `45001 + (N-1)*8`. 4-state maps OFF=0 / ON=1 / FAIL_OPEN=2 / FAIL_SHORT=3 / unknown=99; battery and actual-state use 65535 for unknown. Wire address = register − 40001, **no off-by-one**. **The HMI does not read it** and does not need to — MQTT is richer | `Gateway/bench/scada_probe.py:9` (docstring says 7 registers, code uses 8) | HMI spec (add the missing 42001 and 45001 rows — copy the correct table from `hmi_contract.md:25-35`; state explicitly that the HMI does not read 45001+); architecture doc (add the layout, and flag that **battery sits at +4**, not after the four current channels) |
| **C7** | HMI's OK/FAIL expectation is obsolete — fixed 2026-07-27, all three HMI files handle the 4-state strings correctly. Static Zones 1 and 2 are displayed everywhere; "Load 3/4" retired. **The real issue is the reverse:** `current1`/`current2` are now **2-state intent-echo** (`ON`/`OFF` only), so `FAIL_SHORT` is unreachable on every channel and `FAIL_OPEN` only comes from the static zones. Arrow diagnostics were implemented and then deliberately deferred to **Phase 3** — readings fluctuated too much during animation to be trusted; an all-arrows-on self-test gives a steady reading | `esp32_contract.md` §3 (lines 99-146) and `SCADA_Integration_Guide.md` §5.1 — both still promise 4-state measured health on the arrows. **Highest-priority item in this review**: as written, a SCADA integrator would build an arrow `FAIL_OPEN` alarm that can never fire. Also `esp32_contract.md` §7, `SCADA_Integration_Guide.md` §5.5 and `Embedded/README.md` 3.8 still say battery is never published | HMI spec (delete OK/FAIL; rename Load 3/4 -> Static Zone 1/2); architecture doc (same rename) |

### The three things that matter most

1. **C7 — arrow health is not monitored today, but two contracts say it is.** A SCADA integrator
   building to `esp32_contract.md` §3 or `SCADA_Integration_Guide.md` §5.1 would believe the moving
   arrows are current-verified. They are not; `current1`/`current2` only echo the command. The
   *behaviour* is a deliberate, well-reasoned deferral to Phase 3 — it is the *documentation* that
   is wrong. This is the one finding with a safety consequence.
2. **C4 — the GPIO0 entry is a build hazard.** The board is fine (GPIO0 is unconnected), but anyone
   wiring a new board from the pin map as written would put a 50 MHz clock on a strapping pin and
   get a board that will not boot reliably.
3. **Unvalued schematic symbols are the root cause of C2, C3 and C5 — but the board prints the
   values.** R21–R26 and C1 carry no value on the schematic symbols, yet the silkscreen shows
   `100k`/`18k` twice, `4.7k` twice and `470u`. The board has been right all along; the schematic
   never caught up. Copying those values onto the symbols is the single highest-leverage fix for
   the design files, and it removes the ambiguity that let three documents disagree.

### Minor open conflicts — found during this review, still to be addressed

**These are minor.** None of them is a safety issue and none blocks the handover. They are
recorded here because they are all cases where a document describes behaviour the code no longer
has, and each one will mislead the next person who reads it. Listed in rough order of how likely
they are to cause confusion.

| # | Conflict | Where | Fix |
| --- | --- | --- | --- |
| **M1** | **Chase animation does not match the contract.** `esp32_contract.md:93` specifies frames `{0x07, 0x0E, 0x1C, 0x19, 0x13}`, 5 frames at **300 ms**, *"each frame lights exactly 3 of 5 strips"*. Firmware `v4.1.0.ino:211-213` uses `{0x18, 0x0C, 0x06, 0x03, 0x11}` at **250 ms**, lighting **2 of 5** per frame with an intentional wrap frame. Completely different animation | `esp32_contract.md:93` vs `v4.1.0.ino:211-213` | Update the contract to the shipped Chase-1 2-LED wrapping pattern (A4->A0). The spec is already written up at `Workbench/Firmware_Test/demo_mode_v2.0.0/CHASE_ANIMATION_SPEC.md` |
| **M2** | **Dynamic threshold table is built on the old 3-of-5 chase.** `esp32_contract.md:177-188` scales expected current as `THRESH_PER_STRIP * (n - 0.5)` with chase = 3 strips (`x 2.5`). Firmware calls `setThresholds(2, ...)` for chase modes (`v4.1.0.ino:487-489`) because the chase is now 2 strips. Largely moot while the arrows are intent-echo, but it will matter again at Phase 3 | `esp32_contract.md:177-188` vs `v4.1.0.ino:484-491` | Correct the table to 2 strips per chase frame. Flag that it is dormant until Phase-3 arrow measurement returns |
| **M3** | **ACS712 part selection has changed.** `esp32_contract.md:60` recommends *"5 A parts on the arrows, 20 A parts on the static zones"* and states the defaults are `SENS_STA1/STA2 = 0.100`. Firmware `v4.1.0.ino:68-75` says **all four are 5 A parts**, `SENS = 0.185` on every channel | `esp32_contract.md:51-62` vs `v4.1.0.ino:68-75` | Update the contract to all-5A. **This also appears to close bench item B2** (`DOC_UPDATES_PENDING.md` §0 and `Connections.md` §7.1), which asks what parts are fitted — confirm against the physical markings, then close B2 and update `Connections.md` §4's "Part" column |
| **M4** | **Serial menu documented but removed.** `esp32_contract.md:315-331` documents a 9-key commissioning menu (`h i r s c g v m p`) and a `v -> c -> g -> p` calibration workflow. Firmware `v4.1.0.ino:632-641` has only **`h i s b`**; auto-calibration was deliberately removed in v4.0.0-p1 and returns in Phase 2 with NVS | `esp32_contract.md:315-331` vs `v4.1.0.ino:632-641` | Rewrite §9 to the four shipped keys. Note that calibration is edit-and-reflash until Phase 2 |
| **M5** | **Battery still documented as never published.** Three documents say the firmware deliberately publishes nothing for battery and the gateway always reports `65535`. It now publishes coarse `0` / `50` / `100` | `esp32_contract.md` §7, `SCADA_Integration_Guide.md` §5.5, `Embedded/README.md` item 3.8 | Update all three. Already carried as a change item under **C7** |
| **M6** | **Buck numbering disagrees between two repo docs.** `Firmware_Test/README.md:49-56` numbers them #1 = Static Zone 1, #2 = Static Zone 2, #3 = arrows, #4 = 5 V. `Connections.md:12-17` has Buck 1 = arrows, Buck 2 = Static Zone 1, Buck 3 = Static Zone 2, Buck 4 = 5 V | `Firmware_Test/README.md:49-56` | Renumber to match `Connections.md` and the silkscreen. Already carried as a change item under **C1** |
| **M7** | **`scada_probe.py` docstring says 7 diagnostic registers**; the code uses 8 (`:21`) | `Gateway/bench/scada_probe.py:9` | One-word docstring fix. Already carried under **C6** |
| **M8** | **Folder `Frimware_v3.0.0/` contains v4.0.0-p1 content, not v3.0.0.** The header inside reads `Firmware v4.0.0-p1`. Genuine v3.0.0 is only recoverable from git (`4fa8299:Embedded/ESP32/Firmware/Firmware.ino`). Also a spelling slip in the folder name | `Embedded/ESP32/Firmware/Frimware_v3.0.0/` | **Owner-owned — do not change unprompted.** `Workbench/HANDOFF.md` records that versioning and folder organisation belong to the owner. Flagged only |
| **M9** | **Schematic symbols carry no component values** while the silkscreen prints them (`100k`, `18k` ×2, `4.7k` ×2, `470u`). This is the common root cause of C2, C3 and C5 | `PCB_V3_Final.kicad_sch`, `PCB_V3.1.kicad_sch` — R21–R26, C1 | Copy the silkscreen values onto the schematic symbols. Highest-leverage single fix to the design files |

### Bench items — cannot be answered from the repository

These are not conflicts. They are facts the files genuinely do not contain.

| # | Question | Why the repo cannot answer it |
| --- | --- | --- |
| **B-a** | Does the J15 / J16 socket pin order match the LAN8720 module's own pinout? | The connectors are generic `Conn_01x07_Pin` symbols with unnamed pins, and the module is not in the repo. If Ethernet works on the built boards, it is correct — but it cannot be proven from the files. **(C4)** |
| **B-b** | How long does `power=FAIL` actually take to appear after the PSU is cut? | Calculated at roughly 20 s from the 470 µF / 118 k discharge path, but never measured. A PSU with its own output bleeder will be faster. **(C5)** |
| **B-c** | Does the HMI behave correctly against real hardware? | The 4-state handling and both new `hmi_light.py` / `hmi_dark.py` builds are `py_compile`-clean but **have never been run on device or against a live gateway**. Test plan exists and is unexecuted at `hmi_contract.md` §5.1. **(C7)** |
| **B-d** | Which physical arrow position does each of A0–A4 / B0–B4 drive? | Recorded nowhere. Open as B1 in `DOC_UPDATES_PENDING.md` §0 and `Connections.md` §7.2. Needed before the chase sweep direction can be confirmed |
| **B-e** | Are MCP23017 pins A5–A7 / B5–B7 genuinely unconnected? | Open as B7. The `0xA0` health-pattern readback depends on it; anything wired there causes a permanent false `FAULT` |

### Document authority changed — 2026-10-23, done

**Decision:** `esp32_contract.md` is demoted to a historical record;
`Workbench/Firmware/v4.1.0/firmware_doc.md` becomes the authoritative ESP32 reference.

**Why.** Every one of the seven conflicts resolved the same way — the code and the board were
right, the documents were stale. Four of the nine minor items (M1–M4) were in `esp32_contract.md`
alone. It was written in July 2026 as a specification for firmware that did not yet exist, became
a description of v3.0.0, and then v4.x moved past it.

**The promotion was not a header swap.** `v4.1.0/firmware_doc.md` was a 108-line *delta* document
covering only what changed from v4.0.0-p1, and its baseline chain was broken:

```
v4.1.0/firmware_doc.md  ->  v4.0.0/firmware_doc.md  ->  Frimware_v3.0.0/firmware_doc.md
                                                        (a near-duplicate of v4.0.0's — circular)
                                                     ->  Firmware/firmware_doc.md
                                                        (DOES NOT EXIST)
```

So the working tree contained **no complete firmware reference at all** outside the stale contract.
Promoting the delta as-is would have left nothing authoritative for the pin map, the MQTT contract,
command decode, the MCP health pattern, static-zone semantics or the network failsafe.

`v4.1.0/firmware_doc.md` was therefore made **self-sufficient**. Seven new sections (§8–§15) absorb
what the contract held, each re-verified against `v4.1.0.ino` rather than copied forward, with
every divergence from the old contract called out inline. It is now 15 sections and depends on no
other build's document.

**Files changed (documentation only — no code, no design files):**

| File | Change |
| --- | --- |
| `Embedded/ESP32/Workbench/Firmware/v4.1.0/firmware_doc.md` | **Promoted.** Added §8 pin map, §9 command decode + chase, §10 MCP write-verify, §11 telemetry contract, §12 thresholds + settle, §13 network failsafe, §14 PSU detection, §15 bench items. Authority banner at the top |
| `Embedded/ESP32/esp32_contract.md` | **Demoted.** Banner at the top with a table of the eight places it is now known to be wrong, and an instruction not to patch them here. Original text kept unaltered below it |
| `Embedded/README.md` | Authoritative-docs table now lists v4.1.0 `firmware_doc.md`; the contract is marked superseded. Pin-map pointer repointed |
| `PCB/PCB_V3/Connections.md` | Pin-map pointer repointed to §8 of the new reference |
| `Embedded/RaspberryPi/HMI/hmi_contract.md` | Companion-doc line repointed |
| `Embedded/RaspberryPi/Gateway/SCADA_Integration_Guide.md` | Companion-doc line repointed |
| `Embedded/ESP32/Workbench/Firmware/v4.0.0/firmware_doc.md` | Marked superseded; circular baseline link replaced |
| `Embedded/ESP32/Firmware/Frimware_v3.0.0/firmware_doc.md` | Marked superseded; dead baseline link replaced; mislabelled-folder warning added (**M8** — flagged, folder not renamed) |
| `Embedded/ESP32/Workbench/HANDOFF.md` | "AUTHORITATIVE ESP32 contract" line repointed; contract listed as superseded |
| `Embedded/ESP32/Workbench/Docs/Phase1-Part1_Completion-Report.md` | "Authoritative contract" row repointed |

All twelve new relative links were checked and resolve.

**Effect on the minor-conflicts list above:** **M1, M2, M3 and M4 are now resolved in substance.**
The correct behaviour is documented authoritatively in §9, §12, §8 and §4 respectively, and the
contract's wrong versions are flagged in its demotion banner. They remain listed above only as a
record of what was found.

**Still to do — not part of this change:**

* `SCADA_Integration_Guide.md` §5.1 still describes the arrows as measured 4-state (**C7**). The
  demotion does not fix it, because that file is independently authoritative for SCADA.
* `SCADA_Integration_Guide.md` §5.5 and `Embedded/README.md` item 3.8 still say battery is never
  published (**M5**).
* The `.ino` headers still carry `Contract : ESP32/esp32_contract.md`. Source-comment change,
  deliberately not made.

---

### Stale-pointer cleanup — 2026-10-23, done

Three pointers were outstanding after the authority change. Fixing them surfaced **five more
copies of the same two stale claims**, found by grepping the whole repo rather than trusting the
original list. All are now corrected.

**1. Arrow health described as measured 4-state** (the C7 item — the one with a safety consequence):

| File | Change |
| --- | --- |
| `Gateway/SCADA_Integration_Guide.md` §5.1 | New subsection: a per-register table showing `+2`/`+3` can only produce `0` or `1`, the reason arrow diagnostics were withdrawn, and three explicit consequences for the PLC program — `FAIL_SHORT` unreachable everywhere, `FAIL_OPEN` only from the static zones, arrow health not monitored today. `2`/`3` kept in the mapping for Phase 3 |
| `Gateway/SCADA_Integration_Guide.md` quick-reference card | Added the intent-echo warning to the ALARMS block |
| `Gateway/SCADA_Integration_Guide.md` known-limitations table | New row 3a |
| `RaspberryPi/pi_agent.md` §telemetry table | `current1`/`current2` rows corrected, plus an explanatory note before the `state` semantics |
| `RaspberryPi/HMI/HMI_doc.md` | Telemetry row corrected |

**2. Battery described as never published** (M5, plus four copies the original list missed):

| File | Change |
| --- | --- |
| `Embedded/README.md` item 3.8 | Rewritten: live since v4.1.0, coarse `100`/`50`/`0`, `50` is not "half full", `65535` still means unknown |
| `Gateway/SCADA_Integration_Guide.md` §5.5 | Retitled and rewritten with the band table and a warning not to render it as a percentage bar |
| `Gateway/SCADA_Integration_Guide.md` §5.7, commissioning step 14, quick-reference card, limitations row 3 | All four updated |
| `Gateway/Gateway_doc.md` | Battery paragraph rewritten |
| `RaspberryPi/pi_agent.md` | `battery_pct` row rewritten |
| `RaspberryPi/HMI/hmi_contract.md` §6 | Superseded banner; records that `HMI_new` shows the value while the older `HMI.py` still hardcodes `N/A` |
| `RaspberryPi/HMI/HMI_new/TECHNICAL.md` | "permanently greyed" corrected to live |
| `RaspberryPi/HMI/HMI_doc.md` | Telemetry row corrected |

**3. `.ino` header pointers** — six files
(`v4.1.0`, `v4.0.0`, `Node_1/2/3`, `Firmware_v3.0.0`): `Contract : ESP32/esp32_contract.md`
replaced with a pointer to the authoritative doc plus a superseded note, and the
"see esp32_contract.md §8" reference repointed.

> **Comment-only.** All 30 changed lines begin with ` * ` and sit inside each file's opening
> `/* ... */` header block, verified programmatically. **No code was altered.**
>
> **Not compiled.** The repo's `Embedded/bin/arduino-cli` is a **macOS arm64** binary and cannot
> run on the Windows host used for this review. The change is provably comment-only, but it has
> not been put through a compiler here. Compile before flashing.

**Found and deliberately NOT changed:**

* `Embedded/ESP32/esp32_contract.md` still contains both stale claims. **Correct** — it is the
  demoted historical record and its banner says not to patch them there.
* `RaspberryPi/HMI/hmi_contract.md` §5 bug-list row 13 still reads "Battery is stubbed". That
  table is explicitly a historical regression list from 2026-07-27, and §6 immediately below now
  carries the correction.
* `RaspberryPi/HMI/HMI.py` hardcodes `N/A` / `NOT FITTED` and will not display the live battery.
  It is superseded by the `HMI_new` builds, which do. **Code change — not made.** If `HMI.py` is
  still deployed anywhere, this is a real functional gap, not just a doc issue.

---

### Nothing else was found

Everything else checked during this review was consistent: the mode decode table (`0`–`6` plus the
`10000`–`11023` bitmask), the MQTT topic structure, the gateway's diagnostic translation, the
Modbus register map, the MCP23017 wiring and health pattern, the MOSFET gate network, and the
power tree. `Pi.py`, `HMI.py`, `hmi_light.py` and `hmi_dark.py` required no corrections at all.

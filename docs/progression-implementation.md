# Voyage progression — implementation contracts

Audit and schema freeze for epic `sub-hauler-7px`, task `sub-hauler-7px.1`
(2026-10-26). The accepted design is [progression.md](progression.md); hardware
and resource limits are canonical in [platform.md](platform.md). This document
turns the accepted design into contracts that the dependent tasks implement.
It is not a claim that the combined feature set fits: music and campaign/save
costs are separated into measured and projected below.

No production firmware, generated artifact or FX declaration was changed by
this audit.

## 1. Measured baseline (2026-10-26)

Commands and results from the shared tree:

```sh
make size-matrix            # full, lean, core, min
make mini PROFILE=full
make fxtest                 # six suites, each bare P
make fxtest FXTEST_ONLY=test_memory \
  RELAX_FLAGS="-mrelax -mcall-prologues -DSH_STACK_DIAG=1"
```

| Profile | Flash | Static RAM | `.text` | `.data` | `.bss` |
| --- | ---: | ---: | ---: | ---: | ---: |
| `full` | 20,176 B | 1,583 B | 20,120 | 56 | 1,527 |
| `lean` | 17,368 B | 1,571 B | 17,304 | 64 | 1,507 |
| `core` | 14,756 B | 1,313 B | 14,692 | 64 | 1,249 |
| `min` | 8,750 B | 1,252 B | 8,704 | 46 | 1,206 |
| Mini `full` | 20,176 B | 1,583 B | — | — | — |

Headroom: **9,520 B** hardware flash, **8,824 B** under the 29,000 B planning
ceiling, **117 B** static planning headroom, **977 B** static free SRAM.

The supported stack diagnostic invocation is the `test_memory` suite with the
define injected through the existing `RELAX_FLAGS` build-property hook (there
is no dedicated Make target):

```sh
make fxtest FXTEST_ONLY=test_memory \
  RELAX_FLAGS="-mrelax -mcall-prologues -DSH_STACK_DIAG=1"
```

Observed: `stack_reserve=756`, then `P`. Target is 600 B, so the measured margin
is 156 B. The same suite without diagnostics prints a bare `P`. All six device
suites (`test_boot`, `test_chart`, `test_exploration`, `test_memory`,
`test_render`, `test_world`) printed `P` in the full run.

Other gates in this audit: `make gen-check` printed
`gen-check: generated artifacts are deterministic`; host tests were
150/117/89/44 passed, 0 failed; tooling ran 105 tests, `OK`.

Current static owners in the full image (`avr-nm`): `sBuffer` 1,024 B,
`s_chart` 256 B, `s_state` 91 B, `s_cache` 28 B; the remaining 184 B is
Arduboy2/FX/USB/Serial/screen-tag state. `sizeof(game::State) == 91` is already
asserted in `tst/fxdatatest/test_memory.ino`.

## 2. Internal EEPROM ownership audit

The ATmega32u4 has 1,024 B of internal EEPROM (`E2END == 0x3FF`). The installed
Arduboy2 (`arduboy-homemade:avr 1.4.0`) defines
`EEPROM_STORAGE_SPACE_START == 16` and documents this system map:

| Address | Owner | Writer |
| --- | --- | --- |
| 0 | bootloader | Arduboy2 does not touch it |
| 1 | system flags (logo/name/LED bits) | `Arduboy2Base::writeShowBootLogoFlag`, `writeShowUnitNameFlag`, `writeShowLEDsWithBootLogoFlag` |
| 2 | audio on/off | `Arduboy2Audio::saveOnOff()` |
| 3–7 | reserved for future system use | none in this toolchain |
| 8–9 | unit ID (u16) | `Arduboy2Base::writeUnitID()` |
| 10–15 | unit name (6 bytes) | `Arduboy2Base::writeUnitName()` |
| 16–1,023 | sketch space | game |

Library audit result:

- Arduboy2 writes only the system prefix above; it never writes at or above 16.
- ArduboyFX has no EEPROM code.
- ATMlib2FX has no EEPROM code.
- The only other EEPROM user in the sketch is `src/chart_save.hpp`, which
  currently writes addresses **0–259**. That overlaps the whole system prefix
  and is not a safe allocation. The voyage save replaces it and must never
  write 0–15.
- Audio mute already has a system allocation at address 2. A MUSIC ON/OFF
  option must map to `arduboy.audio` / `saveOnOff()`; it must not add a
  settings block or a voyage field. Sound-effect allocation remains later work.

## 3. Voyage slot layout

Two 320-byte slots after the system prefix:

| Slot | Addresses | Notes |
| --- | --- | --- |
| A | 16–335 | overlaps legacy chart bytes 16–259 |
| B | 336–655 | does not overlap the legacy chart block 0–259 |
| free | 656–1,023 | 368 B unallocated |

Each slot: **16-byte header + 303-byte payload area + 1 commit byte**.

```text
slot +   0  magic 'S' 'H' 'V' 'Y'   (0x53 0x48 0x56 0x59)
slot +   4  schema version          = 1
slot +   5  world layout version    = world::LAYOUT_VERSION (3 now)
slot +   6  campaign content version = 1
slot +   7  feature/profile signature (bitfield, section 4)
slot +   8  sequence                u32 little-endian, starts at 1
slot +  12  payload length          u16 little-endian
slot +  14  CRC-16                  u16 little-endian
slot +  16  payload (used bytes only)
slot + 319  commit                  = 0x5A
```

Payload capacity is 303 B. Schema 1 uses 296 B with a chart (40 scalar + 256
chart) and 40 B without a chart. Bytes after the used payload are neither
written nor read in schema 1; they are reserved for a future schema.

The commit byte is deliberately outside the CRC. `0x5A` means committed;
`0x00` means invalid/in-progress. Any other value is invalid. A torn write can
therefore never validate.

CRC is **CRC-16/CCITT-FALSE**: polynomial `0x1021`, init `0xFFFF`, no
reflection, no final XOR. It covers header bytes 0–13 (including magic,
versions, signature, sequence and length) followed by exactly `payload_length`
payload bytes. It excludes the CRC field and the commit byte. Check value for
`"123456789"` is `0x29B1`; the storage tests must assert that vector.

## 4. Profile signature

One byte, fixed for schema 1:

| Bit | Feature |
| --- | --- |
| 0 | `SH_FEATURE_FOG_CHART` |
| 1 | `SH_FEATURE_SONAR` |
| 2 | `SH_FEATURE_MINES` |
| 3 | `SH_FEATURE_SHOP` |
| 4 | `SH_FEATURE_HULL_DAMAGE` |
| 5 | `SH_FEATURE_MULTI_HULL` |
| 6 | `SH_FEATURE_EXPLORATION` |
| 7 | reserved, 0 |

A slot whose signature differs from the running build is **incompatible**, not
corrupt. Cross-profile loading is rejected until an explicit migration exists.
This means full↔lean and full↔core saves do not interchange; the UI must say so
before offering NEW VOYAGE. `SH_FEATURE_MUSIC` is not in the signature because
music has no saved field.

Chart presence is derived from bit 0, not stored separately. Expected payload
length is `40 + (bit0 ? 256 : 0)`.

## 5. Scalar payload schema (40 B)

Little-endian, fixed order, no padding, no raw C++ struct bytes.

| Offset | Bytes | Field | Width / rule |
| ---: | ---: | --- | --- |
| 0 | 1 | berth | 0–5 |
| 1 | 1 | hull class | 0–2 |
| 2 | 2 | credits | u16 |
| 4 | 1 | air level | 0–3 |
| 5 | 1 | hull level | 0–3 |
| 6 | 1 | sonar level | 0–3 |
| 7 | 1 | tools | CUTTER/SEAL/THRUSTER/LENS bits |
| 8 | 1 | pickups | 8 claim bits |
| 9 | 2 | open gates | 9 bits |
| 11 | 2 | seen gates | 9 bits |
| 13 | 1 | campaign milestones | 5 bits |
| 14 | 1 | surveys | 2 bits |
| 15 | 1 | visited settlements | 4 bits |
| 16 | 1 | active job kind | 0–3 |
| 17 | 1 | active mission ID | 0–6, or 0xFF |
| 18 | 1 | job origin berth | 0–3, or 0xFF |
| 19 | 1 | job destination berth | 0–3, or 0xFF |
| 20 | 1 | job load | 4 for cargo jobs, else 0 |
| 21 | 1 | job reward | credits, multiple of 5 |
| 22 | 2 | job deadline | flight ticks, 0 = untimed |
| 24 | 2 | job age | flight ticks, saturating |
| 26 | 2 | offer cursor | pre-board generator serial |
| 28 | 2 | board seed | serial origin, schema 1 = 0 |
| 30 | 1 | armed mines | 8 bits |
| 31 | 4 | voyage ticks | u32, saturating |
| 35 | 2 | delivered jobs | u16, saturating |
| 37 | 2 | rescue count | u16, saturating |
| 39 | 1 | ending presented | 0/1 |

Chart bytes follow at payload offset 40–295 when bit 0 of the signature is set.
Scalar total is exactly 40 B; this replaces the design's provisional
"seven reserved bytes" inside the scalar block with seven unused slot bytes
after the chart instead. The design's 40-byte scalar budget is unchanged.

Semantic validation, in addition to CRC and header checks:

- berth 0–5; berths 4 and 5 require their restoration milestone (section 8).
- hull class within the roster; levels 0–3; levels must be 0 when SHOP is off.
- tools/claims/gates/milestones/surveys/visits/mines only within their masks.
- milestones are monotonic: BEACON requires PUMP and LIFT; HOME requires
  BEACON; ending-presented requires HOME.
- job kind NONE: mission/origin/destination are 0xFF, load/reward/deadline/age
  are 0.
- campaign job: mission 0–6, origin/destination match the mission table,
  load 4, reward equals the table reward, deadline 0, age 0.
- ordinary job: mission 0xFF, origin/destination 0–3 and different, load 4,
  reward 5–250, deadline 0, age 0.
- express job: as ordinary but deadline equals the generated route deadline
  and age is 0–0xFFFF.
- `voyageTicks`, delivered and rescue counts are unsigned saturating values.
- A checkpoint at a service berth may carry an active job; service berths
  refill, repair and save loaded cargo.

Any failed semantic check invalidates that slot as a whole. It is not partially
applied.

## 6. Commit, selection and interrupted writes

Save transaction (freeze simulation, pause music, ignore departure input):

1. Choose the target slot: the invalid slot if exactly one is valid; otherwise
   the slot with the lower sequence; if both are invalid, **slot B first**.
   Slot A is never the first-ever target because it overlaps the legacy chart.
2. Write commit `0x00` to the target slot.
3. Stream the payload from live state with `EEPROM.update()`. No second
   chart-sized RAM buffer is allocated.
4. Compute the CRC while streaming, then write schema/layout/content/signature/
   sequence/length, then the CRC, then the magic, then commit `0x5A`.
5. Re-read and fully validate the target slot.
6. Only after a successful read-back report `SAVED`, resume music and unfreeze.
   On failure keep the other slot intact and offer `RETRY` or
   `CONTINUE UNSAVED`; never report success optimistically.

Sequence is `newest valid sequence + 1`; the first save is 1. Wrap comparison
is `(int32_t)(a - b) > 0`. Boot selects the newest slot that is committed,
compatible, checksum-valid and semantically valid.

Blank vs incompatible: a slot is blank when its magic is not `SHVY` and its
commit is not `0x5A`. If either slot carries the magic or commit but neither
validates, show an incompatible/corrupt-save message before NEW VOYAGE and do
not erase either slot until the player explicitly replaces it.

Load validates the header, CRC and semantic fields in a first streaming pass,
then decodes directly into initialized state. It never copies a raw struct and
never allocates a second chart-sized buffer. Because the commit byte is the
last write, an interrupted delivery save recovers either the complete
pre-delivery checkpoint (cargo active, no payment) or the complete
post-delivery checkpoint (cargo cleared, payment and milestone set); it cannot
combine them. Replacing a voyage writes a newer checkpoint into the
older/inactive slot and never erases both valid slots first.

Worst case write is 313 bytes (16 header + 296 payload + commit). At the AVR's
~3.3 ms per byte that is about 1.03 s, on a transition only. `update()` skips
unchanged bytes, but commit invalidation and commit set are always two writes.

No writes occur for pickups, sonar, chart closing, per-frame autosaving or
music progress. Developer mode never reads or writes voyage slots.

## 7. Legacy chart migration

The legacy chart block is addresses 0–259: `'S' 'H'`, version, CRC-8 over 256
explored bytes, then 256 bytes. It is not a voyage.

- Read it only before the first voyage slot exists.
- It is importable only when magic, `world::LAYOUT_VERSION` and CRC-8 all
  validate.
- On NEW VOYAGE, offer to keep the chart knowledge. Accepting copies the 256
  chart bytes into the new payload's chart field and defaults everything else:
  no tools, claims, credits, milestones, surveys or equipment.
- Declining starts with an empty chart.
- The first voyage is written to slot B. Slot A may only be used after a valid
  voyage exists. Never write addresses 0–15 and never reconstruct system
  settings from chart bytes.
- Standalone chart save and voyage save are mutually exclusive at compile time:
  `SH_FEATURE_SAVE_VOYAGE` requires `!SH_FEATURE_SAVE_CHART`.

## 8. Stable campaign IDs

Mission IDs (`kMissionNone = 0xFF`):

| ID | Name | Route | Reward | Completion |
| ---: | --- | --- | ---: | --- |
| 0 | FIRST | Tidewall → Benthos | 145 | milestone FIRST |
| 1 | PUMP_SURVEY | Pump Refuge survey | 0 | survey PUMP |
| 2 | PUMP | Cormorant → Benthos | 85 | milestone PUMP |
| 3 | LIFT_SURVEY | Freight Yard survey | 0 | survey LIFT |
| 4 | LIFT | Benthos → Cormorant | 85 | milestone LIFT |
| 5 | BEACON | Cormorant → Deep Anchor | 155 | milestone BEACON |
| 6 | HOME | Deep Anchor → Tidewall | 120 | milestone HOME |

The rewards match the generated ordered-pair table
(`ROUTE_REWARD[0][1] = 145`, `[2][1] = 85`, `[1][2] = 85`, `[2][3] = 155`,
`[3][0] = 120`). Campaign work pays the ordinary route reward once and never
also resolves an ordinary freight reward.

Survey IDs: `SURVEY_PUMP = 0` (Pump Refuge), `SURVEY_LIFT = 1` (Freight Yard).
Both rings exist from voyage start; their clues appear after FIRST. A recorded
survey satisfies the request immediately and is never demanded again.

Milestone bits: `FIRST=0`, `PUMP=1`, `LIFT=2`, `BEACON=3`, `HOME=4`.
Survey bits: `PUMP=0`, `LIFT=1`. Visited bits: one per freight settlement 0–3.

Berth IDs: Tidewall 0, Benthos 1, Cormorant 2, Deep Anchor 3,
Pump Refuge 4, Lower Gantry 5. Berths 0–3 are the only freight destinations;
4–5 are service-only and must never appear in an offer destination or the
route economy. `SETTLEMENT_COUNT` stays 4; a generated `BERTH_COUNT` is 6.

Restoration eligibility:

- Pump Refuge (4) becomes dockable and checkpoint-valid only after milestone
  PUMP; Lower Gantry (5) only after LIFT.
- Before restoration the mooring is visible but inactive and shows a clue.
- Service berths refill air, repair the hull and save the voyage with cargo;
  they do not generate freight, accept deliveries or sell upgrades.
- Both new berths need authored safe approaches and generated service metadata
  (pixel position plus required milestone), produced by `make gen`. Their
  placement must pass the same hull-clearance, escape-column and reachability
  checks as the four existing berths.

Mission eligibility:

- FIRST is available at Tidewall from voyage creation; the board selects it but
  does not auto-accept.
- PUMP requires FIRST and PUMP_SURVEY; source Cormorant.
- LIFT requires FIRST and LIFT_SURVEY; source Benthos.
- BEACON requires PUMP, LIFT and the pressure seal; source Cormorant.
- HOME requires BEACON; source Deep Anchor; explicit acceptance, never
  auto-loaded.
- Completed missions never reappear. Abandoning clears cargo and the active
  job, saves, and lets the source reissue the same mission for free.

## 9. Job kinds, ordinary and express semantics

`ActiveJob` gains explicit kind/mission/origin fields. Freeze it at 10 bytes:

```cpp
struct ActiveJob {
    uint8_t kind;      // NONE=0, ORDINARY=1, EXPRESS=2, CAMPAIGN=3
    uint8_t mission;   // campaign id or kJobNone
    uint8_t from;      // origin berth
    uint8_t to;        // destination berth
    uint8_t crates;    // 4
    uint8_t reward;    // credits
    uint16_t deadline; // flight ticks; 0 = untimed
    uint16_t elapsed;  // flight ticks, saturating
};
```

`hasActiveJob()` becomes `kind != kJobNone`. One cargo job is active at a time;
surveys and tool discoveries do not occupy the slot.

- **Ordinary**: no deadline (`deadline = 0`), pays the route reward on loaded
  arrival at the destination. Age stays 0.
- **Express**: pays `round5(125% of route reward)` and uses the existing route
  deadline. Freeze the integer formula
  `((reward * 5 + 10) / 20) * 5`; with the current maximum reward 165 the result
  is 205, inside the uint8 reward width. Age advances only in flight; docking,
  chart and pause do not advance it. It saturates at `kJobElapsedLate`; a
  saturated age is permanently overdue. Expiry is evaluated at docking: clear
  the job and cargo with an `EXPRESS LATE` notice, no fine, no payment.
- **Campaign**: `deadline = 0`, age 0, pays the table reward once. Wrong-berth
  stops retain the job and load; only the matching destination resolves it.
  Short or missing cargo cannot set completion or pay.
- Accepting any job auto-loads four units, neutralises trim and leaves the boat
  docked. UNDOCK or B starts travel.
- Two freight rows: row 0 ordinary, row 1 express, with the existing duplicate-
  destination retry. Campaign work lives on the WORK page, not the offer rows.
- WORK replaces the redundant SERVICE row: the no-job board is
  `[ordinary] [express] WORK [SHOP] UNDOCK`; a build without SHOP omits SHOP.
  The active-job board keeps UNDOCK initially and ABANDON behind its own
  confirmation.

## 10. Deterministic offers and cursor restoration

The existing generator is kept: `jobs::offerKey(fromId, serial)` and the
12-attempt duplicate guard. Offers are a pure function of the origin berth, the
serial cursor and the saved station-availability inputs (seal ownership).

Freeze:

- `state.offerCursor` is the serial value **before** the current station board
  was generated. `jobs::refreshOffers()` records it before its first
  `makeOffer()` and then advances the working serial as today.
- `state.boardSeed` is the serial origin for the voyage. Schema 1 writes 0, so
  the current offer sequence is preserved exactly. It is stored for forward
  compatibility; changing it is a schema/campaign-content change.
- Offers are not serialized. While a job is active the board shows the active
  summary, so the two rows are irrelevant. Abandon regenerates from the current
  cursor. On load at a berth with no active job, restore `offerSerial` from
  `offerCursor` and regenerate; the same two rows reappear without advancing
  the saved cursor.
- Express age at an intermediate berth is saved and restored unchanged.
  Retrying from the last berth intentionally rolls the express clock back with
  the rest of the checkpoint.

## 11. Campaign state additions

Proposed additions to `game::State` (AVR byte-packed, all fields also mapped to
the payload above):

| Field | Width | Notes |
| --- | ---: | --- |
| `campaign` | 1 | milestone bits |
| `surveys` | 1 | survey bits |
| `visited` | 1 | settlement visit bits |
| `voyageTicks` | 4 | u32 saturating |
| `delivered` | 2 | u16 saturating |
| `rescues` | 2 | u16 saturating |
| `boardSeed` | 2 | u16 |
| `ending` | 1 | 0/1 |

`ActiveJob` grows from 6 to 10 B (`kind`, `mission`, `from` added), and
`offerSerial` is renamed `offerCursor` with no width change. Net projected
static growth is **17 B** for campaign state, plus a small number of sketch
transients (region byte, hysteresis tick, current cue). Budget the campaign at
roughly **20–25 B**, not a measured number yet.

## 12. Feature flags and profiles

Proposed dependencies (to be added to `src/features.hpp` and
`tools/features.py`):

- `SH_FEATURE_CAMPAIGN` requires JOBS, STATIONS and EXPLORATION.
- `SH_FEATURE_SAVE_VOYAGE` requires CAMPAIGN and `!SH_FEATURE_SAVE_CHART`.
- `SH_FEATURE_MUSIC` has no state dependency; it requires the ATMlib2FX
  library and generated music symbols.
- Standalone `SH_FEATURE_SAVE_CHART` remains available when voyage save is off.

Profile contract:

| Profile | Campaign | Voyage save | Music |
| --- | --- | --- | --- |
| full | 1 | 1 | 0 until the measured combined-fit gate passes |
| lean | 1 | 1 | 0 |
| core | 1 | 1 (no chart, 40 B payload) | 0 |
| min | 0 | 0 | 0 |

Core has no chart, sonar, shop or alternate hull roster. Every required survey
and route must work there with the base boat and visual clues. Dev mode keeps
its current flight-only behavior and suppresses voyage reads/writes.

## 13. Music integration contract and measured cost

### Measured cost

A bounded AVR probe was added at
`tst/fxdatatest/probe_music/probe_music.ino`. It is a build-only sketch in a
subdirectory, so `make fxtest` never picks it up. The same sketch was compiled
twice with the shipping flags, `--library ../ATMlib2FX`,
`-DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=64`, differing only in
`-DPROBE_MUSIC=0/1`:

| Variant | Flash | Static |
| --- | ---: | ---: |
| no music | 5,858 B | 1,197 B |
| music | 9,518 B | 1,425 B |
| **marginal** | **+3,660 B** | **+228 B** |

The 228 B static delta is confirmed by `avr-nm` symbols in the same build:
`atm_fx_compact_state` 103, `channels` 84, `osc_params_array` 16,
`osc_pha_acc_array` 8, `osc_cb` 12, `atmlib_state` 3, `osc_isr_reenter` 1,
`osc_int_count` 1 = 228 B. The 64-byte ring is therefore only 64 of 228 B; the
remaining 164 B is decoder, channel, oscillator and timer state. The separate
jukebox image (13,208 B / 1,512 B) includes UI and song metadata and does not
prove this game fits.

### Demonstrated static deficit

Full has **117 B** static headroom. Music alone needs **228 B**. That is a
**111 B static deficit** if MUSIC is enabled in full, before any campaign state.
Adding the projected 17 B campaign state gives roughly a **128–132 B deficit**.
Flash is not the immediate blocker: 20,176 + 3,660 = **23,836 B**, leaving
5,164 B under the 29,000 B planning ceiling for campaign/save code. That flash
projection is isolated-probe arithmetic, not a whole-game measurement; LTO can
move it either way.

Consequences:

- Full MUSIC must default to **0** until a measured combined image passes the
  1,700 B static ceiling and the 600 B stack target. This is the design's
  stated fallback, not a silent gameplay cut.
- `.7` must first measure the actual combined image, then either carve at least
  ~135 B of static state or leave MUSIC off. Raising the ceiling is not an
  option.
- Plausible carves are small: particle pool 16 B (only by dropping debris),
  camera 4 B (derive from position), fractional carries 3 B, transient contact/
  discovery bytes 5 B, dock/impact cooldowns 3 B. These do not reach 135 B
  together. The 256 B chart is the only large block and cannot be removed
  without losing fog. A channel/effect reduction would need listening tests and
  is not a free byte source. Report the exact measured deficit before choosing.

### Runtime contract

- Compile with `ATM_FX_COMPACT_RING=64`, `ATM_FX_COMPACT_LOOP=1` (streaming
  defaults on) and all effects enabled.
- `atm_fx_compact_fetch(offset, dst, n)` calls
  `FX::readDataBytes((uint24_t)offset, dst, n)`; offsets are data-section-
  relative and the FX page is added exactly once.
- `atm_synth_setup()` once in `setup()` after `FX::begin(FX_DATA_PAGE)`.
- Play with `atm_fx_compact_play(sh_<cue>, music_data::sh_<cue>_bytes)` using
  the generated FX symbols and lengths.
- Call `atm_fx_compact_service()` after the preceding `FX::display(...)` and
  before the next render. Service idle station/shop frames even when no OLED
  transaction occurs. The timer consumer must never read FX.
- Chart and pause pause the player (`atm_fx_compact_pause()`) and resume in
  place; station/shop idle frames keep playing; wreck/rescue confirmation is
  silent and resumes on cancel; saving pauses during the EEPROM write and
  resumes after success. Same-cue entries do not restart; different cues
  cut/restart.
- Audio mute uses the Arduboy2 system setting at EEPROM address 2. MUSIC OFF
  stops playback and prevents starts; it is not a voyage field.
- Cue IDs: `HARBOR=0`, `SONAR=1`, `KELP=2`, `BUBBLEJET=3`, `ABYSSAL=4`,
  `TIDEPOOL=5`.
- Cue mapping: all three home boards and the new-voyage title → HARBOR;
  Lantern Reaches travel → SONAR; Kelp Galleries travel → KELP; Broken Works
  travel → BUBBLEJET; Glass Trench travel and unrestored Deep Anchor board →
  ABYSSAL; restored service berths, restored Deep Anchor board and ending →
  TIDEPOOL.
- Travel region comes from the authored `region` field per room, generated as
  a 32-byte PROGMEM table. Resolve the nearest room centre within its radius;
  ties use the lower room index; retain the previous region when no room
  matches. Evaluate at most once per 30 frames. A region must persist for
  120 flight ticks (2 s) before it replaces the cue.
- Verify the 100 Hz Bubblejet Run under real rendering, chart, transitions and
  idle menus; measure the worst refill window against the 64 B ring and the
  library's service deadline. Listen on the physical speaker before claiming
  audio acceptance.

## 14. Downstream ownership and gates

- `.2` pure campaign/untimed freight rules: use sections 8–10; extend
  `ActiveJob`, add kind/mission/origin, ordinary/express rewards, mission and
  survey state, WORK page rules; no save code.
- `.3` atomic two-slot storage: use sections 3–7 and 10; host-native tests for
  every interrupted-write boundary, torn header, corrupt newest slot, sequence
  wrap, incompatible signature/version, invalid field combinations, legacy
  chart import and NEW VOYAGE interruption; assert system bytes 0–15 are never
  written; assert CRC vector `0x29B1`.
- `.4` Continue/checkpoints/recovery: use sections 3, 6 and 9; commit on the
  listed transitions only; no midwater suspend.
- `.5`/`.6` campaign slices: use sections 8 and 11; generated service berth
  metadata; core-profile reachability and retreat/service stops.
- `.7` music: use section 13; combined measurement first; MUSIC stays off
  unless the measured static/stack fit passes.
- `.8` release verification: all canonical gates, profile/Mini builds,
  size-matrix, stack reserve at least 600 B, and explicit reporting of what was
  not manually or physically tested.

## 15. Unproven items

- Combined campaign + voyage save + music flash/static/stack fit is **not
  measured**. The music marginal cost is measured in isolation; campaign code
  and save code are projected only.
- Service berth placement, approach clearance and reachability are not authored
  or validated yet.
- Express reward/deadline tuning and the 90–150 minute voyage target are
  playtest targets, not measured.
- Physical speaker listening, manual playthroughs, soak tests and audio timing
  under combined load were not performed in this audit.

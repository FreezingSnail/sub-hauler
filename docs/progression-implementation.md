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

## 2. Save storage ownership

Voyage checkpoints live in the external Arduboy FX cart save partition, not in
the ATmega32u4's 1,024 B internal EEPROM. `fxdata/fxdata.txt` ends with
`savesection 4096`; `tools/fxdata-build.py` emits 4 KiB of erased `0xFF` bytes,
`FX_SAVE_PAGE` and `FX_SAVE_BYTES`, and `FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE)`
opens both partitions. The data prefix is unchanged, so every shipped FX symbol
keeps its flash offset. The development cart image (`fxdata/fxdata.bin`) carries
the save bytes for Ardens; a physical cart erases/programs the same pages.

Internal EEPROM remains audited for non-voyage settings only. The installed
Arduboy2 (`arduboy-homemade:avr 1.4.0`) defines
`EEPROM_STORAGE_SPACE_START == 16`:

| Address | Owner | Writer |
| --- | --- | --- |
| 0 | bootloader | Arduboy2 does not touch it |
| 1 | system flags (logo/name/LED bits) | `Arduboy2Base::writeShowBootLogoFlag`, `writeShowUnitNameFlag`, `writeShowLEDsWithBootLogoFlag` |
| 2 | audio on/off | `Arduboy2Audio::saveOnOff()` |
| 3–7 | reserved for future system use | none in this toolchain |
| 8–9 | unit ID (u16) | `Arduboy2Base::writeUnitID()` |
| 10–15 | unit name (6 bytes) | `Arduboy2Base::writeUnitName()` |
| 16–655 | unused by voyage save | none |
| 656 | MUSIC ON/OFF | `src/music.hpp` pause toggle |

Library audit result:

- Arduboy2 writes only the system prefix above; it never writes at or above 16.
- ArduboyFX contains no EEPROM code; all voyage bytes go through its SPI save
  API (`seekSave`, `eraseSaveBlock`, runtime `programSavePage` + page-program
  `seekCommand`/`writeByte`, `waitWhileBusy`).
- ATMlib2FX has no EEPROM code.
- The only remaining EEPROM user in the sketch is the optional standalone
  `src/chart_save.hpp` (mutually exclusive with voyage save). It is not part of
  a voyage checkpoint.
- Audio mute keeps the system allocation at address 2. The separate MUSIC
  ON/OFF byte remains at **656**, now the first byte above the retired voyage
  allocation. It is never part of a cart checkpoint, so Continue/rollback
  cannot change it; writes happen only on the pause-menu toggle.

## 3. Cart save layout

The save partition is one 4 KiB erase block (16 pages). It owns one 512 B slot
at offset 0:

| Slot | Cart offset | Erase block | Page numbers |
| --- | ---: | ---: | --- |
| only | 0–511 | 0 | 0–1 |

Each slot occupies 512 B: **16-byte header + 495-byte payload area + 1 commit
byte**. The remaining 3,584 bytes of the block stay erased and are not used.

```text
slot +   0  magic 'S' 'H' 'V' 'Y'   (0x53 0x48 0x56 0x59)
slot +   4  schema version          = 4
slot +   5  world layout version    = world::LAYOUT_VERSION (3 now)
slot +   6  campaign content version = 2
slot +   7  feature/profile signature (bitfield, section 4)
slot +   8  reserved                = 0xFF 0xFF 0xFF 0xFF
slot +  12  payload length          u16 little-endian
slot +  14  CRC-16                  u16 little-endian
slot +  16  payload (used bytes only)
slot + 511  commit                  = 0x5A
```

The slot spans two 256 B flash pages: page 0 carries the header and payload
bytes 0–239; page 1 carries payload bytes 240–494 and the commit byte (the last
byte programmed). Erased flash is `0xFF`, so an uncommitted slot has commit
`0xFF`, not `0x00`. `0x5A` means committed; any other value is invalid.

Payload capacity is 495 B. Schema 4 uses 292 B with a chart (36 scalar + 256
chart) and 36 B without a chart. Bytes after the used payload are programmed as
`0xFF`, matching erased flash, and are not read.

CRC is **CRC-16/CCITT-FALSE**: polynomial `0x1021`, init `0xFFFF`, no
reflection, no final XOR. It covers header bytes 0–13 (including magic,
versions, signature, reserved bytes and length) followed by exactly
`payload_length` payload bytes. It excludes the CRC field and the commit byte.
Check value for `"123456789"` is `0x29B1`; the storage tests assert that vector.

## 4. Profile signature

One byte, fixed for schema 4:

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
length is `36 + (bit0 ? 256 : 0)`.

## 5. Scalar payload schema (36 B)

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
| 22 | 2 | offer cursor | pre-board generator serial |
| 24 | 2 | board seed | serial origin, schema 4 = 0 |
| 26 | 1 | armed mines | 8 bits |
| 27 | 4 | voyage ticks | u32, saturating |
| 31 | 2 | delivered jobs | u16, saturating |
| 33 | 2 | rescue count | u16, saturating |
| 35 | 1 | ending presented | 0/1 |

Chart bytes follow at payload offset 36–291 when bit 0 of the signature is set.
Scalar total is exactly 36 B. Schema 3 deletes the two job-timer fields; the
payload still fits the frozen 495 B area.

Semantic validation, in addition to CRC and header checks:

- berth 0–5; berths 4 and 5 require their restoration milestone (section 8).
- hull class within the roster; levels 0–3; levels must be 0 when SHOP is off.
- tools/claims/gates/milestones/surveys/visits/mines only within their masks.
- milestones are monotonic: BEACON requires PUMP and LIFT; HOME requires
  BEACON; ending-presented requires HOME.
- job kind NONE: mission/origin/destination are 0xFF, load/reward are 0.
- campaign job: mission 0–6, origin/destination match the mission table,
  load 4, reward equals the table reward.
- ordinary job: mission 0xFF, origin/destination 0–3 and different, load 4,
  reward equals the generated route reward.
- no timed/express kind exists in schema 4. A pre-.27 schema 1 slot, a
  schema 2 slot and a dual-slot schema 3 cart are incompatible and are not
  migrated.
- Surveys are facts, not jobs: `recordSurvey()` accepts PUMP or LIFT before
  FIRST and the saved mask keeps either/both bits. Only accepting a survey
  mission from the WORK board requires FIRST; never reject an early recorded
  survey on load or save.
- `voyageTicks`, delivered and rescue counts are unsigned saturating values.
- A checkpoint at a service berth may carry an active job; service berths
  refill, repair and save loaded cargo.

Any failed semantic check invalidates that slot as a whole. It is not partially
applied.

### Schema 3 invalidation

Schema 4 keeps the single-slot cart layout but removes the dual-copy sequence
and fallback. Schema 1/2 EEPROM records, schema 3 cart records, old EEPROM bytes
and the retired standalone chart block are **incompatible or absent, not
migrated**. A player with an older record or a torn current record sees the
normal incompatible/corrupt message and may start a clean NEW VOYAGE; no old
job, reward, deadline, chart or elapsed age is preserved. Current schema 4 slots
still receive the full magic, length, CRC and semantic checks, so corruption is
never silently accepted or loaded.

## 6. Commit and interrupted writes

Save transaction (freeze simulation, pause music, ignore departure input):

1. Validate the live checkpoint and reject invalid state before any erase.
2. Erase the single 4 KiB cart block. This also removes the previous checkpoint.
3. Program page 0 (header, scalar, first 204 chart bytes), then page 1
   (remaining payload and filler), with commit `0x5A` as the last byte. No
   second payload or chart-sized RAM buffer is allocated.
4. Wait for each erase/page program to finish before starting the next one.
5. Re-read the slot, validate CRC/schema/payload semantics, and require the
   read-back CRC to equal the CRC just written.
6. Only after that check report `SAVED`, resume music and unfreeze. On failure
   offer `RETRY` or `CONTINUE UNSAVED`; never report success optimistically.

**This is not power-fail atomic.** An interruption during the erase or page
program may lose the previous checkpoint. The contract is weaker and explicit:
a torn record never validates, `load()` returns corrupt/incompatible/no-save
without mutating live state, and the title offers a clean NEW VOYAGE. There is
no second copy, sequence, newest-slot selection or fallback.

Blank vs incompatible: the slot is blank when its magic is not `SHVY` and its
commit is not `0x5A`. If it carries the magic or commit but does not validate,
show the corrupt/incompatible message and allow a clean NEW VOYAGE; do not load
it. Replacing a valid checkpoint still requires the title confirmation.

Load validates the header, CRC and semantic fields before decoding into
initialized state. It never copies a raw struct and never allocates a second
chart-sized buffer. Because the read-back must match the intended CRC, an
unchanged old record cannot be mistaken for a successful write.

Worst case erase is one 4 KiB block (~40 ms) plus two 256 B page programs
(~3.3 ms each), on a transition only. The 4 KiB partition is `FX_SAVE_BYTES`;
no per-frame autosave, pickups, sonar, chart closing, music progress or
developer traversal touches it.

## 7. No legacy chart migration

Schema 4 removes the old standalone chart migration completely. EEPROM
addresses 0–259 and pre-schema-4 cart slots are never inspected, offered,
copied or repaired. A NEW VOYAGE resets the chart, applies
`chart::revealInitial()` at the generated spawn berth, and persists that reveal
in the first checkpoint. There is no KEEP OLD CHART prompt and no legacy import
path. The single cart slot is erased and rewritten. Standalone chart save and voyage save remain mutually exclusive
at compile time: `SH_FEATURE_SAVE_VOYAGE` requires `!SH_FEATURE_SAVE_CHART`.

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

## 9. Job kinds and untimed freight semantics

`ActiveJob` uses explicit kind/mission/origin fields. Schema 2 freezes it at
6 bytes:

```cpp
struct ActiveJob {
    uint8_t kind;      // NONE=0, ORDINARY=1, CAMPAIGN=2
    uint8_t mission;   // campaign id or kJobNone
    uint8_t from;      // origin berth
    uint8_t to;        // destination berth
    uint8_t crates;    // 4
    uint8_t reward;    // credits
};
```

`hasActiveJob()` becomes `kind != kJobNone`. One cargo job is active at a time;
surveys and tool discoveries do not occupy the slot.

- **Ordinary freight**: untimed, pays the generated route reward on a loaded
  arrival at the destination. Accepting or abandoning refreshes the one
  repeatable freight row.
- **Campaign**: untimed, pays the table reward once. Wrong-berth stops retain
  the job and load; only the matching destination resolves it. Short or missing
  cargo cannot set completion or pay.
- No express kind, express premium, deadline, clock, late failure or expiry
  exists. Air and handling are the expedition pressure.
- Accepting any job auto-loads four units, neutralises trim and leaves the boat
  docked. UNDOCK or B starts travel.
- The station board is `WORK [SHOP] UNDOCK`; WORK opens one board with eligible
  story rows first and the freight row beneath. A accepts directly. The
  active-job board keeps UNDOCK initially and ABANDON behind its own
  confirmation, with WORK available for the carrying summary and clues.

## 10. Deterministic offers and cursor restoration

The existing generator is kept: `jobs::offerKey(fromId, serial)` and the
12-attempt duplicate guard. Offers are a pure function of the origin berth, the
serial cursor and the saved station-availability inputs (seal ownership).

Freeze:

- `state.offerCursor` is the serial value **before** the current station board
  was generated. `jobs::refreshOffers()` records it before its first
  `makeOffer()` and then advances the working serial as today.
- `state.boardSeed` is the serial origin for the voyage. Schema 3 writes 0, so
  the current offer sequence is preserved exactly. It is stored for forward
  compatibility; changing it is a schema/campaign-content change.
- Offers are not serialized. While a job is active the board shows the active
  summary. Abandon regenerates from the current cursor. On load at a berth with
  no active job, restore `offerSerial` from `offerCursor` and regenerate; the
  same freight row reappears without advancing the saved cursor.
- There is no job age or deadline to save. A schema 1 or 2 save is incompatible
  rather than partially applied; schema 4 keeps every current corruption check.

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

`ActiveJob` stays 6 B after dropping the timer fields (`kind`, `mission`,
`from` replace them), and `offerSerial` is renamed `offerCursor` with no width
change. The day-zero campaign state projection is unchanged except that the
save scalar is 4 B smaller.

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
| core | 1 | 1 (no chart, 36 B payload) | 0 |
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
  silent and resumes on cancel; saving pauses during the cart erase/page write
  and resumes after success. Same-cue entries do not restart; different cues
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
- Travel region comes from the authored `region` field per room, baked into a
  deterministic 320x160 FX plane. Every tile is owned by the nearest authored
  room centre, including connecting passages, so approach history cannot
  change ownership; ties use the lower room index. Evaluate at most once per
  30 frames. A region must persist for 120 flight ticks (2 s) before it
  replaces the cue.
- Verify the 100 Hz Bubblejet Run under real rendering, chart, transitions and
  idle menus; measure the worst refill window against the 64 B ring and the
  library's service deadline. Listen on the physical speaker before claiming
  audio acceptance.

## 14. Downstream ownership and gates

- `.2` pure campaign/untimed freight rules: use sections 8–10; extend
  `ActiveJob`, add kind/mission/origin, ordinary rewards, mission and survey
  state, WORK page rules; no save code.
- `.3` single-slot cart storage: use sections 3–7 and 10; host-native tests for
  every interrupted erase/page-write boundary, torn header/commit, corrupt
  payload, incompatible signature/version, invalid field combinations, clean
  New Voyage recovery and repeated overwrite cycles; assert a torn record never
  loads or mutates live state, old chart bytes are ignored, no voyage byte is
  written to internal EEPROM, and CRC vector `0x29B1` holds.
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
- The 90–150 minute voyage target is a playtest target, not measured.
- Physical speaker listening, manual playthroughs, soak tests and audio timing
  under combined load were not performed in this audit.

## 16. First-slice implementation and pacing (sub-hauler-7px.5)

This section records what the first playable slice actually implements and
separates generated/runtime measurements from checks that still need a person
at a device.

Implemented in this slice:

- FIRST (Tidewall → Benthos, 145 once) and PUMP (Cormorant → Benthos, 85 once)
  are explicit one-slot campaign cargo jobs. Accepting fills the existing
  four-unit hold; wrong berths retain it; only the matching destination pays.
- PUMP_SURVEY is a generated ring beside the Pump Refuge pump. It uses the
  existing slow half-second dwell, records before FIRST, never occupies the
  cargo slot and is idempotent. The Benthos and Cormorant WORK pages carry the
  pump clues; the cutter and seal rumours remain useful text-only hints.
- WORK replaces the redundant SERVICE row and the old freight rows. One board
  lists eligible restoration jobs first and the repeatable freight row beneath;
  A accepts directly and auto-loads. Active-job boards keep UNDOCK as the
  initial selection and place ABANDON and WORK behind deliberate movement.
- Pump Refuge (berth 4) and Lower Gantry (berth 5) have generated service
  metadata in `src/generated/level.hpp`: stable symbolic ids, dock pixels and
  restoration milestone bits. Both docks pass the wall-attach, hull/approach,
  hazard-clearance and base-tool reachability validators. Neither enters the
  four-settlement route economy.
- PUMP opens Pump Refuge as a service-only berth: docking refills air, repairs
  hull and saves loaded cargo, but shows only the smaller UNDOCK board. It
  publishes no freight offers and sells nothing.
- Native overlays are derived only from saved milestone bits: inactive
  moorings, active gantries, pump flow, Benthos lamp banks and survey rings.
  They do not change collision, chart water or terrain.
- Continue restores berth 4 through generated berth metadata instead of the
  freight-only settlement table. A test saves and reloads at Pump Refuge with
  an active four-unit ordinary load and checks the generated pixel position,
  full air, cargo and the empty freight board.

Generated route budget (`tools/gen_level.py`, conservative, not a play
measurement): the loaded base boat's shortest first-slice loop is Tidewall →
Benthos → Pump Refuge → Cormorant → Benthos. Packed passage lengths are 1,627
px, 537 px, 1,117 px and 1,539 px; at the loaded 18 px/s top speed that is
about 90 + 30 + 62 + 86 = 268 s of continuous straight-line travel. No timer
caps that route. Adding docking, the half-second survey dwell, reading the WORK
clues and real obstacle steering puts the first-time slice in the design's
5–10 minute target range, but that is an estimate from generated geometry, not
a stopwatch result.

Manual checks still outstanding for this slice:

- a person plays FIRST → PUMP with a physical D-pad and confirms the WORK text
  fits and reads on the 128×64 panel;
- a person confirms the survey ring is noticeable beside the pump scenery and
  that the dwell feels short rather than accidental;
- a person confirms the Pump Refuge service save survives power-off on a real
  device and that the inactive mooring does not invite a failed approach;
- the physical speaker is not relevant here because this slice does not add or
  change music.

This slice's production flags were unchanged at the time: CAMPAIGN and
SAVE_VOYAGE stayed off until the later `.6`/`.7`/`.24` work. The full AVR
campaign override then compiled to 29,998 B flash / 1,660 B static and did not
link; later trims and the cart save partition brought the complete `full`
default to **27,010 B / 1,619 B** with campaign, voyage save and music on.
Those later measurements and the current defaults are canonical in
[platform.md](platform.md).

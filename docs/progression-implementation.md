# Voyage progression — implementation contracts

Current status (2026-11-12): the campaign epic is closed. Production uses one
raw cart checkpoint slot and the ATMlib2FX compact synth with a 32 B ring. The
fixed build has no feature profiles. The shop, currency, job payouts,
repeatable freight offers and credit caches are removed. The `sub-hauler-bwg`
cleanup removed the serialized header, schema, signature, length, CRC, commit
byte and skip cache: the slot is now 16 scalar bytes plus the 256-byte chart,
272 meaningful bytes, with no schema validation. It also removed the hull byte,
voyage-time, delivered and rescue counters, and the legacy HOME path. Mines and
hull strength are retired. Dated baseline and audio-probe sections retain their
original measurements and flags, including earlier schema and ring figures; use
[platform.md](platform.md) for current sizes and acceptance limits.

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
make size
make mini
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
opens both partitions. The save tail stays at `FX_SAVE_PAGE`; retained FX
declarations keep their relative order, while deliberate pre-release asset
removals may shift later offsets. The development cart image (`fxdata/fxdata.bin`) carries
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
- The sketch's EEPROM users are the MUSIC ON/OFF setting at 656 and the
  optional standalone `src/chart_save.hpp` writer (mutually exclusive with
  voyage save). Neither is part of a voyage checkpoint.
- Audio mute keeps the system allocation at address 2. The separate MUSIC
  ON/OFF byte remains at **656**, now the first byte above the retired voyage
  allocation. It is never part of a cart checkpoint, so Continue/rollback
  cannot change it; writes happen only on the pause-menu toggle.

## 3. Cart save layout

The save partition is one 4 KiB erase block (16 pages). It owns one raw slot at
offset 0:

| Slot | Meaningful bytes | Programmed bytes | Page numbers |
| --- | ---: | ---: | --- |
| only | 0–271 | 0–511 | 0–1 |

The meaningful payload is **272 B**: 16 scalar bytes followed by the 256-byte
chart. The programmed stream is one 512 B region: 272 meaningful bytes plus 240
bytes of `0xFF` padding after the payload, preserving the two 256-byte page
programs. The remaining 3,584 bytes of the block stay erased and are not used.

```text
slot +   0  scalar bytes 0–15
slot +  16  chart bytes 0–255
slot + 272  0xFF padding through slot + 511
```

There is no header, magic, schema version, layout version, content version,
profile signature, length, CRC, commit byte or chartless payload variant.

## 4. No profile signature

The former one-byte feature/profile signature is retired with the serialized
header. The fixed build has no profiles to distinguish and no byte to write or
check on load. `MUSIC` still has no saved field.

## 5. Scalar payload (16 B)

Fixed order, no padding, no raw C++ struct bytes.

| Offset | Bytes | Field | Rule |
| ---: | ---: | --- | --- |
| 0 | 1 | berth | 0–7; the first payload byte is also the blank/written test |
| 1 | 1 | tools | CUTTER/SEAL/THRUSTER/LENS bits |
| 2 | 1 | pickups | 4 claim bits |
| 3 | 2 | open gates | 9 bits |
| 5 | 2 | seen gates | 9 bits |
| 7 | 1 | campaign milestones | 5 bits |
| 8 | 1 | surveys | 2 bits |
| 9 | 1 | active job kind | 0 or 2 |
| 10 | 1 | active mission ID | 0–6, or 0xFF |
| 11 | 1 | job origin berth | 0–3, or 0xFF |
| 12 | 1 | job destination berth | 0–3, or 0xFF |
| 13 | 1 | job load | 4 for cargo jobs, else 0 |
| 14 | 1 | ending presented | 0/1 |
| 15 | 1 | pending scene | 0 = none, 1–7 = beat/result deck, 8–14 = departure/joint handoff, 15–16 = relay result/briefing |

Chart bytes follow at payload offset 16–271. Scalar total is exactly 16 B; total
meaningful payload is 272 B. There is no saved hull, voyage-time, delivered-job,
rescue-count, currency, air-level, visited-settlement, job reward, offer-cursor
or board-seed field.

Save does not validate the live checkpoint. Load reads the 16 scalar bytes,
applies them, and reads the chart bytes as-is. Memory-safety bounds still keep
an arbitrary berth, scene index or chart access in range.

Old schema 1–8 records and the retired standalone chart block are not migrated,
imported, rejected or repaired. Pre-release development saves are discarded by
this cleanup. Any non-`0xFF` first byte is read as the current raw payload.

## 6. Save transaction and interrupted writes

Save transaction (freeze simulation, pause music, ignore departure input):

1. Erase the single 4 KiB cart block. This also removes the previous checkpoint.
2. Program page 0 (scalar and the first 240 chart bytes), then page 1
   (the remaining 16 chart bytes plus `0xFF` padding). No second payload or
   chart-sized RAM buffer is allocated.
3. Wait for each erase/page program to finish before starting the next one.
4. Report `SAVED` once both page programs finish, then resume music and unfreeze.
   There is no post-write read-back. A write that does not stick is not detected
   at save time.

Each explicit save performs this transaction, including an identical state.
There is no RAM CRC cache, skip path, dirty flag, commit byte or write-only
header to preserve.

**This is not power-fail atomic.** An interruption during the erase or page
program may lose the previous checkpoint or leave a torn record. The torn record
is not detected: load reads whatever payload bytes are present. There is no
second copy, sequence, newest-slot selection or fallback.

Blank vs written: the slot is blank when its first byte is `0xFF`. Any other
first byte is treated as a written save and loaded. There is no corrupt or
incompatible title state. Replacing a written checkpoint still requires the
title confirmation.

Load reads the scalar and chart directly into initialized state. It never
copies a raw struct and never allocates a second chart-sized buffer. Save
success is assumed after the page programs; without a read-back, a failed erase
that leaves the old record intact cannot be distinguished from a successful new
write at save time. Rescue is load-only: it performs no erase or page program.

Worst case erase is one 4 KiB block (~40 ms) plus two 256 B page programs
(~3.3 ms each), on a transition only. The 4 KiB partition is `FX_SAVE_BYTES`;
no per-frame autosave, pickups, chart closing, music progress or
developer traversal touches it.

## 7. No legacy chart migration

The standalone chart migration and import path are retired completely. EEPROM
addresses 0–259 and older cart records are never inspected, offered, copied or
repaired. A NEW VOYAGE resets the chart, applies `chart::revealInitial()` at the
generated Tidewall spawn berth, writes that docked checkpoint, then plays the
opening deck and spawns live free flight. There is no KEEP OLD CHART prompt
and no legacy import path. The single cart slot is erased and rewritten. The
fixed build writes only the raw single-slot voyage checkpoint.

## 8. Stable campaign IDs

Mission IDs (`kMissionNone = 0xFF`):

| ID | Name | Route | Completion |
| ---: | --- | --- | --- |
| 0 | FIRST | Tidewall → Benthos | milestone FIRST |
| 1 | PUMP_SURVEY | Benthos → Pump Refuge survey | survey PUMP |
| 2 | PUMP | Cormorant → Benthos | milestone PUMP |
| 3 | LIFT_SURVEY | Cormorant → Freight Yard survey | survey LIFT |
| 4 | LIFT | Benthos → Cormorant | milestone LIFT |
| 5 | BEACON | Cormorant → Deep Anchor | milestone BEACON |
| 6 | HOME | Deep Anchor → Tidewall | milestone HOME |
| 7 | DEEP RELAY | Deep Anchor → Deep Relay | milestone EXPEDITION |

Campaign work completes once and pays nothing. No generated route reward or
deadline table exists. DEEP RELAY is mandatory: HOME cannot be accepted until
the expedition milestone is set.

Survey IDs: `SURVEY_PUMP = 0` (Pump Refuge), `SURVEY_LIFT = 1` (Freight Yard).
Both rings exist from voyage start; their clues appear after FIRST. A recorded
survey satisfies the request immediately and is never demanded again.

Milestone bits: `FIRST=0`, `PUMP=1`, `LIFT=2`, `BEACON=3`, `HOME=4`,
`EXPEDITION=5`. Survey bits: `PUMP=0`, `LIFT=1`.

Berth IDs: Tidewall 0, Benthos 1, Cormorant 2, Deep Anchor 3,
Pump Refuge 4, Lower Gantry 5, Freight Yard 6, Deep Relay 7. Berths 0–3 are
the campaign delivery destinations; 4–7 are service-only and must never appear
in a mission destination. `SETTLEMENT_COUNT` stays 4; generated `BERTH_COUNT` is
8.

Restoration eligibility:

- Pump Refuge (4) becomes dockable and checkpoint-valid only after milestone
  PUMP; Lower Gantry (5) and Freight Yard (6) only after LIFT; Deep Relay (7)
  only after EXPEDITION, or while its own matching load is aboard.
- Before restoration the mooring is visible but inactive and shows a clue.
- Service berths refill air, repair the hull and save the voyage with cargo;
  they do not generate work, accept deliveries or sell upgrades.
- All four service berths need authored safe approaches and generated service
  metadata (pixel position plus required milestone), produced by `make gen`.
  Their placement must pass the same hull-clearance, escape-column and
  reachability checks as the four existing berths.

Mission eligibility:

- FIRST is available at Tidewall from voyage creation; the board selects it but
  does not auto-accept.
- PUMP_SURVEY requires FIRST; the ring exists from voyage start, and Benthos
  lists it as a non-cargo WORK row. A opens the chart; the field dwell records.
- LIFT_SURVEY requires FIRST; the ring exists from voyage start, and Cormorant
  lists it as a non-cargo WORK row. A opens the chart; the field dwell records.
- PUMP requires FIRST and PUMP_SURVEY; source Cormorant.
- LIFT requires FIRST and LIFT_SURVEY; source Benthos.
- BEACON requires PUMP, LIFT and the pressure seal; source Cormorant.
- DEEP RELAY requires BEACON plus cutter, pressure seal, thruster and survey
  lens; source Deep Anchor; destination Deep Relay; explicit acceptance.
- HOME requires BEACON and DEEP RELAY; source Deep Anchor; explicit acceptance,
  never auto-loaded. There is no pre-linear HOME exception or legacy save path.
- Completed missions never reappear. Abandoning clears cargo and the active
  job, saves, and lets the source reissue the same mission for free.

## 9. Job kinds and untimed mission semantics

`ActiveJob` uses explicit kind/mission/origin fields and is 5 bytes:

```cpp
struct ActiveJob {
    uint8_t kind;      // NONE=0, CAMPAIGN=2
    uint8_t mission;   // campaign id or kJobNone
    uint8_t from;      // origin berth
    uint8_t to;        // destination berth
    uint8_t crates;    // 4
};
```

`hasActiveJob()` is `kind != kJobNone`. One cargo job is active at a time;
surveys and tool discoveries do not occupy the slot.

- **Campaign**: untimed and unpaid. Wrong-berth stops retain
  the job and load; only the matching destination resolves it. Short or missing
  cargo cannot set completion.
- There is no ordinary/repeatable freight kind, no reward field, no express
  kind, deadline, clock, late failure or expiry. Air and handling are the
  expedition pressure.
- Accepting any job auto-loads four units, neutralises trim and leaves the boat
  docked. UNDOCK or B starts travel.
- The station board is `WORK / UNDOCK`; WORK opens one board listing eligible
  story and survey rows. A accepts directly. The active-job board is
  `summary / WORK / UNDOCK`, keeping UNDOCK initially; mandatory story work has
  no abandon row or confirmation. WORK remains available for the carrying
  summary and clues. A work/service berth shows UNDOCK alone.

## 10. Campaign state additions

Implemented additions to `game::State` (AVR byte-packed, all fields also mapped
to the payload above):

| Field | Width | Notes |
| --- | ---: | --- |
| `campaign` | 1 | milestone bits |
| `surveys` | 1 | survey bits |
| `ending` | 1 | 0/1 |
| `pendingStory` | 1 | 0 = none, 1–7 = result/survey deck, 8–14 = departure/joint handoff, 15 = expedition result, 16 = relay briefing |

`voyageTicks`, `delivered`, `rescues`, `lastDelivery` and the saved hull byte
are removed. `ActiveJob` is 5 B: `kind`, `mission`, `from`, `to` and `crates`;
there is no timer, reward or second validation gate. The saved scalar is 16 B.
The baseline `sizeof(game::State)` was 77 B; the accepted arithmetic is 67 B
after the 10 B removal, with the whole-image figure pending the integration
gate.

## 11. Fixed build and profiles

The fixed build enables campaign, voyage save and synth music unconditionally.
Feature-profile selection, `src/features.hpp`, `tools/features.py`, development
mode, runtime-USB production builds and the standalone chart writer are retired.
The implementation keeps only compiler/platform distinctions and the fixed
ATMlib2FX compact-loop/ring settings in the build recipe. The early proposal
that lean/core would carry campaign+save was not shipped.

## 12. Music integration contract and measured cost

### Historical admission probe (2026-10-26, before the production synth fit)

The probe below used the earlier 64-byte ring and a standalone decoder budget.
It is retained as evidence for the original admission decision. Production now
uses `ATM_FX_COMPACT_RING=32` and the measured whole-image result in
[platform.md](platform.md).

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

Historical consequences of that admission probe (superseded by the measured
production synth fit; the fixed build now ships music):

- The probe implied full MUSIC should default to **0** until a measured
  combined image passed the 1,700 B static ceiling and the 600 B stack target.
- `.7` was to measure the actual combined image, then carve at least ~135 B of
  static state or leave MUSIC off.
- Plausible carves were small: particle pool 16 B (only by dropping debris),
  camera 4 B (derive from position), fractional carries 3 B, transient contact/
  discovery bytes 5 B, dock/impact cooldowns 3 B. The 256 B chart was the only
  large block. The later measured fit is recorded in platform.md.

### Runtime contract

- Compile with `ATM_FX_COMPACT_RING=32` (production 2026-10-28; largest
  authored record is 28 B), `ATM_FX_COMPACT_LOOP=1` (streaming loops on) and all
  effects enabled. The earlier design text used 64 B; the measured production
  image uses 32 B without underruns.
- `atm_fx_compact_fetch(offset, dst, n)` calls
  `FX::readDataBytes((uint24_t)offset, dst, n)`; offsets are data-section-
  relative and the FX page is added exactly once.
- `atm_synth_setup()` once in `setup()` after `FX::begin(FX_DATA_PAGE)`.
- Play with `atm_fx_compact_play(sh_<cue>, music_data::sh_<cue>_bytes)` using
  the generated FX symbols and lengths.
- Call `atm_fx_compact_service()` after the preceding `FX::display(...)` and
  before the next render. Service idle station frames even when no OLED
  transaction occurs. The timer consumer must never read FX.
- Chart and pause pause the player (`atm_fx_compact_pause()`) and resume in
  place; idle station frames keep playing; rescue confirmation is
  silent and resumes on cancel; saving pauses during the cart erase/page write
  and resumes after success. Same-cue entries do not restart; different cues
  cut/restart.
- Audio mute uses the Arduboy2 system setting at EEPROM address 2. MUSIC OFF
  stops playback and prevents starts; it is not a voyage field.
- Cue IDs: `HARBOR=0`, `SONAR=1`, `KELP=2`, `BUBBLEJET=3`, `ABYSSAL=4`,
  `TIDEPOOL=5`.
- Cue mapping: the Tidewall board, unrestored Benthos/Cormorant boards and the
  new-voyage title → HARBOR; Lantern Reaches travel → SONAR; Kelp Galleries
  travel → KELP; Broken Works travel → BUBBLEJET; Glass Trench travel and
  unrestored Deep Anchor board → ABYSSAL; restored Benthos/Cormorant boards,
  restored service berths, restored Deep Anchor board, PUMP/LIFT/BEACON story
  decks and ending → TIDEPOOL.
- Travel region comes from the authored `music_region`, falling back to the
  scenery `region`, baked into a deterministic 320x160 FX plane. Every tile is
  owned by the nearest authored room centre, including connecting passages, so
  approach history cannot change ownership; ties use the lower room index.
  Gate-cut open-water fragments under 256 tiles merge into the neighbouring
  region with the longest shared edge, keeping cue boundaries readable.
  Evaluate at most once per 30 frames. A region must persist for 120 flight
  ticks (2 s) before it replaces the cue.
- Verify the 100 Hz Bubblejet Run under real rendering, chart, transitions and
  idle menus; measure the worst refill window against the 64 B ring and the
  library's service deadline. Listen on the physical speaker before claiming
  audio acceptance.

## 13. Downstream ownership and gates

This is the original 2026-10 implementation plan. The accepted `sub-hauler-bwg`
cleanup supersedes its save-format details: there is no header, CRC, commit or
schema to test.

- `.2` pure campaign/untimed mission rules: use sections 8–9; extend
  `ActiveJob`, add kind/mission/origin, mission and survey
  state, WORK page rules; no save code.
- `.3` single-slot cart storage: use sections 3–7; host-native tests for
  save/load, blank detection, clean New Voyage recovery, active cargo restore,
  arbitrary-berth safety and repeated overwrite cycles; assert no header/CRC
  rejection, every explicit save performs one erase and two page programs even
  for identical state, rescue performs zero writes, any nonblank first byte
  loads raw, no voyage byte is written to internal EEPROM, and runtime
  `FX::programSavePage` addressing is used.
- `.4` Continue/checkpoints/recovery: use sections 3, 6 and 9; commit on the
  listed transitions only; no midwater suspend.
- `.5`/`.6` campaign slices: use sections 8 and 10; generated service berth
  metadata; base-tool reachability and retreat/service stops.
- `.7` music: use section 12; combined measurement first; MUSIC stays off
  unless the measured static/stack fit passes.
- `.8` release verification: all canonical gates, FX/Mini builds,
  whole-image size, stack reserve at least 600 B, and explicit reporting of what
  was not manually or physically tested.

## 14. Unproven items at the 2026-10-26 audit

These were open before the later measured campaign/save/music integration; use
[platform.md](platform.md) for the current evidence.

- Combined campaign + voyage save + music flash/static/stack fit is **not
  measured**. The music marginal cost is measured in isolation; campaign code
  and save code are projected only.
- Service berth placement, approach clearance and reachability are not authored
  or validated yet.
- The 90–150 minute voyage target is a playtest target, not measured.
- Physical speaker listening, manual playthroughs, soak tests and audio timing
  under combined load were not performed in this audit.

## 15. First-slice implementation and pacing (sub-hauler-7px.5)

This section records what the first playable slice actually implements and
separates generated/runtime measurements from checks that still need a person
at a device.

Implemented in this slice:

- FIRST (Tidewall → Benthos) and PUMP (Cormorant → Benthos)
  are explicit one-slot campaign cargo jobs. Accepting fills the existing
  four-unit hold; wrong berths retain it; only the matching destination
  completes it, and no delivery pays.
- PUMP_SURVEY is a generated ring beside the Pump Refuge pump. It uses the
  existing slow half-second dwell, records before FIRST, never occupies the
  cargo slot and is idempotent. The Benthos and Cormorant WORK pages carry the
  pump clues; the cutter and seal rumours remain useful text-only hints. After
  FIRST, an unrecorded survey blinks on the chart, its in-world ring blinks,
  and a wider ±48 px proximity radius raises the stop-to-survey notice while
  the boat is still moving. The clue berth also lists the survey as a
  non-cargo WORK row; A opens the chart instead of loading cargo. Recording the
  fact clears all four prompts.
- WORK replaces the redundant SERVICE row and the retired freight rows. One
  board lists eligible restoration and survey rows;
  A accepts directly and auto-loads. Active-job boards keep UNDOCK as the
  initial selection; story work cannot be abandoned.
- Pump Refuge (berth 4), Lower Gantry (berth 5) and the Freight Yard lift
  (berth 6) have generated work/service metadata in
  `src/generated/level.hpp`: stable symbolic ids, dock pixels and restoration
  milestone bits. The mandatory Deep Relay (berth 7) uses the
  expedition milestone bit and requires all four tools. All docks pass the
  wall-attach, hull/approach and hazard-clearance validators; restoration sites
  stay base-tool reachable and the late expedition is reachable with the full
  tool set.
- PUMP opens Pump Refuge as a service-only berth: docking refills air, repairs
  hull and saves loaded cargo, but shows only the smaller UNDOCK board. It
  publishes no work and sells nothing.
- Native overlays are derived only from saved milestone bits: inactive
  moorings, active gantries, pump flow, Benthos lamp banks and survey rings.
  They do not change collision, chart water or terrain.
- Continue restores berth 4 through generated berth metadata instead of the
  delivery-only settlement table. A test saves and reloads at Pump Refuge with
  an active four-unit campaign load and checks the generated pixel position,
  full air, cargo and the empty WORK board.

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

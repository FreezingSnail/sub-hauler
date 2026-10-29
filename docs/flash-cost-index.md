# Program flash cost index — EEPROM-based combined playtest (2026-10-28)

**Status: historical pre-FX-save snapshot.** This index measures the retired
EEPROM-backed voyage-save playtest image and its former profile selectors;
those build options no longer exist. The user has now directed voyage
saves to move to FX, so these numbers describe the EEPROM implementation and
must be re-measured after the FX-save change. No FX-save build is measured
here and no FX-save delta is estimated.

- Candidate source snapshot: `/private/tmp/sub-hauler-spawn-fix-20261028/build/spawn-sketch/sub-hauler`
- Candidate artifacts: `/private/tmp/sub-hauler-spawn-fix-20261028/playtest-package-20261028`
- Isolated read-only analysis/rebuild workspace: `/private/tmp/sub-hauler-flash-index-20261028`
- Report date: 2026-10-28. No commit or push. Only this file was added to the main workspace.

## Whole-image result

| Item | Value |
| --- | ---: |
| `.text` | 28,984 B |
| `.data` | 14 B |
| `.bss` (SRAM only) | 1,617 B |
| **MCU flash (`.text` + `.data`)** | **28,998 B** |
| Static SRAM (`.data` + `.bss`) | 1,631 B |
| Hardware flash limit | 29,696 B |
| Flash free vs hardware | 698 B |
| Planning ceiling | 29,000 B |
| Flash free vs planning | 2 B |
| Intended unallocated flash reserve | 500 B |
| Reserve consumed by this image | 498 B |
| Reserve remaining | 2 B |
| Static SRAM planning ceiling | 1,700 B |
| Static SRAM headroom | 69 B |
| SRAM free after static | 929 B |

`make size` reproduces exactly:

```text
size: .text=28984 .data=14 .bss=1617
size: flash=28998/29696 (698 free)  ram=1631/2560
```

The 2 B planning headroom is the 2 B remaining of the approved 500 B reserve;
28,998 B has consumed 498 B of it. The image fits hardware with 698 B free,
but has no meaningful flash reserve left.

## Identification and reproducibility

Candidate configuration:

- `arduino-cli 1.2.0`
- `arduboy-homemade:avr` 1.4.0, FQBN `arduboy-homemade:avr:arduboy-fx`
- AVR-GCC `7.3.0-atmel3.6.1-arduino7`
- Compile flags from the actual `compile_commands.json`: `-g -Os -flto -mrelax -mcall-prologues`
- `SH_DEV_MODE=0`, `SH_RELEASE_NO_USB=1`
- Active features: `STATIONS JOBS FOG_CHART SONAR MINES CURRENTS SHOP HULL_DAMAGE PARTICLES MULTI_HULL EXPLORATION`
- Active story/audio features: `CAMPAIGN=1 SAVE_VOYAGE=1 MUSIC=1` (not the default `full` profile; this is an explicit combined build)
- `SH_FEATURE_SAVE_CHART=0`
- Voyage save storage: **EEPROM** in this snapshot (the FX move is pending)

| Artifact | SHA-256 |
| --- | --- |
| Candidate ELF `sub-hauler.ino.elf` | `02fda174e4e0593b2bd19ac1b7c17d82f09cbbd708207acff32700af21c62c68` |
| Candidate HEX `sub-hauler.ino.hex` | `88ea6bc058b1e280981af70eec8350d2944197f0062bc73f6e5d92fe1a72b5e8` |
| Rebuilt ELF (isolated copy) | `befa646979a0f3881acbb3f261992003aa367399c1f01b1356c3c180be99293a` |
| Rebuilt HEX (isolated copy) | `88ea6bc058b1e280981af70eec8350d2944197f0062bc73f6e5d92fe1a72b5e8` |
| Linker map `build/playtest.map` | `646dd2f325a2c0ce640a8a21101ef306a207c08244af871016e93d8688c2285d` |
| Source manifest (70 files) | `12d196bd69432f88ef621305c7132e8f79ef948bcd453224faa0a32bf934e9c9` |
| FX image `fxdata.bin` | `e376f90e0f17bc03f72938f675794c4fe5ad836e8167d500b87240ed412d2ef0` |
| FX payload `fxdata-data.bin` | `ee5b5df1f0df1d41be9f9dfa1493601637ae9e2d4ab4c62745bf687c9042dda1` |
| `air-map-fix.patch` | `181cc902faadd30bb1fd06d8d63eaa97541b6285962f65e2841db5ff0da9cfe2` |
| `spawn-fix.patch` | `2f55f8b02d525a9ba0a11087488dd241f1631e87c4af30686f64654e2bf3eea4` |

The rebuilt HEX is byte-identical to the candidate HEX, so the flash image is
reproducible. The rebuilt ELF has different path-dependent debug metadata
(debug line/string tables); `.text`, `.data`, `.bss` sizes and the complete
flash image are unchanged.

Source manifest command, run inside the candidate source snapshot:

```sh
{ find sub-hauler.ino src tools/features.py Makefile fxdata/fxdata.h fxdata/fxdata.bin fxdata/Sprites.txt \
    -type f -print0 | sort -z | xargs -0 shasum -a 256; } | shasum -a 256
```

Result: `12d196bd69432f88ef621305c7132e8f79ef948bcd453224faa0a32bf934e9c9`.

## Exact address allocation

`avr-size -A` and `avr-objdump -h` give the whole-image sections. The linker
script places all executable/progmem bytes in `.text` (load address 0), and
the initialized SRAM image in `.data` (load address `0x7138`).

| Range | Bytes | Contents |
| --- | ---: | --- |
| `0x0000`–`0x00AB` | 172 | vector table |
| `0x00AC`–`0x02FF` | 596 | PROGMEM data (`.progmem.data`) |
| `0x0300`–`0x0339` | 58 | init2/init4/init9 boot code |
| `0x033A`–`0x033B` | 2 | CRT bad-interrupt stub |
| `0x033C`–`0x5423` | 20,712 | LTO merged `.text` (project + libraries) |
| `0x5424`–`0x6F31` | 6,926 | LTO `.text.startup` / `main` |
| `0x6F32`–`0x7133` | 514 | avr-libc / libgcc / EEPROM primitives |
| `0x7134`–`0x7137` | 4 | `.fini0` |
| **Total `.text`** | **28,984** | |
| `.data` load `0x7138`–`0x7145` | 14 | initialized SRAM image copied from flash |
| `.bss` SRAM `0x80010E`–`0x80075E` | 1,617 | zero-initialized SRAM, not flash |

Symbol coverage is exact but not complete without the non-symbol ranges:

- 148 unique nonzero-size `.text` symbols (one duplicate LTO alias collapsed)
  cover **28,786 B** as a union of ranges.
- **198 B** are not represented by a nonzero-size text symbol: 172 B vector
  table, 20 B `init2`/`init9`, 2 B CRT stub, 4 B `.fini0`.
- 28,786 + 198 = **28,984 B**.
- Two `EEPROMClass::read(int)` LTO aliases share `0x4702`/6 B; count once.
- `exit_to_bootloader` is a zero-size label at `0x53FC` inside the 148 B
  `__vector_23` symbol; do not add it separately.
- `.data` has 13 B of symbols plus 1 B alignment fill = 14 B.

## Exhaustive category allocation

Every byte of `.text` belongs to exactly one row. “Non-PROGMEM” includes the
vector table and boot/init bytes, so it is not all executable code. PROGMEM
data is kept separate because it is flash-resident data, not instructions.
This table uses top-level symbol ownership: an inlined body is charged to its
host symbol (for example, inlined helpers inside `drawPilot()` stay in the
4,486 B `drawPilot()` symbol). The DWARF cross-check below splits host ranges
without changing these totals.

| Category | Non-PROGMEM B | PROGMEM data B | Total B |
| --- | ---: | ---: | ---: |
| Vectors | 172 | 0 | 172 |
| Boot/init/fini | 64 | 0 | 64 |
| Third-party display/cart/SPI (Arduboy2 + ArduboyFX) | 1,194 | 13 | 1,207 |
| avr-libc / libgcc | 514 | 0 | 514 |
| Arduino core ISR/timing | 196 | 0 | 196 |
| Music (StreamFX + `music` + Timer3 ISR) | 1,310 | 258 | 1,568 |
| Checkpoint/save (EEPROM implementation) | 3,216 | 0 | 3,216 |
| Campaign/ending | 1,224 | 48 | 1,272 |
| Physics/rules/world | 3,920 | 277 | 4,197 |
| Render | 7,156 | 0 | 7,156 |
| UI/screens | 2,496 | 0 | 2,496 |
| Main/loop glue | 6,926 | 0 | 6,926 |
| **Total** | **28,388** | **596** | **28,984** |

Important boundaries:

- The 64 B boot/init/fini row is 58 B init + 2 B CRT stub + 4 B `.fini0`.
  Of the 58 B init, 38 B are the `__do_copy_data` / `__do_clear_bss` symbols
  and 20 B are unsymbolized `init2`/`init9`.
- The only bytes outside nonzero-size symbols are 198 B total: 172 B vector
  table, 20 B `init2`/`init9`, 2 B CRT stub, and 4 B `.fini0`. All 28,786 B of
  nonzero-size symbols are categorized exactly once.
- `third-party display/cart/SPI` is a defensible **group** bound: 870 B of
  `FX::*` (cart command/SPI plus pixel decode), 324 B of Arduboy2 drawing/input
  code, and 13 B `lcdBootProgram`. It is not a pure SPI-transaction count.
- Checkpoint/save includes the 40 B `EEPROMClass` read/update wrappers because
  they exist only for the EEPROM save path. It does not include the EEPROM
  device, which is outside MCU flash.
- Music includes the 480 B Timer3 ISR (`__vector_32`), 258 B PROGMEM tables
  (`game_audio::kStreams`, IMA index/step), and StreamFX/`music` code.
- Physics data is 277 B: 119 B `exploration_data::DATA` + 158 B world metadata.
- Campaign data is 48 B: 35 B `campaign::kMissions` + 13 B campaign-view tables.

## Subsystem index

### Largest symbols

| Size | Category | Symbol |
| ---: | --- | --- |
| 6926 | main/loop | `main` |
| 4486 | render | `drawPilot()` |
| 942 | render | `drawChart()` |
| 858 | checkpoint/save | `voyage_save::SaveResult voyage_save::save<voyage_save::EepromStorage>(game::State const&, chart::State const&, voyage_save::EepromStorage&) [clone .constprop.27]` |
| 856 | UI/screens | `drawWork()` |
| 580 | third-party display/cart/SPI | `FX::drawBitmap(int, int, uint24, unsigned char, unsigned char)` |
| 578 | UI/screens | `station_view::draw(game::State const&, unsigned char) [clone .constprop.91]` |
| 574 | physics/rules/world | `bool game::moveAxis<world::FxTables>(game::State&, world::FxTables&, world::TileCache const*, bool) [clone .constprop.15]` |
| 534 | checkpoint/save | `voyage_save::LoadResult voyage_save::load<voyage_save::EepromStorage>(game::State&, chart::State&, voyage_save::EepromStorage&) [clone .constprop.24]` |
| 494 | render | `Blit::draw(int, int, unsigned char, unsigned char, uint24, unsigned int, unsigned char) [clone .part.0]` |
| 490 | physics/rules/world | `jobs::refreshOffers(game::State&, unsigned char) [clone .constprop.120]` |
| 480 | music | `__vector_32` |
| 460 | physics/rules/world | `station::activate(unsigned char, game::State&) [clone .constprop.97]` |
| 438 | UI/screens | `shop_view::draw(game::State const&, unsigned char) [clone .constprop.49]` |
| 368 | music | `streamfx::start(streamfx::Stream const&)` |
| 352 | checkpoint/save | `voyage_save::SlotInfo voyage_save::inspectSlot<voyage_save::EepromStorage>(voyage_save::EepromStorage&, unsigned char) [clone .constprop.28]` |
| 328 | campaign/ending | `campaign::activeJobValid(game::State const&)` |
| 300 | physics/rules/world | `bool world::detail::rectHitsImpl<world::FxTables>(world::FxTables&, world::TileCache const*, int, int, int, int) [clone .constprop.19]` |
| 296 | checkpoint/save | `voyage_save::scalarValid(voyage_save::Scalar const&)` |
| 264 | render | `cave::fillRect(unsigned char*, int, int, unsigned char, unsigned char, bool) [clone .constprop.166]` |
| 260 | campaign/ending | `drawEnding()` |
| 248 | music | `streamfx::(anonymous namespace)::fillRing()` |
| 244 | checkpoint/save | `startNewVoyage(voyage_save::LegacyChart)` |
| 220 | render | `cave::drawGantry(unsigned char*, int, int) [clone .constprop.165]` |
| 210 | physics/rules/world | `chart::revealAtPixels(chart::State&, int, int, unsigned char) [clone .constprop.71]` |
| 196 | checkpoint/save | `voyage_save::Inventory voyage_save::inspect<voyage_save::EepromStorage>(voyage_save::EepromStorage&)` |
| 182 | third-party display/cart/SPI | `FX::drawChar(unsigned char) [clone .part.2]` |
| 178 | music | `streamfx::ima::kStepTable` |
| 170 | physics/rules/world | `exploration::blockingGate(game::State const&, int, int) [clone .constprop.161]` |
| 158 | UI/screens | `drawTitleScreen()` |
| 154 | physics/rules/world | `world::berthMeta(unsigned char, world::Berth&)` |
| 148 | Arduino core ISR/timing | `__vector_23` |
| 144 | UI/screens | `station_view::drawStatus(game::State const&) [clone .constprop.93]` |
| 142 | render | `exploration_view::glyph(unsigned char*, int, int, unsigned char) [clone .constprop.156]` |
| 140 | render | `chart_view::clearMarkerBox(unsigned char*, int, int) [clone .constprop.46]` |
| 132 | physics/rules/world | `world::currentMeta(unsigned char, world::Current&) [clone .part.11]` |
| 130 | physics/rules/world | `world::serviceBerthMeta(unsigned char, world::Berth&) [clone .part.12]` |
| 130 | checkpoint/save | `performRecovery()` |
| 124 | campaign/ending | `campaign::missionPrerequisites(game::State const&, unsigned char)` |
| 122 | checkpoint/save | `refreshBootStatus()` |

### Render (7,156 B)

`drawPilot()` alone is 4,486 B; it contains inlined terrain, restoration,
current, sonar, particle, and chart-drawing helpers. `drawChart()` is 942 B.
`cave` primitives are 550 B, `Blit::draw` 534 B, `chart_view` 236 B,
`exploration_view::glyph` 142 B, `panel_view` 96 B, `text` 170 B.

### UI/screens (2,496 B)

`drawWork()` 856 B, `station_view` 906 B, `shop_view::draw()` 438 B,
`drawTitleScreen()` 158 B, `drawPause()` 58 B, `showStation()`/`showPilot()`
80 B.

### Physics/rules/world (3,920 B code + 277 B data)

Largest: `game::moveAxis` 574 B, `world::rectHitsImpl` 300 B,
`station::activate` 460 B, `jobs::refreshOffers` 490 B, `chart::revealAtPixels`
210 B, `exploration::blockingGate` 170 B, `world::berthMeta` 154 B. This group
is the game simulation, collision/world lookup, jobs, station interaction,
chart reveal, shop rules, hazards, and particles.

### Checkpoint/save — EEPROM (3,216 B)

`voyage_save::save` 858 B, `voyage_save::load` 534 B, `inspectSlot` 352 B,
`scalarValid` 296 B, `inspect` 196 B, `startNewVoyage` 244 B,
`performRecovery` 130 B, `refreshBootStatus` 122 B, `finishSaveResume` 120 B,
plus CRC, EEPROM wrappers, and save/resume glue. This entire category is the
EEPROM-backed implementation and is the main thing that will change when
voyage saves move to FX.

### Campaign/ending (1,224 B code + 48 B data)

`campaign::activeJobValid` 328 B, `campaign::missionPrerequisites` 124 B,
`workMissionAt` 112 B, `missionInfo` 94 B, `missionCompleted` 82 B,
`missionReward` 74 B, `berthAvailable` 58 B, `drawEnding()` 260 B,
`endingPending`/`beginEnding` 46 B, plus campaign-view draw code and tables.

### Music (1,310 B code + 258 B data)

Timer3 ISR `__vector_32` 480 B, `streamfx::start` 368 B, `fillRing` 248 B,
StreamFX service/mute/pause 84 B, `music::*` 130 B, plus 258 B PROGMEM tables.
The ISR is the single largest music item; it is included once.

### Third-party and runtime

- ArduboyFX `FX::*`: 870 B code. `FX::drawBitmap` 580 B and `FX::drawChar`
  182 B dominate. This is the cart/SPI/asset-decode group.
- Arduboy2 drawing/input: 324 B, plus 13 B `lcdBootProgram`.
- avr-libc/libgcc/EEPROM primitives: 514 B.
- Arduino core millis ISR + `micros`: 196 B.
- `main`/loop: 6,926 B. This is mostly setup, frame pacing, screen dispatch,
  and inlined loop glue, not a separate game subsystem.

## Inline/LTO accounting and double-count prevention

The linker map confirms that all project and library C++ code is merged into
one LTO object (`ltrans0.ltrans.o`). Object/archive attribution is therefore
not available for project subsystems; the map can only separate `.vectors`,
`.progmem.data`, init/CRT, the LTO `.text`, the LTO `.text.startup`, and
avr-libc/libgcc. Subsystem attribution uses `avr-nm` symbols plus DWARF inline
records.

DWARF attribution of the 28,216 executable bytes (PROGMEM data and vectors
excluded) charges each instruction to exactly one innermost inline frame. It
is a cross-check, not the exact linker accounting:

| Category | Symbol-table code/vector B | DWARF code B |
| --- | ---: | ---: |
| Render | 7,156 | 6,706 |
| Physics/rules/world | 3,920 | 4,326 |
| Checkpoint/save | 3,216 | 3,240 |
| UI/screens | 2,496 | 2,812 |
| Campaign/ending | 1,224 | 1,424 |
| Music | 1,310 | 1,310 |
| Third-party display/cart/SPI | 1,194 | 1,186 |
| avr-libc/libgcc | 514 | 514 |
| Arduino core | 196 | 196 |
| Boot/init/fini | 64 | 76 |
| Main/loop | 6,926 | 6,426 |

Differences are inline attribution and code layout, not missing bytes. The
symbol-table totals remain authoritative because they are pinned to addresses
and section sizes.

Main and ISR sharing:

- `main` is one 6,926 B symbol. DWARF splits it into 6,426 B main/loop glue,
  406 B physics, 42 B campaign, 24 B checkpoint, 16 B third-party, and 12 B
  music. Do not add those inline shares to `main`; they are already inside
  its 6,926 B.
- `__vector_32` is 480 B of music ISR code. Its `tick`/`decode` bodies are
  inlined into it; do not separately add StreamFX `tick`/`decode` bytes.
- `__vector_23` is 148 B of Arduino core timer/millis code. It physically
  contains the 22 B `exit_to_bootloader` at `0x53FC`; the label has zero size
  and must not be added again.
- `drawPilot()` is 4,486 B and contains inlined render helpers. The helper
  symbols listed elsewhere are separate outlined functions, not duplicates of
  the inlined bodies. Do not add a “drawPilot + inlined helpers” figure.

## Marginal feature costs (matched rebuilds)

All values are exact `avr-size` whole-image measurements from matched rebuilds
of the same source snapshot and toolchain, each in a separate build directory.
They are **not additive**: LTO inlining, clone selection, and code layout move
when flags change.

### Incremental story/save/music chain

| Build | Flash B | Static B | Delta flash | Delta static |
| --- | ---: | ---: | ---: | ---: |
| `CAMPAIGN=0 SAVE_VOYAGE=0 MUSIC=0` (default `full`) | 17,668 | 1,436 | — | — |
| `CAMPAIGN=1 SAVE_VOYAGE=0 MUSIC=0` | 22,548 | 1,461 | +4,880 | +25 |
| `CAMPAIGN=1 SAVE_VOYAGE=1 MUSIC=0` | 26,832 | 1,470 | +4,284 | +9 |
| `CAMPAIGN=1 SAVE_VOYAGE=1 MUSIC=1` (playtest) | 28,998 | 1,631 | +2,166 | +161 |

This chain is valid and sums exactly to the playtest image: +11,330 B flash,
+195 B static. Checkpoint here is the EEPROM implementation.

### Profile matrix

| Profile | Flash B | Static B | `.text` / `.data` / `.bss` |
| --- | ---: | ---: | --- |
| `full` (default) | 17,668 | 1,436 | 17,658 / 10 / 1,426 |
| `lean` | 14,948 | 1,424 | 14,930 / 18 / 1,406 |
| `core` | 12,138 | 1,165 | 12,120 / 18 / 1,147 |
| `min` | 6,094 | 1,114 | 6,084 / 10 / 1,104 |

These match the dated `docs/platform.md` profile matrix for the same snapshot.

### One-at-a-time ablations from the playtest image

Each row is a matched rebuild with only the listed feature(s) disabled.

| Change from playtest | Flash B | Delta flash | Static B | Delta static |
| --- | ---: | ---: | ---: | ---: |
| `MUSIC=0` | 26,832 | -2,166 | 1,470 | -161 |
| `SAVE_VOYAGE=0 MUSIC=0` | 22,548 | -6,450 | 1,461 | -170 |
| `CAMPAIGN=0 SAVE_VOYAGE=0 MUSIC=0` | 17,668 | -11,330 | 1,436 | -195 |
| `SHOP=0` | 27,900 | -1,098 | 1,640 | +9 |
| `MINES=0` | 28,314 | -684 | 1,630 | -1 |
| `CURRENTS=0` | 28,148 | -850 | 1,631 | 0 |
| `PARTICLES=0` | 28,592 | -406 | 1,615 | -16 |
| `MULTI_HULL=0` | 29,228 | **+230** | 1,630 | -1 |
| `SONAR=0` | 28,468 | -530 | 1,630 | -1 |
| `FOG_CHART=0 SONAR=0` | 26,424 | -2,574 | 1,374 | -257 |
| `HULL_DAMAGE=0` | 28,332 | -666 | 1,629 | -2 |
| `EXPLORATION=0` from default `full` | 14,800 | -2,868 vs 17,668 | 1,425 | -11 |

`MULTI_HULL=0` increasing flash by 230 B is real, not a typo: the alternate
code layout and clones are larger. `SHOP=0` also changes `.data` from 14 B to
26 B and raises static SRAM by 9 B. Do not add these deltas; use the matched
build for the configuration you actually ship.

`EXPLORATION=0` is measured from the default `full` build, not from the
playtest build, because `CAMPAIGN` requires `EXPLORATION`. A playtest build
with only `EXPLORATION=0` is not a valid feature set.

### Not measured

- **FX-backed voyage save**: no build exists yet. The FX move can change
  checkpoint code, EEPROM dependencies, SRAM, and possibly flash. This report
  does not estimate that delta.
- **Runtime-USB combined image**: not measured in this pass; the playtest is
  `SH_RELEASE_NO_USB=1`. Older docs have runtime-USB campaign/save numbers,
  but they are not this audio-inclusive candidate.
- **`SAVE_CHART=1` with `SAVE_VOYAGE=1`**: invalid by dependency; not built.
- **Pure SPI cost**: not separately measurable from `FX::drawBitmap/drawChar`
  without source-level instruction attribution; the 870 B `FX::*` group is
  the defensible bound.
- **Mini hardware, physical speaker, manual playthrough**: not performed.
- **EEPROM-vs-FX save cost comparison**: pending the FX implementation.

## SRAM and external FX assets

`.data` is 14 B of initialized SRAM copied from flash at boot; it is part of
the 28,998 B flash number.

| `.data` symbol | Bytes |
| --- | ---: |
| `Arduboy2Base::eachFrameMillis` | 1 |
| `s_uiAux` | 1 |
| `music::s_enabled` | 1 |
| `s_titleRow` | 1 |
| `s_present` | 1 |
| `FX::cursor` | 8 |
| alignment fill | 1 |
| **Total** | **14** |

`.bss` is 1,617 B of zero-initialized SRAM and costs no flash. Largest items:
`Arduboy2Base::sBuffer` 1,024 B, `s_chart` 256 B, `streamfx::g_player` 152 B,
`s_state` 112 B, `s_cache` 28 B, `FX::font` 6 B, `music::s_flight` 5 B. The
remaining 23 symbols are 1–4 B each and total 34 B.

**External FX assets are not MCU flash.** The cart image is 2,008,832 B
(`fxdata.bin`) with a 2,008,634 B payload (`fxdata-data.bin`), under the
2,097,152 B (2 MiB) cap. The MCU flash total is 28,998 B, independent of the
cart image size. EEPROM save storage is also outside MCU flash.

Stack: the package reports `test_memory_music` `stack_reserve=891` against the
600 B target. Reserve is measured from `__bss_end`, not from the top of SRAM;
static free SRAM is 929 B, so the measured high-water leaves 38 B below the
physical RAM top. This is a probe result, not a proof for every path.

## Current status

- This is the **EEPROM-based combined playtest** dated 2026-10-28, not a
  release approval.
- It includes `air-map-fix.patch` and `spawn-fix.patch`. The spawn fix places
  new-voyage, Continue, and recovery entry at the generated berth and snaps the
  follow camera; the package's `test_spawn` passes with position, camera,
  visible hull, and packed terrain checks.
- Package-reported validation: host 177 default / 231 campaign+save+music,
  0 failed; 8/8 device suites `P` (`test_boot`, `test_chart`,
  `test_exploration`, `test_memory_music`, `test_music`, `test_render`,
  `test_spawn`, `test_world`); `test_music` gap=72 ticks, map=1, underruns=0,
  errors=0; `test_memory_music` `stack_reserve=891`.
- No physical speaker listening, manual playthrough, or Mini hardware check is
  claimed.
- The default `full` profile still has `CAMPAIGN=0 SAVE_VOYAGE=0 MUSIC=0`.
  The 28,998 B image is the explicit combined build.
- **Pending:** voyage saves are moving from EEPROM to FX. Re-run this index
  after that change; the 3,216 B checkpoint/save category and the 14 B
  `.data`/1,617 B `.bss` split are the first places to expect movement.

## Reproduction commands

Candidate combined build (from the candidate source snapshot; `touch` only
prevents the generator from rerunning, it does not change content):

```sh
cd /private/tmp/sub-hauler-flash-index-20261028/sub-hauler
touch src/fxdata.h
make build PROFILE=full \
  BUILD_DIR=build/playtest DIST_DIR=dist/playtest \
  FEATURES_PY="python3 tools/features.py --set CAMPAIGN=1 --set SAVE_VOYAGE=1 --set MUSIC=1"
```

Map build adds only a linker map flag; the resulting HEX is byte-identical:

```sh
make build PROFILE=full BUILD_DIR=build/map DIST_DIR=dist/map \
  FEATURES_PY="python3 tools/features.py --set CAMPAIGN=1 --set SAVE_VOYAGE=1 --set MUSIC=1" \
  SIZE_FLAGS='--build-property compiler.cpp.extra_flags="$(CPP_EXTRA_FLAGS)" \
    --build-property compiler.c.extra_flags="$(C_EXTRA_FLAGS)" \
    --build-property compiler.c.elf.extra_flags="-mrelax -Wl,-Map=$PWD/build/playtest.map"'
```

Measurement and listing:

```sh
AVR=$HOME/Library/Arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin
$AVR/avr-size -A package/sub-hauler.ino.elf
$AVR/avr-objdump -h package/sub-hauler.ino.elf
$AVR/avr-nm -S --size-sort --radix=d -C package/sub-hauler.ino.elf
$AVR/avr-nm -S --radix=d -C package/sub-hauler.ino.elf | sort -n
grep -n "^Linker script and memory map" build/playtest.map
```

Ablations use the same command with a distinct `BUILD_DIR`/`DIST_DIR` and
`FEATURES_PY="python3 tools/features.py --set NAME=0 ..."`. The analysis
artifacts are:

- `/private/tmp/sub-hauler-flash-index-20261028/symbols_final.csv`
- `/private/tmp/sub-hauler-flash-index-20261028/nm-demangled.txt`
- `/private/tmp/sub-hauler-flash-index-20261028/source-manifest.txt`
- `/private/tmp/sub-hauler-flash-index-20261028/analysis.json`
- `/private/tmp/sub-hauler-flash-index-20261028/sub-hauler/build/playtest.map`

## Assumptions and caveats

- Exact whole-image totals come from `avr-size` sections, not symbol sums.
- Symbol categories are name/namespace heuristics reviewed against addresses.
  “Render” vs “UI” is a presentation split, not a linker fact; “physics” is the
  simulation/rules/world group.
- `third-party display/cart/SPI` is a group bound, not a pure SPI measurement.
- DWARF inline attribution is approximate because LTO can lose or merge inline
  records. It is used only for the cross-check and the main/ISR split.
- Feature deltas are whole-image matched rebuilds. They include code-layout and
  inlining effects and must not be summed.
- All numbers are for the EEPROM save implementation. No FX-save number is
  inferred.

## Appendix A — full sorted symbol listing

All 148 unique nonzero-size `.text` symbols, sorted by address. The duplicate
`EEPROMClass::read` LTO alias at `0x4702` is listed once.

| Addr | Size | Section | Category | Symbol |
| --- | ---: | --- | --- | --- |
| `0x00AC` | 119 | PROGMEM | physics/rules/world | `exploration_data::DATA` |
| `0x0123` | 35 | PROGMEM | campaign/ending | `campaign::kMissions` |
| `0x0146` | 10 | PROGMEM | physics/rules/world | `world::SURVEY_META` |
| `0x0150` | 6 | PROGMEM | campaign/ending | `campaign_view::kRestorationSites` |
| `0x0156` | 7 | PROGMEM | campaign/ending | `campaign_view::kMissionPanel` |
| `0x015D` | 13 | PROGMEM | third-party display/cart/SPI | `Arduboy2Core::lcdBootProgram` |
| `0x016A` | 32 | PROGMEM | physics/rules/world | `world::MINE_META` |
| `0x018A` | 16 | PROGMEM | physics/rules/world | `world::SETTLEMENT_META` |
| `0x019A` | 40 | PROGMEM | physics/rules/world | `world::CURRENT_META` |
| `0x01C2` | 12 | PROGMEM | physics/rules/world | `world::SERVICE_BERTH_META` |
| `0x01CE` | 72 | PROGMEM | music | `game_audio::kStreams` |
| `0x0216` | 8 | PROGMEM | music | `streamfx::ima::kIndexTable` |
| `0x021E` | 178 | PROGMEM | music | `streamfx::ima::kStepTable` |
| `0x02D0` | 32 | PROGMEM | physics/rules/world | `world::ROUTE_DEADLINE` |
| `0x02F0` | 16 | PROGMEM | physics/rules/world | `world::ROUTE_REWARD` |
| `0x030C` | 22 | code | boot/init/fini | `__do_copy_data` |
| `0x0322` | 16 | code | boot/init/fini | `__do_clear_bss` |
| `0x033C` | 30 | code | physics/rules/world | `game::hardWallImpact(game::State const&) [clone .constprop.175]` |
| `0x035A` | 56 | code | physics/rules/world | `world::routeEconomy(unsigned char, unsigned char, unsigned char&, unsigned int&) [clone .constprop.171]` |
| `0x0392` | 66 | code | render | `cave::setPixel(unsigned char*, int, int) [clone .constprop.168]` |
| `0x03D4` | 264 | code | render | `cave::fillRect(unsigned char*, int, int, unsigned char, unsigned char, bool) [clone .constprop.166]` |
| `0x04DC` | 220 | code | render | `cave::drawGantry(unsigned char*, int, int) [clone .constprop.165]` |
| `0x05B8` | 38 | code | physics/rules/world | `exploration::slow(game::State const&) [clone .constprop.162]` |
| `0x05DE` | 92 | code | physics/rules/world | `exploration::nearGate(game::State const&, exploration_data::Gate const&, int) [clone .constprop.160]` |
| `0x063A` | 142 | code | render | `exploration_view::glyph(unsigned char*, int, int, unsigned char) [clone .constprop.156]` |
| `0x06C8` | 86 | code | third-party display/cart/SPI | `Arduboy2Base::drawFastVLine(int, int, unsigned char, unsigned char) [clone .constprop.152]` |
| `0x071E` | 92 | code | third-party display/cart/SPI | `Arduboy2Base::drawFastHLine(int, int, unsigned char, unsigned char) [clone .part.2] [clone .constprop.151]` |
| `0x077A` | 70 | code | third-party display/cart/SPI | `Arduboy2Base::drawRect(int, int, unsigned char, unsigned char, unsigned char) [clone .constprop.149]` |
| `0x07C0` | 44 | code | physics/rules/world | `damage::resolveWallImpact(game::State&) [clone .constprop.145]` |
| `0x07EC` | 72 | code | physics/rules/world | `particles::spawn(game::State&) [clone .constprop.138]` |
| `0x0834` | 78 | code | physics/rules/world | `game::snapCamera(game::State&) [clone .constprop.135]` |
| `0x0882` | 490 | code | physics/rules/world | `jobs::refreshOffers(game::State&, unsigned char) [clone .constprop.120]` |
| `0x0A6C` | 60 | code | physics/rules/world | `station::rowKind(unsigned char, game::State const&) [clone .constprop.109]` |
| `0x0AA8` | 24 | code | physics/rules/world | `station::initialRow(game::State const&) [clone .constprop.108]` |
| `0x0AC0` | 14 | code | campaign/ending | `campaign_view::drawLamp(unsigned char*, int, int) [clone .constprop.87]` |
| `0x0ACE` | 74 | code | physics/rules/world | `chart::isExplored(chart::State const&, int, int) [clone .constprop.73]` |
| `0x0B18` | 92 | code | physics/rules/world | `shop::canBuy(game::State const&, shop::Item) [clone .part.84] [clone .constprop.52]` |
| `0x0B74` | 140 | code | render | `chart_view::clearMarkerBox(unsigned char*, int, int) [clone .constprop.46]` |
| `0x0C00` | 96 | code | render | `chart_view::drawDestinationMark(unsigned char*, int, int) [clone .constprop.44]` |
| `0x0C60` | 26 | code | physics/rules/world | `voyage_flow::prepareBoard(game::State&) [clone .constprop.32]` |
| `0x0C7A` | 38 | code | third-party display/cart/SPI | `Arduboy2Core::delayByte(unsigned char) [clone .constprop.2]` |
| `0x0CA0` | 48 | code | Arduino core ISR/timing | `micros` |
| `0x0CD0` | 40 | code | music | `streamfx::setMuted(bool)` |
| `0x0CF8` | 26 | code | music | `streamfx::pause()` |
| `0x0D12` | 12 | code | third-party display/cart/SPI | `FX::readEnd()` |
| `0x0D1E` | 20 | code | third-party display/cart/SPI | `FX::readBytesEnd(unsigned char*, unsigned int)` |
| `0x0D32` | 12 | code | third-party display/cart/SPI | `FX::readPendingUInt8()` |
| `0x0D3E` | 6 | code | third-party display/cart/SPI | `FX::readPendingLastUInt16()` |
| `0x0D44` | 6 | code | third-party display/cart/SPI | `FX::readPendingUInt16()` |
| `0x0D4A` | 16 | code | third-party display/cart/SPI | `FX::seekCommand(unsigned char, uint24)` |
| `0x0D5A` | 22 | code | third-party display/cart/SPI | `FX::seekData(uint24)` |
| `0x0D70` | 248 | code | music | `streamfx::(anonymous namespace)::fillRing()` |
| `0x0E68` | 18 | code | music | `streamfx::service()` |
| `0x0E7A` | 368 | code | music | `streamfx::start(streamfx::Stream const&)` |
| `0x0FEA` | 88 | code | render | `panel_view::draw(uint24, unsigned int, unsigned char) [clone .constprop.90]` |
| `0x1042` | 580 | code | third-party display/cart/SPI | `FX::drawBitmap(int, int, uint24, unsigned char, unsigned char)` |
| `0x1286` | 182 | code | third-party display/cart/SPI | `FX::drawChar(unsigned char) [clone .part.2]` |
| `0x133C` | 14 | code | third-party display/cart/SPI | `FX::writeByte(unsigned char)` |
| `0x134A` | 16 | code | third-party display/cart/SPI | `Arduboy2Core::buttonsState()` |
| `0x135A` | 22 | code | third-party display/cart/SPI | `Arduboy2Base::justPressed(unsigned char)` |
| `0x1370` | 494 | code | render | `Blit::draw(int, int, unsigned char, unsigned char, uint24, unsigned int, unsigned char) [clone .part.0]` |
| `0x155E` | 40 | code | render | `Blit::draw(int, int, unsigned char, unsigned char, uint24, unsigned int, unsigned char)` |
| `0x1586` | 38 | code | music | `music::setCue(unsigned char) [clone .part.82]` |
| `0x15AC` | 130 | code | physics/rules/world | `world::serviceBerthMeta(unsigned char, world::Berth&) [clone .part.12]` |
| `0x162E` | 132 | code | physics/rules/world | `world::currentMeta(unsigned char, world::Current&) [clone .part.11]` |
| `0x16B2` | 108 | code | physics/rules/world | `world::settlementMeta(unsigned char, world::Settlement&) [clone .part.9]` |
| `0x171E` | 28 | code | checkpoint/save | `voyage_save::crc16Update(unsigned int, unsigned char)` |
| `0x173A` | 26 | code | music | `music::overrideCue(unsigned char)` |
| `0x1754` | 30 | code | music | `music::resume()` |
| `0x1772` | 158 | code | UI/screens | `drawTitleScreen()` |
| `0x1810` | 18 | code | music | `music::pause()` |
| `0x1822` | 18 | code | music | `music::pauseForTransaction()` |
| `0x1834` | 90 | code | physics/rules/world | `chart::cellFromPixels(int, int, int&, int&)` |
| `0x188E` | 210 | code | physics/rules/world | `chart::revealAtPixels(chart::State&, int, int, unsigned char) [clone .constprop.71]` |
| `0x1960` | 8 | code | render | `panel_view::draw(uint24)` |
| `0x1968` | 58 | code | UI/screens | `drawPause()` |
| `0x19A2` | 38 | code | checkpoint/save | `beginSaveFailure(SaveResume)` |
| `0x19C8` | 84 | code | UI/screens | `station_view::nameFor(unsigned char)` |
| `0x1A1C` | 124 | code | campaign/ending | `campaign::missionPrerequisites(game::State const&, unsigned char)` |
| `0x1A98` | 94 | code | campaign/ending | `campaign::missionInfo(unsigned char, campaign::MissionSpec&)` |
| `0x1AF6` | 82 | code | campaign/ending | `campaign::missionCompleted(game::State const&, unsigned char)` |
| `0x1B48` | 32 | code | campaign/ending | `campaign::missionAvailable(game::State const&, unsigned char) [clone .part.39] [clone .constprop.131]` |
| `0x1B68` | 112 | code | campaign/ending | `campaign::workMissionAt(game::State const&, unsigned char, unsigned char) [clone .constprop.127]` |
| `0x1BD8` | 22 | code | campaign/ending | `endingPending()` |
| `0x1BEE` | 74 | code | campaign/ending | `campaign::missionReward(unsigned char)` |
| `0x1C38` | 328 | code | campaign/ending | `campaign::activeJobValid(game::State const&)` |
| `0x1D80` | 46 | code | physics/rules/world | `world::tileFromPixel(int)` |
| `0x1DAE` | 300 | code | physics/rules/world | `bool world::detail::rectHitsImpl<world::FxTables>(world::FxTables&, world::TileCache const*, int, int, int, int) [clone .constprop.19]` |
| `0x1EDA` | 76 | code | physics/rules/world | `world::surveyMeta(unsigned char, world::Survey&)` |
| `0x1F26` | 154 | code | physics/rules/world | `world::berthMeta(unsigned char, world::Berth&)` |
| `0x1FC0` | 58 | code | campaign/ending | `campaign::berthAvailable(game::State const&, unsigned char) [clone .part.41]` |
| `0x1FFA` | 296 | code | checkpoint/save | `voyage_save::scalarValid(voyage_save::Scalar const&)` |
| `0x2122` | 32 | code | physics/rules/world | `game::neutralFor(int)` |
| `0x2142` | 88 | code | physics/rules/world | `game::reset(game::State&) [clone .constprop.173]` |
| `0x219A` | 460 | code | physics/rules/world | `station::activate(unsigned char, game::State&) [clone .constprop.97]` |
| `0x2366` | 22 | code | physics/rules/world | `exploration_data::wordAt(unsigned int)` |
| `0x237C` | 74 | code | physics/rules/world | `exploration_data::pickup(unsigned char)` |
| `0x23C6` | 78 | code | physics/rules/world | `exploration_data::gate(unsigned char)` |
| `0x2414` | 170 | code | physics/rules/world | `exploration::blockingGate(game::State const&, int, int) [clone .constprop.161]` |
| `0x24BE` | 942 | code | render | `drawChart()` |
| `0x286C` | 94 | code | render | `text::number(unsigned int)` |
| `0x28CA` | 260 | code | campaign/ending | `drawEnding()` |
| `0x29CE` | 24 | code | campaign/ending | `beginEnding()` |
| `0x29E6` | 76 | code | render | `text::print(uint24)` |
| `0x2A32` | 144 | code | UI/screens | `station_view::drawStatus(game::State const&) [clone .constprop.93]` |
| `0x2AC2` | 438 | code | UI/screens | `shop_view::draw(game::State const&, unsigned char) [clone .constprop.49]` |
| `0x2C78` | 856 | code | UI/screens | `drawWork()` |
| `0x2FD0` | 100 | code | UI/screens | `station_view::printDestination(unsigned char, unsigned int, unsigned int)` |
| `0x3034` | 578 | code | UI/screens | `station_view::draw(game::State const&, unsigned char) [clone .constprop.91]` |
| `0x3276` | 60 | code | UI/screens | `showStation()` |
| `0x32B2` | 574 | code | physics/rules/world | `bool game::moveAxis<world::FxTables>(game::State&, world::FxTables&, world::TileCache const*, bool) [clone .constprop.15]` |
| `0x34F0` | 4486 | code | render | `drawPilot()` |
| `0x4676` | 20 | code | UI/screens | `showPilot()` |
| `0x468A` | 120 | code | checkpoint/save | `finishSaveResume()` |
| `0x4702` | 6 | code | checkpoint/save | `EEPROMClass::read(int) [clone .isra.0] [clone .lto_priv.204]` |
| `0x4708` | 352 | code | checkpoint/save | `voyage_save::SlotInfo voyage_save::inspectSlot<voyage_save::EepromStorage>(voyage_save::EepromStorage&, unsigned char) [clone .constprop.28]` |
| `0x4868` | 196 | code | checkpoint/save | `voyage_save::Inventory voyage_save::inspect<voyage_save::EepromStorage>(voyage_save::EepromStorage&)` |
| `0x492C` | 122 | code | checkpoint/save | `refreshBootStatus()` |
| `0x49A6` | 534 | code | checkpoint/save | `voyage_save::LoadResult voyage_save::load<voyage_save::EepromStorage>(game::State&, chart::State&, voyage_save::EepromStorage&) [clone .constprop.24]` |
| `0x4BBC` | 102 | code | checkpoint/save | `bool voyage_save::legacyChartValid<voyage_save::EepromStorage>(voyage_save::EepromStorage&) [clone .isra.97]` |
| `0x4C22` | 34 | code | checkpoint/save | `EEPROMClass::update(int, unsigned char) [clone .isra.2]` |
| `0x4C44` | 858 | code | checkpoint/save | `voyage_save::SaveResult voyage_save::save<voyage_save::EepromStorage>(game::State const&, chart::State const&, voyage_save::EepromStorage&) [clone .constprop.27]` |
| `0x4F9E` | 130 | code | checkpoint/save | `performRecovery()` |
| `0x5020` | 244 | code | checkpoint/save | `startNewVoyage(voyage_save::LegacyChart)` |
| `0x5114` | 44 | code | checkpoint/save | `saveVoyageNow()` |
| `0x5140` | 30 | code | checkpoint/save | `requestSave(SaveResume)` |
| `0x515E` | 82 | code | checkpoint/save | `leaveStation()` |
| `0x51B0` | 480 | code | music | `__vector_32` |
| `0x5390` | 148 | code | Arduino core ISR/timing | `__vector_23` |
| `0x5424` | 6926 | code | main/loop | `main` |
| `0x6F32` | 16 | code | avr-libc/libgcc | `eeprom_read_byte` |
| `0x6F42` | 28 | code | avr-libc/libgcc | `eeprom_write_byte` |
| `0x6F5E` | 40 | code | avr-libc/libgcc | `__divmodsi4` |
| `0x6F86` | 16 | code | avr-libc/libgcc | `__negsi2` |
| `0x6F96` | 20 | code | avr-libc/libgcc | `__muluhisi3` |
| `0x6FAA` | 68 | code | avr-libc/libgcc | `__udivmodsi4` |
| `0x6FEE` | 30 | code | avr-libc/libgcc | `__umulhisi3` |
| `0x700C` | 18 | code | avr-libc/libgcc | `__mulpsi3` |
| `0x701E` | 30 | code | avr-libc/libgcc | `__mulsi3` |
| `0x703C` | 24 | code | avr-libc/libgcc | `__udivmodqi4` |
| `0x7054` | 40 | code | avr-libc/libgcc | `__udivmodhi4` |
| `0x707C` | 38 | code | avr-libc/libgcc | `__divmodhi4` |
| `0x70A2` | 12 | code | avr-libc/libgcc | `__tablejump2__` |
| `0x70AE` | 12 | code | avr-libc/libgcc | `__mulhisi3` |
| `0x70BA` | 2 | code | avr-libc/libgcc | `__usmulhisi3` |
| `0x70BC` | 10 | code | avr-libc/libgcc | `__usmulhisi3_tail` |
| `0x70C6` | 56 | code | avr-libc/libgcc | `__prologue_saves__` |
| `0x70FE` | 54 | code | avr-libc/libgcc | `__epilogue_restores__` |

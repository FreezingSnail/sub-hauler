# Platform — sub-hauler

Canonical hardware, resource, and toolchain context for this project.
`README.md` and `AGENTS.md` point here; task prompts should link here too.
Keep dated measurements in the checkpoint table instead of copying them around.

## Target

| Property | Value |
| --- | --- |
| Board | Arduboy FX (`arduboy-homemade:avr:arduboy-fx`) |
| MCU | ATmega32u4 |
| Display | 128×64 SSD1306 OLED, native 1-bit |
| Framebuffer | 1,024 B (`Arduboy2Base::sBuffer`) |
| Cart | external FX flash; current `fxdata/fxdata.bin` image is 583,168 B; the unpadded 579,010 B payload is `fxdata/fxdata-data.bin`, with the 4,096 B save partition at `FX_SAVE_PAGE = 0xFFF0` |
| Display mode | native 1-bit via `Arduboy2Base` |
| Logical frame | `FRAME(x)` = `x`; one image per frame |
| Blitting | `Blit::draw` plus-mask blits into `sBuffer`; no OLED access |
| Text | external 6x8 FX font via `src/text.hpp`; no built-in Arduboy font/Print path |
| Presentation | one `FX::display(CLEAR_BUFFER)` after update/render |
| Frame rate | 60 fps (`arduboy.setFrameRate(60)`) |
| Toolchain | arduino-cli 1.2.0; arduboy-homemade:avr 1.4.0; AVR-GCC 7.3.0-atmel3.6.1-arduino7; `-mrelax -mcall-prologues`; LTO |
| Math | integer/fixed-point only; no `float`/`double` |

Story art checkpoint (2026-11-10, story beat wave): the New Voyage opening
reuses the arrival row, DEEP RELAY acceptance has a unique beacon-console
illustration, DEEP RELAY result keeps the wreck row, and HOME departure has a
unique loaded-sub row. The source sheet is 512×1216 with nineteen rows. See the
dated checkpoint table for current resource figures.

## Resource budgets

| Resource | Limit | Measurement |
| --- | --- | --- |
| Program flash | 29,696 B | `.text` + `.data` from `make size` |
| Flash planning line | 29,000 B, advisory | user accepted the 29,696 B hardware ceiling for the final stretch on 2026-10-28 |
| SRAM | 2,560 B | `.data` + `.bss` static; the remainder is stack/locals |
| Static SRAM planning ceiling | 1,700 B | dated whole-image target, raised from 1,680 B on 2026-10-23 |
| Stack | explicit reserve | static free bytes alone do not prove stack fit |

Final-stretch acceptance (2026-10-28): the user authorizes using program flash
up to the **29,696 B hardware limit**. The former 29,000 B planning ceiling
is advisory for this work; no additional flash reserve is required. Static
SRAM remains capped at **1,700 B** and measured stack reserve must remain at
least **600 B**. Prior task documents quoting a hard 29,000 B flash ceiling
are superseded by this decision.

Current fixed build (2026-11-11, after `sub-hauler-oad`): one configuration
always links the ATMlib2FX compact synth (`ATM_FX_COMPACT_LOOP=1`,
`ATM_FX_COMPACT_RING=32`) over the six authored C1 streams. The whole image is
**29,378 B flash / 1,665 B static RAM** (`.text/.data/.bss` **29,362 / 16 /
1,649 B**), leaving **318 B** free under the 29,696 B hardware flash limit and
**35 B** static headroom / **895 B** static free SRAM. The 29,000 B planning
line remains advisory; the image is 378 B over it under the user's 2026-10-28
hardware-ceiling acceptance. The developer `DEBUG=1` image is **29,688 B /
1,666 B** after the NOCLIP text hint removal, leaving **8 B** free; it is not a
shipping configuration. Build outputs are `build/arduino` / `dist/fx` and
`build/arduino-mini` / `dist/mini`.
Feature-profile, development, runtime-USB, streaming-backend and EEPROM
chart-save variants are retired.

The fixed build has no shop, currency, job payout, repeatable freight offer or
credit cache. Air is a fixed 300-unit tank with free berth refill and free
rescue; blowing ballast costs 4 air units/s while flooding stays free. The
station board is native (`WORK / UNDOCK`,
`summary / WORK / UNDOCK`, or `UNDOCK` at a work/service berth) and WORK lists
only story and survey rows. Mandatory story work cannot be abandoned. The
generated station/shop menu frames and their menu-only text were removed from
the cart by `sub-hauler-jxk.2`; no frame or label is packed or drawn.

Air retune (2026-11-10, `sub-hauler-646.10`): the 300-unit tank and 4 air
units/s blow cost are applied in `src/game.hpp`; flooding stays free and the
18-unit/s ballast rate is unchanged. The relaxed Tidewall-to-Benthos FIRST host
probe docks and delivers with **18 air** left, so the guard is a **5% reserve**
rather than the former 20%. A 270-unit tank with the same blow cost arrives
with 0 air and was rejected.

Thruster boost (2026-11-10, `sub-hauler-646.11`): owning the THRUSTER applies a
**2x** multiplier in `src/game.hpp` after the cargo penalty. Empty horizontal
top speed is 30 -> **60 px/s** (loaded 18 -> **36 px/s**) and thrust
acceleration is 34 -> **68 px/s^2** (loaded 22 -> **44 px/s^2**); the clamp and
acceleration apply in both directions, so reverse doubles from its half-power
17 px/s^2 to 34 px/s^2 empty. The ballast valve moves 18 -> **36 units/s** in
both directions. Blow air still drains at the fixed 4 units/s, so a full
boosted blow empties ballast in half the time and spends about half the air.
Pre-thruster handling is unchanged. Camera look-ahead grows to 30 px at boosted
top speed and the hull still stays inside the 128 px view; collision still
walks one pixel at a time, so the faster clamp cannot tunnel through rock.

Startup splash measurements and art provenance are in
[startup-splashes.md](startup-splashes.md). The new FX section appends after
all shipped menus. `story/scenes.json` is the single editable UTF-8 source for
scene order, prose and named image rows. `story/art.png` is the canonical
512×1216 Piskel sheet (nineteen rows of four 128×64 poses); retained pixelgen
bundles are provenance only. The canonical sources produce seventeen scenes:
seven result/survey decks, six departures, two joint-restoration variants, one
DEEP RELAY result and the New Voyage opening. Automatic pagination emits
forty-three cards (twenty-one image, twenty-two text) and ninety-eight frames
from nineteen shared animated scenes; consecutive text pages are allowed. The
`story-handoffs-v1` scenes replace the old
departure placeholders. The exact text, preview, trigger/replay/save contract
and 57,316 B RLE cart blob are in [story-beats.md](story-beats.md); the blob
appends after the chart hint and preserves every earlier offset and the 4 KiB
save page. A 122 B cold story-metadata blob appends after the story decks, and
the dock-bark address table ends the data layout before the save partition
(`sub-hauler-oad`).

Campaign, voyage save and synth music are unconditional. The stack target
remains 600 B.

## Stack measurement

Current fixed-build checkpoint (2026-11-03): **`stack_reserve=665`** for the
exact production `updateStation()`→`requestSave()`→`saveVoyageNow()` chain
with active synth music (`make fxtest-save-chain`), and **642** for the
combined `test_memory_music` probe, which also renders a fractional camera
through the terrain shift path and decodes every one of the 90 story-deck
frames through a 16 B scratch chunk; both exceed the 600 B target.
The device suites compile the serial marker with `SERIAL_BUFFER_SIZE=8` so their
USB-only RX buffer is not charged against the production no-USB stack path. All
fifteen default device suites print `P`, plus the separate `test_flight` probe.
The representative path includes salvage, cutter operation, object rendering,
chart gate markers and discovery HUD messages in addition to the earlier
flight/chart/hazard/roster/contract paths. The additional
`make fxtest-flight` probe passes at **11.096 ms maximum** over **842**
complete flight frames with active music, zero 16 ms deadline misses and zero
audio underruns/errors; `fail_mask=0` and `cases_ok=1`. See
`docs/camera-review.md` for coverage limits. AVR
`game::State` is **77 B**, including run-local tools, claims, gate
masks and interaction/notice state plus the campaign counters. The following method and older probe coverage explain the
measurement; dated older figures remain historical checkpoints.

Static free SRAM (`.data` + `.bss`) does not prove stack fit: the stack, locals,
and interrupt frames share the remaining bytes. The painted-stack probe is
`tst/fxdatatest/test_memory.ino`, compiled and run by the Ardens device harness.

Method:

1. `__bss_end` is the linker symbol for the first byte above all static RAM.
2. Read the live stack pointer (`SPL`/`SPH`) and set `paintTop = SP - 64`. The
   64 B guard keeps the active frame and one interrupt frame out of the paint.
3. Fill `[__bss_end + 1, paintTop]` with `0xA5` with interrupts disabled.
4. Re-enable interrupts and run 8 frames of the real path:
   `pollButtons` -> `view::frame` (native terrain streamed from FX each frame,
   collision cache filled on the first frame) -> `FX::display(CLEAR_BUFFER)`.
5. Scan upward for the first byte that is no longer `0xA5`; that address is the
   stack high-water mark.
6. `reserve = highWater - __bss_end`. The suite prints exactly one bare `P` when
   the reserve is at least 600 B, otherwise one bare `F`. Diagnostics stay off
   the marker line (`SH_STACK_DIAG=0` by default) because the Ardens harness
   matches the exact marker.

The result is conservative: an untouched byte reads back `0xA5`, and a path
that stays inside the guard region is measured as if it stopped at `paintTop`.
The marker print happens after the scan, so the final Serial writes are outside
the measured window; the 600 B target leaves room for them.

Target: **at least 600 B of reserve above `__bss_end`** on the fixed build.
This is the 2026-10-23 re-baseline. The earlier 800 B target predated the 258 B
persistent chart slice; the chart-inclusive probe measured **654 B**, the
chart+sonar probe **649 B**, the M7 mine+current probe **641 B**, the M7
damage+particle probe **616 B**, the M7 shop probe **606 B**, the M8 roster
probe **601 B**, the M8 holds probe **619 B**, the M9 polish probe **624 B**,
the cargo-free probe **816 B**, and the final UI/art/resource probe **812 B**,
so that checkpoint kept a 212 B margin. Do not relax this target further
without a new dated measurement and an explicit note here.

Historical probe coverage (passing 2026-10-24): `test_memory` includes the M6 chart path: a
static `chart::State` (the ownership the firmware uses), a spawn
reset/reveal, `revealAtPixels`/`revealOnStep` calls, and three
`chart_view::draw` + `FX::display(CLEAR_BUFFER)` frames, one with the
destination X lit and two blinked off. The M7 current path steps inside
generated current 0, draws the flow marks, and draws the pilot frame. The
particle path latches a deterministic hard wall impact, spawns/ages the
four-particle pool, and draws a pilot frame with debris. The shop path docks,
buys an air upgrade, and draws the one-row shop board. The retired M8 roster
path drew all three title screens (name, playable silhouette, length stat),
then flew the LEVIATHAN silhouette through the pilot frames. The current fixed
build draws one baked HAULER title panel and the HAULER pilot only; title and
pilot still read the baked FX sprites and the external font/string sections.
The cargo-free contract
path replaces the removed hold screens: it posts offers at a real berth,
accepts the first offer (auto-loading four units), draws the active station
board, undocks through the board action, and draws the first pilot frame. The
M9 reset path runs `game::resetWorld` (the aggregate `game::reset()` is the
same path used by boot), and the pause/save-failure screens are text-only and
add no deeper call chain. At that 2026-10-24 checkpoint the combined
`test_memory_music` probe reported **643 B** and the exact production save
chain reported **696 B** against the 600 B target; current figures are 660 B
and 702 B. The 2026-10-29 mine/hull-damage removal reduced `game::State` from
97 B to **90 B**.

At the 2026-10-24 checkpoint the firmware comparison was about **758 B** of reserve: that image had
**988 B** of static free SRAM and an estimated **230 B** stack depth (the
chart+sonar 222 B estimate plus roughly 8 B for the deeper hazard path). Do not
subtract the probe's stack locals from the firmware number: `game::State`,
`world::FxTables`, and the tile cache are stack locals in the probe but static
in the firmware, so doing so would charge those bytes twice. The probe's 812 B
already accounts for that state like the shipped image.

At the 2026-10-24 checkpoint static free SRAM was **988 B** and the image was **1,572 B** static, 128 B
under the 1,700 B planning ceiling. Removing the cargo-packing runtime freed
**98 B of static RAM**; the final resource pass removed another **20 B**.
That checkpoint used an **80 B** AVR `game::State` after the gl1.3/gl1.4
compaction removed diagnostic counters and the job clock and narrowed route
reward fields. The persistent chart (256 B), current metadata, particle pool,
hull-class byte, and delivery byte remain. The
2026-10-29 removal of mines and hull damage dropped `game::State` to **90 B**
in the current fixed build; there are no feature flags to disable. Any further
**SRAM** persistent state needs a carve of existing state first; there is no
room for another 256 B-class feature. The optional chart save is not SRAM: its
260 B block lives in EEPROM, so it does not consume static headroom. The
archival `cargoBank` generator output is no longer packed into external FX
flash after `sub-hauler-jxk.1`; its runtime state and reader were already gone.

The suite builds with `make fxtest-build FXTEST_ONLY=test_memory`; rerun it
after render/state changes:

```sh
make fxtest FXTEST_ONLY=test_memory
```

A bare `P` means the reserve is at least 600 B; `F` means it is not. Record the
dated high-water result in the checkpoint table after that run.

The active map format is documented in [levels/README.md](../levels/README.md).
Its object records are 91 B in PROGMEM and used directly for collision without
extra SPI transactions or a dynamic SRAM object array; the cart copy was
retired by `sub-hauler-jxk.1` under the approved ABI exception.
The original 88 B position-only world metadata and 128 B cave atlas retain
their storage approach. Generated `level.hpp` is active; `world.hpp` under
`src/generated` still describes the archival legacy sections, which are no
longer packed.

## SPI / display contract

- The OLED and FX cart share SPI.
- Cart reads happen in update/render, never during an OLED transaction.
- Normal presents once per frame with `FX::display(CLEAR_BUFFER)` after
  rendering; that call is the display transaction.
- `FX::begin(FX_DATA_PAGE)` opens the data partition. Voyage save builds use
  `FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE)` because the generated cart image ends
  with a 4 KiB `savesection`. Cart save reads belong in update/render, never
  inside an OLED transaction; erases/page programs pause music first.

## Save data

### Voyage checkpoints (cart partition)

The fixed build keeps fresh voyage checkpoints in the external cart save
region. `fxdata/fxdata.txt` ends with
`savesection 4096`; `make gen` emits `FX_SAVE_PAGE = 0xFFF0`,
`FX_SAVE_BYTES = 4096`, and `fxdata/fxdata-save.bin` (all `0xFF`). The packed
development image `fxdata/fxdata.bin` contains that region, so Ardens and a
physical cart see the same pages. The data prefix and every earlier FX symbol
keep their offsets.

The partition is one 4 KiB erase block. The only 512 B slot is at offset 0:
16 B header, 495 B payload area and commit byte at offset 511, programmed as two
256 B pages with the commit last. Schema 8 payloads are 25 B scalar (281 B with
the chart; 25 B without). `voyage_save::FxStorage` wraps `FX::seekSave`,
`FX::eraseSaveBlock`, `FX::programSavePage` + `FX::seekCommand(SFC_WRITE)` +
`FX::writeByte` and `FX::waitWhileBusy`; all four use the same runtime save page
selected by `FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE)`.

Saving erases the single block, so **a power interruption during erase/write
may lose the previous checkpoint**. A torn slot never validates: boot/load
distinguish blank, incompatible and corrupt data, never mutate live state on a
rejected load, and the title offers a clean New Voyage (a valid record still
needs replacement confirmation). This is intentionally not a power-fail atomic
two-copy protocol. No voyage byte is written to internal EEPROM. The 4 KiB
region adds 0 B of static SRAM and no data-image bytes; the adapter is a
stateless stack object and the page stream is generated from live state.
Schema 1/2 EEPROM records, schema 3 dual-slot carts, schema 4 and schema 7
slots, and the retired chart block are intentionally not migrated.

### Optional standalone chart save

The retired standalone EEPROM chart writer is documented here only for historical
context; the fixed build does not compile it. It persists the explored
chart in 260 B and adds 0 B static SRAM. It currently writes from address 0 and
therefore overlaps the Arduboy2 system prefix (`EEPROM_STORAGE_SPACE_START ==
16`); it is not a safe allocation and is not used by the fixed build.

Record layout, with the version following `world::LAYOUT_VERSION` (currently 3):

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 1 | magic `'S'` (0x53) |
| 1 | 1 | magic `'H'` (0x48) |
| 2 | 1 | world layout version; older maps are rejected without migration |
| 3 | 1 | CRC-8/ATM (poly 0x07, init 0) over the 256 explored bytes |
| 4 | 256 | explored mask, identical to `chart::State.explored` |

`chart_save::load()` validates magic, version, and the whole checksum before
copying, so a blank EEPROM (`0xFF`), corrupt block, or wrong version is ignored
and the reset chart stays empty; there is no partial load. The sketch calls it
once in `setup()` after `resetRun()`, and calls `chart_save::save()` only on the
chart-close transition. No flight, station, display, or per-frame path writes
EEPROM.

## Measurement checkpoints

Dated whole-image numbers are evidence, not guaranteed headroom. Entries before
2026-10-29 are historical: references to `full`/`lean`/`core`/`min`
profiles, `MUSIC_BACKEND`, `SH_RELEASE_NO_USB`, `SH_FEATURE_*`, developer mode
or EEPROM chart save describe retired variants, not the shipping build. The
current developer-only `DEBUG=1` image is documented in
[dev-flow.md](dev-flow.md) and is not part of the shipped matrix.

| Date | Flash | Static RAM | Toolchain / notes |
| --- | --- | --- | --- |
| 2026-11-11 | 29,146 B | 1,665 B | Redundant berth air readout removal (`sub-hauler-39u`): berth service always refills to capacity, so the station board no longer draws the tank gauge or the `AIR <current>/<capacity>` status line. Removed `drawAirGauge`/`drawStatus`, their layout constants and static asserts, and the now-unreferenced `txt_air_status = 'AIR '` declaration (the HUD keeps `txt_air`). Before `.text/.data/.bss` **29,362 / 16 / 1,649 B**, **29,378 B flash / 1,665 B static**, debug **29,688 B / 1,666 B**, FX payload/image **579,010 / 583,168 B**, `FX_DATA_PAGE=0xF71A`. After **29,146 B flash / 1,665 B static**, `.text/.data/.bss` **29,130 / 16 / 1,649 B**, **550 B** hardware flash free, debug **29,456 B / 1,666 B**, FX payload/image **579,005 / 583,168 B** (−5 B payload, image unchanged), `FX_DATA_PAGE=0xF71A` and `FX_SAVE_PAGE=0xFFF0` unchanged; later FX symbols shift down **5 B**. `make gen-check` deterministic; host **262**, tooling **161**, docs **33** + preview **8**; targeted `test_boot` and `test_menu` (**29** pixel cases) pass; Mini **29,146 / 1,665 B**. No physical playthrough claimed. |
| 2026-11-11 | 29,378 B | 1,665 B | Cold cart tables and debug overlay trim (`sub-hauler-oad`): moved story metadata **122 B**, dock-bark address table **284 B** (32 B offsets + 252 B text) and berth icons **130 B** packed from program flash/stack tables to the FX cart; removed the DEBUG NOCLIP text hint. Before **29,644 B flash / 1,665 B static**, FX payload/image **578,474 / 582,656 B**, `FX_DATA_PAGE=0xF71C`, debug image **30,064 B / 1,666 B** (368 B over). After `.text/.data/.bss` **29,362 / 16 / 1,649 B**, **29,378 B flash / 1,665 B static**, **318 B** hardware flash free, FX payload/image **579,010 / 583,168 B** (+536/+512 B), `FX_DATA_PAGE=0xF71A`, `FX_SAVE_PAGE=0xFFF0` unchanged, debug **29,688 B / 1,666 B** (**8 B** free). `make gen-check` deterministic; host **262**, tooling **161**, docs **33** + preview **8**; targeted `test_boot`, `test_menu`, `test_story` (**10,760 µs** max) and `test_work` pass; Mini **29,378 / 1,665 B**. |
| 2026-11-11 | 29,644 B | 1,665 B | Unused FX pack retirement (`sub-hauler-jxk`, user-approved one-time append-only ABI exception): retired **23 packed assets** (`worldTiles`, `worldChart`, `cargoBank`, `explorationObjects`; `hero`, `minnow`, `leviathan`; `panel_ending_stats`, `panel_pause_rescue`, `panel_pause_resume`, `title_keep`, `work_active`, `work_details`, `work_rows`, `hud_controls`, `shop_hints`, `shop_menu`, `station_hints`, `station_menu`, `story_beacon`, `story_first`, `story_lift`, `story_pump`) and **61 unreferenced packed strings**. Title sheets compacted from 30 to 8 (`title_menu`) and 5 to 1 (`title_replace`) frames; the HUD control stamp was removed and `hud_notices` compacted from 14 to 11 live frames, fixing `kNoticeFoundLens` selecting the former credits frame. Final symbol count **114 = 84 strings + 30 assets**, down from **198 = 145 strings + 53 assets**. Baseline `a040071` **29,652 B flash / 1,665 B static**, FX payload/image **672,205 / 676,352 B**, `FX_DATA_PAGE=0xF5AE`; final `.text/.data/.bss` **29,628 / 16 / 1,649 B**, **29,644 B flash / 1,665 B static**, **52 B** hardware flash free, FX payload/image **578,474 / 582,656 B**, `FX_DATA_PAGE=0xF71C`, `FX_SAVE_PAGE=0xFFF0` unchanged. Removed **93,731 B payload / 93,696 B cart image** and **8 B flash / 0 B static**; the three waves contributed **−11,009/−11,008**, **−82,133/−82,176** and **−589/−512 B**. Host **262**, tooling **158**, docs **33** + preview **8**, **15/15** device suites P, save-chain **680**, combined music **634**, runtime-page P, flight **842** frames **11,788 µs max**, zero 16 ms misses/audio errors, `make gen-check` deterministic, Mini **29,644 / 1,665 B**. No physical playthrough claimed. |
| 2026-11-11 | 29,654 B | 1,665 B | Legacy cart table retirement (`sub-hauler-jxk.1`, approved one-time ABI exception): removed shipping `worldTiles`, `worldChart`, `cargoBank`, `hero`, `minnow`, `leviathan` and `explorationObjects`; HAULER is the only pilot sheet. Archival generators/outputs (`world.bin`, `cargo.bin`, `exploration-objects.bin`, `gen_cargo.py`, `gen_pilot.py`) remain but are not packed. `.text/.data/.bss` **29,638 / 16 / 1,649 B**; **42 B** hardware flash free. Host **264**, tooling **166**, gen-check deterministic, targeted `test_boot` P and `test_world` P. FX payload/image **661,196 / 665,344 B**, **11,009 B payload / 11,008 B image** smaller than the prior **672,205 / 676,352 B**; `FX_DATA_PAGE=0xF5D9`, `FX_SAVE_PAGE=0xFFF0` unchanged. No physical playthrough claimed. |
| 2026-11-10 | 29,652 B | 1,665 B | Equipment-cove landmarks and lens-cave texture (`sub-hauler-646.8`, `.9`): the four equipment coves get distinct wall-etched wreck silhouettes (`cutter-tender` at `W`, `thruster-pod` at `BR`, `workshop-lock` at `CW`, `trench-lens` at `DL`) because their pocket water is protected route corridor; `DL` gains a scoped `texture: "lens"` override with `lens-shard` wall growths and concentric lens rings. Collision, chart, object stream, `level.hpp`, `exploration.hpp` and `fxdata.h` are byte-identical to the preceding image; only the **409,600 B** native terrain section changes. `.text/.data/.bss` **29,636 / 16 / 1,649 B**; **44 B** hardware flash free. Host **264**, tooling **159**, docs **33** + preview **8**; targeted `test_world` P; gen-check deterministic. FX payload/image **672,205 / 676,352 B**. No physical playthrough or human art review claimed. |
| 2026-11-10 | 29,652 B | 1,665 B | Story beat wave (`sub-hauler-646.6`, `.12`, `.13`, `.15`): New Voyage opening deck (scene 17, RAM-only, skipped by Continue), unique `relay-briefing` and `home-departure` rows; DEEP RELAY result keeps `deep-relay`, HOME result keeps `home-lights`. Sheet **512×1216**, nineteen rows. `.text/.data/.bss` **29,636 / 16 / 1,649 B**; **44 B** hardware flash free. Host **264**, tooling **157**, docs **33** + preview **8**; targeted `test_story` P at **10,752 µs** max over 98 frame CRCs; gen-check deterministic. `make gen`: 17 scenes / 43 cards / 98 frames, story **61,365 B**; FX payload/image **672,205 / 676,352 B**, **1,420,800 B** below 2 MiB with **51 B** alignment padding; `storyBeats=0x095200`, `FX_DATA_PAGE=0xF5AE`, `FX_SAVE_PAGE=0xFFF0` unchanged. No physical playthrough or human art review claimed. |
| 2026-11-10 | 29,634 B | 1,665 B | Archival ADPCM removal (`sub-hauler-646.18`): the six unused `raw_t audio_*` streams (1,424,640 B) leave the shipping `fxdata.txt` layout as the approved one-time append-only ABI exception; no runtime code referenced them. `.bin` files, manifest, `src/generated/audio.hpp` and `make gen-audio` remain archival and are not compiled into firmware or tests. `.text/.data/.bss` **29,618 / 16 / 1,649 B**; **62 B** hardware flash free. Host **264**, gen-check deterministic, targeted `test_boot` P. FX payload/image **667,863 / 672,000 B**, **1,425,152 B** below 2 MiB with **41 B** data alignment padding; `FX_DATA_PAGE=0xF5BF`, `FX_SAVE_PAGE=0xFFF0` unchanged. Every symbol through `txt_a_finish` keeps its offset; later symbols shift down by 1,424,640 B. No physical playthrough or speaker listening claimed. |
| 2026-11-09 | 28,702 B | 1,650 B | Thruster tide gates, return-shaft tides, survey-lens relocation, and refreshed story art: all nine tide zones are thruster gates; without the thruster they push downstream and block upstream, and owning the thruster disables force, blocking and flow marks. The two former T thruster gates are now tides: Basalt descent pushes up and Jet approach pushes left. The return-shaft shortcut keeps a seal-only P gate and gains a rightward gate tide and an upward tide above it; Pipe Junction pushes down, Cormorant pushes up. The survey lens moves to the upper-west Trench Cove `(36,78)`, above the pressure line at the end of a long jagged switchback route from Trench Refuge; Trench Refuge→Deep Anchor widens with a 10×7 chamber so the station approach dominates. Guide full map/route maps use the fully rendered native world. `.text/.data/.bss` **28,690 / 12 / 1,638 B**; 994 B hardware flash free, 298 B under the advisory 29,000 B line, 50 B static headroom, 910 B static free. Host **246**, tooling **155**, docs **32** + preview **8**; **15/15** default device suites P; save-chain **703**, combined music **626**; runtime-page P; flight **842** frames, **11,440 µs max**, 0 misses, gap **1**, 0 underruns/errors, `fail_mask=0`; Mini **28,702 / 1,650 B**; gen-check deterministic. `make gen`: 16 scenes / 41 cards / 89 frames, story **57,023 B**; FX payload/image **2,092,503 / 2,096,640 B**, **512 B** below 2 MiB; `FX_SAVE_PAGE=0xFFF0` unchanged. No physical playthrough, human map review or speaker listening claimed. |
| 2026-11-07 | 28,418 B | 1,649 B | Wall-spanning currents (`sub-hauler-vc6`): all eight zones scan their full flow strip and extend through adjoining side openings to solid walls, then overlap the walls by one tile. Per-zone tile counts retain compact AVR metadata while runtime extents support more than 255 pixels. Guide map regenerated through `make gen`. `.text/.data/.bss` **28,406 / 12 / 1,637 B**, 1,278 B hardware flash free. Host **244**, generator tests **117**, docs **32** + preview **8**; targeted `test_world`, `test_render`, `test_memory_music`, `test_save_chain` pass. Stack reserve **641 B** combined music and **703 B** save-chain. Flight **842** frames, **11,428 µs max**, zero misses, gap **1**, zero audio underruns/errors. `make gen-check` deterministic; FX payload/image **2,092,670 / 2,097,152 B**. |
| 2026-11-07 | 28,396 B | 1,649 B | Linear DEEP RELAY progression (`sub-hauler-yt0`) and guide/full-map refresh (`sub-hauler-gp0`): HOME now requires BEACON + DEEP RELAY; DEEP RELAY requires BEACON and cutter/seal/thruster/lens and appears as the Deep Anchor WORK row. Pre-linear in-flight HOME saves still load, validate and complete without fabricating EXPEDITION; completed legacy HOME saves remain valid. `.text/.data/.bss` **28,384 / 12 / 1,637 B**; 1,300 B hardware flash free, 604 B under the advisory 29,000 B line, 51 B static headroom, 911 B static free. Host **244**, tooling **150**, **15/15** default device suites P; save-chain **703**, combined music **646**; runtime-page P; flight **842** frames, **11,412 µs max**, 0 misses, gap 1, 0 underruns/errors, `fail_mask=0`; Mini **28,396 / 1,649 B**; gen-check deterministic. `make gen`: 16 scenes / 41 cards / 89 frames, story **57,190 B**; FX payload/image **2,092,670 / 2,097,152 B**, 183 B before the save section; `FX_SAVE_PAGE=0xFFF0` unchanged. The generated overview adds current/equipment-cove/survey/delivery labels and the guide embeds it. No physical playthrough, human map review or speaker listening claimed. |
| 2026-10-20 | 6,636 B | 1,214 B | arduino-cli 1.2.0; arduboy-homemade:avr 1.4.0; AVR-GCC 7.3.0-atmel3.6.1-arduino7; `make size` |
| 2026-10-22 | 6,840 B | 1,217 B | M1 cave render (static camera, 16x8 visible window); full/lean/core/min identical; no Ardens run |
| 2026-10-22 | 10,584 B | 1,259 B | M2 pilot view (world step, HAULER silhouette, HUD, D-pad input); full/min identical; no Ardens run |
| 2026-10-22 | 12,346 B | 1,276 B | M3 stations (auto-dock, station board, refill/repair/undock); full/core identical, min unchanged at 10,584 B / 1,259 B; no Ardens run |
| 2026-10-22 | 14,410 B | 1,319 B | M3 jobs/loop (offers, deadlines, delivery/credits, auto-load hint); full/lean/core identical, min unchanged; device suites P |
| 2026-10-22 | 18,892 B | 1,416 B | M5 cargo shell (HAULER cutaway, 11x3 hold, tray, status); full/core/lean identical, min unchanged; device suites P |
| 2026-10-22 | 21,836 B | 1,418 B | M5 cargo rules (selection, pick/lift, move, support, rotation, undo, feedback); full/core identical, min unchanged at 10,584 B / 1,259 B; device suites P, stack probe includes the cargo path |
| 2026-10-22 | 22,038 B | 1,420 B | M5 cargo integration (job handoff, A+B commit, auto-undock, fallback, LOAD CARGO row); full/core/lean identical, min unchanged; device suites P, stack probe includes commit/undock |
| 2026-10-23 | 23,286 B | 1,678 B | M6 fog chart (64x32 explored set, paused 128x64 chart, B toggle); full/lean identical, core 22,038 B / 1,420 B, min 10,584 B / 1,259 B; `test_memory` P at 654 B under the 600 B target (chart in probe), other suites P |
| 2026-10-23 | 23,770 B | 1,680 B | M6 sonar (A ping: air cost, 3+level cell reveal, 120-tick cooldown, 24-tick pilot ring); full/lean identical, core 22,038 B / 1,420 B, min 10,584 B / 1,259 B; `test_memory` P at 649 B under the 600 B target (chart + sonar in probe), other suites P |
| 2026-10-29 | 28,168 B | 1,656 B | Mines and hull strength removed: eight fixed mines, hull HP/damage/cooldown/edge flash, hull-loss failure screen, HULL PLATE shop row, hull-pod pickup and hull status readouts are gone; hull-class roster, currents, wall-impact debris, air-tank shop, docking/service and checkpoints remain. `.text/.data/.bss` **28,156 / 12 / 1,644 B**; 1,528 B hardware flash free, 44 B static headroom, 904 B static free. Save schema **6**: scalar **33 B**, no-chart payload **33 B**, chart payload **289 B**, signature **0x69** (bits 1/2/4 retired); schema 5 slots are incompatible with no migration. `sizeof(game::State)` **90 B**. Host **230**, tooling **116**, **14/14** default Ardens suites P plus `test_memory` P; save-chain **696**, combined music **643**; flight 842 frames **11,692 µs max**, 0 misses, gap 1, 0 underruns/errors; runtime-page P; gen-check deterministic. FX payload/image **2,035,454 / 2,039,552 B**. No physical playthrough or speaker check. |
| 2026-10-23 | 25,334 B | 1,680 B | M7 hazards (8 fixed mines, one mask byte, circle/hull hit, one-frame spark, 8 generated current zones with Q8 acceleration and flow marks); full; lean 23,760 B / 1,679 B, core 22,038 B / 1,420 B, min 10,584 B / 1,259 B; `test_memory` P at 641 B under the 600 B target (chart + sonar + mine + current in probe), other suites P |
| 2026-10-23 | 26,088 B | 1,697 B | M7 damage/particles (16 px/s wall threshold, 24-tick cooldown, edge brackets, 4-slot 16 B debris pool); full; lean 24,096 B / 1,680 B, core 22,394 B / 1,421 B, min 10,586 B / 1,259 B; `test_memory` P at 616 B under the 600 B target (chart + sonar + mine + current + damage/particle in probe), other suites P |
| 2026-10-23 | 27,082 B | 1,690 B | M7 shop (AIR TANK 60, HULL PLATE 70, SONAR 50, max level 3, refill/repair on buy, SHOP station row, Screen::Shop); full; SHOP=0 full 26,088 B / 1,697 B, lean 24,096 B / 1,680 B, core 22,394 B / 1,421 B, min 10,586 B / 1,259 B; `sizeof(game::State)` 91 B; `test_memory` P at 606 B under the 600 B target (chart + sonar + mine + current + damage/particle + shop in probe), other suites P |
| 2026-10-23 | 27,962 B | 1,689 B | M8 roster (title selection, MINNOW/LEVIATHAN 24-26 px pilot art, one hull-class byte); full; `MULTI_HULL=0` full 27,042 B / 1,688 B, lean 24,962 B / 1,679 B, core 22,368 B / 1,420 B, min 10,568 B / 1,259 B; `sizeof(game::State)` 92 B; `test_memory` P at 601 B under the 600 B target (title + all hull art paths), other suites P |
| 2026-10-23 | 28,792 B | 1,690 B | M9 polish (pause/failure screens, one `lastDelivery` byte, LATE/SHORT board note, aggregate `game::reset()`); full; lean 25,800 B / 1,680 B, core 22,244 B / 1,421 B, min 10,456 B / 1,259 B; `sizeof(game::State)` 93 B; `test_memory` P with `stack_reserve=624` under the 600 B target (reset path included), other suites P |
| 2026-10-23 | 28,792 B | 1,690 B | M9 chart save default off; feature-off image is byte-identical to the M9 polish image (`.text` 28,722 B, `.data` 70 B, SHA-256 of `.hex` unchanged); `SAVE_CHART=1` full variant 29,352 B / 1,690 B (+560 B flash, +0 B static), lean variant 26,238 B / 1,680 B (+438 B); `test_memory` with save path P at 620 B for the variant and 624 B with the flag off; 75 tool tests; full/lean/core/min device suites P |
| 2026-10-23 | 28,792 B | 1,690 B | M9 release freeze at `4f66830`: full gate passes; full/lean/core/min **28,792 / 25,800 / 22,244 / 10,456 B** flash and **1,690 / 1,680 / 1,421 / 1,259 B** static; mini full 28,792 B / 1,690 B; 156/123/99/46 host tests, 75 tool tests, four device suites P; `test_memory` diagnostics `stack_reserve=624` (600 B target, 24 B margin); 904 B hardware flash free, 208 B under 29,000 B, 10 B static headroom; default `SAVE_CHART=0` FX `.hex` rebuild SHA-256 `ab3929ac43e952b987dfb0391eee54e77eeda75f0811bb9765cd04a5a530e5b0` (deterministic across two builds; docs-only diff from HEAD); no manual playthrough |
| 2026-10-23 | 28,988 B | 1,690 B | Post-M9 fixes: idle cargo frames skip composition and the OLED transaction; cargo horizontal runs batch page/bit lookup; undock uses a ring latch instead of a 1.5 s timer; A+B pause requires fresh edges; cargo resume preserves a partial hold. Full matrix **28,988 / 26,120 / 22,522 / 10,456 B** flash and **1,690 / 1,680 / 1,421 / 1,259 B** static; mini full 28,988 B / 1,690 B; 157/124/100/46 host tests, 75 tool tests, four device suites P; `test_memory` P at **624 B** reserve; 708 B hardware flash free, **12 B** under 29,000 B; `SAVE_CHART=1` full 29,368 B / 1,690 B (**+560 B**, 328 B under hardware, 368 B over planning), lean 26,480 B / 1,680 B |
| 2026-10-24 | 28,930 B | 1,690 B | Post-M9 menu fix: station/shop/pause/failure/cargo idle frames skip composition and the OLED transaction; selected rows use a `fillRect` bar with the font background disabled; the dock latch calls `nearestDock` once. Full matrix **28,930 / 26,020 / 22,406 / 10,456 B** flash and **1,690 / 1,680 / 1,421 / 1,259 B** static; mini full 28,930 B / 1,690 B; 157/124/100/46 host tests, 75 tool tests, four device suites P; `test_memory` P at the 600 B target (prior diagnostic **624 B**); 766 B hardware flash free, **70 B** under 29,000 B; `SAVE_CHART=1` full 29,330 B / 1,690 B (**+400 B**, 366 B under hardware, 330 B over planning) |
| 2026-10-24 | 20,196 B | 1,592 B | Cargo-free contract loading (`sub-hauler-gl1.1`): removes `Screen::Cargo`, LOAD CARGO row, cargo commit chord/hold-B timer, `s_cargo`, packing rules/runtime, and the `CARGO_LOAD` flag; accept auto-loads four units and stays docked, explicit UNDOCK/B starts travel. Full matrix **20,196 / 17,362 / 14,756 / 10,456 B** flash and **1,592 / 1,580 / 1,321 / 1,259 B** static; mini full 20,196 B / 1,592 B; 129/96/72/38 host tests, 75 tool tests, three device suites P; `test_memory` diagnostics **`stack_reserve=816`** (600 B target, 220 B margin); 9,500 B hardware flash free, 8,804 B under 29,000 B; 108 B static headroom, 968 B static free; `make gen-check` deterministic, shipped `cargoBank` offset/bytes unchanged; `test_cargo` retired |
| 2026-10-24 | 17,548 B | 1,572 B | Cargo-free resource pass (`sub-hauler-gl1.2`–`gl1.8`): compact world metadata 148→88 B PROGMEM with no RAM/cart-read change; route economy generated; diagnostic counters and job clock removed, route rewards narrowed, AVR `game::State` 80 B; surviving UI strings/6x8 font and three pilot silhouettes moved to FX, default Arduboy font/Print absent from ELF. Full matrix **17,548 / 14,792 / 12,626 / 9,142 B** flash and **1,572 / 1,560 / 1,302 / 1,252 B** static; mini full 17,548 B / 1,572 B; 132/99/75/39 host tests, 77 tool tests, `test_boot`/`test_memory`/`test_world` all P; full diagnostics **`stack_reserve=812`** (600 B target, 212 B margin); 12,148 B hardware flash free, 11,452 B under 29,000 B; 128 B static headroom, 988 B static free; savings vs cargo-free baseline **2,648 / 2,570 / 2,130 / 1,314 B flash** and **20 / 20 / 19 / 7 B static**; `cargoBank` remains at **0x001AC2** with unchanged bytes/order, FX payload **13,012 B** (`fxdata/fxdata-data.bin`), padded flashable image **13,056 B** (`fxdata/fxdata.bin`); no manual playthrough or device soak |

| 2026-10-25 | 20,500 B | 1,583 B | Authored exploration map: 32 rooms/42 passages, 9 equipment gates, 8 unique coves; active collision/chart/object sections append after the unchanged 34,540 B prefix. Packed payload/image **41,553 / 41,728 B**. Full/lean/core/min flash **20,500 / 17,668 / 15,166 / 9,142 B**, static **1,583 / 1,571 / 1,313 / 1,252 B**; Mini full **20,500 / 1,583 B**. Full run state **91 B** (+11). Host **140/107/82/39**, optional chart-save host **143**, tooling **88**, all **4** Ardens suites P; expanded stack probe **775 B** at target600. `make gen-check` deterministic; real hull/tool reachability validated for all eight capability subsets. Tools/claims/open grates remain run-local; campaign/ending/voyage save not implemented; no manual playthrough or physical-device soak. |
| 2026-10-26 | 20,178 B | 1,583 B | First campaign slice (`sub-hauler-7px.5`): FIRST/PUMP campaign work, PUMP survey rings, WORK page, generated Pump Refuge/Lower Gantry service metadata, native restoration overlays and service-berth Continue restore. Full/lean/core/min **20,178 / 17,368 / 14,756 / 8,750 B** flash and **1,583 / 1,571 / 1,313 / 1,252 B** static; full `.text/.data/.bss` **20,122 / 56 / 1,527 B**; Mini full **20,178 / 1,583 B**; host tests **153/120/92/47**, campaign overrides **195/161/133**, DEV **158/125/97/52**, campaign DEV **200/166/138**; tooling **108**, all six Ardens suites P; `test_memory` diagnostics **`stack_reserve=756`** (600 B target). Campaign-enabled full AVR override compiles to **29,998 B / 1,660 B** but does not link: 302 B over hardware and 998 B over the 29,000 B planning line, so CAMPAIGN/SAVE_VOYAGE remain default-off until task .6 trims the combined sequence. No manual playthrough or physical-device soak. |
| 2026-10-28 | 27,010 B | 1,619 B | Fresh voyage checkpoint in one Arduboy FX cart save block (`sub-hauler-7px.24`): `savesection 4096`, `FX_SAVE_PAGE=0xFFF0`, one 512 B schema 4 slot committed over two 256 B pages with commit last. Runtime `FX::programSavePage` is the page base for seekSave/erase/page writes. No voyage byte touches internal EEPROM; schema 1/2/3 records and the old chart block are not migrated. A power interruption during erase/write may lose the checkpoint; torn records never load and the title offers a clean New Voyage. First checkpoint persists the spawn chart reveal; early PUMP/LIFT survey bits remain valid before FIRST. Full campaign+save+music **27,010 B / 1,619 B** (`.text/.data/.bss` **26,998 / 12 / 1,607**), 1,990 B under the 29,000 B planning line, 81 B static headroom, 2,686 B hardware flash free; `test_save_chain` **`stack_reserve=666`** for the exact `updateStation()`→`requestSave()`→`saveVoyageNow()` production path with active music (`save_chain=1`, `save_underruns=0`, `save_errors=0`); combined `test_memory_music` **646**, campaign+save no-music `test_memory` **728**, music **underruns=0 errors=0**. Full default is **27,010 / 1,619 B**; lean/core/min remain **14,406 / 11,812 / 5,912 B** flash and **1,418 / 1,159 / 1,115 B** static; FX payload/image **2,008,674 / 2,012,928 B** (4,096 B save partition, earlier data offsets unchanged). Host **242** full default (**178** with campaign off); tooling **135**; `make gen-check` deterministic; all **11** default full Ardens suites pass, including `test_save` blank/valid/repeated/corrupt/interrupted-commit, exact `test_save_chain`, `test_music`, and emulator-only runtime-page cases. Mini campaign+save+music repeats **27,010 / 1,619 B**. No manual playthrough or physical-device save writes. |
| 2026-10-28 | 27,938 B | 1,619 B | True pixel-granularity camera scrolling (`sub-hauler-7px.22`): `cave::Camera` carries floor tile plus `subX/subY`; `cameraAtPixel()` clamps to `[0,2432] × [0,1216]`; collision/cache stay tile-based. Terrain stream keeps the X-aligned 8×128 B path and adds a sub-page-Y path that reads the next source page, combines in place through the next framebuffer page, and fetches the final upper row as four 32 B tail bursts (12 reads / 1,152 B worst case, 32 B stack scratch). Settlements, campaign markers, hazards/currents, particles, exploration markers and pilot all map from `pixelOriginX/Y`. Full default **27,938 B / 1,619 B** (`.text/.data/.bss` **27,926 / 12 / 1,607**), **1,062 B** under the 29,000 planning line, 1,758 B hardware free, 81 B static headroom; lean/core/min **14,970 / 12,348 / 6,368 B** flash and **1,418 / 1,159 / 1,115 B** static. Mini full repeats **27,938 / 1,619 B**. Host **253/154/124/80**; tooling **135**; `make gen-check` deterministic; all **12** default full Ardens suites P. `test_memory_music` **`stack_reserve=638`**, `test_save_chain` **666**. `test_music` remains **gap=65 ticks**, **svc_us=84**, **isr_ticks=27**, underruns/errors **0** with a forced fractional-camera render; `test_render` case 6 (fractional origin) passes its 15 ms scene+HUD+display budget. No manual GUI playthrough claimed. |

| 2026-10-28 | 27,720 B | 1,617 B | Sonar retirement (`sub-hauler-7px.28`): removed the free-flight A-button ping, cooldown/ring state, chart reveal, shop SONAR row/price/level, and the `SH_FEATURE_SONAR` gate; fog-chart passive reveal and the Sonar Lullaby music cue are unchanged. Save schema **5** removes the sonar level byte: scalar **35 B**, no-chart payload **35 B**, chart payload **291 B**; schema 4 slots are incompatible, with no migration. Paired removal delta from the prior current tree: **−508 B flash / −2 B static** with music (**27,720 / 1,617 B**) and **−488 B / −2 B** music-off (**25,520 / 1,458 B**). `test_memory_music` **`stack_reserve=636`**, `test_save_chain` **664**; Mini full repeats **27,720 / 1,617 B**. Host **248**; tooling **137**; all default Ardens suites P plus `test_flight` P. FX declaration offsets unchanged (`txt_sonar` remains unused); `make gen-check` deterministic. |
| 2026-10-29 | 29,604 B | 1,660 B | Five restoration story flourishes (`sub-hauler-e2z`): four one-shot dock cards for FIRST/PUMP/LIFT/BEACON plus the revised TIDEWALL thanks card for HOME; artwork by Sol (`sub-hauler-e2z.2`), 128×64 opaque native cards generated by `tools/gen_panels.py`. `.text/.data/.bss` **29,592 / 12 / 1,648 B**; 92 B hardware flash free, 40 B static headroom, 900 B static free. Host **246**, tooling **119**, **14/14** default device suites P; save-chain **690**, combined music **652**; flight 842 frames **11,864 µs max**, 0 misses, gap 1, 0 underruns/errors; runtime-page P; gen-check deterministic including native/4× previews. FX payload/image **2,081,656 / 2,085,888 B** (+4,104/+4,096 vs baseline); the four new card records append after startup, and `panel_ending_thanks=0x1CAFC5`, `startup=0x1FAB6E` are preserved. Save schema 5/layout and all existing clue/ending/input/music ordering unchanged. No physical playthrough or speaker check. See [story-flourishes.md](archive/story-flourishes.md). |
| 2026-10-29 | 29,436 B | 1,659 B | Fixed build after switch removal and retained freight-offer consolidation: `.text/.data/.bss` **29,424 / 12 / 1,647**; 260 B hardware flash free, 41 B static headroom, 901 B static free. Host **243**, tooling **115**, 14 default device suites P; save-chain **690**, combined music **652**; flight 842 frames **11.864 ms max**, 0 misses, gap 1, 0 underruns/errors; gen-check deterministic, FX payload/image unchanged at **2,077,552 / 2,081,792 B**; no physical playthrough/speaker check. |
| 2026-10-28 | 29,696 B | 1,662 B | ATMlib2FX synth is the production music backend (`SH_MUSIC_STREAMING=0`, `ATM_FX_COMPACT_LOOP=1`, `ATM_FX_COMPACT_RING=32`). Paired full music-off/on **25,000 / 1,458 → 29,696 / 1,662 B** (`.text/.data/.bss` **29,684 / 12 / 1,650**); 0 B hardware flash free, 38 B static headroom, 898 B static free. Mini full repeats **29,696 / 1,662 B**. `MUSIC_BACKEND=stream` keeps the comparison at **27,198 / 1,618 B**. `test_memory_music` **`stack_reserve=616`**, `test_save_chain` **691**; `test_music` **gap=1 tick**, **svc_us=104**, underruns/errors **0**; `make fxtest-flight` **12.376 ms max / 8.355 ms mean** over **842** frames, **0** deadline misses and **0** audio underruns/errors. The flight probe is build-only with voyage save disabled so its USB test image fits; the flight update/render/music path is the shipping code. Host **251**, tooling **142**, twelve default Ardens suites plus `test_flight` P; `make gen-check` deterministic. FX payload/image unchanged at **2,075,502 / 2,079,744 B**. No physical speaker/listening, playthrough or Mini hardware check. |
| 2026-10-28 | 29,598 B | 1,662 B | Startup splashes (`sub-hauler-7px.29`): recover 180 B (cue tables4, shared title reader48, equivalent fractional projection128), spend82 B on two skippable FX screens; 98 B hardware flash free. Full/Mini29598/1662; music-off24922/1458, stream27104/1618; lean/core/min14874/12742/6778 flash. Original Snailware logo and license preserved; prior2075502B FX prefix identical, payload/image2077552/2081792. Host251/149/131/87, tools144, gen-check and14device suites P;52startup/title pixel comparisons, introduction1977ms. Stack614music/689save. Flight842frames max12368us mean8346us, zero16ms misses and zero audio errors/underruns. Details/preview: startup-splashes.md. No GUI/physical playthrough or skip-button injection claimed. |
| 2026-10-28 | 29,616 B | 1,662 B | Splash hold: the Snailware logo keeps 60 × 16 ms, while the title frame now holds 150 × 16 ms; the real introduction measures **3,417 ms**. Full/Mini **29,616 / 1,662 B** (+18 B flash, no RAM), **80 B** hardware flash free; music-off **24,940 / 1,458 B**, stream **27,122 / 1,618 B**; lean/core/min unchanged at **14,874 / 12,742 / 6,778 B** flash and **1,417 / 1,159 / 1,115 B** static. `test_startup` still compares 52 frames and passes its 3,250–4,000 ms window; `test_startup` and the production-sketch `test_save_chain` use an 8 s suite capture. Host **251**, tooling **144**, `make gen-check` deterministic, all **14** default device suites P. FX payload/image unchanged at **2,077,552 / 2,081,792 B**. No GUI playthrough or skip-button injection claimed. |
| 2026-10-30 | 28,848 B | 1,660 B | Finalized seven-beat story decks (`sub-hauler-7vx`): 30 alternating cards (15 animated 128×64 scenes ×4 poses at 3.3 fps plus 15 baked text cards) packed as a 46,673 B RLE cart blob. FIRST has three pairs; the other six beats two. Survey decks play at the in-world dwell and return to the frozen pilot; HOME is the first part of the ending and resumes after reload until the ending flag commits. `.text/.data/.bss` **28,836 / 12 / 1,648**; 848 B hardware flash free, 40 B static headroom, 900 B static free. Host **234**, tooling **122**, **14/14** default device suites P; save-chain **696**, combined music **626**; flight 842 frames **11,768 µs max**, 0 misses, gap 1, 0 underruns/errors; runtime-page P; Mini **28,848 / 1,660 B**; gen-check deterministic. FX payload/image **2,082,151 / 2,086,400 B** (10,752 B under 2 MiB); `FX_SAVE_PAGE` remains **0xFFF0** and all earlier declaration offsets are preserved; `storyBeats=0x1F0F16`. No physical playthrough, GUI check or speaker listening claimed. See [story-beats.md](story-beats.md). |
| 2026-11-01 | 27,230 B | 1,646 B | Economy removal (`sub-hauler-9q0`): shop, currency, job payouts, repeatable freight offers, air-tank levels and the three credit coves are gone; campaign deliveries are intrinsic unpaid restoration work. Pickup records are now `x/y/kind` with a **5-byte stride** (4 records, object stream **91 B**); the former credit cove rooms remain as geography with no pickup. Fixed **300-unit** air tank with free berth refill and free rescue. Station board `WORK / UNDOCK`, `summary / ABANDON / WORK / UNDOCK`, or `UNDOCK` at a service berth; WORK lists only story and survey rows. The generated route reward/deadline API is removed. Save schema **7**: scalar **24 B**, no-chart payload **24 B**, chart payload **280 B**, fixed-build signature **0x61** (shop bit retired); schema 6 slots are incompatible and are never migrated. `sizeof(game::State)` **90 B**, `ActiveJob` **5 B**. `.text/.data/.bss` **27,218 / 12 / 1,634**; **2,466 B** hardware flash free, **54 B** static headroom, **914 B** static free. Host **215**, tooling **128**, **14/14** default device suites P; save-chain **702**, combined music **660**; flight 842 frames **11,600 µs max**, 0 misses, gap 1, 0 underruns/errors, `fail_mask=0`; runtime-page P; Mini **27,230 / 1,646 B**; gen-check deterministic. FX payload/image **2,082,129 / 2,086,400 B** (10,752 B under 2 MiB); `FX_SAVE_PAGE` remains **0xFFF0** and all earlier declaration offsets are preserved. The flight probe's discovery-notice case no longer looks for a removed credit pickup kind. No physical playthrough, GUI check or speaker listening claimed. |
| 2026-11-02 | 27,230 B | 1,646 B | Story motion/clarity pass (`sub-hauler-e9s`): fifteen native pixelgen loops with clearer sub hulls and cable break, moving carriage/reel/crane, sweeping beacon, rising circulation/leak bubbles; 4 poses at 300 ms. All fifteen render/check/replay/check bundles pass. New production `test_story` verifies all 75 decoded frame CRCs, cadence/input/returns/save-failure continuation/HOME; render/display/service max **10,748 µs** (CRC work excluded). Host **217**, tooling **130**, browser **7**, **15/15** default device suites P; save-chain **702**, combined music **660**; runtime-page P; flight **842** frames, **11,600 µs max**, zero misses/audio errors. FX/Mini unchanged **27,230 / 1,646 B**. Story blob **45,439 B** (-1,234 B); FX payload/image **2,080,895 / 2,085,120 B** (-1,280 B padded), **12,032 B** cart headroom; save page **0xFFF0**. Gen-check deterministic. Human artistic approval/physical playthrough unrecorded. |
| 2026-11-07 | 28,316 B | 1,649 B | DEEP RELAY departure art and acceptance beat (`sub-hauler-4jh`): a distinct 128×64 1-bit wreck/relay image replaces the now-unused shared dispatch row; HOME departure uses existing cargo-loading art. New expedition scenes follow BEACON and precede HOME. Story has 16 scenes, 41 cards, 89 frames, and a 57,377 B packed story blob. `.text/.data/.bss` **28,304 / 12 / 1,637 B**; 1,380 B hardware flash free, 684 B under 29,000 planning, 51 B static headroom, 911 B static free. `make gen`, FX build, size, and Mini pass. Cart payload/image **2,092,857 / 2,097,152 B**, leaving **183 B** before the save section; save page `0xFFF0` unchanged. Pixelgen conversion/check/replay pass (`hard_valid=true`, replay matched); the source image used built-in image generation because Pixelgen MFLUX could not access Metal. Human visual review and host/tool/device suites remain unclaimed. |
| 2026-11-05 | 28,218 B | 1,650 B | Playable hull and cargo-slot UI reduction (`sub-hauler-9he` + `sub-hauler-ded`): only HAULER remains playable; title LEFT/RIGHT hull cycling, B reset and LEN stat are retired. Legacy schema-8 hull bytes 0/2 load and normalize to HAULER while retaining voyage progress; saves always write HAULER at the frozen offset-1 byte. HUD CGO boxes/label and dead active-WORK CARGO fallback are removed; accepted destination, pickup/delivery, loaded-v-empty handling, ballast notch and one-active-job state remain. `.text/.data/.bss` **28,206 / 12 / 1,638 B**; **1,478 B** hardware flash free, 782 B under 29,000 planning, **50 B** static headroom, 910 B static free. Host **235**, tooling **147**, browser **8**, **15/15** default device suites P; save-chain **665**, combined music **642**; `test_story` **90** frame CRCs; runtime-page P; flight **842** frames, **11,096 µs max**, 0 misses, gap 1, 0 underruns/errors, `fail_mask=0`; Mini **28,218 / 1,650 B**; gen-check deterministic. Cart unchanged **2,092,772 / 2,096,896 B** (256 B under 2 MiB); FX declaration order/offset ABI and `FX_SAVE_PAGE=0xFFF0` unchanged. Human visual/physical acceptance remains explicit and unclaimed. |
| 2026-11-03 | 28,432 B | 1,650 B | Integrated handoff art (`sub-hauler-sxc`, verified through `sub-hauler-3ca`): Sol's four `story-handoffs-v1` bundles replace the FIRST/HOME/BEACON departure placeholders and both joint variants. Source prose is unchanged; `beacon-descent` and `home-return` leave the active blob, while `delivery-loading` and `repair-bay` remain for PUMP pickup and FIRST result. Actual net art change **+8,520 B RLE blob** (10,306 B new − 1,810 B removed + 24 B offsets), not the 1,485 B estimate that assumed removing `home-return` too. Story blob **57,316 B**, now **90 frames** (17 unique images). `.text/.data/.bss` **28,420 / 12 / 1,638 B**; **1,264 B** hardware flash free, 568 B under 29,000 planning, **50 B** static headroom, 910 B static free. Host **235**, tooling **144**, browser **8**, **15/15** default device suites P; save-chain **665**, combined music **659**; `test_story` **90** frame CRCs, handoffs/cold reload P; runtime-page P; flight **842** frames, **11,604 µs max**, 0 misses, gap 1, 0 underruns/errors, `fail_mask=0`; Mini **28,432 / 1,650 B**; gen-check deterministic. FX payload/image **2,092,772 / 2,096,896 B**, only **256 B** under 2 MiB; every earlier offset and `FX_SAVE_PAGE = 0xFFF0` unchanged, `storyBeats = 0x1F0F00`. Human artistic review remains **unreviewed**. No physical playthrough, GUI check or speaker listening claimed. |
| 2026-11-03 | 28,432 B | 1,650 B | Editable story source and automatic text pagination (`sub-hauler-640`): `story/scenes.json` is the single UTF-8 authoring source for scene order, prose and image references; the packer uppercases, wraps at 20 columns and emits consecutive six-row text cards with no truncation. Scene/card kinds are generated, not image/text alternation. Fourteen scenes produce **40 cards** (18 image, 22 text) and **78 frames** from **14** deduped images; story blob **48,796 B**. FIRST names the remote Cormorant Freight Yard lift; survey findings name replacement collection stations directly; handoff prose keeps destinations, seal location and dispatch motivation. Shared preview exposes all scenes/pages. `.text/.data/.bss` **28,420 / 12 / 1,638 B**; **1,264 B** hardware flash free, 568 B under 29,000 planning, **50 B** static headroom, 910 B static free. Host **235**, tooling **144**, browser **8**, **15/15** default device suites P; save-chain **665**, combined music **659**; `test_story` **78** frame CRCs, handoffs/cold reload P, max **10,756 µs**; runtime-page P; flight **842** frames, **11,604 µs max**, 0 misses, gap 1, 0 underruns/errors, `fail_mask=0`; Mini **28,432 / 1,650 B**; gen-check deterministic. FX payload/image **2,084,252 / 2,088,448 B** (**8,704 B** under 2 MiB); every earlier offset and `FX_SAVE_PAGE = 0xFFF0` unchanged, `storyBeats = 0x1F0F00`. No physical playthrough, GUI check or speaker listening claimed. |
| 2026-11-03 | 28,330 B | 1,650 B | Story geography and progression handoffs (`sub-hauler-3ca`): PUMP copy and guidance now name Benthos as the delivery berth; LIFT names Cormorant and the remote Freight Yard repair; next-stop guidance checks the unfinished survey/repair before the seal and never says BEACON ready early; empty WORK boards keep the clue; five cargo departures and one joint-restoration handoff reuse existing loading art. Beat delivery decks now carry result cards only. Scene/card/frame map covers **36 cards / 78 frames** from **15** shared scenes; story blob **49,168 B**. `pendingStory` now validates saved scene ids 0–14 with the same 25 B scalar schema 8. `.text/.data/.bss` **28,318 / 12 / 1,638 B**; **1,366 B** hardware flash free, 670 B under 29,000 planning, **50 B** static headroom, 910 B static free. Host **235**, tooling **135**, browser **7**, **15/15** default device suites P; save-chain **665**, combined music **660**; `test_story` **78** frame CRCs, handoffs/cold reload P, max **10,748 µs**; runtime-page P; flight **842** frames, **11,604 µs max**, 0 misses, gap 1, 0 underruns/errors, `fail_mask=0`; Mini **28,330 / 1,650 B**; gen-check deterministic. FX payload/image **2,084,624 / 2,088,960 B** (**8,192 B** under 2 MiB); every earlier offset and `FX_SAVE_PAGE = 0xFFF0` unchanged, `storyBeats = 0x1F0F00`. No physical playthrough, GUI check or speaker listening claimed. |

Chart performance checkpoint (2026-10-26, `sub-hauler-h89`): paused charts
compose on entry and at the existing 16-frame sub/destination blink boundaries
at 3.75/s while open. Other frames skip composition and
OLED transfers. The same 256 chart bytes now use eight 32-byte FX bursts and a
32-byte stack scratch page, expanding straight into all 1,024 framebuffer bytes.
Normal full-chart render time falls **10.327→4.966 ms**, with repeatable CRCs;
OLED presentation stays about **1.16 ms**. Normal/dev modes pass all six device
suites, including the new permanent chart benchmark with a 7 ms combined
render/display budget. Stack reserve is **756 B** against 600 B. Full/lean/core/min
flash is **20,176 / 17,368 / 14,756 / 8,750 B**, static **1,583 / 1,571 / 1,313 /
1,252 B**; dev **15,592 / 1,582 B**, Mini full **20,176 / 1,583 B**. A pre-change
source snapshot rebuilt with this toolchain measures **20,132 / 1,583 B** full
and **8,750 / 1,252 B** min, so this pass adds **44 B flash / 0 B static** full
and leaves min unchanged. Earlier dated sizes remain historical measurements.
Host full/lean/core/min **150/117/89/44**, dev **155**, chart-save **154**; tooling
**105** and deterministic generation pass. Map/FX bytes are unchanged. See
[render performance](render-performance.md) for timing scope and reproduction.

Wall settlement checkpoint (2026-10-26, `sub-hauler-cxg`): all four physical
berths move from navigation-room centres to wall ledges. Separate dock metadata
drives docking, charts and centred home spawn; authored-route deadlines include
the approach lengths. Hull clearance, a 48-pixel escape column, floor attachment,
hazard separation and berth reachability across all eight tool subsets are
validated. Four inhabited rooms per settlement, piers and airlocks bake into
the existing native image; no extra runtime draw calls or state. Collision,
chart and object blobs and the first 41,572 payload bytes stay unchanged.
Payload/image remain **451,172 / 451,328 B**, including the **409,600 B** terrain.
Full/lean/core/min flash stays **20,098 / 17,292 / 14,722 / 8,696 B** and static
**1,583 / 1,571 / 1,313 / 1,252 B**; dev **15,540 / 1,582 B**. Stack reserve
**775 B**, target **600 B**. Native full/lean/core/min **147/114/89/44**, dev
**152**, chart-save **151**; tooling **105**, all five device suites P in
normal/dev modes. Generation deterministic; native dock/undock checks use the
actual packed collision plane and all four towns were visually inspected.

Works texture checkpoint (2026-10-25, `sub-hauler-1oj`): repeating closed
plate rectangles and centre bars replaced with staggered broken seams,
corrosion chips, bent ribs and sparse rivet runs aligned with the nearby wall.
Only offline scenery generation changed. Native masks retain protected routes,
interactive markers and cave silhouettes; all 18 landmarks/130 smaller motifs
remain. Generated FX header, preceding 41,572 B payload prefix and total
payload/image sizes are unchanged. Full firmware remains **20,098 B flash /
1,583 B static RAM**. Generation deterministic; **146** host cases, **103**
tooling cases and all **5** Ardens suites pass. Native before/after views were
visually inspected. No runtime texture code or extra transfers were added.

Native contour checkpoint (2026-10-25, `sub-hauler-8se`): streamed terrain now
reconstructs a continuous bicubic field at 2560x1280 and thresholds it to 1-bit,
removing square tile steps without device-side smoothing. All collision-tile
centres retain their class; permanent geometry checks bound visual movement
to three pixels in solid and water, in both directions. Scenery is re-anchored
to curved floors and clipped inside the new rock silhouette. Eighteen larger
landmarks remain, with 130 smaller motifs. Collision/chart/object tables and
the first 41,572 payload bytes stay unchanged; no layout or storage growth.
Native terrain remains **409,600 B**, payload/image **451,172 / 451,328 B**.
The runtime stream path and eight 128-byte reads are unchanged.
Generation is deterministic; host full/dev **146 / 151**, tooling **103**, all
five Ardens suites P in each mode. Full/dev firmware HEX files are byte-identical
to the preceding scenery build: **20,098 / 1,583 B** and **15,540 / 1,582 B**
flash/static respectively. The device memory suite retains its P at the 600 B
stack target. Twelve-case render/presentation timing differences are at most
**4 µs normal / 5 µs dev** against that build. Native previews were visually
inspected; no interactive playthrough or physical-device soak is claimed.

Regional scenery checkpoint (2026-10-25, `sub-hauler-9bc`): four distinct
native scenery styles add 18 larger landmarks and 113 smaller floor motifs.
The art generator masks navigation corridors, station/spawn footprints,
gates and pickups; rock marks stay inside the collision silhouette. All
collision/chart/object bytes and the preceding 41,572 B payload prefix remain
unchanged. The existing native terrain section is still 409,600 B, payload/image
**451,172 / 451,328 B**; terrain streaming code and transfer lengths are unchanged.
Host full/dev suites pass **146 / 151**, tooling **99**; streamed-image crop
coverage remains 6,499 viewports. All five Ardens suites pass in normal and
dev modes. Full/Mini remain **20,098 B flash / 1,583 B static RAM**; dev
**15,540 / 1,582 B**. Mini/dev HEX files are byte-identical to their archived
pre-art builds, and the expanded stack probe remains **770 B**. Across twelve camera/cache
cases per mode, measured render/presentation changes are at most **3 µs normal /
4 µs dev**, within the timer resolution. Native world and eight enlarged region
crops were visually inspected; no interactive playthrough or physical soak is claimed.

Render optimization checkpoint (2026-10-25, `sub-hauler-w16`): terrain bakes
to a 409,600 B native image and streams into the existing screen buffer in
eight 128-byte FX bursts; objects/HUD remain overlays. The prior 41,572 B
payload remains an identical prefix. HUD/cave rectangles use clipped page
spans. The native reference renderer decodes packed cache rows once, with its
checked partial-cache fallback retained; this path drops out of device firmware.
Cache refresh reads each distinct packed cart byte once (30–40 transactions
per interior window, down from 180), retaining the 23 B bitmap and sealed
borders. Full/lean/core/min flash **20,098 / 17,292 / 14,722 / 8,696 B**, static
**1,583 / 1,571 / 1,313 / 1,252 B**. Flash savings **402 / 376 / 444 / 446 B**;
static deltas all zero. Full Mini **20,098 / 1,583 B**; dev **15,540 / 1,582 B**
(422 B flash saved). Stack reserve remains **770 B**, target **600 B**.
Native tests **146/113/88/44**, dev full **151**, chart-save full **150**,
tooling **97**, all five Ardens suites P in normal/dev modes; min render suite
also P. Generation deterministic, native panorama byte-identical, all 24
normal/dev cold/cached frame CRCs match the pre-change baseline. Six-scene
AVR timings show **34–58%** less dev render/presentation time; this is not a
measurement of the entire gameplay loop. See [render performance](render-performance.md)
for timings, measurement scope and reproduction commands. FX payload/image
are **451,172 / 451,328 B**. No physical-device timing measurement is claimed.

Organic terrain checkpoint (2026-10-25, `sub-hauler-ukp`): irregular, lobed
caverns and variable-width winding passages replace the uniform tubes. Thirty-one
solid features occupy 509 formerly-water tiles; 31 of 42 passages wind over
12% beyond their straight-line distance. All eight hull/tool reachability
subsets pass. Layout version 3 rejects preceding chart saves; the SHL2 object
format and FX declaration order stay unchanged, including the identical
34,540 B legacy prefix. Payload/image remain **41,572 / 41,728 B**.
Full/lean/core/min flash is **20,500 / 17,668 / 15,166 / 9,142 B**, static RAM
**1,583 / 1,571 / 1,313 / 1,252 B**; full Mini **20,500 / 1,583 B**.
The terrain/timing changes add **0 B flash / 0 B static**. Freight allowances
are baked from navigable route lengths, excluding optional cutter/jet routes.
Native tests pass **141/108/83/39**, dev full **146**, optional chart-save
full **145**, tooling **95**. Generation is deterministic, all four Ardens
suites pass, and the updated diagnostic probe measures **770 B** stack reserve
against the unchanged **600 B** target. The real scene-renderer overview was
visually inspected; no interactive playthrough or physical-device soak is claimed.

Developer traversal checkpoint (2026-10-25, `sub-hauler-m35`): explicit
`DEV_MODE=1` builds in isolated dev directories and measures **15,962 B flash /
1,582 B static RAM**. Normal firmware remains **20,500 / 1,583 B** and its HEX
is byte-identical to the preceding authored-map build. A 19 B FX banner appends
after the map labels, making payload/image **41,572 / 41,728 B**. Generation is
deterministic; native full suites pass **140 normal / 145 dev** and tooling
passes **91**. All four device suites pass in each mode. Free-flight uses direct
bounded D-pad movement, A boost, complete chart, full air/hull, no docking,
hazard simulation or reward claiming; the application suppresses chart EEPROM
reads/writes in dev mode. No interactive playthrough is claimed.


Music asset checkpoint (2026-10-24, `sub-hauler-5ai`): the full game remains
**17,548 B flash / 1,572 B static RAM**. Three original slow loops add
**9,822 B** of external music streams; the cart payload is **22,834 B** and
the padded flashable image is **23,040 B**. The separate ATMlib2FX jukebox
viewer builds at **13,084 B / 1,512 B**. `make gen-check`, 132 host cases,
80 tool tests and all three device suites pass. Each music stream ran through
three loop periods in the host device decoder with zero underruns/errors and
identical consecutive cycles; no listening or physical-speaker check was run.

Music expansion (2026-10-24, `sub-hauler-9g1`): three new cues at
40/75/100 BPM append **11,706 B**, preserving all **22,834 B** of the
previous payload. The six-song payload/image are **34,540 / 34,560 B**.
Full game size stays **17,548 B flash / 1,572 B static RAM**; the six-song
viewer measures **13,208 B / 1,512 B**. New native effects cover arpeggio,
note cut, vibrato/tremolo/duty LFO, volume slide, glissando and noise retrigger.
Three-period host decoder traces (10,800 / 5,760 / 11,520 ticks) have zero
underruns/errors and identical consecutive cycles; the 100 Hz cue uses
two-tick service spacing. Generation is deterministic; 132 host cases and
81 tool tests and all three Ardens device suites pass. Physical speaker sound
was not measured.

Historical cargo-free checkpoint: static free SRAM was **988 B**. `chart::State` is
256 B, the retired sonar fields accounted for 2 B, the mine mask adds 1 B, the damage cooldown
adds 1 B, the four-particle pool adds 16 B, the run-only shop levels add 3 B,
the M8 hull class adds 1 B, and the M9 last-delivery byte adds 1 B. The M9
chart save adds **0 B**: its 260 B block is EEPROM and its adapter is a
stateless stack object. The cargo-free carve removed the static
`cargo_screen::State` and its B-hold counter (**98 B**); the final resource pass
removed another **20 B** and compacted AVR `game::State` to **80 B**. The
whole-image static total is **1,572 B**. The chart, mine, current,
damage/particle, shop, title/roster, contract/undock, and reset paths are all
inside the fixed probe with the explored set owned statically; the probe
measures an **812 B** reserve and prints `P` under the **600 B** target
re-baselined on 2026-10-23. The firmware reserve is about **726 B** (988 B
static free minus an estimated 230 B stack depth); the test's stack locals
(`game::State`, tile cache) are static in the firmware, so they must not be
subtracted a second time. The planning ceiling is **1,700 B**: the full image
has 128 B of headroom.

The total movement from the post-M9 menu-fix checkpoint (28,930 B / 1,690 B) is
**-11,382 B flash / -118 B static**; the cargo-free carve alone was
**-8,734 B / -98 B**, and this pass added **-2,648 B / -20 B** without changing
the contract loop. The pass also measured an incremental compact-metadata A/B:
17,584/14,810/12,640/9,178 B -> 17,548/14,792/12,626/9,142 B, with the
position-only PROGMEM metadata at **88 B** (from 148 B) and no RAM or cart-read
change. The 128 B cave atlas stays in PROGMEM because routing hot tile renders
through FX would add cart traffic. UI text and the three pilot sheets live after
`cargoBank`; the default Arduboy font/Print symbols are absent from the ELF.
The `cargoBank` declaration stays at **0x001AC2** with unchanged size, bytes,
and order. The six underwater music streams occupy **21,528 B**;
the FX payload in `fxdata/fxdata-data.bin` is now **34,540 B**, while
the padded flashable `fxdata/fxdata.bin` image is **34,560 B**. The normal
game has no music playback calls yet. The separate library jukebox viewer
build measures **13,208 B flash / 1,512 B static RAM** with a 64 B ring.
The optional
chart save variant
was last measured at 29,330 B / 1,690 B before the carve; a fresh
`SAVE_CHART=1` measurement is required before changing the default.
The retired `MULTI_HULL` option added class art/string calls; the fixed build
now compiles only the HAULER sprite call, and the earlier collapsed core/min
measurements were 12,626 B / 1,302 B and 9,142 B / 1,252 B. Any future
persistent state still needs a carve rather than another byte-class allocation. The probe does not print the
exact high-water byte unless `SH_STACK_DIAG=1`; static free bytes are still not
a stack measurement.

Progression resource/storage audit checkpoint (2026-10-26,
`sub-hauler-7px.1`): no production firmware or generated artifact changed.
Whole-image baseline re-measured with `make size-matrix`: full/lean/core/min
**20,176 / 17,368 / 14,756 / 8,750 B** flash and **1,583 / 1,571 / 1,313 /
1,252 B** static; `make mini PROFILE=full` repeats **20,176 / 1,583 B**;
`make fxtest` builds six suites and each prints a bare `P`. The supported stack
diagnostic is `make fxtest FXTEST_ONLY=test_memory
RELAX_FLAGS="-mrelax -mcall-prologues -DSH_STACK_DIAG=1"`, which prints
**`stack_reserve=756`** (600 B target). Host tests pass **150/117/89/44**,
tooling **105**, and `make gen-check` reports deterministic artifacts.

The installed Arduboy2 reserves EEPROM 0–15: address 1 system flags, 2 audio
mute, 3–7 reserved, 8–9 unit ID, 10–15 unit name; `EEPROM_STORAGE_SPACE_START`
is 16 and `E2END` is `0x3FF` (1,024 B). Arduboy2 writes only that prefix,
ArduboyFX and ATMlib2FX contain no EEPROM code, and the optional standalone
chart save writes 0–259 (including the system prefix). The voyage checkpoint
does not use internal EEPROM at all; its single 512 B slot lives in the generated
4 KiB cart save partition (`FX_SAVE_PAGE`), and every save erases that block.
Details are frozen in
[progression-implementation.md](progression-implementation.md).

A bounded build-only AVR probe,
`tst/fxdatatest/probe_music/probe_music.ino`, measured the ATMlib2FX compact
decoder with the intended `RING=64`, `LOOP=1` flags: **5,858 B / 1,197 B**
without music versus **9,518 B / 1,425 B** with it, a marginal
**+3,660 B flash / +228 B static**. `avr-nm` confirms the 228 B static split
(`atm_fx_compact_state` 103, `channels` 84, `osc_params_array` 16,
`osc_pha_acc_array` 8, `osc_cb` 12, `atmlib_state` 3, two ISR bytes). The
64-byte ring is only 64 of those 228 B. Music alone therefore exceeds the
**117 B** static headroom by **111 B**; adding the projected 17 B campaign
state gives roughly a 128–132 B projected deficit. Flash projection
20,176 + 3,660 = **23,836 B** leaves 5,164 B under the 29,000 B planning
ceiling, but campaign/save flash and the combined stack are unmeasured. Full
music must stay default-off until `.7` measures a combined fit or carves the
state; the 1,700 B static ceiling and 600 B stack target are unchanged. No
manual playthrough, physical listening or soak test was performed.

Sampled-audio admission checkpoint (2026-10-27, `sub-hauler-7px.11.2`): six
raw 4-bit IMA ADPCM loops add **1,424,640 B** of cart payload; the payload/image
become **1,876,899 / 1,876,992 B** under the 2 MiB cap. The player static state
is exactly **152 B** (128 B ring plus 24 B state), and the paired full-game
noUSB images measure **17,412 B / 1,443 B** without the player and
**19,006 B / 1,595 B** with it: **+1,594 B flash / +152 B static** against
**1,600 / 152** allocations. Diagnostics add **544 B flash / 14 B static**
when enabled. Timer3 runs the 8 kHz sample ISR; Timer4 runs a 187.5 kHz PWM
carrier. A bounded `world::FxTables::tileByte()` service point (gated by
`SH_AUDIO_PLAYER`) keeps the cold collision path within the deadline. The
device suite reports a **114-tick (14.25 ms)** maximum active refill gap with
no warm frame, **0** underruns, **168 µs** maximum foreground service and
**27 Timer3 ticks (13.5 µs)** maximum ISR. A build-only noUSB actual-game probe
with player, foreground work and ISR reports **815 B** stack reserve against
the 600 B target. The shipping full image remains **20,074 B / 1,583 B**;
final combined campaign/presentation fit is not claimed. See
[audio-admission.md](audio-admission.md).

USB-free release entry checkpoint (2026-10-27, `sub-hauler-7px.12`): the
default shipping build now compiles with `SH_RELEASE_NO_USB=1` and places
`ARDUBOY_NO_USB` immediately before `setup()`. The release has no in-game
loader row; the library's startup **DOWN** check is the bootloader entry.
Full/lean/core/min USB-free images measure
**17,620 / 14,902 / 12,088 / 6,094 B** flash and **1,444 / 1,432 / 1,173 /
1,114 B** static; Mini full repeats **17,620 B / 1,444 B**. The runtime-USB
full development image is **20,282 B / 1,584 B**, so the no-USB entry saves
**2,662 B flash / 140 B static** on this image. The removed title loader added
**208 B flash / 1 B static** against the .11.2 noUSB full control
(17,412/1,443), inside the **400 B/16 B** loader+cue/settings allocation. A
build-only noUSB actual-game probe with the sampled player, foreground work,
cold/cached world paths and title paths measures **820 B** stack reserve
(600 B target, 220 B margin; `build/stack-7px12b/run.log`). The current
campaign+save USB-free image compiles and links at **27,786 B / 1,525 B**; that
is still **1,286 B flash / 45 B static** over the **26,500 / 1,480** pre-audio
target. The same image with runtime USB is **30,474 B / 1,663 B** and does not
fit (**778 B** over the 29,696 B hardware limit). The final combined
story/presentation/audio fit remains task `.13`. No physical loader, uploader
or Mini hardware check is claimed; the automatic-reset limitation is documented
in `README.md`.

Fitted story presentation checkpoint (2026-10-27, `sub-hauler-7px.13`): the
USB-free campaign+save full profile now links at **26,472 B flash / 1,469 B
static** (`.text/.data/.bss` **26,460 / 12 / 1,457 B**), inside the
**26,500 / 1,480** pre-audio allocation with **28 B / 11 B** headroom. Against
the .12 baseline (27,786/1,525) the pass recovers **1,314 B flash / 56 B
static**. The Mini full image repeats **26,472 / 1,469 B**. Baked FX
presentation replaces repeated WORK/ending/title/prompt drawing: 17 generated
panel sheets (WORK detail, carrying strip and row frames; title menu/confirm;
save-failure, pause, rescue and confirmation prompts; ending network/thanks/
stats cards) plus four restoration sprites (survey ring, inactive mooring, pump
flow, lift cable). One shared 128x64 cart-burst renderer overlays only saved
numbers; idle screens still skip composition and the OLED transaction. The
three RAM-resident `CSWTCH` label tables (12/18/21 B) are now branch code, and
mutually exclusive WORK/pause/save row and WORK-mission/ending-card
transients share two bytes. `SH_CART_SERVICE()` keeps the sampled-player
foreground refill hook in front of panel and cold world reads. FX payload/image
are **1,951,241 / 1,951,488 B**, below the 2 MiB cap; every earlier declaration
offset is unchanged and the payload grows 10,556 B for the new art.

Default (campaign/save-off) images measure full/lean/core/min **17,668 /
14,948 / 12,138 / 6,094 B** flash and **1,436 / 1,424 / 1,165 / 1,114 B**
static; the default full image is **+48 B flash / -8 B static** against .12
because the six-label settlement name switch became an if-chain. Runtime-USB
full is **20,330 B / 1,576 B**; runtime-USB campaign+save is **29,146 B /
1,609 B** (within hardware, 146 B over the planning line). Host tests pass
**169** (default) and **220** (campaign+save); tooling passes **128**; all seven
Ardens suites print `P` in both default and campaign+save builds. `make
gen-check` is deterministic. The campaign+save `test_memory` probe reports
**`stack_reserve=725`**; a build-only noUSB actual-game probe with the real
sampled player, Timer3 ISR, cold/cached world render, restoration overlays and
all new WORK/ending/title/prompt panels reports **`reserve=779`** (600 B target,
179 B margin; `build/stack-7px13/run3.log`). Allocation arithmetic for the
admitted player (**+1,594 / +152**) gives **28,066 / 1,621 B**, under the
**29,000 / 1,700** combined ceiling by **934 / 79 B**; music is not integrated,
so this is not a final combined-image measurement. No physical playthrough,
listening or Mini hardware check is claimed.

Streaming music integration checkpoint (2026-10-27, `sub-hauler-7px.7`): the
game now links the sibling **ArduboyStreamFX** library (read-only checkout;
`ARDUBOYSTREAMFX_ROOT` in the Makefile) instead of the local admission player.
The combined USB-free full profile with campaign, atomic save and music
enabled is **28,984 B flash / 1,631 B static** (`.text/.data/.bss` **28,970 /
14 / 1,617 B**), fitting the **29,000 / 1,700** ceilings by **16 B / 69 B** and
hardware by **712 B**, but **using 484 B of the approved 500 B unallocated
flash reserve**. Mini full repeats **28,984 B / 1,631 B**. The default
`full` profile still keeps `CAMPAIGN=0`, `SAVE_VOYAGE=0`, `MUSIC=0` until `.6`
and `.13` review; the combined gate is measured with all three enabled. Default
full/lean/core/min remain **17,668 / 14,948 / 12,138 / 6,094 B** flash and
**1,436 / 1,424 / 1,165 / 1,114 B** static.

The final library increment is **1,596 B flash / 152 B static** against the
**1,600 / 152** player allocation, i.e. **4 B under** (library worker final).
The full delta from pre-audio .13 is **2,512 B flash / 162 B static**;
subtracting the library leaves **916 B / 10 B** for cue/settings/UI/service code
against the **400 / 16 B** allocation, i.e. **516 B over**. This is a real component/reserve
deficit: the full image fits 29,000/1,700 but does **not** restore the approved
500 B flash reserve or the controls allocation. Default profiles therefore
remained story/save/music off at that checkpoint; later `.24` work and the
current full default supersede those numbers.

All six cues use the existing authored C1 streams and FX offsets; the normal
game decodes them with the ATMlib2FX compact synth, while
`MUSIC_BACKEND=stream` keeps the ADPCM comparison build. `src/music_logic.hpp`
owns deterministic region selection from the generated 32-room table; the
device reads the 51,200 B `musicRegions` FX plane at most every 30 flight
frames, and a region must persist 120 flight ticks (2 s) before the cue changes.
Berths, restored Deep Anchor/service berths, ending, title, chart/pause, rescue
and save-failure transitions follow `progression.md`; same-cue starts do not
restart. The chart keeps the current flight cue playing (only the world pauses);
chart water bursts call the foreground service hook between cart reads.
`SH_CART_SERVICE()` calls `atm_fx_compact_service()` before cold world and
large panel reads, after presentation and on idle frames, never inside the OLED
transaction or ISR. `text::print` and panel bursts service the ring between
reads so menu composition stays within the deadline.

The synth compiles with `ATM_FX_COMPACT_RING=32` (the largest authored record is
28 B), `ATM_FX_COMPACT_LOOP=1`, and all native effects enabled. The library's
Timer3 consumer never reads FX; `src/music_backend.cpp` supplies the
foreground-only `atm_fx_compact_fetch()` through `FX::readDataBytes()`.

`test_music` reports **gap=1 tick**, **svc_us=100**, **underruns=0**,
**errors=0** while cycling all six cues, world/cart/OLED, WORK/ending/title/
pause/save panels and idle refills. The exact production
`updateStation()`→`requestSave()`→`saveVoyageNow()` suite reports
**`stack_reserve=702`** with `save_chain=1`, `save_underruns=0` and
`save_errors=0`; the combined `test_memory_music` probe reports **660 B**
against 600 B after adding the fractional-camera render path and all 75
story-deck frames. The device suites
set `SERIAL_BUFFER_SIZE=8` because their USB marker buffer is test-only and not
part of the production no-USB image. `make fxtest-flight` measures **11.600 ms
maximum** over 842 production flight frames with active synth music, zero 16 ms
misses and zero audio underruns/errors; its build-only image disables voyage
save to fit the USB test harness while keeping the shipping flight
update/render/music path. Host tests pass **215**; tooling passes **128**;
`make gen-check` is deterministic. FX payload/image are **2,082,129 /
2,086,400 B** under 2 MiB, including the 4,096 B single-slot save partition;
earlier declaration offsets are unchanged. MUSIC ON/OFF is a one-byte setting
at EEPROM **656**, above the retired voyage allocation and outside the Arduboy2
prefix (0..15); voyage checkpoints are cart pages and never share those bytes.
Writes occur only on the pause-row toggle, and device audio mute at address 2 is
honored without writing it. No physical speaker/listening, playthrough or Mini
hardware check is claimed.

Earlier streaming backend and 256 B ring-probe figures remain historical
checkpoints; the production image now uses the 32 B synth ring.

First-delivery air and map-music checkpoint (2026-10-28, `sub-hauler-7px.20`
and `.21`): the base tank is **300 units** with passive drain **0.4 units/s**
(one unit per 150 ticks), so a full tank lasts **750 s** before deliberate
blowing; the AIR TANK upgrade then added +30 per level and was removed on
2026-11-01. A permanent host route
test loads the real generated collision plane, starts at the Tidewall dock
anchor, accepts FIRST with four cargo units, flies with whole-pixel hull
collision, currents, mines and auto-docking, and docks at Benthos in **9,585
ticks (160 s)** with **72/300 air (24%)** and an intact hull; it enforces a
60-unit (20%) reserve. The same route with the old 180-unit/0.6 unit/s tank
arrived at zero air. The chart no longer pauses the soundtrack: map frames keep
the current flight cue, the loop services the stream on idle and render frames,
and `FxTables::chartBytes()` calls `SH_CART_SERVICE()` between its eight cart
bursts. `test_music` reports **gap=72 ticks**, map stage gap **72**, **0**
underruns/errors and **map=1**; `test_memory_music` reports
**`stack_reserve=891`**. Combined USB-free CAMPAIGN=1 SAVE_VOYAGE=1 MUSIC=1
full image is **28,986 B flash / 1,631 B static** (`.text/.data/.bss`
**28,972 / 14 / 1,617 B**), inside **29,000 / 1,700** by **14 B / 69 B** and
hardware by 710 B. Host tests pass **177** default and **230** with
campaign/save/music; `make gen-check` is deterministic and all device suites
print `P`. No commit, physical playthrough, speaker listening or Mini hardware
check is claimed.

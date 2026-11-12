# How sub-hauler is built

This is the engineering companion to [`history.md`](history.md). It describes
the current production pipeline at the last committed checkpoint (2026-11-11):
how content is authored, how the firmware is generated and tested, and how the
resource budget is enforced. Current contracts remain authoritative in
[`platform.md`](platform.md) and [`dev-flow.md`](dev-flow.md).

## 1. One fixed build, two board outputs

Sub-hauler has one shipping game configuration. There are no feature profiles,
runtime feature selectors, backend selectors or in-game loader rows. The same
firmware is built for the Arduboy FX and the Arduboy Mini; only the board target
changes.

| Surface | Location | Responsibility |
| --- | --- | --- |
| Release entry | `sub-hauler.ino` | `ARDUBOY_NO_USB`, `setup()`, `loop()` |
| Shared application | `src/app.hpp` | Boot, screen flow, station/flight/story transitions |
| Pure rules | `src/game.hpp`, `campaign.hpp`, `jobs.hpp`, `chart.hpp`, `station.hpp`, `pilot.hpp` | No Arduino headers; host-testable |
| Device boundary | `src/common.hpp`, `src/render.hpp`, `src/lib/Blit.*` | Arduboy2/FX/OLED/SPI/input |
| Views | `src/*_view.hpp`, `src/hud.hpp`, `src/cave_render.hpp` | 1-bit composition |
| Generated | `src/generated/`, `src/fxdata.h`, `fxdata/` | Never hand-edited |
| Assets and sources | `images/`, `story/`, `levels/`, `music/` | Authored inputs |
| Tests | `tst/`, `tst/fxdatatest/`, `tools/tests/` | Permanent, co-located |
| Generators | `tools/*.py`, `tools/gen.sh` | `make gen` pipeline |

The `DEBUG=1` build is a developer-only map-inspection image. It uses isolated
output directories and is not part of the shipped matrix. There is no release
variant without the campaign, save, music or splash features.

Core rules are integer-only. Device code uses no `float`, `double`, heap
allocation, `String` or dynamic containers. The simulation runs at a fixed
60 Hz; the renderer composes one 1-bit image per frame and presents it with one
`FX::display(CLEAR_BUFFER)`.

## 2. The generation pipeline

`make gen` is the only shipping generation entry. It runs the asset generators,
packs the ordered FX layout and emits the generated headers. It typically
writes:

- `fxdata/fxdata.bin` — the padded flashable cart image;
- `fxdata/fxdata-data.bin` — the unpadded payload;
- `fxdata/fxdata-save.bin` — the 4 KiB save partition;
- `fxdata/fxdata.h` and the mirrored `src/fxdata.h`;
- `src/generated/*.hpp` — symbolic constants, counts and table accessors;
- previews under `docs/assets/` (terrain, story cards, dock hints, and so on).

`make gen-check` snapshots the generated set, runs `gen` again, and fails on any
byte change. Generation must be deterministic; a half-generated set is never
committed. After a regeneration, stage all generated artifacts together.

### Ordered flash ABI

[`fxdata/fxdata.txt`](../fxdata/fxdata.txt) is an ordered declaration list.
Declaration order is flash ABI: append new entries, never reorder or remove a
shipped one. The file ends with a `savesection 4096` marker; the save partition
is placed at `FX_SAVE_PAGE = 0xFFF0`.

There is one documented exception to append-only order: `sub-hauler-jxk.1`
removed unreferenced legacy tables and retired hull art, and the resulting
offset shift is recorded in the commit and platform notes. Do not treat that as
a general licence to rewrite the layout. New content is appended before the
save section.

### Generated content

| Pipeline | Source | Generator | Runtime artifact |
| --- | --- | --- | --- |
| Level | `levels/exploration.json` | `tools/gen_level.py` | Collision/chart/object tables, native terrain panorama, `level.hpp`, SVG preview |
| Sprites/panels | `images/` PNGs | `tools/gen_sprites.py`, `gen_panels.py`, `gen_hud.py` | Plus-mask FX sheets |
| Story | `story/scenes.json`, `story/art.png` | `tools/gen_story_beats.py` | RLE story blob, scene/card headers, frame CRC, HTML/PNG previews |
| Dock hints | `story/dock_hints.json` | `tools/gen_text_images.py`, `gen_dock_barks.py` | Baked 128×8 text stamps and preview data |
| Text | String sources and font | `tools/gen_text_images.py`, `gen_font.py` | `fxdata/TextData.txt`, text-stamp sheets |
| Music | `music/*/song.json` | `tools/gen_music.py` using ATMlib2FX | Six synth streams, region map, manifest |
| Cargo (archival) | Generator constants | `tools/gen_cargo.py` | Historical cargo bank, no runtime reader |
| Audio evidence | Retired | removed | Sampled-audio reference pipeline retired and removed |

The Python tools have their own `unittest` suites under `tools/tests/`. Python
is never used as a harness to drive the C++ tests; the host tests are compiled
and run with the native C++ test framework in `tst/`.

## 3. Level and terrain

[`levels/exploration.json`](../levels/exploration.json) is canonical for the
authored map: rooms, passages, gates, coves, settlements and object records.
The generator validates connectivity and bounds, emits the packed collision and
chart planes, and bakes the static rock into a native 1-bit panorama.

The active terrain image is **2,560 × 1,280 pixels**, or **409,600 B** in the
FX payload. It is stored in the OLED page layout, so the renderer can read
visible strips directly into the framebuffer:

- A page-aligned Y origin reads eight 128-byte strips with `FX::readDataBytes`.
- A sub-page Y origin reads the next source page and combines rows in place
  using AVR hardware multiplication.
- The final page uses a 32-byte stack scratch for its upper row.
- There is no second framebuffer, no per-tile draw loop and no clearing pass.

Stations, gates, finds, currents, particles, the boat and the HUD are composed
afterward from the same rounded camera origin. Collision remains tile-based; the
visual contours and scenery are generated offline and do not change the
collision grid. `make render-world` exports an overview and exact native scene
pixels for inspection.

## 4. Rendering and presentation

The display path is native 1-bit through `Arduboy2Base`. `FRAME(x)` is simply
`x`; there is no greyscale or frame-blending path in the shipping build.

`src/lib/Blit.cpp` draws headerless plus-mask bytes into `sBuffer` and never
touches the OLED. Generated sprite blobs start with a two-byte
`(width, height)` header; `SpritesU::drawPlusMaskFX` owns that header, while
`Blit::draw` expects the caller to skip it.

The OLED and cart share SPI. Cart reads happen only during update/render, never
during the OLED transaction. The frame ends with one
`FX::display(CLEAR_BUFFER)`.

The follow camera keeps fractional position in Q4, projects the sub before
rounding, and bounds normal corrections to 2 px per tick. Terrain and integer
world objects share the rounded origin, so the whole scene scrolls together.
The camera uses a small velocity look-ahead; collision still walks one pixel at
a time so a faster thruster cannot tunnel through rock.

Runtime UI text is drawn from baked 128×8 plus-mask FX stamps
(`src/text.hpp`, `text::stamp`/`stampFrame`). The release image links
`Arduboy2Base`, so the inherited built-in `Print`/font path is not
instantiated; the external font renderer remains reference-test-only. Station
rows keep the 7 px pitch and 128×64 clipping. HUD notices are generated rows
rather than per-glyph cart reads.

When a function selects a 24-bit FX address or `txt_*`/`img_*` constant across
branches, assign it to one local and return it once at the end. Repeated
`return <24-bit>;` sites emit duplicate three-byte constant loads. Eight-bit
enum selectors are exempt.

## 5. Rules, views and tests

The pure/device split is the main testability decision:

- `src/game.hpp` and the other rule modules know nothing about Arduino, SPI,
  OLED or input sampling.
- Host tests compile the same rules and drive deterministic inputs.
- Views are composed into a plain 1,024-byte framebuffer and compared against
  expected pixels; the device boundary supplies the real display and cart.

The test pyramid is deliberately layered:

| Layer | Location | What it proves | What it does not prove |
| --- | --- | --- | --- |
| Host C++ | `tst/` | Rules, state transitions, collisions, save codec, pixel composition | AVR timing, SPI, physical hardware |
| Tooling | `tools/tests/` | Generators, encoding, validation, preview parity, determinism | Firmware behavior |
| Device suites | `tst/fxdatatest/` via Ardens | Real AVR execution, FX reads, SPI, screen pixels, music and stack probes | A full manual playthrough |
| Save-chain | `make fxtest-save-chain` | Exact production `updateStation()` → `requestSave()` → `saveVoyageNow()` path with music | Physical cart wear or power-loss injection |
| Save runtime page | `make fxtest-save-runtime-page` | Runtime `FX::programSavePage` path, not the compile-time constant | Hardware; emulator-only |
| Flight probe | `make fxtest-flight` | 842 complete flight frames with music, cold caches and camera boundaries | Whole-session CPU percentage |
| Size | `make size` | Whole-image flash and static RAM | Stack depth; use the painted-stack probe |
| Docs | `make docs-check` | Local links and retired-selector guards | Prose accuracy |

Tests are permanent and co-located with the code they cover. Device suites are
separate fixed `.ino` sketches that keep USB for serial markers; the release
sketch does not. The stack probe paints the free SRAM with `0xA5`, runs the real
path, scans for the high-water mark, and requires at least **600 B** of reserve
above `__bss_end`. Static free bytes alone are not accepted as proof of stack
fit.

## 6. Save system

Voyage checkpoints live in the external FX cart save partition, not the
ATmega32u4's internal EEPROM. `fxdata/fxdata.txt` ends with
`savesection 4096`; the generator emits 4 KiB of erased bytes and
`FX_SAVE_PAGE = 0xFFF0`. `FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE)` opens both
partitions, and every earlier FX offset stays unchanged.

The current schema is **8**. It stores a 25-byte scalar payload with a schema
signature (`0x61`), CRC and commit bytes. A save:

1. erases the 4 KiB block;
2. programs two 256-byte pages;
3. writes the commit byte last.

Success is reported after the second page; there is no post-write read-back. A
power interruption may lose the checkpoint or leave a torn record. Load only
checks whether the first byte is `0xFF`; any other value is read as the raw
payload. There is no CRC, schema, commit or semantic validation, and no corrupt
or incompatible title state. The retired hull byte stays at its frozen offset
and is normalized to HAULER on load. Music is paused around the synchronous
erase/program transaction. No save or cart write belongs in the per-frame path.

`make gen` must be re-run after changing the save section; `make gen-check`
compares the packed image, save partition and header. The EEPROM chart writer
and the schema-6 economy fields are retired.

## 7. Music

Six original underwater loops are authored as `atmlib-score` v1 JSON under
[`music/`](../music/). The score sources define tempo, meter, voices, effects
and loop length; the generated streams are packed into FX and referenced by
symbolic offsets and lengths.

The shipping runtime uses the **ATMlib2FX compact synth** with
`ATM_FX_COMPACT_LOOP=1` and `ATM_FX_COMPACT_RING=32`. The foreground services
the FX refill between complete SPI transactions. The sample ISR decodes from
SRAM and does not read FX, SPI or the OLED.

Cue selection follows the region and berth; music starts with the first splash
and keeps its position through the title. Cue, dock, story and pause transitions
switch tracks directly; playtest found the 160 ms fade envelope unpleasant, so
the wrapper has no gain ramp. Save pauses the music because the cart transaction
is synchronous.

The sampled IMA ADPCM path was implemented, measured and admitted as comparison
evidence. It is not the shipping backend. That reference pipeline was later
retired and removed, and the streams are no longer
declared in `fxdata/fxdata.txt`; they add no cart bytes. This is archival
evidence, deliberately kept out of `gen`, `build`, `run` and `gen-check`.

## 8. Story

The story has two canonical editing sources:

- [`story/scenes.json`](../story/scenes.json) — scene order, titles, paragraph
  text, image names and sprite-sheet row names.
- [`story/art.png`](../story/art.png) — one opaque black/white Piskel sheet,
  512×1216, with nineteen rows of four 128×64 poses.

Editing flow:

1. Edit prose in `scenes.json` or artwork in `art.png`.
2. Run `make gen`.
3. Open [`docs/story-sketchbook.html`](story-sketchbook.html) for every sequence
   and paginated text, or the complete card sheet at
   [`docs/assets/story-cards.png`](assets/story-cards.png).
4. Run `make gen-check`, host tests and the story device suite.

The generator validates JSON, image names, duplicate rows, supported
characters, empty paragraphs and over-width words. It wraps on word boundaries
at twenty columns, uppercases for the native font, and emits as many six-row
text pages as needed. Consecutive text pages are ordinary cards. Image cards
animate four poses at 300 ms each. Cards are packed as RLE frames and shared
images are packed once.

The canonical PNG may be 1-bit, palette, RGB or RGBA, including Piskel exports,
but must have the exact sheet dimensions and contain only fully opaque
black/white pixels. The generator never overwrites the two canonical sources.
Shipping card copy is never duplicated in the HTML preview, generator or tests;
the preview is generated from the same data.

Runtime semantics are intentionally modest around saves: a pending scene is a
one-byte field. Acceptance writes the load and the departure scene id before
playback. Continue resumes the pending scene. Finishing it clears the byte in
RAM only, so a reload before the next natural checkpoint can replay it:
**at-least-once**, not exactly-once. Delivery decks are saved before playback;
survey decks are unsaved field finds. HOME is the ending prologue and resumes
until the ending-presented flag is committed.

## 9. Resource accounting

The resource budgets are:

| Resource | Limit | Measurement |
| --- | ---: | --- |
| Program flash | 29,696 B | `.text` + `.data` from `make size` |
| Static SRAM | 1,700 B planning ceiling | `.data` + `.bss`; remainder is stack/locals |
| Stack reserve | at least 600 B | Painted-stack probe above `__bss_end` |
| Cart image | 2 MiB | `fxdata/fxdata.bin`, including the 4 KiB save section |

LTO makes per-symbol arithmetic misleading. A change is judged on the whole
image, before and after. The workflow for flash-sensitive work is:

1. Build and record the current whole-image number.
2. Spike the risky part.
3. Split “implement” from “make it fit”.
4. Measure the integrated change, not the sum of independent estimates.
5. If it does not fit, remove or move data to the cart; do not raise the limit
   or claim a pass.

Moving data to FX is not free: hot cart reads cost SPI time, so only cold
tables or content that already streams from the cart belong there. The final
compression wave moved cold story/dock tables to FX and replaced runtime text
drawing with baked stamps because those changes reduced hot-path work as well as
flash.

The cart is the large store: terrain, art, story, music, text stamps and save
data live there. Program flash holds code and the small tables that must stay
hot. The generated layout is reviewed as an ABI, not as an incidental build
artifact.

## 10. Working method

Work is organized with beads (`bd`). An epic describes a phase; child issues are
bounded implement/verify units with explicit acceptance criteria and
verification commands. Dependencies serialize work when tasks share a resource
or generated file. The worker protocol is:

- Read the bead and the linked contracts before editing.
- Implement and verify in one loop; report exact commands and numbers.
- Do not commit or push unless the orchestrator asks.
- If it cannot fit or a gate fails, report **BLOCKED** with the deficit and
  options; never fake a pass.
- Record durable design and measurement decisions in docs, not only in the
  issue.

The orchestrator reviews each diff and commits between waves. The full gate
before a commit is:

```sh
make gen-check
make test
make test-tools
make docs-check
make fxtest
make fxtest-save-chain
make fxtest-save-runtime-page
make fxtest-flight
make size
make mini
```

For iteration, `make fxtest FXTEST_ONLY=test_boot` runs one device suite, and
host/tooling targets can be run alone. The full gate is required for a release
commit. If Ardens is missing, device targets report SKIPPED rather than a false
pass; a run without `captureserial` is BLOCKED.

Repository conventions:

- Generated sets are staged together after `make gen`.
- Tests reference generated symbolic constants, never literal record indices.
- Dated checkpoints go in the platform or archive docs; retired designs stay
  in [`archive/`](archive/) with a superseded note.
- Commit messages are `<what> (sub-hauler-XXX.n)` style for task commits.
- The pure rules stay free of Arduino headers; device code stays free of
  floats, heap and `String`.

## 11. Reproducing the build

Prerequisites:

- `arduino-cli` with the `arduboy-homemade:avr` core;
- the sibling `../ATMlib2FX` checkout for `make gen` and the synth library;
- Ardens for device suites and flight timing (optional for a host-only build);
- `bd` only for issue tracking, not for building.

Common commands:

```sh
make gen          # regenerate all shipping assets and headers
make gen-check    # prove generation is deterministic
make build        # FX firmware -> dist/fx
make mini         # Mini firmware -> dist/mini
make size         # whole-image flash/RAM report
make test         # host C++ tests
make test-tools   # Python tooling tests
make docs-check   # local links + selector guard
make fxtest       # 15 default device suites
make fxtest-flight
make fxtest-save-chain
make fxtest-save-runtime-page
make run          # Ardens with ELF + FX image
make debug        # developer map-inspection image
make save-migrate SAVE=/path/to/absim_<hash>.save
make render-world # overview + exact native scene pixels
make music-viewer # six-song FX jukebox in Ardens
```

The retired reference pipeline used the Ardens absim source to rebuild
reference PCM/ADPCM and listening WAVs; it was not part of the shipping
build or `gen-check`.

For a first read of the code, start with `sub-hauler.ino`, then `src/app.hpp`,
then `src/game.hpp` and the view modules. For a first read of the pipeline,
start with `Makefile` (`FX_INPUTS`, `gen`, `gen-check`) and
`tools/gen_story_beats.py`. For the current limits, read
[`platform.md`](platform.md); for the conventions, [`dev-flow.md`](dev-flow.md).

## See also

- [Development history](history.md) — what changed and why.
- [Platform and resource contract](platform.md) — current limits and measurements.
- [Development flow](dev-flow.md) — gates and repository conventions.
- [Design](design.md) — game direction and contract loop.
- [Render performance](render-performance.md) — terrain streaming and camera.
- [Story beats](story-beats.md) — source format, preview and save semantics.
- [Music authoring](../music/README.md) — scores, generation and playback.
- [Level format](../levels/README.md) — map encoding and behavior.

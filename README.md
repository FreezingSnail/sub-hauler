# Sub-hauler

Sub-hauler is an Arduboy FX game rendered with **Arduboy2Base** in native 1-bit
mode. Each logical frame is one image; `FRAME(x)` is simply `x`. Cart data is
fetched through `src/lib/Blit.cpp` and the display is pushed with
`FX::display(CLEAR_BUFFER)`.

New to the game? Open the [offline player guide](docs/guide.html). It is a
static HTML file that uses relative checkout assets and needs no network
access.

Hardware, resource, and toolchain facts are canonical in
[`docs/platform.md`](docs/platform.md). Game direction and the contract-loading
model live in [`docs/design.md`](docs/design.md). The retired device-port sequence and profile history are retained in
[`docs/archive/roadmap.md`](docs/archive/roadmap.md).

## Status — 2026-11-09 (fixed build)

The device loads the authored map from [`levels/exploration.json`](levels/exploration.json):
32 rooms, 42 passages, nine gate records (six equipment gates and three tide sections) and four equipment coves.
The high-pressure region is lowest. Recover the cutter near Tidewall, then
find the pressure seal, current thruster and survey lens that open the deep
routes and the mandatory lower-right DEEP RELAY expedition. The thruster
disables every tide wall; without it, tides push downstream and block upstream
travel.
Hold still near finds or cutter grates for half a second; station rumors and
bottom-line hints provide guidance. Small C/P/T HUD marks show owned tools.

Passages wind through asymmetric caverns with changing widths. Thirty-one
physical rock features add pillars, wall teeth, shelves and wreck ribs to the
collision map. `make render-world` exports a labeled overview and the exact
native scene pixels. Baked regional scenery, wall-ledge berths and the
inhabited settlement cutaways are all streamed from the same generated native
terrain image.

The fixed build enables stations, campaign, exploration, fog chart,
currents, wall-impact particles, the fixed HAULER hull, schema 8 FX saves,
startup splashes and ATMlib2FX synth music unconditionally. There is no shop, currency,
job payout, repeatable freight offer or credit cache: campaign story deliveries
are intrinsic restoration work. There is one shipping build; the developer-only
`make debug` image is not part of the shipped matrix. The shipped matrix is:

| Image | Flash | Static RAM |
| --- | ---: | ---: |
| FX fixed build | 29,644 B | 1,665 B |
| Mini fixed build | 29,644 B | 1,665 B |

The image leaves **52 B** under the 29,696 B hardware flash limit and **35 B** of
static headroom under the 1,700 B ceiling. AVR `game::State` is 77 B. Measured
stack reserves are **680 B** for the exact production save chain and **634 B**
for the combined music probe, both above the 600 B target. The fixed native
suite passes **262** host tests and **158** tooling tests; all **15** default
device suites print `P`, and the flight probe covers **842** complete frames at
**11.788 ms maximum**, zero 16 ms misses, **gap=1 tick**, and zero audio
underruns/errors. `make gen-check` is deterministic. The latest generation
produces 17 scenes, 43 cards and 106 frames in a **67,468 B** story blob; the
FX payload/image are **658,682 / 662,784 B**. The sampled IMA ADPCM backend and
its reference-audio pipeline have been retired and removed.

Story prose comes from editable UTF-8 JSON:
[`story/scenes.json`](story/scenes.json) for scene text and
[`story/missions.json`](story/missions.json) for mission descriptions. Dock
hints live in [`story/dock_hints.json`](story/dock_hints.json); artwork is one
Piskel sheet, [`story/art.png`](story/art.png). Use the local
[story editor](docs/story-editor.html) for text-only edits, or follow the
[editing instructions](story/README.md). The packer wraps and paginates prose
automatically into as many native text cards as needed, so long handoffs keep
their full destinations, seal location and dispatch motivation. The seventeen
scenes produce 43 cards and 106 frames from 21 shared 128×64 art scenes,
including six departure briefings, both joint-restoration variants and the
mandatory lower-right DEEP RELAY result. Four
`story-handoffs-v1` bundles replace the old departure placeholders. Survey decks play when
the in-world dwell records the fact. HOME is the first part of the ending and
resumes after a reload until the ending-presented flag is committed. See
[story beats](docs/story-beats.md) for the source format, preview and
save/replay semantics.
The latest motion pass retains the original sub artwork, a separated cable break, a moving
freight carriage and reel, a sweeping beacon and visible water circulation.
The older static restoration cards stopped being drawn before `sub-hauler-jxk.2`
removed them from the packed cart; their generator, source sheets and docs
preview remain archival.

[Level encoding and behavior](levels/README.md) documents the packed 1-bit
collision/chart planes and little-endian object records.
[Actual encoded terrain](docs/assets/exploration-terrain.svg) shows the native
map geometry; the [fully rendered game world](docs/assets/exploration-world-2026-11-09.png)
is the guide’s labeled in-game overview, and the
[music-region map](docs/assets/exploration-world-music-2026-11-09.png) highlights
which travel cue plays in each area. A 41,544-byte payload prefix precedes the unchanged 409,600 B
native terrain section; later music, menu, splash, story and save sections make
the current payload/image **658,682 / 662,784 B** (**1,434,368 B** below 2 MiB).
The six sampled IMA ADPCM streams were removed from the shipping cart
(`sub-hauler-646.18`); the remaining sampled-audio reference pipeline was later
retired and removed entirely.

Normal startup shows Snailware for about 1 s, then the Sub-hauler game splash.
The title/menu music plays through the introduction and continues into the
title. The splash waits indefinitely for a fresh button press; a button held to
skip the logo must be released first, and its release is consumed before menu
input. See the [splash preview and measurements](docs/startup-splashes.md).

Gameplay sonar and its upgrade are removed, and so are the shop, currency and
repeatable freight. The station board is drawn natively: `WORK / UNDOCK`
without a job, `summary / WORK / UNDOCK` while carrying one, and `UNDOCK` alone
at a work/service berth. Mandatory story work cannot be abandoned. WORK lists
only story and survey rows. Campaign work is untimed with no payment. The shipped shop/menu frames and text
remain archival cart ABI and are never drawn. See the
[menu measurements](docs/menu-flash-design.md) and
[progression design](docs/progression.md).

The drawing pass streams native terrain directly from FX into the existing
screen buffer. A page-aligned Y origin keeps eight 128-byte bursts; a sub-page Y
origin reads the next source page and combines rows with AVR multiplies, with a
32-byte tail scratch and no second framebuffer. Stations, gates, finds,
currents, particles, the boat and HUD all use the camera's exact pixel origin, so
the whole world scrolls smoothly while collision stays tile-based. HUD fills
use framebuffer page writes. The fractional follow camera retains 1/16-pixel
state and projects the sub before rounding. The full-flight music probe covers
842 frames with zero deadline misses and zero audio errors. See
[camera measurements and verification](docs/camera-review.md).

The paused chart holds its image between sub/destination blink changes instead
of redrawing at 60 Hz, so it redraws every 16 frames while open. Chart reads use
eight bursts instead of 256 separate transactions. A fully explored chart
renders in 4.96 ms versus 10.33 ms before, with repeatable frame checksums and
the original blink cadence.

## Release runtime and uploader entry

The fixed firmware always uses the Arduboy2 `ARDUBOY_NO_USB` entry point in
`sub-hauler.ino`; the shared game operations live in `src/app.hpp`. There is no
runtime-USB production variant. Build outputs are `build/arduino` /
`dist/fx` for FX and `build/arduino-mini` / `dist/mini` for Mini.

There is no in-game bootloader option. Hold **DOWN** at power-on/reset to enter
the bootloader, so a blank or non-booting image can still be uploaded.

A desktop uploader cannot force an automatic reset from a running USB-free
game. If the uploader waits for the board, reset it (or power-cycle it while
holding **DOWN**), then start the upload. The bootloader and program-size limit
are unchanged. Device suites keep USB because their own fixed sketches are
compiled separately; only the release sketch uses the no-USB entry point.

Schema 7 uses one 4 KiB cart save block: a 16 B header plus a 24 B scalar
payload (280 B with the 256 B chart). Schema 6 and earlier records are
incompatible and are not migrated, and there is no keep-chart prompt. No
physical loader, uploader, save-write, playthrough or speaker check is claimed.

Historical 2026-10-24 profile measurements are retained in
[`docs/archive/checkpoint-2026-10-24.md`](docs/archive/checkpoint-2026-10-24.md).

## Walkthrough

```sh
make gen          # images/fonts -> Sprites/PilotSprites/fxdata.bin + headers
make gen-check    # regeneration must be byte-identical
make build        # dist/fx/sub-hauler.ino.hex + .elf
make test         # host rules tests (217)
make test-tools   # Python tooling tests (135)
make fxtest       # 15 device suites through Ardens
make fxtest-flight # 842-frame complete-flight timing with active music
make size         # flash/RAM headroom
make mini         # Arduboy Mini firmware -> dist/mini/
make run          # Ardens with the ELF + FX image
make render-world # native scene PNG + labeled overview + guide copies
make music-viewer # audition the underwater loops in ATMlib2FX's jukebox
```

Full gate: `make gen-check`, `make test`, `make test-tools`, `make fxtest`,
`make fxtest-save-chain`, `make fxtest-save-runtime-page`,
`make fxtest-flight`, `make size`, and `make mini`.

The editable music scores and viewer catalog live in [`music/`](music/README.md).
`make gen` requires the sibling `../ATMlib2FX` checkout (override
`ATMLIB2FX_ROOT` for another location). The six cues range from Abyssal Glass
(40 BPM) through Tidepool Waltz (75 BPM, 3/4) to Bubblejet Run (100 BPM), with
the original Harbor Bubbles, Kelp Drift and Sonar Lullaby loops included. The
jukebox plays the packed assets; the game decodes the same six cues with the
ATMlib2FX compact synth. The former sampled-audio reference pipeline has been
retired and is not part of the shipping `fxdata.txt` layout.

## Fixed build

There is one shipping game configuration and one host suite. `src/features.hpp`,
profile selection, chart-save EEPROM writing, alternate menus, the streaming
comparison backend and runtime-USB production builds are retired. The
developer-only `DEBUG=1` image (`make debug`) adds map-inspection no-clip and
uses `build/debug` / `dist/debug`; it is not a shipping configuration. Ardens
keys saves by build hash, so `make save-migrate SAVE=/path/absim_<hash>.save`
copies an existing save under the current image key without changing its
payload. Platform/compiler distinctions (`__AVR__`, include guards, `-mrelax`,
`-mcall-prologues`, LTO and the dependency's fixed compact synth ring/loop
settings) remain build requirements, not gameplay switches.

Build outputs are `build/arduino` / `dist/fx` for FX and
`build/arduino-mini` / `dist/mini` for Mini. The native suite writes
`build/tests/host`.

## What to edit

- `src/common.hpp` / `src/text.hpp` — device boundary and external FX text
  adapter. Keep pure rules free of Arduino headers.
- `src/game.hpp` — pure rules. Keep Arduino headers out of this file so the
  host tests can compile it.
- `src/render.hpp` — 1-bit drawing. Add scenes, HUD, and animation here.
- `sub-hauler.ino` — release entry only (`ARDUBOY_NO_USB`, `setup`, `loop`).
- `src/app.hpp` — shared hardware init, boot presentation, state and frame operations.
- `story/scenes.json`, `story/missions.json`, `story/dock_hints.json` — story,
  mission and berth-hint text. Use `make editor` or edit the JSON, then run
  `make gen`.
- `story/art.png` — canonical Piskel story-art sheet.
- `images/<section>/<symbol>_<W>x<H>.png` — authored sprites. Add a file in the
  matching source directory, run `make gen`.
- `images/pilot/hauler_26x8.png` — authored HAULER sheet: four 28-px-wide
  frames (26-px body plus a 1-px black outline). The generator trims
  transparent rows and bottom-aligns the body to the fixed 26x8 collision hull.
  Do not edit the generated files in `build/gen/`.
- `fxdata/fxdata.txt` — ordered flash layout. Append new `image_t`/`raw_t`/
  `string` entries; order is ABI.

## Generated sprite contract

`tools/gen_sprites.py --shades 2` emits a two-byte `(width, height)` header and
one page-major plus-mask plane. `Blit::draw` addresses the headerless pixel
bytes, so the device passes the symbol plus 2 and supplies the dimensions
explicitly:

```cpp
Blit::draw(x, y, HAULER_WIDTH, HAULER_HEIGHT, ::hauler + 2, FRAME(frame),
           Blit::PLUSMASK);
```

The fixed pilot sheet uses the same contract from `fxdata/PilotSprites.txt`
(`hauler`; four outlined frames). `tools/gen_pilot.py` normalizes the authored
`images/pilot/hauler_26x8.png`, writes the packed sheet source under
`build/gen/pilot/`, and emits `src/generated/pilot_host.hpp` so host pixel tests
draw the same art. The retired MINNOW/LEVIATHAN sheets are not generated or
packed. UI text uses `FX::setFont(font_6x8, dcmWhite)` plus
`FX::drawString`/`drawNumber` through `src/text.hpp`; selected rows switch to
`dcmInvert`.

`Blit` writes into `Arduboy2Base::sBuffer`; it never touches the OLED itself.
Finish the frame with:

```cpp
FX::display(CLEAR_BUFFER);
```

## Display/FX loop

```cpp
if (!arduboy.nextFrame())
    return;
arduboy.pollButtons();
run();
render();
FX::display(CLEAR_BUFFER);
```

All cart reads stay in `run()`/`render()`; the display transaction is the only
SPI handoff after them.

## Save data

The fixed build calls `FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE)`: `fxdata/fxdata.txt`
ends with `savesection 4096`, so the generated image carries one 4 KiB cart
erase block after the unchanged data prefix. A schema 7 voyage checkpoint erases
the block, programs two 256 B pages and sets the commit byte last. A power
interruption during that erase/write may lose the checkpoint; a torn record is
rejected and the title offers a clean New Voyage.

Saves happen on berth/delivery transitions with music paused; no voyage byte is
stored in internal EEPROM. Schema 6 and earlier records and old EEPROM chart
bytes are not migrated, and the standalone chart writer is retired. Keep all
save writes out of the per-frame path and run `make gen-check` after changing
`savesection`.

## Layout

```
src/common.hpp      Arduboy2Base + FX + FRAME
src/text.hpp        external FX font/string adapter
src/game.hpp        pure rules (host tests)
src/render.hpp      1-bit scene + sprite draw
src/lib/Blit.*      headerless plus-mask FX blitter
src/app.hpp         shared application operations and state
src/fxdata.h        generated mirror of fxdata/fxdata.h
src/generated/      generated sprite/legacy-world/active-level/object constants
fxdata/             fxdata.txt, Sprites.txt, PilotSprites.txt, packed .bin, fxdata.h
levels/             authored exploration JSON and binary format contract
images/pilot/       authored HAULER pilot sheet; build/gen/pilot/ is generated
fxdata/fonts/       generated external 6x8 text font
tst/                host tests + device suite
tools/              gen.sh, gen_sprites.py, gen_font.py, gen_pilot.py, fxdata-build.py, tests
docs/README.md      documentation index and archive pointer
docs/platform.md    hardware/resource contract + dated checkpoints
docs/design.md      game direction and contract-loading loop
docs/archive/       dated milestones, flash reviews and retired profile records
docs/dev-flow.md    repo conventions
docs/guide.html     offline player guide (relative checkout assets)
```

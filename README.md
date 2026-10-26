# Sub-hauler

Sub-hauler is an Arduboy FX game rendered with **Arduboy2Base** in native 1-bit
mode. Each logical frame is one image; `FRAME(x)` is simply `x`. Cart data is
fetched through `src/lib/Blit.cpp` and the display is pushed with
`FX::display(CLEAR_BUFFER)`.

Hardware, resource, and toolchain facts are canonical in
[`docs/platform.md`](docs/platform.md). Game direction and the contract-loading
model live in [`docs/design.md`](docs/design.md). The device-port sequence and
compile-time feature-flag plan live in
[`docs/roadmap.md`](docs/roadmap.md).

## Status — 2026-10-26 (chart performance and wall settlements)

The device loads the authored map from [`levels/exploration.json`](levels/exploration.json):
32 rooms, 42 passages, nine equipment gates and eight hidden cove rewards.
The high-pressure region is lowest. Recover the cutter near Tidewall, then
find the pressure seal and optional current thruster to open more routes.
Hold still near finds or cutter grates for half a second; station rumors and
bottom-line hints provide guidance. Small C/P/T HUD marks show owned tools.

Passages wind through asymmetric caverns with changing widths. Thirty-one
physical rock features add pillars, wall teeth, shelves and wreck ribs to the
collision map. `make render-world` exports a labeled overview and the exact
native scene pixels; `make dev` lets you inspect the whole world freely.

Each region now has baked native scenery: layered stone and moorings in the
Lantern Reaches, kelp and pump equipment around Benthos, worn hull plating and
freighter wrecks in the Broken Works, and glass crystals and chimney vents in
the trench. Eighteen larger landmarks and 126 smaller growths/scrap formations
stay clear of navigation corridors and interactive markers. The image uses
the same storage, reads and runtime code. `make render-world` also exports
`build/world-render/game-world-regions.png`, eight native viewport crops at 4×.
The Works walls use sparse broken seams and rivet runs aligned with the wall,
with gaps and staggered placement rather than repeating rectangular panels.

The four real docking points now sit on cave-wall ledges, with short piers
and airlocks leading into inhabited rock cutaways. Tidewall has homes, Benthos
has grow rooms, Cormorant has workshops, and Deep Anchor has pressure capsules.
Each settlement contains four visible rooms. Navigation-room centres stay
separate from berths; chart markers, auto-docking and home spawn use the new
berth coordinates. Native views are exported to
`build/world-render/game-world-settlements.png`.

Cave boundaries are now reconstructed at native pixel resolution, with rounded
noses and continuous slopes instead of square tile steps. Scenery roots follow
the curved floors. Collision tiles keep their original geometry; visual wall
movement is limited to a three-pixel boundary band. All smoothing is baked
offline, so image storage, RAM and rendering cost stay unchanged.

[Level encoding and behavior](levels/README.md) documents the packed 1-bit
collision/chart planes and little-endian object records.
[Actual encoded terrain](docs/assets/exploration-terrain.svg) shows the native
map geometry. The preceding 41,572-byte cart payload remains an identical prefix;
the native terrain adds 409,600 B, making payload/image **451,172 / 451,328 B**.
`make gen` is still the only
generation entry and the six music cues remain available in the jukebox.

| Profile | Flash | Static RAM |
| --- | ---: | ---: |
| `full` | 20,176 B | 1,583 B |
| `lean` | 17,368 B | 1,571 B |
| `core` | 14,756 B | 1,313 B |
| `min` | 8,750 B | 1,252 B |

AVR run state is 91 B; static headroom is 117 B and the expanded device probe
measures 756 B stack reserve against the 600 B target. The chart optimization
adds 44 B flash and no static RAM against a fresh build of its source snapshot.
All six device suites pass, alongside 150/117/89/44 host tests, 105 tooling
tests, deterministic generation and the full Mini build. An optional chart-save
host build passes 154 tests.
No manual interactive playthrough or physical-device soak is claimed.

Exploration is enabled in full/lean/core; min remains free-flight/cave with
barriers disabled. Equipment, reward claims and opened grates last for the
current run. Campaign restoration missions, an ending and persistent voyage
checkpoints remain proposed in [`docs/campaign-exploration.md`](docs/campaign-exploration.md).
Freight allowances follow the winding route lengths and allow intermediate
berth stops for resupply. Rewards retain the existing economy. Gameplay still
has no music player.

The drawing pass streams native terrain directly from FX into the existing
screen buffer in eight 128-byte bursts, then draws objects and HUD overlays.
HUD fills use framebuffer page writes. The AVR benchmark measures 34–58% less
render/presentation time in dev mode, with identical frame checksums.
See [render timings and reproduction commands](docs/render-performance.md).

The paused chart now holds its image between sub/destination blink changes
instead of redrawing at 60 Hz, so it redraws every 16 frames while open. Chart
reads use eight bursts instead of 256 separate transactions. A fully explored
chart renders in 4.96 ms versus 10.33 ms before, with repeatable frame
checksums and the original blink cadence.

## Developer map traversal

```sh
make dev                    # build and launch free-flight in Ardens
make build DEV_MODE=1       # build only: dist/dev/full/
make run                   # normal game
```

Developer mode starts directly in flight. D-pad moves directly through rock
and equipment gates; hold **A** for faster movement (6 px/frame, versus 2).
**B** opens/closes the fully revealed chart; the existing A+B pause gesture
remains available. Air/hull stay full, currents/mines cannot affect travel,
and station rings do not auto-dock. The bottom line reads `DEV NOCLIP A FAST`.
Coves, closed gates and unclaimed rewards remain visible for inspection;
free-flight does not collect finds or advance contracts. The hull/camera stay
within the world bounds. Chart EEPROM loading/writing is disabled in this
mode, including if the chart-save feature is explicitly enabled.

`DEV_MODE` defaults to 0 and accepts only 0/1. Developer artifacts are isolated
under `build/dev/<profile>/` and `dist/dev/<profile>/`. `make test DEV_MODE=1`
runs the normal rules suite plus five free-flight cases (154 passing in full).
The dev firmware measures 15,592 B flash / 1,582 B static RAM. All six device
suites pass in both modes. `make dev` uses the full profile by default; other
profiles retain their configured chart availability.

## Historical checkpoint — 2026-10-24 (cargo-free resource pass)

Content is frozen at commit `4f66830` (`sub-hauler-m9-save`); the working tree
adds the post-release fixes, the cargo-packing removal, and the UI/art resource
pass. Contract acceptance now auto-loads the four cargo units while the boat
stays docked; an explicit UNDOCK row or B starts travel. The packing screen,
LOAD CARGO row, commit chord, and cargo runtime were removed because their value
did not justify the flash/SRAM cost. The remaining UI labels and the 6x8 font
now live in FX, the three pilot silhouettes are baked FX sprites, the route
economy tables are generated, and the AVR run state is 80 B. Every profile
builds, all host/tool tests are green, and all three remaining device suites
print `P` through Ardens. No manual interactive playthrough or manual device
soak was run; the soak evidence is the permanent host loop test across all
three hulls plus the device-suite smoke run.

| Profile | Flash | Static RAM | Notes |
| --- | ---: | ---: | --- |
| `full` | 17,548 B | 1,572 B | 12,148 B under hardware; 11,452 B under the 29,000 B planning ceiling |
| `lean` | 14,792 B | 1,560 B | no currents, mines, particles, shop, or save |
| `core` | 12,626 B | 1,302 B | single HAULER, no fog/sonar/multi-hull |
| `min` | 9,142 B | 1,252 B | pilot + cave only |

Flash hardware limit is **29,696 B**; static planning ceiling is **1,700 B**
with **128 B** headroom and **988 B** of static free SRAM. The stack probe
(`test_memory`, full profile) reports **`stack_reserve=812`** against the
unchanged **600 B** target. `make mini PROFILE=full` builds the same
17,548 B / 1,572 B image for the Arduboy Mini.

Against the cargo-free baseline (20,196 / 17,362 / 14,756 / 10,456 B flash and
1,592 / 1,580 / 1,321 / 1,259 B static), the final pass saves
**2,648 / 2,570 / 2,130 / 1,314 B flash** and **20 / 20 / 19 / 7 B static**.
The compact position-only world metadata fell from 148 B to 88 B PROGMEM with
no RAM or cart-read change; that A/B measured 17,584/14,810/12,640/9,178 ->
17,548/14,792/12,626/9,142 B. The 128 B cave atlas intentionally stays in
PROGMEM because moving it to FX adds hot render traffic for no RAM win.

The post-M9 menu fix still makes the station, shop, pause, and failure screens
compose only on an input edge and skip the OLED transaction while idle.
Selected rows use a raw `fillRect` bar with `dcmInvert`; only the external-font
glyph strokes are black, so no opaque 6x8 cell can paint through the next 7 px
row.

`SH_FEATURE_SAVE_CHART` remains default **0** and is inactive in every shipped
profile. The measured `SAVE_CHART=1` variant was last recorded at
29,330 B / 1,690 B before the cargo carve; the carve freed far more than that
overage, but a fresh variant measurement is still required before changing the
default.

The shipped `cargoBank` FX declaration stays at offset **0x001AC2** with its
original bytes and order for flash ABI compatibility. The font, strings, and
pilot sheets were appended after it. Six original underwater music
loops now follow those sheets; the FX payload in
`fxdata/fxdata-data.bin` is **34,540 B**, while the padded flashable
`fxdata/fxdata.bin` image is **34,560 B**. Only
the cargo runtime reader, screen, rules, and tests were removed.

The default Arduboy font and `Print` path are absent from the ELF: the firmware
uses `Arduboy2Base` plus the external font adapter in `src/text.hpp`.

## Walkthrough

```sh
make gen          # images/fonts -> Sprites/PilotSprites/fxdata.bin + headers
make gen-check    # regeneration must be byte-identical
make build        # dist/<profile>/<project>.ino.hex + .elf
make test         # host rules tests (150 full / 117 lean / 89 core / 44 min)
make test-tools   # Python tooling tests (105)
make fxtest       # device smoke/render tests through Ardens (6 suites, each P)
make size         # flash/RAM headroom
make size-matrix  # build and report full, lean, core, min
make mini         # Arduboy Mini firmware for the selected profile
make run          # Ardens with the ELF + FX image
make dev          # developer noclip + full chart + A speed boost
make render-world # native scene PNG + labeled full-world overview
make music-viewer # audition the underwater loops in ATMlib2FX's jukebox
```

Full release gate: `make gen-check`, `make test`, `make test-tools`,
`make fxtest`, `make size`, `make size-matrix`, and `make mini PROFILE=full`.

The editable music scores and viewer catalog live in [`music/`](music/README.md).
`make gen` requires the sibling `../ATMlib2FX` checkout (override
`ATMLIB2FX_ROOT` for another location). The six cues range from Abyssal Glass
(40 BPM) through Tidepool Waltz (75 BPM, 3/4) to Bubblejet Run (100 BPM),
with the original Harbor Bubbles, Kelp Drift and Sonar Lullaby loops included.
The jukebox plays the packed assets; normal gameplay has no music player yet.

## Feature profiles

`tools/features.py` is the single source of profile values and dependency
rules. Valid profiles are `full` (default), `lean`, `core`, and `min`; every
flag is passed explicitly as a numeric `-DSH_FEATURE_*` macro.

```sh
make build PROFILE=lean
make size PROFILE=core
make feature-report PROFILE=lean
make size-matrix            # build and report full, lean, core, min
```

Builds and images are separated by profile (`build/<profile>/`,
`dist/<profile>/`) so objects compiled under different flag sets cannot mix.
An unknown profile or an invalid dependency fails before compilation. Feature
flags never change generated FX data.

## What to edit

- `src/common.hpp` / `src/text.hpp` — device boundary and external FX text
  adapter. Keep pure rules free of Arduino headers.
- `src/game.hpp` — pure rules. Keep Arduino headers out of this file so the
  host tests can compile it.
- `src/render.hpp` — 1-bit drawing. Add scenes, HUD, and animation here.
- `sub-hauler.ino` — hardware setup, input sampling, and the frame loop.
- `images/<symbol>_<W>x<H>.png` — authored sprites. Add a file, run `make gen`.
- `images/pilot/*.png` — generated fixed-hull sheets; edit `tools/gen_pilot.py`,
  not the PNGs.
- `fxdata/fxdata.txt` — ordered flash layout. Append new `image_t`/`raw_t`/
  `string` entries; order is ABI.

## Generated sprite contract

`tools/gen_sprites.py --shades 2` emits a two-byte `(width, height)` header and
one page-major plus-mask plane. `Blit::draw` addresses the headerless pixel
bytes, so the game passes `::hero + 2` and supplies the dimensions explicitly:

```cpp
Blit::draw(x, y, HERO_WIDTH, HERO_HEIGHT, ::hero + 2, FRAME(frame),
           Blit::PLUSMASK);
```

The fixed pilot sheets use the same contract from `fxdata/PilotSprites.txt`
(`hauler`, `leviathan`, `minnow`; four frames each). UI text uses
`FX::setFont(font_6x8, dcmWhite)` plus `FX::drawString`/`drawNumber` through
`src/text.hpp`; selected rows switch to `dcmInvert`.

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

`FX::begin(FX_DATA_PAGE)` is the single-argument form. The optional chart save
(`SH_FEATURE_SAVE_CHART`, default `0`) lives in the ATmega32u4 internal EEPROM
as a 260 B versioned, CRC-8-checked block; it loads once in setup and writes
only when the paused chart closes. It needs no `FX_SAVE_PAGE` and adds no static
RAM. Only if a future save outgrows internal EEPROM or needs cart-sized state
should the image be regenerated with `FX_SAVE_PAGE`. Keep writes out of the
per-frame path.

## Layout

```
src/common.hpp      Arduboy2Base + FX + FRAME
src/text.hpp        external FX font/string adapter
src/game.hpp        pure rules (host tests)
src/render.hpp      1-bit scene + sprite draw
src/lib/Blit.*      headerless plus-mask FX blitter
src/fxdata.h        generated mirror of fxdata/fxdata.h
src/generated/      generated sprite/legacy-world/active-level/object constants
fxdata/             fxdata.txt, Sprites.txt, PilotSprites.txt, packed .bin, fxdata.h
levels/             authored exploration JSON and binary format contract
images/pilot/       generated fixed-hull pilot sheets
fxdata/fonts/       generated external 6x8 text font
tst/                host tests + device suite
tools/              gen.sh, gen_sprites.py, gen_font.py, gen_pilot.py, fxdata-build.py, features.py, tests
spike/              v1 auto-forward feel test (not firmware)
spike-trim/         v2 browser reference; cargo packing is historical, not shipped
docs/platform.md    hardware/resource contract + dated checkpoints
docs/design.md      game direction and cargo-free contract-loading model
docs/roadmap.md     device-port milestones, feature flags, budgets
docs/dev-flow.md    repo conventions
```

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

## Status — 2026-10-28 (voyage progression, cart checkpoints and music)

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
| `full` (campaign + voyage save + music) | 27,010 B | 1,619 B |
| `lean` | 14,406 B | 1,418 B |
| `core` | 11,812 B | 1,159 B |
| `min` | 5,912 B | 1,115 B |

The full default fits the 29,696 B hardware limit with 2,686 B free and the
29,000 B planning line by 1,990 B; static RAM leaves 81 B under the 1,700 B
ceiling and 941 B free. AVR `game::State` is 99 B in the campaign+save build
(79 B with campaign off). Measured stack reserves are 728 B for campaign+save,
666 B for the exact production save chain with active music, and 646 B for the
combined music probe, all above the 600 B target. All eleven default device
suites pass, alongside 242/145/115/72 host tests, 135 tooling tests,
deterministic generation and the full Mini build.
No manual interactive playthrough, physical save write or speaker check is
claimed.

Exploration is enabled in full/lean/core; min remains free-flight/cave with
barriers disabled. The `full` profile enables the complete voyage progression:
seven restoration objectives, restored refuge berths, one cart checkpoint slot,
homecoming and six regional music cues. Lean/core keep campaign/save/music off;
min keeps its flight-only behavior. The
[progression design](docs/progression.md) records the accepted direction and
the [exploration proposal](docs/campaign-exploration.md) the wider context.
Freight allowances follow the winding route lengths and allow intermediate
berth stops for resupply. Rewards retain the existing economy.

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

## Release runtime and uploader entry

Shipping images compile with `SH_RELEASE_NO_USB=1` (the Makefile default) and
the Arduboy2 `ARDUBOY_NO_USB` entry point immediately before `setup()`. This
removes the runtime USB stack while keeping the bootloader, the supported
startup button path and the in-sketch `Arduboy2Core::exitToBootloader()` API.
A runtime-USB development image is still available:

```sh
make build RELEASE_NO_USB=0   # USB runtime for serial development/measurement
make build                    # USB-free shipping image
make dev                      # DEV_MODE=1 defaults to the USB runtime
```

The title screen has an explicit **LOADER** row. Select it with **UP/DOWN** and
press **A** to enter the bootloader. The library also checks **DOWN** at
power-on/reset, so a blank or non-booting image can still be uploaded by
holding **DOWN** while resetting. The `full`/`lean` title and the Mini full
image have the row; `core` and `min` boot directly to the pilot and rely on the
power-on **DOWN** path.

A desktop uploader cannot force an automatic reset from a running USB-free
game. If the uploader waits for the board, reset it (or power-cycle it while
holding **DOWN**), then start the upload. The bootloader and program-size limit
are unchanged. Device suites keep USB because their own sketches are compiled
separately; only the release sketch uses the no-USB entry point.

Measured on 2026-10-28 with the full profile:

| Image | Flash | Static RAM |
| --- | ---: | ---: |
| USB-free release, full default | 27,010 B | 1,619 B |
| Mini, full default | 27,010 B | 1,619 B |
| USB-free release, lean/core/min | 14,406 / 11,812 / 5,912 B | 1,418 / 1,159 / 1,115 B |

The single WORK board (`.27`) removes the station offer rows and all job
timers: story restoration is listed first and the one repeatable freight row
sits beneath. The full default now enables campaign, voyage save and music and
measures **27,010 B / 1,619 B**, inside the 29,696 B hardware and 1,700 B static
limits. A build-only actual-game probe with the real player ISR, six cues,
cold/cached world paths, restoration overlays and WORK/ending/title/prompt
panels measured **903 B** stack reserve against 600 B; the current combined
probe reports **646 B** and the exact production save chain **666 B**.
`test_music` reports zero underruns and zero errors. Schema 4 uses one 4 KiB
cart save block; schema 1/2/3 records and the old chart bytes are not migrated,
and there is no keep-chart prompt. No physical loader, uploader, save-write or
speaker check is claimed.

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
runs the normal rules suite plus the free-flight cases. `make dev` uses the
full profile and therefore the full campaign/save/music flag set while keeping
the USB runtime; that developer image is not a release image and may exceed
the shipping flash budget. Release gates use `DEV_MODE=0` and the USB-free
entry point.

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
make test         # host rules tests (242 full / 145 lean / 115 core / 72 min)
make test-tools   # Python tooling tests (135)
make fxtest       # device smoke/render/save/music suites through Ardens (11 each P)
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
The jukebox plays the packed assets; the full game also streams the same six
cues through the sibling ArduboyStreamFX player.

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
An unknown profile or an invalid dependency fails before compilation. The
`full` profile enables `CAMPAIGN=1 SAVE_VOYAGE=1 MUSIC=1` with `SAVE_CHART=0`;
lean, core and min keep all four off. Feature flags never change generated FX
data except that music builds link the sibling library.

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

`FX::begin(FX_DATA_PAGE)` is the single-argument data form. Campaign builds use
`FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE)`: `fxdata/fxdata.txt` ends with
`savesection 4096`, so the generated image carries one 4 KiB cart erase block
after the unchanged data prefix. A voyage checkpoint erases the block, programs
two 256 B pages and sets the commit byte last. A power interruption during that
erase/write may lose the checkpoint; a torn record is rejected and the title
offers a clean New Voyage.
Saves happen on berth/delivery transitions with music paused; no voyage byte is
stored in internal EEPROM and schema 1/2/3 records are not migrated.

The optional standalone chart save (`SH_FEATURE_SAVE_CHART`, default `0`, and
mutually exclusive with voyage save) still uses a 260 B versioned,
CRC-8-checked internal-EEPROM block; it loads once in setup and writes only when
the paused chart closes. Keep all save writes out of the per-frame path and run
`make gen-check` after changing `savesection`.

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

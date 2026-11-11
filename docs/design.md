# Design — sub-hauler

Status: post-review design (2026-10-24, cargo-free resource pass). The
cargo-packing minigame was removed in `sub-hauler-gl1.1` because its value did
not justify its flash/SRAM cost. The final pass moved the surviving UI text and
fixed pilot art into FX and compacted the generated world metadata and AVR run
state. The browser sandbox is retired (available in git history); the shipped
device path is the cargo-free contract loop described below.

Current implementation note (2026-11-01): the fixed build ships the
eight-objective campaign and ending, schema 7 single-slot cart checkpoints,
ATMlib2FX synth music and the boot introduction. Sonar is retired. The shop,
currency, job payouts, repeatable freight offers and credit caches were removed;
campaign deliveries are intrinsic restoration work and all work is untimed.

## Active exploration map — 2026-10-25

The device now loads the authored `levels/exploration.json` world, with
32 rooms, 42 passages, six equipment gates, three tide sections and four equipment coves. The high
pressure region is lowest. `make gen` appends its collision/chart/object
sections and writes the active `src/generated/level.hpp`; legacy seed-4242
sections and `src/generated/world.hpp` stay unchanged for ABI compatibility.
The active terrain uses winding, variable-width passages and asymmetric
caverns with 31 solid pillars, shelves, wall teeth and wreck ribs. Work is
untimed and unpaid; optional shortcuts save travel time only.
See [the level contract](../levels/README.md) for encoding, controls and limits.
The older baked-world measurements below are historical. Campaign/ending,
persistent cart voyage saves and gameplay music now ship in the fixed build;
the [progression design](progression.md) records the accepted direction.

## Direction

- The cutaway is the identity of the game. The external sub is the travel view;
  the inside is where cargo, ballast, air, engine, and crew live.
- Cargo belongs inside the hull. The pilot always uses the fixed HAULER
  external hull and cargo is represented by coarse internal mass, trim,
  acceleration, and top speed. Accepting a contract auto-loads the story cargo; the
  player starts travel with an explicit UNDOCK row action or B.
- The internal view should be used at stations and for maintenance work, not
  as a second full-time flight view.
- Feel is slow, heavy, deliberate. The former packing puzzle is removed rather
  than gated: there is no loading screen, LOAD CARGO row, commit chord, or
  arrangement runtime on device.

## Sub class

The fixed build has one playable hull, HAULER. The title starts NEW VOYAGE
directly with it; LEFT/RIGHT hull cycling, the B reset action and the LEN
statistic are retired.

| Class | Hull | Hold (legacy) | Status |
| --- | ---: | ---: | --- |
| MINNOW | 64 px | 6×3 hex bay | retired; legacy save id 0 only |
| HAULER | 94 px | 11×3 split/notched bay | playable |
| LEVIATHAN | 118 px | 14×4 twin-spine bay | retired; legacy save id 2 only |

The `FLEET` browser reference shows three round hulls, external and cutaway:
- MINNOW: TADPOLE rounded shuttle with bow viewport and landing skids.
- HAULER: FLATBED rounded freighter with deckhouse and cargo doors.
- LEVIATHAN: FORTRESS rounded heavy with outrigger pods and deckhouse/gantry.

The device draws only the HAULER silhouette from baked FX sprites with
right/left and unloaded/loaded frames, shared by title and flight; the host
tests retain the equivalent procedural shape for exact-pixel checks. The
retired Minnow/Leviathan pilot sheets and the legacy hold dimensions are no
longer packed: `sub-hauler-jxk.1` removed the cart declarations under the
approved one-time ABI exception. `tools/gen_pilot.py` and `tools/gen_cargo.py`
retain the historical hull fills/bank as archival provenance only. No flight,
title or station code references them. Schema-8 saves keep their hull byte at
the frozen offset, always write HAULER, and normalize an older
Minnow/Leviathan byte to HAULER on load without dropping voyage progress.

## Contract loading loop

1. Dock (auto-dock within 14 px of a settlement gantry at or below the dock
   speed; the hull stops in place instead of snapping to the gantry centre).
2. Open `WORK`, select an eligible story restoration row or survey lead, and
   press `A`. Acceptance fills the hold with the mission's four coarse cargo
   units, neutralizes trim, and keeps the boat docked. Survey rows open the
   chart instead. Mandatory story work has no abandon action or confirmation.
3. Confirm the active board's `UNDOCK` row with `A`, or press `B`, to start
   travel. Docking latch and flight pause are unchanged.
4. Deliver at the destination station. Work is untimed and unpaid, and a loaded
   arrival resolves exactly once; wrong-berth stops keep the job and load.
   Repair cargo completes at the Pump Refuge pump or Freight Yard lift.

The top-level station board is `WORK / UNDOCK`; while carrying a job it is
`summary / WORK / UNDOCK`, and a work/service berth shows `UNDOCK` alone. `WORK` opens the single board with eligible story rows only. There is no
shop row, no separate offer list, no `LOAD CARGO` row, no job timer or
late-failure state, and no way to reach a packing screen.

### Historical: packing prototype (removed)

The M5–M8 packing flow was implemented and then removed in `sub-hauler-gl1.1`:
selection, suspended movement, support, lift blocking, rotation, undo, the
`A+B` commit, hold-B exit timer, and per-class arrangement loading. The design
and measurements remain in `docs/archive/roadmap.md` as history. Do not reintroduce or
optimize those paths without a new user decision and a measured budget.

## Random shape generation (browser reference only)

Browser-only generator, retained as historical reference for the removed
packing prototype. It is not part of the firmware.

1. Choose a bottom-filled target region:
   - base fill per column, plus random extra cells in the top row(s);
   - total occupied cells equals the hull load (12 / 26 / 40) inside the hull's open hold mask.
2. Start with one cluster per occupied cell.
3. Repeatedly merge orthogonally adjacent clusters while the cluster count is
   above the hull piece count:
   - reject merges over the hull's max area/bounding box;
   - prefer merges among the smallest candidates, with random choice;
   - update area, bbox, and owner map.
4. Normalize each cluster into a connected polyomino.
   - Max 4×4 bounding box, max 8 cells.
   - Randomly assign required/bonus kinds.
   - Store the target placement as the known solution.
5. If random merging stalls, retry. After a bounded number of retries, fall
   back to splitting target columns into connected groups.

This constructs a packing rather than solving arbitrary shapes, so every
generated load has a full arrangement by construction.

The JS generator uses `Math.random`, `Set`, `Array.sort`, `filter`, object
arrays, and dynamic allocation. It was never portable as-is.

## Archival cargo arrangement generator (not packed)

`tools/gen_cargo.py` still reproduces `fxdata/tables/cargo.bin` (3,648 B) and
`src/generated/cargo.hpp`, but `sub-hauler-jxk.1` retired the `raw_t cargoBank`
cart declaration under the approved one-time ABI exception. The bank is no
longer packed or included by any device header or test; it is archival
provenance alongside the generator. No cart symbol resolves to cargo bytes.

Historical record layout:

```c
struct CargoPieceRecord {
    uint16_t mask;   // 4x4 normalized shape, bit set = filled cell
    uint8_t  posKind;// x: 0-3, y: 4-5, bonus: 6, spare: 7
};
```

- 3 bytes per piece; 64 arrangements per hull.
- MINNOW 12 B / HAULER 21 B / LEVIATHAN 24 B per arrangement.
- Total bank: 3,648 B in external FX flash, zero program flash.

Rules for future work:
- Append FX declarations only; reclaiming shipped bytes requires an approved
  ABI exception (the `sub-hauler-jxk.1` retirement is the one-time precedent).
- Generate tables with `make gen`; never hand-edit packed tables.
- Run `make gen-check` to prove reproducibility.
- Reading this bank again requires a new measured feature decision.

## Device text and fixed art — historical 2026-10-24 snapshot

The following records the initial cargo-free text/art pass. Current station
presentation is drawn natively; the baked shop frames remain archival cart ABI
and are never drawn. See [menu-flash-design.md](menu-flash-design.md). Current
cart and firmware totals are in [platform.md](platform.md).

All device text uses baked 128x8 plus-mask FX stamps (`sub-hauler-2va`);
`src/text.hpp` owns `text::stamp`/`stampFrame` and `Blit::draw`, and the
external 6x8 font plus `print`/`number`/`character` remain only for reference
tests. Strings stay deduplicated in `fxdata/fxdata.txt` as the generator
source. The firmware uses `Arduboy2Base`, so the inherited `Print`/built-in-font
path is never instantiated; release code also drops the external font renderer.
Stamps keep the same 6x8 cell and layout constants, so station rows stay at
7 px pitch and 128x64 clipping is unchanged.

The authored HAULER sheet is `images/pilot/hauler_26x8.png`: four 26-px-wide
frames (right empty, right loaded, left empty, left loaded). `gen_pilot.py`
validates the palette and mirroring, trims transparent rows, and writes a
normalized sheet to `build/gen/pilot/`; `gen_sprites.py` packs it into a plus-mask
include. Art may be taller than the fixed 26x8 collision hull and is
bottom-aligned, so the mast extends above the hull box. Host pixel tests use
`src/generated/pilot_host.hpp` from the same normalized pixels, keeping one art
source. Title and pilot share the frames through `Blit::draw`. The retired
MINNOW/LEVIATHAN fills remain in `tools/gen_pilot.py` as archival provenance and
are not packed. The multi-hull selector is retired, so only the HAULER sprite
call is compiled into device code.

Both additions were append-only at the time. The shipped `cargoBank` and the
legacy `worldTiles`/`worldChart` tables are no longer packed after
`sub-hauler-jxk.1`; their generator outputs remain archival. Current cart and
firmware totals are in [platform.md](platform.md).

## Baked world pipeline (M1, historical)

`tools/gen_world.py` is the single offline source of cave truth. It is a
standard-library Python port of the retired browser `generateCave` with
seed `4242`; generation does not invoke Node or a browser. The PRNG, chamber
list, tunnel wiggle and seal pass match the browser, so the
baked bytes match the browser grid for the same seed.

Artifacts:

| File | Size | Meaning |
| --- | ---: | --- |
| `fxdata/tables/world.bin` | 6,400 B | 320×160 tiles, 1 bpp, 1 = solid |
| `fxdata/tables/world-chart.bin` | 256 B | 64×32 cells, 1 bpp, 1 = water |
| `src/generated/world.hpp` | small | constants + metadata (host `constexpr`; AVR position-only `PROGMEM`) |

Layout is headerless and row-major. The tile plane uses 40 B per row and the
chart plane uses 8 B per row; byte `y * row_bytes + x / 8` holds cell `x` in
bit `x & 7`, LSB = leftmost. There is no per-file version byte: `LAYOUT_VERSION`
and both sizes are generated in `world.hpp`. The chart cell covers the 5×5
tile block at the same coordinates, so `world.bin` and `world-chart.bin` share
one 1:5 downsample grid. A chart cell is water when any tile in its block is
open; this matches the browser chart.

The generator validates dimensions, the two-tile sealed border, the open spawn
footprint, open and clear settlement chambers, a single connected open
component (junctions and dead-end branches included), and exact chart-mask
consistency. `tools/tests/test_gen_world.py` covers known bytes/bits, the
downsample rule, determinism, and corrupt-input rejection.

`world.hpp` metadata is split by target. Host tests get full `constexpr`
records including settlement names. AVR code never references those arrays:
`src/generated/world.hpp` also emits position-only settlement and current
records in `PROGMEM`, and `world::settlementInfo()`/`currentInfo()` read them
with `pgm_read_byte`/`pgm_read_word`. This keeps both
costs off SRAM: an odr-used namespace-scope `constexpr` array is copied into
SRAM by the AVR startup code, and name pointers would drag their string
literals along with it. `Settlement::name` is therefore null on device. The
compact AVR metadata total is **88 B of program flash** (down from 148 B) with
no RAM or cart-read change. No route reward or deadline table is generated: the
fixed rules add no diagnostic counters or job clock to `game::State`.

### Cave view and visible window (M1.4)

`src/cave.hpp` holds the window math and tile atlas, `src/cave_render.hpp`
composes a frame, and `src/render.hpp` is the device entry. The view is native
1-bit and integer-only: no float, anti-aliasing or greyscale.

- The camera is a pixel-precise 16×8 window. `cave::cameraAtPixel()` stores the
  top-left pixel as floor tile plus `subX/subY` remainder and clamps it to
  `[0, 2432] × [0, 1216]`, so the window always covers exactly 128×64 pixels
  inside the baked map, including at the origin and both far edges. Terrain and
  every world overlay scroll from that same origin; collision and the tile
  cache keep using the floor tile.
- `world::refreshCache(cache, reader, cameraTileX - 1, cameraTileY - 1)` fills
  the 18×10 tile cache one tile before the window. The extra ring is what makes
  every visible tile's four neighbours available for edge selection without a
  second cart pass.
- Rendering walks only the 16×8 = 128 visible tiles. Nothing loops over the
  320×160 map, and no world-sized buffer exists.
- The 8×8 atlas is 16 tiles × 8 bytes, one byte per column in display page
  order: **128 B in program flash, 0 B SRAM**. Index is a four-bit open-neighbour
  mask; each open side is trimmed by one pixel and two open adjacent sides add a
  45-degree chamfer, which rounds the rock silhouette. Interior rock gets one
  deterministic dark pixel per tile as texture. The atlas intentionally stays in
  PROGMEM: moving it to FX adds hot per-tile render traffic for no RAM win.
- Settlements are drawn by iterating the generated ids and reading the PROGMEM
  metadata: a station body, beam, dock ring, and clamps centred on the
  settlement `pixel_x/pixel_y`. No name and no second position table.
- The camera is static in M1. `refreshCache()` therefore reads the 18×10 window
  (180 bytes, one FX transaction each) on the first frame and performs no cart
  reads afterwards. Steady state is 128 atlas copies and the gantry rectangles,
  composed into the display buffer before the single `FX::display(CLEAR_BUFFER)`.

Measured 2026-10-22: whole image **6,840 B flash / 1,217 B static RAM**, versus
6,636 B / 1,214 B for the template. The delta includes removing the walker,
ground, hero, and HUD: the atlas is 128 B flash, AVR settlement metadata 36 B
flash, and the runtime cave state is a 28 B `TileCache` plus 4 B camera. The
static delta is only +3 B because the removed `game::State` offsets most of that
32 B. 60 fps is not device-measured here; Ardens is unavailable, so the
checkpoint records build/static evidence only.

## Cold cart tables (2026-11-11)

Program flash is the tight budget, so cold lookup tables that are read rarely
live on the FX cart instead of in a second program-flash copy:

- Story metadata (`fxdata/tables/story-meta.bin`, 122 B): scene card counts,
  first-card indices, card frames and card kinds in one contiguous blob.
  `tools/gen_story_beats.py` emits it; `src/story_view.hpp` reads one byte at a
  time on AVR and keeps the `constexpr` arrays for host tests only.
- Baked text stamps (`fxdata/TextData.txt`): every runtime
  label is a two-frame 128x8 plus-mask image. `tools/gen_text_images.py` owns
  the 6x8 glyph raster; the per-berth hint matrix lives in
  `story/dock_hints.json` and is baked into `img_dock_barks`. `Blit::draw` reads
  the headerless frames directly, so the station board and WORK clue spend no
  font draw calls or per-glyph cart reads.
- Berth icons (`images/panels/berth_icons_16x8.png`, 130 B packed): eight 16x8
  opaque silhouettes authored in `tools/gen_panels.py`, packed into the panel
  sprite sheet and drawn with one `Blit::draw`.

The external 6x8 font is still cart-resident for reference tests but no release
code references its renderer. These additions add no static SRAM or a
display-transaction cart read; the debug NOCLIP text hint was removed in the
`sub-hauler-oad` pass, so the developer image fits the hardware limit.

## Arduboy fit

From `docs/platform.md`:

- 128×64 SSD1306, 1,024 B framebuffer.
- 2,560 B SRAM; **895 B** static free in the current full image.
- 29,696 B flash; current full image **29,378 B flash / 1,665 B static RAM**
  (318 B flash free, 35 B static headroom under the 1,700 B planning ceiling).
- AVR `game::State` is **77 B**; the compact generated world metadata is 88 B
  PROGMEM; the 128 B cave atlas stays in fast program flash.
- No floats, no heap in device code, no cart reads during display.

The archival `cargoBank` generator output is no longer packed into external FX
flash after `sub-hauler-jxk.1`, so it consumes no cart, program-flash or static
SRAM budget. The cargo-free station/job/undock path is the device path. Measure
every future change with `make size`; do not assume the remaining
headroom survives another feature.

## Browser reference (retired)

The `spike/` and `spike-trim/` sandboxes were removed; use git history for
their sources.

- `PILOT` — external sim, stations, contracts, shop. Draws the selected round
  hull; cargo is internal mass, not a stack above the hull.
- Loading screen — historical packing prototype; not shipped on device.
- `FLEET` — external and cutaway profiles for the same HAULER hull.

The browser cargo generator is reference-only; the retained FX arrangement
bank is an ABI artifact, not a runtime plan.

## Open questions

- How exactly coarse cargo units map to mass/neutral trim for future balance.
- Jobs currently require the full story load so acceptance has a clear
  pass/fail. Revisit partial-hold jobs and their effect on top speed, air, and
  routes.
- The survey lens is required for the mandatory Deep Relay expedition and reads
  the buried lower-right relay; the late expedition is part of the linear
  BEACON → DEEP RELAY → HOME route.

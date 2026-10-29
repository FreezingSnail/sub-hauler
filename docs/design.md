# Design — sub-hauler

Status: post-review design (2026-10-24, cargo-free resource pass). The
cargo-packing minigame was removed in `sub-hauler-gl1.1` because its value did
not justify its flash/SRAM cost. The final pass moved the surviving UI text and
fixed pilot art into FX and compacted the generated world metadata and AVR run
state. The browser slices in `spike-trim/` remain a historical reference; the
shipped device path is the cargo-free contract loop described below.

## Active exploration map — 2026-10-25

The device now loads the authored `levels/exploration.json` world, with
32 rooms, 42 passages, nine equipment gates and eight cove rewards. The high
pressure region is lowest. `make gen` appends its collision/chart/object
sections and writes the active `src/generated/level.hpp`; legacy seed-4242
sections and `src/generated/world.hpp` stay unchanged for ABI compatibility.
The active terrain uses winding, variable-width passages and asymmetric
caverns with 31 solid pillars, shelves, wall teeth and wreck ribs. Freight
allowances follow authored route lengths; optional shortcuts save travel time.
See [the level contract](../levels/README.md) for encoding, controls and limits.
The older baked-world measurements below are historical. Campaign/ending,
persistent cart voyage saves and gameplay music now ship in the `full` profile;
the [progression design](progression.md) records the accepted direction.

## Direction

- The cutaway is the identity of the game. The external sub is the travel view;
  the inside is where cargo, ballast, air, engine, and crew live.
- Cargo belongs inside the hull. The pilot uses the selected external hull and
  cargo is represented by coarse internal mass, trim, acceleration, and top
  speed. Accepting a contract auto-loads four units; the player starts travel
  with an explicit UNDOCK row action or B.
- The internal view should be used at stations and for damage/repair work, not
  as a second full-time flight view.
- Feel is slow, heavy, deliberate. The former packing puzzle is removed rather
  than gated: there is no loading screen, LOAD CARGO row, commit chord, or
  arrangement runtime on device.

## Sub classes

| Class | Hull | Hold (legacy FX) | Cargo units |
| --- | ---: | ---: | ---: |
| MINNOW | 64 px | 6×3 hex bay | 4 |
| HAULER | 94 px | 11×3 split/notched bay | 4 |
| LEVIATHAN | 118 px | 14×4 twin-spine bay | 4 |

The `FLEET` browser reference shows three round hulls, external and cutaway:
- MINNOW: TADPOLE rounded shuttle with bow viewport and landing skids.
- HAULER: FLATBED rounded freighter with deckhouse and cargo doors.
- LEVIATHAN: FORTRESS rounded heavy with outrigger pods and deckhouse/gantry.

The device roster keeps the three playable hulls and their pilot art. The
silhouettes are baked FX sprites with right/left and unloaded/loaded frames,
shared by title and flight; the host tests retain equivalent procedural shapes
for exact-pixel checks. The legacy hold dimensions remain only in the shipped
`cargoBank` FX declaration and generator so flash offsets stay stable; no
flight or station code reads that bank.

## Contract loading loop

1. Dock (auto-dock within 14 px of a settlement gantry at or below the dock
   speed; the hull stops in place instead of snapping to the gantry centre).
2. Select an offer and press `A`. Acceptance fills the hold with the contract's
   four coarse cargo units, neutralizes trim, and keeps the boat docked.
3. Confirm the active board's `UNDOCK` row with `A`, or press `B`, to start
   travel. Docking latch and flight pause are unchanged.
4. Deliver at the destination station for credits; late or wrong-station
   arrivals follow the existing economy rules. `ABANDON` clears the hold and
   contract while docked.

The active station board is now `[summary] ABANDON SHOP UNDOCK` (or
`[summary] ABANDON UNDOCK` without the shop). There is no `LOAD CARGO` row and
no way to reach a packing screen.

### Historical: packing prototype (removed)

The M5–M8 packing flow was implemented and then removed in `sub-hauler-gl1.1`:
selection, suspended movement, support, lift blocking, rotation, undo, the
`A+B` commit, hold-B exit timer, and per-class arrangement loading. The design
and measurements remain in `docs/roadmap.md` as history. Do not reintroduce or
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

## Device plan: retained FX arrangement ABI (not loaded at runtime)

The shipped `fxdata/fxdata.txt` declaration `raw_t cargoBank` and its 3,648 B
`fxdata/tables/cargo.bin` stay in place at offset **0x001AC2**, because
declaration order is flash ABI. `tools/gen_cargo.py` and `make gen-check` still
reproduce them. The generated `src/generated/cargo.hpp` constants remain
generator evidence only; no device header includes it after the runtime removal.

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
- Append FX declarations only; never reorder or reclaim shipped bytes.
- Generate tables with `make gen`; never hand-edit packed tables.
- Run `make gen-check` to prove reproducibility.
- Reading this bank again requires a new measured feature decision.

## Device text and fixed art

All device text uses the external 6x8 FX font; `src/text.hpp` owns
`FX::setFont`/`drawString`/`drawNumber` and switches to `dcmInvert` for selected
menu rows. Strings are deduplicated in `fxdata/fxdata.txt`. The firmware uses
`Arduboy2Base`, so the inherited `Print`/built-in-font path is not instantiated
and the old font symbols are absent from the ELF. The external font keeps the
same 6x8 cell and layout constants, so station/shop rows stay at 7 px pitch and
128x64 clipping is unchanged.

MINNOW/HAULER/LEVIATHAN are generated 26x8 plus-mask sheets: right/left facing
and unloaded/loaded. Title and pilot draw the same frames through
`Blit::draw`, preserving class pixels, mirroring, masking, and draw order.
`tools/gen_pilot.py` owns the shapes; `tools/gen_sprites.py` packs them into a
separate include appended after every shipped FX symbol. With `MULTI_HULL=0`,
only the HAULER sprite call is compiled.

Both additions are append-only. The shipped `cargoBank` stays at offset
**0x001AC2** with unchanged bytes; the FX payload in
`fxdata/fxdata-data.bin` is **13,012 B**, while the padded flashable
`fxdata/fxdata.bin` image is **13,056 B**.

## Baked world pipeline (M1)

`tools/gen_world.py` is the single offline source of cave truth. It is a
standard-library Python port of `generateCave` from `spike-trim/game.js` with
seed `4242`; generation does not invoke Node or a browser. The PRNG, chamber
list, tunnel wiggle, seal pass, and mine pull-back match the browser, so the
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
component (junctions and dead-end branches included), mines in open water
outside every settlement's 42 px dock clearance, and exact chart-mask
consistency. `tools/tests/test_gen_world.py` covers known bytes/bits, the
downsample rule, determinism, and corrupt-input rejection.

`world.hpp` metadata is split by target. Host tests get full `constexpr`
records including settlement names. AVR code never references those arrays:
`src/generated/world.hpp` also emits position-only settlement, mine, and current
records in `PROGMEM`, and `world::settlementInfo()`/`mineInfo()`/
`currentInfo()` read them with `pgm_read_byte`/`pgm_read_word`. This keeps both
costs off SRAM: an odr-used namespace-scope `constexpr` array is copied into
SRAM by the AVR startup code, and name pointers would drag their string
literals along with it. `Settlement::name` is therefore null on device. The
compact AVR metadata total is **88 B of program flash** (down from 148 B) with
no RAM or cart-read change. The route economy is generated as compact reward
and deadline tables; it no longer requires diagnostic counters or a job clock
in `game::State`.

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

## Arduboy fit

From `docs/platform.md`:

- 128×64 SSD1306, 1,024 B framebuffer.
- 2,560 B SRAM; **988 B** static free after the cargo-free resource pass.
- 29,696 B flash; current image **17,548 B flash / 1,572 B static RAM**
  (12,148 B flash free, 11,452 B under the 29,000 B planning ceiling, 128 B
  static headroom; `test_memory` reserve **812 B** against a 600 B target).
- AVR `game::State` is **80 B**; the compact generated metadata is 88 B
  PROGMEM; the 128 B cave atlas stays in fast program flash.
- No floats, no heap in device code, no cart reads during display.

The retained `cargoBank` lives in external FX flash, so it does not consume
program flash or static SRAM. It is shipped for ABI stability only; the
cargo-free station/job/undock path is the device path. Measure every future
change with `make size` and `make size-matrix`; do not assume the remaining
headroom survives another feature.

## Browser reference (historical)

- `spike-trim/index.html` — PILOT and FLEET views plus the removed station
  loading screen.
- `spike-trim/subs.js` — shared MINNOW / HAULER / LEVIATHAN roster and selection.
- `PILOT` — external sim, stations, contracts, shop. Draws the selected round
  hull; cargo is internal mass, not a stack above the hull.
- Loading screen — historical packing prototype; not shipped on device.
- `FLEET` — external and cutaway profiles for the same selected hull.

The browser cargo generator is reference-only; the retained FX arrangement
bank is an ABI artifact, not a runtime plan.

## Open questions

- How exactly coarse cargo units map to mass/neutral trim for future balance.
- Whether bonus crates ever affect payout, capacity, or only fill.
- Jobs currently require a full four-unit hold so acceptance has a clear
  pass/fail. Revisit partial-hold jobs and their effect on top speed, air, and
  deadlines.
- Whether any save data should persist delivery or roster state.

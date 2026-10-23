# Design — sub-hauler

Status: working design. The browser slices in `spike-trim/` are one flow:
pilot travel, station loading, and fleet reference. The precomputed cargo-table
plan below is the intended device path.

## Direction

- The cutaway is the identity of the game. The external sub is the travel view;
  the inside is where cargo, ballast, air, engine, and crew live.
- Cargo belongs inside the hull. The pilot now uses the selected round external
  hull and no longer stacks crates above it; cargo is represented by internal
  mass, trim, acceleration, and top speed. The station loading screen's LOAD &
  UNDOCK action feeds the pilot's coarse four-unit load model from the hold.
- The internal view should be used at stations and for damage/repair work, not
  as a second full-time flight view.
- Feel is slow, heavy, deliberate. Packing is a planning puzzle, not a twitch
  minigame.

## Sub classes

| Class | Hull | Hold | Cargo load | Pieces |
| --- | ---: | ---: | ---: | ---: |
| MINNOW | 64 px | 6×3 hex bay | 12/14 cells | 4 (3 required + 1 bonus) |
| HAULER | 94 px | 11×3 split/notched bay | 26/28 cells | 7 (6 required + 1 bonus) |
| LEVIATHAN | 118 px | 14×4 twin-spine bay | 40/44 cells | 8 (6 required + 2 bonus) |

The `FLEET` view shows three round hulls, external and cutaway:
- MINNOW: TADPOLE rounded shuttle with bow viewport and landing skids.
- HAULER: FLATBED rounded freighter with deckhouse and cargo doors.
- LEVIATHAN: FORTRESS rounded heavy with outrigger pods and deckhouse/gantry.

Hold masks are fixed per class; only the arrangement and random cargo shapes
vary. Hull studies do not change the hold capacity. PILOT draws playable-scale
silhouettes inside the tunnel envelope; FLEET shows the full hull proportions.

## Cargo packing loop

- Dock, accept a contract, and load the selected hull in the station loading
  screen. The screen opens automatically from the accepted job.
- Device D-pad: with nothing held it cycles the selection across tray and
  installed cargo; while holding a crate it moves the suspended crate, clamped
  to the open hold.
- `A` picks up or lifts the selected crate, and releases the held crate.
- `B` rotates a held crate clockwise (`B`+UP is counter-clockwise); with
  nothing held it undoes the last placement. Holding `B` exits to the station
  board so a manipulation tap never leaves the screen by accident.
- A crate can only be released when at least one bottom cell rests on cargo or
  the floor. No mid-air placement. Structural mask cells are solid; on HAULER
  cargo cannot bridge or occupy the central split bulkhead.
- Crates can be moved sideways/up/down while suspended, so they can be tucked
  under overhangs and into corners.
- Installed crates stay in the tray. Select an installed piece and press `A`
  to lift it; cargo directly above blocks the lift.
- The Arduino pad has no spare key, so device `R`/`N` are omitted; the pure
  `cargo_screen::restart` helper exists for a future pause menu. The browser
  slice keeps `R` reset and `N` reroll.
- `A+B` while nothing is held is `LOAD & UNDOCK`. The prompt appears only when
  the required pieces are placed and no crate is held; a short load is rejected
  with `LOAD SHORT`. `A+B` cannot collide with pick/release (`A`), rotate/undo
  (`B`), or the `B`+UP counter-clockwise modifier.
- A successful commit checks the job crate requirement, sets the pilot load to
  the contract's four units, neutralizes trim, clears the loading state,
  auto-undocks, and returns to the pilot with the job still active. Holding `B`
  (cancel/back) returns to the station board with the job open, selected on
  `LOAD CARGO`; the board row reopens the screen.
- With `SH_FEATURE_CARGO_LOAD=0` the loading screen, its strings, and the
  handoff compile out. Acceptance then auto-loads the four units directly and
  the M3 board shows summary, ABANDON, and UNDOCK.
- The hold is schematic 128×64, 1-bit, and shown as one compartment inside the
  pressure hull. Cells scale per class (7 px MINNOW, 6 px HAULER, 5 px
  LEVIATHAN) so the bow cabin, engine room, and ballast remain visible without
  scrolling. The FX arrangement data is independent of this view scale.

The browser views share one hull roster and selection (`spike-trim/subs.js`).
The cargo view is the station loading step; the device `A+B` commit checks the
contract requirement and maps the packed hold to the pilot's four-unit cargo
model. The FX arrangement bank remains the device path.

## Random shape generation

Browser-only generator, used to produce candidate loads:

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
arrays, and dynamic allocation. It is not portable as-is. Do not port the
runtime generator to AVR unless it is rewritten with fixed arrays.

## Device plan: precomputed FX arrangements

Recommended Arduboy path: precompute arrangements offline and store them in the
FX cart. At runtime, select one arrangement and copy it into SRAM.

### Piece record

```c
struct CargoPieceRecord {
    uint16_t mask;   // 4x4 normalized shape, bit set = filled cell
    uint8_t  posKind;// x: 0-3, y: 4-5, bonus: 6, spare: 7
};
```

- 3 bytes per piece.
- Shape area and bbox derive from `mask`.
- Target orientation is in the mask; player rotation is runtime bit logic.
- Required pieces can be first in the record, so the bonus bit is optional.
- Target position is the known solution placement.

### Arrangement sizes

| Hull | Per arrangement | 32 | 64 | 128 |
| --- | ---: | ---: | ---: | ---: |
| MINNOW | 12 B | 384 B | 768 B | 1,536 B |
| HAULER | 21 B | 672 B | 1,344 B | 2,688 B |
| LEVIATHAN | 24 B | 768 B | 1,536 B | 3,072 B |
| **Total** | | **1.8 KB** | **3.6 KB** | **7.3 KB** |

Use 64 arrangements per hull initially. Add a table header with format version
and count, or carry fixed counts/offsets in `src/fxdata.h`.

### Runtime

- Pick an arrangement index with a small seeded PRNG or a shuffle bag.
- Use `FX::seekData(tableBase + index * recordBytes)`.
- Read 12–24 bytes with `FX::readPendingUInt8`.
- Call `FX::readEnd`.
- Copy piece masks/positions into a fixed `CargoPiece[8]` array.
- Keep only the selected arrangement and puzzle state in SRAM.
- Read cart data in update/render, never during the OLED transaction.
- No runtime generator, retries, owner grid, cluster metadata, or heap.

Suggested SRAM state: under ~100 B for the selected arrangement plus the cargo
screen. This avoids the roughly 0.5–0.7 KB generator working set.

### Generation and ABI

- Add generated raw tables to `fxdata/fxdata.txt` with `raw_t` entries.
- Append tables only; declaration order is flash ABI.
- Generate the `.bin` tables with a tool called from `make gen`; never hand-edit
  packed tables.
- Put counts/offsets in the generated `src/fxdata.h`.
- Run `make gen-check` to prove reproducibility.
- Offline validation must reject:
  - disconnected shapes;
  - overlap/out-of-bounds target positions;
  - cells over the max 4×4/8-cell shape;
  - wrong load or required/bonus counts;
  - target arrangements without support/stability.
- Generate a larger candidate pool than needed, then keep visually distinct
  arrangements.

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
`src/generated/world.hpp` also emits position-only `SETTLEMENT_META` and
`MINE_META` records in `PROGMEM`, and `world::settlementInfo()`/`mineInfo()`
read them with `pgm_read_byte`/`pgm_read_word`. This keeps both costs off SRAM:
an odr-used namespace-scope `constexpr` array is copied into SRAM by the AVR
startup code, and name pointers would drag their string literals along with it.
`Settlement::name` is therefore null on device. The metadata copy is 36 B of
program flash for four settlements; `MINE_META` is only linked when a device
test uses it.

### Cave view and visible window (M1.4)

`src/cave.hpp` holds the window math and tile atlas, `src/cave_render.hpp`
composes a frame, and `src/render.hpp` is the device entry. The view is native
1-bit and integer-only: no float, anti-aliasing, greyscale, or scrolling.

- The camera is a tile-aligned 16×8 window. `cave::cameraCenteredAtPixel()`
  centres it and clamps the top-left tile to `[0, 304] × [0, 152]`, so the
  window always covers exactly 128×64 pixels inside the baked map, including
  at the origin and both far edges.
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
  deterministic dark pixel per tile as texture.
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
- 2,560 B SRAM; 1,346 B static free at the last checkpoint.
- 29,696 B flash; current image 6,636 B flash / 1,214 B static RAM.
- No floats, no heap in device code, no cart reads during display.

The arrangement tables live in external FX flash, so they do not consume
program flash or static SRAM. The remaining work is the in-device cargo screen,
input, and state. Measure with `make size` after adding it; do not assume the
current headroom survives the UI.

## Browser reference

- `spike-trim/index.html` — PILOT and FLEET views, plus the station loading screen.
- `spike-trim/subs.js` — shared MINNOW / HAULER / LEVIATHAN roster and selection.
- `PILOT` — external sim, stations, contracts, shop. Draws the selected round
  hull; cargo is internal mass, not a stack above the hull.
- Loading screen — opens from the station when a contract is accepted; playable
  internal hold with crane, random shapes, support, lifting, and LOAD & UNDOCK.
- `FLEET` — external and cutaway profiles for the same selected hull.

The browser cargo generator is for experimentation; the FX arrangement bank is
the device plan.

## Open questions

- How many arrangements per hull before the loads feel repetitive? Start 64.
- Add horizontal mirror/rotation of whole arrangements for variety?
- Any-bottom support vs full-bottom support for release?
- How exactly cargo cell area maps to mass/neutral trim once cargo is internal.
- Do bonus crates affect payout, capacity, or only fill?
- Jobs currently require a full four-unit hold so the loading screen has a
  clear pass/fail. Revisit partial-hold jobs and how their crate count should
  size the required packed region.
- Whether arrangement index persists in save data or is rerolled per dock.

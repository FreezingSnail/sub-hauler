# Authored exploration level

`exploration.json` is the editable source for the active game map. It contains
32 named rooms, 42 passages, four station IDs, six equipment gates, three tide
sections and four equipment coves (cutter, seal, thruster, survey lens).
Coordinates/radii are in
8-pixel tiles. `terrain` selects the
deterministic organic carve; room `size`/`lobes` shape asymmetric chambers and
route `via` points describe winding, variable-width passages. Room `obstacles`
and route `obstacles` add solid shelves, teeth, ribs and rock islands; route
`caverns` provide room to go around them. The active map has 31 physical rock
features, occupying 509 tiles that would otherwise be water.
`requires` lists capability tools. Explicit gate positions/axes
allow an entrance to be sealed without blocking a neighboring route.
`music_region` optionally overrides the room's travel cue without changing its
scenery theme; the generator also merges gate-cut cue fragments under 256 open
tiles into the neighbouring region.

Station rooms also supply a `dock: [tile_x, tile_y]` separate from the room's
navigation centre. It places the real berth on a cave-wall ledge. Generation
requires a clear 26x8 hull and a 48-pixel upward escape column, a nearby rock
foundation, no obstacles in the approach, and reachability under every applicable
equipment combination. `DOCK_*` symbols and station metadata drive auto-docking,
chart markers, hints and spawn; `ROOM_*` still describes the route graph.
The home boat starts centred at the Tidewall berth. The generator emits no route
reward, deadline or freight-allowance table: delivery work is untimed and
unpaid.

Run `make gen` after editing. It retains the old seed-4242 sections and appends
the new level to `fxdata/fxdata.txt`, preserving every earlier FX address.
`fxdata/fxdata.bin` remains the only flashable image.

| Generated section | Format | Size |
| --- | --- | ---: |
| `explorationTiles` | 320x160 collision tiles, one bit per tile, 1 = solid | 6,400 B |
| `explorationChart` | 64x32 chart cells, one bit per cell, 1 = water | 256 B |
| `explorationObjects` | `SHL2` header + fixed gate/pickup records | 91 B |
| `explorationTerrain` | native 2560x1280 terrain image, page-major, 1 = lit | 409,600 B |

Both planes are row-major, with the leftmost tile/cell in the least significant
bit of its byte. The collision plane has 40 bytes per row; the chart has eight.
Each chart cell covers 5x5 collision tiles and is water if any tile is open.

The terrain image reconstructs a continuous bicubic field from the same
collision grid, then thresholds it to native 1-bit pixels. `tools/gen_contours.py`
rounds the tile stair steps into smooth slopes and rock noses; this is an
offline operation, with no grayscale or smoothing on the device. All tile
centres keep their solid/water identity and the visual boundary stays within
three pixels of the collision silhouette in both directions. Interior speckles
and regional scenery are added afterward. Byte offset `(pixel_y / 8) * 2560 + pixel_x` holds eight vertical
pixels, least significant bit at the top. A page-aligned Y origin reads eight
128-byte strips straight into the existing screen buffer. A sub-page Y origin
reads the next source page and shifts adjacent page rows together; the device
does this in place with the next framebuffer page as lookahead and a 32-byte
tail scratch, never a second whole frame. Stations, gates, finds, currents, the
boat and HUD draw from the same pixel origin. The 6,400 B collision plane still
supplies physics; visual pixels never substitute for collision rules.
This avoids per-frame terrain drawing with no extra SRAM. The checked tile
renderer remains a legacy reference for collision-tile art. The streamed
image also contains the offline scenery described below; crop tests preserve
its native page/bit orientation across the entire world.

`scenery` in `exploration.json` owns the decorative art version, deterministic
seed and named landmark preferences (`room`, `kind`, pixel `offset`). Landmarks
default to `mount: "water"` and sit on the curved cave floor; `mount: "wall"`
etches the same hollow line art into the surrounding rock instead. The four
equipment coves are small dead-end pockets whose water is protected route
corridor, so each carries its own wall-etched wreck silhouette:
`cutter-tender` at the Abandoned tender (`W`), `thruster-pod` at the Old relay
compartment (`BR`), `workshop-lock` at the Sealed workshop (`CW`) and
`trench-lens` at the Hidden trench cove (`DL`). A room may also name a
`texture`; the override is scoped to that room's own pocket and swaps its
ambient growths and wall markings without re-theming a neighbour or its region.
The Hidden trench cove uses `texture: "lens"`: `lens-shard` wall growths plus
concentric lens rings and facet arcs mark the lens cave.
The wider Sealed workshop pickup pocket uses `texture: "pressure"`, with
buckled ribs, rivets and layered bulkhead outlines marking the seal cave.
`tools/gen_scenery.py` adds region-specific rock markings and hollow background
motifs during `make gen`: strata/coral/moorings, kelp/pumps, weathered plating and
wreck machinery, and fractured basalt/crystals/vents. Scenery does not alter
collision tiles, chart cells, gates or pickups. Artwork stays outside a
30-pixel route-centre corridor, station/spawn footprints and gate/pickup markers;
dark wall markings stay three pixels inside solid rock. Larger motifs search
near their authored room for a safe floor location, failing generation if no
placement fits. Wall-mounted motifs search the room's wall for a fully
rock-clipped location, prioritizing the portion visible in a 128×64 viewport
centered on the pickup, and fail the same way. Roots follow the actual curved
floor; textures are clipped inside its silhouette. `docs/assets/exploration-scenery.json` is a generated inspection
report, not another flashable image. It records each landmark's `mount` and
position. All decoration is baked into the same
409,600-byte native section, with no runtime decoration arrays or draw calls.
`tools/gen_settlements.py` then bakes piers, airlocks and pressure-room cutaways
into the rock beside the four berths. Each settlement has four inhabited rooms,
with homes, grow rooms, workshops or armored capsules matching its region.
Rooms fit entirely in solid rock and leave the boat's approach pixels clear;
they are background cutaways, not additional boat passages. Thin service
shafts connect them to the pier. Their positions are in the same generated
inspection report, and only the existing native image is packed into FX.
Works plating uses deterministic position/shape variation, partial seams,
corrosion chips and sparse rivet groups. Seam direction follows the local
wall-distance gradient; it does not repeat closed panels or central handles.

The object stream begins with eight bytes: ASCII `SHL2`, gate count, pickup
count, gate stride (7) and pickup stride (5). Nine gate records follow, then
four pickup records. Multibyte values are **little-endian**:

- Gate: `u16 x, u16 y, u8 width, u8 height, u8 required_tools`.
- Pickup: `u16 x, u16 y, u8 kind`.

Object coordinates are world pixels. Gates use the rectangle's top-left;
pickups use their center. Required tool bits are cutter=1, pressure seal=2,
thruster=4. Combined requirements need every bit. Pickup kinds are cutter=1,
seal=2, thruster=3 and survey lens=6; the lens equips bit8. There is no credit
kind and no amount: a pickup is one piece of equipment. Room, gate and pickup
IDs in `src/generated/exploration.hpp` are symbolic constants; keep source
ordering stable because saved voyages index them.

`src/generated/level.hpp` provides the active station/current/route metadata.
The older `src/generated/world.hpp` is retained with the legacy sections; do
not include both world headers in one translation unit. `world.hpp` selects
the active header and `world::FxTables` reads the new FX symbols.

The object stream has a byte-identical, 91-byte PROGMEM mirror for collision
and rendering. It occupies program flash, not SRAM, and avoids cart reads in
the inner gate-collision loop. The tile cache stays 23 bytes; no complete map
or object array is copied into SRAM. Device code uses integer math only.

## Playable behavior

- Current zones cover the common navigable cross-section of their flow strip
  and overlap rock on each edge. The intersection rule deliberately leaves
  side coves and dead-end pockets out of the tide, so a current cannot pin the
  boat in a pocket. Every tide is a thruster gate: without the current
  thruster it blocks upstream travel while leaving the downstream escape open;
  owning the thruster disables the tide completely. The authored Cormorant
  passage is a one-way upward tide, so its downstream return stays open.
  Remaining room-centre currents are thruster-gated tide walls.
  Tile-count metadata preserves extents above 255 pixels.
- The cutter is recoverable from the western cove near Tidewall. Hold the boat
  slow and close to a find for half a second to recover it. Bottom-line hints
  identify equipment; station rumors guide required discoveries.
- Cutter grates need a half-second slow operation nearby after finding the
  cutter. Open grates remain open for the run. Pressure/current passages pass
  only with their required equipment; contact blocks movement without damage. Flow marks distinguish current boundaries from grates and bulkheads.
- The pressure seal and current thruster can be found in either order after
  the cutter. Every entry into the lowest region needs the seal; the return
  shaft additionally needs the thruster. Deep Anchor is the lowest settlement.
- Four equipment coves hold the cutter, pressure seal, current thruster and
  survey lens. Each cove carries a distinct wall-etched landmark (cutting
  tender, relay pod, workshop hatch, lens array); the Hidden trench cove also
  uses the scoped `lens` wall texture. The survey lens is required for the mandatory lower-right DEEP
  RELAY expedition, where it reads the buried relay. Gameplay sonar was retired
  on 2026-10-28, so the lens does not change any reveal radius. There are no
  credit caches or purses.
- The chart remembers encountered gates and shows finds only in explored
  cells. Claimed finds and passable gates use smaller markers.
- Story work is untimed and unpaid. Contracts do not offer Deep Anchor before
  the pressure seal is owned. Intermediate berth stops refill the fixed 300-unit
  air tank while retaining the active job and cargo.

The fixed build ships campaign restoration missions, the mandatory DEEP RELAY
expedition, homecoming and a persistent schema 7 voyage save. Continue and rescue restore
the last committed berth, including saved tools, claims and opened grates;
unsaved expedition gains roll back. A New Voyage resets exploration.
See [progression.md](../docs/progression.md). `layout_version` is
now 3, so the optional standalone chart save rejects the preceding terrain's
chart. The JSON schema version and SHL2 object encoding remain version 2.

Exploration is always enabled in the fixed build; there are no feature
profiles or shipping developer modes. The developer-only `make debug` image
adds no-clip map inspection without changing the shipping build.

## Validation and preview

`make gen` checks the real tile geometry using the 26x8 hull, for all eight
cutter/seal/thruster combinations, against the authored route graph. It rejects
accidental gate bypasses, disconnected rooms and tools trapped behind their
own prerequisites. A few cove positions/entrance elbows differ from the design
schematic to maintain collision clearance at native tile resolution.

[Actual generated terrain](../docs/assets/exploration-terrain.svg) shows the
encoded grid, gate rectangles and equipment cove positions. The larger
[design schematic](../docs/assets/proposed-exploration-map.svg) records the
2026-10-25 route-network design; its three credit coves and capacity pod were
removed on 2026-11-01 and the four equipment coves remain. The generated
terrain is authoritative for exact collision geometry.

Permanent tooling tests check the packed stream, legacy offsets, physical
reachability, deterministic curved passages, rock footprints and corrupt source
rejection. Native C++ tests additionally check actual gate and rock-pillar
collision with cached tiles, movement, dwell and one-time equipment claims. Device suites read the FX objects against their PROGMEM
mirror and exercise salvage, gate operation and rendering in Ardens.

## PNG world export

After `make gen`, run `make render-world` to export the packed world using the
actual host versions of the cave, station, gate, pickup, current and pilot
renderers, including the streamed native terrain. It writes `build/world-render/game-world-native.png` (2560x1280,
1-bit scene pixels without HUD) and `build/world-render/game-world.png`
(2560x1530, labeled overview) and `build/world-render/game-world-music.png`
(2560x1630, travel cue regions highlighted), then refreshes the guide copies at
`docs/assets/exploration-world-native-2026-11-09.png`,
`docs/assets/exploration-world-2026-11-09.png` and
`docs/assets/exploration-world-music-2026-11-09.png`. All finds/gates are in new-run state and
all areas are visible. This preview command reads existing generated data and
does not change game assets or the FX layout.
`game-world-regions.png` shows eight 128x64 native crops enlarged 4× without
smoothing, including the buoys, pump, freight crane, wreck and trench crystals.
`game-world-settlements.png` shows the four wall berths and inhabited cutaways
in 128x96 native area crops enlarged 4×.

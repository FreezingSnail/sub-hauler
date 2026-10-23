# Sub-hauler — trim sandbox (v2)

Design context: [`../docs/design.md`](../docs/design.md).

Browser feel test for the **submarine** direction: free 2D piloting, ballast
tanks, slow deliberate movement, cargo that changes trim, four settlements,
and delivery contracts. This replaces the v1 auto-forward cave flyer as the
design probe; v1 is kept in `spike/` for comparison.

Open `spike-trim/index.html` directly. No server, build step, or dependencies.
The game loop is one flow: dock, accept a contract, pack the hold in the
loading screen, undock, and deliver. **PILOT** is the travel view; the loading
screen opens automatically from the station; **FLEET** is the reference view.
Hull selection and the selected ship profile are shared. `subs.js` is the
single hull roster.

Optional query parameters:

```
spike-trim/index.html?seed=2027
spike-trim/index.html?seed=2027&crates=4
spike-trim/index.html?seed=2027&map=all
spike-trim/index.html?seed=2027&credits=500
```

`crates` is a debug starting load. `map=all` opens the whole cave on the chart.
`credits` starts with shop credit for testing the shop mock.

Loop: fly to a settlement → `E` dock → select a contract → `Z` accept → pack
the hold → `Enter` load & undock → dock at the destination.

## Controls

| Input | Action |
| --- | --- |
| `←` / `→` or `A` / `D` | propeller thrust / reverse |
| `↑` / `↓` or `W` / `S` | blow ballast (rise) / flood ballast (sink) |
| `1` / `2` / `3` | select MINNOW / HAULER / LEVIATHAN; shared with cargo/fleet |
| `E` | dock / undock at a settlement |
| `↑` / `↓` | select a station or shop row |
| `Z` | A: select / accept / buy in menus |
| `X` | B: back / undock in menus |
| `Space` | sonar ping while piloting (costs air) |
| `M` | chart (pauses the sim) |
| `C` | toggle a horizontal current |
| `R` | reset the same seed |
| `N` | new random cave |

Release the ballast keys to close the valves and hold the current fill.
There is no auto-forward. `Z` / `X` are Ardens' `A` / `B` buttons.

## Trim model

- Ballast fill is `0–100%`. More water = heavier = sink.
- Neutral fill is `50 − 5 × crates` (50% empty, 30% with four crates).
- Cargo changes the neutral point. Loading makes the sub heavy until the
  player blows tanks down to the new notch. The loading screen's LOAD &
  UNDOCK handoff feeds the coarse four-unit pilot cargo model from the hold.
- `↑` blows at 18%/s and costs air. `↓` floods at 18%/s for free.
- Vertical drive is the fill-to-neutral gap, clamped. Release at neutral and
  the sub settles to a hover; release off-neutral and it keeps rising or
  sinking until corrected.
- The left gauge shows fill. The tick to its right is neutral for the current
  cargo.

## Air

- 180 units. Time drains 0.6/s, sonar costs 5, blowing costs 3/s.
- At zero, blowing stops. Flooding and thrust still work, but the sub cannot
  rise.
- Docking refills air to the current maximum and repairs hull and cargo
  integrity. O2 tanks raise the maximum.

## Cargo

- Contracts load 2 or 4 crates when accepted at a settlement.
- Each crate adds 2 px of collision height and increases mass.
- Thrust acceleration drops 3 px/s² per crate; top speed falls from ~28 px/s
  to ~18 px/s.
- Loading changes the neutral notch. The dock re-trims to neutral before you
  leave, but the notch is lower with a full load.
- If the taller hull would clip rock, the sub is nudged down and takes one
  hull damage. If there is no room at all, the cargo change is reverted.
  The debug `setCrates` API still applies a live load for testing; there is no
  cargo key in normal play.

## Cave

- 2,560 × 1,280 px world, 8 px tiles, 128 × 64 view.
- Twelve chambers: four settlement chambers, four junctions, and four side
  chambers. The settlements form a ring with a central shortcut and three
  dead-end branches.
- Main tunnels are 58 px wide; side tunnels are 54 px. The loaded hull fits on
  the centre lines.
- Full 2D collision; the sub is an axis-aligned box resolved per axis.
- Camera follows x/y with spring lag and a small velocity look-ahead.
- Mines sit near chamber walls. Faint 1 px dots at range; sonar promotes them
  to bright crosses for five seconds.
- Silt drifts with the current; a fading wash trail marks the route.

## Settlements and contracts

- Four settlements: **TIDEWALL**, **BENTHOS**, **CORMORANT**, and
  **DEEP ANCHOR**. TIDEWALL is the start.
- Each is a dashed docking gantry. Stop inside it, slow to 16 px/s or less,
  and press `E`.
- Docking snaps the sub to the pad, refills air, repairs hull and integrity,
  and re-trims to neutral.
- Every station posts two jobs: one 2-crate and one 4-crate run to another
  settlement. Reward and time limit scale with straight-line distance.
- Docking opens a device-shaped station board directly in the 128 × 64 canvas:
  station name, credits, air/hull, job rows, `SHOP`, and `UNDOCK`. No HTML.
- `↑` / `↓` selects a row. `Z` (A) accepts a job or activates `SHOP` /
  `UNDOCK`; `X` (B) leaves the station immediately.
- Job rows read like `2CR>CORM $160 230S`: 2 crates to CORMORANT, $160,
  230-second limit. Codes are TIDE, BENT, CORM, DEEP.
- Accepting loads the cargo and switches the board to contract mode:
  `SHOP`, `ABANDON`, `UNDOCK`. The timer starts when you leave.
- Dock at the destination to deliver. The board shows `DELIVERED +$N`,
  credits increase, cargo unloads, and fresh offers appear.
- An active job is shown in the bottom hint and side panel.
- Deadlines use simulated travel time. Time is paused while docked or while
  the chart is open, so those pauses do not burn the clock yet.
- The chart draws all known settlements as crosses; the active destination
  blinks. Unexplored cave water still stays black.

## Shop (device mock)

- Select `SHOP` on the station board. The shop replaces the board with another
  screen drawn directly into the 128 × 64 canvas.
- Station board and shop share the same hardware shape: native 1-bit, integer
  coordinates, 5 × 7 bitmap font, no HTML, anti-aliasing, scrolling, or float.
- Controls match the device: `↑` / `↓` select, `A` buys, `B` backs out.
  In the browser `A` is `Z`, and `B` is `X` (Escape also backs out).
- Items:
  - `O2 TANK` — +60 max air, $120, two levels.
  - `HULL PLATE` — +1 max hull, $150, two levels.
  - `SONAR` — ping costs 1 less air, chart reveal +30 px, $100, one level.
- The selected row is an inverted bar. Credits sit top-right. The bottom line
  shows the current effect or `BOUGHT ...`, `NOT ENOUGH CREDITS`, `SOLD OUT`.
- Use `?credits=500` to preview purchases without running contracts first.
- Both station and shop are in-canvas; the station DOM overlay is gone.

## Cargo loading flow

- The loading screen opens when you accept a contract at a station. The
  current hull is shared with PILOT and FLEET; the pilot draws the same round
  external profile.
- Select the hull with the MINNOW / HAULER / LEVIATHAN buttons or `1` / `2` /
  `3`. Selection carries across all views. `R` resets the current hold;
  switching hull also resets it.
- `LOAD & UNDOCK` checks the contract's required crates, maps the packed hold
  fraction onto the pilot cargo/trim model, and starts travel. Full hold loads
  map to four units. `CANCEL` or `Esc` returns to the station.
  - MINNOW — 6×3 hex hold, 3 required + 1 bonus, 12/14 cells.
  - HAULER — 11×3 split/notched hold, 6 required + 1 bonus, 26/28 cells.
  - LEVIATHAN — 14×4 twin-spine hold, 6 required + 2 bonus, 40/44 cells.
- The cutaway scales the hold inside the pressure hull: 7 px cells on MINNOW,
  6 px on HAULER, 5 px on LEVIATHAN. The bow cabin, engine room, ballast tanks,
  and hatch remain visible; the hold never replaces the whole interior. No
  scrolling: the entire hold stays on screen.
- Blocked mask cells are structural and cannot be occupied: corner notches on
  MINNOW and LEVIATHAN, and the central bulkhead on HAULER. The split-hull
  bulkhead is solid; cargo cannot span it.
- Cargo pieces are random polyominoes generated for each load, with connected
  shapes up to 4×4 and 8 cells. Rotation changes their footprint.
- `N` rolls a new random cargo set for the current hull; switching hull also
  generates a fresh load. `R` resets only the current placement.
- Crates are lowered manually with the crane. You can slide them sideways or
  raise/lower them, including under overhangs and into corners.
- A crate can only be released when at least one bottom cell rests on cargo or
  the floor. No floating or mid-air placement.
- Installed crates remain in the tray with hollow icons. Select one and press
  `Z` (A) to lift it; cargo above blocks the lift, so remove the top layer first.
- The D-pad moves the suspended crate. `Z` (A) picks up/releases; `X` (B)
  rotates a suspended crate or undoes the last placement; `R` resets the hold.
- `Enter` / `L` loads the hold and undocks; `Esc` cancels back to the station.
- The hold screen is 128 × 64, native 1-bit, with the same 5 × 7 bitmap font.
- The contract clock and pilot air pause while the loading screen is open.
- After loading, the pilot undocks automatically. At the destination, dock to
  deliver; the cargo clears and the station board returns.

## Fleet mockups

- `FLEET` tab: three round hulls, external and cutaway.
  - MINNOW TADPOLE — 64 px, 6×3 hex hold, 12/14.
  - HAULER FLATBED — 94 px, 11×3 split/notched hold, 26/28.
  - LEVIATHAN FORTRESS — 118 px, 14×4 twin-spine hold, 40/44.
- `←` / `→` cycles hulls; `Z` (A) toggles external/cutaway.
- External view has sail, twin antenna masts, deck hatches, side cargo doors,
  portholes, ballast vents, dive planes, rudder, and propeller. Cutaway shows crew, cargo hold, air
  flasks, ballast tanks, bulkheads, engine, and piping.
- Cargo blocks fill upward from the keel; empty cells are dots.
- The screen is 128 × 64, 1-bit, with schematic 3 px hold cells.
- The selected hull is shared with PILOT and the loading screen; these are
  profile views, not separate playable variants.
- PILOT uses playable-scale silhouettes inside the tunnel envelope; FLEET shows
  the full 64 / 94 / 118 px proportions.

## Chart

- `M` opens a paused 64 × 32 chart. Each cell is 40 × 40 px of world.
- Only explored water is drawn. Unexplored space stays black.
- Moving reveals a 56 px radius. A sonar ping reveals a 120 px radius, plus
  30 px per sonar upgrade, so pings are useful for mapping, not just mines.
- Settlements are always marked with crosses. The blinking block is the sub;
  the active contract destination blinks.
- The chart persists for the current seed: `R` keeps it, `N` starts a new one.
  It is also saved to `localStorage` per seed, where the browser allows it.
- Debug: `G` reveals the whole cave on the chart (explored cells solid,
  unexplored water faint), without changing exploration progress.
  `?map=all` starts with the full chart open.
- AVR budget: 64 × 32 bits = 256 B.

## Scope

In: expanded 2D cave, free thrust, ballast trim, job cargo, air, current,
mines, sonar, particles, hull damage, camera lag, fog-of-war chart, four
settlements, docking, delivery contracts, credits, device-shaped shop and
upgrades.

Out: broader economy, finish, vertical shafts, audio, save data beyond the
chart, Arduboy implementation.

## Porting note

The JS build holds the full tile grid. The expanded world is 320 × 160
tiles; packed at one bit per tile that is 6.4 KB, still too much SRAM for the
ATmega32u4. Keep the chamber/tunnel descriptors and rasterize a local tile
window around the camera, or generate tiles deterministically from the
descriptors. The 64 × 32 chart is 256 B. Settlement/job state is tiny. The
station board and shop already use the device display budget: 128 × 64,
native 1-bit, 5 × 7 glyphs, about 5 bytes per glyph, roughly 210 B of font
data in PROGMEM. The remaining browser overlays are the transient bottom hint
and the hull-breach card; those can move in-canvas on device.

## Tests

```sh
node --test spike-trim/*.test.js
```

`game.test.js` covers cave determinism, connectivity, loaded-hull clearance
along tunnels, spawn clearance, neutral hover, cargo sink, blow/flood air
costs, air-zero lockout, sonar cost/cooldown, current drift, cargo clipping
nudge and revert, camera clamps, mine placement, and chart reveal growth.

`main.test.js` boots the real browser scripts against a minimal DOM/canvas
stub, renders a frame, checks a live cargo change leaves the hull clear of
rock, and exercises the chart render.

Docking, contracts, delivery, and the shop are exercised manually in the
browser for this spike iteration; no new spike tests were added.

## Tuning

Everything lives in `CFG` and `neutralFor()` in `game.js`:

- `ballastRate`, `ballastDrive`, `ballastK`, `vertDrag` — depth control.
- `thrustAccel`, `thrustDrag`, `reverseFactor`, `maxVx` — horizontal feel.
- `cameraTauX/Y`, `cameraScreenX/Y` — camera lag and framing.
- `airDrain`, `airBlow`, `airSonar` — pressure.
- `dockRadius`, `dockSpeed` — how close and how slow docking is.
- `jobOffers` and the reward/deadline formulas in `makeOffer()` — contract
  length and payout.
- `SHOP_ITEMS` prices, limits, and effects — shop balance.
- Chamber/tunnel layout in `generateCave()` — cave size and route shape.

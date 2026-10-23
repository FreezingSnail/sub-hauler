# Roadmap — Arduboy device port

Status: planning. `spike-trim/` is the behavioural reference; the firmware is
still the walking/jumping template. This document sequences the port so each
milestone leaves a playable or testable device build.

## Goal

Stand up the browser game loop on the Arduboy FX:

> dock → accept contract → load hold → undock → travel → deliver → repeat

The full game is the target, but every feature must be removable at compile
time so flash or SRAM pressure can be answered by carving scope rather than
rewriting the game.

## Non-goals

- Port the `FLEET` reference view.
- Port the browser random cave/cargo generators to AVR.
- Hold the 320×160 tile grid in SRAM.
- Use `float`, `double`, heap allocation, `String`, or dynamic containers.
- Exact visual parity with the browser. The device is a separate 1-bit
  composition at 128×64.
- Save data in the first playable build. Chart save is optional and late.

## Current baseline

Measured 2026-10-22 with the existing template:

| Resource | Current | Planning ceiling | Hardware limit |
| --- | ---: | ---: | ---: |
| Flash | 6,636 B | 27,000 B | 29,696 B |
| Static SRAM | 1,214 B | 1,680 B | 2,560 B |
| Stack | unmeasured | ≥600 B reserve | remainder |

The 1,680 B static ceiling is a planning target, not a hardware fact. It was
lowered from 1,700 B at the M6 chart checkpoint, where the full image reached
1,678 B. The ceiling stays at 1,680 B through M7 hazards: the full image is
25,334 B flash / 1,680 B static after carving the chart `initialised` byte to
pay for the one-byte mine mask, and the chart+sonar+mine+current stack probe
passes at 641 B under a 600 B reserve target. Any further persistent static
feature has to carve existing state first.

Whole-image measurements only: LTO makes per-symbol arithmetic misleading. Use
`make size` and record profile A/B deltas.

### Profile matrix — 2026-10-22

`make size-matrix` at commit `ab8531d`, before any feature code is gated:

| Profile | Flash | Static RAM | Note |
| --- | ---: | ---: | --- |
| `full` | 6,636 B | 1,214 B | |
| `lean` | 6,636 B | 1,214 B | |
| `core` | 6,636 B | 1,214 B | |
| `min` | 6,636 B | 1,214 B | |

All four profiles are byte-identical because the flags currently gate no
feature code: the template has no feature-specific strings, tables, state, or
calls yet. The matrix is the comparison baseline for every later milestone; a
profile that fails to shrink when its features land is a harness regression.

### M1 render checkpoint — 2026-10-22

`sub-hauler-m1-render` replaces the template walker with the cave view:
full/lean/core/min all measure **6,840 B flash / 1,217 B static RAM**. The
static-only delta is +32 B of cave state (28 B `TileCache`, 4 B camera) offset
by the removed `game::State`, so the whole-image static delta is +3 B. The tile
atlas is 128 B `PROGMEM` and the AVR settlement metadata 36 B `PROGMEM`; name
strings are host-only. Ardens was unavailable, so this is build/static
evidence, not a device frame-rate measurement.

### M2 pilot view checkpoint — 2026-10-22

`sub-hauler-m2-view` wires the world-aware pilot into firmware: D-pad input,
fixed-step `stepWorld` against the tile cache, camera derived by flooring the
pilot camera to a tile, the 26×8 HAULER silhouette, and the compact HUD.
`full` and `min` both measure **10,584 B flash / 1,259 B static RAM**
(`.text` 10,534, `.data` 50, `.bss` 1,209). Against `fa0df2e` (6,840 B /
1,217 B) the whole-image delta is **+3,744 B flash, +42 B static RAM**; the
delta covers the M2 physics/collision commits plus this view slice because the
last recorded checkpoint predates them. Ardens was unavailable, so there is no
device frame-rate or screenshot claim.

### M3 station checkpoint — 2026-10-22

`sub-hauler-m3-station` adds auto-dock at the generated 42 px settlement rings,
a 128x64 station board, refill/repair service, and B/row undock with a 1.5 s
re-dock cooldown. Full/core measure **12,346 B flash / 1,276 B static RAM**
(+1,762 B / +17 B over the M2 view checkpoint). `min` stays at 10,584 B /
1,259 B because the station code and screen compile out. Ardens was
unavailable; no device docking playtest or screenshot is claimed.

### M3 jobs checkpoint — 2026-10-22

`sub-hauler-m3-jobs` adds deterministic two-offer boards, acceptance,
abandonment, flight-tick deadlines, delivery/credits, and the temporary
four-unit auto-load handoff. Full/lean/core measure **14,378 B flash /
1,319 B static RAM** (+2,032 B / +43 B over the station checkpoint). `min`
remains 10,584 B / 1,259 B because station/job code and strings compile out.
Ardens was unavailable; no device contract-loop playtest is claimed.

### M3 loop checkpoint — 2026-10-22

`sub-hauler-m3-loop` closes the fallback economy loop without adding cargo
loading UI. The permanent host integration suite (`tst/loop_test.hpp`) drives
the same station and flight transitions the sketch uses:

- spawn -> idle `stepFlight` -> auto-dock -> two offers;
- accept -> four-unit auto-load -> active summary, ABANDON, UNDOCK board;
- undock -> cooldown -> destination auto-dock -> on-time delivery paid ->
  destination offers -> a second contract accepted, delivered, and paid;
- accept -> flight past the deadline -> destination dock -> contract failed,
  cleared, and offers restored;
- accept -> ABANDON -> hold cleared -> offers restored and acceptable.

The auto-load remains unconditional in M3, including with
`SH_FEATURE_CARGO_LOAD=0`; the loader screen does not exist yet, so the
fallback has to keep every profile playable until M5 replaces it.

The checkpoint adds one active-board hint string; `full`, `core`, and `lean`
measure **14,410 B flash / 1,319 B static RAM**; `min` is **10,584 B /
1,259 B**.
Ardens was available at the end of this checkpoint: all four device suites
(`test_boot`, `test_cargo`, `test_memory`, `test_world`) printed `P`. The stack
probe confirms at least 800 B reserve under the target then in force. No manual
end-to-end playtest,
frame-rate, or screenshot claim is made.

### M5 cargo shell checkpoint — 2026-10-22

`sub-hauler-m5-shell` adds the 128x64 HAULER cutaway, 11x3 hold at 6 px cells,
structural bulkhead/notch marks, solid installed crates, held/tray cues, and
status slots. The shell loads HAULER arrangement 0 through the fixed
`cargo_screen::State` (104 B) and `cargo::loadArrangement`. All code, data, and
strings are inside `SH_FEATURE_CARGO_LOAD`; the `min` image is unchanged.

Full/core/lean now measure **18,892 B flash / 1,416 B static RAM**
(+4,482 B / +97 B over the M3 loop checkpoint); `min` remains 10,584 B /
1,259 B. A dock-only dev entry (`B+LEFT`) shows the shell until M5 rules and
integration wire the job flow. All four device suites printed `P` with Ardens;
the stack probe does not yet include the cargo screen. No screenshot or
frame-rate claim.

### M5 cargo rules checkpoint — 2026-10-22

`sub-hauler-m5-rules` makes the HAULER hold playable: selection across tray and
installed pieces, suspended movement, supported release, blocked cells,
installed lift with above-cargo blocking, clockwise/CCW rotation, undo,
restart helper, and short feedback. The stack probe now loads an arrangement
and runs cargo input/render frames. Full/core measure **21,836 B flash /
1,418 B static RAM**; `min` remains 10,584 B / 1,259 B. Device suites all
printed `P`, including the cargo path. Job commit and auto-undock remain for
M5 integration.

### M5 cargo integration checkpoint — 2026-10-22

`sub-hauler-m5-integration` connects the accepted contract to the loading
screen and removes the temporary dev entry:

- `jobs::accept` clears the hold under `SH_FEATURE_CARGO_LOAD` and leaves the
  four-unit fill to the handoff; with `CARGO_LOAD=0` it keeps the M3 auto-load.
- The active board is `[summary] LOAD CARGO ABANDON UNDOCK`, starts on the safe
  `LOAD CARGO` row, and wraps over selectable rows only. Acceptance opens the
  HAULER arrangement; `LOAD CARGO` reopens it.
- `A+B` with nothing held is `LOAD & UNDOCK`; the prompt replaces the feedback
  slot only when `cargo::requiredComplete` is true. A short attempt reports
  `LOAD SHORT` and keeps the job and berth. Success sets four pilot units,
  neutral trim, clears the loading state, and auto-undocks. Holding `B` returns
  to the station with the job open.
- The board layout still fits 128x64 (four rows, last baseline 48, hint at 56).

`tst/loop_test.hpp` now runs the full `CARGO_LOAD` loop accept -> cargo screen
-> required packing -> commit -> auto-undock -> deliver, plus rejection,
cancel-with-job-open, and the `CARGO_LOAD=0` auto-load fallback. The device
stack probe includes the commit/undock path.

Full/lean/core measure **22,038 B flash / 1,420 B static RAM** (+202 B / +2 B
over the M5 rules checkpoint); `min` remains **10,584 B / 1,259 B**. Host tests:
78 passed on full/core, 40 on min, and a separate 63-test `CARGO_LOAD=0` run
passed. `make test-tools` passed 71 tests. Ardens ran all four device suites
(`test_boot`, `test_cargo`, `test_memory`, `test_world`), each printed `P`; the
stack probe still passed its 800 B reserve (the target was re-baselined to
600 B at M6). No screenshot or frame-rate claim.

### M6 chart checkpoint — 2026-10-23

`sub-hauler-m6-chart` adds persistent 64x32 fog exploration and the paused
128x64 chart; sonar, save, and hazards remain out of this slice:

- `chart::State` is **257 B** at M6: `explored[world::CHART_BYTES]` (256 B)
  plus one `initialised` byte so the spawn reveal is idempotent. The sketch
  adds one 1 B blink counter, so the whole-image static delta is **+258 B**.
  The M7 hazard slice carves that byte (the spawn reveal runs once from
  `setup()`), returning `chart::State` to 256 B.
- Pixel/tile cell mapping is bounds-safe and floor-based; a square reveal
  radius clips at the world border. The spawn reveal is radius 1 and the
  movement reveal is radius 1 every 30 flight ticks, driven by `state.ticks`.
- `chart_view` draws 2x2 pixels per cell, reads the cart water mask exactly
  once per byte (256 reads per frame), and lights only explored water.
  Settlement crosses appear only after their cell is explored. The sub marker
  is a filled 2x2 block; the active job destination is an X driven by a
  caller-supplied blink phase, never by the paused simulation clock.
- `B` opens the chart in free flight and closes it; the chart frame performs
  no world step, so travel time, air, ballast, and `jobClock` do not tick and
  D-pad/A input is contained. There is no text or `B` cue on the chart; the
  key contract is documented here and in the sketch.
- `FOG_CHART=0` removes the state, screen enum value, calls, and paths. `core`
  is unchanged at **22,038 B / 1,420 B** and `min` at **10,584 B / 1,259 B**.
  A `STATIONS=0, FOG_CHART=1` host build also compiles and passes.

`full`/`lean` measure **23,286 B flash / 1,678 B static RAM**; an A/B full
build with chart and sonar off measures **22,038 B / 1,420 B**, so the chart
slice is **+1,248 B flash / +258 B static**. The full image now sits at
1,678 B static, so the roadmap planning ceiling is lowered from 1,700 B to
1,680 B: the next persistent state needs a carve. Host tests are 86 on full, 40
on min, and 47 on the no-station chart build.

All four device suites print `P`. `test_memory` includes the static
`chart::State` (the firmware ownership) and three chart frames; it measures a
**654 B** reserve and passes the **600 B** target with 54 B of margin.

Stack-target re-baseline, 2026-10-23: the old 800 B target was set before the
258 B persistent chart slice and is not reachable while the chart is in the
probe. Moving the explored set to a stack local is not a valid fix: the
firmware owns it statically, so a local would only hide 257 B of static state
in the probe. The honest numbers are the static probe's 654 B reserve and a
219 B measured stack depth; with 882 B of firmware static free, the firmware
reserve is about **663 B**. The 600 B target is therefore a deliberate pass
line 54 B below the measurement and about 381 B above the measured stack depth.
`core` remains **22,038 B / 1,420 B** and `min` **10,584 B / 1,259 B**; neither
profile changed. No screenshot or frame-rate claim.

### M6 sonar checkpoint — 2026-10-23

`sub-hauler-m6-sonar` adds the A-key ping as a navigation and chart tool. No
shop upgrade is wired; the pure rule already takes the level the M7 shop will
supply:

- `src/sonar.hpp` owns tuning and rules: base cost 6 air, one air unit cheaper
  per level (floor 1), base reveal radius 3 chart cells, one cell larger per
  level (cap 12), 120 flight ticks of cooldown, and a 24-tick ring window.
  `fire(state, chart, level)` returns `kFired`, `kNoAir`, `kCooldown`, or
  `kDocked` and is atomic: a rejected ping spends no air, reveals nothing, and
  does not increment `sonarPings`.
- `game::State` gains exactly two bytes under `SONAR`, in order:
  `sonarCooldown` then `sonarPings`. On AVR `sizeof(game::State)` is **70 B**
  (68 + 2; the host layout reports 80 B because of its alignment holes). The
  whole-image static delta is **+2 B**, taking the full image from 1,678 B to
  exactly the **1,680 B** planning ceiling. No other static state, string, or
  table was added.
- `sonarCooldown` decrements once per flight tick in both `game::step` and
  `game::stepWorld` (the two entry points are never nested), and `game::reset`
  clears both fields. The station screen never steps the world and the chart
  screen is paused, so neither ticks the cooldown; a host test pins that an
  open chart frame leaves the cooldown, ticks, and air unchanged.
- `src/sonar_view.hpp` is presentation-only: a midpoint circle centred on the
  sub's pilot-view position, radius derived solely from `sonarCooldown`
  (4..20 px over the 24-tick window), clipped by `cave::setPixel`. `view::frame`
  draws it before the HAULER silhouette so the boat stays solid. There is no
  extra animation state and no cart read.
- The sketch fires level 0 on `A` just-pressed after the world step and after
  the docking branch returns, and before the single `FX::display`. `SONAR=0`
  removes the fields, action, ring, include, and tests; `FOG_CHART=0` forces
  `SONAR=0` through the existing dependency error.
- Host tests add 11 suites entries: level cost/radius, reveal radius and cell
  count, no-air atomicity, cooldown rejection and expiry, docked rejection,
  once-per-step decrement on both paths, reset, ring phase/radius, hidden and
  deterministic ring, edge clipping with guard bytes, and chart-pause proof.
  `make test` passes **97** on full, **78** on core, and **40** on min.

`full`/`lean` measure **23,770 B flash / 1,680 B static RAM**; an A/B full build
with only `SONAR=0` measures **23,286 B / 1,678 B**, so the sonar slice is
**+484 B flash / +2 B static**. `core` stays **22,038 B / 1,420 B** and `min`
**10,584 B / 1,259 B**. There is now no static headroom under the ceiling; the
next persistent byte has to carve existing state.

All four device suites print `P`. `test_memory` adds one real ping, one world
step, and a pilot frame with the ring to the measured path (the probe reuses
the existing `probeChart`, so no chart state is duplicated). With diagnostics
enabled it measures **`stack_reserve=649`** and passes the **600 B** target
with 49 B of margin; the production run prints a bare `P`. No screenshot or
frame-rate claim.

### M7 hazards checkpoint — 2026-10-23

`sub-hauler-m7-hazards` adds deterministic mines and current zones from
generated metadata. Damage, particles, and the shop stay in their later M7
beads; the mine hit calls a one-point `hazards::applyDamage()` hook under
`SH_FEATURE_HULL_DAMAGE` and nothing else.

- **Mines** are the eight existing `MINE_META` positions. `src/hazards.hpp`
  owns a single `uint8_t minesAlive` mask in `game::State`; no per-mine array
  exists in SRAM. The hit test is a circle (`MINE_RADIUS_PX`) against the 26x8
  HAULER hull, using squared integer distance. `collide()` consumes at most one
  armed mine per step, records `impactAxis = kMineImpactAxis` (3) and
  `impactSpeedQ8` for a one-frame pilot-view spark, and clears that spark at
  the start of the next world step. A hit cannot fire twice; `reset()` re-arms
  the generated mask.
- Mines are consumed only after the free-flight world step. `stepWorld()`
  guards the hazard pass with the same `docked < 0` test the station path
  already enforces; the chart and cargo screens never call `stepWorld()`.
- **Currents** are eight generated 32x16 px zones (one per non-settlement
  chamber), stored as `{x, y, w, h, ax_q8, ay_q8}` metadata in
  `src/generated/world.hpp`. `src/currents.hpp` applies `ax_q8 / 60` or
  `ay_q8 / 60` to the Q8.8 velocity before the same axis-separated collision
  walk, capped at +/-32 px/s. No current state is stored: outside a zone the
  velocity is untouched, and a docked boat is never pushed.
- `tools/gen_world.py` validates every zone: positive size, non-zero
  acceleration, inside the world, every overlapped tile open, and at least
  42 px (nearest-point distance) from every settlement/spawn dock centre. The
  zones consume no PRNG and no FX table/ABI bytes; only the generated header
  grows.
- `src/hazard_view.hpp` draws mine glyphs and animated flow marks before the
  HAULER silhouette, and the one-frame impact spark after it. All drawing is
  clipped by `cave::setPixel`; there are no new strings.
- `MINES` and `CURRENTS` are independent flags. `full` has both, `lean`,
  `core`, and `min` have both off, so their state, tables, calls, includes,
  and render paths compile out. The current metadata constants/records still
  exist in the header, but no runtime code references them when the flag is
  off.

Budget: `sizeof(game::State)` is **71 B** on AVR with `MINES=1` (70 B with
`MINES=0`); `chart::State` drops from 257 B to exactly **256 B** by removing
the `initialised` byte, so the full image stays at **1,680 B static**, exactly
the ceiling. Whole-image A/B: mines **+730 B flash / +1 B static**, currents
**+852 B flash / +0 B static**, both together **+1,574 B flash / +1 B static**.
The full image is **25,334 B / 1,680 B**; `lean` is **23,760 B / 1,679 B**;
`core` is unchanged at **22,038 B / 1,420 B**; `min` is unchanged at
**10,584 B / 1,259 B**. The stack probe with the mine and current frames
measures **`stack_reserve=641`**, passing the 600 B target with 41 B of
margin. Host tests: 111 full, 97 lean, 78 core, 40 min; `make test-tools`
passes 75. All four device suites print `P`.

### Harness sentinel proof — 2026-10-22

A temporary `SH_FEATURE_SENTINEL` block put a unique string
(`SENTINEL_M0_PROOF_7QK9`) and a small arithmetic helper on the normal render
path (`view::hud`), with a temporary header default. `full` was built through
the existing profile harness with the sentinel default on and off:

| Build | Flash | Static RAM | Sentinel string |
| --- | ---: | ---: | --- |
| `full`, sentinel on | 6,690 B | 1,214 B | present |
| `full`, sentinel off | 6,636 B | 1,214 B | absent |

Delta: **54 B flash, 0 B static RAM**. `strings dist/full/sub-hauler.ino.elf |
grep SENTINEL_M0_PROOF_7QK9` matched only the enabled build. The sentinel
block, string, and temporary default were then removed; the committed tree has
no sentinel and no `SH_FEATURE_SENTINEL` default. This proves the harness
removes code and strings from the whole image, not just a runtime branch.

## Architecture decisions

### 1. Bake the cave into FX flash

The browser world is 320×160 tiles. A byte grid is 51,200 B and a bit grid is
6,400 B; neither fits SRAM.

- Generate one canonical cave bitmap offline: 320×160 bits = **6,400 B** in the
  FX image.
- Generate a 64×32 chart water bitmap: **256 B** in the FX image.
- Put settlement, spawn, mine, and current metadata in a generated header
  (PROGMEM) or a small FX record. Metadata is tiny.
- At runtime read only the visible tile window and the tiles around the sub.
  An 18×10 tile cache is about **23 B** of SRAM.
- Draw cave walls from a small 8×8 tile atlas. Do not draw individual tiles
  from the collision grid one pixel at a time.
- Keep all cart reads in update/render before `FX::display(CLEAR_BUFFER)`.

The map pipeline is `tools/gen_world.py` → `fxdata/tables/world.bin` +
`src/generated/world.hpp` → `raw_t world` in `fxdata/fxdata.txt`. Add the new
artifacts to `gen-check`; the current snapshot list only covers sprites.

One cave layout is enough for the first device build. Multiple baked layouts
can be appended later; external flash is not the constraint.

### 2. Fixed-step integer simulation

- 60 Hz fixed step; no `dt` in the device rules.
- Integer positions in pixels; subpixel velocity in Q8.8 or a small
  accumulator. No `float`/`double`.
- Ballast 0–100 integer; air in fixed tenths; job deadlines in seconds or
  frames.
- Camera uses an integer shift lerp, not an exponential.
- Fixed-size arrays only. No per-frame allocation.
- Host tests drive the same pure rules with deterministic inputs.

### 3. Keep the pure/device boundary

Proposed shape:

```text
src/features.hpp      feature defaults + dependency checks
src/game.hpp          public rules state and API (no Arduino headers)
src/sim/*.hpp         world, pilot, jobs, cargo, chart (pure where possible)
src/view/*.hpp        pilot/station/cargo/chart drawing
src/common.hpp        Arduboy/FX/Blit boundary
src/render.hpp        frame dispatch only
sub-hauler.ino        setup, input sampling, screen state machine
```

Header-only modules are fine while the project is small; split into `.cpp`
only if build time or code organisation demands it. The host test target must
keep compiling the same pure rules.

### 4. Reuse screen state

Screens are mutually exclusive. Use a `Screen` enum and, where useful, a union
for screen-local state (cargo pieces/grid vs. station selection). Persistent
state stays in one `GameState`: position, ballast, air, cargo units, credits,
active job, upgrades, explored chart.

### 5. Cargo data is precomputed; the UI is native

- Generate 64 arrangements per hull offline (target: 3.6 KB external).
- Runtime record: `uint16_t mask` + `uint8_t posKind` per piece.
- Load one arrangement into a fixed `CargoPiece[8]`; keep only the selected
  puzzle in SRAM (<100 B).
- Validate all records offline: connected shapes, bounds, overlap, support,
  required/bonus counts, exact load.
- Start with the HAULER hold. MINNOW and LEVIATHAN holds follow behind the
  multi-hull flag.
- No browser generator, cluster map, retry loop, or heap on device.

### 6. One hull first

Default device hull: **HAULER**. It exercises the split bulkhead and the
largest practical cargo screen before the easy hulls. Multi-hull selection is
a late feature flag so it can be cut without touching the core loop.

### 7. Input contract — D-pad flight, A/B reserved in flight

Locked 2026-10-22 for M2, before any station rows or cargo UI depend on it:

| Input | M2 action |
| --- | --- |
| D-pad left / right | propeller thrust / reverse |
| D-pad up | blow ballast (rise; spends air) |
| D-pad down | flood ballast (sink; free) |
| A | reserved; no action in M2 |
| B | reserved; no action in M2 |

Releasing up/down closes the valves and holds the current ballast. There is no
auto-forward and no context action yet.

Change from the earlier candidate: A (sonar/context) and B (chart) are not
wired in M2, and auto-dock is not implemented or validated. Auto-dock when
slow inside a settlement ring remains the leading M3 option because it avoids
a third button, but M3 must prove it in a playtest before station rows depend
on it. If it fails, B becomes the docking/context action and chart moves to a
later screen.

M3 resolution (2026-10-22): auto-dock when slow inside a settlement ring is
implemented and covered by host tests. Station rows use D-pad to select, A to
confirm, and B to undock; flight A/B remain inert. No context-dock fallback is
wired. Device playtest for auto-dock is still pending.

M6 resolution (2026-10-23): in free flight `B` opens the paused chart and `A`
fires the sonar ping. Both edges are consumed before the flight step, so the
frame that opens the chart does not tick travel/air/ballast and the ping does
not also count as a station action. The chart, station, and cargo screens never
route A to the sonar. A docked boat is rejected by the pure rule even if a
future screen calls it. Station A remains SELECT/confirm and B remains UNDOCK;
cargo keeps its `A+B` commit.

## Feature flags

### Naming

Use numeric `0`/`1` macros named `SH_FEATURE_*`. They are compile-time only;
there is no runtime options menu.

| Flag | Default | Depends on | Purpose |
| --- | ---: | --- | --- |
| `SH_FEATURE_STATIONS` | 1 | — | docking, station board, refill |
| `SH_FEATURE_JOBS` | 1 | `STATIONS` | contracts, credits, delivery |
| `SH_FEATURE_CARGO_LOAD` | 1 | `JOBS` | loading screen + job load handoff |
| `SH_FEATURE_FOG_CHART` | 1 | — | 256 B explored chart; chart screen |
| `SH_FEATURE_SONAR` | 1 | `FOG_CHART` | ping, reveal, cooldown |
| `SH_FEATURE_MINES` | 1 | — | mine placement, collision, damage |
| `SH_FEATURE_CURRENTS` | 1 | — | current zones/toggle |
| `SH_FEATURE_SHOP` | 1 | `STATIONS` | upgrades and credit sink |
| `SH_FEATURE_HULL_DAMAGE` | 1 | — | impact damage/integrity |
| `SH_FEATURE_PARTICLES` | 1 | — | wash/bubbles/debris |
| `SH_FEATURE_MULTI_HULL` | 1 | — | hull roster/selection and hold variants |
| `SH_FEATURE_SAVE_CHART` | 0 | `FOG_CHART` | persist explored chart |

Core (always on): sub physics, ballast/air, cave collision, camera, HUD,
screen/input state machine.

If `SH_FEATURE_CARGO_LOAD=0`, accepting a job still sets the four cargo units
and undocks; the loading UI and arrangement loader are not compiled. If
`SH_FEATURE_STATIONS=0`, jobs/shop/cargo must also be off and the build is a
free-flight cave run. If `SH_FEATURE_FOG_CHART=0`, sonar and chart save must be
off.

### Header and validation

`src/features.hpp` is the single source of feature names, defaults, and
dependency rules; docs and tools describe or consume that header rather than
redeclaring flags. It supplies defaults for direct builds and validates
dependencies with `#error`. Example:

```cpp
#pragma once
#ifndef SH_FEATURE_JOBS
#define SH_FEATURE_JOBS 1
#endif
#ifndef SH_FEATURE_STATIONS
#define SH_FEATURE_STATIONS 1
#endif

#if SH_FEATURE_JOBS && !SH_FEATURE_STATIONS
#error "SH_FEATURE_JOBS requires SH_FEATURE_STATIONS"
#endif
```

Rules:

- Wrap types, state fields, strings, menu rows, input branches, update calls,
  and render calls. A disabled feature must not leave its strings or tables in
  the image.
- Do not use `if (SH_FEATURE_X)` for compile-time scope.
- Do not gate the same build with different flags across translation units.
- Keep flags out of save/wire formats.
- Every milestone must build `full`, `lean`, and `core` profiles.

### Profiles

| Profile | Intent |
| --- | --- |
| `full` | All features; default development and release target |
| `lean` | No currents, mines, particles, shop, or save; keeps fog/sonar/cargo/multi |
| `core` | Lean plus no fog/sonar/multi; cargo load still available; single HAULER |
| `min` | Pilot + cave only; no stations/jobs/cargo/shop/chart |

Carving order when space runs low:

1. particles
2. chart save
3. currents
4. mines
5. hull damage
6. shop
7. sonar
8. fog/chart
9. multi-hull
10. cargo loading UI (fall back to auto-load)
11. stations/jobs (free-flight build)

Keep the pilot, cave, and one hull as the last useful build.

### Build harness

Add `tools/features.py` to validate a profile and print `-D` flags:

```sh
make build PROFILE=lean
make size PROFILE=lean
make test PROFILE=lean
make fxtest PROFILE=lean
make feature-report PROFILE=full
make size-matrix
```

- `features.py` owns profile definitions, dependency checks, and flag output.
- Pass the flags to C and C++ compiler extra flags and to host test flags.
- Use profile-specific `build/$(PROFILE)` and `dist/$(PROFILE)` paths so a
  stale object file cannot mix feature sets.
- `make size-matrix` builds `full`, `lean`, `core`, and `min`, then prints the
  whole-image table. It is the release gate, not just a convenience.
- Add Python unit tests for unknown profiles and invalid dependencies.

Before adding features, prove the harness with a sentinel feature: a disabled
build must be measurably smaller, not merely skip a runtime branch.

## Milestones

Each milestone ends with host tests, profile builds, `make size`, and an
entry in the feature ledger. Device suites run when Ardens is available.

### M0 — Feature-flag harness and budget guard

Tasks:

- Add `docs/roadmap.md`, `src/features.hpp`, `tools/features.py`, and its tests.
- Add profile-aware Makefile targets, build paths, and `feature-report`.
- Add a painted-stack probe to the device smoke suite.
- Record baseline full/lean/core/min images.

Acceptance:

- Profiles compile; invalid dependencies fail with a clear message.
- A disabled sentinel feature reduces the image.
- Current behaviour unchanged.
- Stack method and reserve are documented in `docs/platform.md`.

### M1 — World pipeline and cave rendering

Tasks:

- Add `tools/gen_world.py`; port the fixed chamber/tunnel layout to a baked
  bitmap; generate chart water bits and metadata.
- Add `raw_t world` to `fxdata/fxdata.txt`; update `make gen`, `FX_INPUTS`, and
  `gen-check`.
- Add `world` accessors for tile solidity, visible-window cache, settlement
  positions, mines, and currents.
- Add an 8×8 cave tile atlas and a wireframe/debug render of the visible cave.
- Add tool tests for connectivity, map bounds, settlement clearance, and
  deterministic output.

Acceptance:

- `make gen-check` covers every new artifact.
- No full-world grid in SRAM; static delta is small and recorded.
- Known tile bits and chart-water samples match the generator.
- Cart reads happen only in update/render.

### M2 — Pilot core

Tasks:

- Replace the template walker with submarine state and fixed-step physics.
- Implement ballast blow/flood, neutral trim from cargo units, thrust/reverse,
  vertical drag, collision, impact handling, and camera.
- Draw the HAULER pilot silhouette, cave, settlements/gantries, and minimal HUD.
- Add host tests for trim, clamping, collision, camera bounds, and air.
- Establish the input contract for flight.

Input map (locked 2026-10-22; see "Input contract — D-pad flight, A/B
reserved" above):

- D-pad: thrust left/right; blow/flood up/down.
- A and B: reserved with no action in M2.
- Auto-dock when slow inside a settlement ring is the leading M3 option so a
  third button is not consumed; playtest it before station rows depend on it.

Acceptance:

- Fly the cave at 60 fps with no floats or heap.
- Collision never tunnels at expected speeds.
- `PROFILE=min` builds and flies.
- Whole-image delta recorded; static stays under target.
- D-pad control contract documented for M3.

### M3 — Stations, contracts, economy shell

Tasks:

- Add station screen, docking/undocking, refills, and row selection.
- Add two job offers per station, accept/abandon/deadline/delivery, credits.
- Add `STATIONS`, `JOBS` flags and remove all rows/strings when off.
- Auto-load four units per job for now; cargo screen comes next.
- Add host tests for offer generation, deadlines, delivery, abandonment, and
  edge cases at zero/partial cargo.

Acceptance:

- Complete auto-load loop: dock → accept → undock → deliver → credits.
- Dock at origin, wrong station, late arrival, and insufficient cargo behave
  deterministically.
- `lean` and `core` profiles still build.

### M4 — Cargo arrangement pipeline

Can run in parallel with M1–M3.

Tasks:

- Add `tools/gen_cargo.py` and a shared record definition.
- Generate 64 validated arrangements per hull (HAULER first).
- Add tables to `fxdata/fxdata.txt`; keep declaration order append-only.
- Add `CargoPiece`/`CargoPuzzle` loader state and a host parser test.
- Add a device test that reads one record through FX and verifies masks and
  target positions.

Acceptance:

- Every generated record passes connectivity, bounds, overlap, support, load,
  and required/bonus validation.
- External table size is documented; SRAM use is under ~100 B.
- No runtime generator code is compiled.
- Loader reads happen outside the display transaction.

### M5 — Cargo loading screen

Split into three beads:

1. Screen shell: cutaway frame, hold mask, tray, text, one hull.
2. Interaction: move held piece, support, pick/lift, rotate, undo, blocked-above.
3. Job integration: required/bonus progress, load commit, auto-undock, cancel.

Tasks:

- Draw 128×64 cutaway with HAULER cell scale.
- Add `CARGO_LOAD` flag and compile-out fallback.
- Lock the cargo commit gesture: `A+B` with nothing held, shown only when the
  required load is met. The station `LOAD CARGO` row reopens the screen.
- Add host tests for placement, support, block, rotate, and undo.
- Add device test for a known arrangement.

Acceptance:

- Solve a generated HAULER arrangement on device.
- Cannot place in blocked structural cells.
- Job cannot load below requirement.
- `CARGO_LOAD=0` still delivers through auto-load.
- Size delta recorded; carve order remains viable.

### M6 — Fog, chart, and sonar

Tasks:

- Add 64×32 explored bitfield (256 B) and movement reveal.
- Draw paused chart from baked water bits; settlement/job/sub markers.
- Add sonar ping cost, reveal radius, cooldown, and ring effect.
- Add `FOG_CHART` and `SONAR` flags.
- Add host tests for cell mapping, radius, and persistence within a run.

Acceptance:

- Chart opens/closes without burning travel time.
- Unexplored water stays black; explored water draws.
- Sonar spends air, reveals its chart radius, and honours its cooldown; a
  docked boat or an open chart cannot ping.
- 256 B static cost is in the ledger.
- `FOG_CHART=0` removes chart, sonar, and their strings.

### M7 — Hazards, damage, particles, shop

Tasks:

- Add mines and currents from generated metadata; integer collision.
- Add hull/integrity damage and collision feedback.
- Add optional particle effects.
- Add shop screen and upgrades (air, hull, sonar) if not already in M3.
- Add `MINES`, `CURRENTS`, `HULL_DAMAGE`, `PARTICLES`, `SHOP` flags.
- Add host tests for each hazard and shop transaction.

Acceptance:

- Each feature toggles independently and is absent from lean builds.
- Full run remains survivable and clearable; no unfair spawns.
- Size ledger identifies the cheapest first cuts.

### M8 — Multi-hull and content

Tasks:

- Add MINNOW and LEVIATHAN pilot art, holds, cargo scales, and arrangements.
- Add hull selection UI (title screen or station drydock).
- Add `MULTI_HULL`; single-hull builds compile only HAULER.
- Add `HAULER` default and selection persistence within a run.
- Balance cargo/mass/trim across classes.
- Add host tests for class-specific hold masks and scaling.

Acceptance:

- All three classes can complete the loop.
- `MULTI_HULL=0` compiles with HAULER only.
- Hold masks, blocked cells, and cargo records match the roster.
- No per-class strings/data remain in single-hull builds.

### M9 — Polish, save, release hardening

Tasks:

- Add optional chart save (`SAVE_CHART`) with checksummed writes on stable
  transitions; keep writes out of the frame loop.
- Add title/pause flow, clearer failure states, and final HUD text.
- Run full device suite, stack paint, and a soak run.
- Balance jobs/shop; freeze the content set.
- Update `README.md`, `docs/design.md`, and `docs/platform.md` measurements.
- Produce the final size matrix and carve report.

Acceptance:

- `make gen-check`, `make test`, `make test-tools`, `make fxtest`, and
  `make size` pass.
- Full profile meets flash/static/stack ceilings.
- Every profile builds; disabled features are absent, not just hidden.
- A complete loop is playable on hardware.

## Feature ledger

Keep this table current after every milestone. Costs are whole-image A/B
deltas against the same profile with the feature disabled.

| Feature | Flash Δ | Static Δ | Measured on | Notes |
| --- | ---: | ---: | --- | --- |
| baseline template | 6,636 | 1,214 | 2026-10-22 | current firmware |
| cave view (M1) | +204 | +3 | 2026-10-22 | replaces walker/ground/hero/HUD; 128 B atlas + 36 B AVR metadata in flash; 32 B runtime state |
| stations (M3) | +1,762 | +17 | 2026-10-22 | auto-dock, board, refill/repair; min compiles out |
| jobs (M3) | +2,064 | +43 | 2026-10-22 | offers, deadlines, delivery, credits, active auto-load hint; M5 replaces fallback |
| cargo load (M5) | +7,628 | +101 | 2026-10-22 | cutaway, hold/tray, rules, job handoff, auto-undock |
| fog/chart | +1,248 | +258 | 2026-10-23 | 64x32 explored set (256 B) + initialised byte + blink; paused chart, reveal, markers; full A/B; M7 carved the initialised byte |
| sonar | +484 | +2 | 2026-10-23 | A-key ping: 6 air base (-1/level), 3+level cell reveal, 120-tick cooldown, 24-tick ring; 2 state bytes; full A/B |
| mines | +730 | +1 | 2026-10-23 | 8 fixed positions, one mask byte, circle/hull hit, one-frame spark, optional hull hook; full A/B |
| currents | +852 | +0 | 2026-10-23 | 8 generated 32x16 zones, Q8 acceleration, flow marks; metadata only; full A/B |
| shop | — | — | — | |
| hull damage | — | — | — | |
| particles | — | — | — | |
| multi-hull | — | — | — | |

The mine and current A/B deltas share 8 B of LTO folding: both enabled is
+1,574 B, not +1,582 B. The chart `initialised` carve (-1 B) offsets the mine
mask (+1 B), so the full image remains at the 1,680 B ceiling.

Record the profile matrix separately:

| Date | Profile | Flash | Static | Stack evidence | Notes |
| --- | --- | ---: | ---: | --- | --- |
| 2026-10-22 | template | 6,636 | 1,214 | none | pre-port baseline |
| 2026-10-22 | M1 cave render | 6,840 | 1,217 | none (Ardens unavailable) | full/lean/core/min identical |
| 2026-10-23 | M6 chart | 23,286 | 1,678 | `test_memory` P (chart path, 654 B under 600 B target) | full/lean identical; core 22,038/1,420; min 10,584/1,259 |
| 2026-10-23 | M6 sonar | 23,770 | 1,680 | `test_memory` P (chart + sonar path, 649 B under 600 B target) | full/lean identical; core 22,038/1,420; min 10,584/1,259; at the static ceiling |
| 2026-10-23 | M7 hazards | 25,334 | 1,680 | `test_memory` P (chart + sonar + mine + current path, 641 B under 600 B target) | full; lean 23,760/1,679; core 22,038/1,420; min 10,584/1,259; full still at the static ceiling after the chart-byte carve |

## Verification gates

Every milestone:

- `make test`
- `make test-tools`
- `make build PROFILE=full`
- `make build PROFILE=lean`
- `make build PROFILE=core`
- `make size PROFILE=full`
- review static/flash delta against the ledger

Before content freeze:

- `make gen-check`
- full `make fxtest`
- `make size-matrix`
- stack-paint run
- no `float`/`double` symbols in device objects
- no heap symbols beyond the Arduino core
- no FX reads between render completion and display

## Risks and mitigations

| Risk | Mitigation |
| --- | --- |
| Static SRAM or stack overrun | ceilings, union screen state, PROGMEM/FX tables, stack paint early, feature carve |
| Cargo screen too large in flash | one hull first, offline records, split shell/rules, cut UI last |
| Cave collision/render too slow | baked FX map + small visible cache; no full-map work per frame |
| Feature-flag rot | profiles built every gate; flags wrap data/strings, not just branches |
| Input map too cramped | lock flight/station/cargo controls in M2/M3/M5 with playtests |
| Cargo puzzle parity drift | validate generated records and test known fixtures, not JS runtime |
| Art flash growth | tile atlas + small pilot sprites; measure per asset; no FLEET art |
| Scope creep from browser | browser remains reference only; no DOM/objects/localStorage ports |
| Save complexity | default off; add only after core loop fits |

## Open decisions

- Auto-dock versus a button dock action.
- Cargo commit gesture: resolved to `A+B` with nothing held, prompted only when
  the required load is complete.
- Whether job holds stay full four units or support partial holds.
- Number of cave layouts; one fixed layout initially.
- Hull selection location (title, drydock, or station row).
- Chart save in v1; currently deferred.
- Final flash/static ceilings after stack measurement.

## Bead map

The roadmap is loaded into `bd` under root epic `sub-hauler-port`. Milestone
epics are containers; dispatch their child tasks, not the epics.

| Milestone | Epic | Child beads |
| --- | --- | --- |
| M0 | `sub-hauler-m0` | `m0-flags`, `m0-harness`, `m0-proof`, `m0-stack` |
| M1 | `sub-hauler-m1` | `m1-world-gen`, `m1-fx-abi`, `m1-runtime`, `m1-render` |
| M2 | `sub-hauler-m2` | `m2-physics`, `m2-collision`, `m2-view` |
| M3 | `sub-hauler-m3` | `m3-station`, `m3-jobs`, `m3-loop` |
| M4 | `sub-hauler-m4` | `m4-cargo-gen`, `m4-cargo-fx`, `m4-loader` |
| M5 | `sub-hauler-m5` | `m5-shell`, `m5-rules`, `m5-integration` |
| M6 | `sub-hauler-m6` | `m6-chart`, `m6-sonar` |
| M7 | `sub-hauler-m7` | `m7-hazards`, `m7-damage`, `m7-shop` |
| M8 | `sub-hauler-m8` | `m8-roster`, `m8-holds` |
| M9 | `sub-hauler-m9` | `m9-save`, `m9-polish`, `m9-release` |

Full IDs use the `sub-hauler-` prefix. Each child carries description, design,
acceptance criteria, labels, estimate, and blocking dependencies.

## Immediate next work

1. Add the feature-flag harness and measure the first profile matrix (M0).
2. Add the stack-paint check and confirm the SRAM ceiling.
3. Build the baked world pipeline and a flyable cave before adding economy or
   cargo UI.
4. Start the offline cargo generator in parallel so it is validated before the
   screen exists.

No firmware feature work should land before the M0 flag harness; otherwise
carving later becomes a rewrite instead of a build flag.

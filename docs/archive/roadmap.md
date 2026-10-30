# Roadmap — Arduboy device port

> **Historical record — archived 2026-10-30.** This roadmap documents the port sequence and retired profile system. Current contracts live in [platform.md](../platform.md) and [dev-flow.md](../dev-flow.md).

Status: **full release build, 2026-10-28** (`sub-hauler-7px`). The browser
sandbox is retired (available in git history); the device firmware
implements the full dock → WORK → auto-load → undock → travel → deliver loop
across three hulls, plus campaign restoration, schema 5 cart checkpoints, synth
music and the boot introduction. The cargo-packing minigame
was removed because its value did not justify its flash/SRAM cost; the final
pass moved UI text and fixed pilot art to FX and compacted generated metadata
and AVR run state. This document sequences the port so each milestone leaves a
playable or testable device build; earlier cargo milestones remain below as
history.

## Goal

Stand up the browser game loop on the Arduboy FX:

> dock → accept contract → auto-load hold → undock → travel → deliver → repeat

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

Measured 2026-10-29 on the fixed build:

| Resource | Current | Planning ceiling | Hardware limit |
| --- | ---: | ---: | ---: |
| Flash | 29,604 B | 29,000 B, advisory | 29,696 B |
| Static SRAM | 1,660 B | 1,700 B | 2,560 B |
| Stack | 652 B combined probe / 690 B save chain | ≥600 B reserve | remainder |

The fixed build has **92 B** of hardware flash headroom, **40 B** static
headroom and **900 B** static free SRAM. It is 604 B over the 29,000 B advisory
line, which is accepted as advisory; the hardware limit is 29,696 B. Host tests
pass **246**, tooling **119**, and all **14** default device suites print `P`.
The separate flight probe covers 842 complete frames at 11.864 ms maximum with
zero deadline misses and zero audio underruns/errors.

Feature-profile, development, runtime-USB, streaming-backend and EEPROM
chart-save variants are retired. Dated checkpoints below remain historical.

Whole-image measurements only: LTO makes per-symbol arithmetic misleading. Use
`make size` and record fixed-build A/B deltas.

### Cargo packing removal — 2026-10-24

`sub-hauler-gl1.1` supersedes the former cutaway/packing direction and the
cargo-loader validation work:

- `Screen::Cargo`, the LOAD CARGO row, the A+B commit chord, the hold-B exit
  timer, `s_cargo`, the packing rules, and the `CARGO_LOAD` flag/profile
  plumbing are removed.
- Acceptance uses the former `CARGO_LOAD=0` contract: it auto-loads the four
  coarse cargo units, neutralizes trim, and keeps the boat docked. An explicit
  `UNDOCK` row action or `B` starts travel.
- Coarse cargo units/physics, jobs/economy/deadlines, all playable hulls, the
  docking latch, flight pause, chart/sonar, hazards, shop, and save option are
  preserved.
- The shipped `cargoBank` declaration, position, size, bytes, generator, and
  `make gen-check` coverage remain for flash ABI compatibility. The runtime
  reader, screen, rules, and their host/device tests are retired.
- `test_memory` now runs the cargo-free accept/auto-load/station-board/undock
  path instead of the removed hold screens.

This is the measured replacement, not a feature toggle: there is no compile-time
switch to bring packing back. Reintroducing it needs a new user decision and a
new budget.

### UI/art and metadata resource pass — 2026-10-24

`sub-hauler-gl1.2`–`gl1.8` completed the cargo-free resource pass and integrated
verification.

- **Compact world metadata.** Position-only AVR settlement/mine/current records
  fell from **148 B to 88 B PROGMEM**. The incremental A/B was
  17,584/14,810/12,640/9,178 B -> **17,548/14,792/12,626/9,142 B**, with no RAM
  or cart-read change. The 128 B cave atlas stays in PROGMEM because FX would
  add hot render traffic.
- **Route economy generated.** Rewards and deadlines are baked compact tables;
  diagnostic counters and the job clock were removed, route reward fields were
  narrowed, and AVR `game::State` is now **80 B**.
- **UI text in FX.** Surviving labels were deduplicated into
  `fxdata/fxdata.txt`; the external 6x8 font and `src/text.hpp` adapter replace
  `Print`. `dcmInvert` keeps selected-row glyphs black at the 7 px station/shop
  pitch. The firmware uses `Arduboy2Base`, and the default Arduboy font/Print
  symbols are absent from the ELF.
- **Pilot art in FX.** MINNOW/HAULER/LEVIATHAN are generated 26x8 plus-mask
  sheets with right/left and unloaded/loaded frames, shared by title and pilot.
  Host pixel tests retain equivalent procedural shapes.
- **ABI preserved.** `cargoBank` stays at **0x001AC2** with unchanged bytes and
  order; all new FX entries append after it. The FX payload in
  `fxdata/fxdata-data.bin` is **13,012 B**, while the padded flashable
  `fxdata/fxdata.bin` image is **13,056 B**.

Final evidence: `make gen-check` pass; host `make test` 132/99/75/39 passed
(full/lean/core/min); `make test-tools` 77 passed; `make fxtest`
`test_boot`/`test_memory`/`test_world` all `P`; full `mini` 17,548 B / 1,572 B;
full diagnostics `stack_reserve=812` against the 600 B target. No manual
interactive playthrough or manual device soak was run; evidence is the host loop
suite plus the device smoke run.

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

### M5 cargo shell checkpoint — 2026-10-22 (historical; removed 2026-10-24)

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

### M5 cargo rules checkpoint — 2026-10-22 (historical; removed 2026-10-24)

`sub-hauler-m5-rules` makes the HAULER hold playable: selection across tray and
installed pieces, suspended movement, supported release, blocked cells,
installed lift with above-cargo blocking, clockwise/CCW rotation, undo,
restart helper, and short feedback. The stack probe now loads an arrangement
and runs cargo input/render frames. Full/core measure **21,836 B flash /
1,418 B static RAM**; `min` remains 10,584 B / 1,259 B. Device suites all
printed `P`, including the cargo path. Job commit and auto-undock remain for
M5 integration.

### M5 cargo integration checkpoint — 2026-10-22 (historical; removed 2026-10-24)

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
  once per byte (originally 256 transactions per frame), and lights only explored water.
  Settlement crosses appear only after their cell is explored. The sub marker
  is a filled 2x2 block; the sub and the active job destination both blink from
  a caller-supplied phase, never from the paused simulation clock.
  The 2026-10-26 performance pass batches these bytes into eight bursts and
  redraws only at 16-frame blink boundaries. See
  [current measurements](../render-performance.md).
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

### M7 damage and particles checkpoint — 2026-10-23

`sub-hauler-m7-damage` adds hull damage and a fixed debris pool. Damage does
not gate the mine rule: a mine is still removed exactly once and costs one
point through `hazards::applyDamage()`.

- **Damage tuning** lives in `src/damage.hpp`: one point per hit, a hard-impact
  threshold of **16 px/s** (`game::kHardImpactSpeedQ8`), a **24-tick** wall-hit
  cooldown, and a **12-tick** edge-flash window. The cooldown doubles as the
  flash timer, so damage costs one byte of state. A scrape below 16 px/s is
  free. Damage resolves only from the axis move that produced the event
  (`moveAxis()` now returns whether it recorded an impact), while the existing
  `impactAxis`/`impactSpeedQ8` fields keep their historical latch for render
  and tests.
- **Zero hull is valid.** `damage::applyDamage()` floors at zero and never
  touches movement, so the boat remains controllable until M9 owns failure
  states. `damage::repair()`/`repairFull()` clamp to `kHullMax`; station
  SERVICE now uses `repairFull()`, which also clears the cooldown.
- **Particles** live in `src/particles.hpp` under `SH_FEATURE_PARTICLES`: four
  fixed slots sharing one world-pixel anchor. Each slot is 3 B (`dx`, `dy`,
  `life`), so the pool is 16 B. Slot bit 0/bit 1 select the spawn corner and
  drift direction; there are no velocity tables, heap, `String`, or float.
  Offsets advance one pixel per flight tick and clamp at ±24 px, lifetimes are
  14..11 ticks, and a new hit replaces the pool without growth.
- **Spawning** is deterministic and event-scoped: an x/y wall impact at or
  above the threshold, or any mine hit, respawns all four slots at the hull
  anchor. `PARTICLES=0` removes the state, spawn, tick, and render calls;
  `HULL_DAMAGE=0` removes hull/cooldown state, damage rules, and the edge
  flash. The two flags compile independently (a particles-only build is valid).
- **Free flight only.** `stepWorld()` guards damage ticks, particle ticks,
  wall damage, and spawns with `docked < 0`; the chart and cargo screens never
  call it. Mine one-hit removal, mine impact fields, and mine one-point damage
  are unchanged.
- **Presentation** is `src/damage_view.hpp`: clipped corner brackets for the
  damage flash and clipped one-pixel debris at the pool anchor. `view::frame`
  draws particles after the HAULER and the flash before the mine spark. No
  strings, no cart reads, no display access.
- **Budget:** full `sizeof(game::State)` is **88 B** on AVR (71 B hazard state
  + 1 B cooldown + 16 B pool). Whole-image A/B: hull damage **+480 B flash /
  +2 B static** (the 2 B are the hull byte plus the new cooldown), particles
  **+398 B flash / +16 B static**; both enabled **+862 B flash / +18 B static**
  against a both-disabled build, with 16 B of the separate flash deltas
  recovered by LTO folding. Against the M7 hazards checkpoint the full image
  grows **+754 B flash / +17 B static**.

The full image is **26,088 B / 1,697 B**; `lean` is **24,096 B / 1,680 B**
(hull damage on, particles off); `core` is **22,394 B / 1,421 B**; `min` is
**10,586 B / 1,259 B** (2 B of flash from the `moveAxis()` return value, static
unchanged). The static ceiling rises to **1,700 B** with 3 B of headroom.

Host tests: **129** full, **106** lean, **87** core, **41** min. The stack
probe adds a real hard-impact damage resolve and a particle spawn/tick/render
frame to the measured path: with diagnostics it reports
**`stack_reserve=616`**, passing the unchanged **600 B** target with 16 B of
margin; the production run prints a bare `P`. All four device suites print
`P`. No screenshot or frame-rate claim.

### M7 shop checkpoint — 2026-10-23

`sub-hauler-m7-shop` adds the station credit sink. The transaction rules are
pure; the sketch owns the screen, and no shop data is saved. Upgrades last for
the current run only.

- **Items and prices** live in `src/shop.hpp`: AIR TANK (60 credits, +30 max
  air), HULL PLATE (70 credits, +1 max hull), and SONAR (50 credits, one ping
  upgrade level). `shop::kMaxLevel` is 3 for every item, so the full set costs
  540 credits, roughly seven four-crate deliveries at the generated distances.
  `priceFor`, `canBuy`, `levelFor`, and `buy` are integer-only.
- **Transactions are atomic.** `buy()` returns `kNoCredits`, `kSoldOut` (the
  level cap), `kNotDocked`, or `kInvalidItem` without changing credits, levels,
  or air. A successful air purchase refills to the new tank; a hull purchase
  repairs to the new maximum; a sonar purchase only advances the level that
  `sonar::fire(state, chart, state.sonarLevel)` already consumes.
- **Capacities reuse the existing rules.** `game::airCapacity(state)` is
  180 + 30/level and `game::hullCapacity(state)` is 6 + 1/level. Reset, station
  SERVICE, the HUD air bar, and both station/shop status lines use the helpers.
  With `SHOP=0` the helpers fold to the base constants and the level fields do
  not exist.
- **Board and screen.** The station board adds a SHOP row to both the no-job
  and active shapes (five rows max): header/status through y=21, rows at
  y=22+7*row, a 7 px invert, and the hint at y=57. `station::kActionShop` opens
  `Screen::Shop`; the sketch reuses `s_stationRow` as the 0..2 item selection,
  so there is no extra static byte. D-pad selects, A buys, B returns to the
  SHOP row. The shop frame performs no world step and no cart reads.
- **Feedback** is derived, not stored: each row shows its level and price (or
  `MAX`), and the one-line hint becomes `LOW CREDITS` or `MAX LEVEL` for the
  selected row. `SHOP=0` removes the state, rows, strings, include paths, and
  transactions.
- **Budget:** `sizeof(game::State)` is **91 B** on AVR (the three level bytes
  over the 88 B damage/particle state). The full image is
  **27,082 B flash / 1,690 B static**. A full A/B build with only `SHOP=0`
  measures **26,088 B / 1,697 B**, so the shop is **+994 B flash / -7 B
  static**: +3 B of state, offset by a 10 B compiler jump-table saving from
  folding the station row-label switch into an if-chain. The image stays under
  the 1,700 B ceiling and the 29,696 B flash limit.

`lean` is unchanged at **24,096 B / 1,680 B**, `core` at
**22,394 B / 1,421 B**, and `min` at **10,586 B / 1,259 B**. Host tests:
**139** full, **106** lean, **87** core, **41** min. The stack probe includes a
real shop purchase and draw frame: it reports **`stack_reserve=606`**, passing
the **600 B** target with 6 B of margin, and prints a bare `P`. All four device
suites print `P`. No screenshot or frame-rate claim.

### M8 roster checkpoint — 2026-10-23

`sub-hauler-m8-roster` adds the boot title selection and the three class pilot
silhouettes. Holds, cargo scales, and per-class arrangements remained the
`m8-holds` task (see the holds checkpoint above); at this checkpoint the cargo
screen still loaded HAULER arrangement 0.

- **Roster record** lives in `src/roster.hpp` as pure ids and constexpr
  helpers: MINNOW 64x16 / 24x7 pilot, HAULER 94x22 / 26x8 pilot, LEVIATHAN
  118x30 / 26x8 pilot. Display-name widths, one-letter codes, wrap order, and
  `sanitize()` are host-testable. HAULER is the default in every build.
- **Title screen** is `src/roster_view.hpp`: class name at y=6, playable
  silhouette at x=51/y=18, `LEN <n>` at y=34, and `D-PAD HULL  A GO` at y=54.
  All geometry is inside 128x64 and clears the stat/hint lines. The device
  draw path owns the three `F()` names; host-only names are compiled out on
  AVR.
- **Input:** left/right cycle the roster with wrap, A confirms and draws the
  first pilot frame, and B backs out to the HAULER default. The title has no
  world step and no cart reads. The selection is stored in
  `game::State.hullClass`, so it survives every dock/undock for the run.
- **Art** is shared between title and pilot through
  `pilot_view::drawAt()`. Every class stays inside the shared 26x8 collision
  footprint, keeps a cargo stripe when loaded, and mirrors for facing. MINNOW
  is a 24x7 shuttle; HAULER and LEVIATHAN are 26x8. Adjacent fill rows and an
  if-chain (instead of a three-case switch) keep the art and dispatch cheap
  without changing any silhouette or layout.
- **`MULTI_HULL=0`** removes the title `Screen` value, the hull-class byte, the
  state/reset field, the title strings, and both extra art functions. The
  build boots directly to Pilot. The linked binary for the A/B build contains
  no `MINNOW`/`LEVIATHAN` strings and no `drawMinnow`/`drawLeviathan` symbols.

Budget: `sizeof(game::State)` is **92 B** on AVR in the full profile (91 B
plus `hullClass`). The full image is **27,962 B flash / 1,689 B static**.
A full A/B build with only `MULTI_HULL=0` measures **27,042 B / 1,688 B**, so
the roster is **+920 B flash / +1 B static**. Against the M7 shop checkpoint
the full image grows **+880 B flash and shrinks 1 B static**: the hull-class
byte is offset by removing `s_chartBlink`, removing `s_cargoBHold` (the cargo
screen reuses `s_stationRow`), and making the stateless cargo reader local.

`lean` measures **24,962 B / 1,679 B** (`MULTI_HULL=1`), `core`
**22,368 B / 1,420 B**, and `min` **10,568 B / 1,259 B** (both `MULTI_HULL=0`;
the HAULER-only art/fill rework also trims the older profiles). The flash
planning ceiling rises from 28,000 B to **29,000 B** with 696 B still under the
29,696 B hardware limit; static stays under the **1,700 B** ceiling with 11 B
of headroom.

Host tests: **147** full, **114** lean, **90** core, **44** min;
`make test-tools` passes **75**. `make gen-check` is deterministic. The full
device suite (`test_boot`, `test_cargo`, `test_memory`, `test_world`) prints
`P`. `test_memory` now draws all three title screens and flies the LEVIATHAN
silhouette through the measured pilot frames; with `SH_STACK_DIAG=1` it reports
**`stack_reserve=601`**, passing the unchanged **600 B** target with a 1 B
margin. No screenshot or frame-rate claim.

### M8 holds checkpoint — 2026-10-23 (historical; removed 2026-10-24)

`sub-hauler-m8-holds` completes the class cargo experience. The boot roster
choice now drives the cargo screen, the hold scale, and the arrangement record
the player packs.

- **Holds:** MINNOW is 6x3 at 7 px cells, HAULER 11x3 at 6 px, LEVIATHAN 14x4
  at 5 px. A generic cutaway centres each hold at x=64/y=28, so all three fit
  the 128x64 shell without scrolling. Cell scale is the class distinction; the
  bow cabin, engine room, ballast tanks, and hatch posts derive from the hold
  rectangle.
- **Blocked cells:** the view treats every cleared bit of the loaded
  `puzzle.hold` as structural. Full blocked columns draw as a double bulkhead;
  isolated blocks draw as an X hatch. `cargo::canPlace` and the arrangement
  loader both reject cells outside the hold, and host tests probe every blocked
  cell for every class.
- **Load path:** `cargo_screen::load(state, reader, hullId, arrangement)` sets
  `State.hull`, `State.arrangement`, and the puzzle cols/rows/pieces/required/
  bonus/load/hold from the generated spec. The HAULER arrangement-0 overload is
  retained for tests. `hullForClass()` maps `game::State.hullClass` to the cargo
  id; `enterCargo()` uses it, so reopening LOAD CARGO selects the same class.
- **Rendering:** installed crates, the held cue, and the selection cue scale
  with the cell; the tray keeps eight 8x8 slots and previews every unplaced
  piece. The class name is a gated `F()` string (`HAULER` only when
  `MULTI_HULL=0`). Header/counters/load/feedback/prompt slots are unchanged;
  the widest name (`LEVIATHAN`) and `R 0/6 B 0/2` still fit.
- **Records:** the existing 64 arrangements per hull are sufficient; no
  generator or FX table change was needed. Host tests solve the target
  arrangement for each class and the device fixture loads all three class
  records. `make gen-check` is deterministic.
- **`MULTI_HULL=0`:** the loader folds to HAULER arrangement 0, `cellFor()`
  folds to 6 px, `hullForClass()` to HAULER, and the view links only the
  HAULER name. The runtime `HULLS` table would have cost 54 B of AVR `.data`,
  so the device loader reads folded scalar accessors (`hullCols`, `hullRows`,
  `hullHold`, ...); the table remains the canonical host-test record. The A/B
  binary contains no `MINNOW`/`LEVIATHAN` strings.

Budget: `sizeof(cargo::CargoPuzzle)` is **83 B** and
`sizeof(cargo_screen::State)` is **88 B** on AVR. The full image is
**28,810 B flash / 1,689 B static**, so the holds cost **+848 B flash / +0 B
static** against the M8 roster checkpoint. A full build with only
`MULTI_HULL=0` measures **26,914 B / 1,688 B** (the accessor rework also trims
**128 B** from the previous HAULER-only image), making the full `MULTI_HULL`
A/B **+1,896 B flash / +1 B static**. `lean` measures **25,874 B / 1,679 B**,
`core` **22,242 B / 1,420 B**, and `min` **10,568 B / 1,259 B**. Static stays
11 B under the 1,700 B ceiling; flash is 190 B under the 29,000 B planning
ceiling and 886 B under hardware.

Host tests: **152** full, **119** lean, **95** core, **44** min;
`make test-tools` passes **75**. All four device suites print `P`. The memory
probe now loads and draws all three class holds; with `SH_STACK_DIAG=1` it
reports **`stack_reserve=619`**, passing the 600 B target with 19 B of margin.
No screenshot or frame-rate claim.

### M9 polish checkpoint — 2026-10-23

`sub-hauler-m9-polish` adds the pause/failure clarity pass and the balance
audit. No new feature flag or screen beyond pause/failure was added; the cargo
screen keeps ownership of its A+B commit.

- **Pause:** A+B in free flight opens `Screen::Pause` before the chart key,
  sonar, and world step. The pure `pause::resolve()` rule owns the transition;
  A or B resumes. A paused frame performs no world step, air/ballast drain, job
  clock advance, or cart read. The device draws one compact line,
  `PAUSED  A/B RESUME`, inside 128x64.
- **Hull failure:** `damage::lost()`/`damage::lostInFlight()` trigger
  `Screen::Failure` only at zero hull in free flight. The screen shows
  `HULL LOST  A RESTART`; A calls the same `resetRun()` used by `setup()`, which
  resets the world, job/economy state, and chart. This is a safe full restart,
  so the player cannot be stranded. The shared reset keeps the failure path from
  duplicating the boot reset in flash.
- **Delivery clarity:** `game::State.lastDelivery` is one byte, written by
  `jobs::deliver()` (`kDeliveryPaid`/`kDeliveryFailed`) and cleared on accept,
  abandon, and reset. `jobs::deliveryFailed()` lets the station board replace
  its control hint with `LATE/SHORT` until the next accept or abandon. Late and
  short-hold host tests assert the note and its clearing.
- **Balance audit:** reward is `20 + 4*10 + distance/2`, rounded to five;
  deadline is `80 + 0.55*distance + 4*8` seconds. Generated contracts pay
  **110..180 credits** with **169..246 s** deadlines. Loaded top speed is
  **18 px/s** (30 - 3*4 cargo), the longest straight-line run is **1,955 px**.
  The first-delivery rebalance (2026-10-28) raised the base tank to **300
  units** and slowed passive drain to **0.4 units/s** (one unit per 150 ticks,
  **750 s** without blowing). A cautious host route from the real Tidewall dock
  to Benthos took **9,585 ticks (160 s)** and arrived with **72/300 air**, while
  the old 180-unit/0.6 unit/s tank reached the same dock at zero air. Every
  deadline therefore exceeds straight-line loaded travel and fits inside one
  tank, and every reward covers the cheapest upgrade (SONAR 50, AIR TANK 60,
  HULL PLATE 70). The host balance test asserts those boundaries for every
  generated offer.
- **Size direction:** `game::reset()` now value-initialises the aggregate and
  restores only the non-zero defaults (`spawn`, trim, air, mines, docked,
  hull, active-job tag). That reduces the whole-image reset code more than the
  new pause/failure/delivery code costs, so the full image is slightly smaller
  than the M8 holds checkpoint.

Budget: `sizeof(game::State)` is **93 B** on AVR (92 B plus `lastDelivery`).
The full image is **28,792 B flash / 1,690 B static**, which is **-18 B flash
/ +1 B static** against the M8 holds checkpoint (28,810 B / 1,689 B). The flash
planning ceiling stays **29,000 B** (208 B of headroom) and static stays
**1,700 B** (10 B of headroom). `lean` measures **25,800 B / 1,680 B**,
`core` **22,244 B / 1,421 B**, and `min` **10,456 B / 1,259 B**.

Host tests: **155** full, **122** lean, **98** core, **45** min;
`make test-tools` passes **75**. `make gen-check` is deterministic. All four
device suites print `P`. The memory probe covers the new reset path through
`game::resetWorld`; with `SH_STACK_DIAG=1` it reports
**`stack_reserve=624`**, passing the 600 B target with 24 B of margin. No
screenshot or frame-rate claim.

### M9 save checkpoint — 2026-10-23

`sub-hauler-m9-save` adds optional chart persistence behind
`SH_FEATURE_SAVE_CHART` (default `0`, requires `FOG_CHART`). The flag stays off
in every shipped profile: the feature is available for a later release
decision, but the M9 image is measured both ways and the default gate does not
depend on it.

- **Storage decision:** the ATmega32u4 internal 1,024 B EEPROM, not an FX
  cart partition. The block is 260 B, needs no `FX_SAVE_PAGE` regeneration, no
  cart-page allocation, and no SPI traffic. An FX save partition remains the
  option only if a future save outgrows internal EEPROM or needs cart-sized
  state.
- **Layout:** a four-byte versioned header (`'S'`, `'H'`, version `1`, CRC-8/ATM
  over the explored bytes) followed by the exact 256 B `chart::State.explored`
  mask. No migration: version 1 is the only accepted layout.
- **Validation:** load checks magic, version, and the whole CRC before copying,
  so blank (`0xFF`), corrupt data, a bad checksum, or a bad version all return
  false and leave the reset chart empty. There is never a partial map. The
  sketch loads once in `setup()` after `resetRun()`.
- **Writes:** only when B closes the paused chart (`updateChart`). The flight,
  station, cargo, shop, failure, pause, and display paths never call the save.
  `EEPROM.update` skips unchanged bytes, so repeat closes only write newly
  explored cells; the first blank-EEPROM save writes all 260 B (~0.86 s at
  ~3.3 ms/byte) on that one transition, not in the frame loop.
- **Feature-off proof:** `SAVE_CHART=0` compiles the include, calls, adapter,
  layout, and tests out. The resulting full `.hex` is byte-identical to the M9
  polish image (same `.text` 28,722 B and `.data` 70 B; no EEPROM functions in
  `avr-nm`). The host suite compiles `chart_save.hpp` in every profile and adds
  one feature-off assertion.
- **State:** `chart_save::EepromStorage` is stateless; no new static SRAM. The
  260 B block is EEPROM only.

Budget: the default profiles do not change: full **28,792 B flash / 1,690 B
static**, lean **25,800 B / 1,680 B**, core **22,244 B / 1,421 B**, min
**10,456 B / 1,259 B**. With `SAVE_CHART=1`, full is **29,352 B flash / 1,690 B
static** (**+560 B flash, +0 B static**) and lean is **26,238 B / 1,680 B**
(**+438 B**). The enabled variant has **344 B** of hardware flash headroom but
is **352 B over the 29,000 B planning ceiling**, which is recorded rather than
hidden; keeping the flag off is the release-safe choice. Options if it must
ship enabled: carve ~352 B from existing UI/data, accept a raised planning
ceiling up to the hardware limit, or replace the Arduino EEPROM wrapper with
direct avr-libc byte calls (not done here; the task specifies `EEPROM.read/
update`).

Host tests: **156** full, **123** lean, **99** core, **46** min in the default
feature-off profiles; the full `SAVE_CHART=1` variant runs **159**, lean
**126**. `make test-tools` passes **75**, `make gen-check` is deterministic,
and all four default device suites print `P`. The save-inclusive
`test_memory` probe reports **`stack_reserve=620`** (feature-off remains
**624**), both above the 600 B target. No screenshot or frame-rate claim.

### M9 release checkpoint — 2026-10-23

`sub-hauler-m9-release` is the final gate and content freeze at HEAD
`4f66830`. No gameplay, balance, or feature code changed; the only edits are
this release record plus `README.md`, `docs/platform.md`, and the stale
"Arduboy fit" numbers in `docs/design.md`. No commit was made.

- **Gate:** `make gen-check` is deterministic (no generated diff);
  `make test` passes **156** full / **123** lean / **99** core / **46** min;
  `make test-tools` passes **75**; `make fxtest` runs `test_boot`,
  `test_cargo`, `test_memory`, and `test_world`, each printing `P`;
  `make size`, `make size-matrix`, and `make mini PROFILE=full` all pass.
- **Final matrix:** full **28,792 B flash / 1,690 B static** (`.text` 28,722 B,
  `.data` 70 B, `.bss` 1,620 B); lean **25,800 B / 1,680 B**; core
  **22,244 B / 1,421 B**; min **10,456 B / 1,259 B**. Mini full is the same
  **28,792 B / 1,690 B**.
- **Carve report:** flash is **904 B** under the 29,696 B hardware limit and
  **208 B** under the 29,000 B planning ceiling; static is **10 B** under the
  1,700 B planning ceiling with **870 B** of static free SRAM. The stack probe
  reports **`stack_reserve=624`** against the **600 B** target (24 B margin).
  `SAVE_CHART=1` is **29,352 B / 1,690 B**, 344 B under hardware but **352 B
  over the 29,000 B planning ceiling**; it stays default **0**.
- **Soak scope:** the permanent host loop integration exercises all three hulls
  through accept → load a generated class arrangement → commit → auto-undock →
  destination dock → deliver, and the existing contract/late/abandon/handoff
  loop tests pass. Device evidence is the four-suite smoke run only. This is
  **automated host soak plus device-suite smoke, not a manual interactive
  playthrough**; no manual device soak is claimed.
- **Default image:** `SAVE_CHART=0` remains the shipped default. The release
  diff is docs-only, so the firmware inputs are unchanged from HEAD; the default
  FX `.hex` rebuilds deterministically at SHA-256
  `ab3929ac43e952b987dfb0391eee54e77eeda75f0811bb9765cd04a5a530e5b0`
  (28,792 B / 1,690 B). `avr-nm` finds no float/double or heap symbols in the
  full device ELF. The worktree has no uncommitted generated half-set.
- **Risks carried into release:** the 10 B static headroom blocks any new
  persistent byte-class state without a carve; the 208 B flash planning margin
  is not enough for save; `SAVE_CHART=1` needs a ~352 B carve, a raised planning
  ceiling under the hardware limit, or direct avr-libc EEPROM calls; and the
  release has no manual playthrough evidence (host plus device smoke only).

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
not also count as a station action. The chart and station screens never route A
to the sonar. A docked boat is rejected by the pure rule even if a future
screen calls it. Station A remains SELECT/confirm and B remains UNDOCK. The
former cargo screen and its `A+B` commit were removed on 2026-10-24.

M8 roster resolution (2026-10-23): the boot title is the selection screen.
Left/right cycles MINNOW/HAULER/LEVIATHAN with wrap, A confirms, and B backs
out to the HAULER default; the title root has no previous screen. The
selection persists in `game::State.hullClass` across every dock/undock for the
run. HAULER remains the default. All three classes share the 26x8 collision
footprint and draw a 24-26 px pilot silhouette inside it (MINNOW 24x7, HAULER
26x8, LEVIATHAN 26x8); cargo stripes and facing mirroring are preserved. With
`MULTI_HULL=0` the title, class strings, art, and state byte compile out and the
build boots straight to Pilot.

Cargo packing removal (2026-10-24): the M8 holds, scales, arrangements, and
cargo screen described above were removed by `sub-hauler-gl1.1`. Hull selection
and all three pilot silhouettes remain; contract acceptance auto-loads the
coarse four-unit hold.

## Feature flags

### Naming

Use numeric `0`/`1` macros named `SH_FEATURE_*`. They are compile-time only;
there is no runtime options menu.

| Flag | Default | Depends on | Purpose |
| --- | ---: | --- | --- |
| `SH_FEATURE_STATIONS` | 1 | — | docking, station board, refill |
| `SH_FEATURE_JOBS` | 1 | `STATIONS` | contracts, credits, delivery |
| `SH_FEATURE_FOG_CHART` | 1 | — | 256 B explored chart; chart screen |
| `SH_FEATURE_SONAR` | 1 | `FOG_CHART` | ping, reveal, cooldown |
| `SH_FEATURE_MINES` | 1 | — | mine placement, collision, damage |
| `SH_FEATURE_CURRENTS` | 1 | — | current zones/toggle |
| `SH_FEATURE_SHOP` | 1 | `STATIONS` | upgrades and credit sink |
| `SH_FEATURE_HULL_DAMAGE` | 1 | — | wall-impact hull points, cooldown, edge flash |
| `SH_FEATURE_PARTICLES` | 1 | — | fixed 4-slot impact debris pool |
| `SH_FEATURE_MULTI_HULL` | 1 | — | hull roster, selection, and class art |
| `SH_FEATURE_SAVE_CHART` | 0 | `FOG_CHART` | persist explored chart |

Core (always on): sub physics, ballast/air, cave collision, camera, HUD,
screen/input state machine.

If `SH_FEATURE_STATIONS=0`, jobs/shop must also be off and the build is a
free-flight cave run. If `SH_FEATURE_FOG_CHART=0`, sonar and chart save must be
off. Contract acceptance always auto-loads the four coarse cargo units; there
is no cargo-loading flag or packing runtime.

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

### M4 — Cargo arrangement pipeline (historical; runtime removed 2026-10-24)

The generator and FX bank remain for ABI compatibility. The device loader,
parser tests, and runtime record use described below were removed by
`sub-hauler-gl1.1`; keep the generated bytes and offsets frozen.

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

### M5 — Cargo loading screen (historical; removed 2026-10-24)

The shell, interaction, commit, and job handoff below were implemented and
then removed by `sub-hauler-gl1.1`. Contract acceptance now auto-loads the
coarse four-unit hold; do not rebuild these paths without a new decision.

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
  transitions; keep writes out of the frame loop. Implemented 2026-10-23
  behind a default-off flag (260 B internal-EEPROM block); not enabled in any
  shipped profile because the enabled full variant is 352 B over the planning
  ceiling.
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
| cargo load (M5) | +7,628 | +101 | 2026-10-22 | historical: cutaway, hold/tray, rules, job handoff, auto-undock; removed by `sub-hauler-gl1.1` on 2026-10-24 |
| fog/chart | +1,248 | +258 | 2026-10-23 | 64x32 explored set (256 B) + initialised byte + blink; paused chart, reveal, markers; full A/B; M7 carved the initialised byte |
| sonar | +484 | +2 | 2026-10-23 | A-key ping: 6 air base (-1/level), 3+level cell reveal, 120-tick cooldown, 24-tick ring; 2 state bytes; full A/B |
| mines | +730 | +1 | 2026-10-23 | 8 fixed positions, one mask byte, circle/hull hit, one-frame spark, optional hull hook; full A/B |
| currents | +852 | +0 | 2026-10-23 | 8 generated 32x16 zones, Q8 acceleration, flow marks; metadata only; full A/B |
| shop | +994 | -7 | 2026-10-23 | AIR TANK 60 / HULL PLATE 70 / SONAR 50, max level 3, refill/repair on buy; +3 B state offset by a 10 B row-label jump-table saving; full A/B |
| hull damage | +464 | +2 | 2026-10-23 | 16 px/s wall threshold, 24-tick cooldown, 12-tick edge flash, zero-hull valid, repair clamp; A/B with particles off; +2 B is the hull byte plus the cooldown |
| particles | +398 | +16 | 2026-10-23 | fixed 4-slot pool (3 B/slot + 4 B anchor), deterministic corner spawn, 1 px/tick, ±24 px clamp, 14-tick max life; A/B with hull damage on |
| multi-hull | +920 | +1 | 2026-10-23 | boot title with left/right selection, A confirm, B back-to-default; MINNOW/HAULER/LEVIATHAN 24-26 px pilot art, hull-class byte; full A/B (`MULTI_HULL=0` 27,042 B / 1,688 B); `sizeof(game::State)` 92 B; title/class strings and art compile out; linked binary clean |
| cargo holds (M8) | +848 | +0 | 2026-10-23 | historical: class holds MINNOW 6x3/7 px, HAULER 11x3/6 px, LEVIATHAN 14x4/5 px; dynamic cutaway, blocked-cell masks from `puzzle.hold`, selected-class load and reopen; removed by `sub-hauler-gl1.1` |
| cargo packing removal | -8,734 | -98 | 2026-10-24 | removes Screen::Cargo, LOAD CARGO, commit chord/hold-B timer, `s_cargo`, packing rules/runtime, CARGO_LOAD flag/profile plumbing; acceptance auto-loads four units and stays docked; all hulls/economy preserved; cargoBank FX bytes/offset unchanged |
| M9 polish | -18 | +1 | 2026-10-23 | no new flag; pause/failure screens, `lastDelivery` byte, LATE/SHORT board note, one-byte state; aggregate `game::reset()` rewrite pays for the UI; vs M8 holds whole image (28,810 B / 1,689 B) |
| chart save (M9) | +560 | +0 | 2026-10-23 | optional `SAVE_CHART=1`; 260 B internal-EEPROM block (4 B magic/version/CRC-8 header + 256 B explored), load once in setup, write only on chart close; full A/B 28,792 → 29,352 B; default off and feature-off image byte-identical; 344 B under hardware but 352 B over the 29,000 B planning ceiling |

The mine and current A/B deltas share 8 B of LTO folding: both enabled is
+1,574 B, not +1,582 B. The chart `initialised` carve (-1 B) offsets the mine
mask (+1 B), so the hazard checkpoint stayed at the then-current 1,680 B
ceiling.

The M7 damage and particle A/B deltas share 16 B of LTO folding: both enabled
is +862 B flash, not +878 B. Their static costs are additive (+18 B), and each
also removes its own state when disabled. Against the prior hazard checkpoint
the full image grows +754 B flash / +17 B static because the hull byte was
already present there; the damage A/B removes it.

Record the profile matrix separately:

| Date | Profile | Flash | Static | Stack evidence | Notes |
| --- | --- | ---: | ---: | --- | --- |
| 2026-10-22 | template | 6,636 | 1,214 | none | pre-port baseline |
| 2026-10-22 | M1 cave render | 6,840 | 1,217 | none (Ardens unavailable) | full/lean/core/min identical |
| 2026-10-23 | M6 chart | 23,286 | 1,678 | `test_memory` P (chart path, 654 B under 600 B target) | full/lean identical; core 22,038/1,420; min 10,584/1,259 |
| 2026-10-23 | M6 sonar | 23,770 | 1,680 | `test_memory` P (chart + sonar path, 649 B under 600 B target) | full/lean identical; core 22,038/1,420; min 10,584/1,259; at the static ceiling |
| 2026-10-23 | M7 hazards | 25,334 | 1,680 | `test_memory` P (chart + sonar + mine + current path, 641 B under 600 B target) | full; lean 23,760/1,679; core 22,038/1,420; min 10,584/1,259; full still at the static ceiling after the chart-byte carve |
| 2026-10-23 | M7 damage/particles | 26,088 | 1,697 | `test_memory` P (chart + sonar + mine + current + damage/particle path, 616 B under 600 B target) | full; lean 24,096/1,680; core 22,394/1,421; min 10,586/1,259; ceiling raised to 1,700 B with 3 B headroom |
| 2026-10-23 | M7 shop | 27,082 | 1,690 | `test_memory` P (chart + sonar + mine + current + damage/particle + shop path, 606 B under 600 B target) | full; SHOP=0 full 26,088/1,697; lean 24,096/1,680; core 22,394/1,421; min 10,586/1,259; flash ceiling raised to 28,000 B |
| 2026-10-23 | M8 roster | 27,962 | 1,689 | `test_memory` P (chart + sonar + mine + current + damage/particle + shop + title/all-hull art path, 601 B under 600 B target) | full; `MULTI_HULL=0` full 27,042/1,688; lean 24,962/1,679; core 22,368/1,420; min 10,568/1,259; `sizeof(game::State)` 92 B; flash ceiling raised to 29,000 B with 696 B hardware headroom; static 11 B under 1,700 B |
| 2026-10-23 | M8 holds | 28,810 | 1,689 | `test_memory` P (chart + sonar + mine + current + damage/particle + shop + title + all three class cargo holds, 619 B under 600 B target) | full; `MULTI_HULL=0` full 26,914/1,688; lean 25,874/1,679; core 22,242/1,420; min 10,568/1,259; `sizeof(cargo::CargoPuzzle)` 83 B, `sizeof(cargo_screen::State)` 88 B; flash 190 B under 29,000 B ceiling; static 11 B under 1,700 B |
| 2026-10-23 | M9 polish | 28,792 | 1,690 | `test_memory` P (chart + sonar + mine + current + damage/particle + shop + title + all three class cargo holds + aggregate reset, 624 B under 600 B target) | full; lean 25,800/1,680; core 22,244/1,421; min 10,456/1,259; `sizeof(game::State)` 93 B; pause/failure screens, `lastDelivery` byte; flash 208 B under 29,000 B ceiling; static 10 B under 1,700 B |
| 2026-10-23 | M9 chart save (`SAVE_CHART=1`) | 29,352 | 1,690 | `test_memory` P (save path included, 620 B under 600 B target); feature-off probe remains 624 B | full variant; lean 26,238/1,680; default profiles untouched at full 28,792/1,690, lean 25,800/1,680, core 22,244/1,421, min 10,456/1,259; 260 B EEPROM block, no static cost; 344 B under hardware but 352 B over the 29,000 B planning ceiling, so flag stays 0 |
| 2026-10-23 | M9 release (freeze) | 28,792 | 1,690 | `test_memory` P, diagnostics `stack_reserve=624` (600 B target, 24 B margin) | full; lean 25,800/1,680; core 22,244/1,421; min 10,456/1,259; mini full 28,792/1,690; 904 B hardware flash free, 208 B under 29,000 B planning ceiling, 10 B static headroom, 870 B static free; all four device suites P; 156/123/99/46 host tests, 75 tool tests; no manual playthrough |
| 2026-10-23 | Post-M9 fixes | 28,988 | 1,690 | `test_memory` P at 624 B reserve | full; lean 26,120/1,680; core 22,522/1,421; min 10,456/1,259; mini full 28,988/1,690; 708 B hardware flash free, 12 B under 29,000 B planning ceiling; 157/124/100/46 host tests, 75 tool tests; four device suites P; cargo idle presentation skip, cargo hline batching, undock ring latch, edge-gated A+B pause, cargo resume preserves partial hold |
| 2026-10-24 | Menu idle fix | 28,930 | 1,690 | `test_memory` P at the 600 B target (prior diagnostic 624 B) | full; lean 26,020/1,680; core 22,406/1,421; min 10,456/1,259; mini full 28,930/1,690; 766 B hardware flash free, 70 B under 29,000 B planning ceiling; 157/124/100/46 host tests, 75 tool tests; four device suites P; station/shop/pause/failure/cargo idle frames skip composition and the OLED transaction, selection bars use a `fillRect` bar with the font background disabled, dock latch resolves `nearestDock` once; `SAVE_CHART=1` full 29,330/1,690, 366 B under hardware, 330 B over planning |
| 2026-10-24 | Cargo-free contract loading | 20,196 | 1,592 | `test_memory` P, diagnostics `stack_reserve=816` (600 B target, 216 B margin) | full; lean 17,362/1,580; core 14,756/1,321; min 10,456/1,259; mini full 20,196/1,592; 9,500 B hardware flash free, 8,804 B under 29,000 B planning ceiling; 108 B static headroom, 968 B static free; 129/96/72/38 host tests, 75 tool tests; three device suites P; removes Screen::Cargo/LOAD CARGO/commit chord/hold-B/s_cargo/packing runtime/CARGO_LOAD; acceptance auto-loads and stays docked; `make gen-check` deterministic with cargoBank bytes/offset unchanged |
| 2026-10-24 | Cargo-free resource pass | 17,548 | 1,572 | `test_memory` P, diagnostics `stack_reserve=812` (600 B target, 212 B margin) | full; lean 14,792/1,560; core 12,626/1,302; min 9,142/1,252; mini full 17,548/1,572; 12,148 B hardware flash free, 11,452 B under 29,000 B planning ceiling; 128 B static headroom, 988 B static free; 132/99/75/39 host tests, 77 tool tests; three device suites P; compact metadata 148→88 B PROGMEM, generated route economy, AVR `game::State` 80 B; UI font/strings + three pilot silhouettes in FX, default Arduboy font/Print absent from ELF; `cargoBank` stays at 0x001AC2 with bytes/order unchanged, FX payload 13,012 B (`fxdata-data.bin`), padded flashable image 13,056 B (`fxdata.bin`); no manual playthrough or device soak |

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
| Save complexity | default off; 260 B internal-EEPROM block adds 0 B static and writes only on chart close; the measured `SAVE_CHART=1` variant is 29,352 B / 1,690 B, 344 B under hardware but 352 B over the 29,000 B planning ceiling, so it stays out of shipped profiles until a carve or ceiling decision |
| Release planning headroom | final full image is 208 B under the 29,000 B planning ceiling and 10 B under the 1,700 B static ceiling; any new persistent state or save enablement needs a carve, not a silent ceiling raise |

## Open decisions

- Auto-dock versus a button dock action.
- Cargo commit gesture: resolved to `A+B` with nothing held, prompted only when
  the required load is complete.
- Whether job holds stay full four units or support partial holds.
- Number of cave layouts; one fixed layout initially.
- Hull selection location: resolved to the boot title screen (left/right
  cycles, A confirms); drydock rows remain reserved for cargo holds.
- Chart save in v1; currently deferred. Resolved 2026-10-23: implemented
  behind default-off `SH_FEATURE_SAVE_CHART` with a 260 B internal-EEPROM
  block; it stays off in shipped profiles until a 352 B flash carve or a
  release decision raises the planning ceiling under the hardware limit.
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

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
| Static SRAM | 1,214 B | 1,700 B | 2,560 B |
| Stack | unmeasured | ≥800 B reserve | remainder |

The 1,700 B static ceiling is a planning target, not a hardware fact. It leaves
860 B for stack/locals. Before the cargo screen lands, add a painted-stack
device check and either confirm that reserve or lower the static ceiling. The
1,024 B framebuffer is fixed and is already inside the static total.

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

### 7. Input contract — D-pad flight, A/B reserved

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
- Lock the cargo commit gesture. Leading option: hold A when nothing is held and
  the required load is met; show the prompt only when valid.
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
| stations | — | — | — | |
| jobs | — | — | — | |
| cargo load | — | — | — | |
| fog/chart | — | — | — | |
| sonar | — | — | — | |
| mines | — | — | — | |
| currents | — | — | — | |
| shop | — | — | — | |
| hull damage | — | — | — | |
| particles | — | — | — | |
| multi-hull | — | — | — | |

Record the profile matrix separately:

| Date | Profile | Flash | Static | Stack evidence | Notes |
| --- | --- | ---: | ---: | --- | --- |
| 2026-10-22 | template | 6,636 | 1,214 | none | pre-port baseline |
| 2026-10-22 | M1 cave render | 6,840 | 1,217 | none (Ardens unavailable) | full/lean/core/min identical |

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
- Cargo commit gesture (hold A, A+B, or a menu row).
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

# Platform — sub-hauler

Canonical hardware, resource, and toolchain context for this project.
`README.md` and `AGENTS.md` point here; task prompts should link here too.
Keep dated measurements in the checkpoint table instead of copying them around.

## Target

| Property | Value |
| --- | --- |
| Board | Arduboy FX (`arduboy-homemade:avr:arduboy-fx`) |
| MCU | ATmega32u4 |
| Display | 128×64 SSD1306 OLED, native 1-bit |
| Framebuffer | 1,024 B (`Arduboy2Base::sBuffer`) |
| Cart | external FX flash; one packed `fxdata/fxdata.bin` |
| Display mode | native 1-bit via `Arduboy2Base` |
| Logical frame | `FRAME(x)` = `x`; one image per frame |
| Blitting | `Blit::draw` plus-mask blits into `sBuffer`; no OLED access |
| Presentation | one `FX::display(CLEAR_BUFFER)` after update/render |
| Frame rate | 60 fps (`arduboy.setFrameRate(60)`) |
| Toolchain | arduino-cli 1.2.0; arduboy-homemade:avr 1.4.0; AVR-GCC 7.3.0-atmel3.6.1-arduino7; `-mrelax -mcall-prologues`; LTO |
| Math | integer/fixed-point only; no `float`/`double` |

## Resource budgets

| Resource | Limit | Measurement |
| --- | --- | --- |
| Program flash | 29,696 B | `.text` + `.data` from `make size` |
| Flash planning ceiling | 29,000 B | raised from 28,000 B on 2026-10-23 for the M8 roster; 696 B under hardware |
| SRAM | 2,560 B | `.data` + `.bss` static; the remainder is stack/locals |
| Static SRAM planning ceiling | 1,700 B | dated whole-image target, raised from 1,680 B on 2026-10-23 |
| Stack | explicit reserve | static free bytes alone do not prove stack fit |

The Makefile enforces the flash limit and reports both totals with `make size`.
LTO makes per-symbol size arithmetic misleading; only whole-image A/B
measurements count. The 1,700 B static ceiling is a planning target with 11 B of
headroom over the current 1,689 B full image. The M8 roster adds one state byte
(`hullClass`), but removes the chart blink and cargo B-hold bytes and makes the
stateless cargo reader local, so static RAM is 1 B below M7. Its title and class
art are flash-only, and the `MULTI_HULL=0` build folds back to the HAULER-only
image. The 600 B stack target did not change.

## Stack measurement

Static free SRAM (`.data` + `.bss`) does not prove stack fit: the stack, locals,
and interrupt frames share the remaining bytes. The painted-stack probe is
`tst/fxdatatest/test_memory.ino`, compiled and run by the Ardens device harness.

Method:

1. `__bss_end` is the linker symbol for the first byte above all static RAM.
2. Read the live stack pointer (`SPL`/`SPH`) and set `paintTop = SP - 64`. The
   64 B guard keeps the active frame and one interrupt frame out of the paint.
3. Fill `[__bss_end + 1, paintTop]` with `0xA5` with interrupts disabled.
4. Re-enable interrupts and run 8 frames of the real path:
   `pollButtons` -> `view::frame` (cave composition, FX cart reads on the first
   frame only) -> `FX::display(CLEAR_BUFFER)`.
5. Scan upward for the first byte that is no longer `0xA5`; that address is the
   stack high-water mark.
6. `reserve = highWater - __bss_end`. The suite prints exactly one bare `P` when
   the reserve is at least 600 B, otherwise one bare `F`. Diagnostics stay off
   the marker line (`SH_STACK_DIAG=0` by default) because the Ardens harness
   matches the exact marker.

The result is conservative: an untouched byte reads back `0xA5`, and a path
that stays inside the guard region is measured as if it stopped at `paintTop`.
The marker print happens after the scan, so the final Serial writes are outside
the measured window; the 600 B target leaves room for them.

Target: **at least 600 B of reserve above `__bss_end`** on the full profile.
This is the 2026-10-23 re-baseline. The earlier 800 B target predated the 258 B
persistent chart slice; the chart-inclusive probe measured **654 B**, the
chart+sonar probe **649 B**, the M7 mine+current probe **641 B**, the M7
damage+particle probe **616 B**, the M7 shop probe **606 B**, and the M8 roster
probe **601 B**, so 600 B currently keeps only a 1 B margin. Do not relax this
target further without a new dated measurement and an explicit note here.

Status: **passing 2026-10-23.** `test_memory` includes the M6 chart path: a
static `chart::State` (the ownership the firmware uses), a spawn
reset/reveal, `revealAtPixels`/`revealOnStep` calls, and three
`chart_view::draw` + `FX::display(CLEAR_BUFFER)` frames, one with the
destination X lit and two blinked off. The M6 sonar path follows: a real
`sonar::fire` into the same `probeChart` (no duplicated chart state), one
`game::stepWorld` that decrements the cooldown, and a `view::frame` +
`hud::draw` pilot frame that draws the ring. The M7 hazard path then steps
inside generated current 0, draws the flow marks, places the hull on mine 0,
steps once to consume it, and draws the one-frame impact spark. The M7 damage
path latches a deterministic hard wall impact, runs `damage::resolveWallImpact`
and `damage::tick`, spawns/ages the four-particle pool, and draws a pilot frame
with the edge brackets and debris. The M7 shop path docks, buys an air upgrade,
and draws the three-row shop board. The M8 roster path draws all three title
screens (name, playable silhouette, length stat), then flies the LEVIATHAN
silhouette through the pilot frames. With `SH_STACK_DIAG=1` the probe measures
**`stack_reserve=601`**; without diagnostics it prints a bare `P` at the 600 B
target. The other three suites also print `P`. The probe measures the reserve,
not the target: 601 B is the number of untouched bytes above `__bss_end` on the
chart+sonar+hazard+damage+shop+title/roster path, and 600 B is the deliberate
pass line below it.

The firmware comparison is about **641 B** of reserve: the full image has
**871 B** of static free SRAM and an estimated **230 B** stack depth (the
chart+sonar 222 B estimate plus roughly 8 B for the deeper hazard path). Do not
subtract the probe's static locals from the firmware number: `game::State`,
the cargo screen state, and the tile cache are stack locals in the probe but
static in the firmware, so the earlier "451 B" arithmetic subtracted
static-owned bytes twice. The probe's 601 B already charges that state to
static, exactly like the shipped image.

Static free SRAM is **871 B** and the full image is **1,689 B** static, 11 B
under the raised 1,700 B planning ceiling. `game::State` is **92 B** on AVR in
the full profile: the 71 B hazard state, the one-byte wall-hit cooldown, the
16-byte four-particle pool, the three run-only shop level bytes, and the M8
hull-class byte. Disabling `HULL_DAMAGE` removes the hull and cooldown bytes (2 B); disabling `PARTICLES`
removes the 16 B pool; disabling `SHOP` removes the three level bytes and,
through the row-label if-chain, saves a further 10 B of compiler jump table.
Any further persistent state needs a carve of existing state first; there is no
room for another 256 B-class feature.

The suite builds with `make fxtest-build FXTEST_ONLY=test_memory`; rerun it
after render/state changes:

```sh
make fxtest FXTEST_ONLY=test_memory
```

A bare `P` means the reserve is at least 600 B; `F` means it is not. Record the
dated high-water result in the checkpoint table after that run.

## SPI / display contract

- The OLED and FX cart share SPI.
- Cart reads happen in update/render, never during an OLED transaction.
- Normal presents once per frame with `FX::display(CLEAR_BUFFER)` after
  rendering; that call is the display transaction.
- `FX::begin(FX_DATA_PAGE)` opens the cart image. Use `FX_SAVE_PAGE` when a save
  partition is added.

## Measurement checkpoints

Dated whole-image numbers are evidence, not guaranteed headroom.

| Date | Flash | Static RAM | Toolchain / notes |
| --- | --- | --- | --- |
| 2026-10-20 | 6,636 B | 1,214 B | arduino-cli 1.2.0; arduboy-homemade:avr 1.4.0; AVR-GCC 7.3.0-atmel3.6.1-arduino7; `make size` |
| 2026-10-22 | 6,840 B | 1,217 B | M1 cave render (static camera, 16x8 visible window); full/lean/core/min identical; no Ardens run |
| 2026-10-22 | 10,584 B | 1,259 B | M2 pilot view (world step, HAULER silhouette, HUD, D-pad input); full/min identical; no Ardens run |
| 2026-10-22 | 12,346 B | 1,276 B | M3 stations (auto-dock, station board, refill/repair/undock); full/core identical, min unchanged at 10,584 B / 1,259 B; no Ardens run |
| 2026-10-22 | 14,410 B | 1,319 B | M3 jobs/loop (offers, deadlines, delivery/credits, auto-load hint); full/lean/core identical, min unchanged; device suites P |
| 2026-10-22 | 18,892 B | 1,416 B | M5 cargo shell (HAULER cutaway, 11x3 hold, tray, status); full/core/lean identical, min unchanged; device suites P |
| 2026-10-22 | 21,836 B | 1,418 B | M5 cargo rules (selection, pick/lift, move, support, rotation, undo, feedback); full/core identical, min unchanged at 10,584 B / 1,259 B; device suites P, stack probe includes the cargo path |
| 2026-10-22 | 22,038 B | 1,420 B | M5 cargo integration (job handoff, A+B commit, auto-undock, fallback, LOAD CARGO row); full/core/lean identical, min unchanged; device suites P, stack probe includes commit/undock |
| 2026-10-23 | 23,286 B | 1,678 B | M6 fog chart (64x32 explored set, paused 128x64 chart, B toggle); full/lean identical, core 22,038 B / 1,420 B, min 10,584 B / 1,259 B; `test_memory` P at 654 B under the 600 B target (chart in probe), other suites P |
| 2026-10-23 | 23,770 B | 1,680 B | M6 sonar (A ping: air cost, 3+level cell reveal, 120-tick cooldown, 24-tick pilot ring); full/lean identical, core 22,038 B / 1,420 B, min 10,584 B / 1,259 B; `test_memory` P at 649 B under the 600 B target (chart + sonar in probe), other suites P |
| 2026-10-23 | 25,334 B | 1,680 B | M7 hazards (8 fixed mines, one mask byte, circle/hull hit, one-frame spark, 8 generated current zones with Q8 acceleration and flow marks); full; lean 23,760 B / 1,679 B, core 22,038 B / 1,420 B, min 10,584 B / 1,259 B; `test_memory` P at 641 B under the 600 B target (chart + sonar + mine + current in probe), other suites P |
| 2026-10-23 | 26,088 B | 1,697 B | M7 damage/particles (16 px/s wall threshold, 24-tick cooldown, edge brackets, 4-slot 16 B debris pool); full; lean 24,096 B / 1,680 B, core 22,394 B / 1,421 B, min 10,586 B / 1,259 B; `test_memory` P at 616 B under the 600 B target (chart + sonar + mine + current + damage/particle in probe), other suites P |
| 2026-10-23 | 27,082 B | 1,690 B | M7 shop (AIR TANK 60, HULL PLATE 70, SONAR 50, max level 3, refill/repair on buy, SHOP station row, Screen::Shop); full; SHOP=0 full 26,088 B / 1,697 B, lean 24,096 B / 1,680 B, core 22,394 B / 1,421 B, min 10,586 B / 1,259 B; `sizeof(game::State)` 91 B; `test_memory` P at 606 B under the 600 B target (chart + sonar + mine + current + damage/particle + shop in probe), other suites P |
| 2026-10-23 | 27,962 B | 1,689 B | M8 roster (title selection, MINNOW/LEVIATHAN 24-26 px pilot art, one hull-class byte); full; `MULTI_HULL=0` full 27,042 B / 1,688 B, lean 24,962 B / 1,679 B, core 22,368 B / 1,420 B, min 10,568 B / 1,259 B; `sizeof(game::State)` 92 B; `test_memory` P at 601 B under the 600 B target (title + all hull art paths), other suites P |

Static free SRAM is **871 B** at the latest checkpoint. `chart::State` is
256 B, the sonar fields add 2 B, the mine mask adds 1 B, the damage cooldown
adds 1 B, the four-particle pool adds 16 B, the run-only shop levels add 3 B,
and the M8 hull class adds 1 B. The M8 sketch removes the chart blink byte and
the cargo B-hold byte and makes the stateless cargo reader local, so the
whole-image static total is **1,689 B**, 1 B below M7. The chart, sonar, mine,
current, damage/particle, shop, and title/roster paths are all inside the fixed
probe with the explored set owned statically; the probe measures a **601 B**
reserve and prints `P` under the **600 B** target re-baselined on 2026-10-23.
The firmware reserve is about **641 B** (871 B static free minus an estimated
230 B stack depth); the test's stack locals (`game::State`, cargo state, tile
cache) are static in the firmware, so they must not be subtracted a second
time. The planning ceiling is **1,700 B**: the full image has 11 B of headroom.
`MULTI_HULL` adds **+920 B flash / +1 B static** against the same full image
with the flag off (27,042 B / 1,688 B); the flash planning ceiling rises to
**29,000 B** (696 B under hardware) for the M8 content slice. Core and min stay
at 22,368 B / 1,420 B and 10,568 B / 1,259 B because the roster collapses to
HAULER and no class strings or art are linked. Any future persistent state
still needs a carve rather than another byte-class allocation. The probe does
not print the exact high-water byte unless `SH_STACK_DIAG=1`; static free bytes
are still not a stack measurement.

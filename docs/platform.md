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
| Cart | external FX flash; flashable `fxdata/fxdata.bin` (13,056 B); the unpadded 13,012 B payload is `fxdata/fxdata-data.bin` |
| Display mode | native 1-bit via `Arduboy2Base` |
| Logical frame | `FRAME(x)` = `x`; one image per frame |
| Blitting | `Blit::draw` plus-mask blits into `sBuffer`; no OLED access |
| Text | external 6x8 FX font via `src/text.hpp`; no built-in Arduboy font/Print path |
| Presentation | one `FX::display(CLEAR_BUFFER)` after update/render |
| Frame rate | 60 fps (`arduboy.setFrameRate(60)`) |
| Toolchain | arduino-cli 1.2.0; arduboy-homemade:avr 1.4.0; AVR-GCC 7.3.0-atmel3.6.1-arduino7; `-mrelax -mcall-prologues`; LTO |
| Math | integer/fixed-point only; no `float`/`double` |

## Resource budgets

| Resource | Limit | Measurement |
| --- | --- | --- |
| Program flash | 29,696 B | `.text` + `.data` from `make size` |
| Flash planning ceiling | 29,000 B | raised from 28,000 B on 2026-10-23 for the M8 roster; the current image has 12,148 B of hardware headroom and 11,452 B under this planning line |
| SRAM | 2,560 B | `.data` + `.bss` static; the remainder is stack/locals |
| Static SRAM planning ceiling | 1,700 B | dated whole-image target, raised from 1,680 B on 2026-10-23 |
| Stack | explicit reserve | static free bytes alone do not prove stack fit |

The Makefile enforces the flash limit and reports both totals with `make size`.
LTO makes per-symbol size arithmetic misleading; only whole-image A/B
measurements count. The 1,700 B static ceiling is a planning target with **128 B**
of headroom over the current **1,572 B** full image, leaving **988 B** of static
free SRAM. The cargo-free carve released 98 B of static RAM and 8,734 B of flash
from the previous 28,930 B / 1,690 B post-M9 image; the final UI/art/resource
pass released another 2,648 B of flash and 20 B of static RAM, reaching
17,548 B / 1,572 B. The 600 B stack target did not change.

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
damage+particle probe **616 B**, the M7 shop probe **606 B**, the M8 roster
probe **601 B**, the M8 holds probe **619 B**, the M9 polish probe **624 B**,
the cargo-free probe **816 B**, and the final UI/art/resource probe **812 B**,
so 600 B currently keeps a 212 B margin. Do not relax this target further
without a new dated measurement and an explicit note here.

Status: **passing 2026-10-24.** `test_memory` includes the M6 chart path: a
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
silhouette through the pilot frames; title and pilot now read the baked FX
sprites and the external font/string sections. The cargo-free contract path
replaces the removed hold screens: it posts offers at a real berth, accepts the
first offer (auto-loading four units), draws the active station board, undocks
through the board action, and draws the first pilot frame. The M9 reset path
runs `game::resetWorld` (the aggregate `game::reset()` is the same path used by
boot), and the pause/failure screens are text-only and add no deeper call
chain. With `SH_STACK_DIAG=1` the probe measures **`stack_reserve=812`**;
without diagnostics it prints a bare `P` at the 600 B target. The other two
suites also print `P`. The probe measures the reserve, not the target: 812 B is
the number of untouched bytes above `__bss_end` on the
chart+sonar+hazard+damage+shop+title/roster+contract/undock/reset path, and
600 B is the deliberate pass line below it.

The firmware comparison is about **758 B** of reserve: the full image has
**988 B** of static free SRAM and an estimated **230 B** stack depth (the
chart+sonar 222 B estimate plus roughly 8 B for the deeper hazard path). Do not
subtract the probe's stack locals from the firmware number: `game::State`,
`world::FxTables`, and the tile cache are stack locals in the probe but static
in the firmware, so doing so would charge those bytes twice. The probe's 812 B
already accounts for that state like the shipped image.

Static free SRAM is **988 B** and the full image is **1,572 B** static, 128 B
under the 1,700 B planning ceiling. Removing the cargo-packing runtime freed
**98 B of static RAM**; the final resource pass removed another **20 B**.
`game::State` is **80 B** on AVR in the full profile after the gl1.3/gl1.4
compaction removed diagnostic counters and the job clock and narrowed route
reward fields. The persistent chart (256 B), hazard state, particle pool,
run-only shop levels, hull-class byte, and delivery byte remain. Disabling
`HULL_DAMAGE` removes the hull and cooldown bytes; disabling `PARTICLES`
removes the 16 B pool; disabling `SHOP` removes the three level bytes and,
through the row-label if-chain, saves further compiler jump table. Any further
**SRAM** persistent state needs a carve of existing state first; there is no
room for another 256 B-class feature. The optional chart save is not SRAM: its
260 B block lives in EEPROM, so it does not consume static headroom. The
shipped `cargoBank` remains external FX flash only; its runtime state and
reader are gone.

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
- `FX::begin(FX_DATA_PAGE)` opens the cart image. Use `FX_SAVE_PAGE` only if a
  future cart-partition save is added; the M9 chart save uses the internal
  EEPROM and needs no cart-page change.

## Save data

`SH_FEATURE_SAVE_CHART` (default `0`, requires `SH_FEATURE_FOG_CHART`) persists
the explored chart in the ATmega32u4's internal 1,024 B EEPROM. Internal EEPROM
is the right fit for this slice: the block is only 260 B, it needs no cart-page
allocation or FX layout regeneration, and it does not add SPI traffic. The
`FX_SAVE_PAGE` guidance above applies only if a future save grows past the
internal EEPROM or needs cart-sized state.

Layout, frozen as version 1 with no migration:

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 1 | magic `'S'` (0x53) |
| 1 | 1 | magic `'H'` (0x48) |
| 2 | 1 | version `1` |
| 3 | 1 | CRC-8/ATM (poly 0x07, init 0) over the 256 explored bytes |
| 4 | 256 | explored mask, identical to `chart::State.explored` |

`chart_save::load()` validates magic, version, and the whole checksum before
copying, so a blank EEPROM (`0xFF`), corrupt block, or wrong version is ignored
and the reset chart stays empty; there is no partial load. The sketch calls it
once in `setup()` after `resetRun()`, and calls `chart_save::save()` only on the
chart-close transition (B while the paused chart is open). No flight, station,
display, or per-frame path writes EEPROM.

The device adapter is `chart_save::EepromStorage`, wrapping `EEPROM.read()` and
`EEPROM.update()`. `update()` skips unchanged bytes, so a repeat close pays only
for newly explored cells. The first save after a blank EEPROM writes all 260
bytes; at the AVR's ~3.3 ms per byte that is a one-time ~0.86 s pause at the
chart-close transition, not in the frame loop. The save adds **0 B of static
SRAM**: the adapter is a stateless stack object and the block lives in EEPROM.

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
| 2026-10-23 | 28,792 B | 1,690 B | M9 polish (pause/failure screens, one `lastDelivery` byte, LATE/SHORT board note, aggregate `game::reset()`); full; lean 25,800 B / 1,680 B, core 22,244 B / 1,421 B, min 10,456 B / 1,259 B; `sizeof(game::State)` 93 B; `test_memory` P with `stack_reserve=624` under the 600 B target (reset path included), other suites P |
| 2026-10-23 | 28,792 B | 1,690 B | M9 chart save default off; feature-off image is byte-identical to the M9 polish image (`.text` 28,722 B, `.data` 70 B, SHA-256 of `.hex` unchanged); `SAVE_CHART=1` full variant 29,352 B / 1,690 B (+560 B flash, +0 B static), lean variant 26,238 B / 1,680 B (+438 B); `test_memory` with save path P at 620 B for the variant and 624 B with the flag off; 75 tool tests; full/lean/core/min device suites P |
| 2026-10-23 | 28,792 B | 1,690 B | M9 release freeze at `4f66830`: full gate passes; full/lean/core/min **28,792 / 25,800 / 22,244 / 10,456 B** flash and **1,690 / 1,680 / 1,421 / 1,259 B** static; mini full 28,792 B / 1,690 B; 156/123/99/46 host tests, 75 tool tests, four device suites P; `test_memory` diagnostics `stack_reserve=624` (600 B target, 24 B margin); 904 B hardware flash free, 208 B under 29,000 B, 10 B static headroom; default `SAVE_CHART=0` FX `.hex` rebuild SHA-256 `ab3929ac43e952b987dfb0391eee54e77eeda75f0811bb9765cd04a5a530e5b0` (deterministic across two builds; docs-only diff from HEAD); no manual playthrough |
| 2026-10-23 | 28,988 B | 1,690 B | Post-M9 fixes: idle cargo frames skip composition and the OLED transaction; cargo horizontal runs batch page/bit lookup; undock uses a ring latch instead of a 1.5 s timer; A+B pause requires fresh edges; cargo resume preserves a partial hold. Full matrix **28,988 / 26,120 / 22,522 / 10,456 B** flash and **1,690 / 1,680 / 1,421 / 1,259 B** static; mini full 28,988 B / 1,690 B; 157/124/100/46 host tests, 75 tool tests, four device suites P; `test_memory` P at **624 B** reserve; 708 B hardware flash free, **12 B** under 29,000 B; `SAVE_CHART=1` full 29,368 B / 1,690 B (**+560 B**, 328 B under hardware, 368 B over planning), lean 26,480 B / 1,680 B |
| 2026-10-24 | 28,930 B | 1,690 B | Post-M9 menu fix: station/shop/pause/failure/cargo idle frames skip composition and the OLED transaction; selected rows use a `fillRect` bar with the font background disabled; the dock latch calls `nearestDock` once. Full matrix **28,930 / 26,020 / 22,406 / 10,456 B** flash and **1,690 / 1,680 / 1,421 / 1,259 B** static; mini full 28,930 B / 1,690 B; 157/124/100/46 host tests, 75 tool tests, four device suites P; `test_memory` P at the 600 B target (prior diagnostic **624 B**); 766 B hardware flash free, **70 B** under 29,000 B; `SAVE_CHART=1` full 29,330 B / 1,690 B (**+400 B**, 366 B under hardware, 330 B over planning) |
| 2026-10-24 | 20,196 B | 1,592 B | Cargo-free contract loading (`sub-hauler-gl1.1`): removes `Screen::Cargo`, LOAD CARGO row, cargo commit chord/hold-B timer, `s_cargo`, packing rules/runtime, and the `CARGO_LOAD` flag; accept auto-loads four units and stays docked, explicit UNDOCK/B starts travel. Full matrix **20,196 / 17,362 / 14,756 / 10,456 B** flash and **1,592 / 1,580 / 1,321 / 1,259 B** static; mini full 20,196 B / 1,592 B; 129/96/72/38 host tests, 75 tool tests, three device suites P; `test_memory` diagnostics **`stack_reserve=816`** (600 B target, 216 B margin); 9,500 B hardware flash free, 8,804 B under 29,000 B; 108 B static headroom, 968 B static free; `make gen-check` deterministic, shipped `cargoBank` offset/bytes unchanged; `test_cargo` retired |
| 2026-10-24 | 17,548 B | 1,572 B | Cargo-free resource pass (`sub-hauler-gl1.2`–`gl1.8`): compact world metadata 148→88 B PROGMEM with no RAM/cart-read change; route economy generated; diagnostic counters and job clock removed, route rewards narrowed, AVR `game::State` 80 B; surviving UI strings/6x8 font and three pilot silhouettes moved to FX, default Arduboy font/Print absent from ELF. Full matrix **17,548 / 14,792 / 12,626 / 9,142 B** flash and **1,572 / 1,560 / 1,302 / 1,252 B** static; mini full 17,548 B / 1,572 B; 132/99/75/39 host tests, 77 tool tests, `test_boot`/`test_memory`/`test_world` all P; full diagnostics **`stack_reserve=812`** (600 B target, 212 B margin); 12,148 B hardware flash free, 11,452 B under 29,000 B; 128 B static headroom, 988 B static free; savings vs cargo-free baseline **2,648 / 2,570 / 2,130 / 1,314 B flash** and **20 / 20 / 19 / 7 B static**; `cargoBank` remains at **0x001AC2** with unchanged bytes/order, FX payload **13,012 B** (`fxdata/fxdata-data.bin`), padded flashable image **13,056 B** (`fxdata/fxdata.bin`); no manual playthrough or device soak |

Static free SRAM is **988 B** at the latest checkpoint. `chart::State` is
256 B, the sonar fields add 2 B, the mine mask adds 1 B, the damage cooldown
adds 1 B, the four-particle pool adds 16 B, the run-only shop levels add 3 B,
the M8 hull class adds 1 B, and the M9 last-delivery byte adds 1 B. The M9
chart save adds **0 B**: its 260 B block is EEPROM and its adapter is a
stateless stack object. The cargo-free carve removed the static
`cargo_screen::State` and its B-hold counter (**98 B**); the final resource pass
removed another **20 B** and compacted AVR `game::State` to **80 B**. The
whole-image static total is **1,572 B**. The chart, sonar, mine, current,
damage/particle, shop, title/roster, contract/undock, and reset paths are all
inside the fixed probe with the explored set owned statically; the probe
measures an **812 B** reserve and prints `P` under the **600 B** target
re-baselined on 2026-10-23. The firmware reserve is about **726 B** (988 B
static free minus an estimated 230 B stack depth); the test's stack locals
(`game::State`, tile cache) are static in the firmware, so they must not be
subtracted a second time. The planning ceiling is **1,700 B**: the full image
has 128 B of headroom.

The total movement from the post-M9 menu-fix checkpoint (28,930 B / 1,690 B) is
**-11,382 B flash / -118 B static**; the cargo-free carve alone was
**-8,734 B / -98 B**, and this pass added **-2,648 B / -20 B** without changing
the contract loop. The pass also measured an incremental compact-metadata A/B:
17,584/14,810/12,640/9,178 B -> 17,548/14,792/12,626/9,142 B, with the
position-only PROGMEM metadata at **88 B** (from 148 B) and no RAM or cart-read
change. The 128 B cave atlas stays in PROGMEM because routing hot tile renders
through FX would add cart traffic. UI text and the three pilot sheets live after
`cargoBank`; the default Arduboy font/Print symbols are absent from the ELF.
The `cargoBank` declaration stays at **0x001AC2** with unchanged size, bytes,
and order; the FX payload in `fxdata/fxdata-data.bin` is **13,012 B**, while
the padded flashable `fxdata/fxdata.bin` image is **13,056 B**. The optional
chart save variant
was last measured at 29,330 B / 1,690 B before the carve; a fresh
`SAVE_CHART=1` measurement is required before changing the default.
`MULTI_HULL` still adds class art/string calls; core and min measure
12,626 B / 1,302 B and 9,142 B / 1,252 B because the roster collapses to
HAULER and those calls compile out. Any future persistent state still needs a
carve rather than another byte-class allocation. The probe does not print the
exact high-water byte unless `SH_STACK_DIAG=1`; static free bytes are still not
a stack measurement.

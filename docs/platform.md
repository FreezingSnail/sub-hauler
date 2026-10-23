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
| SRAM | 2,560 B | `.data` + `.bss` static; the remainder is stack/locals |
| Stack | explicit reserve | static free bytes alone do not prove stack fit |

The Makefile enforces the flash limit and reports both totals with `make size`.
LTO makes per-symbol size arithmetic misleading; only whole-image A/B
measurements count. There is no separate project ceiling yet; add one here
with a date if the project adopts one.

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
   the reserve is at least 800 B, otherwise one bare `F`. Diagnostics stay off
   the marker line (`SH_STACK_DIAG=0` by default) because the Ardens harness
   matches the exact marker.

The result is conservative: an untouched byte reads back `0xA5`, and a path
that stays inside the guard region is measured as if it stopped at `paintTop`.
The marker print happens after the scan, so the final Serial writes are outside
the measured window; the 800 B target leaves room for them.

Target: **at least 800 B of reserve above `__bss_end`** on the full profile.
If a measured reserve is below 800 B, lower the static planning ceiling in
`docs/roadmap.md` instead of relaxing the target.

Status: **passed 2026-10-22.** Ardens ran
`make fxtest FXTEST_ONLY=test_memory` and the suite printed the bare `P`
marker, so the measured reserve is at least the 800 B target. The probe now
loads a cargo arrangement and runs a few cargo input/render frames, so the
cargo screen is inside the measured window. The probe does not print the exact
high-water byte unless `SH_STACK_DIAG=1`; static free bytes are still not a
stack measurement. The suite builds with
`make fxtest-build FXTEST_ONLY=test_memory`; rerun it after render/state
changes:

```sh
make fxtest FXTEST_ONLY=test_memory
```

A bare `P` means the reserve is at least 800 B; `F` means it is not. Record the
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

Static free SRAM is 1,140 B at the latest checkpoint. The painted-stack probe
reported `P` on 2026-10-22 after the cargo commit/undock path was added to
`test_memory`, so the measured reserve still covers the M5 integration at the
800 B target. The probe does not print the exact high-water byte unless
`SH_STACK_DIAG=1`; static free bytes are still not a stack measurement.

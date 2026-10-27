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
| Cart | external FX flash; flashable `fxdata/fxdata.bin` (451,328 B); the unpadded 451,172 B payload is `fxdata/fxdata-data.bin` |
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
| Flash planning ceiling | 29,000 B | raised from 28,000 B on 2026-10-23 for the M8 roster; the current image has 9,520 B of hardware headroom and 8,824 B under this planning line |
| SRAM | 2,560 B | `.data` + `.bss` static; the remainder is stack/locals |
| Static SRAM planning ceiling | 1,700 B | dated whole-image target, raised from 1,680 B on 2026-10-23 |
| Stack | explicit reserve | static free bytes alone do not prove stack fit |

The Makefile reports whole-image totals with `make size`. LTO makes per-symbol
size arithmetic misleading. The chart optimization checkpoint (2026-10-26)
is **20,176 B flash / 1,583 B static SRAM**, leaving **9,520 B** hardware flash,
**8,824 B** under the flash planning ceiling, **117 B** static planning headroom
and **977 B** static free SRAM. It adds **44 B flash / 0 B static** against a
fresh build of the pre-change source snapshot (20,132 / 1,583 B). The stack
target remains 600 B.

## Stack measurement

Current checkpoint (2026-10-26): **`stack_reserve=756`**, 156 B above the
600 B target. The representative path includes salvage, cutter operation,
object rendering, chart gate markers and discovery HUD messages in addition
to the earlier flight/chart/sonar/hazard/shop/roster/contract paths. All six
device suites print `P`. AVR `game::State` is **91 B**, including 11 B for
run-local tools, claims, gate masks and interaction/notice state. The following
method and older probe coverage explain the measurement; dated older figures
remain historical checkpoints.

Static free SRAM (`.data` + `.bss`) does not prove stack fit: the stack, locals,
and interrupt frames share the remaining bytes. The painted-stack probe is
`tst/fxdatatest/test_memory.ino`, compiled and run by the Ardens device harness.

Method:

1. `__bss_end` is the linker symbol for the first byte above all static RAM.
2. Read the live stack pointer (`SPL`/`SPH`) and set `paintTop = SP - 64`. The
   64 B guard keeps the active frame and one interrupt frame out of the paint.
3. Fill `[__bss_end + 1, paintTop]` with `0xA5` with interrupts disabled.
4. Re-enable interrupts and run 8 frames of the real path:
   `pollButtons` -> `view::frame` (native terrain streamed from FX each frame,
   collision cache filled on the first frame) -> `FX::display(CLEAR_BUFFER)`.
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
so that checkpoint kept a 212 B margin. Do not relax this target further
without a new dated measurement and an explicit note here.

Historical probe coverage (passing 2026-10-24): `test_memory` includes the M6 chart path: a
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

At the 2026-10-24 checkpoint the firmware comparison was about **758 B** of reserve: that image had
**988 B** of static free SRAM and an estimated **230 B** stack depth (the
chart+sonar 222 B estimate plus roughly 8 B for the deeper hazard path). Do not
subtract the probe's stack locals from the firmware number: `game::State`,
`world::FxTables`, and the tile cache are stack locals in the probe but static
in the firmware, so doing so would charge those bytes twice. The probe's 812 B
already accounts for that state like the shipped image.

At the 2026-10-24 checkpoint static free SRAM was **988 B** and the image was **1,572 B** static, 128 B
under the 1,700 B planning ceiling. Removing the cargo-packing runtime freed
**98 B of static RAM**; the final resource pass removed another **20 B**.
That checkpoint used an **80 B** AVR `game::State` after the gl1.3/gl1.4
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

The active map format is documented in [levels/README.md](../levels/README.md).
Its object records are 119 B in FX and mirrored byte-for-byte in PROGMEM for
collision without extra SPI transactions or a dynamic SRAM object array.
The original 88 B position-only world metadata and 128 B cave atlas retain
their storage approach. Generated `level.hpp` is active; `world.hpp` under
`src/generated` describes the retained legacy sections.

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

The installed Arduboy2 reserves EEPROM addresses 0–15 for system settings
(`EEPROM_STORAGE_SPACE_START == 16` in its `src/Arduboy2.h`). The current
default-off chart implementation writes from address 0 and overlaps that
reservation; its documented layout below describes existing behavior, not a
safe allocation for new saves. The proposed [voyage save](progression.md)
starts after the system prefix and replaces the chart writer. That correction
and migration are design work; the current implementation is unchanged.

Record layout, with the version following `world::LAYOUT_VERSION` (currently 3):

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 1 | magic `'S'` (0x53) |
| 1 | 1 | magic `'H'` (0x48) |
| 2 | 1 | world layout version; older maps are rejected without migration |
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

| 2026-10-25 | 20,500 B | 1,583 B | Authored exploration map: 32 rooms/42 passages, 9 equipment gates, 8 unique coves; active collision/chart/object sections append after the unchanged 34,540 B prefix. Packed payload/image **41,553 / 41,728 B**. Full/lean/core/min flash **20,500 / 17,668 / 15,166 / 9,142 B**, static **1,583 / 1,571 / 1,313 / 1,252 B**; Mini full **20,500 / 1,583 B**. Full run state **91 B** (+11). Host **140/107/82/39**, optional chart-save host **143**, tooling **88**, all **4** Ardens suites P; expanded stack probe **775 B** at target600. `make gen-check` deterministic; real hull/tool reachability validated for all eight capability subsets. Tools/claims/open grates remain run-local; campaign/ending/voyage save not implemented; no manual playthrough or physical-device soak. |
| 2026-10-26 | 20,178 B | 1,583 B | First campaign slice (`sub-hauler-7px.5`): FIRST/PUMP campaign work, PUMP survey rings, WORK page, generated Pump Refuge/Lower Gantry service metadata, native restoration overlays and service-berth Continue restore. Full/lean/core/min **20,178 / 17,368 / 14,756 / 8,750 B** flash and **1,583 / 1,571 / 1,313 / 1,252 B** static; full `.text/.data/.bss` **20,122 / 56 / 1,527 B**; Mini full **20,178 / 1,583 B**; host tests **153/120/92/47**, campaign overrides **195/161/133**, DEV **158/125/97/52**, campaign DEV **200/166/138**; tooling **108**, all six Ardens suites P; `test_memory` diagnostics **`stack_reserve=756`** (600 B target). Campaign-enabled full AVR override compiles to **29,998 B / 1,660 B** but does not link: 302 B over hardware and 998 B over the 29,000 B planning line, so CAMPAIGN/SAVE_VOYAGE remain default-off until task .6 trims the combined sequence. No manual playthrough or physical-device soak. |


Chart performance checkpoint (2026-10-26, `sub-hauler-h89`): paused charts
compose on entry and at the existing 16-frame sub/destination blink boundaries
at 3.75/s while open. Other frames skip composition and
OLED transfers. The same 256 chart bytes now use eight 32-byte FX bursts and a
32-byte stack scratch page, expanding straight into all 1,024 framebuffer bytes.
Normal full-chart render time falls **10.327→4.966 ms**, with repeatable CRCs;
OLED presentation stays about **1.16 ms**. Normal/dev modes pass all six device
suites, including the new permanent chart benchmark with a 7 ms combined
render/display budget. Stack reserve is **756 B** against 600 B. Full/lean/core/min
flash is **20,176 / 17,368 / 14,756 / 8,750 B**, static **1,583 / 1,571 / 1,313 /
1,252 B**; dev **15,592 / 1,582 B**, Mini full **20,176 / 1,583 B**. A pre-change
source snapshot rebuilt with this toolchain measures **20,132 / 1,583 B** full
and **8,750 / 1,252 B** min, so this pass adds **44 B flash / 0 B static** full
and leaves min unchanged. Earlier dated sizes remain historical measurements.
Host full/lean/core/min **150/117/89/44**, dev **155**, chart-save **154**; tooling
**105** and deterministic generation pass. Map/FX bytes are unchanged. See
[render performance](render-performance.md) for timing scope and reproduction.

Wall settlement checkpoint (2026-10-26, `sub-hauler-cxg`): all four physical
berths move from navigation-room centres to wall ledges. Separate dock metadata
drives docking, charts and centred home spawn; authored-route deadlines include
the approach lengths. Hull clearance, a 48-pixel escape column, floor attachment,
hazard separation and berth reachability across all eight tool subsets are
validated. Four inhabited rooms per settlement, piers and airlocks bake into
the existing native image; no extra runtime draw calls or state. Collision,
chart and object blobs and the first 41,572 payload bytes stay unchanged.
Payload/image remain **451,172 / 451,328 B**, including the **409,600 B** terrain.
Full/lean/core/min flash stays **20,098 / 17,292 / 14,722 / 8,696 B** and static
**1,583 / 1,571 / 1,313 / 1,252 B**; dev **15,540 / 1,582 B**. Stack reserve
**775 B**, target **600 B**. Native full/lean/core/min **147/114/89/44**, dev
**152**, chart-save **151**; tooling **105**, all five device suites P in
normal/dev modes. Generation deterministic; native dock/undock checks use the
actual packed collision plane and all four towns were visually inspected.

Works texture checkpoint (2026-10-25, `sub-hauler-1oj`): repeating closed
plate rectangles and centre bars replaced with staggered broken seams,
corrosion chips, bent ribs and sparse rivet runs aligned with the nearby wall.
Only offline scenery generation changed. Native masks retain protected routes,
interactive markers and cave silhouettes; all 18 landmarks/130 smaller motifs
remain. Generated FX header, preceding 41,572 B payload prefix and total
payload/image sizes are unchanged. Full firmware remains **20,098 B flash /
1,583 B static RAM**. Generation deterministic; **146** host cases, **103**
tooling cases and all **5** Ardens suites pass. Native before/after views were
visually inspected. No runtime texture code or extra transfers were added.

Native contour checkpoint (2026-10-25, `sub-hauler-8se`): streamed terrain now
reconstructs a continuous bicubic field at 2560x1280 and thresholds it to 1-bit,
removing square tile steps without device-side smoothing. All collision-tile
centres retain their class; permanent geometry checks bound visual movement
to three pixels in solid and water, in both directions. Scenery is re-anchored
to curved floors and clipped inside the new rock silhouette. Eighteen larger
landmarks remain, with 130 smaller motifs. Collision/chart/object tables and
the first 41,572 payload bytes stay unchanged; no layout or storage growth.
Native terrain remains **409,600 B**, payload/image **451,172 / 451,328 B**.
The runtime stream path and eight 128-byte reads are unchanged.
Generation is deterministic; host full/dev **146 / 151**, tooling **103**, all
five Ardens suites P in each mode. Full/dev firmware HEX files are byte-identical
to the preceding scenery build: **20,098 / 1,583 B** and **15,540 / 1,582 B**
flash/static respectively. The device memory suite retains its P at the 600 B
stack target. Twelve-case render/presentation timing differences are at most
**4 µs normal / 5 µs dev** against that build. Native previews were visually
inspected; no interactive playthrough or physical-device soak is claimed.

Regional scenery checkpoint (2026-10-25, `sub-hauler-9bc`): four distinct
native scenery styles add 18 larger landmarks and 113 smaller floor motifs.
The art generator masks navigation corridors, station/spawn footprints,
gates and pickups; rock marks stay inside the collision silhouette. All
collision/chart/object bytes and the preceding 41,572 B payload prefix remain
unchanged. The existing native terrain section is still 409,600 B, payload/image
**451,172 / 451,328 B**; terrain streaming code and transfer lengths are unchanged.
Host full/dev suites pass **146 / 151**, tooling **99**; streamed-image crop
coverage remains 6,499 viewports. All five Ardens suites pass in normal and
dev modes. Full/Mini remain **20,098 B flash / 1,583 B static RAM**; dev
**15,540 / 1,582 B**. Mini/dev HEX files are byte-identical to their archived
pre-art builds, and the expanded stack probe remains **770 B**. Across twelve camera/cache
cases per mode, measured render/presentation changes are at most **3 µs normal /
4 µs dev**, within the timer resolution. Native world and eight enlarged region
crops were visually inspected; no interactive playthrough or physical soak is claimed.

Render optimization checkpoint (2026-10-25, `sub-hauler-w16`): terrain bakes
to a 409,600 B native image and streams into the existing screen buffer in
eight 128-byte FX bursts; objects/HUD remain overlays. The prior 41,572 B
payload remains an identical prefix. HUD/cave rectangles use clipped page
spans. The native reference renderer decodes packed cache rows once, with its
checked partial-cache fallback retained; this path drops out of device firmware.
Cache refresh reads each distinct packed cart byte once (30–40 transactions
per interior window, down from 180), retaining the 23 B bitmap and sealed
borders. Full/lean/core/min flash **20,098 / 17,292 / 14,722 / 8,696 B**, static
**1,583 / 1,571 / 1,313 / 1,252 B**. Flash savings **402 / 376 / 444 / 446 B**;
static deltas all zero. Full Mini **20,098 / 1,583 B**; dev **15,540 / 1,582 B**
(422 B flash saved). Stack reserve remains **770 B**, target **600 B**.
Native tests **146/113/88/44**, dev full **151**, chart-save full **150**,
tooling **97**, all five Ardens suites P in normal/dev modes; min render suite
also P. Generation deterministic, native panorama byte-identical, all 24
normal/dev cold/cached frame CRCs match the pre-change baseline. Six-scene
AVR timings show **34–58%** less dev render/presentation time; this is not a
measurement of the entire gameplay loop. See [render performance](render-performance.md)
for timings, measurement scope and reproduction commands. FX payload/image
are **451,172 / 451,328 B**. No physical-device timing measurement is claimed.

Organic terrain checkpoint (2026-10-25, `sub-hauler-ukp`): irregular, lobed
caverns and variable-width winding passages replace the uniform tubes. Thirty-one
solid features occupy 509 formerly-water tiles; 31 of 42 passages wind over
12% beyond their straight-line distance. All eight hull/tool reachability
subsets pass. Layout version 3 rejects preceding chart saves; the SHL2 object
format and FX declaration order stay unchanged, including the identical
34,540 B legacy prefix. Payload/image remain **41,572 / 41,728 B**.
Full/lean/core/min flash is **20,500 / 17,668 / 15,166 / 9,142 B**, static RAM
**1,583 / 1,571 / 1,313 / 1,252 B**; full Mini **20,500 / 1,583 B**.
The terrain/timing changes add **0 B flash / 0 B static**. Freight allowances
are baked from navigable route lengths, excluding optional cutter/jet routes.
Native tests pass **141/108/83/39**, dev full **146**, optional chart-save
full **145**, tooling **95**. Generation is deterministic, all four Ardens
suites pass, and the updated diagnostic probe measures **770 B** stack reserve
against the unchanged **600 B** target. The real scene-renderer overview was
visually inspected; no interactive playthrough or physical-device soak is claimed.

Developer traversal checkpoint (2026-10-25, `sub-hauler-m35`): explicit
`DEV_MODE=1` builds in isolated dev directories and measures **15,962 B flash /
1,582 B static RAM**. Normal firmware remains **20,500 / 1,583 B** and its HEX
is byte-identical to the preceding authored-map build. A 19 B FX banner appends
after the map labels, making payload/image **41,572 / 41,728 B**. Generation is
deterministic; native full suites pass **140 normal / 145 dev** and tooling
passes **91**. All four device suites pass in each mode. Free-flight uses direct
bounded D-pad movement, A boost, complete chart, full air/hull, no docking,
hazard simulation or reward claiming; the application suppresses chart EEPROM
reads/writes in dev mode. No interactive playthrough is claimed.


Music asset checkpoint (2026-10-24, `sub-hauler-5ai`): the full game remains
**17,548 B flash / 1,572 B static RAM**. Three original slow loops add
**9,822 B** of external music streams; the cart payload is **22,834 B** and
the padded flashable image is **23,040 B**. The separate ATMlib2FX jukebox
viewer builds at **13,084 B / 1,512 B**. `make gen-check`, 132 host cases,
80 tool tests and all three device suites pass. Each music stream ran through
three loop periods in the host device decoder with zero underruns/errors and
identical consecutive cycles; no listening or physical-speaker check was run.

Music expansion (2026-10-24, `sub-hauler-9g1`): three new cues at
40/75/100 BPM append **11,706 B**, preserving all **22,834 B** of the
previous payload. The six-song payload/image are **34,540 / 34,560 B**.
Full game size stays **17,548 B flash / 1,572 B static RAM**; the six-song
viewer measures **13,208 B / 1,512 B**. New native effects cover arpeggio,
note cut, vibrato/tremolo/duty LFO, volume slide, glissando and noise retrigger.
Three-period host decoder traces (10,800 / 5,760 / 11,520 ticks) have zero
underruns/errors and identical consecutive cycles; the 100 Hz cue uses
two-tick service spacing. Generation is deterministic; 132 host cases and
81 tool tests and all three Ardens device suites pass. Physical speaker sound
was not measured.

Historical cargo-free checkpoint: static free SRAM was **988 B**. `chart::State` is
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
and order. The six underwater music streams occupy **21,528 B**;
the FX payload in `fxdata/fxdata-data.bin` is now **34,540 B**, while
the padded flashable `fxdata/fxdata.bin` image is **34,560 B**. The normal
game has no music playback calls yet. The separate library jukebox viewer
build measures **13,208 B flash / 1,512 B static RAM** with a 64 B ring.
The optional
chart save variant
was last measured at 29,330 B / 1,690 B before the carve; a fresh
`SAVE_CHART=1` measurement is required before changing the default.
`MULTI_HULL` still adds class art/string calls; core and min measure
12,626 B / 1,302 B and 9,142 B / 1,252 B because the roster collapses to
HAULER and those calls compile out. Any future persistent state still needs a
carve rather than another byte-class allocation. The probe does not print the
exact high-water byte unless `SH_STACK_DIAG=1`; static free bytes are still not
a stack measurement.

Progression resource/storage audit checkpoint (2026-10-26,
`sub-hauler-7px.1`): no production firmware or generated artifact changed.
Whole-image baseline re-measured with `make size-matrix`: full/lean/core/min
**20,176 / 17,368 / 14,756 / 8,750 B** flash and **1,583 / 1,571 / 1,313 /
1,252 B** static; `make mini PROFILE=full` repeats **20,176 / 1,583 B**;
`make fxtest` builds six suites and each prints a bare `P`. The supported stack
diagnostic is `make fxtest FXTEST_ONLY=test_memory
RELAX_FLAGS="-mrelax -mcall-prologues -DSH_STACK_DIAG=1"`, which prints
**`stack_reserve=756`** (600 B target). Host tests pass **150/117/89/44**,
tooling **105**, and `make gen-check` reports deterministic artifacts.

The installed Arduboy2 reserves EEPROM 0–15: address 1 system flags, 2 audio
mute, 3–7 reserved, 8–9 unit ID, 10–15 unit name; `EEPROM_STORAGE_SPACE_START`
is 16 and `E2END` is `0x3FF` (1,024 B). Arduboy2 writes only that prefix,
ArduboyFX and ATMlib2FX contain no EEPROM code, and the current default-off
chart save writes 0–259 (including the system prefix). The voyage save contract
places two 320-byte slots at 16–335 and 336–655 and never writes 0–15; details
are frozen in [progression-implementation.md](progression-implementation.md).

A bounded build-only AVR probe,
`tst/fxdatatest/probe_music/probe_music.ino`, measured the ATMlib2FX compact
decoder with the intended `RING=64`, `LOOP=1` flags: **5,858 B / 1,197 B**
without music versus **9,518 B / 1,425 B** with it, a marginal
**+3,660 B flash / +228 B static**. `avr-nm` confirms the 228 B static split
(`atm_fx_compact_state` 103, `channels` 84, `osc_params_array` 16,
`osc_pha_acc_array` 8, `osc_cb` 12, `atmlib_state` 3, two ISR bytes). The
64-byte ring is only 64 of those 228 B. Music alone therefore exceeds the
**117 B** static headroom by **111 B**; adding the projected 17 B campaign
state gives roughly a 128–132 B projected deficit. Flash projection
20,176 + 3,660 = **23,836 B** leaves 5,164 B under the 29,000 B planning
ceiling, but campaign/save flash and the combined stack are unmeasured. Full
music must stay default-off until `.7` measures a combined fit or carves the
state; the 1,700 B static ceiling and 600 B stack target are unchanged. No
manual playthrough, physical listening or soak test was performed.

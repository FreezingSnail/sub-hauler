# Historical checkpoint — 2026-10-24 (cargo-free resource pass)

> **Historical record — archived 2026-10-30.** Retired profile measurements. Current status and limits live in [`README.md`](../../README.md) and [platform.md](../platform.md).

Content is frozen at commit `4f66830` (`sub-hauler-m9-save`); the working tree
adds the post-release fixes, the cargo-packing removal, and the UI/art resource
pass. Contract acceptance now auto-loads the four cargo units while the boat
stays docked; an explicit UNDOCK row or B starts travel. The packing screen,
LOAD CARGO row, commit chord, and cargo runtime were removed because their value
did not justify the flash/SRAM cost. The remaining UI labels and the 6x8 font
now live in FX, the three pilot silhouettes are baked FX sprites, the route
economy tables are generated, and the AVR run state is 80 B. Every profile
builds, all host/tool tests are green, and all three remaining device suites
print `P` through Ardens. No manual interactive playthrough or manual device
soak was run; the soak evidence is the permanent host loop test across all
three hulls plus the device-suite smoke run.

| Profile | Flash | Static RAM | Notes |
| --- | ---: | ---: | --- |
| `full` | 17,548 B | 1,572 B | 12,148 B under hardware; 11,452 B under the 29,000 B planning ceiling |
| `lean` | 14,792 B | 1,560 B | no currents, mines, particles, shop, or save |
| `core` | 12,626 B | 1,302 B | single HAULER, no fog/multi-hull |
| `min` | 9,142 B | 1,252 B | pilot + cave only |

Flash hardware limit is **29,696 B**; static planning ceiling is **1,700 B**
with **128 B** headroom and **988 B** of static free SRAM. The stack probe
(`test_memory`, full profile) reports **`stack_reserve=812`** against the
unchanged **600 B** target. `make mini` built the same
17,548 B / 1,572 B image for the Arduboy Mini.

Against the cargo-free baseline (20,196 / 17,362 / 14,756 / 10,456 B flash and
1,592 / 1,580 / 1,321 / 1,259 B static), the final pass saves
**2,648 / 2,570 / 2,130 / 1,314 B flash** and **20 / 20 / 19 / 7 B static**.
The compact position-only world metadata fell from 148 B to 88 B PROGMEM with
no RAM or cart-read change; that A/B measured 17,584/14,810/12,640/9,178 ->
17,548/14,792/12,626/9,142 B. The 128 B cave atlas intentionally stays in
PROGMEM because moving it to FX adds hot render traffic for no RAM win.

The post-M9 menu fix still makes the station, shop, pause, and save-failure screens
compose only on an input edge and skip the OLED transaction while idle.
Selected rows use a raw `fillRect` bar with `dcmInvert`; only the external-font
glyph strokes are black, so no opaque 6x8 cell can paint through the next 7 px
row.

`SH_FEATURE_SAVE_CHART` remains default **0** and is inactive in every shipped
profile. The measured `SAVE_CHART=1` variant was last recorded at
29,330 B / 1,690 B before the cargo carve; the carve freed far more than that
overage, but a fresh variant measurement is still required before changing the
default.

The shipped `cargoBank` FX declaration stays at offset **0x001AC2** with its
original bytes and order for flash ABI compatibility. The font, strings, and
pilot sheets were appended after it. Six original underwater music
loops now follow those sheets; the FX payload in
`fxdata/fxdata-data.bin` is **34,540 B**, while the padded flashable
`fxdata/fxdata.bin` image is **34,560 B**. Only
the cargo runtime reader, screen, rules, and tests were removed.

The default Arduboy font and `Print` path are absent from the ELF: the firmware
uses `Arduboy2Base` plus the external font adapter in `src/text.hpp`.

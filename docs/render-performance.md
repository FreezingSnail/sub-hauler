# Rendering performance

## Paused chart — 2026-10-26

The second profile shows **77.9% CPU active**, with the largest samples in
`drawChart` and FX byte operations. The chart was recomposed and presented at
60 Hz while the simulation was paused. Every composition performed 256
separate cart transactions and expanded individual water cells with variable
bit shifts. The destination blink was its only animation.

The chart now composes immediately on entry, then redraws only at the existing
16-frame blink boundaries. The sub marker and the active contract destination
both blink, so the panel redraws at **3.75/s** whenever the chart is open,
preserving the original blink phase even when opening mid-cycle or wrapping the
frame counter. Other paused frames skip both cart reads and the OLED
transaction. No extra persistent state is needed.

Each composition reads the same 256 water bytes in **eight 32-byte bursts**.
A 32-byte stack scratch area combines four chart rows with the explored bits,
then expands them directly into one framebuffer page. All 1,024 output bytes
are assigned, including black fog and land, eliminating the clearing pass.
The existing settlement, discovery, destination and sub markers follow.

The permanent `tst/fxdatatest/test_chart.ino` benchmark measures eight samples
per case in Ardens using actual FX data and `micros()`, interrupts enabled.
The normal-mode fixtures include all nine seen gates and a contract to
Cormorant. CRCs match the pre-change renderer in all four cases:

| Chart case | Render before | Render after | Frame CRC |
| --- | ---: | ---: | --- |
| Spawn fog, destination on | 5.427 ms | 4.405 ms | `8487` |
| Spawn fog, destination off | 5.342 ms | 4.242 ms | `3CAC` |
| Fully explored, destination on | 10.327 ms | 4.966 ms | `9BE4` |
| Fully explored, destination off | 10.242 ms | 4.803 ms | `9AE4` |

OLED presentation remains about **1.16 ms** on frames that redraw. Fully
explored chart rendering falls about **52%** per redraw; render plus display
falls about **47%**. Combined with the cadence change, steady paused chart
render/presentation work falls about **97%**, with or without a contract. This
excludes input sampling and the frame scheduler; it is not a measured
replacement for the screenshot's whole-loop CPU percentage. No physical-device
timing is claimed.

Native tests compare expansion against independent per-pixel expectations on
512 packed-water/fog fixtures with buffer guards. They also compare the held
panel image against a freshly rendered chart on every phase, opening at all
32 positions in a blink cycle and crossing counter wrap. The device benchmark
requires repeatable CRCs and render plus display below **7 ms**.

```sh
make test
make fxtest FXTEST_ONLY=test_chart
make fxtest FXTEST_ONLY=test_chart DEV_MODE=1
```

Logs are under `build/chart-profile/`: `baseline.log`, `device.log`,
`dev-device.log`, `size-matrix.log` and `stack.log`. Current full firmware is
**20,176 B flash / 1,583 B static RAM**. A source snapshot taken before this
change measures **20,132 / 1,583 B** with the current toolchain: **+44 B flash,
0 B static**. The full stack probe measures **756 B** reserve, above the 600 B
target. Map data and generated FX bytes are unchanged.

## Native terrain streaming — 2026-10-25

The Ardens screenshot reported 77.2% CPU active, with 21.87% in
`Arduboy2Base::drawFastVLine` and about 9% in `world::cacheLookup`. The installed
Arduboy2 vertical-line routine calls `drawPixel` for every pixel. Its rectangle
fill repeats that for every column; the dev HUD cleared a 128×9 banner this
way every frame. The old cave renderer checked four neighbours for each solid
tile before selecting an edge pattern. `drawPilot` contains inlined scene
rendering, so its profiler entries do not identify just the submarine sprite.

## Current render path

`make gen` bakes the static rock pixels into `exploration-terrain.bin`, a
2560×1280 native 1-bit panorama. It uses **409,600 B (400 KiB)** of FX storage.
Each byte holds eight vertical pixels, in the OLED framebuffer layout. The
8-pixel camera alignment lets the device read eight contiguous 128-byte strips
directly into the existing 1,024-byte framebuffer through `FX::readDataBytes`.
No terrain draw calls, tile selection, bit shifting, clearing pass or extra
framebuffer are needed on device. Stations, equipment gates, finds, currents,
mines, sonar, the boat and HUD compose afterward; the OLED is presented once.
Cart reads all finish before `FX::display(CLEAR_BUFFER)`.

Regional scenery added in `sub-hauler-9bc` bakes moorings, kelp, wreck machinery,
crystals and rock textures into those same image bytes. It does not change the
stream size or device drawing code. The benchmark table below records the
original streaming checkpoint before that art pass; current CRCs intentionally
include the new scenery.

The later native contour pass (`sub-hauler-8se`) also runs entirely offline:
a bicubic field reconstructed from tile centres is thresholded into curved
1-bit walls. Decorations anchor to those floors. This changes neither the
stream format nor device work; geometry tests limit the visual/collision
boundary difference to three pixels and preserve every tile centre's class.

Wall settlements (`sub-hauler-cxg`) move the four berth metadata points to
rock ledges and bake piers, airlocks and inhabited rooms into the same image.
They add no runtime layers or transfers. The home spawn and Deep Anchor timing
case now use the wall berths, so those camera samples differ from the older
table below. Docking and charts use the same new coordinates.

The separate 6,400-byte collision plane and 23-byte visible cache still provide
physics. Refilling an interior cache reads each source cart byte once,
reducing 180 transactions to 30–40. HUD and object fills update clipped
framebuffer page spans instead of individual pixels.

The native procedural renderer remains a host reference, including a faster
packed-row path and checked fallback for invalid/displaced caches. It drops
out of the device firmware. That optimized procedural path can be faster in
mostly empty water because it needs no 1 KB cart transfer. Streaming has a
steady transfer cost, improves dense-rock views, and uses less program flash.

## Measured AVR timings

The permanent `tst/fxdatatest/test_render.ino` benchmark runs in Ardens with the
shipping AVR flags and actual FX data. It measures eight samples per case
using `micros()`, with interrupts enabled. Scene, HUD and OLED presentation are
timed separately; serial output and checksum work are outside those sections.
A cold case invalidates the collision cache each frame; a cached case reuses
it. State is fixed within each case.

Dev mode, milliseconds for scene + HUD + OLED presentation, compared with the
original renderer from the screenshot investigation:

| Camera case | Cached before | Streamed | Cold before | Streamed cold |
| --- | ---: | ---: | ---: | ---: |
| Tidewall spawn | 10.847 | 5.984 | 14.297 | 7.919 |
| Solid rock at (64,64) | 13.980 | 5.811 | 17.242 | 7.662 |
| Lantern cavern F | 8.958 | 5.936 | 12.432 | 7.896 |
| Cutter gate E–V | 12.649 | 6.271 | 16.087 | 8.305 |
| Cutter cove W | 12.874 | 5.994 | 16.286 | 7.881 |
| Deep Anchor | 10.403 | 6.078 | 13.859 | 8.010 |

Dev render/presentation time falls **34–58%**. HUD alone falls from about
5.68 ms to 2.60 ms; the rock scene falls from 7.14 ms to 2.05 ms. Normal mode's
cached cutter-gate case falls from 12.368 ms to 6.050 ms, with its cold case
falling from 15.806 ms to 8.085 ms. OLED transfer remains about 1.16 ms.
Collision cache refill still adds about 1.9–2.0 ms.

These timings exclude simulation, input sampling and the frame scheduler.
They cannot directly replace the screenshot's whole-loop CPU percentage.
No physical-device timing measurement is claimed.

## Reproduction and correctness

```sh
make gen-check
make fxtest FXTEST_ONLY=test_render
make fxtest FXTEST_ONLY=test_render DEV_MODE=1
make fxtest FXTEST_ONLY=test_render PROFILE=min
make test
make test-tools
make render-world
```

The suite prints scene/HUD/display microseconds and a CRC per case, requires
repeatable pixels, and checks combined render/presentation time below 15 ms.
Gameplay update must be measured separately when changed. All five device
suites pass in normal and dev modes; the min render suite passes too.

At the original streaming checkpoint, all 24 normal/dev cold/cached frame CRCs
matched the original baseline and the complete native world PNG was byte-identical.
With the regional art, permanent native tests compare streaming against
independent image crops across 6,499 actual-world viewports, checking
exactly eight bounded reads into a guarded framebuffer. Other tests cover
pixel-exact rectangle clipping, 136 synthetic terrain windows, partial caches,
and collision packing/read counts across world edges. Tool tests verify baked
edges, chamfers, speckles, native layout and the packed FX section.

At the original streaming checkpoint, full firmware was **20,098 B flash /
1,583 B static RAM**, saving **402 B flash** and using **0 B extra static RAM**
against the original organic-map build. Dev was **15,540 / 1,582 B**, saving
**422 B flash**. Its stack probe measured **770 B**, above the **600 B** target.
The previous **41,572 FX payload bytes**
are unchanged; appending the native panorama makes payload/image
**451,172 / 451,328 B**. See [platform.md](platform.md) for the full matrix.

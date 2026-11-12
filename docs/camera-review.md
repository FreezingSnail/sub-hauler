# Camera performance and motion review — 2026-10-28

Reviewed directly with Sol after the pixel-scroll change, then implemented
after approval. The user visually accepted the smooth camera on 2026-10-29;
the camera task (`sub-hauler-7px.22`) is closed. Git metadata remains read-only
in this environment; acceptance does not imply a commit or physical-device
validation.

The older dated figures in this camera checkpoint precede sonar removal and menu baking.
The current fixed-build checkpoint measures **28,218 / 1,650 B** with the
ATMlib2FX synth and an **11.096 ms** maximum complete-flight frame with the same
camera behavior.

## HUD fill checkpoint — 2026-11-11

The supplied Ardens profile showed 81.6% CPU active, with samples in the
music interrupt, cart reads, display transfer and drawing. `drawPilot` includes
inlined scene work, so it is not a measurement of the hull sprite alone.

Full-page rectangles now use a bulk buffer fill, and partial-page rectangles
move the color choice out of the pixel loop. On the same device render probe,
the cached case 0 HUD fell from **1,912 µs to 1,724 µs**; all **24** cached and
cold frame CRCs matched. The complete 842-frame flight probe's maximum fell
from **12,868 µs to 12,652 µs**, with zero 16 ms misses, audio gap 1 tick and
zero underruns/errors in both runs. These targeted timings do not establish a
new whole-session CPU-active percentage.

The release image changed from **28,554 to 28,626 B flash** and stayed at
**1,640 B static SRAM**. The music and production save-chain probes measured
**640 B** and **683 B** stack reserve respectively. Host and device render
checks passed; no physical-device or whole-session CPU measurement is claimed.

## Verified implementation

The shipping FX and Mini images measure **28,218 B flash / 1,650 B static
SRAM**, 1,478 B under the hardware flash limit and 50 B under the static planning
ceiling. The combined music stack probe measures **642 B** reserve; the
production save-chain probe measures **665 B**, against the 600 B floor.

`make fxtest-flight` measures **842 complete flight frames** with active synth
music: all eight Y remainders, all six cues, 30 cold and 812 warm frames,
sustained motion/stop/reversal, camera boundaries, survey notices and discovery
messages. Maximum work is **11.096 ms** with **zero frames
at or above 16 ms**. The 13 ms target passes; the maximum synth service gap is
**1 tick**, with zero underruns and errors. Diagnostic counters and the probe's
stack guard pass. These are build-only production-flight measurements
including input/update, scene, HUD, display and refill; the probe omits voyage
save so its USB test image fits, but the flight update/render/music path is the
shipping code. Boot/title setup and serial output are outside timing. They are
not an uninstrumented shipping-ELF measurement or a claim about whole-session
CPU percentage.

The terrain kernel uses two AVR hardware multiplies instead of runtime shifts
per byte. Page-aligned Y frames keep the fast direct-read path; fractional
frames retain the existing 32 B scratch and single framebuffer. Device tests
check the actual assembly against the reference calculation.

Camera state now uses unsigned Q4 in the existing two 16-bit fields, preserving
fractional position and velocity lead. Normal corrections are bounded to
2 px/tick; explicit developer traversal allows 6. The transient view camera
retains a signed fractional residual so the sub is projected before rounding,
avoiding independent rounding jitter. Terrain and integer-world objects share
the rounded origin. Berth snaps, save schema, physics and music quality remain
unchanged. Temporal tests check exact steady-speed travel, complete slow pixel
intervals, stable hull framing, both vertical directions, monotonic settling,
negative projections and far-world Q4 bounds.

HUD notice rows are generated from the existing font and appended to the FX
layout. One 128 B row read replaces repeated glyph transactions; values and
priority remain dynamic. Actual AVR pixel comparisons cover all eleven shipped
notice frames over varied backgrounds. The retired control row and three
never-selected notice frames were removed by `sub-hauler-jxk.3`. Existing scene
CRCs match. The slower outline and full-panel stamp experiments were discarded.
At the camera checkpoint, FX payload/image were **2,010,726 / 2,014,976 B**;
earlier asset offsets and the 4 KiB save section remain intact. Current totals
are in [startup-splashes.md](startup-splashes.md).

Verification at the camera checkpoint (2026-10-28, retired profile builds):
deterministic generation, host **260/161/131/87** for full/lean/core/min,
developer host **265**, tooling **137**, all **12** default device suites,
extended flight probe, min-profile render suite, full size and Mini build.
Those profile counts are historical; current fixed-build totals live in
[platform.md](platform.md). GUI playthrough remains unverified.

## Evidence

The supplied Ardens screenshot reports **89.3% CPU active**. Its `drawPilot`
entry includes inlined world rendering, so its 18.47% sample share cannot be
attributed to the submarine sprite alone. The ungrouped interrupt ranges also
belong to the same music ISR; individual profiler rows are not separate systems.
The screenshot establishes high utilization, not a frame-duration trace or a
count of missed deadlines.

Re-ran `make fxtest FXTEST_ONLY=test_render` on the current source. All cases
pass the existing gate. Selected measured times, in microseconds:

| Case | Scene | HUD | Display | Total |
| --- | ---: | ---: | ---: | ---: |
| Spawn, cached, page-aligned Y | 2446 | 3393 | 1163 | 7002 |
| Fractional origin, cached | 6934 | 3390 | 1161 | 11485 |
| Spawn, cold cache | 4489 | 3391 | 1162 | 9042 |
| Fractional origin, cold cache | 8974 | 3392 | 1162 | 13528 |

The two spawn-area fixtures use slightly different origins; the 4.49 ms scene
difference is not an isolated kernel A/B. Source and shipping ELF disassembly
independently identify the expensive per-byte shift loops. The loop processes
1024 output bytes, using runtime left shifts and promoted 16-bit right shifts.
It also reads 128 additional source bytes in four tail transactions.

The installed Arduboy2 `setFrameRate(60)` computes `1000 / 60`, so its actual
scheduling interval is **16 ms**. The fractional cold render leaves 2.472 ms
for the rest of the frame, even before considering these benchmark limitations:

- `test_render` never starts music and excludes gameplay updates and refill work.
- It checks eight-sample averages against 15 ms, not complete-frame maxima.
- `test_music` checks refill gaps and underruns, not complete-frame deadlines.
- Existing camera tests check bounds, symmetry and convergence, not visual
  motion cadence. Passing them did not establish smooth flight.

Current reported full-build baseline: **27938 B flash / 1619 B static SRAM**,
**638 B** minimum measured stack reserve. The ceilings remain **29000 / 1700 B**
and the stack floor remains **600 B**.

## Stage A measured evidence — 2026-10-28

Bounded stage A is implemented and measured. No camera motion, HUD, cache, or
sibling-library behavior changed.

The permanent `tst/fxdatatest/test_flight.ino` harness includes the real
production sketch and runs its shipping `updateFlight()` (music state, real
`station::stepFlight` and damage), `drawPilot()` (scene and HUD),
`FX::display(CLEAR_BUFFER)` and `music::service()` around the actual
`arduboy.nextFrame()` 16 ms scheduler. It measures 838 scheduled frames with
active music: all eight Y remainders cold and warm, a 120-frame fractional-Y
hold, all six cues over cold/warm fractional and page-aligned paths, steady,
diagonal, slow, reverse and settling motion, and all four camera boundaries.
Fixtures use the generated Room Q navigation centre in still water; currents
remain enabled. The reversal path stays in x 663..688 at y 408 in the trailing
window, outside every generated `CURRENTS` rectangle, and its assertion is not
relaxed. Timed spans exclude checksums and printing.

Historical diagnostic snapshots follow. Fixtures and implementation details
were refined between these runs; these numbers are **not a controlled A/B**
and their differences must not be quoted as an isolated kernel speedup:

| Full-flight metric | Earlier shift probe | Later kernel probe |
| --- | ---: | ---: |
| Frame maximum | 18.496 ms | 14.600 ms |
| Frame mean | 12.096 ms | 10.106 ms |
| Frames at/over 16 ms | 34 | 0 |
| Maximum service gap | 80 ticks | 50 ticks |
| Underruns / errors | 0 / 0 | 0 / 0 |

The candidate distribution is `9 ms: 532`, `10: 147`, `11: 103`, `12: 29`,
`13: 0`, `14: 27`, `15: 0`, `16+: 0` frames (sub-9 ms bins empty). Per-case
maxima show
the remaining worst cases: diagonal flight **14.600 ms**, cold fractional
remainders 14.44 ms, and all six cue cases 14.44 ms; the sustained fractional
hold is 10.936 ms and boundaries are 9.1 ms. The 13 ms target is still missed by
**1.600 ms**. The hard 16 ms moving-frame limit and zero-miss requirement pass.
The shortfall is now dominated by HUD 4.236 ms plus display 1.396 ms; stage A's
terrain kernel alone cannot meet 13 ms. This is a measured shortfall, not a
relaxed pass.

The renderer computes `factor = 2^(8-s)` once per fractional frame and calls
one byte-combine helper for the in-place pages and the 32-byte tail. On AVR the
helper is a four-instruction hardware multiply sequence (`mul` high byte,
`mul` low byte, `or`, then `eor r1,r1`); disassembly confirms exactly two 8x8
MULs per output byte. The inline assembly declares `r0` and `cc` clobbered and
restores the AVR zero register. The first C-only version emitted 16-bit
multiply sequences and measured 15.256 ms maximum, so the AVR helper is
required, not cosmetic.

Correctness evidence is both formula and device execution. The host suite adds
an exhaustive 7 x 256 x 256 byte-pair check against the direct shift expression;
the existing native-crop test covers every X/Y remainder and map edge. The
device `test_render` suite now runs the actual AVR helper for all 256 isolated
source bytes in each position for all seven shifts plus mixed patterns before
its timing cases. `make test` reports **254 passed, 0 failed**.

Resource gates at the camera checkpoint (retired profile build): shipping image
**27,878 B flash / 1,619 B static RAM**, down **60 B flash** and unchanged
static from the 27,938 / 1,619 B baseline. That is 1,122 B under the 29,000 B
planning ceiling and 81 B static headroom. `test_save_chain` remains **666 B**
stack reserve; the combined `test_memory_music` reserve is **634 B**, above the
600 B target with the extra probe globals. `make gen-check` is deterministic,
`make test-tools` passes 135, all **12** default Ardens suites pass, and the
Mini build repeats 27,878 / 1,619 B.

These are instrumented device-suite figures: the suite is compiled with
`--optimize-for-debug`, `STREAMFX_DIAG=1`, and guarded `SH_FLIGHT_PROBE`
microsecond hooks in `drawPilot()`. The hooks compile out of shipping images
(`SH_FLIGHT_PROBE` defaults to 0), so the absolute numbers are not a shipping
ELF measurement. The stage A probe failed the 13 ms target at that checkpoint;
the verified implementation above now passes after the HUD and motion changes.

## Proposed rendering design

Keep the page-major FX panorama and the single existing framebuffer. Preserve
pixel origins and the page-aligned Y fast path. Replace the runtime shift loops
with one compact AVR byte-combine kernel, rather than seven duplicated renderers.

For vertical remainder `s` in 1..7, compute the byte factor `2^(8-s)` once per
frame. For source bytes `a` (current page) and `b` (next page), the output is:

`highByte(a * factor) | lowByte(b * factor)`

Both products fit unsigned 16 bits and can use AVR's 8×8 hardware multiply.
Confirm the emitted instructions; use a small AVR-specific helper only if the
compiler fails to generate the intended kernel. Keep a pure C++ reference and
check all shifts, byte combinations and real-world edge crops. If assembly is
used, preserve the AVR ABI, including the zero register. Reuse the kernel for
the 32-byte tail. Expected savings require AVR timing and whole-image A/B proof.

Retain bounded scratch and the current shared-SPI transaction rules. A second
128-byte stack page is unsuitable with only 38 B of measured reserve above the
stack floor. Eight preshifted panoramas would exceed the 2 MiB cart capacity.

## Proposed camera motion design

The current follow function reads whole-pixel submarine positions and rounded
velocity lead, then forces a minimum one-pixel correction. It cannot retain
fractional easing, and it moves on quantized target changes.

Use a fractional follow target derived from `x/y` plus their existing Q12
remainders, and retain fractional camera position between ticks. A candidate
is **unsigned Q4 camera coordinates** in the existing two 16-bit camera fields:
the maximum X origin is 38912 in Q4 and fits uint16. Use signed 32-bit temporary
differences for clamping and easing; do not store world-wide signed Q4 values
in int16. Fractional velocity lead in Q4 is `velocityQ8 / 32`.

Give these fields explicit names and getters so callers cannot confuse pixels
and fixed point. Round once at the render boundary and share that origin across
every world layer. Preserve berth/world-edge snaps; camera state is derived
when restoring a checkpoint. Compare emitted flash/cycles before adopting Q4.
If quarter-step truncation leaves a fractional tail, retain its remainder or
snap only below the fractional resolution; do not force a full-pixel step.

A 1-bit display still advances by whole pixels: at 30 px/s, even motion is
approximately one pixel every two 60 Hz ticks. Acceptance concerns regular
cadence and controlled stop/reversal motion, not subpixel display interpolation.

## Further measured opportunities

The HUD costs about 3.39 ms even in the music-free benchmark. The installed
`drawRect` draws vertical sides pixel by pixel. Compare page-span rectangle
outlines and generated FX stamps for recurring text against the current pixels
and cost. Any stamps must be generated through `make gen` and appended to the
FX ABI. The cleared framebuffer means static HUD elements still need drawing
each presented frame; simply skipping them is incorrect.

Cold collision-cache refresh costs roughly 2 ms. Terrain streaming no longer
uses this cache, so following every camera tile boundary is unnecessary for
rendering. Consider retaining the collision cache until the hull/probe bounds
approach its edge, then refreshing around the hull with hysteresis. Preserve
the existing direct-read fallback and measure cold-frame maxima regardless of
the lower refresh frequency. Pursue these changes only after the first kernel
measurement identifies the remaining full-frame cost.

## Measurement and acceptance

First establish a shipping-loop trajectory baseline with active music: both
directions, diagonal motion, slow travel, stop/reversal, tile crossings, world
edges and a long hold at fractional Y. Exercise all eight Y remainders, cold
and reused caches, all six cues, and recurring HUD/discovery states. Record
update, scene, HUD, display and refill times, full-frame maximum and distribution,
deadline misses, service gaps and underruns. Keep timing output outside timed
sections; distinguish diagnostic builds from shipping ELF measurements.

Compare the terrain kernel first, then fractional camera behavior, then any
remaining HUD/cache changes. Each comparison needs whole-image flash/RAM and
painted-stack measurements. Proposed target: full-flight maximum at most 13 ms,
giving 3 ms reserve under the current scheduler; no measured moving-flight
frame may exceed 16 ms. Save/erase transitions are a separate paused path.
Music must retain zero underruns/errors and service gaps at most 16 ms.

Use motion traces to check shared layer origins, stable stationary framing,
regular steady-speed pixel cadence, monotonic settling, and bounded reversals.
Finish with a GUI flight comparison and the project's full generation, host,
tool, device, size and Mini gates. A render-only P marker is insufficient.

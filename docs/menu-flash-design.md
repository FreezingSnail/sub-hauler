# Remaining menu baking — 2026-10-28

Accepted direction: remove unused gameplay sonar, then reduce program flash by
baking the remaining station/shop presentation into FX assets. The target is
29,696 B flash, 1,700 B static and at least 600 B measured stack reserve. The
starting estimated live-synth gap was 700 B. The verified savings below now
gave room for the measured synth build; the final resource gate is the paired
whole-image result in [platform.md](platform.md).

The menu/backend measurements below are historical checkpoints. Current totals
after adding the boot introduction are in [startup-splashes.md](startup-splashes.md).

## Verified result

| Shipping configuration | Flash | Static RAM |
| --- | ---: | ---: |
| Before removal/baking, streaming | 28,228 B | 1,619 B |
| Sonar removed, streaming | 27,720 B | 1,617 B |
| Sonar removed and menus baked, streaming | **27,206 B** | **1,617 B** |
| Before removal/baking, music off | 26,008 B | 1,460 B |
| Sonar removed, music off | 25,520 B | 1,458 B |
| Sonar removed and menus baked, music off | **25,000 B** | **1,458 B** |

Sonar saves 508 B in the streaming image and 488 B music-off. Menu baking
saves another 514 B streaming / 520 B music-off. Combined savings are
**1,022 B streaming / 1,008 B music-off**, with 2 B less static RAM.

The selected design uses **30 station frames, 32 complete shop frames and ten
7-pixel hint rows**. Prices/MAX, levels, names and selection bars are baked;
live balances, health/capacities and job summaries remain overlays. The shop
uses 12-pixel row spacing. Existing station information and action order are
preserved. Hint rendering reuses the existing clipped blitter; this saves
98 B against the first custom hint-shifter candidate and removes its scratch
buffer. The cart image is **2,079,744 B**, leaving 17,408 B under 2 MiB; its
payload is 2,075,502 B. Earlier data offsets remain unchanged.

The earlier optimized live-synth projection was superseded by the measured
production backend on 2026-10-28: ATMlib2FX synth, ring 32, all six authored
streams. The paired whole-image result is **25,000 B / 1,458 B** music-off and
**29,696 B / 1,662 B** music-on, i.e. 0 B free under the hardware limit and
38 B under the static ceiling. `MUSIC_BACKEND=stream` preserves the comparison
build at **27,198 B / 1,618 B**. The shipping backend is now the synth.

Verification: host **251/149/131/87** for full/lean/core/min and **256** DEV;
**142** tool tests; deterministic generation including all new artifacts;
**13/13** standard device suites. The new device menu suite compares **69**
complete frames against independent live-font composition, including all
selections/levels, service berths with carried jobs, hints, credit extremes,
capacity changes and nonzero background patterns. Full music/FX/save checks
pass. Full and Mini match 29,696 / 1,662 B; the music-off image is
25,000 / 1,458 B and the streaming comparison 27,198 / 1,618 B. Stack reserves
are **616 B** combined music and **691 B** production save chain. The 842-frame
flight probe measures **12.376 ms max / 8.355 ms mean**, zero 16 ms misses and
zero audio errors/underruns; the synth service gap is 1 tick.

The generated [preview](../build/menu-preview/menu-contact.png) includes
illustrative live overlays. Automated device pixels and offline visual review
are verified; no interactive GUI playthrough is claimed. Source and generated
artifacts remain uncommitted because Git metadata is read-only.

## Measured starting point

Full streaming image: 28,228 B / 1,619 B. A compiler-flag-only SONAR=0 probe
measures 27,748 B / 1,618 B: 480 B flash saved before deleting the sonar shop
upgrade, persistent field and remaining dependencies. The music cue named
Sonar Lullaby is independent of the retired mechanic.

Title selection, pause, save/recovery prompts and ending chrome are already
baked. At that baseline, station and shop drew individual strings, separators and row
highlights. WORK also has dynamic composition, but remains a later candidate
after the primary two menus are measured.

## Compact station backgrounds

- Generate full 128x64 backgrounds containing berth names, fixed status/credit
  captions, divider, action labels and selected bars.
- Four main berths each need three idle and four active selections: 28 frames.
  Two service-only berths need one UNDOCK frame each: 30 frames, about 30 KiB.
- Active-job summary remains a live destination/reward overlay. Credits, air,
  capacity and hull values remain live numeric overlays. No Cartesian product
  of credit/health/job values is baked.
- Keep current row/action identities and safe initial UNDOCK selection. Keep
  hints and their service/short-load/rumor/normal priority. Hint stamps can be
  128x7 at y57, preserving the last action bar at y56.
- Use a pure frame-index helper for exhaustive host selection/berth contracts.
  Keep the smaller feature-profile layouts supported rather than displaying
  disabled SHOP/WORK actions.

## Two-item shop

- After sonar removal, show AIR TANK and HULL PLATE with more vertical spacing.
- Compare complete variants: 4 air levels x 4 hull levels x 2 selections gives
  32 frames baking labels, levels, prices/MAX and selected bars. Indexing the
  finite state can eliminate the whole runtime row loop.
- A compact alternative is two backgrounds plus 2 items x 4 levels x 2
  selection-color rows, roughly 52x8 each. Choose by whole-image measurement.
- Credit/air/hull numbers remain dynamic. Hint variants preserve normal buy,
  insufficient funds and maximum-level feedback; purchase rules stay pure.
- Read prices/level limits from the rules during generation to avoid a second
  manually maintained economy table. Verify generated selector/frame order.

## Resource and correctness checks

Expected new cart use is approximately 35 KiB for compact rows or 65 KiB for
complete shop variants, within the current roughly 80 KiB cart headroom.
Actual output must remain under 2 MiB. Append declarations
before the save-section marker, preserve every existing data offset and keep
all generated artifacts under `make gen`/`make gen-check`.

Measure whole-image flash/static after sonar removal and after each menu;
discard changes that add code without a justified benefit. Test all valid
selection/layout/item/level combinations, readable bounds, hints and dynamic
amounts. Preserve idle-screen presentation gating, music service between FX
bursts, save behavior and the camera's complete-flight deadline. Finish with
generation, host/tool/device, flight, size-matrix and Mini gates. Existing
uncommitted work must be preserved; Git metadata is read-only here.

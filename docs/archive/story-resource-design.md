# Story, checkpoints and music within the full-build budget

> **Historical record — archived 2026-10-30.** Retired profile and sampled-audio design. Current contracts live in [platform.md](../platform.md) and [progression.md](../progression.md).

Status: **historical design record, superseded for production audio on
2026-10-28.** The shipping `full` profile uses the ATMlib2FX compact synth
(`ATM_FX_COMPACT_RING=32`), not the sampled ADPCM player proposed below. The
sampled-asset work remains useful as comparison evidence and as the source of
the generated ADPCM/reference files. Current behavior and whole-image numbers
are in [platform.md](../platform.md), [progression.md](../progression.md) and
[music/README.md](../../music/README.md).

Approved implementation direction, 2026-10-27 (`sub-hauler-7px.9`): the user
approved cleanup of superseded experiments and continuation of this plan.
The resource allocations remain acceptance targets, not a proven combined fit.
The user requires gameplay music in the same full build as the story and
persistent checkpoints. Gameplay semantics remain those in `progression.md`;
hardware ceilings remain those in `platform.md`.

## What the measurements establish

The frozen native control contains all seven objectives and the ending draft.
The table and VM experiments preserved its mission and clue decisions.

| Full profile, music off | Flash | Static RAM | Memory-probe reserve |
| --- | ---: | ---: | ---: |
| Native campaign + checkpoints | 30,432 B | 1,663 B | 711 B |
| FX condition table | 30,566 B | 1,663 B | 711 B |
| Small story VM | 30,592 B | 1,721 B | 660 B |

The condition representations did not reduce this story's firmware cost. They
still retained native screen drawing, menu controllers, checkpoint handling
and world overlays. A generic interpreter alone does not address those costs.
The table's WORK drawing cases also perform 51–62 record reads per redraw;
idle WORK screens skip rendering, so this is not a continuous idle-frame cost.

Two additional **build-only** measurements use the native control, unchanged
gameplay sources copied into ignored staging:

| Build probe | Flash | Static RAM |
| --- | ---: | ---: |
| Native story/checkpoints, `ARDUBOY_NO_USB` | 27,764 B | 1,525 B |
| Above + actual ATMlib2FX player, one cue and foreground refill | 31,584 B | 1,753 B |

The USB-free entry point saves 2,668 B flash and 138 B static. The actual
player/refill chain adds 3,820 B flash and 228 B static in that image. The
second probe does not implement region selection, mute, pause or save hooks;
their code would add to it. It fails both planning ceilings and the hardware
flash limit. Neither probe establishes audio timing, speaker quality or
bootloader-entry behavior on hardware.

Measurements and archive provenance are retained in
[`story-resource-measurements.md`](story-resource-measurements.md).
The superseded prototype worktrees and compiler caches have been removed;
their patches, source prerequisites, reports and selected ELFs are preserved
in the verified archive described there.

## Recommended architecture

Move static content and music synthesis work into generation. Keep a small
native mission engine, a shared panel renderer, an atomic checkpoint codec,
and a bounded sampled-audio player on the AVR. This preserves the seven
objectives, both repair orders, early surveys, optional thruster, five payments
totalling 590 credits, restoration, homecoming and continued exploration.

### 1. A release entry point without runtime USB

Use the installed Arduboy2 `ARDUBOY_NO_USB` entry point in the release build.
Keep USB in development and serial device-test builds. The library supports
this explicitly; it removes runtime USB, not the bootloader.

The user-visible tradeoff is that a desktop uploader cannot force an automatic
reset from a running game. The release must retain the supported startup
button path into the bootloader; no in-game title action is required. Verify it
on FX and Mini hardware before release. Do not change the bootloader, available
program size or canonical ceilings.

### 2. Native mission facts and generated presentation

Use the existing stable milestone, survey and tool bits. A small PROGMEM
mission record contains source, destination, required milestone mask, required
survey mask, required tool mask and completion fact. A shared mask test answers
prerequisites; completion and payment remain native and idempotent. Keep these
frequently used facts available without FX reads during save validation or
world updates.

Do not add a VM, script stack, general callback system or dialogue coroutine.
The authored story has a fixed set of native actions: accept freight, record a
survey, resolve delivery, show a panel, and advance the ending.

Generate story text, panel backgrounds, row labels and restoration artwork in
FX. Append declarations and regenerate the complete set through `make gen`.
Keep a shared native panel renderer for fixed text rows, selection highlight,
numeric slots and A/B actions. The same renderer covers WORK details,
confirmations, save failure and ending cards. Selectable labels remain text
so the existing highlight and controls work. Bake the lit-network and thanks
cards as images; overlay the saved time/cove/job values on a common stats card.

Use small masked FX sprites for lamps, repaired machinery and survey markers
with one existing blit path, instead of separate procedural drawing recipes.
The flags select artwork; collision and berth rules stay unchanged. Preserve
all clue priorities and all existing confirmation/retry choices.

Clues use ordered per-settlement rules with fixed required-set/required-clear
masks and an FX text/panel result. Index directly to that settlement's range;
do not scan other settlements' records. Evaluate on panel entry or a state
change. Retain only result IDs in the existing menu transient state. Ordinary
idle frames perform no story evaluation or story metadata reads.

### 3. Pre-rendered music in FX

Render the existing six scores offline with a pinned reference synthesizer,
including their authored tempo changes and effects. Low-pass/resample the
result and encode mono 4-bit IMA ADPCM at a proposed 8,000 samples/second.
Keep the score sources authoritative and include source/tool hashes in a
generated manifest. A deterministic native renderer/capture tool and encoder
must be added to the generation pipeline; this capability does not yet exist
in the current music generator.

The current six loops total 356.16 seconds. At 8 kHz and 4 bits/sample their
sample data totals approximately **1,424,640 B**, before descriptors/padding.
With the current 452,259 B payload and new panel assets, set an initial packed
game-image cap of **2 MiB**, and verify allocation against the actual cart.
The trade is additional external FX storage and sampled sound fidelity in
exchange for removing the live oscillator/score engine from firmware.

Proposed player contract:

- 128-byte compressed-data ring, at most 24 B of decoder/stream state.
- One integer ADPCM decoder and PWM output, no channel mixer or score parser.
- Timer interrupt consumes SRAM only; it never reads FX or draws.
- Foreground refill before the frame gate and after presentation, including
  idle menus. Pause before EEPROM transactions; resume in place afterward.
- A cue descriptor supplies FX address, encoded length, exact sample count,
  initial predictor/index and loop reset state. Switching pre-fills before
  enabling output. Same-cue selection does not restart playback.
- Exact sample-count loops, no inserted silence. Reset decoder state at the
  authored loop boundary; generator validates nibble alignment and loop data.
- Preserve the accepted region ownership, two-second hysteresis, station and
  ending overrides, chart/pause freezing, mute persistence and silent rescue.

A 128-byte ring holds roughly 32 ms at this data rate. This is an arithmetic
buffer duration, **not an underrun proof**. Require a measured maximum refill
gap no greater than 16 ms under the admitted combined paths. The player must
demonstrate that deadline with the real ISR, OLED, cart and save transitions.
Do not silently enlarge the ring past the SRAM allocation or reduce sample
quality if admission fails.

Sound quality is a design acceptance decision. Compare all six encoded loops
with the reference, especially Bubblejet Run's faster events and noisy/high
notes, and listen on the device. If 8 kHz ADPCM is unacceptable, remeasure the
whole design at the chosen quality. No fidelity or final decoder-size result
is claimed by this document.

### 4. Preserve atomic checkpoints; simplify their presentation

Keep the single cart save slot, CRC, commit-last transaction, read-back success
check and clean-New-Voyage recovery for torn records. This is not a power-fail
atomic two-copy protocol. Keep
the current explicit saved-field encoding; do not save raw C++ objects. Save
repair/payment before showing its celebration, and mark ending presentation
separately so an interrupted ending can replay without paying again.

Consolidate the application boundary to one checkpoint operation plus an
explicit resume destination. Use the shared panel controller for failed saves,
retry/continue-unsaved, replacement and rescue confirmations. Keep semantic
validation at the storage boundary. A codec/API rewrite is optional further
work only if the presentation/runtime changes miss the budget; removing
interruption safety is not an allowed saving.

### 5. Reclaim small static allocations before adding the player

The USB-free control still has **51 B** of compiler-generated switch data
(`CSWTCH` objects of 12, 21 and 18 B). Replace those specific mappings with
measured PROGMEM lookups or smaller native branches and check the entire ELF.
Do not assume source syntax forces the compiler to avoid `.data`.

Use one row/card cursor and a bounded union for mutually exclusive title,
WORK, confirmation and ending transients. A paused world must retain its
physics/particles. Persistent flags and the complete 256-byte discovery chart
remain intact. This design does not require a lower-resolution chart, smaller
music ring from the earlier synth admission, fewer hulls or fewer missions.

## Full-build allocation and admission

These are **maximum allocations to enforce**, not predicted or measured
implementation costs. Flash is `.text + .data`; static RAM is `.data + .bss`.

| Allocation | Flash cap | Static RAM cap |
| --- | ---: | ---: |
| USB-free game, story, panels and atomic checkpoints | 26,500 B | 1,480 B |
| ADPCM player, PWM, ring and decoder state | 1,600 B | 152 B |
| Cue selection, settings and loader action | 400 B | 16 B |
| Unallocated integration reserve | 500 B | 52 B |
| **Existing planning ceilings** | **29,000 B** | **1,700 B** |

The first allocation requires at least **1,264 B flash and 45 B static** saved
from the measured USB-free native control. Shared/baked presentation and the
specific `.data` mappings are the first candidates. Their savings are not yet
measured. The player must meet its allocation before broad story migration.
Use paired whole-image builds for each boundary; marginal costs can overlap
under LTO. The final combined image, not allocation arithmetic, decides fit.

The 29,696 B hardware maximum and the **600 B measured stack-reserve target**
remain unchanged. Diagnose ISR and foreground stack with the real audio
player; a probe built with runtime USB has different static/ISR ownership and
cannot be substituted uncritically for the release measurement.

## Verification before selecting this design for release

1. Establish the no-USB control and verify loader entry on FX/Mini. This is the
   only new resource saving measured so far; it is not adopted automatically.
2. Build the smallest real ADPCM player with all six looped assets and real
   refill/mute/pause APIs. Admit flash, static RAM, sound quality and refill
   timing against its allocation before replacing the synth integration plan.
3. Convert WORK/ending/prompt presentation and overlays; demonstrate the
   26,500/1,480 pre-audio cap with identical mission/save behavior.
4. Integrate region ownership and all audio lifecycle hooks. Verify the real
   full image at or below 29,000/1,700, stack reserve at least 600 B, no underruns,
   no timer-side FX reads, and no rule scans on idle panels.
5. Run deterministic generation, native/tool/device tests, full/lean/core/min,
   Mini, and interrupted-write/branch-order/early-discovery/replay regressions.

If a boundary fails its allocation, record the exact deficit and revise the
design before integrating it. At that checkpoint, the live-synthesis
alternative remained available for comparison, but its measured 31,584/1,753
minimal combination already needed at least 2,584 B flash and 53 B static
recovered before adding the remaining audio controls. The later compact-synth
integration met the canonical budgets; see platform.md.

Implementation is authorized in dependency order: sampled-audio admission,
USB-free release runtime, fitted presentation, then full cue integration and
release verification. A failed admission stops dependent integration until
the resource or fidelity deficit is resolved. No push is authorized.

## Implementation checkpoint: USB-free entry and title loader (2026-10-27)

`sub-hauler-7px.12` adopts the approved release entry point without changing
the canonical ceilings. Shipping builds default to `RELEASE_NO_USB=1` and place
`ARDUBOY_NO_USB` immediately before `setup()`; `RELEASE_NO_USB=0` and
`DEV_MODE=1` retain the runtime USB stack for development. The title screen
briefly had an explicit **LOADER** row; it was removed on 2026-10-29. The
library's startup **DOWN** check remains the
recovery path; a desktop uploader cannot force an automatic reset from a running
USB-free game.

Paired full-profile measurements: USB-free **17,620 B / 1,444 B**; runtime USB
**20,282 B / 1,584 B**; the no-USB entry saves **2,662 B flash / 140 B static**.
The removed title loader cost **208 B flash / 1 B static** against the .11.2 noUSB
control, inside the **400 B/16 B** loader+cue/settings allocation. The current
campaign+save USB-free image links at **27,786 B / 1,525 B**, still
**1,286 B flash / 45 B static** over the **26,500 / 1,480** pre-audio target;
with runtime USB it is **30,474 B / 1,663 B**, 778 B over hardware. A build-only
noUSB actual-game stack probe reports **820 B** reserve against the 600 B
target. The final combined story/presentation/audio fit remains task `.13`; no
cap was raised and no physical loader/uploader or Mini hardware check was
performed.

## Implementation checkpoint: fitted story presentation (2026-10-27)

`sub-hauler-7px.13` converts the repeated story drawing to generated FX
presentation without changing mission, save, confirmation or recovery rules.
`tools/gen_panels.py` bakes fixed text and selection bars into 17 panel sheets:
WORK details, carrying summary and row frames; title menu/replace/keep; save
failure, pause and wreck variants; rescue/abandon confirmations; and the ending
network/thanks/stats cards. Four plus-mask restoration sprites replace the
survey-ring, inactive-mooring, pump-flow and lift-cable recipes on device; the
host keeps the procedural reference used by the permanent pixel tests. The
shared `panel_view` renderer streams one opaque panel directly into the
framebuffer and overlays only saved status, hull, time, cove and job values.
`SH_CART_SERVICE()` precedes every long presentation burst and remains the
single foreground-refill hook shared with the cold world collision path.

The specific RAM switch tables are gone: station names, mission purposes and
voyage status labels use branch code instead of the 12/18/21 B `CSWTCH`
objects. Mutually exclusive WORK/pause/save/wreck row selection and the
WORK-detail/ending-card transients share two bytes; the station/shop row stays
separate because a failed shop save must resume the purchased item row.

Whole-image measurements:

| Full profile, USB-free | Flash | Static RAM |
| --- | ---: | ---: |
| .12 campaign + atomic save | 27,786 B | 1,525 B |
| .13 fitted presentation | **26,472 B** | **1,469 B** |
| Pre-audio allocation | 26,500 B | 1,480 B |

Flash is **28 B** under the pre-audio allocation and static is **11 B** under
it. Mini repeats 26,472/1,469 B. Host tests pass 169 default / 220 with
campaign+save; tooling passes 128; `make gen-check` is deterministic; all seven
Ardens suites pass in default and campaign+save builds. The campaign+save
`test_memory` probe is **725 B**. A build-only noUSB actual-game probe with the
real sampled player and Timer3 ISR, cold/cached render, restoration overlays and
all new panels measures **779 B** stack reserve against the 600 B target.

The admitted player allocation (**+1,594 B flash / +152 B static**) would put
the combined image at **28,066 / 1,621 B**, under the **29,000 / 1,700**
combined ceiling by 934/79 B; music is not integrated, so this is an allocation
projection, not a measured combined build. No physical playthrough, listening
or Mini hardware check is claimed.

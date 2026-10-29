# Direct follow-up flash review — 2026-10-28

I inspected and measured these changes directly, following the first
[three-group review](flash-savings-review.md). Baseline is its combined
**28,114 B flash / 1,632 B static SRAM** prototype, not the original
28,998 B playtest. Configuration is full campaign/save/music, USB-free,
normal gameplay, with the same compiler and flags. This snapshot still uses
EEPROM voyage saves; the FX-save migration needs a separate measurement.

## Accepted incremental probes

Each row was built after retaining the preceding wins; these are incremental
whole-image results, not independently added symbol estimates.

| Change | Flash after | Saving at this step | Correctness basis |
| --- | ---: | ---: | --- |
| Baseline | 28,114 B | — | Earlier combined prototype |
| Mission availability: remove unused metadata validation/read | 28,082 B | 32 B | Prerequisites reject every unknown ID; completion is checked for valid IDs |
| Mission completion: use the metadata milestone instead of a second switch | 28,020 B | 62 B | Mission table already stores the exact bit; survey and invalid-ID handling retained |
| Q12 floor: arithmetic shift instead of signed division plus correction | 27,996 B | 24 B | Compile-time arithmetic-shift contract; accumulator fits int16 |
| Movement: quotient/remainder decomposition instead of a wide multiply/divide | 27,944 B | 52 B | `(v/15)*4 + ((v%15)*4)/15` exactly equals truncating `(int32(v)*4)/15`; intermediates fit int16 |
| Shop: byte-sized prices and price result | 27,930 B | 14 B | Prices are 50/60/70; credit balance and subtraction retain their existing width |
| Saved rewards: remove range/modulo checks preceding exact equality | 27,886 B | 44 B | Exact generated ordinary/express reward comparison still rejects every malformed reward |
| Music hysteresis: remove unreachable saturation path | 27,878 B | 8 B | Transient counter resets on the transition threshold; never loaded from saves |
| Sonar ring: use one unsigned interval check | 27,864 B | 14 B | Equivalent for all 256 cooldown values, including zero and out-of-range inputs |
| Sonar upgrades: cap before adding; avoid needless wide intermediates | 27,860 B | 4 B | Static one-unit upgrade step; equivalence checked for all 256 levels |
| **Combined additional saving** | **27,860 B** | **254 B** | **Static RAM unchanged at 1,632 B** |

This is **1,138 B saved** against the original 28,998 B playtest. At 27,860 B,
planning headroom is 1,140 B; hardware flash headroom is 1,836 B. Static RAM
planning headroom remains 68 B. These are prototype measurements, not a claim
that the production working tree has been integrated.

## Rejected additional probe

Sharing an already-loaded mission specification between completion,
acceptance and save validation looked simpler, but grew the whole image from
27,860 to 27,876 B (**+16 B**). Restored the smaller version. This illustrates
why reducing repeated source expressions is not sufficient evidence on AVR
with LTO.

## Verification

- Combined host: **236 passed, 0 failed**.
- Default host: **180 passed, 0 failed**.
- Tooling: **133 tests passed**. Generation is deterministic after local
  regeneration. The initial check against the copied PNG failed on font-file
  encoding bytes; decoded pixels were identical and the packed FX image was
  byte-identical. A second complete generation check passed.
- All eight combined Ardens suites passed: boot, chart, exploration,
  memory/music, music, render, spawn and world.
- Device music: gap=72 ticks, svc_us=88, underruns=0, errors=0.
- Device memory probe: stack_reserve=892, above the 600 B target. This remains
  the existing probe, not a proof of every stack path.
- Movement reference test covers every signed 16-bit velocity at five
  remainder boundaries, including both extrema.
- Sonar tests cover every byte-valued level and cooldown.
- Campaign tests assert milestone-table invariants and test all 256 reward
  values for every ordinary/express freight route; exact equality still
  rejects corrupt saves.
- Music test compares 100,000 frames of crossings and cue overrides against
  the preceding bounded-counter policy, including transition frames.

Working prototype: `/private/tmp/sub-hauler-direct-review-20261028/sub-hauler`.
Portable patch against the earlier combined prototype was written to
`build/direct-flash-review/additional-savings.patch` during the 2026-10-28
review. `build/` is disposable and that patch is not present in this checkout;
the review notes below are the retained record.
The patch dry-run applies cleanly to that baseline. Exact final sections:
`.text=27846`, `.data=14`, `.bss=1618`.
Measurements: `/private/tmp/sub-hauler-direct-{baseline,available,milestone,floor,narrow,prices,rewards,hysteresis,ring,sonar,spec}.log`.
Camera behavior, assets, save schema and music fidelity are unchanged. No
production-source edits, commit, physical-device playthrough or speaker test
is claimed by this review.

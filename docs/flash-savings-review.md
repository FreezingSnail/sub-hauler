# Review of the three largest flash users — 2026-10-28

This review uses the combined playtest snapshot identified in
[the flash index](flash-cost-index.md): **28,998 B flash / 1,631 B static
SRAM**, with campaign, voyage saves and music enabled and runtime USB disabled.
Voyage saves in this snapshot use EEPROM. The requested FX-save migration
requires a new measurement. Production source has not been changed by these
review prototypes.

## Measured recommendations

The three largest linked groups are rendering (7,156 B), main/loop (6,926 B),
and physics/rules/world (4,197 B). Inlining makes these ownership groups rather
than independent feature costs. Main-loop probes also touch UI drawing.

| Group and change | Measured flash saving | Static change | Correctness basis |
| --- | ---: | ---: | --- |
| Main: snapshot button edges once per frame | 248 B | +1 B | Single-button edges equivalent; preserve the library's separate multi-button-mask semantics |
| Main: dispatch screens with a switch | 56 B | 0 | One update per frame; retain every configured screen |
| Main: share station-board entry | 4 B | 0 | Same row, screen tag and draw sequence |
| Main: read held input from the polled state | 4 B | 0 | Same per-frame button byte as the library API |
| UI: merge repeated WORK clue-line drawing | 106 B | 0 | Same label selection, coordinates and draw order |
| **Main/UI combined, actually built** | **424 B** | **+1 B** | **28,574 B flash / 1,632 B static** |
| Render: share clipped rectangle drawing for chart markers/cells | 154 B | 0 | Preserve page layout and clipping; chart pixel/CRC tests |
| Render: outline repeated hazard/sonar glyphs | 68 B combined | 0 | Same pixels and clipping; individual inlining results interact |
| Render: omit impossible metadata-failure branches in generated-count loops | 4 B | 0 | Loop bounds equal generated table extents; dynamic checks retained |
| **Render combined, actually built** | **226 B** | **0** | **28,772 B flash / 1,631 B static** |
| World: arithmetic shift for floor pixel-to-tile conversion | 20 B | 0 | Compiler contract checked by static assertions; all 65,536 int16 inputs checked on host |
| Rules: avoid reclamping normalized cargo in internal helpers | 98 B incremental | 0 | Cargo clamped on entry; save cargo validated; public checked helpers remain |
| Jobs: remove clears of invisible offer rows | 98 B incremental | 0 | Readers use offerCount; each visible row fully initialized; inactive tails never serialized |
| **Physics/world combined, actually built** | **216 B** | **0** | **28,782 B flash / 1,631 B static** |

Rows are measured probes or incremental steps, not a promise that independently
measured savings add.

### Combined result

Applying all accepted changes to the baseline and rebuilding measured
**28,114 B flash / 1,632 B static SRAM**: **884 B flash saved, 1 B static
added**. Sections are `.text=28100`, `.data=14`, `.bss=1618`. That leaves
**886 B below the 29,000 B planning ceiling**, 1,582 B below the hardware
limit, and 68 B below the static SRAM ceiling. It restores the 500 B flash
reserve with another 386 B available. The standalone savings sum to 866 B;
the measured combined saving is 884 B because of LTO/layout interactions.

Combined validation: **233 combined host cases**, **178 default host cases**,
**133 tooling cases**, deterministic generation, and **8/8 Ardens device
suites** passed. Chart and render CRCs are unchanged; music reports zero
underruns/errors with a 72-tick service gap. The existing memory probe reports
891 B reserve; it is not a proof of every possible stack path.

Combined report:
`/private/tmp/sub-hauler-review-combined-20261028/report.md`.
Portable patch:
`/private/tmp/sub-hauler-review-combined-20261028/patches/99-combined-review.patch`.
It applies cleanly to the pristine baseline and reproduces the tested source.
These remain isolated review prototypes; production integration and FX-save
remeasurement are separate work.

## Review findings

1. Repeated input-edge queries expand across the large screen dispatcher.
   Sharing the edge byte is useful, but replacing a multi-button
   `justPressed(mask)` with an ordinary any-new-edge check changes behavior
   when another masked button is held. The prototype explicitly preserves
   that distinction.
2. Chart drawing duplicates pixel loops for small solid rectangles already
   supported by the clipped framebuffer rectangle primitive. Reusing that
   primitive saves more than broadly forcing pixel helpers out of line.
3. Cargo values are normalized at state-entry boundaries. Internal trim,
   thrust and speed calculations need not repeatedly defend against the same
   invalid cargo value. Static range assertions protect the formulas; corrupt
   saved cargo is still rejected before it enters the state.
4. Offer count already defines which array entries are valid. Clearing every
   unused field before refresh and after removing an offer writes dead data.
   Preserve full initialization of each newly visible offer.
5. Generated-count loops provide a narrow opportunity to omit impossible
   accessor failures. This saves only four bytes here, but is retained.

## Rejected probes and limits

- The suspected repeated bounds check inside rectangle collision saves
  nothing: replacing the inner checked read increased the whole image by 6 B.
  LTO already eliminates the redundant check. Keep the original code.
- Forcing all pixel writes out of line increases flash by 66 B.
- A table-based WORK clue selection increases flash by 6 B.
- Removing unused metadata assignments and clearing fewer already-dead
  intermediate values produced byte-identical images in several probes.
- Removing clue content can save bytes but changes the game; those stubs are
  rejected and are not included in any accepted total.
- No measured bitfield or narrower-state-field win is claimed. World/camera
  coordinates require more than eight bits, air capacity is now 300, and Q8
  velocities require signed 16-bit values. Packing may add masking code.
- Dynamic clipping, collision/cache fallback, persistence validation and
  hardware sequencing remain protected. Tests can establish fixed authored
  invariants; they cannot replace handling of actual runtime failures.

## Evidence and artifacts

Render: 231 host cases, 133 tooling cases, deterministic generation and 8/8
Ardens suites passed. Chart CRCs are 8487/3CAC/9BE4/9AE4; reported stack probe
reserve is 891 B. Physics/world: 233 combined host cases, 178 default cases,
deterministic generation and world/spawn/exploration device suites passed;
full device/stack gate was not rerun in that isolated slice. Main/UI: 231 host
cases, deterministic generation and boot/memory/render/spawn device suites
passed. WORK screen lacks a dedicated pixel-diff check. No physical-device
playthrough is claimed.

Detailed reports and portable prototype patches:

- `/private/tmp/sub-hauler-review-render-20261028/report.md`
- `/private/tmp/sub-hauler-review-render-20261028/patches/00-render-savings-combined.patch`
- `/private/tmp/sub-hauler-review-physics-20261028/review/report.md`
- `/private/tmp/sub-hauler-review-physics-20261028/review/patch-bcu.diff`
- `/private/tmp/sub-hauler-review-main-20261028/report/REPORT.md`
- `/private/tmp/sub-hauler-review-main-20261028/report/patches/99-combined.patch`

The camera improvement remains undispatched. No commit or deployment is part
of these isolated review prototypes.

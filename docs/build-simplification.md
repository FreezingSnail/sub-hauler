# Fixed build and consolidation design

Proposed on 2026-10-29 at the user's request. Beads owns task status and
dependencies. Implementation status: the fixed build is implemented; entry
separation and switch removal preserved the release size at **29,490 B flash /
1,662 B static**; the retained freight-offer consolidation finishes at
**29,436 B flash / 1,659 B static** with the same generated cart hashes. This document retains the
design contract and adds measured evidence where the implementation differs.

**Current status, 2026-11-01:** the shop, currency, job payouts, repeatable
freight offers and credit caches were removed after this design was executed.
Later sections that discuss a freight offer, shop row or schema-6 save record
are the 2026-10-29 implementation record, not current configuration; current
behavior and the schema-7 save contract are in
[platform.md](platform.md), [progression.md](progression.md) and
[progression-implementation.md](progression-implementation.md).

## Intended result

Sub-hauler has one complete game configuration. Stations, campaign,
exploration, fog chart, hazards, damage, particles, the fixed HAULER hull,
schema-7 FX saves, startup splashes and ATMlib2FX synth music are unconditional.
The shop, currency, job payouts, repeatable freight offers and credit caches
were removed on 2026-11-01.
The release entry always uses `ARDUBOY_NO_USB`. FX and Mini remain board
targets for the same game. The player's existing MUSIC setting remains a
runtime setting.

Remove the profile system, development noclip mode, music-off configuration,
streaming comparison backend, standalone EEPROM chart writer, alternate menu
renderers and project diagnostic selectors. Do not replace them with a new
configuration header, constexpr feature booleans or runtime feature switches.

Compiler/platform distinctions such as `__AVR__`, include guards, necessary
optimization options and dependency API configuration are not game variants.
Keep `-mrelax`, `-mcall-prologues` and LTO. Fix ATMlib2FX's required compact
loop/ring settings in the build recipe; expose no backend or tuning selector.
Tool locations, output paths, board targets and test selection remain ordinary
build inputs, not feature switches.

## Evidence and constraints

The 2026-10-29 audit rebuilt the current tree at **29,490 B flash / 1,662 B
static RAM** (`.text=29478`, `.data=12`, `.bss=1650`). Omitting all fourteen
explicit feature defines and using their identical full defaults produced the
same HEX bytes, SHA-256
`b7706e8da41b935b97c8c5bb9b3ebc441ee26497fe9c85d8d635241caff3d9a5`.
This proves no saving from omitting those defines. Equivalent guard removal
should likewise have no intrinsic cost or saving. Establish a fresh baseline
when implementation starts; the current measured value differs from the last
dated platform checkpoint, without an attributed cause in this audit.

The hardware ceiling is 29,696 B flash; static RAM must stay at or below
1,700 B and measured stack reserve at or above 600 B. Use whole-image deltas,
not summed symbol sizes. Keep the pure rules/device boundary and the existing
SPI transaction ownership. Do not combine the framebuffer, chart or live
simulation storage with UI transients.

At that 2026-10-29 baseline the task was to keep schema 6, its
feature-signature value, scalar/payload layout, CRC and commit
semantics, validation of untrusted save bytes, the then-current freight boards,
and all shipped FX declarations and bytes. Internal structs can change while
explicit serialization preserves the on-cart format. Do not remove generated
ADPCM assets or their declarations just because the comparison player retires;
`make gen` must reproduce the existing image and offsets. The reference capture
and ADPCM encode are archival evidence: rebuild them explicitly with
`make gen-audio`, not as part of `gen`, `build`, `run` or `gen-check`.

## Entry points and test instrumentation

First separate the release entry from the shared application functions. A
small `sub-hauler.ino` owns `ARDUBOY_NO_USB`, `setup()` and `loop()`; a shared
application header owns the same state and game operations. Device suites
include those operations from their own fixed USB sketches. There is no USB
override macro in production and no generated or text-patched game variant.
Keep ordinary internal linkage; avoid virtual dispatch and runtime hook state.

Separate hardware initialization from boot presentation so fixed device
fixtures can initialize FX/text/music and exercise the real operations without
needing the title loop. Release startup still calls both in the same order.
This also lets the flight probe link the operations it actually exercises
without disabling save/campaign features. Test reachability may let LTO omit
unexercised screens; it must not substitute an alternate flight implementation.

Remove `SH_FLIGHT_PROBE` and its scene/HUD timing globals from the game. The
flight suite times the real full update/render/display/service interval from
outside. Preserve its 842 measured frames, cold/warm paths, eight Y remainders,
six cues, camera boundaries, movement cases and audio assertions. Per-phase
timings may move to emulator tracing; they must not introduce a shipping hook.
Use the existing native Ardens harness approach if the fixed USB fixture
cannot fit; do not weaken scenarios or claim a reduced build as production.

Diagnostics belong to dedicated test translation units. Read synth diagnostic
state from a test adapter, with fixed dependency diagnostics for that test
target, rather than exposing diagnostic selectors through `music.hpp`.
Replace `SH_SAVE_RUNTIME_PAGE_TEST` with a separate named emulator-only suite
sharing save-test helpers. Stack suites always print their numeric diagnostic
and a separate P/F marker; remove `SH_STACK_DIAG`. Keep emulator-only save
probes isolated from physical-cart commands.

## Removal boundaries

`src/features.hpp`, `tools/features.py` and their profile tests retire.
Resolve every gameplay branch to the complete game; resolve SAVE_CHART and
DEV_MODE branches to absent. Remove `SH_VOYAGE_UI`, `SH_MUSIC_STREAMING`,
`SH_RELEASE_NO_USB`, `SH_STATION_MENU_PANELS` and `SH_SHOP_MENU_PANELS` along
with the branches they select. Baked menus become the only device renderer;
keep any pure row/layout helpers required by host tests.

Retire `src/dev_mode.hpp`, `src/chart_save.hpp` and tests exclusively for their
removed behaviors. Remove sampled/streaming backend code, the local sampled
runtime, obsolete admission suites, audio comparison executables and obsolete
stack-specific scaffolding, and the ArduboyStreamFX checkout dependency. Retain
or move the generic absim runner if the fixed probes need it. Keep the asset
generation/reference
pipeline and its relevant native/tool tests, including any codec helper needed
to reproduce shipped bytes. Remove retired references rather than leaving dead
build recipes or selectable comparison tools.

Replace the cart-service selection macro with a fixed inline synth service
function at the existing call sites. Preserve calls and ordering; changing
service frequency is a separate timing-sensitive optimization.

The Makefile exposes one firmware path and one host suite. Remove PROFILE,
FEATURE_FLAGS, DEV_MODE, RELEASE_NO_USB and MUSIC_BACKEND controls; profile
report/check/matrix targets; `dev`; and comparison recipes. Use distinct fixed
FX and Mini output locations (`build/arduino` / `dist/fx` and
`build/arduino-mini` / `dist/mini`), `build/tests/host` for the native suite,
and separate test build directories. Remove
variant guards from host/device tests. Preserve all assertions about retained
game behavior; retire only assertions tied exclusively to removed variants.
Update generation readers if they depend on source spellings; regenerate only
through `make gen` and require byte-identical artifacts.

Current README/platform/dev-flow/help/AGENTS instructions must describe the
fixed build. Keep historical measurements clearly historical. Current plans
must not recommend retired profile, USB or development overrides.

## Consolidation candidates after removal

The removal phase establishes a verified baseline before any representation or
behavior-neutral optimization. Evaluate the following candidates separately,
in the listed order. These are opportunities, not promised byte savings.

| Candidate | Concrete change | Expected opportunity | Required proof |
| --- | --- | --- | --- |
| Boot status | Keep one BootState; derive valid/corrupt/incompatible predicates | 3 B less declared state | Same four title states, Continue availability and replacement confirmation |
| Freight offer | Store destination and reward for the single ordinary four-crate row; derive kind, crates and origin from rules/current berth | Up to 3 B less declared state; simpler acceptance | Same destinations, rewards, acceptance, abandonment, delivery and reload boards |
| Screen transients | Audit title row/mode, ending card and confirmation lifetimes against existing UiRow | A few bytes possible; unmeasured | Transition/lifetime table including failure retry/resume, title return and ending replay |
| Save/music operations | Factor repeated transaction pause/resume and result handling into direct named operations | Flash saving possible; unknown | PauseToken semantics, mute/pause retention, readback and failure screens identical |
| Contract handling | Share proven common freight/campaign cargo setup and clearing | Flash saving possible; unknown | Mission eligibility, single payment, intermediate berths and trim behavior identical |

Boot-status and freight estimates are nominal AVR field counts. Whole-image
RAM and flash must still be measured; deriving a field can increase code size.
The offer shape is derived state, not part of the saved payload. Keep the
active job's serialized fields in this pass; removing them adds validation and
compatibility work without a demonstrated win. Keep the saved offer cursor
semantics, including wraparound. Do not compact unrelated counters or make a
new generic menu/transaction framework as part of this work.

For UI storage, retain the station/shop row across save-failure handling. A
single shared row for every screen is unsafe because retry resumes that row.
Only overlap fields whose lifetimes are proven disjoint. Existing WORK,
pause and save-failure rows already share storage.

Retain an optimization only if it reduces flash with no RAM regression, or
reduces RAM with no flash regression, while preserving stack/timing targets.
Record rejected candidates as measured results and restore their source
changes. Do not promise a total flash saving or keep a larger abstraction just
because it removes source lines.

## Verification contract

Before removal, capture a fresh `make size`, full Mini build and HEX hashes,
generation hashes, full host/tool results and device suite names, stack and
flight diagnostics. After entry separation and again after switch removal,
run `make gen-check`, `make test`, `make test-tools`, `make fxtest`,
`make size`, `make mini`, `make fxtest-save-chain`,
`make fxtest-save-runtime-page` and `make fxtest-flight`.

Aim for identical release HEX after entry separation and pure guard pruning.
Any difference needs a concrete explanation and behavior evidence. The
removal stage must not increase flash/static usage relative to its fresh
baseline. At that checkpoint the generated cart image and schema-6 records
stayed identical; the 2026-11-01 economy removal later introduced schema 7.

For each consolidation, run focused permanent native tests and whole-image
size first. State/save changes also run save/stack probes; renderer or cart
service changes also run the full flight probe. Finish with the complete gate
above on both board builds. Report exact test counts, numeric sizes, stack
reserves, flight timings and audio errors; explain expected test-count changes
from retired variants. An unavailable emulator is SKIPPED, not a pass; a
resource failure is BLOCKED with the deficit and measured options.

Search active source, tests, tools and recipes for retired switch names and
selectors. Review remaining preprocessor branches individually: only platform,
storage/compiler attributes or dependency-owned test diagnostics should remain.
No git operation, commit or push is authorized by this design.

## Consolidation results

### Screen transient lifetime contract (2026-10-29)

- `UiRow`: WORK selection lives only on WORK; pause selection only on Pause;
  save-failure selection only on SaveFailed.
  Station/shop selection remains separate because a failed shop save resumes
  the same item row.
- Title row and confirm-replace mode are live together on Title only. Ending
  card is live on Ending only. Title and Ending are never active together, so
  they could share storage, but packing title row+mode into one byte and
  aliasing Ending with `UiRow` measured **+14 B flash / -2 B static**
  (29,436/1,659 -> 29,450/1,657). Rejected: flash grew without a RAM-only
  tradeoff.
- No framebuffer, chart, live simulation, dirty/resume flag or station/shop
  row was aliased.

### State consolidation (2026-10-29)

- BootStatus enum-only representation measured **29,452 B flash / 1,663 B
  static** versus the 29,490/1,662 baseline: -38 B flash but +1 B static.
  Rejected/restored.
- Two-byte `{destination, reward}` freight offer with origin/crates/kind
  derived measured **29,436 B flash / 1,659 B static**: -54 B flash / -3 B
  static. Retained.

### Transaction and contract consolidation (2026-10-29)

- Common `startCargoJob`/`clearCargo` helpers for freight and campaign
  measured identical whole-image size **29,436 B / 1,659 B**. No Pareto
  improvement, so the explicit paths were restored.
- A scoped `TransactionPause` guard around save/load/recovery measured
  **29,444 B flash / 1,659 B static**, +8 B flash with no RAM gain. Rejected
  and restored; the explicit `pauseForTransaction`/`resumeAfterTransaction`
  pairs preserve the same resume point.

# Development history

Sub-hauler was built in three broad stages: a browser design sandbox, a
measured device port that first shipped a cargo-packing contract loop, and a
campaign/restoration rewrite that removed the economy and most optional systems
to fit a single Arduboy FX image. The last two waves in the record are a
playtest-driven polish pass and a cart/UI compression pass.

This page reconstructs the sequence from the git log (84 commits, 2026-10-22 to
2026-11-11), the bead database (217 issues: 216 closed, one open), and the dated
checkpoints under [`docs/archive/`](archive/). It records why decisions were
made; it is not a specification. Current behavior lives in
[`platform.md`](platform.md), [`design.md`](design.md) and
[`dev-flow.md`](dev-flow.md). Retired designs are retained in
[`archive/`](archive/).

## Timeline at a glance

| Date | Phase | Main outcome |
| --- | --- | --- |
| 2026-10-22 | Browser baseline, M0–M5 | Feature-flag harness, baked world, flight physics, stations/jobs, cargo bank and packing screen |
| 2026-10-23 | M6–M9 | Fog chart, sonar, hazards, damage, shop, three hulls, pause/failure, release matrix |
| 2026-10-24 | First carve | Cargo packing removed; text and pilot art moved to FX; image drops to 17,548 B |
| 2026-10-25–26 | Exploration/campaign pivot | Authored 32-room map, equipment gates, native campaign, atomic voyage save, music exploration |
| 2026-10-27–29 | Story/save/music integration | Native story prototype chosen; USB-free release; synth backend; smooth camera; sonar/mines/hull damage cut |
| 2026-10-30 | Story decks and cleanup | Finalized animated beat decks; docs/archive structure; retired prototypes removed |
| 2026-11-01 | Fixed build and economy removal | Profiles/selectors retired; shop, currency, payouts and repeatable freight removed; schema 7 |
| 2026-11-10 | Playtest wave | 16 notes executed; art, story, handling, audio fades, cart reclamation, flight probe refit |
| 2026-11-11 | Cart/UI compression | Dead tables/hulls/UI assets removed; cold tables and runtime text moved to FX; 27,792 B image |

The project was not a straight port. Each time the image approached the flash
limit, a feature was removed or its data was moved to the cart rather than
raising the limit or rewriting the architecture. The final shape is the result
of those measured choices.

## 1. Browser sandbox

The starting commit (`379cbc0`, 2026-10-22) imported two browser prototypes and
the device-port plan:

- `spike/` — **Tight Haul**, an auto-forward cave flyer used to test cargo mass,
  collision height, mines and sonar. It had a fixed-step loop and Node tests.
- `spike-trim/` — the v2 **trim sandbox**: free 2D submarine piloting, ballast and
  trim, four settlements, station contracts, a shop, a cargo-packing screen and
  a `FLEET` reference view. This was the behavioral reference for the device
  port, not firmware.

The baseline also contained the M0–M9 roadmap, a feature-flag schema, a host
test skeleton, an FX data pipeline and the first device test. The browser
sandbox was explicitly a design probe; the device was to become a separate
128×64 composition, not a pixel-for-pixel port.

The browser work established the core feel that survived every later rewrite:

- Ballast is the vertical control; blowing costs air, flooding is free.
- Cargo changes mass, trim, acceleration and collision height.
- Slow, heavy movement is the point; there is no auto-forward.
- Docking is a deliberate low-speed action.
- Sonar and a fog chart support navigation rather than replace it.

## 2. Device port, M0–M9 (2026-10-22 to 2026-10-23)

The port epic (`sub-hauler-port`, 41 beads) completed in two days. Every
milestone left a build that could be measured or played, and every optional
feature was compiled behind a `SH_FEATURE_*` flag so flash pressure could be
answered by carving scope. The work was tracked in
[`archive/roadmap.md`](archive/roadmap.md).

### M0 — Feature-flag harness and budget guard

M0 added numerical feature flags with dependency checks, profile builds
(`full`, `lean`, `core`, `min`), a `make size-matrix` harness and a painted-stack
device probe. The sentinel proof showed that disabling a feature removed code
from the whole image, not just a runtime branch. The initial template measured
6,636 B flash / 1,214 B static in every profile because no feature code was
gated yet.

This was the most important architectural decision of the port: measure the
whole image, not per-symbol estimates. With LTO, independent symbol savings are
not additive, so every later change used a whole-image A/B comparison.

### M1 — Baked world and cave rendering

The browser world was 320×160 tiles; a byte grid would not fit SRAM. The port
baked a 6,400 B collision bitmap and a 256 B chart-water bitmap into the FX
cart, kept only a small visible-tile cache in RAM, and drew walls from an 8×8
tile atlas. Cart reads stayed in update/render, before the single OLED
transaction.

### M2 — Fixed-step pilot

M2 replaced the template walker with integer submarine physics: thrust,
reverse, blow/flood, trim, vertical drag, tile collision, impact handling and a
camera. The input contract was locked before menus depended on it: D-pad flies,
A and B stay reserved in flight, and auto-dock at low speed becomes the docking
action. No `float`, heap, `String` or dynamic containers were allowed.

### M3 — Stations, contracts and economy

M3 added auto-dock inside settlement rings, a 128×64 station board,
accept/abandon/deliver, deadlines, credits, refills and a permanent host
integration test for the complete loop. The first version auto-loaded four
coarse cargo units; the packing screen came later.

### M4–M5 — Cargo bank and packing screen

M4 generated validated cargo arrangements offline into a 3,648 B FX bank. M5
built the cutaway loading screen: move, rotate, support, undo, and an `A+B`
commit. The generated bank and its declaration order were kept for ABI
compatibility even after the runtime was removed. M6–M8 then added the fog
chart and sonar, mines/currents/damage/particles/shop, and the three-hull
roster with class-specific holds.

### M9 — Release hardening

M9 added pause and hull-failure handling, an optional EEPROM chart save
(default off, never shipped), and the first release matrix. At `4f66830` the
`full` profile measured **28,792 B flash / 1,690 B static**, with a **624 B**
stack reserve. The gate passed **156** host tests, **75** tooling tests, and
four device suites. The release notes explicitly did not claim a manual
playthrough; the evidence was an automated host soak plus device-suite smoke.

## 3. First carve: cargo packing removed (2026-10-24)

The first release candidate was playable, but the packing minigame did not
justify its flash and SRAM cost. The user chose removal, not further optimization. The
cargo-free path was already the `CARGO_LOAD=0` fallback: accepting a contract
auto-loads four coarse units, neutralizes trim, and waits for an explicit
`UNDOCK`.

The removal (`sub-hauler-gl1.1`) deleted the cutaway screen, `LOAD CARGO` row,
`A+B` commit chord, packing rules and runtime state. The shipped `cargoBank`
bytes and offset stayed for ABI compatibility. A follow-up resource pass then
moved surviving UI strings and pilot art to FX, narrowed physics
intermediates, generated the fixed route economy data, and compacted the AVR
run state.

The measured result was **17,548 B flash / 1,572 B static**, down from 28,930 B
at the start of the carve. Cargo removal itself saved about 8,734 B; the
follow-up pass saved another 2,648 B. The browser sandbox was retired from the
shipping tree (it remains in git history).

Two habits were established here and never abandoned:

- A feature must earn its flash and SRAM. Removal is a valid design decision.
- “Make it fit” is a separate step from “implement it”; measure the whole image
  after each change.

## 4. Exploration and campaign pivot (2026-10-25 to 2026-10-26)

After the cargo-free release, the game direction changed from repeatable freight
to a relaxed restoration campaign. The legacy twelve-chamber cave was replaced
by an authored map in [`levels/exploration.json`](../levels/exploration.json):
32 rooms, 42 passages, nine gate records (six equipment gates and three tide
sections) and four equipment coves. The high-pressure region sits lowest; the
cutter, pressure seal, current thruster and survey lens open the deep routes.

The rendering approach also changed. `make gen` now bakes the static rock into
a **2,560×1,280 native 1-bit panorama (409,600 B)** and the runtime streams the
visible window directly from FX into the existing framebuffer. There is no
second framebuffer, no per-tile draw call, and no greyscale path. Wall berths,
piers, airlocks and inhabited cutaways are baked into the same image. Native
contours and regional scenery were added offline; collision remains tile-based.

The accepted progression design (`docs/progression.md`) defined eight main
objectives, two service berths, free recovery and a homecoming ending. Work is
untimed and unpaid; mandatory story work cannot be abandoned. The first voyage
codec was an atomic two-slot EEPROM save with schema validation and legacy
chart migration; it later moved to the FX cart save partition with a commit
byte and CRC, and the chart writer was retired.

Before committing to a story representation, the team built three prototypes
from the same frozen snapshot:

| Representation | Flash | Static RAM |
| --- | ---: | ---: |
| Native campaign control | 30,432 B | 1,663 B |
| Data-driven condition table | 30,566 B | 1,663 B |
| Small story event VM | 30,592 B | 1,721 B |

Neither alternative fit the full campaign/save firmware, so the native
representation stayed. The comparison is preserved in the `sub-hauler-cus`
beads and the archived story-resource notes.

Music followed the same pattern. Six original cues were authored as
ATMlib2FX scores. A sampled IMA ADPCM player was implemented and admitted as
comparison evidence, but the synth backend was selected for shipping because
the sampled path did not fit the combined budget and the synth could be tuned
to a 32 B ring. `make gen-audio` and the ADPCM files remain archival evidence.

The USB-free release entry saved another **2,668 B flash / 138 B static**; the
bootloader remains reachable through the library's startup `DOWN` check. The
release sketch always uses `ARDUBOY_NO_USB`; device test sketches keep USB for
serial markers.

## 5. Story, save and music under flash pressure (2026-10-27 to 2026-10-30)

The full voyage now combined campaign, checkpoints, story presentation and
music, and the image hit the hardware ceiling. The response was a sequence of
measured cuts and bake passes:

- Retire gameplay sonar: **−508 B**.
- Bake the remaining station/shop presentation into FX: **−514 B**.
- Add the Snailware and Sub-hauler startup splashes: **+100 B** after a
  **−180 B** camera/reader simplification.
- Enable the ATMlib2FX compact synth: the combined image reached exactly
  **29,696 B flash / 1,662 B static**.

The camera pass made scrolling smooth without a second framebuffer: Q4
fractional state, velocity look-ahead, projection before rounding, and a
terrain kernel that uses AVR hardware multiplies for sub-page Y origins. The
flight probe expanded to **842 complete frames** with active music; its later
gate required zero 16 ms misses and zero audio underruns.

Story presentation evolved from four static dock cards (plus the ending thanks
card) into animated beat decks. The final pipeline uses two canonical sources:
[`story/scenes.json`](../story/scenes.json) for scene order, prose and image-row
names, and [`story/art.png`](../story/art.png) for a single Piskel sheet of
four 128×64 poses per row. The generator validates the input, wraps prose at
twenty columns, paginates as needed, and packs RLE cards into the cart. At the
last committed checkpoint the source produced **17 scenes, 43 cards and 98
frames from 19 shared art scenes**.

Other cuts in this period:

- Mines and hull damage removed from the voyage build.
- Every story beat became WORK; no separate dialogue or mission backend.
- Sonar and its upgrade removed.
- Station/shop menus baked, then later removed from the cart entirely.
- Static story flourishes stopped being drawn; their generator and previews
  were kept as archival provenance.

On 2026-10-30 the repository was cleaned up: retired designs moved to
`docs/archive/`, the docs index was added, and disposable worktrees and
compiler caches were removed.

## 6. Fixed build and economy removal (2026-11-01)

The feature-profile system had served its purpose. The project then collapsed
to one fixed game configuration: stations, campaign, exploration, fog chart,
currents, particles, the fixed HAULER hull, FX saves, startup splashes and
synth music are unconditional. The developer `DEBUG=1` image is the only
separate build, and it is not shipped.

A design review on 2026-10-31 concluded that the shop added no meaningful
decision. Air was not a survival resource (free refill at every berth, free
rescue, enough tank for the longest route), credits had no sink outside the
shop, and fixed income left a large surplus. The decision was to remove the
economy rather than retune it:

- Shop screen, SHOP rows, air-tank levels and purchases.
- Currency, job payouts, rewards, deadlines and repeatable freight offers.
- Credit coves and generated reward art.
- Schema-6 save fields; schema 7 is a campaign-only save.

Campaign deliveries became intrinsic restoration work with no payment. The
`WORK` board lists only story and survey rows. The final measured image was
**27,230 B flash / 1,646 B static**, with **215** host tests, **128** tooling
tests, 14 device suites, a save-chain reserve of **702 B** and an 842-frame
flight probe at **11.600 ms** maximum.

## 7. Playtest wave (2026-11-10)

The 2026-11-10 playtest produced sixteen notes; the epic also tracked cart
reclamation and a flight-probe refit. The tree was checkpointed at `7f06e9a`
before the wave, including the new pixelgen source bundles.

Changes shipped in this wave:

| Area | Bead | Outcome |
| --- | --- | --- |
| Tides | `646.1` | Tides stay visible and push after the thruster; thrust can still fight upstream |
| Dock hints | `646.3` | One progression bark line at all eight berths; shared selector saves 86 B |
| Dock visuals | `646.4` | Native board chrome and berth treatment; row semantics unchanged |
| HAULER art | `646.5` | Four pilot frames redrawn in cutscene style, still 26×8 |
| Opening | `646.6` | New Voyage plays a Benthos-direction cutscene; Continue skips it |
| Music | `646.7` | 160 ms fades at cue, dock, story and pause transitions |
| Cove art | `646.8`, `646.9` | Four unique wreck landmarks and a lens-cave retexture; collision unchanged |
| Air/thruster | `646.10`, `646.11` | 300-unit tank, 4 air/s blow cost; thruster doubles speed/accel and ballast rate |
| Relay/HOME art | `646.12`, `646.13`, `646.15` | Unique DEEP RELAY briefing/result and HOME departure art |
| Survey notice | `646.14` | Survey bark outranks the inactive-berth repair notice |
| Ending | `646.16` | Voyage-stats card cut; network and thanks remain |
| Cart | `646.18` | 1,424,640 B of archival ADPCM streams removed from the shipping cart |
| Flight probe | `646.19` | Probe refit under the 29,696 B sketch limit |
| Sketchbook | `646.17` | Offline dock-bark preview added |

The integrated gate after the wave measured **29,652 / 29,696 B flash (44 B
free)**, **1,665 B static**, **264/264** host tests, **166** tooling tests,
**15/15** device suites, save-chain **682 B**, runtime-page **P**, and the
842-frame flight probe at **11,792 µs** maximum with zero misses and zero audio
errors. The story source produced 17 scenes / 43 cards / 98 frames; the cart
was **672,205 / 676,352 B** before the compression wave.

The playtest backlog is preserved in
[`playtest-backlog-2026-11-10.md`](playtest-backlog-2026-11-10.md). It keeps the
original player wording, the evidence, the decision and the bead for each note.

## 8. Cart and UI compression (2026-11-11)

The final committed wave reclaimed cart and program flash without changing the
game:

- `jxk.1` removed legacy cart tables and the retired MINNOW/LEVIATHAN pilot
  art. This is the documented one-time exception to the append-only FX ABI:
  the removed bytes were no longer referenced, and the offset shift is recorded.
- `jxk.2` removed dead menu/shop/title assets and compacted the remaining
  title sheets.
- `jxk.3` compacted the HUD notice sheet and removed the unused control row.
- `oad` moved cold story/dock tables to the FX cart and trimmed the debug
  overlay.
- `39u` removed the redundant berth air gauge and status.
- `2va` replaced runtime UI text drawing with baked 128×8 plus-mask FX stamps.
  `fxdata/TextData.txt` is now generated from the same string sources; the
  release image uses `Arduboy2Base` and makes no font draw calls.
- The last commit documented the single-return rule for 24-bit label
  selectors: assign the address once and return it at the end to avoid
  duplicate 3-byte constant loads.

At the last committed checkpoint the image is **27,792 B flash / 1,655 B
static**, leaving **1,904 B** under the 29,696 B hardware limit and **45 B**
of static headroom. The text-stamp pass grew the cart to a **637,779 / 642,048
B** payload/image while removing 1,354 B of program flash; the 4 KiB save
section is intact.

## 9. Resource history

| Date | Checkpoint | Flash | Static RAM | Note |
| --- | --- | ---: | ---: | --- |
| 2026-10-22 | Template | 6,636 B | 1,214 B | Before feature code |
| 2026-10-23 | M9 release | 28,792 B | 1,690 B | Four profiles, 156 host tests |
| 2026-10-24 | Cargo-free + resource pass | 17,548 B | 1,572 B | Packing removed; FX UI/pilot art |
| 2026-10-26 | Progression baseline | 20,176 B | 1,583 B | Campaign/save/music audit |
| 2026-10-28 | Menu baking + synth | 29,696 B | 1,662 B | Exactly at the hardware limit |
| 2026-10-28 | Startup splashes | 29,616 B | 1,662 B | 80 B free before later art |
| 2026-10-30 | Story flourishes | 29,604 B | 1,660 B | 92 B free; 842-frame flight |
| 2026-11-01 | Economy removed | 27,230 B | 1,646 B | Schema 7; fixed build |
| 2026-11-10 | Playtest gate | 29,652 B | 1,665 B | 44 B free; 15 suites |
| 2026-11-11 | Compression wave | 27,792 B | 1,655 B | Text stamps, cold tables |

The early profile matrix is intentionally absent after 2026-11-01: the game no
longer has profiles. Measurements from retired designs remain in
[`archive/`](archive/) as evidence, not as current limits.

## 10. Why the history matters

The final build is small because the project repeatedly chose to remove
features rather than defend them:

- The cargo-packing minigame was implemented, tested and then deleted.
- Sonar, mines, hull damage, the shop, currency and repeatable freight were
  all shipped at some point, then removed when their value did not justify
  their resource cost.
- The browser sandbox and several story/audio prototypes were retired once
  their questions were answered.
- Every cut was measured on the whole image; several plausible source-level
  optimizations were rejected because they grew the binary.

What survived is the part that carried the game: ballast-and-trim flight,
authored cave exploration, equipment gates, a short restoration campaign,
atomic cart saves, animated story cards, six music cues, and a strict native
1-bit presentation. The engineering method behind that is described in
[`making-of.md`](making-of.md).

## Appendix: commit and bead map

| Phase | Commits | Beads |
| --- | --- | --- |
| Browser baseline and device port | `379cbc0`–`4f66830` | `sub-hauler-port` (41) |
| Cargo removal and resource pass | `7d5ff5b` | `sub-hauler-gl1` (8) |
| Exploration and campaign | `cdd11a5`–`abe1ad1` | `sub-hauler-7px` (31) |
| Story prototype comparison | Prototype worktrees, 2026-10-27 | `sub-hauler-cus` (3) |
| Story/save/music integration | `0f739ee`–`9fa8f7f` | `sub-hauler-7px.9`–`.29` |
| Repository cleanup | `3bc34ed` | — |
| Economy removal | `f1b38e3`–`cbbf2e3` | `sub-hauler-9q0` (3) |
| Playtest wave | `7f06e9a`–`a040071` | `sub-hauler-646` (18) |
| Cart/UI compression | `e8d8aa2`–`3c1ba0b` | `jxk`, `oad`, `39u`, `2va` |

The only open issue at the time of writing is `sub-hauler-3ca` (story geography
and progression handoffs); it is not part of the committed history above.

## See also

- [How sub-hauler is built](making-of.md) — pipelines, tests and working method.
- [Platform and resource contract](platform.md) — current limits and measurements.
- [Design](design.md) — current game direction and contract loop.
- [Development flow](dev-flow.md) — repository conventions and commit gate.
- [Playtest backlog](playtest-backlog-2026-11-10.md) — the 2026-11-10 notes.
- [Archived roadmap](archive/roadmap.md) — the M0–M9 port sequence.

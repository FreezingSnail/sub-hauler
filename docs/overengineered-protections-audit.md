# Over-engineered protection audit

Date: 2026-11-12
Baseline: `93d0a34` (`Simplify voyage saves to raw single-slot loads`)
Status: accepted and dispositioned by epic `sub-hauler-bwg`; implementation is
split across `sub-hauler-bwg.2` (runtime), `.3` (assets), `.4` (documentation)
and `.5` (integration gate). This is not a claim that every item is already
verified or measured; the integration worker records the fresh whole-image
result after the full gate.

The save-loader rework removed schema, CRC, commit and semantic validation.
This audit looks for the same pattern elsewhere: compatibility, guards,
counters, result types and metadata that no longer have a consumer.

## Summary

| Priority | Finding | Accepted effect |
| --- | --- | --- |
| 1 | Dead unsaved-progress flag and departure save | Remove unreachable feature path |
| 2 | Counters with no reader | Counters alone: `State` 77->69 B, scalar 25->17 B |
| 3 | Dead delivery feedback | Remove `lastDelivery` and result plumbing |
| 4 | Retired hull compatibility | Remove legacy IDs and saved hull field |
| 5 | Legacy HOME compatibility | Remove pre-linear save path |
| 6 | Test-only save validation | Remove dead contract API and tests |
| 7 | Write-only save header/CRC/commit and skip cache | Remove dead format/protection layer |
| 8 | Duplicate status/result types | Remove one-value enum and duplicate boot state |
| 9 | Minor dead API/assets | Retire with the asset worker |

Items 1-3 are behavior-neutral cleanup. Items 4-6 rely on the stated no-release
baseline. Item 7 is the only structural save-format change and needs a fresh
whole-image measurement.

## Accepted disposition

The user accepted the cleanup as a pre-release change on 2026-11-12. No old-save
import or migration is required.

| Finding | Accepted disposition |
| --- | --- |
| 1 | Delete `s_voyageDirty`, the departure save, the unsaved-discovery notice branch and its stamps. |
| 2 | Delete `voyageTicks`, `delivered`, `rescues`, `tickVoyage()`, `countDelivered()` and their save bytes. Rescue becomes load-only. |
| 3 | Delete `lastDelivery`, `DeliveryResult`, `deliveryFailed()` and unused delivery return values. Keep a focused gameplay check of mission cargo metadata, destination and the fixed four-crate requirement. |
| 4 | Delete retired Minnow/Leviathan roster IDs, `isValidId()`, `normalize()`, `nameOf()`, the saved hull byte and the live hull field. |
| 5 | Delete `legacyHomeJob()`, the HOME exception and legacy mask allowances. HOME requires BEACON and DEEP RELAY. |
| 6 | Delete the whole-save validators and retired storage-range constants. Keep the raw-load memory-safety bounds; do not retain `activeJobValid()` as a second delivery gate. |
| 7 | Delete the header, CRC, commit byte, CRC skip cache and one-value `SaveResult`. Save raw scalar 16 + chart 256 = 272 meaningful bytes. |
| 8 | Collapse `BootStatus` to the meaningful blank/written truth and remove the unused ending draw argument. |
| 9 | Retire only unreachable generated frames, panels and stamps after checking generator references. |

Final save contract: one 4 KiB erase block, raw payload at offset 0. The first
16 bytes are scalar (`berth`, `tools`, `pickups`, `openGates`, `seenGates`,
`campaign`, `surveys`, `jobKind`, `jobMission`, `jobFrom`, `jobTo`, `jobCrates`,
`ending`, `pendingStory`); the chart follows for 256 bytes. First byte `0xFF`
means blank; every other record loads raw. There is no magic, schema, signature,
length, CRC, commit, cache or semantic validation. Each explicit save erases
once and programs two 256-byte pages with `0xFF` padding after the 272-byte
payload, even when the state is identical. Rescue loads only and writes zero
bytes. The blank/written title distinction, replacement confirmation and rescue
loss warning remain.

Arithmetic, to be confirmed by whole-image measurement: removing the three
counters alone accounts for 8 B (`State` 77->69 B, scalar 25->17 B); removing
them together with `lastDelivery` and the saved hull byte accounts for 10 B
(`State` 77->67 B, scalar 25->16 B). These are byte-count expectations, not
measured section sizes. Boot/cache-global removal should reduce SRAM further,
but only the integration worker's `make size` result is evidence.

## 1. Dead unsaved-progress machinery

`State` is never marked dirty:

- `s_voyageDirty` is only assigned `false` (`src/app.hpp:106,258,424,448,486`).
- The departure save in `leaveStation()` is therefore unreachable
  (`src/app.hpp:750`).
- The `UNSAVED FIND` branch in `drawPilot()` is unreachable
  (`src/app.hpp:192-193`).
- `requestSave()` only clears a flag that is already false
  (`src/app.hpp:256-259`).
- The then-current `docs/progression.md` still promised `UNSAVED FIND` on an
  expedition discovery.

Action: remove `s_voyageDirty`, the departure save branch, the unsaved notice
branch and the unused unsaved-progress stamps. If the notice is still wanted,
restore the setter instead; do not keep the current dead state.

## 2. Counters with no reader

These fields are incremented, serialized and loaded, but nothing renders or
branches on them:

- `voyageTicks` (`src/game.hpp:189`, `src/voyage_save.hpp:156-159`)
- `delivered` (`src/game.hpp:190`, `src/jobs.hpp:38-40`)
- `rescues` (`src/game.hpp:191`, `src/voyage_flow.hpp:198-200,228-230`)

`ending_view` is two static cards and explicitly says the voyage-stats overlay
was retired (`src/ending_view.hpp:10-13`). The then-current docs still described
three ending cards and voyage statistics.

Action:

- Delete the three fields and their save scalar entries.
- Delete `game::tickVoyage()` and both calls from the step paths.
- Delete `jobs::countDelivered()`.
- Stop incrementing `rescues`; `recoverCheckpoint()` becomes load-only, so
  recovery no longer needs a post-load checkpoint save.
- Update the ending/save docs to the two-card, no-stats contract.

Arithmetic: `sizeof(game::State)` is pinned at 77 B by
`tst/fxdatatest/test_memory.ino:60` at the audit baseline; these three fields
account for 8 B before alignment, so the expected State is 69 B and the expected
scalar is 17 B after this item alone. Combining item 3 (`lastDelivery`) and
item 4 (saved hull byte) removes another 2 B: expected State 67 B and scalar
16 B. The whole-image and final `sizeof` figures come from the integration
gate, not from this arithmetic.

## 3. Dead delivery feedback

`lastDelivery` has no consumer:

- It is set by `jobs::deliver()` (`src/jobs.hpp:65`) but never displayed or
  serialized.
- `DeliveryResult` is returned by `deliver()`/`onDock()` and ignored by
  `station::dock()`.
- `deliveryFailed()` is referenced only by tests (`src/jobs.hpp:24-28`).
- `voyage_save::applyScalar()` clears it on load (`src/voyage_save.hpp:402`).

Action: remove `lastDelivery`, `DeliveryResult`, `deliveryFailed()` and the
unused return values. Make `deliver()`/`onDock()` void. This also removes the
stale “SHORT note” comment.

## 4. Retired hull compatibility

The game has one playable hull, but the save path still carries retired-hull
compatibility:

- `roster` accepts retired Minnow/Leviathan IDs and normalizes every value to
  HAULER (`src/roster.hpp:6-35`).
- `isValidId()` is test-only.
- `normalize()` is used only by load, and accepts any byte.
- `hullClass` is always HAULER in live state (`src/game.hpp:164-166`).
- `roster::nameOf()` is unused; `kClassCount` is test-only.

Given no release, delete the compatibility ID range, `isValidId()`,
`normalize()`, the legacy comments and the associated tests. Prefer also
dropping `State::hullClass` and the saved hull byte; nothing reads them for
gameplay.

## 5. Legacy HOME compatibility

`legacyHomeJob()` preserves a pre-linear save shape where HOME was accepted
with only BEACON (`src/campaign.hpp:321-333`). It feeds:

- `completeMission()`’s HOME exception (`src/campaign.hpp:378-379`).
- `activeJobValid()` (`src/campaign.hpp:491-492`).
- The save-mask allowances in `milestonesValid()`.

No release means no save can predate the linear DEEP RELAY requirement. Remove
the helper, the HOME exception and the legacy mask allowances. Update the
“save-compatibility path” text in `docs/progression-implementation.md`.

## 6. Test-only save validation

After load validation was removed, the following are no longer production
rules:

- `campaign::milestonesValid()`, `surveysValid()`, `checkpointValid()` and
  `stateValid()` have no production caller (`src/campaign.hpp:454-507`).
- `campaign::activeJobValid()` is used only by the `jobs::deliver()` guard
  (`src/jobs.hpp:56`) and tests.
- `story_view::validScene()` is test-only; `validCard()` is unused
  (`src/story_view.hpp:113-120`).
- `voyage_save::kPendingStoryMax`/`kPendingStoryNone` and the `app.hpp`
  static assert are leftovers from storage-side range validation
  (`src/voyage_save.hpp:165-172`, `src/app.hpp:43-45`).
- `campaign::recordSurvey()` and `isCargoMission()` are test-only;
  `missionForSurvey()` is dead.

Action: remove the validator block and its tests. Remove the `activeJobValid()`
guard from `deliver()`. Preserve actual delivery semantics with a focused
gameplay check against mission cargo metadata, the destination berth and the
fixed four-crate requirement; `completeMission()` already checks mission
metadata, completion and prerequisites. Remove the story/range leftovers.

Keep the memory-safety guards that raw loads now rely on:

- `voyage_save::positionAtBerth()` and `world::berthInfo()`.
- `story_view::sceneCards()`/`cardFrame()`/`cardKind()` range checks.
- `chart` and `world` bounds checks.
- `station_view::nameFor()` fallback.

These prevent out-of-bounds reads; they are not schema validation.

## 7. Write-only save header, CRC, commit and skip cache

At the audit baseline the save transaction wrote a 16 B header plus a commit
byte, but load never read them:

- Magic, schema, layout, content version, signature, length and CRC
  (`src/voyage_save.hpp:57-100,277-323`).
- Commit byte (`src/voyage_save.hpp:332-356`).
- CRC-16 (`src/voyage_save.hpp:115-129`) is used only by
  `CheckpointCache`.
- `CheckpointCache` (`src/voyage_flow.hpp:38-65`) and
  `saveCheckpointIfChanged()` (`src/voyage_flow.hpp:126-141`).
- `SaveResult` now has one value, `kSaved` (`src/voyage_save.hpp:107-109`).

The skip cache is a dead optimization for the accepted single-slot contract,
not a needed protection. The accepted format deliberately rewrites the slot on
every explicit save, including an identical state, so the cart write is
unconditional.

Accepted change, as one coherent block:

- Delete header, commit and CRC writes.
- Delete `CheckpointCache`, `checkpointCrc()`, CRC helpers and
  `saveCheckpointIfChanged()`.
- Delete `SaveResult`; make save/new-voyage/affinity helpers `void`.
- Keep a one-byte written/blank test. The first payload byte is the berth ID
  (0-7); erased `0xFF` is blank and every other first byte loads raw.
- Keep exactly one 4 KiB erase and two 256-byte page programs per explicit
  save, with `0xFF` padding after the 272 meaningful bytes, to preserve the
  runtime `FX::programSavePage` adapter and music pause boundaries.
- Measure whole-image flash/RAM before accepting the change.

## 8. Duplicate status/result types

- `BootStatus` carries both `state` and `hasValid`; they are always equal
  (`src/voyage_flow.hpp:28-37`).
- `continueAvailable()` and `needsReplaceConfirm()` both return `hasValid`
  (`src/voyage_view.hpp:42-44`).
- `SaveResult` is a one-value enum (see item 7).
- `ending_view::draw()` takes an unused `State` parameter
  (`src/ending_view.hpp:33`).

Action: collapse `BootStatus` to one enum or bool and derive the title frame
index from it. Remove the duplicate predicate. Remove the unused draw
parameter when touching the ending view.

## 9. Minor dead API and assets

Lower-value follow-ups:

- `campaign::markEndingPresented()` returns a guard result that production
  ignores; its HOME check is already guaranteed by the caller.
- `roster::nameOf()` is unused.
- `world::cacheContains()` is test-only.
- `recordSurvey()` duplicates the survey bit update already performed in
  `exploration::update()`; tests could set the bit directly or use the shared
  helper.
- Packed save-failure/unsaved assets remain after the UI removal. Candidates:
  `panel_save_retry`, `panel_save_continue`, `img_txt_unsaved_find`,
  `img_txt_unsaved_lost`, `img_txt_continue_unsaved`, `txt_save_failed`,
  `txt_save_corrupt`, `txt_save_incompatible`, `txt_save_ready`, `txt_retry`,
  `txt_salvage_hint`, `txt_unsaved_find`, `txt_unsaved_lost`, `txt_no_save`,
  `txt_save_hint` and `txt_title_save_hint`. Verify symbol references before
  retiring them.

## Keep

Do not remove these just because they look defensive:

- Raw-load out-of-bounds guards listed in section 6.
- `hasSave()`’s one-byte blank/written test.
- Startup splash release latch: prevents a held skip input leaking into the
  title menu.
- Station selection wrap and row bounds checks: gameplay behavior.
- Physics clamps and collision bounds: game rules, not save validation.
- `LoadResult`: blank and loaded are meaningfully different to the title flow,
  unless a later simplification makes it a bool.

## Accepted order

1. Remove the dead dirty flag, counters, `lastDelivery` and delivery result
   plumbing (sections 1-3).
2. Remove test-only validators and legacy compatibility (sections 4-6).
3. Collapse duplicate types (section 8).
4. Remove the write-only save format, CRC and cache (section 7); measure the
   whole image in the integration gate.
5. Retire the remaining dead API and packed assets (section 9).

## Resolved decisions

- `UNSAVED FIND` does not exist as a reachable production notice. Delete the
  path and its stamps; do not restore a setter for it. The rescue confirmation
  text `UNSAVED FINDS LOST` remains because rescue still rolls back unsaved
  finds.
- No development cart save must survive the legacy-hull or legacy-HOME removal.
  Old development saves are discarded for this cleanup; no migration is built.
- The save format changes before release. Remove the header, CRC, commit and
  cache rather than preserving a fictional ABI.
- The ending has two fixed cards. Remove the voyage-statistics overlay and the
  counters that fed it; do not restore stats.
- Documentation is reconciled in `.4`. Final whole-image, stack and flight
  evidence is recorded by the integration worker in `platform.md` and `.5`.

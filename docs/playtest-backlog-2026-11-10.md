# Playtest backlog — 2026-11-10

Tracking document for the 2026-11-10 playtest notes. Each numbered entry keeps
the user's original wording, the current implementation evidence, the proposed
change, open decisions, and the planned bead. Beads are the execution record;
this file is the human-readable index.

Status key: **plan** = not filed yet, **filed** = bead exists, **blocked** =
waiting on a decision or another bead, **done** = shipped and verified.

## Filed beads

All sixteen notes are filed under epic **sub-hauler-646**. Decision summary:

| Note | Bead | Decision |
| ---: | --- | --- |
| 1 | sub-hauler-646.1 | Tides stay visible and always push; thrust fights the flow; upstream blocking stays off. |
| 2 | sub-hauler-646.3 | All eight berths; one native progression bark line; reuse existing FX strings. |
| 3 | sub-hauler-646.4 | Improve the native station board; keep row semantics. |
| 4 | sub-hauler-646.5 | Redraw the four HAULER frames in cutscene style; keep 26x8. |
| 5 | sub-hauler-646.6 | Opening scene on New Voyage; skip on Continue; points to Benthos. |
| 6 | sub-hauler-646.7 | Fade cue, dock, story and pause transitions if the synth supports a ramp. |
| 8 | sub-hauler-646.8 | Distinct in-world wreck/scenery art at all four equipment coves. |
| 9 | sub-hauler-646.9 | Retexture the Hidden trench cove (lens pickup room). |
| 11 | sub-hauler-646.10 | Air tank 270; blowing costs 4 air/s; flooding stays free. |
| 12 | sub-hauler-646.11 | Thruster doubles horizontal speed/accel both ways and doubles ballast rate. |
| 13 | sub-hauler-646.12 | Unique DEEP RELAY acceptance art. |
| 14 | sub-hauler-646.13 | Unique DEEP RELAY site result art. |
| 15 | sub-hauler-646.14 | Survey interaction outranks the inactive-berth repair notice. |
| 16 | sub-hauler-646.15 | Unique HOME departure art. |
| 17 | sub-hauler-646.16 | Keep network + thanks; cut the voyage-stats card. |
| 18 | sub-hauler-646.17 | Read-only dock-dialog preview in the existing sketchbook; blocked by note 2. |
| cart | sub-hauler-646.18 | Remove archival ADPCM streams from the shipping cart; unblocks notes 13/14/16. |

Related work links: notes 13/14/16 share the canonical story sheet; notes 8/9
share generated exploration terrain; notes 1/12 both touch thruster-era
handling; note 18 depends on note 2.

### Completed in this pass

| Bead | Commit | Result |
| --- | --- | --- |
| 646.4 | `e90d33e` | Native dock board chrome; +302 B flash at the time. |
| 646.14 | `31f3d67` | Survey bark outranks the inactive-berth repair notice. |
| 646.16 | `40129ca` | Ending keeps network + thanks; stats card cut. |
| 646.3 | `7b6b14a` | All-berth progression bark; −86 B flash via shared selector. |
| 646.1 | `9f51022` | Tides stay visible/push after the thruster; no upstream block. |
| 646.10 | `a56fc86` | Air 300 / blow 4 per user decision; first run leaves 18. |

Integrated baseline after these six commits: **29,608 / 29,696 B flash**,
**1,665 / 2,560 B static RAM**, **88 B flash free**, host **256/256**.

## 1. Tides after the thruster

**Original note:** "when getting the thruster, the tides should still be
visible and push the player if they are motionless."

**Current evidence**

- `src/hazard_view.hpp` returns early when `THRUSTER` is owned, so all flow
  marks disappear.
- `src/currents.hpp::apply()` returns early for gated zones when `THRUSTER` is
  owned, so all tide force and upstream blocking stop.
- `src/hud.hpp`/HUD already exposes the owned-tool glyphs.

**Proposed change**

- Keep drawing tide flow marks after the thruster is owned.
- Keep applying tide acceleration after the thruster is owned, but only while
  the player is not commanding horizontal thrust (or at least so normal thrust
  can still overcome the flow).
- Never restore upstream blocking once the thruster is owned.
- Cap post-thruster drift so an idle player is nudged, not swept into rock.

**Decision:** the tide always applies after the thruster is owned; the player's
thrust is expected to overcome it upstream. Upstream blocking stays disabled.
Keep the existing drift cap and verify the strongest authored tide against the
new thruster acceleration from note 12.

**Filed:** sub-hauler-646.1. Bug fix in `currents.hpp` + `hazard_view.hpp`,
with host tests for idle drift, upstream travel under thrust, downstream
travel, and flow-mark visibility. Verification: `make test`, `make fxtest`,
`make fxtest-flight`, `make size`.

## 2. Per-dock dialog driven by story progression

**Original note:** "need to investigate if can have some dialog when dock at
any dock based on story progression. ideal each port could have its own dialog
giving a hint about the next story bit."

**Current evidence**

- Docking currently opens the station board; story only plays for milestones,
  surveys, deliveries and the ending.
- `src/campaign.hpp` already derives `NextStop` guidance; station hints are
  generated stamp rows selected by `station_view::panelHint()`.
- `src/story_view.hpp` owns generated scenes; `src/app.hpp` owns playback and
  the one-byte pending scene.

**Proposed change**

- Add a small, data-driven dock-dialog layer: one short scene per main berth,
  selected from saved campaign facts, shown once on first dock at that
  progression state (or every dock until the state advances).
- Candidate triggers: Tidewall, Benthos, Cormorant, Deep Anchor; optionally the
  four service/work sites.
- Reuse the existing story card renderer and FX blob; do not add a second text
  or image pipeline.
- Keep mandatory story decks, delivery decks and the ending higher priority.

**Decision (2026-11-10, clarified):** one native one-line dock bark at all
eight berths, for example "BENTHOS HAS WORK" or "WRECK NEARBY". The line is
selected from saved campaign/tool facts and replaces the station board's bottom
hint row. No story scenes, art rows, pendingStory values, or seen flags. Reuse
existing FX clue strings so the 41 B cart headroom is not consumed.

**Cost note:** the earlier 64-scene story model needed +504 B flash (tables
+384 B, code +120 B) and was rejected. The bark is code-only and much smaller;
its exact flash delta is measured by the bead.

**Filed:** sub-hauler-646.3. **Done:** committed `7b6b14a`; all eight berths
return a pure `campaign::dockBark` line and the station board draws it at
`y=56`. The shared selector also replaced duplicate WORK clue logic, so flash
fell 86 B to **29,608 / 29,696 B**. No new FX strings or story scenes. Note 18
depends on this bead; its preview should show the bark lines.

## 3. Improve the dock menu visuals

**Original note:** "need to improve the visuals of the doc menu."

**Current evidence**

- The runtime station board is native text on black (`src/station_view.hpp`);
  generated panel frames remain archival cart ABI but are not drawn.
- Header, air status, rows and a generated hint row are the whole composition.

**Proposed change**

- Improve the station board composition without changing row semantics:
  berth-specific header/divider, clearer status cluster, selected-row treatment,
  and a berth silhouette or small native icon.
- Keep the live WORK/UNDOCK layout and the existing 128×64 text metrics.
- Reuse generated FX panels or draw from native primitives; no new backend.

**Decision:** native redraw, not a new baked panel set. Keep the industrial
console/berth identity and produce a native-size preview for review.

**Filed:** sub-hauler-646.4. UI/art feature. Verification includes host menu
tests, a 128×64 preview, `make size`.

## 4. Player sprite should match the cutscene sub

**Original note:** "need to update the sub player sprite to match how it looks
in the cutscenes."

**Current evidence**

- Player art is a 26×8 blocky plus-mask silhouette generated by
  `tools/gen_pilot.py`; collision footprint is 26×8.
- Cutscene art (`story/art.png`, especially `arrival`, `delivery-loading`,
  `deep-relay`) shows a rounded, detailed sub with portholes/fins.

**Proposed change**

- Redraw the four HAULER frames (right/left × empty/loaded) in the cutscene
  visual language while keeping the 26×8 collision footprint and the
  right/left × unloaded/loaded frame ABI.
- Use `@pixelgen-assets` for the 128×64 source stills, then downscale/compose
  into the 26×8 sprite sheet; keep `pilot_view` host pixels in sync.
- Preserve readability at 1× on the OLED and keep the loaded/unloaded cue.

**Decision:** keep the 26×8 collision footprint and frame ABI. Use the
cutscene visual language (rounded hull, portholes, fins) rather than copying one
row pixel-for-pixel.

**Filed:** sub-hauler-646.5. Art + generator change in `tools/gen_pilot.py`,
plus host pixel tests in `tst/pilot_view_test.hpp` and `make gen-check`.

## 5. Opening cutscene: dock at Benthos for work

**Original note:** "need an initial cutscene telling the player to dock at
benthos in front of them for work."

**Current evidence**

- New Voyage spawns in free flight beside Tidewall.
- FIRST departure currently plays only when the FIRST job is accepted at
  Tidewall; it explains filters and Benthos but does not appear at boot.

**Proposed change**

- Add one short opening scene at New Voyage that points the player at Benthos
  (the berth in front of the spawn) and tells them to dock for work.
- Reuse existing art if possible (`arrival` or `tidewall-filters`), or add one
  new row if the composition needs the player sub facing the route.
- Keep it skippable with A/B like every other scene.

**Decision:** show automatically on New Voyage, skip on Continue, advance with
A/B, then return control to the pilot. One work instruction is the target; add
a control card only if playtesting shows it is needed.

**Filed:** sub-hauler-646.6. Story/scene + spawn-flow feature. Authoring and
preview do not depend on note 18.

## 6. Fade music at transitions

**Original note:** "need to investigate fading the music at transistions."

**Current evidence**

- `src/music.hpp` hard-switches cue streams through
  `atm_fx_compact_play(...)`; `setCue()` restarts immediately.
- The compact ATMlib2FX backend exposes play/stop/pause/resume, but no
  application-level volume ramp in the current wrapper.

**Proposed change**

- Investigate whether the companion ATMlib2FX build exposes a master volume,
  per-channel gain, or a cheap envelope hook.
- If yes, add a short fade-out/fade-in around cue changes, docking/story
  transitions and the pause menu; keep it integer and bounded.
- If no, evaluate a short stop/restart cadence or accept hard cuts with a
  documented limitation.

**Decision:** fade song changes, dock, story and pause/resume transitions.
Target 150–300 ms. If the bundled backend has no volume/gain ramp, stop after
the investigation and report concrete options rather than adding per-frame
work blindly.

**Filed:** sub-hauler-646.7. One implement-verify loop with a mandatory backend
capability check first.

## 7. (Not present in the source notes)

## 8. Unique art around pickup upgrades and wrecks

**Original note:** "add unique art around the pickup upgrades, shipwreks
probably."

**Current evidence**

- Four equipment coves: cutter in the Abandoned tender, thruster in the Old
  relay compartment, seal in the Sealed workshop, lens in the Hidden trench
  cove (`levels/exploration.json`).
- In-world terrain and scenery are streamed from the same generated native
  terrain image; `scenery` entries include generic wrecks, pumps, cranes,
  crystals and vents.
- Pickups themselves are HUD notices, not scene art.

**Proposed change**

- Give each equipment cove a distinct in-world landmark/wreck silhouette
  (cutter tender, thruster relay pod, seal workshop lock, lens trench wreck)
  so the four upgrades read as different places.
- Prefer generated terrain/scenery art over per-pickup story cards; keep the
  collision map and pickup coordinates unchanged.
- Use `@pixelgen-assets` for any new 128×64 source art.

**Decision:** in-world cove scenery only, no extra pickup story cards. Each
cove gets a distinct wreck/landmark type while staying inside the existing
scenery language.

**Filed:** sub-hauler-646.8. One combined art/level-scenery bead; coordinates
with note 9 because both regenerate terrain/scenery.

## 9. Lens cave texture

**Original note:** "change texture of lens cave."

**Current evidence**

- The lens pickup is in the Hidden trench cove (`DL`, region `deep`).
- Region textures are generated from level terrain/scenery; "lens cave" is not
  a named room in the JSON.

**Proposed change**

- Confirm the intended room (Hidden trench cove versus Trench Refuge or Glass
  Chimney) and change its terrain/scenery treatment to a distinct lens-cave
  look.
- Keep collision bytes and pickup positions unchanged; regenerate the terrain
  image and update the world preview.

**Decision:** the lens cave is the Hidden trench cove (`DL`), where the survey
lens is picked up. The bead chooses the specific lens-cave texture treatment and
shows a render-world comparison.

**Filed:** sub-hauler-646.9. Level/art chore; coordinates with note 8.

## 10. (Not present in the source notes)

## 11. Air and blowing-ballast economy

**Original note:** "reduce air slightly and make blowing ballast cost more."

**Current evidence**

- `kAirCapacity = 300`; air drains one unit per 150 ticks.
- Blowing costs `kBlowCarryStep = 3` air units/s and changes ballast at
  `kBallastCarryStep = 18` units/s.
- First delivery currently has a tight air budget, so this is a difficulty
  adjustment, not just a number tweak.

**Proposed change**

- Reduce the fixed tank modestly and raise the blow air rate, then retune the
  starting/route air checks and tests.
- Keep flooding free and keep the berth refill to a full tank.
- Validate the first delivery remains achievable; if it does not, adjust routes
  or starting air rather than hiding the cost.

**Decision:** tank 270 units; blowing costs 4 air units/s; flooding stays free
(and so does its 18 units/s ballast rate). The first delivery must remain
achievable; report a deficit instead of hiding it.

**Filed:** sub-hauler-646.10. Balance task touching `game.hpp`, tests and
progression docs. Verification includes the full save/flight probes and a
first-delivery playthrough check.

## 12. Thruster speed and faster ballast

**Original note:** "let a make the ship move left and rate 2x speed after
getting the truster, and change balast faster."

**Decision:** after the thruster is owned, double horizontal top speed and
acceleration in both directions, and double the ballast change rate. Reverse
already exists; this makes it fast too.

**Current evidence**

- Reverse thrust is already allowed at half power
  (`power = -(thrustAccel / 2)`), so "move left" likely means "move left
  faster", not "unlock left".
- Top speed is 30 px/s empty, reduced by cargo; ballast changes at 18 units/s.
- The thruster currently only disables tide force/blocking.

**Proposed change**

- Add a thruster-owned speed multiplier (for example 2× horizontal top speed
  and acceleration) and a faster ballast rate (for example 1.5× or 2×).
- Keep collision tunneling impossible: `moveAxis` already walks pixel by pixel,
  but camera look-ahead and the flight probe must be revalidated.
- Keep the values inside the fixed-step integer model and update host proofs.

**Filed:** sub-hauler-646.11. Feature/balance in `game.hpp`, `pilot.hpp`,
HUD/camera tests, and `docs/platform.md` if tuning becomes canonical. The
multiplier applies after the cargo penalty and to both directions.

## 13. Lens-job acceptance art at the survey lens location

**Original note:** "the art shown when accept the lens job should be shown at
the survey lens location."

**Current evidence**

- The DEEP RELAY acceptance scene (`ID 16`) plays the `deep-relay` wreck art
  at Deep Anchor before travel.
- The expedition result (`ID 15`) plays the same `deep-relay` art at the
  lower-right site.

**Decision:** give the DEEP RELAY acceptance a unique image instead of reusing
the wreck art. The wreck/site illustration belongs to the result at the
lower-right relay site (note 14). This is piece one of the three unique story
art rows.

**Filed:** sub-hauler-646.12. Coordinated with sub-hauler-646.13 (site result)
and sub-hauler-646.15 (HOME departure) because all three append rows to the
same canonical sheet.

**Cart dependency:** each new 128×64 row costs roughly 2–3 KB compressed, but
the cart had only 41 B before the save page. New bead **sub-hauler-646.18**
removes the six archival ADPCM streams (~1.42 MB) from `fxdata/fxdata.txt`
while keeping the files and `make gen-audio` as evidence. `FX_SAVE_PAGE` stays
`0xFFF0`; this is a user-approved one-time exception to the append-only layout
rule. The three art rows land after 646.18.

## 14. Second-to-last job needs unique site art

**Original note:** "survy location at bottom second last job needs unique art."

**Current evidence**

- The second-to-last campaign job is DEEP RELAY (lower-right work site).
- Its result uses the same `deep-relay` image as the departure briefing.

**Decision:** "second last job" is DEEP RELAY; add one unique 128×64 site
result row showing the buried relay/wreck state. Append the row to
`story/art.png` and `art.rows`; never renumber existing rows.

**Filed:** sub-hauler-646.13. Story art + scene mapping, coordinated with
sub-hauler-646.12 and sub-hauler-646.15.

## 15. Wrong "REPAIR LIFT FIRST" notice at a survey site

**Original note:** "at survey location, bottom says 'repair lift first'
incorrectrly. then story shows survey scene correctly."

**Current evidence**

- The Freight Yard survey ring is next to the inactive Freight Yard service
  berth.
- `hud::noticeFor()` checks nearby inactive service berths after gates and
  returns `kNoticeRepairLift` before the survey hint can show.
- The survey itself records and plays correctly after the dwell.

**Decision:** the nearby unrecorded survey interaction outranks the
inactive-service-berth repair notice. Keep the genuine repair notice at the
lift when no survey is pending; do not show a misleading repair line at the
survey site.

**Filed:** sub-hauler-646.14. Bug fix in `hud.hpp` with a focused host test,
then `make test` and `make fxtest`.

## 16. Final job reuses a story-beat graphic

**Original note:** "for the final job, it resuses a story beat graphic."

**Current evidence**

- HOME departure (`ID 14`) uses the `delivery-loading` image, shared with PUMP
  pickup.
- HOME is the final cargo job before the ending.

**Decision:** "final job" is HOME. Add a unique departure illustration
(survey records/supplies loaded at Deep Anchor) instead of reusing
`delivery-loading`; keep scene ID 14 stable.

**Filed:** sub-hauler-646.15. Story art + scene mapping, coordinated with
sub-hauler-646.12 and sub-hauler-646.13.

## 17. Cut ending voyage stats and the last credit graphic

**Original note:** "can cut voyage stats at the end, and the last credit
graphic."

**Current evidence**

- `ending_view` has three cards: network, thanks, stats.
- Stats overlays voyage time, cove count and delivered jobs on
  `panel_ending_stats`; the thanks card is the final card.
- The user says "can cut voyage stats" and "the last credit graphic".

**Decision:** keep the network and thanks cards; cut the voyage-stats card
(which is the last graphic and the stats overlay). Keep the ending save flag
and replay path intact.

**Filed:** sub-hauler-646.16. Ending simplification + resource cleanup, with
story/save tests and `make size`.

## 18. Small HTML editor for story beats and dock text

**Original note:** "investigate a small html based editor that shows the sotry
beat images, and then the various text at each dock.. depends on 2."

**Current evidence**

- `docs/story-sketchbook.html` already previews scenes, animation and text
  pages; `docs/assets/story-cards.png` and `.json` show the complete sheet.
- Dock text does not exist yet (item 2).
- `story/scenes.json` and `story/art.png` are the canonical sources.

**Decision:** extend the existing story sketchbook as a read-only preview of
the per-berth bark lines once note 2 lands. Keep it offline and dependency-free.
If editing is later requested, export copyable JSON rather than writing
canonical files.

**Filed:** sub-hauler-646.17. Blocked by sub-hauler-646.3 (note 2).

## Cross-cutting constraints

- One fixed shipping build; no new feature selectors.
- Story prose and art stay canonical in `story/scenes.json` and
  `story/art.png`; new image rows are appended.
- FX declaration order is flash ABI; append only.
- Do not hand-edit generated files; run `make gen` and stage generated sets
  together.
- Full gate before commits: `make gen-check`, `make test`, `make test-tools`,
  `make docs-check`, `make fxtest`, `make fxtest-save-chain`,
  `make fxtest-save-runtime-page`, `make fxtest-flight`, `make size`,
  `make mini`.
- Keep flash/RAM/stack reserves explicit; measure every art or code addition.
- No worker commits or pushes.

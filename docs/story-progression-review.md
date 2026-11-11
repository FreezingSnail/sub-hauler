# Story progression and job geography review — 2026-11-03

Review of the current source, authored map, shipping story-copy source and host
tests. This is a proposal, not an implemented campaign change. No emulator or
human playthrough was performed for this review.

## Assessment

The restoration arc works: food supplies establish Benthos, the two settlements
exchange repair materials, repaired infrastructure supports a deep expedition,
and the player brings a reply home. The crossed repair routes give return
visits a purpose. Both repair orders are supported by the mission rules.

The missing material is mostly between objectives. The game has five deliveries
and two surveys, but its seven story decks concentrate on completion. Loading,
departure, the reason both repairs enable the beacon, and the reason to haul a
dispatch after communications resume need their own narrative moments.

Add six short story triggers at existing locations before adding map rooms or
mandatory delivery legs. This gives thirteen narrative nodes while retaining
the seven gameplay objectives. A node can be one brief text screen; it does not
need a new animated deck.

## Actual starts and ends

The mission table in [campaign.hpp](../src/campaign.hpp), field objects in
[exploration.json](../levels/exploration.json), and delivery rules in
[jobs.hpp](../src/jobs.hpp) define these routes:

| Objective | Request / collection | Actual completion | Assessment |
| --- | --- | --- | --- |
| FIRST | Tidewall, filter supplies | Dock at Benthos | Coherent; lacks an opening explanation before loading. |
| PUMP SURVEY | Benthos WORK lead; acceptance is unnecessary | Dwell at Pump Refuge | Coherent remote inspection. |
| PUMP | Cormorant, impeller | Dock at Benthos | Conflicts with the story card naming Pump Refuge as the destination. |
| LIFT SURVEY | Cormorant WORK lead; acceptance is unnecessary | Dwell at Freight Yard | Coherent remote inspection. |
| LIFT | Benthos, cable reels | Dock at Cormorant | Story names the Freight Yard lift; station handoff to its repair crew needs explaining. |
| BEACON | Cormorant, beacon core | Dock at Deep Anchor | Route works; the dependency on both repairs is unexplained. |
| HOME | Deep Anchor, network dispatch | Dock at Tidewall | Good return arc; cargo and communications fiction need clarifying. |

Pump Refuge and Lower Gantry are service berths opened by the repairs. Neither
accepts a delivery. Pump Refuge is in the pump survey room; Lower Gantry is a
different room from Freight Yard. The lift's restoration opening a separate
mooring needs a sentence connecting the two facilities.

## Findings

1. **Pump destination contradicts the rules.** The shipping copy in
   [story-sketchbook.html](story-sketchbook.html) says the impeller travels from
   Cormorant “TO THE PUMP AT PUMP REFUGE.” PUMP instead targets Benthos, and the
   refuge cannot even dock the player until that delivery completes. Keep
   Benthos as the handoff and say its maintenance crew takes the impeller to
   Pump Refuge. Likewise, make Cormorant the cable handoff for its Freight Yard
   crew. These are proposals for clarifying the existing routes, not existing
   crew-transfer mechanics.

2. **Lift-first guidance skips the unfinished pump.** In
   `campaign::nextStopAfter`, the LIFT case checks only the seal. With LIFT
   complete, PUMP incomplete and a seal owned, it returns `kNextStopBeacon`.
   [app.hpp](../src/app.hpp) maps that to `BEACON CORE READY`, even though
   `missionPrerequisites(kBeacon)` rejects the job. Without the seal it sends
   the player toward equipment without mentioning the missing repair. The
   current host test explicitly expects these hints, so passing tests do not
   establish narrative correctness. Choose the unfinished survey or repair
   before announcing the beacon, symmetrically for both orders.

3. **Guidance disappears at empty WORK boards.** In
   [work_view.hpp](../src/work_view.hpp), `draw()` returns when row count is
   zero before drawing `clueFor()` or equipment guidance. This can happen at
   Benthos immediately after PUMP with the lift survey still missing, or at
   Cormorant after both repairs while the seal is missing. A temporary notice
   after a deck does not replace a persistent, revisit-able lead. Show the
   next actionable place even when this station has nothing to load.

4. **Departure scenes play after arrival.** `updateWork()` accepts and saves a
   load, then returns directly to the station board. Milestone decks are
   triggered after delivery. Consequently the impeller and cable loading
   scenes play at their destination, after unloading; the beacon descent is
   also retrospective. Move loading scenes to acceptance and leave repair
   results at delivery. Merely adding cards to the existing completion decks
   would preserve this problem.

5. **The surveys imply an unplayed conversation.** Each survey deck immediately
   names a settlement as pointing the player to a supplier, despite returning
   directly to the survey location. Make the finding itself actionable:
   “Replacement impeller: collect at Cormorant,” and “Replacement cable:
   collect at Benthos.” Explain any station contact as a received message if
   that is the intended fiction. Surveys can happen before FIRST, so copy must
   also work before the player receives a repair request.

6. **The shared beacon goal arrives late.** The code requires PUMP, LIFT and a
   seal, but the story does not establish what the repairs contribute to the
   core. The first request also calls the lift simply “THE FREIGHT LIFT,” which
   can make it sound local to Benthos. Name Cormorant and its Freight Yard.
   Introduce Deep Anchor's isolation early, then explain the joint effort when
   both repairs are complete. Any proposed explanation must allow either
   repair first; Cormorant can already supply the impeller with its lift broken.

7. **HOME is mechanically a full freight load.** All five cargo missions load
   four units, including the dispatch. That changes handling and makes the
   deep-to-home ascent a loaded trip. Meanwhile the station says Tidewall can
   hear Deep Anchor. Explain the beacon as a limited signal and the cargo as
   physical survey records and return supplies, or deliberately change HOME's
   cargo rules. A short message alone does not explain the existing load.

## Six proposed additional nodes

| New node | Location and trigger | Information delivered |
| --- | --- | --- |
| First departure | Tidewall, accepting FIRST | Benthos needs filters for its food tanks; Deep Anchor has gone quiet. Establish home before the return. |
| Impeller handoff | Cormorant, accepting PUMP | Load the replacement inside the hull. Deliver to Benthos; its crew services Pump Refuge. Reuse the loading scene here. |
| Cable handoff | Benthos, accepting LIFT | Benthos supplies replacement cable; Cormorant's crew repairs Freight Yard and reopens Lower Gantry. Reuse the reel scene here. |
| Joint restoration | After the second repair, at whichever settlement receives it | Both communities can now finish the beacon core at Cormorant. If the seal is missing, give its location; otherwise direct collection at Cormorant. |
| Beacon departure | Cormorant, accepting BEACON | Name Deep Anchor as the destination, confirm the sealed hull, and explain the core's limited signaling role. |
| Reply entrusted | Deep Anchor, accepting HOME | Explain why Tidewall needs a physical dispatch and what fills the hold for the return. Give the home destination before departure. |

The joint-restoration node must trigger after either repair completes second;
it must not assume the player is at Cormorant. Departure nodes belong to an
accepted load. Their reload/abandon/reaccept semantics need an explicit design
before implementation. Existing survey and delivery checkpoint behavior must
remain intentional when scenes are split.

Tool discoveries can gain short contextual notices later: the cutter opens old
routes, and the seal makes the deep expedition possible. They should remain
independent discoveries rather than requiring an extra settlement errand.

## Implementation implications and evidence

First fix contradictory geography, branch-aware guidance and empty-board clues.
Then introduce the six triggers, reusing existing loading art and keeping the
new connective moments brief. No additional map room is required by this
proposal. If playtesting still finds the middle too short, consider a playable
beacon commissioning step at Deep Anchor; its action and payoff should be
defined before adding another haul.

The story format currently hard-codes seven alternating image/text decks and
their card offsets in [story_view.hpp](../src/story_view.hpp) and
[gen_story_beats.py](../tools/gen_story_beats.py). More narrative triggers do
not automatically fit that format. The cart is close to 2 MiB; a larger set of
animated decks requires measured regeneration and resource checks. Do not
assume thirteen full decks fit. Existing FX declarations remain append-only.

Fresh verification: `make test` reports **232 passed, 0 failed**. Compilation
also reports an existing missing `subX` initializer warning in
`tst/pilot_view_test.hpp:204`. The tests support campaign reachability in both
repair orders, early survey facts and one-time deliveries; the physical-flight
route test covers FIRST, not a measured complete-campaign playthrough.

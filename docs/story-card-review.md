# Story location and screen-card review — 2026-11-03

> **Implementation status (2026-11-03, `sub-hauler-640`).** The source-text
> pipeline now lives in `story/scenes.json` with `story/art.png` as the canonical artwork; text paginates automatically into
> consecutive cards with no six-row truncation. FIRST names the remote
> Cormorant Freight Yard lift, the survey findings name collection stations
> directly, and the all-scene preview includes departures and both joint
> variants. Sol's four `story-handoffs-v1` bundles are integrated through the
> source image tokens; human artistic review remains **unreviewed**. The
> findings below are retained as the audit that drove the fix.

Reviewed the implemented campaign after the progression-handoff change. This
is a source, retained-art and packed-content review, not an interactive
playthrough. Gameplay and generated artifacts were not changed.

## Verdict

All seven main beats have screen cards wired into the runtime. All five cargo
jobs also have departure cards, and the second repair has a joint-restoration
transition. Mission origins and destination berths are coherent. The remaining
problems are clipped text, inappropriate reused departure images, ambiguous
survey conversations, and missing handoff preview controls.

## Location and card coverage

Counts include image and text screens. An image screen cycles four poses.

| Beat | Start / request | Actual finish | Departure cards | Finish cards | Location assessment |
| --- | --- | --- | ---: | ---: | --- |
| FIRST | Tidewall: filters | Benthos berth | 2 | 6 | Arrival and food tanks fit Benthos. Departure incorrectly reuses impeller art. |
| PUMP SURVEY | Benthos lead | Pump Refuge survey ring | — | 4 | Art and field trigger fit Pump Refuge; supplier conversation wording needs revision. |
| PUMP | Cormorant: impeller | Benthos berth | 2 | 2 | Correct handoff: Benthos's crew services the remote Pump Refuge pump. |
| LIFT SURVEY | Cormorant lead | Freight Yard survey ring | — | 4 | Art and field trigger fit Freight Yard; supplier conversation wording needs revision. |
| LIFT | Benthos: cable | Cormorant berth | 2 | 2 | Correct handoff: Cormorant's crew repairs Freight Yard and reopens Lower Gantry. |
| BEACON | Cormorant: core | Deep Anchor berth | 2 | 2 | Descent art describes the coming trip rather than the pickup berth; label that distinction or use a core-loading view. |
| HOME | Deep Anchor: dispatch and supplies | Tidewall berth | 2 | 2 | Result fits Tidewall. Departure reuses Tidewall approach art while the player is still at Deep Anchor. |

After the second repair, either receiving settlement shows a further image/text
pair. There are separate seal-missing and seal-owned versions. Both use the
original repair-bay picture. This is functional coverage, but a restored or
neutral planning view would communicate the completed joint effort better.

The generated map contains **36 card entries, 78 stored frames, 15 unique
animated images and 18 text cards**. There are 14 scene sequences: seven main
beats, five departures and two alternative joint-restoration sequences. The
network, thanks and statistics ending screens are additional ending screens.
Tool pickups and service-berth activation have notices/world changes rather
than dedicated full-screen story decks.

## Findings, ordered by impact

### P1 — Five handoff text cards lose essential information

[gen_story_beats.py](../tools/gen_story_beats.py) wraps at 20 columns, then
`text_frame()` silently takes only six rows. These are current packed-content
defects, not merely warnings in the preview:

| Card | Wrapped rows | Omitted text |
| --- | ---: | --- |
| FIRST departure | 7 | BENTHOS. |
| LIFT pickup | 8 | REOPENS LOWER / GANTRY. |
| Joint restoration, seal missing | 7 | WEST WORKSHOP. |
| BEACON departure | 10 | LIMITED / THE NETWORK STILL / NEEDS / A PHYSICAL DISPATCH. |
| HOME departure | 8 | ANCHOR, DELIVER TO / TIDEWALL. |

This removes destinations, the seal location and the intended explanation for
carrying a physical dispatch. Tracked by **sub-hauler-640**. Shorten or split
the copy, and reject overflow during generation. All seven main result/survey
decks currently fit; the failures are in the added handoffs.

### P2 — FIRST shows the wrong cargo

`handoffs[first]` selects `delivery-loading`, the same native picture used for
the replacement impeller at Cormorant. The prominent impeller makes this a
specific part-loading scene. At Tidewall, FIRST carries filters for Benthos.
Use filter-loading art or a cargo-neutral berth image. Reusing the animation
does not make its cargo appropriate to both jobs.

### P2 — HOME departure shows the destination before leaving its source

`handoffs[home]` selects `home-return`. This picture was authored as the approach
to Tidewall, but the trigger is accepting HOME at Deep Anchor, while still
docked. The final `home-lights` result correctly belongs at Tidewall. Give the
departure a Deep Anchor dispatch/loading view, or explicitly frame it as the
journey ahead. The beacon descent at Cormorant similarly benefits from an
explicit journey-ahead label rather than implying the image is the current
location.

### P2 — Survey cards still imply conversations that did not occur

The pump finding says “BENTHOS POINTS YOU TO CORMORANT,” immediately at Pump
Refuge. The lift finding similarly says “CORMORANT POINTS YOU TO BENTHOS” while
at Freight Yard. Both can trigger before FIRST and without visiting the clue
station. These are sensible suppliers, but the conversation is not established.
Prefer direct findings such as “REPLACEMENT IMPELLER: COLLECT AT CORMORANT”
and “REPLACEMENT CABLE: COLLECT AT BENTHOS,” or explicitly establish a received
message. The claim in [story-beats.md](story-beats.md) that these findings have
already been rewritten is inconsistent with the source copy.

FIRST also requests “THE PUMP AND FREIGHT LIFT” without naming Cormorant. Name
the remote lift so the repair-bay picture does not suggest both faults are
inside Benthos. Use wording that remains appropriate after early surveys.

### P2 — Added handoffs have no selectable sketchbook preview

[story-sketchbook.html](story-sketchbook.html) defines `handoffs`, and the
shipping generator reads them. However, the page instantiates controllers and
navigation for only the seven main decks. No controller or gallery consumes
the handoff array. Thus these cards exist in the game but cannot be selected
for review through the current preview UI. Add native/enlarged playable
previews for every departure and both joint variants.

## Evidence and limits

- Traced mission routes in [campaign.hpp](../src/campaign.hpp), remote survey
  and service objects in [exploration.json](../levels/exploration.json), and
  destination-only delivery in [jobs.hpp](../src/jobs.hpp).
- Traced acceptance, survey, dock, joint-restoration and HOME/ending playback
  in [app.hpp](../src/app.hpp) and scene selection in
  [story_view.hpp](../src/story_view.hpp).
- Read the generated card/frame map and checked every deck's text layout with
  the production parser. Inspected retained native frames for impeller
  loading, HOME approach, repair bay and beacon descent.
- Repacked the source in memory using the production packer: it matched
  `fxdata/tables/story-beats.bin` byte for byte (**49,168 B**). No generated
  file was written. This establishes that the observed copy/art assignments
  and truncation are present in the retained shipping story blob.
- No full gate was rerun for this review. Prior passing CRC/playback tests
  demonstrate faithful rendering, not semantic consistency or complete text.

The next correction pass should address the findings above before treating
**sub-hauler-3ca** as accepted. No additional main objective is required to
provide screen-card coverage.

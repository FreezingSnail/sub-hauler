# Story source, cards and handoffs — 2026-11-03

The shipping story has two canonical editing sources:

- [`story/scenes.json`](../story/scenes.json): scene order, titles, paragraph
  text, image references and sprite-sheet row names.
- [`story/art.png`](../story/art.png): one opaque black/white artwork sheet,
  **512×1216 pixels**, with nineteen rows of four 128×64 animation poses.

The [authoring instructions](../story/README.md) describe the JSON format and
Piskel import/export settings. Pixelgen bundles remain historical provenance;
the generator reads the editable sheet directly, not those original frames.

## Editing and preview

1. Edit text in `story/scenes.json` or artwork in `story/art.png` using Piskel.
2. Run `make gen`.
3. Open [the story sketchbook](story-sketchbook.html) for every sequence
   and paginated text, or [the complete card sheet](assets/story-cards.png)
   for every card in one image. [Its row map](assets/story-cards.json) identifies
   the scene and card on each row.

The same sketchbook ends with a read-only dock-bark preview: every berth's
station-board line at each saved progression checkpoint, drawn at the native
6×8 bark row. `make gen` writes `docs/assets/dock-barks.js` from the
generator's `campaign::DockBark`-validated mapping and the shipped FX strings;
the same run writes the cold cart address table. The page never writes a
canonical source.

The generator never overwrites the two canonical editing sources. Prose is
stored as paragraph strings. It wraps on word boundaries at twenty columns,
uppercases for the native 5×7 font, and emits as many six-row text pages as
needed. Consecutive text pages are allowed. Supported input is A-Z, a-z
(rendered uppercase), 0-9, space, period, comma, colon, slash, hyphen and
apostrophe. Invalid JSON, unknown image names, duplicate row names, unsupported
characters, empty paragraphs and over-width words fail with an error.

Cards follow JSON order; image/text alternation is not required. Image names
select rows from `art.rows` in the JSON. Four poses run left to right, each at
300 ms. Canonical PNGs may use 1-bit, palette, RGB or RGBA mode, including
Piskel exports, but must have the exact sheet dimensions and contain only
fully opaque black/white pixels. Color, grayscale and transparency are errors.

The complete card sheet is generated for inspection, not an authoring input.
It has one row per card in scene order. Image cards occupy four columns;
text pages occupy column zero, with the remaining columns black. Preview poses
are embedded from the canonical sheet, so the browser shows the same pixels
as the packed game. Edit JSON for text changes and `story/art.png` for art.

## Scene and card shape

Existing scene IDs remain stable; the DEEP RELAY briefing appends as
ID 16 after the expedition result at ID 15, and the New Voyage opening appends
as ID 17. The JSON scene array and complete card overview follow narrative
order: opening, Tidewall departure, Benthos arrival, pump branch, lift branch,
joint restoration, beacon and homecoming. Packed storage order is independent
of the editing order. The two repair branches remain playable in either order.
The scene inventory is:

| Scene | Purpose |
| --- | --- |
| Opening | New Voyage instruction to dock at Benthos; reuses the arrival art |
| FIRST result | Benthos arrival, grow tanks, repair request naming the remote Cormorant Freight Yard lift |
| PUMP SURVEY | Pump Refuge inspection; direct replacement-impeller collection at Cormorant |
| PUMP result | Water and grow-room lights return |
| LIFT SURVEY | Freight Yard inspection; direct replacement-cable collection at Benthos |
| LIFT result | Freight lift and workshops return; Lower Gantry opens |
| BEACON result | Deep Anchor beacon and capsule lights |
| HOME result | Tidewall welcome and restored network |
| FIRST departure | Filters, Benthos, quiet Deep Anchor and the remote lift request |
| PUMP pickup | Impeller loaded internally at Cormorant, delivered to the Pump Refuge pump |
| LIFT pickup | Cable loaded at Benthos, delivered to the Freight Yard lift |
| Joint restoration, seal missing | Both repairs done; core at Cormorant; seal at West Workshop |
| Joint restoration, seal held | Both repairs and seal ready; collect the core at Cormorant |
| BEACON departure | Dive ahead to Deep Anchor; sealed hull, limited signal and physical dispatch |
| DEEP RELAY departure | Beacon-call briefing: lens locates the buried relay, cutter clears rock, seal holds pressure, thruster beats current |
| DEEP RELAY result | Relay found at the lower-right work site; keeps the wreck illustration |
| HOME departure | Physical survey records and return supplies loaded at Deep Anchor for Tidewall; unique loaded-sub art |
| HOME result | Tidewall welcome and restored network |

The current source produces **43 cards** (21 image, 22 text) and **98 frames**
from **19 shared images**. The opening scene reuses `arrival`; the DEEP RELAY
acceptance and HOME departure append the two new rows. DEEP RELAY departure and
result no longer share an image, and HOME departure no longer reuses the
PUMP-pickup cargo art. Text-only overflow creates adjacent pages rather than
truncation. Shared images are still packed once. The `story-handoffs-v1`
scenes replace the old departure placeholders; `beacon-descent` and
`home-return` are omitted from the active blob, while `delivery-loading` and
`repair-bay` remain for PUMP pickup and FIRST result.

## Runtime and save contract

- A or B advances one card. Image cards animate four poses at 3.3 fps; text
  cards stay still. Consecutive text pages are ordinary cards.
- New Voyage plays the opening deck after the initial checkpoint and before
  pilot control. It is RAM-only and skipped by Continue: a cold reload resumes
  the committed Tidewall berth and does not replay it.
- Acceptance writes the accepted load, then the departure scene id in the
  one-byte `pendingStory` field, before playback. Continue resumes the pending
  scene; finishing it clears the byte in RAM only. A reload before the next
  natural checkpoint can replay it: **at-least-once**, not exactly-once.
- Mandatory story jobs cannot be voluntarily abandoned. Continue resumes the
  accepted load's departure scene before travel.
- Delivery/repair decks are selected from a new milestone bit after docking and
  the post-delivery checkpoint is written before the deck. Survey decks are
  unsaved field finds.
- The second repair hands off to the joint-restoration scene at either
  receiving settlement. Save ownership selects the seal-missing or seal-ready
  variant.
- HOME result cards are the ending prologue. The ending flag commit still
  controls replay after the cards.
- The ending presentation is two fixed cards, lit network then thanks. The
  retired voyage-stats panel remains declared in the FX layout so later flash
  offsets stay stable, but no screen draws it.

## Generated outputs

`make gen` writes:

- `fxdata/tables/story-beats.bin` — packed RLE card frames and offset table;
- `src/generated/story_beats.hpp` — scene/card counts, scene ids, card kinds,
  first-frame map and compatibility aliases;
- `tst/fxdatatest/story_frame_crc.hpp` — device frame CRC reference;
- `docs/assets/first-beat-animation/frames.js` — embedded `StoryAnimation` image poses
  plus `StoryData` scene/card/row metadata for the all-scene preview;
- `docs/assets/story-cards.png` and `story-cards.json` — the complete card sheet
  and its scene/card row map.

`fxdata/fxdata.txt` appends `storyBeats` after `txt_survey_chart_hint` and before
`savesection 4096`. Every earlier FX offset, `storyBeats = 0x1F0F00` and
`FX_SAVE_PAGE = 0xFFF0` are unchanged.

## Current verification — 2026-11-03

- Host **235** tests, tooling **144** tests and browser **8** preview tests pass.
  Coverage includes source parsing, full-prose preservation, automatic
  pagination, consecutive text pages, blank/unsupported glyph errors, card-kind
  derivation, image dedupe, preview/packed parity, scene/handoff selection,
  branch guidance and save/reload/abandon behavior.
- **15/15** default device suites pass. `test_story` decodes all **90** frame
  CRCs, exercises departure playback, save-failure continuation, cold reload and
  both joint variants; render/display/service maximum is **10,756 µs**.
- Save-chain reserve **665 B**, combined music reserve **659 B**; runtime
  save-page probe passes. The 842-frame flight probe measures **11,604 µs**
  maximum with zero misses and zero audio errors/underruns.
- FX and Mini firmware are **28,432 B flash / 1,650 B static**. The cart payload
  is **2,092,772 B** and the padded image is **2,096,896 B**, leaving **256 B**
  below 2 MiB. `storyBeats = 0x1F0F00` and `FX_SAVE_PAGE = 0xFFF0` are
  unchanged; `make gen-check` is deterministic.

## Canonical editing format verification — 2026-11-05

The JSON/sprite-sheet migration preserves the packed story blob, generated
device header, frame CRC reference and complete FX image byte for byte.
The authoring sheet is **512×1088**; the generated complete card sheet is
**512×2624**, containing forty-one card rows. Both contain opaque native 1-bit
pixels. Piskel RGB, RGBA and palette exports are accepted when every pixel is
opaque black or white. Generation never writes the canonical artwork sheet.

Fresh checks: **237 host tests**, **147 tooling tests**, documentation links
and selectors, **8 browser playback tests**, and deterministic `make gen-check`
all pass. Tooling checks exercise pixel edits reaching the packed pose,
JSON/schema errors, invalid color/transparency/size rejection, exported-sheet
equivalence, and all generated card-sheet/preview pixels matching the packed
frames. Logs are `build/story-canonical-*.log`.

The device gate and size figures above are the prior combined-art checkpoint;
they were not rerun for this authoring-only migration. The unchanged FX image
retains its **256 B** cart headroom. No physical-device playthrough, Piskel GUI
round trip, GUI screenshot or speaker listening is claimed.

## User artwork update — 2026-11-06

The edited canonical sheet now builds after normalizing its #010101 palette
entry to #000000. Pixel indices and other colors were preserved. The story
blob is **57,363 B**; the FX payload/image is **2,092,843 / 2,097,152 B**. The
padded cart now fills 2 MiB, with 213 B of data alignment padding remaining.
`make gen-check` is deterministic and all **21** story tooling tests pass.

## DEEP RELAY departure beat — 2026-11-07

Added a briefing shown when the mandatory DEEP RELAY expedition is accepted at
Deep Anchor.
It links the restored beacon's call to the buried relay and explains each
required upgrade. The existing result remains scene ID 15; the new briefing
uses scene ID 16. The canonical sheet's former shared dispatch row now depicts
the relay; HOME departure uses the cargo-loading image. The generated card
sheet has 41 rows and the story has 89 frames. `make gen` passes with a 57,190 B
story blob and **2,092,670 / 2,097,152 B** FX payload/image, leaving 183 B
before the save section. No test suites were run for this story change.

## Story art refresh — 2026-11-09

`story/art.png` was updated and regenerated through `make gen`. The active set
remains 16 scenes, 41 cards and 89 frames; the story blob is **57,023 B** and
the FX payload/image is **2,092,503 / 2,096,640 B**, leaving 512 B below 2 MiB.
`test_story` passes all 89 frame CRCs with a **10,756 µs** maximum; the full
host, tooling and default device gates pass. No human art review is claimed.

## Archival ADPCM removal — 2026-11-10

`sub-hauler-646.18` removed the six unused archival ADPCM streams from the
shipping `fxdata.txt` layout (1,424,640 B). The story art, card and frame map is
unchanged; `storyBeats` moves down to **0x095200** and every symbol through
`txt_a_finish` keeps its offset. The current FX payload/image is
**667,863 / 672,000 B**, leaving **1,425,152 B** below 2 MiB, and
`FX_SAVE_PAGE = 0xFFF0` is unchanged. The `.bin` files and `make gen-audio`
remain archival evidence only.

## Opening, DEEP RELAY acceptance and HOME departure art — 2026-11-10

`sub-hauler-646.6`, `sub-hauler-646.12`, `sub-hauler-646.13` and
`sub-hauler-646.15` append the New Voyage opening deck and two story rows.

- The opening scene is stable scene ID 17 and plays after the initial New
  Voyage checkpoint and before pilot control. A/B advances its arrival image
  and one Benthos instruction card, then returns to free flight. Continue
  restores the committed Tidewall berth and never replays it; a reload during
  the deck skips it, so the opening is at-most-once and never persisted.
- DEEP RELAY acceptance (ID 16) now uses the appended `relay-briefing` row
  (beacon console call), while the site result (ID 15) keeps `deep-relay`.
- HOME departure (ID 14) now uses the appended `home-departure` row (loaded sub
  leaving Deep Anchor); `delivery-loading` remains PUMP pickup only.

The canonical sheet is **512×1216**, nineteen rows and 76 poses. The source
produces **17 scenes, 43 cards** (21 image, 22 text) and **98 frames** from
**19 shared images**. The packed story blob is **61,365 B**; the FX payload is
**672,205 B** and the padded image is **676,352 B**, leaving **1,420,800 B**
below 2 MiB. `storyBeats = 0x095200` and `FX_SAVE_PAGE = 0xFFF0` are unchanged.

Fresh checks: `make gen-check` deterministic; host **264/264**, tooling
**157/157**, docs links/selectors and **8** browser preview tests pass.
`test_story` decodes all **98** frame CRCs and passes opening playback,
Continue skip and cold reload with a **10,752 µs** maximum. FX firmware is
**29,652 / 29,696 B flash** and **1,665 / 2,560 B static**. Both new rows were
generated with pixelgen, `check`/`replay` matched, and the two rows plus the
opening deck have native-size previews under `build/worker-story/`. Human art
review and a physical playthrough are not claimed.

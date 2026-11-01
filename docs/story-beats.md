# Story beats — 2026-10-30

The seven campaign beats now use the finalized sketchbook decks in
[`docs/story-sketchbook.html`](story-sketchbook.html) and
`docs/assets/first-beat-animation/all-beats-v3`. This supersedes the earlier
four static restoration cards. Those images stay appended in the cart as
historical flash ABI, but the game no longer draws them.

## Deck shape

Every beat alternates an image card with a static text card, starting with an
image. FIRST has three image/text pairs; the other six beats have two.

| Beat | Cards | Images | Trigger |
| --- | ---: | --- | --- |
| FIRST | 6 | arrival, grow-tanks, repair-bay | FIRST delivery checkpoint |
| PUMP SURVEY | 4 | pump-approach, pump-cracked-impeller | pump survey dwell |
| PUMP | 4 | delivery-loading, delivery-water | PUMP delivery checkpoint |
| LIFT SURVEY | 4 | lift-yard, lift-cable | lift survey dwell |
| LIFT | 4 | lift-delivery-cable, lift-delivery-working | LIFT delivery checkpoint |
| BEACON | 4 | beacon-descent, beacon-lit | BEACON delivery checkpoint |
| HOME | 4 | home-return, home-lights | HOME delivery, as the ending prologue |

That is 30 cards: 15 images ×4 poses = 60 image frames plus 15 static text
cards, 75 frames total. Image cards advance one pose every 18 device frames
(300 ms, 3.3 fps); text cards stay still.

## Cart format and generation

`tools/gen_story_beats.py` reads the sketchbook and native poses directly.
Text cards use the preview's 5×7 glyphs, 20-column wrap, six-row limit, 10 px
leading and centring, so they are pixel-identical to the approved preview.
Every page-major 1,024 B frame is run-length encoded. The generated blob is
46,673 B:

- one little-endian `uint16` frame offset per frame (150 B);
- concatenated RLE frames;
- 32 B zero padding so the device's 16 B scratch reader can over-read the last
  chunk safely.

Card order always alternates image/text from the first image, so the device
derives a card's first frame from its global index; no per-card metadata is
stored. `fxdata/fxdata.txt` appends `storyBeats` after
`txt_survey_chart_hint` and before `savesection 4096`. Every earlier FX offset
and `FX_SAVE_PAGE = 0xFFF0` are unchanged. `make gen` emits both
`fxdata/tables/story-beats.bin` and `src/generated/story_beats.hpp`;
`make gen-check` compares them byte-for-byte.

## Trigger and replay contract

- Delivery/repair decks are selected from a new campaign milestone bit after
  `station::stepFlight` docks. The complete post-delivery checkpoint is written
  **before** the deck. A failed write keeps the SAVE FAILED screen; RETRY or
  CONTINUE UNSAVED resumes the pending deck.
- Survey decks are selected from a newly recorded survey bit at the field
  dwell. They play immediately, then return to the frozen pilot frame. The
  survey remains an unsaved find like other field discoveries.
- A fresh A or B edge advances one card. A held chord does not.
- No story cursor is serialized. Completed milestone decks do not replay after
  reload, station return or rescue. Survey decks can replay only if a rollback
  removed the survey fact.
- HOME is the first four cards of the ending. `beginEnding()` shows them while
  `state.ending == 0`, so a reload between the final delivery and the ending
  resumes them without paying again. After the four cards, the existing
  network, thanks and stats cards follow. Once `markEndingPresented()` commits
  the ending flag, Tidewall `VIEW ENDING` replays only the three ending cards.
- No new save field, EEPROM byte, mission, tool or input chord was added. The
  existing ending-card ordering, clue text and music cues are unchanged.

## Resource and verification record (2026-10-30 story-deck checkpoint)

Dated numbers from the story-deck change. Current fixed-build totals after the
2026-11-01 economy removal are **27,230 B flash / 1,646 B static**, 215 host
tests, 128 tooling tests, save-chain **702 B** and combined music **660 B**; see
[platform.md](platform.md).

| Item | Value |
| --- | ---: |
| FX and Mini flash | **28,848 B** (`.text 28,836 + .data 12`) |
| Static RAM | **1,660 B** (`.data 12 + .bss 1,648`) |
| Flash headroom | **848 B** under 29,696 B |
| Static headroom | **40 B** under 1,700 B |
| Cart payload | **2,082,151 B** |
| Cart image | **2,086,400 B** (10,752 B under 2 MiB) |
| Story blob | **46,673 B** |
| Save-chain stack | **696 B** |
| Combined music/memory stack | **626 B** |

Final gates: `make gen-check` deterministic; `make test` **234 passed / 0
failed**; `make test-tools` **122 tests OK**; `make fxtest` **14/14** default
suites `P` (`test_memory_music` `stack_reserve=626`); `make
fxtest-save-chain` `stack_reserve=696`, `save_chain=1`,
`audio_after_save=1`, zero underruns/errors; `make
fxtest-save-runtime-page` `P`; `make fxtest-flight` **842** frames,
**11,768 µs** maximum, zero 16 ms misses, gap 1, zero underruns/errors;
`make size` **28,848 / 1,660 B**; `make mini` **28,848 / 1,660 B**.

No physical-device playthrough, GUI screenshot or speaker listening is
claimed. The evidence is the host/tool/emulator gates above.

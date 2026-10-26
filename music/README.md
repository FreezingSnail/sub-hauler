# Sub-hauler music

Six original underwater loops, authored with the sibling
ATMlib2FX [atm-music skill](../../ATMlib2FX/docs/skills/atm-music/SKILL.md).
Round pulse voices, quiet replies, open bass and tiny noise bubbles keep the
water theme across slow drifts, a waltz and a quicker current-riding cue.

| Cue | Use | Tempo | Loop | FX bytes |
| --- | --- | ---: | ---: | ---: |
| [Harbor Bubbles](harbor_bubbles/song.json) | Warm harbor / station | 62.50 BPM | 61.44 s | 3,586 |
| [Kelp Drift](kelp_drift/song.json) | Unhurried travel, D Dorian | 55.56 BPM | 69.12 s | 3,442 |
| [Sonar Lullaby](sonar_lullaby/song.json) | Deep water, sparse sonar pairs | 50.00 BPM | 76.80 s | 2,794 |
| [Abyssal Glass](abyssal_glass/song.json) | Suspended deep-water drift | 40.00 BPM | 72.00 s | 2,504 |
| [Tidepool Waltz](tidepool_waltz/song.json) | Buoyant tidepool interlude | 75.00 BPM | 38.40 s | 3,254 |
| [Bubblejet Run](bubblejet_run/song.json) | Lively current-riding travel | 100.00 BPM | 38.40 s | 5,948 |

The first three cues are 16 bars of 4/4 at an exact 50 Hz music clock.
The second eight bars develop the opening phrase. The final harmony leads
into the first bar; there is no end fade or long terminal rest. Melodies and
arrangements are new.

The newer variations explore all six native ATM effect types:

- **Abyssal Glass:** 12 bars of 4/4 in F# minor. Long glassy calls have shallow
  vibrato; the quiet water voice breathes with a slow tremolo LFO. Three
  four-bar phrases move from a question to a lift and return.
- **Tidepool Waltz:** 16 bars of 3/4 in G major. Root bass on beat one supports
  slow triad arpeggios on the answering voice. A duty-modulation LFO moves
  the pearl lead's timbre while volume slides keep its notes rounded.
- **Bubblejet Run:** 16 bars of 4/4 in A Dorian, at a 100 Hz music clock.
  One-shot note cuts make the hook bounce, with a repeated cut on occasional
  replies. Short downward glissandos approach chord tones, volume slides
  soften the echoes, and tiny noise-retrigger rolls color the wake.

Every effect window closes within its pattern, including the final phrase.

## Audition in the library viewer

```sh
make music-viewer
```

This uses ATMlib2FX's existing FX jukebox sketch in Ardens with all six
songs under **Sub-hauler**. Up/down selects a song, A restarts, B pauses/resumes.
The generated sketch/index lives in `build/music-viewer/`; it reads the same
`fxdata/fxdata.bin` as the game. `make music-viewer-build` compiles without
opening Ardens. No WAV files are produced.

[`catalog.json`](catalog.json) also supplies the library catalog format, with
paths relative to this repository root, for a viewer that accepts catalogs.
Individual `song.json` files are editable `atmlib-score` v1 sources.

## Regenerate and use

```sh
make gen
make gen-check
```

The sibling `../ATMlib2FX` checkout is required. Override its location with
`ATMLIB2FX_ROOT=/path/to/ATMlib2FX make gen` (also works for the viewer).
The exporter uses `--ring 64 --streaming-loops`. `make gen` writes compact
streams and a source/event/stream hash manifest into `fxdata/music/`, appends
the streams to the ordered FX layout, and stages the jukebox. Detailed
`report.json` / `review.json` files are in `build/music/<score-id>/`.

All streams use `0xC1` streaming loop framing. A future game player
must build ATMlib2FX with `ATM_FX_COMPACT_RING=64` and
`ATM_FX_COMPACT_LOOP=1`, use the generated `sh_*` FX offsets and
`music_data::sh_*_bytes` lengths from `src/generated/music.hpp`, and service
the ring in the foreground between complete SPI transactions. The normal
game has no playback calls yet. Asset bytes are external cart storage;
they do not measure the cost of a future player in AVR flash, SRAM or stack.

Validation/export and host decoder loop checks establish timing and stream
shape. They do not establish physical piezo sound or listening quality.

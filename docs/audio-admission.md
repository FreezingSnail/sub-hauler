# Sampled audio admission (sub-hauler-7px.11.2)

Status, 2026-10-27: **admitted**. The paired whole-image player allocation
passes, all six loops decode, the permanent cold-cache world path meets the
16 ms service-gap target through a bounded foreground service point, and the
noUSB actual-game stack probe reports 815 B reserve. No cap was raised.

Historical note (2026-10-28): the sampled player was not selected as the
shipping backend. Production `full` now uses the ATMlib2FX compact synth with
`ATM_FX_COMPACT_RING=32`; the ADPCM assets, player measurements and admission
gates below remain as comparison evidence. See
[platform.md](platform.md) for the current backend and whole-image result.

## Assets

`tools/gen_audio_adpcm.py` encodes the six deterministic 8 kHz s16le
references from `build/audio-reference/` as blockless raw 4-bit IMA/DVI ADPCM:

- low nibble is the earlier sample;
- initial predictor = first reference sample (currently 33 for every cue),
  initial step index = 0;
- loops reset predictor/index to that exact state at the authored sample
  boundary, with an even nibble count and no inserted silence;
- no per-cue gain, normalization or DC removal;
- fixed integer step/index tables are `PROGMEM`, not initialised `.data`.

| Cue | Samples | Encoded bytes | SNR | RMSE | Max abs error |
| --- | ---: | ---: | ---: | ---: | ---: |
| Harbor Bubbles | 491,520 | 245,760 | 15.612 dB | 423.89 | 7,844 |
| Kelp Drift | 552,960 | 276,480 | 15.133 dB | 432.23 | 6,540 |
| Sonar Lullaby | 614,400 | 307,200 | 14.004 dB | 388.57 | 7,323 |
| Abyssal Glass | 576,000 | 288,000 | 20.221 dB | 306.83 | 6,496 |
| Tidepool Waltz | 307,200 | 153,600 | 18.824 dB | 292.94 | 4,714 |
| Bubblejet Run | 307,200 | 153,600 | 16.885 dB | 339.06 | 5,644 |
| **Total** | **2,849,280** | **1,424,640** | | | |

The metrics compare decoded PCM with the calibrated reference PCM. These are
waveform/error measurements; they are not a physical speaker or listening
claim. Decoded listening files are written to `build/audio-adpcm/<id>.wav` and
`.pcm`.

The six streams append to `fxdata/fxdata.txt` after every shipped symbol. The
cart payload becomes **1,876,899 B** and the padded flashable image
**1,876,992 B**, below the 2 MiB admission cap (2,097,152 B). All earlier FX
offsets are unchanged.

## Player contract

`src/audio/sampled_player.hpp` is the pure core and
`src/audio/sampled_player_avr.hpp` is the device boundary. AVR static state is
exactly **152 B**:

- 128 B ring storage, of which **127 B are usable**; one sentinel slot makes
  empty/full unambiguous without a shared full flag. At 8 kHz that is
  **31.75 ms** of compressed audio, above the 16 ms service target.
- 24 B: predictor/loop predictor, step/loop index, nibble cursor, byte cursor,
  sample/loop counts, FX cursor/bytes/base, flags/cue id.

Semantics:

- `start(cue)` resets and pre-fills before enabling the sample interrupt. A
  start for the already-active cue does not restart playback; an active-paused
  cue is left paused.
- `stop()` clears playback/position and silences output; mute is preserved.
- `pause()` freezes position, predictor, index and ring; `resume()` continues
  from the same sample. Paused time is excluded from the active refill-gap
  diagnostic.
- `setMuted(true)` silences output and freezes decoding at the current sample;
  `setMuted(false)` resumes there. Mute persists across cue changes.
- `service()` performs FX/SPI refills only. It is called by the foreground
  before the frame gate, after presentation, and from the bounded
  `world::FxTables::tileByte()` service point (enabled by `SH_AUDIO_PLAYER`)
  before collision cart reads. It is never called during the OLED transaction.
  The ISR touches SRAM only: no FX, SPI, OLED or descriptor read.

If the ring ever empties while active, the ISR returns silence and raises the
sticky error/underrun bits. Admission requires both counters to stay zero.

## Clocks and output

- **Sample clock:** Timer3 CTC, `16 MHz / 8 / (249 + 1) = 8,000 Hz`. The
  `TIMER3_COMPA` ISR decodes one nibble and writes the PWM duty.
- **Carrier:** Timer4 fast PWM, 96 MHz PLL / 1 / (511 + 1) = **187,500 Hz**.
  The carrier is above the audible band, including while paused/muted/stopped;
  the earlier 8 kHz carrier experiment is not used.
- **Mapping:** documented 9-bit equivalent of the accepted 8-bit calibration:
  `duty = 256 + (signed16 >> 7)`, range 0..511, centre 256. This removes the
  former 32-bit `sample * 375` multiply and maps to roughly
  `sample * 2000 / 32768` in the emulator. The emulator's PWM averaging is not
  used as evidence of silence or quality.

## Measurements

All builds use arduino-cli 1.2.0, `arduboy-homemade:avr 1.4.0`, AVR-GCC
7.3.0-atmel3.6.1-arduino7, `--optimize-for-debug`, `-mrelax
-mcall-prologues`, LTO, and the same full feature flags. Both paired images use
`ARDUBOY_NO_USB` immediately before `setup()`.

| Image | Flash | Static RAM |
| --- | ---: | ---: |
| Full game, noUSB, no player (baseline) | 17,412 B | 1,443 B |
| Same + real player, all API calls linked | 19,006 B | 1,595 B |
| **Marginal player** | **1,594 B** | **152 B** |
| Admission cap | 1,600 B | 152 B |
| Headroom | 6 B | 0 B |

The candidate calls `begin`, `start`, `pause`, `resume`, `setMuted`, `stop`,
`start` and `service` in a reachable path; `avr-nm` shows `audio::start`,
`audio::fillRing`, `audio::service`, `audio::g_player`, the codec tables and
`__vector_32` in the candidate, and shows no `atm_*`, `osc_*` or synth symbols.
The simultaneous shipping full image (player not integrated) remains
**20,074 B / 1,583 B**; the staged admission candidate is
**19,006 B / 1,595 B**, still under 29,000/1,700 and 29,696/2,560. This is an
interim result; the final combined story/presentation image is task .13.

Diagnostics are compiled out of the measured candidate. With
`SH_AUDIO_DIAG=1` they add **544 B flash / 14 B static** (measured
19,550 B / 1,609 B); this overhead is not part of the 1,594/152 allocation.

### Device/emulator timing

`tst/fxdatatest/test_audio.ino` runs the real Timer3 ISR, Timer4 carrier, FX
refills, world physics/render/cart/OLED frames, idle menu refills, a modeled
120 ms paused EEPROM wait, mute, short FX-backed loop repeats and all six cues:

```
audio gap=114 svc_us=168 isr_ticks=27 underruns=0 stages=0/0/0/0 errors=0
P
```

`gap=114` is 14.25 ms (8 kHz ticks), `svc_us=168` is the maximum foreground
refill, and `isr_ticks=27` is 13.5 µs at 0.5 µs Timer3 ticks. The suite clears
the collision/terrain cache first and runs the first world frame cold; no warm
frame is performed. Cold and later cached frames include terrain cart reads and
one OLED presentation; idle/static menu frames refill without rendering. The
paused blocking wait contributes zero underruns and is excluded from the active
gap clock. The bounded `tileByte()` service point keeps the cold collision
reads below the deadline.

## Stack reserve

The reserve is measured from a build-only **noUSB** full-game image that links
the real player, runs the cold/cached world path, presentation, pause/resume,
mute and Timer3 ISR, then paints and scans the stack. A permanent native
Ardens-absim runner (`tools/audio_stack/runner.cpp`) loads the HEX plus the
actual `fxdata/fxdata.bin` and reads the probe's SRAM diagnostic:

```
stack_diag: magic=5354414b reserve=815 high=2684 bss=1869 sp=2812 frames=9
```

`reserve=815 B` against the 600 B target is **215 B of margin**. The probe adds
only the 20 B SRAM diagnostic and its setup locals; it is not the USB-heavy
device-test ELF and does not infer reserve from static free RAM. The measured
window includes foreground game work, audio refills and Timer3 interrupt
frames. The probe ELF is `build/audio-admission/stack/dist/stack.ino.elf`.

## Reproduction

```sh
ATMLIB2FX_ROOT=/Users/connorfranc/code/arduboy/ATMlib2FX make gen
make gen-audio
make gen-check
make test
make test-tools
make fxtest
make size-matrix
```

Paired admission builds and logs live under `build/audio-admission/`; decoded
audio is under `build/audio-adpcm/`. No commit, stage or push was performed.

Post-admission append: `sub-hauler-7px.12` adds the title loader strings after
the six sampled streams. The streams, descriptors, player state and FX offsets
are unchanged; the current payload/image are **1,876,927 / 1,876,992 B**, still
under the 2 MiB cap. The loader's firmware cost is tracked separately in
[`archive/story-resource-design.md`](archive/story-resource-design.md); it does
not change the player allocation.

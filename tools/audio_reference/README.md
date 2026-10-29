# Offline reference audio

`tools/gen_audio_ref.py` is archival comparison evidence, not a firmware build
input. Run it explicitly with `make gen-audio`. It produces deterministic mono
8 kHz s16le references for the six authored scores without a GUI or audio
device.

## Pipeline

1. Export each score with the pinned ATMlib2FX `atm-music` tool (`gen_music.py`).
2. Build a bare-metal ATmega32u4 image with the real ATMlib2FX oscillator,
   compact decoder and Timer4 PWM path. The stream is embedded in PROGMEM.
3. Run that image in the pinned Ardens `absim` core through
   `tools/audio_reference/capture.cpp`, advancing exactly 1 ms at a time
   (`1_000_000_000ull` ps). absim's `SOUND_CYCLES = 320` at 16 MHz gives
   50,000 samples/s.
4. Discard one full authored loop (the intro cycle), capture the next loop
   cycle, and retain 50 ms of guard on both sides.
5. Apply a fixed 192-tap Blackman-windowed sinc polyphase low-pass
   (3.4 kHz, quarter-sample phases), then a fixed global calibration:
   `ref = clamp(round(pwm * 32767 / 2000), -32768, 32767)`. There is no per-cue
   normalization and no DC removal.
6. Write `build/audio-reference/<id>.pcm` and a listening `.wav`, plus raw
   resampled emulator units in `build/audio-reference/raw/<id>.pcm`. Write the
   compact provenance manifest to `fxdata/music/reference-manifest.json`.

The cart payload is untouched: the manifest is not in `fxdata/fxdata.txt`.
Generated PCM/WAV/50 kHz intermediates stay under the ignored `build/` tree.

## Calibration

- `emulator_pwm_full_scale = 2000` (absim `SOUND_GAIN`).
- Canonical 8 kHz s16: `ref = clamp(round(pwm * 32767 / 2000), -32768, 32767)`.
- Inverse for provenance: `pwm = round(ref * 2000 / 32767)`.
- 8-bit playback mapping: `pwm8 = clamp(128 + (ref >> 8), 0, 255)`.
- **Do not** feed raw PWM units directly to `sample >> 8`; that nearly silences
  playback. Use the calibrated reference or apply the manifest gain first.
- Clipping and measured DC are recorded per cue; current sources clip zero
  samples.

## Verification

Use `AUDIO_REFERENCE_FORCE=1 make gen-audio` to rebuild the capture tool and
render all six cues afresh. Compare output hashes between two such runs when
validating renderer determinism; ordinary runs can reuse verified caches.
`make gen-check` covers the shipping synth/FX generation only.

The native suite covers the quarter-phase
resampler, DC/passband/stopband behavior, guard contract and capture-step
arithmetic. The Python suite covers exact sample counts, WAV headers, manifest
provenance and the global calibration contract.

`capture.cpp` self-checks the 50 kHz rate over seconds 1–2 and the 16,000,000
cycles/s advance interval. A short or mis-scaled capture fails instead of
producing silence.

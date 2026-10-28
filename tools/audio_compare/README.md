# Sub-hauler audio emulator comparison

Two small Arduboy FX jukebox firmwares, built from the same UI source, for
auditioning the same six authored cues through two real device paths in Ardens:

- **LIVE ATM2FX** — the existing compact C1 streams decoded by the real
  ATMlib2FX synth, driving the real Timer4 PWM.
- **SAMPLED PWM** — the existing IMA ADPCM streams decoded by
  `src/audio/sampled_player_avr.hpp`, with the real Timer3 8 kHz sample ISR and
  Timer4 187.5 kHz PWM carrier.

Both firmwares load the **same complete** `fxdata/fxdata.bin` from this
checkout. Nothing is repacked, sliced or embedded in the firmware. This is a
listening/comparison demo; it is **not** a combined campaign/presentation
resource acceptance.

## Quick start

Double-click either launcher in this folder:

- `live-synth.command`
- `sampled-pwm.command`

Each launcher resolves its own absolute path, checks for the firmware and full
cart beside itself, and starts Ardens with `volume=100 display=ssd1306
fxport=d1`. It does not rebuild assets and does not depend on the current
working directory or an exported environment variable. Quit the first Ardens
window before starting the second so the two audible backends are never mixed.

Equivalent manual launch:

```sh
/Users/connorfranc/code/arduboy/Ardens/build/Ardens.app/Contents/MacOS/Ardens \
  volume=100 display=ssd1306 fxport=d1 \
  file="/absolute/path/to/firmware/live.ino.elf" \
  file="/absolute/path/to/cart/fxdata.bin"
```

Use `sampled.ino.elf` for the sampled build. The launchers use the ELF files so
Ardens has symbols; the matching HEX files are included for completeness.

## Controls

The cue order is identical in both builds:

1. Harbor Bubbles
2. Kelp Drift
3. Sonar Lullaby
4. Abyssal Glass
5. Tidepool Waltz
6. Bubblejet Run

| Input | Action |
| --- | --- |
| UP / LEFT | previous cue (wraps) |
| DOWN / RIGHT | next cue (wraps) |
| A | restart the selected cue |
| B | pause / resume |

The UI names the backend, the cue number/name, and PLAYING/PAUSED. Rendering is
native 1-bit and presents with exactly one `FX::display(CLEAR_BUFFER)` per
changed frame. Audio/cart service runs in the foreground before and after that
presentation and on idle frame-gate returns; no cart read happens inside the
OLED transaction. The UI uses the stock Arduboy font deliberately, so the
screen itself needs no cart reads.

## Important honesty notes

- The **sampled** assets are the captured steady loop cycle after one authored
  warmup loop. The **live** build starts the cue's real authored intro. Do not
  read the first sampled cycle as identical to the live intro; it is not.
- Switching builds or pressing A restarts the cue. The two builds are not phase
  synchronized and there is no shared transport/clock between them.
- The verification harness captures the raw emulator PWM output without gain,
  normalization, resampling or host-generated playback. The WAV files in
  `evidence/` are raw 50 kHz emulator captures for inspection only; the demo
  itself is the Ardens firmware.
- A paused sampled player can hold a tiny constant PWM DC level (measured mean
  2.0, AC peak 0.0 in this build); a paused live player held a constant DC level
  of 2000 with AC peak 0.0. Those are carrier/DC values, not music. Active cues
  were required to show nonconstant AC output and zero crossings.
- The sampled demo's maximum foreground service gap measured **146 Timer3
  ticks (18.25 ms)** in the native harness and **159 ticks (19.875 ms)** in the
  serial-evidence build. That is above the production 16 ms target and is a
  **demo-only limitation** (zero underruns were observed; the ring has 31.75 ms
  of compressed audio). Do not infer production timing acceptance from this
  demo.

## Verified images

Built 2026-10-27 from this worktree (HEAD `f340fb5`). Both fit the hardware
budgets:

| Image | `.text` | `.data` | `.bss` | Flash | Static SRAM |
| --- | ---: | ---: | ---: | ---: | ---: |
| `live.ino.elf` | 11,746 B | 236 B | 1,403 B | **11,982 / 29,696 B** | **1,639 / 2,560 B** |
| `sampled.ino.elf` | 9,458 B | 194 B | 1,326 B | **9,652 / 29,696 B** | **1,520 / 2,560 B** |

The sampled image links `audio::g_player`; the live image links
`atm_fx_compact_state`/`atmlib_state` and does not contain the other backend's
player state. Both are built with the same shared UI header and differ only by
`SH_AUDIO_COMPARE_BACKEND`.

The full cart is `cart/fxdata.bin`:

- size **1,876,992 B** (payload **1,876,927 B**)
- SHA-256 `586ca6d4b221eb46a19352b682142a8ed3d5cba000c62efb2db19ebbc327cd6c`

## Verification evidence

- `evidence/live-harness.log`, `evidence/sampled-harness.log` — native absim
  harness: loads the actual HEX and complete cart, injects D-pad/A/B, captures
  raw PWM, checks all six cues, A restart, B pause/resume, and frozen decoder
  position. Both end with `ok=1`.
- `evidence/live-ardens-serial.log`, `evidence/sampled-ardens-serial.log` —
  actual Ardens headless serial-evidence builds (`SH_AUDIO_COMPARE_AUTODEMO=1`)
  cycling all six cues, restart, pause and resume. Every line reports `ur=0
  err=0`.
- `evidence/*.wav` — raw 50 kHz emulator captures for the six cues per backend.
- `evidence/*.pbm` — raw 128×64 framebuffer captures at each cue.
- `evidence/sizes.txt`, `evidence/sha256sums.txt`, `evidence/symbols.txt` —
  exact size, hash and backend-symbol evidence.

Representative active waveform ranges from the harness (raw emulator units):

| Cue | Live min/max | Sampled min/max |
| --- | --- | --- |
| Harbor Bubbles | −372/378 | −444/456 |
| Kelp Drift | −314/320 | −380/378 |
| Sonar Lullaby | −326/332 | −372/378 |
| Abyssal Glass | −416/422 | −482/488 |
| Tidepool Waltz | −302/308 | −350/362 |
| Bubblejet Run | −326/332 | −404/402 |

All active captures had nonzero AC peak and zero crossings; all harness cue
records reported `error=0` and `underrun=0`.

## Reproduce inside the repo

```sh
make audio-compare-build       # both delivered images + complete-cart launch target data
make audio-compare-verify      # rebuild, run native absim input/waveform checks
make audio-compare-serial      # actual Ardens headless logs for both backends
make audio-compare-live        # launch live image in Ardens
make audio-compare-sampled     # launch sampled image in Ardens
```

Build inputs: `tools/audio_compare/audio_compare_ui.hpp`,
`tools/audio_compare/live/live.ino`, `tools/audio_compare/sampled/sampled.ino`,
`tools/audio_compare/harness.cpp`. Generated cart and audio headers come from
`make gen`; this comparison adds no generator changes.

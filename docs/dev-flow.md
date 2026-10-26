# Dev flow conventions

These rules are distilled from trawler-incremental, monhun-ardu, and
CreatureGathererFX. They are about the repo, not a particular tool. Canonical
hardware and resource facts live in `docs/platform.md`.

## Inner loop vs full gate

- Host: `make test`; tooling: `make test-tools`.
- One device suite while iterating: `make fxtest FXTEST_ONLY=test_boot`.
- Full device gate: `make fxtest` compiles every `tst/fxdatatest/test_*.ino`
  and requires an exact bare `P` marker. If `ARDENS` is unset or not
  executable, the run reports SKIPPED; an Ardens without `captureserial` is
  BLOCKED, never a pass.
- Full gate before a commit: `make gen-check`, `make test`, `make test-tools`,
  `make fxtest`, `make size`.

## Generated artifacts

- `make gen` is the only generation entry.
- `make gen-check` snapshots the generated set, re-runs `gen`, and fails on any
  byte change. Run `make gen` once before using it the first time.
- `fxdata/fxdata.txt` is an ordered flash layout. Declaration order is ABI:
  append new entries; never reorder or remove one after it has shipped.
- Stage generated sets together after a regen. Never commit a half-generated
  set.
- Tests read generated symbolic constants, never literal record indices.

## Budget-first for flash/SRAM work

- LTO makes per-symbol size math meaningless. Measure the whole image with
  `make size` (`.text` + `.data` for flash; `.data` + `.bss` for static SRAM)
  and compare against the limits in `docs/platform.md`.
- Spike before a change that can touch flash or RAM; report the whole-image
  delta. Split "implement" from "make it fit".
- Keep a stack reserve. Static free bytes alone do not prove stack fit.
- The current game image is deliberately tiny; replace the placeholder art
  early and re-measure.

## Device-test RAM budget

- Device suites compile with `--optimize-for-debug`, which keeps globals
  visible and often larger. Keep large constant tables in PROGMEM.
- If a suite corrupts earlier cases, inspect `.data`/`.bss` before suspecting
  render drift.

## Rendering and FX

- The OLED and cart share SPI. Cart reads happen in update/render, never during
  an OLED transaction. The canonical display contract is in `docs/platform.md`.
- Static menu screens compose once per input edge and skip the OLED
  transaction while idle. The station/shop board uses a 7 px row pitch, so the
  selection bar is a raw `fillRect` and the glyphs draw with background
  disabled (bg == fg); the Arduboy font cell is 8 px plus 1 px line spacing and
  an opaque background would paint through the next row.
- Greyscale: `FRAME(x)` selects one of three planes. Draw the same scene once
  per plane and keep the shade bitmask consistent across planes.
- Normal: `FRAME(x)` is the logical frame; `Blit::draw` reads headerless
  page-major plus-mask bytes.
- Generated sprite blobs start with a two-byte `(width, height)` header.
  `SpritesU::drawPlusMaskFX` owns that header; `Blit::draw` expects the caller
  to skip it.

## Save data

- `FX::begin(FX_DATA_PAGE)` is the single-argument form.
- The optional chart save uses the internal EEPROM: a 260 B versioned,
  checksummed block loaded once in setup and written only on the chart-close
  transition. It adds no static RAM and no cart-page allocation.
- For a future save that outgrows internal EEPROM, regenerate with
  `FX_SAVE_PAGE` and keep the versioned, checksummed block. Writes belong on
  transitions, never in the per-frame path.

## Developer traversal

`make dev` launches a separate `DEV_MODE=1` firmware in Ardens. D-pad moves
through walls and gates, A boosts speed, B opens the complete chart. It skips
station docking, world physics/hazards, pickups and contract time. Both health
resources stay full; chart-save reads/writes are suppressed. Coves and gate
appearances are retained for inspection. `make build DEV_MODE=1` builds without
launching, and `make test DEV_MODE=1` exercises free-flight plus ordinary rules.
Outputs live under `build/dev/PROFILE` and `dist/dev/PROFILE`; the default
`DEV_MODE=0` uses the normal directories and gameplay. This override is separate
from the shipped feature-profile list.

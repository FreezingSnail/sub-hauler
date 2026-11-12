# AGENTS.md — Sub-hauler

Sub-hauler is a small, measured Arduboy FX game. Read `README.md` for the
project, `docs/platform.md` for the hardware/resource contract, and
`docs/dev-flow.md` for workflow conventions.

## Source of truth

- `docs/platform.md` is canonical for hardware, resource, and toolchain facts.
- `src/game.hpp` is pure rules and must stay free of Arduino headers.
- `src/render.hpp` and `src/common.hpp` are the device boundary.
- `sub-hauler.ino` is the release entry; shared game operations live in `src/app.hpp`.
- There is one fixed game build. Do not add feature, profile, development,
  backend, USB or instrumentation selectors.
- `make gen` is the only shipping generation entry. Never hand-edit
  `fxdata/Sprites.txt`, `fxdata/fxdata.bin`, `src/fxdata.h`, or
  `src/generated/`.
- `fxdata/fxdata.txt` declaration order is flash ABI. Append entries; never
  reorder or remove shipped ones.
- `story/scenes.json` is canonical for scene order, prose and image-row names;
  `story/art.png` is the canonical Piskel artwork sheet (four 128x64 poses per row). Never duplicate shipping card copy in the HTML
  preview, generator or tests; run `make gen` after a source edit.

## Commands

```sh
make gen                  # images + FX layout -> packed image + headers
make gen-check            # regeneration must be byte-identical
make test                 # host C++ tests
make test-tools           # Python tooling tests
make docs-check           # local doc links + active selector guard
make build | mini         # FX (dist/fx) / Mini (dist/mini) firmware
make size                 # whole-image flash/RAM report
make fxtest               # 15 device suites via Ardens (P/F markers)
make fxtest-save-chain    # exact production save-chain stack/audio probe
make fxtest-flight        # 842-frame complete-flight timing/audio probe
make run                  # Ardens with ELF + FX image
```

Full gate before a commit: `make gen-check`, `make test`, `make test-tools`,
`make docs-check`, `make fxtest`, `make fxtest-save-chain`,
`make fxtest-save-runtime-page`, `make fxtest-flight`, `make size`, `make mini`.

## Platform invariants

Canonical hardware, resource, and toolchain facts live in `docs/platform.md`.
These are the invariants that change code behavior:

- Target Arduboy FX (ATmega32u4, 128×64 SSD1306, external FX flash).
- Normal renders native 1-bit via `Arduboy2Base`; `FRAME(x)` is one image and
  presentation is one `FX::display(CLEAR_BUFFER)` after rendering.
- The OLED and cart share SPI: cart reads happen in update/render, never during
  an OLED transaction.
- Program flash and SRAM are hard budgets. Measure the whole image with
  `make size` and keep an explicit stack reserve; limits and dated checkpoints
  live in `docs/platform.md`.
- No `float`/`double` in device code.

## Hard rules

- **Tests**: native framework per language; permanent and co-located.
  Host tests in `tst/`, device suites in `tst/fxdatatest/`, tool tests in
  `tools/tests/`. Never write test code to a temp directory and never use a
  scripting language as a harness for C++ tests.
- **Generated sets**: stage all generated artifacts together after a regen.
  Tests reference generated symbolic constants, not literal indices.
- **FX image**: `fxdata/fxdata.bin` is the only flashable cart image; table
  blobs are sections inside it.

- **24-bit label returns**: when a function selects an FX address/`txt_*`/`img_*`
  or another 24-bit constant across branches, assign it to a local once and use a
  single `return` at the end; prefer `if`/`else if`. Repeated `return <24-bit>;`
  sites emit duplicate 3-byte constant loads. Eight-bit enum selectors are exempt.

## Worker protocol

One task = one implement-verify loop. Run the exact verification commands for
the task, report exact output and numbers, and do not commit or push unless the
orchestrator asks. If a build cannot fit or a gate fails, report BLOCKED with
the deficit and options instead of faking a pass.

Use @pixelgen-assets for 128x64 monochrome assets.

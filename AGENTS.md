# AGENTS.md — Sub-hauler

Sub-hauler is a small, measured Arduboy FX game. Read `README.md` for the
project, `docs/platform.md` for the hardware/resource contract, and
`docs/dev-flow.md` for workflow conventions.

## Source of truth

- `docs/platform.md` is canonical for hardware, resource, and toolchain facts.
- `src/game.hpp` is pure rules and must stay free of Arduino headers.
- `src/render.hpp` and `src/common.hpp` are the device boundary.
- `make gen` is the only generation entry. Never hand-edit
  `fxdata/Sprites.txt`, `fxdata/fxdata.bin`, `src/fxdata.h`, or
  `src/generated/`.
- `fxdata/fxdata.txt` declaration order is flash ABI. Append entries; never
  reorder or remove shipped ones.

## Commands

```sh
make gen                  # images + FX layout -> packed image + headers
make gen-check            # regeneration must be byte-identical
make test                 # host C++ tests
make test-tools           # Python tooling tests
make build | mini         # FX / Mini firmware
make size                 # whole-image flash/RAM report
make fxtest               # device suites via Ardens (P/F markers)
make run                  # Ardens with ELF + FX image
```

Full gate before a commit: `make gen-check`, `make test`, `make test-tools`,
`make fxtest` (when Ardens is available), `make size`.

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

## Worker protocol

One task = one implement-verify loop. Run the exact verification commands for
the task, report exact output and numbers, and do not commit or push unless the
orchestrator asks. If a build cannot fit or a gate fails, report BLOCKED with
the deficit and options instead of faking a pass.

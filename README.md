# Sub-hauler

Sub-hauler is a game for Arduboy FX and Arduboy Mini.

Open the [offline player guide](docs/guide.html) to play; it uses relative
checkout assets and needs no network access.

Canonical project references:

- [`docs/platform.md`](docs/platform.md) — hardware, resource and toolchain facts.
- [`docs/design.md`](docs/design.md) — game direction and contract-loading model.
- [`docs/dev-flow.md`](docs/dev-flow.md) — inner loop, gates and repo conventions.
- [`docs/README.md`](docs/README.md) — documentation index.

## Status

There is one fixed shipping configuration. The developer-only `DEBUG=1` image
adds map-inspection no-clip and is not shipped. Exact flash/RAM headroom, test
counts and dated device measurements live in
[`docs/platform.md`](docs/platform.md).

## Build and test

```sh
make gen          # regenerate every shipped generated artifact
make gen-check    # regeneration must be byte-identical
make build        # FX firmware -> dist/fx/
make mini         # Mini firmware -> dist/mini/
make test         # host rules tests
make test-tools   # Python tooling tests
make docs-check   # doc links, generated pages and doc tooling
make size         # whole-image flash/RAM report
make fxtest       # fixed-build device suites via Ardens
make run          # Ardens with the ELF + FX image
```

Full gate: `make gen-check`, `make test`, `make test-tools`, `make docs-check`,
`make fxtest`, `make fxtest-save-chain`, `make fxtest-save-runtime-page`,
`make fxtest-flight`, `make size`, and `make mini`.

`make gen` and the firmware builds require the sibling `../ATMlib2FX` checkout.
Set `ATMLIB2FX_ROOT` to use another location.

## Release artifacts

Pushing a `v*` tag runs [the release workflow](.github/workflows/release.yml).
The same packages can be built locally:

```sh
make release-package RELEASE_VERSION=v1.2.3
```

This writes:

- `dist/release/sub-hauler-fx.arduboy`
- `dist/release/sub-hauler-mini.arduboy`

Each package contains its target HEX, `fxdata.bin` and a schema-3 `info.json`.

## Uploader entry

The release sketch uses `ARDUBOY_NO_USB`. A desktop uploader cannot force an
automatic reset from a running game. If the uploader waits for the board, reset
it or power-cycle it while holding **DOWN**, then start the upload.

## Project layout

```text
src/game.hpp       pure rules; no Arduino headers
src/render.hpp     native 1-bit scene and sprite drawing
src/common.hpp     device boundary: Arduboy2Base, FX cart and display
src/app.hpp        shared application operations and state
src/lib/Blit.*     headerless plus-mask FX blitter
sub-hauler.ino     release entry (ARDUBOY_NO_USB, setup, loop)
images/            authored sprite sheets
story/             canonical story, mission and dock-hint JSON + art
fxdata/            ordered flash layout and generated FX image
levels/            authored exploration data and format contract
tools/             generation and packaging tools
tst/               host tests and device suites
docs/              player guide and canonical project documents
```

## What to edit

- `story/scenes.json`, `story/missions.json`, `story/dock_hints.json` — story,
  mission and berth-hint text. Run `make editor` or edit the JSON, then
  `make gen`.
- `story/art.png` — canonical Piskel story-art sheet.
- `images/<section>/<symbol>_<W>x<H>.png` — authored sprites. Add a file, then
  run `make gen`.
- `fxdata/fxdata.txt` — ordered flash layout. Append entries; never reorder or
  remove shipped ones.
- `src/game.hpp` — pure rules. Keep Arduino headers out so host tests compile.
- `src/render.hpp` — native 1-bit scene, HUD and animation drawing.

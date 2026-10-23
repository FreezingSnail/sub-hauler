# Sub-hauler

Sub-hauler is an Arduboy FX game rendered with **Arduboy2** in native 1-bit
mode. Each logical frame is one image; `FRAME(x)` is simply `x`. Cart data is
fetched through `src/lib/Blit.cpp` and the display is pushed with
`FX::display(CLEAR_BUFFER)`.

Hardware, resource, and toolchain facts are canonical in
[`docs/platform.md`](docs/platform.md). Game direction and the cargo/FX plan
live in [`docs/design.md`](docs/design.md). The device-port sequence and
compile-time feature-flag plan live in
[`docs/roadmap.md`](docs/roadmap.md).

## Walkthrough

```sh
make gen          # images/*.png -> fxdata/Sprites.txt + fxdata.bin + headers
make build        # dist/<profile>/<project>.ino.hex + .elf
make test         # host rules tests
make fxtest       # device smoke test through Ardens
make run          # Ardens with the ELF + FX image
make size         # flash/RAM headroom
```

## Feature profiles

`tools/features.py` is the single source of profile values and dependency
rules. Valid profiles are `full` (default), `lean`, `core`, and `min`; every
flag is passed explicitly as a numeric `-DSH_FEATURE_*` macro.

```sh
make build PROFILE=lean
make size PROFILE=core
make feature-report PROFILE=lean
make size-matrix            # build and report full, lean, core, min
```

Builds and images are separated by profile (`build/<profile>/`,
`dist/<profile>/`) so objects compiled under different flag sets cannot mix.
An unknown profile or an invalid dependency fails before compilation. Feature
flags never change generated FX data.

## What to edit

- `src/game.hpp` — pure rules. Keep Arduino headers out of this file so the
  host tests can compile it.
- `src/render.hpp` — 1-bit drawing. Add scenes, HUD, and animation here.
- `sub-hauler.ino` — hardware setup, input sampling, and the frame loop.
- `images/<symbol>_<W>x<H>.png` — authored sprites. Add a file, run `make gen`.
- `fxdata/fxdata.txt` — ordered flash layout. Append new `image_t`/`raw_t`
  entries; order is ABI.

## Generated sprite contract

`tools/gen_sprites.py --shades 2` emits a two-byte `(width, height)` header and
one page-major plus-mask plane. `Blit::draw` addresses the headerless pixel
bytes, so the game passes `::hero + 2` and supplies the dimensions explicitly:

```cpp
Blit::draw(x, y, HERO_WIDTH, HERO_HEIGHT, ::hero + 2, FRAME(frame),
           Blit::PLUSMASK);
```

`Blit` writes into `Arduboy2Base::sBuffer`; it never touches the OLED itself.
Finish the frame with:

```cpp
FX::display(CLEAR_BUFFER);
```

## Display/FX loop

```cpp
if (!arduboy.nextFrame())
    return;
arduboy.pollButtons();
run();
render();
FX::display(CLEAR_BUFFER);
```

All cart reads stay in `run()`/`render()`; the display transaction is the only
SPI handoff after them.

## Save data

`FX::begin(FX_DATA_PAGE)` is the single-argument form. If the game needs a
save partition, regenerate with `FX_SAVE_PAGE` and add a checksummed EEPROM
save module. Keep writes out of the per-frame path.

## Layout

```
src/common.hpp      Arduboy2 + FX + FRAME
src/game.hpp        pure rules (host tests)
src/render.hpp      1-bit scene + sprite draw
src/lib/Blit.*      headerless plus-mask FX blitter
src/fxdata.h        generated mirror of fxdata/fxdata.h
src/generated/      generated sprite constants
fxdata/             fxdata.txt, Sprites.txt, packed .bin, fxdata.h
tst/                host tests + device suite
tools/              gen.sh, gen_sprites.py, fxdata-build.py, features.py, tests
spike/              v1 auto-forward feel test (not firmware)
spike-trim/         v2 pilot/cargo/fleet tabs, stations/contracts/shop (not firmware)
docs/platform.md    hardware/resource contract + dated checkpoints
docs/design.md      game direction, cargo model, FX arrangement plan
docs/roadmap.md     device-port milestones, feature flags, budgets
docs/dev-flow.md    repo conventions
```

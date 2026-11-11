#pragma once

#include <stdint.h>

#include <Arduboy2.h>
#include <ArduboyFX.h>

// Base, not Arduboy2: production text is baked FX stamps and never touches the
// inherited Print/virtual-write path, so the 1,280 B built-in font can be
// dropped by the linker. The external font adapter remains for reference tests.
extern Arduboy2Base arduboy;

// Native 1-bit assets are one byte per 8 vertical pixels. A frame is one
// image; FX sprite reads are page-major plus-mask blobs emitted by
// tools/gen_sprites.py.
#define FRAME(x) (x)

#include "lib/Blit.hpp"
#include "text.hpp"

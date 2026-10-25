#pragma once

#include <stdint.h>

#include <Arduboy2.h>
#include <ArduboyFX.h>

// Base, not Arduboy2: all text is drawn through the external FX font adapter,
// so the firmware never instantiates the inherited Print/virtual-write path and
// the 1,280 B built-in font can be dropped by the linker.
extern Arduboy2Base arduboy;

// Native 1-bit assets are one byte per 8 vertical pixels. A frame is one
// image; FX sprite reads are page-major plus-mask blobs emitted by
// tools/gen_sprites.py.
#define FRAME(x) (x)

#include "lib/Blit.hpp"
#include "text.hpp"

#pragma once

#include <stdint.h>

#include <Arduboy2.h>
#include <ArduboyFX.h>

extern Arduboy2 arduboy;

// Native 1-bit assets are one byte per 8 vertical pixels. A frame is one
// image; FX sprite reads are page-major plus-mask blobs emitted by
// tools/gen_sprites.py.
#define FRAME(x) (x)

#include "lib/Blit.hpp"

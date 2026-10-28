#pragma once

#include "features.hpp"

// Device cart-read service boundary. The streaming player's foreground refill
// is safe only outside an OLED transaction and before a long FX burst. Keep the
// hook in one place so world collision reads and baked presentation panels use
// the same rule. The game links the sibling ArduboyStreamFX library; the old
// local admitted player remains only in its standalone tools/tests.
#if defined(SH_AUDIO_PLAYER)
// Standalone local-player admission suite; never defined by the game sketch.
#include "audio/sampled_player_avr.hpp"
#define SH_CART_SERVICE() ::audio::service()
#elif SH_FEATURE_MUSIC
#include <ArduboyStreamFX.h>
#define SH_CART_SERVICE() ::streamfx::service()
#else
#define SH_CART_SERVICE() ((void)0)
#endif

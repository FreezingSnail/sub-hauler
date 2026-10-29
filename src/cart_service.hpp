#pragma once

#include "features.hpp"

// Device cart-read service boundary. The foreground refill is safe only outside
// an OLED transaction and before a long FX burst. Keep the hook in one place so
// world collision reads and baked presentation panels use the same rule.
// Production links the ATMlib2FX compact synth; the optional stream comparison
// keeps the earlier ArduboyStreamFX player. The old local admitted player
// remains only in its standalone tools/tests.
#if defined(SH_AUDIO_PLAYER)
// Standalone local-player admission suite; never defined by the game sketch.
#include "audio/sampled_player_avr.hpp"
#define SH_CART_SERVICE() ::audio::service()
#elif SH_FEATURE_MUSIC
#if SH_MUSIC_STREAMING
#include <ArduboyStreamFX.h>
#define SH_CART_SERVICE() ::streamfx::service()
#else
extern "C" {
#include <atm_synth.h>
#include <atm_fx_compact.h>
}
#define SH_CART_SERVICE() ::atm_fx_compact_service()
#endif
#else
#define SH_CART_SERVICE() ((void)0)
#endif

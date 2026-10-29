#pragma once

// Device cart-read service boundary. The foreground synth refill is safe only
// outside an OLED transaction and before a long FX burst. Keep it in one place
// so world collision reads and baked presentation panels share the same rule.
extern "C" {
#include <atm_synth.h>
#include <atm_fx_compact.h>
}

inline void cartService() {
    ::atm_fx_compact_service();
}

#include <ArduboyFX.h>
#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_MUSIC && !SH_MUSIC_STREAMING

// ATMlib2FX owns the decoder; the sketch owns the FX byte source. Offsets are
// data-section-relative and the current FX data page is added exactly once.
extern "C" void atm_fx_compact_fetch(uint32_t offset, uint8_t *dst, uint8_t n) {
    FX::readDataBytes(static_cast<uint24_t>(offset), dst, n);
}

#endif

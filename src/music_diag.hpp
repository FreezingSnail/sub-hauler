#pragma once

// Test-only synth diagnostics. The shipping music boundary exposes no
// diagnostic selectors; device suites compile with ATM_FX_COMPACT_DIAG=1 and
// include this adapter to read the dependency counters.
#include <stdint.h>

extern "C" {
#include <atm_fx_compact.h>
}

namespace music_diag {

inline bool underrun() {
#if ATM_FX_COMPACT_DIAG
    return atm_fx_compact_state.underruns != 0;
#else
    return false;
#endif
}

inline bool error() {
    return atm_fx_compact_state.error != 0;
}

inline uint16_t maxGapTicks() {
#if ATM_FX_COMPACT_DIAG
    return atm_fx_compact_state.max_service_gap;
#else
    return 0;
#endif
}

} // namespace music_diag

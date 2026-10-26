#pragma once
#include "features.hpp"
#if SH_DEV_MODE
#include "game.hpp"
#include "pilot.hpp"
#if SH_FEATURE_FOG_CHART
#include "chart.hpp"
#endif

// Explicit development build: direct bounded movement, no simulation/SPI reads.
// Tools, finds and gates keep their ordinary appearance for map inspection.
namespace dev_mode {
constexpr int16_t kStepPixels = 2;
constexpr int16_t kFastStepPixels = 6;

inline void prepare(game::State &state) {
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    state.xFrac = 0;
    state.yFrac = 0;
    state.air = game::airCapacity(state);
    state.impactAxis = 0;
    state.impactSpeedQ8 = 0;
#if SH_FEATURE_HULL_DAMAGE
    state.hull = game::hullCapacity(state);
    state.damageCooldown = 0;
#endif
#if SH_FEATURE_STATIONS
    state.docked = -1;
    state.dockCooldown = 0;
#endif
#if SH_FEATURE_EXPLORATION
    state.contactId = exploration::kNone;
    state.contactTicks = 0;
    state.discoveryTicks = 0;
    state.seenGates = static_cast<uint16_t>((1UL << exploration_data::GATE_COUNT) - 1);
#endif
}

inline void step(game::State &state, const game::Input &input, bool fast) {
    prepare(state);
    const int16_t distance = fast ? kFastStepPixels : kStepPixels;
    const int16_t dx = static_cast<int16_t>(input.right) - static_cast<int16_t>(input.left);
    const int16_t dy = static_cast<int16_t>(input.down) - static_cast<int16_t>(input.up);
    state.x = game::clampI16(static_cast<int16_t>(state.x + dx * distance), game::kHullHalfW, static_cast<int16_t>(world::PIXELS_X - game::kHullHalfW));
    state.y = game::clampI16(static_cast<int16_t>(state.y + dy * distance), game::kHullHalfH, static_cast<int16_t>(world::PIXELS_Y - game::kHullHalfH));
    if (dx)
        state.facingLeft = dx < 0;
    ++state.ticks;
    game::updateCamera(state);
}

#if SH_FEATURE_FOG_CHART
inline void revealChart(chart::State &chart) {
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
        chart.explored[i] = 0xFF;
}
#endif
}   // namespace dev_mode
#endif

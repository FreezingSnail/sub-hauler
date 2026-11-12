#pragma once

#include "pilot_view.hpp"

// Presentation-only wash, propeller and ballast cues for the pilot view.
// Motion is derived from flight ticks, so everything freezes with the rest of
// flight. Cues are supplied by the caller from the current input sample; no
// state is added and no cart read happens here.
namespace bubble_view {

constexpr int16_t kMinSpeedQ8 = 2 * game::kQ8One;

enum Cue : uint8_t {
    kCuePropeller = 1u << 0,   // horizontal thrust is applied
    kCueBoost = 1u << 1,       // A + owned thruster, so boost really applies
    kCueBlow = 1u << 2,        // up valve can still lighten the boat
    kCueFlood = 1u << 3,       // down valve can still fill the boat
};

// Map the live rules to presentation cues. Boost is only reported when the
// thruster is owned and horizontal thrust is applied, so holding A with no
// thruster (or while only trimming) never draws the boosted wash. Blowing is
// only a cue while it can actually spend air and lighten ballast; flooding is
// only a cue while ballast can still rise. Both-held cancels the valve, exactly
// like game::stepPhysics().
inline uint8_t cuesFor(const game::State &state, const game::Input &input) {
    uint8_t cues = 0;
    if (state.docked >= 0)
        return cues;
    if (input.left != input.right) {
        cues |= kCuePropeller;
        if (input.boost && game::thrusterOwned(state))
            cues |= kCueBoost;
    }
    const bool blow = input.up && !input.down;
    const bool flood = input.down && !input.up;
    if (blow) {
        if (state.air > 0 && state.ballast > 0)
            cues |= kCueBlow;
    } else if (flood) {
        if (state.ballast < game::kBallastMax)
            cues |= kCueFlood;
    }
    return cues;
}

// A short animated wake, projected from the boat without any particle state.
inline void draw(uint8_t *buffer, const game::State &state, const cave::Camera &camera, uint8_t cues = 0) {
    if (state.docked >= 0)
        return;

    const int16_t sx = pilot_view::screenX(state, camera);
    const int16_t sy = pilot_view::screenY(state, camera);

    // Potential-flow cues sit outside the 28x14 art box so the later HAULER
    // blit cannot erase them: vents rise above the mast and intake sinks below
    // the keel.
    if ((cues & kCueBlow) != 0) {
        const uint8_t phase = static_cast<uint8_t>(state.ticks & 7);
        for (uint8_t i = 0; i < 3; ++i) {
            const int16_t x = static_cast<int16_t>(sx - 8 + i * 8);
            const int16_t y = static_cast<int16_t>(sy - game::kHullHalfH - 7 - ((phase + i * 3) & 7));
            cave::setPixel(buffer, x, y);
        }
    } else if ((cues & kCueFlood) != 0) {
        const uint8_t phase = static_cast<uint8_t>(state.ticks & 7);
        for (uint8_t i = 0; i < 3; ++i) {
            const int16_t x = static_cast<int16_t>(sx - 8 + i * 8);
            const int16_t y = static_cast<int16_t>(sy + game::kHullHalfH + 3 + ((phase + i * 3) & 7));
            cave::setPixel(buffer, x, y);
        }
    }

    // A two-frame blade flicker at the stern. The first blade lifts while the
    // second falls; the flight clock owns the phase. Boost adds the opposite
    // blade pair so the higher thrust reads at a glance.
    if ((cues & kCuePropeller) != 0) {
        const int8_t back = state.facingLeft ? 1 : -1;
        // Sit just outside the 28x14 art box so the HAULER blit cannot erase
        // the blade, but still inside the first wake bubble.
        const int16_t x = static_cast<int16_t>(sx + back * (game::kHullHalfW + 2));
        const int8_t blade = static_cast<int8_t>((state.ticks & 2) != 0 ? 1 : -1);
        cave::setPixel(buffer, x, static_cast<int16_t>(sy + blade));
        if ((cues & kCueBoost) != 0)
            cave::setPixel(buffer, x, static_cast<int16_t>(sy - blade));
    }

    const bool movingRight = state.vxQ8 >= kMinSpeedQ8;
    const bool movingLeft = state.vxQ8 <= -kMinSpeedQ8;
    if (!movingRight && !movingLeft)
        return;

    const int8_t back = movingRight ? -1 : 1;
    const bool boosted = (cues & kCueBoost) != 0;
    const uint8_t trail = static_cast<uint8_t>(boosted ? 5 : 3);
    const uint8_t phase = static_cast<uint8_t>(state.ticks >> 2);
    for (uint8_t i = 0; i < trail; ++i) {
        const uint8_t age = static_cast<uint8_t>((phase + i * 5) & 15);
        const int16_t x = static_cast<int16_t>(sx + back * (game::kHullHalfW + 3 + age));
        const int16_t y = static_cast<int16_t>(sy - age / 4 + (i == 1 ? 2 : 0));
        cave::setPixel(buffer, x, y);
        if (boosted) {
            // Paired bubbles and a longer, denser trail distinguish real boost.
            cave::setPixel(buffer, x, static_cast<int16_t>(y + 3));
            if (age >= 6)
                cave::setPixel(buffer, static_cast<int16_t>(x + back), y);
        } else if (age >= 8) {
            cave::setPixel(buffer, static_cast<int16_t>(x + back), y);
        }
    }
}

} // namespace bubble_view

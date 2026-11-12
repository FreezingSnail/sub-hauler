#pragma once


#if defined(__AVR__)
#include <stdint.h>

#include "common.hpp"
#include "panel_view.hpp"
#include "startup_logic.hpp"
#include "generated/splash_sprites.hpp"

namespace startup_view {

// The Snailware brand screen holds for three seconds, or advances immediately
// when any player input is present.
constexpr uint16_t kBrandFrames = 180;

// Boot-only: before voyage time advances. The brand logo keeps its three-second
// beat; the game splash then waits indefinitely for a fresh press.
// A button already held to skip the logo must be released first, so that held
// input can never leak into the title menu. The final release is consumed
// before returning.
//
// The poll policy is injected so device fixtures can drive the exact production
// sequence deterministically without a real button press. service() runs
// between frames so the production poll can keep menu music fed while the
// splash waits.
template <typename Poll> inline void run(Poll &poll) {
    panel_view::draw(startup, 0, 64);
    FX::display(CLEAR_BUFFER);
    for (uint16_t tick = 0; tick < kBrandFrames; ++tick) {
        if (poll.buttons() != 0)
            break;
        poll.delay();
        poll.service();
    }

    panel_view::draw(startup, 1, 64);
    FX::display(CLEAR_BUFFER);

    // Wait for the logo-skip button to come up, then for a new press. With no
    // input the splash stays up forever.
    startup_logic::Splash splash;
    startup_logic::reset(splash);
    while (!startup_logic::tick(splash, poll.buttons() != 0)) {
        poll.delay();
        poll.service();
    }
    poll.waitNoButtons();
}

} // namespace startup_view
#endif

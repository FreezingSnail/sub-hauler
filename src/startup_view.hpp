#pragma once


#if defined(__AVR__)
#include <stdint.h>

#include "common.hpp"
#include "panel_view.hpp"
#include "startup_logic.hpp"
#include "generated/splash_sprites.hpp"

namespace startup_view {

// Boot-only: before music starts or voyage time advances. The brand logo keeps
// its short one-second beat; the game splash then waits indefinitely for a
// fresh press. A button already held to skip the logo must be released first,
// so that held input can never leak into the title menu. The final release is
// consumed before returning.
//
// The poll policy is injected so device fixtures can drive the exact production
// sequence deterministically without a real button press.
struct HardwarePoll {
    static uint8_t buttons() {
        return arduboy.buttonsState();
    }
    // Sleep through timer0 ticks instead of spinning in delay()/micros().
    // The splash has no work to do while it waits, so idle mode keeps the
    // boot screen off the CPU and saves battery. One idle() returns on the
    // next ~1 ms timer0 overflow; 16 of them preserve the 16 ms beat.
    static void delay() {
        for (uint8_t i = 0; i < 16; ++i)
            arduboy.idle();
    }
    // Same 50 ms debounce as Arduboy2Base::waitNoButtons(), but idling.
    static void waitNoButtons() {
        do {
            for (uint8_t i = 0; i < 50; ++i)
                arduboy.idle();
        } while (buttons() != 0);
    }
};

template <typename Poll> inline void run(Poll &poll) {
    panel_view::draw(startup, 0, 64);
    FX::display(CLEAR_BUFFER);
    for (uint8_t tick = 0; tick < 60; ++tick)
        poll.delay();

    panel_view::draw(startup, 1, 64);
    FX::display(CLEAR_BUFFER);

    // Wait for the logo-skip button to come up, then for a new press. With no
    // input the splash stays up forever.
    startup_logic::Splash splash;
    startup_logic::reset(splash);
    while (!startup_logic::tick(splash, poll.buttons() != 0))
        poll.delay();
    poll.waitNoButtons();
}

inline void show() {
    HardwarePoll poll;
    run(poll);
}

} // namespace startup_view
#endif

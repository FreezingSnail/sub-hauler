#pragma once


#if defined(__AVR__)
#include "common.hpp"
#include "panel_view.hpp"
#include "generated/splash_sprites.hpp"

namespace startup_view {

// Boot-only: before music starts or voyage time advances. Any held button
// skips the remaining introduction; consume its release before the title.
// The second frame is the actual title splash, so it stays up longer than the
// brand logo (2.4 s versus 1 s at the 16 ms tick).
inline void show() {
    for (uint8_t frame = 0; frame < STARTUP_FRAMES; ++frame) {
        panel_view::draw(startup, frame, 64);
        FX::display(CLEAR_BUFFER);
        const uint8_t ticks = frame == 0 ? 60 : 150;
        for (uint8_t tick = 0; tick < ticks; ++tick) {
            if (arduboy.buttonsState()) {
                arduboy.waitNoButtons();
                return;
            }
            arduboy.delayShort(16);
        }
    }
    arduboy.waitNoButtons();
}

} // namespace startup_view
#endif

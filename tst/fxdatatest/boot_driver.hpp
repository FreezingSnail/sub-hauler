// Deterministic startup driver for production device fixtures. The game splash
// waits indefinitely for a fresh press, so a fixture supplies one scripted
// press/release through the same applicationBootWith() path used by hardware.
#pragma once

#include <stdint.h>

#include "src/app.hpp"

namespace boot_driver {

struct Poll {
    uint16_t ticks = 0;
    uint8_t buttons() const {
        return ticks == 5 ? A_BUTTON : 0;
    }
    void delay() {
        ++ticks;
    }
    void service() {}
    void waitNoButtons() {}
};

inline void boot() {
    Poll poll;
    applicationBootWith(poll);
}

}   // namespace boot_driver

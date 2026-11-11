#pragma once

#include <stdint.h>

// Pure game-splash input policy. The brand logo is time-limited; after it, the
// splash stays up indefinitely. A button already held when the splash starts
// must be released before a fresh press can dismiss it, so a logo-skip input
// never leaks into the title menu. The release is consumed by the caller.
namespace startup_logic {

enum Splash : uint8_t {
    kWaitRelease = 0,
    kWaitPress,
    kDone,
};

inline void reset(Splash &splash) {
    splash = kWaitRelease;
}

// Returns true exactly once, on the fresh press that dismisses the splash.
inline bool tick(Splash &splash, bool pressed) {
    if (splash == kWaitRelease) {
        if (!pressed)
            splash = kWaitPress;
        return false;
    }
    if (splash == kWaitPress && pressed) {
        splash = kDone;
        return true;
    }
    return false;
}

inline bool done(const Splash &splash) {
    return splash == kDone;
}

}   // namespace startup_logic

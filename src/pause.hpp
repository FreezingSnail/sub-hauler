#pragma once

#include <stdint.h>

// Pure pause-screen transition. The sketch owns the Screen value; this header
// owns the input rule so the host can test enter/resume without linking an
// Arduboy. aPressed and bPressed are fresh just-pressed edges, not held levels:
// a held A+B chord must not re-enter Pause after a cargo commit, and a held
// resume button must not immediately reopen it. Either A or B alone resumes.
// The paused frame never reaches the world step, so travel, air, ballast, the
// job clock, and cart reads all stay frozen.
namespace pause {

enum Action : uint8_t {
    kNone,
    kEnter,
    kLeave
};

constexpr Action resolve(bool paused, bool aPressed, bool bPressed) {
    return paused ? ((aPressed || bPressed) ? kLeave : kNone)
                  : ((aPressed && bPressed) ? kEnter : kNone);
}

}   // namespace pause

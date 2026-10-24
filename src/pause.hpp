#pragma once

#include <stdint.h>

// Pure pause-screen transition. The sketch owns the Screen value; this header
// owns the input rule so the host can test enter/resume without linking an
// Arduboy. A+B opens the pause screen in free flight, and either A or B alone
// resumes. The paused frame never reaches the world step, so travel, air,
// ballast, the job clock, and cart reads all stay frozen.
namespace pause {

enum Action : uint8_t {
    kNone,
    kEnter,
    kLeave
};

constexpr Action resolve(bool paused, bool a, bool b) {
    return paused ? ((a || b) ? kLeave : kNone) : ((a && b) ? kEnter : kNone);
}

}   // namespace pause

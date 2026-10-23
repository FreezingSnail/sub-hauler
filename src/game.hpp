#pragma once

#include <stdint.h>

// Pure game state and rules. No Arduino headers: this file is compiled into
// the host test binary and the firmware from the same source.
namespace game {

constexpr int16_t kScreenWidth = 128;
constexpr int16_t kScreenHeight = 64;
constexpr int16_t kGroundY = 56;
constexpr int16_t kHalfWidth = 8;
constexpr int16_t kHalfHeight = 16;

struct Input {
    bool left;
    bool right;
    bool jump;
};

struct State {
    int16_t x;
    int16_t y;
    int8_t velocity;
    uint8_t frame;
    bool grounded;
    bool facingLeft;
    uint16_t ticks;
};

inline void reset(State &state) {
    state.x = kScreenWidth / 2;
    state.y = kGroundY;
    state.velocity = 0;
    state.frame = 0;
    state.grounded = true;
    state.facingLeft = false;
    state.ticks = 0;
}

inline void step(State &state, const Input &input) {
    const int8_t direction = static_cast<int8_t>(input.right) - static_cast<int8_t>(input.left);
    state.x += direction * 2;
    if (direction != 0) {
        state.facingLeft = direction < 0;
        state.frame = static_cast<uint8_t>((state.frame + 1) % 3);
    } else {
        state.frame = 0;
    }

    if (state.x < kHalfWidth)
        state.x = kHalfWidth;
    if (state.x > kScreenWidth - kHalfWidth)
        state.x = kScreenWidth - kHalfWidth;

    if (input.jump && state.grounded) {
        state.velocity = -4;
        state.grounded = false;
    }

    if (!state.grounded) {
        state.y += state.velocity;
        state.velocity++;
        if (state.y >= kGroundY) {
            state.y = kGroundY;
            state.velocity = 0;
            state.grounded = true;
        }
    }

    state.ticks++;
}

}   // namespace game

#pragma once

#include <stdint.h>

#include "campaign.hpp"

// Brief restoration beats. Cards are selected only from a new milestone bit at
// dock time; no story cursor is saved, so a reload never replays a beat. HOME
// keeps its existing pending-ending path; the revised thanks card carries the
// homecoming recognition. Pure selection rules stay host-testable.
namespace story_view {

// Zero is the bss default so the pending byte needs no startup initializer.
constexpr uint8_t kCardNone = 0;
constexpr uint8_t kCardFirst = 1;
constexpr uint8_t kCardPump = 2;
constexpr uint8_t kCardLift = 3;
constexpr uint8_t kCardBeacon = 4;
constexpr uint8_t kCardCount = 4;

// One new milestone per dock. Stable order also resolves a malformed state
// that somehow gains several bits at once.
inline uint8_t cardForNewMilestone(uint8_t before, uint8_t after) {
    const uint8_t fresh = static_cast<uint8_t>(after & ~before & campaign::kMilestoneMask);
    if (fresh & campaign::kMilestoneFirst)
        return kCardFirst;
    if (fresh & campaign::kMilestonePump)
        return kCardPump;
    if (fresh & campaign::kMilestoneLift)
        return kCardLift;
    if (fresh & campaign::kMilestoneBeacon)
        return kCardBeacon;
    return kCardNone;
}

#if defined(__AVR__)

#include "common.hpp"
#include "generated/story_sprites.hpp"
#include "panel_view.hpp"

inline uint24_t panel(uint8_t card) {
    if (card == kCardFirst)
        return story_first;
    if (card == kCardPump)
        return story_pump;
    if (card == kCardLift)
        return story_lift;
    if (card == kCardBeacon)
        return story_beacon;
    return 0;
}

inline void draw(uint8_t card) {
    const uint24_t image = panel(card);
    if (image)
        panel_view::draw(image);
}

#endif   // __AVR__

}   // namespace story_view

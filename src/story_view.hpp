#pragma once

#include <stdint.h>

#include "campaign.hpp"
#include "generated/story_beats.hpp"

// Finalized seven-beat story decks. Each beat alternates an animated image
// card (four poses at 3.3 fps) with a static text card. The FIRST beat has
// three pairs; every other beat has two. Selection is driven only by a new
// milestone/survey bit, so no story cursor is serialized and a reload never
// replays a completed beat. HOME joins the ending path and is resumable while
// the ending-presented flag is still clear.
namespace story_view {

constexpr uint8_t kCardNone = 0;
constexpr uint8_t kCardFirst = 1;
constexpr uint8_t kCardPumpSurvey = 2;
constexpr uint8_t kCardPump = 3;
constexpr uint8_t kCardLiftSurvey = 4;
constexpr uint8_t kCardLift = 5;
constexpr uint8_t kCardBeacon = 6;
constexpr uint8_t kCardHome = 7;
constexpr uint8_t kCardCount = 7;

static_assert(kCardCount == 7, "seven story beats are frozen");
static_assert(STORY_BEAT_CARDS == 30, "fifteen image cards plus fifteen text cards");
static_assert(STORY_BEAT_IMAGES == 15, "fifteen animated scenes");
static_assert(STORY_BEAT_FRAMES == 75, "sixty image poses plus fifteen static text cards");
static_assert(STORY_BEAT_MAX_CARDS == 6, "FIRST has three image/text pairs");

constexpr uint8_t cardStart(uint8_t beat) {
    return beat <= kCardFirst ? 0 : static_cast<uint8_t>(6 + (beat - 2) * 4);
}

inline uint8_t cardCount(uint8_t beat) {
    if (beat == kCardFirst)
        return 6;
    if (beat > kCardFirst && beat <= kCardCount)
        return 4;
    return 0;
}

constexpr uint8_t globalCard(uint8_t beat, uint8_t card) {
    return static_cast<uint8_t>(cardStart(beat) + card);
}

// Cards alternate image/text from the first image in every beat.
constexpr bool isImageCard(uint8_t global) {
    return (global & 1u) == 0;
}

// Frames are packed five per image/text pair (four poses, one static card).
constexpr uint8_t firstFrame(uint8_t global) {
    return static_cast<uint8_t>(global + 3 * ((global + 1) >> 1));
}

constexpr uint8_t firstFrameFor(uint8_t beat, uint8_t card) {
    return firstFrame(globalCard(beat, card));
}

// One new cargo milestone per dock, in stable order. HOME is intentionally
// absent: the ending path presents it after the final checkpoint.
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

// Survey beats are recorded by the in-world dwell, not by a dock transition.
// Pump wins a malformed two-bit gain so the order stays deterministic.
inline uint8_t cardForNewSurvey(uint8_t before, uint8_t after) {
    const uint8_t fresh = static_cast<uint8_t>(after & ~before & campaign::kSurveyMask);
    if (fresh & campaign::kSurveyPump)
        return kCardPumpSurvey;
    if (fresh & campaign::kSurveyLift)
        return kCardLiftSurvey;
    return kCardNone;
}

#if defined(__AVR__)

#include "common.hpp"

// The RLE control byte uses its high bit for a repeated byte and its low seven
// bits for a count minus one. Literal tokens hold count minus one bytes.
constexpr uint8_t kChunkBytes = 16;

inline uint8_t nextByte(uint24_t &source, uint8_t *buffer, uint8_t &position) {
    if (position == kChunkBytes) {
        cartService();
        FX::readDataBytes(source, buffer, kChunkBytes);
        source += kChunkBytes;
        position = 0;
    }
    return buffer[position++];
}

inline void decode(uint24_t source, uint8_t *dest) {
    uint8_t buffer[kChunkBytes];
    uint8_t position = kChunkBytes;
    uint8_t *const end = dest + STORY_BEAT_WIDTH * STORY_BEAT_HEIGHT / 8;
    while (dest < end) {
        const uint8_t token = nextByte(source, buffer, position);
        if (token & 0x80) {
            const uint8_t value = nextByte(source, buffer, position);
            uint8_t count = static_cast<uint8_t>((token & 0x7F) + 1);
            while (count-- && dest < end)
                *dest++ = value;
        } else {
            uint8_t count = static_cast<uint8_t>(token + 1);
            while (count-- && dest < end)
                *dest++ = nextByte(source, buffer, position);
        }
    }
}

inline uint16_t frameOffset(uint8_t frame) {
    uint8_t bytes[2];
    const uint24_t address = storyBeats + static_cast<uint24_t>(frame) * 2;
    cartService();
    FX::readDataBytes(address, bytes, 2);
    return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

inline void draw(uint8_t beat, uint8_t card, uint8_t phase) {
    const uint8_t count = cardCount(beat);
    if (count == 0)
        return;
    if (card >= count)
        card = static_cast<uint8_t>(count - 1);
    const uint8_t global = globalCard(beat, card);
    uint8_t frame = firstFrame(global);
    if (isImageCard(global))
        frame = static_cast<uint8_t>(frame + (phase & 3));
    decode(storyBeats + frameOffset(frame), Arduboy2Base::sBuffer);
}

inline void draw(uint8_t beat) {
    draw(beat, 0, 0);
}

#endif   // __AVR__

}   // namespace story_view

#pragma once

#include <stdint.h>

#include "campaign.hpp"
#include "generated/story_beats.hpp"

#if defined(__AVR__)
#include "common.hpp"
#endif

// Story scenes generated from story/scenes.json. Card kinds come from
// the generated source map, not image/text alternation: a scene may contain any
// number of consecutive text pages after automatic pagination. Scene ids 8..14
// remain the original departures/joint restoration; the appended ids 15..16
// hold the optional expedition result and its acceptance briefing, and id 17 is
// the opening deck. Selection is driven by a new milestone/survey bit or by
// accepting a cargo mission, so no story cursor is serialized. HOME joins the
// ending path and is resumable while the ending-presented flag is still clear.
// The opening deck is RAM-only: New Voyage shows it once and Continue resumes
// the committed berth checkpoint without replaying it.
namespace story_view {

constexpr uint8_t kSceneNone = 0;
constexpr uint8_t kSceneFirst = STORY_SCENE_FIRST_RESULT;
constexpr uint8_t kScenePumpSurvey = STORY_SCENE_PUMP_SURVEY;
constexpr uint8_t kScenePump = STORY_SCENE_PUMP_RESULT;
constexpr uint8_t kSceneLiftSurvey = STORY_SCENE_LIFT_SURVEY;
constexpr uint8_t kSceneLift = STORY_SCENE_LIFT_RESULT;
constexpr uint8_t kSceneBeacon = STORY_SCENE_BEACON_RESULT;
constexpr uint8_t kSceneHome = STORY_SCENE_HOME_RESULT;
constexpr uint8_t kSceneFirstDeparture = STORY_SCENE_FIRST_DEPARTURE;
constexpr uint8_t kScenePumpPickup = STORY_SCENE_PUMP_PICKUP;
constexpr uint8_t kSceneLiftPickup = STORY_SCENE_LIFT_PICKUP;
constexpr uint8_t kSceneJointMissing = STORY_SCENE_JOINT_MISSING;
constexpr uint8_t kSceneJointReady = STORY_SCENE_JOINT_READY;
constexpr uint8_t kSceneBeaconDeparture = STORY_SCENE_BEACON_DEPARTURE;
constexpr uint8_t kSceneHomeDeparture = STORY_SCENE_HOME_DEPARTURE;
constexpr uint8_t kSceneExpedition = STORY_SCENE_EXPEDITION_RESULT;
constexpr uint8_t kSceneExpeditionDeparture = STORY_SCENE_EXPEDITION_DEPARTURE;
constexpr uint8_t kSceneOpening = STORY_SCENE_OPENING;
constexpr uint8_t kSceneMax = STORY_SCENE_MAX;

// Frozen beat names remain the public ids used by milestone selection.
constexpr uint8_t kCardNone = kSceneNone;
constexpr uint8_t kCardFirst = kSceneFirst;
constexpr uint8_t kCardPumpSurvey = kScenePumpSurvey;
constexpr uint8_t kCardPump = kScenePump;
constexpr uint8_t kCardLiftSurvey = kSceneLiftSurvey;
constexpr uint8_t kCardLift = kSceneLift;
constexpr uint8_t kCardBeacon = kSceneBeacon;
constexpr uint8_t kCardHome = kSceneHome;
constexpr uint8_t kCardExpedition = kSceneExpedition;
constexpr uint8_t kCardCount = 7;
constexpr uint8_t kHandoffCount = 8;
constexpr uint8_t kHandoffFirst = kSceneFirstDeparture;
constexpr uint8_t kHandoffPump = kScenePumpPickup;
constexpr uint8_t kHandoffLift = kSceneLiftPickup;
constexpr uint8_t kHandoffJointMissing = kSceneJointMissing;
constexpr uint8_t kHandoffJointReady = kSceneJointReady;
constexpr uint8_t kHandoffBeacon = kSceneBeaconDeparture;
constexpr uint8_t kHandoffHome = kSceneHomeDeparture;
constexpr uint8_t kHandoffExpedition = kSceneExpeditionDeparture;

static_assert(kSceneExpedition == 15, "existing expedition result scene id stays frozen");
static_assert(kSceneExpeditionDeparture == 16, "new expedition briefing appends after every existing scene");
static_assert(kSceneOpening == 17, "opening deck appends after every existing scene");
static_assert(STORY_CARD_COUNT <= 0xFF, "card ids fit the pending-scene metadata");
static_assert(STORY_FRAME_COUNT <= 0xFF, "frame ids fit one-byte card metadata");

#if defined(__AVR__)
// Story metadata is cold and lives on the FX cart. One-byte reads keep SRAM
// flat and avoid a program-flash mirror of the four lookup tables.
inline uint8_t metaByte(uint24_t address) {
    uint8_t value;
    cartService();
    FX::readDataBytes(address, &value, 1);
    return value;
}
#endif

inline uint8_t sceneCards(uint8_t scene) {
    if (scene > STORY_SCENE_MAX)
        return 0;
#if defined(__AVR__)
    return metaByte(storyMeta + STORY_META_SCENE_CARDS + scene);
#else
    return STORY_SCENE_CARDS[scene];
#endif
}

inline uint8_t sceneFirstCard(uint8_t scene) {
    if (scene > STORY_SCENE_MAX)
        return 0;
#if defined(__AVR__)
    return metaByte(storyMeta + STORY_META_SCENE_FIRST + scene);
#else
    return STORY_SCENE_FIRST_CARD[scene];
#endif
}

inline uint8_t cardFrame(uint8_t scene, uint8_t card) {
    if (scene > STORY_SCENE_MAX || card >= sceneCards(scene))
        return 0;
    const uint8_t index = static_cast<uint8_t>(sceneFirstCard(scene) + card);
#if defined(__AVR__)
    return metaByte(storyMeta + STORY_META_CARD_FRAME + index);
#else
    return STORY_CARD_FRAME[index];
#endif
}

// A saved pending-scene byte is none or one of the generated stable scenes.
constexpr bool validScene(uint8_t scene) {
    return scene <= kSceneMax;
}

constexpr bool validCard(uint8_t scene) {
    return validScene(scene);
}

inline uint8_t cardCount(uint8_t scene) {
    return sceneCards(scene);
}

inline uint8_t cardKind(uint8_t scene, uint8_t card) {
    if (scene > STORY_SCENE_MAX || card >= sceneCards(scene))
        return STORY_KIND_TEXT;
    const uint8_t index = static_cast<uint8_t>(sceneFirstCard(scene) + card);
#if defined(__AVR__)
    return metaByte(storyMeta + STORY_META_CARD_KIND + index);
#else
    return STORY_CARD_KIND[index];
#endif
}

inline bool isImageCard(uint8_t scene, uint8_t card) {
    return cardKind(scene, card) == STORY_KIND_IMAGE;
}

inline uint8_t firstFrameFor(uint8_t scene, uint8_t card) {
    return cardFrame(scene, card);
}

// Cargo acceptance owns the departure scenes. Surveys never load cargo
// and HOME still uses the explicit WORK row.
inline uint8_t handoffForMission(uint8_t mission) {
    return mission == campaign::kFirst ? kHandoffFirst :
           mission == campaign::kPump ? kHandoffPump :
           mission == campaign::kLift ? kHandoffLift :
           mission == campaign::kBeacon ? kHandoffBeacon :
           mission == campaign::kHome ? kHandoffHome :
           mission == campaign::kExpedition ? kHandoffExpedition : kSceneNone;
}

// The joint-restoration scene is chosen from saved seal ownership, so a reload
// after the second repair always gets the correct lead.
constexpr uint8_t jointHandoff(bool sealHeld) {
    return sealHeld ? kHandoffJointReady : kHandoffJointMissing;
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
    if (fresh & campaign::kMilestoneExpedition)
        return kCardExpedition;
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

inline uint24_t frameOffset(uint8_t frame) {
    uint8_t bytes[3];
    const uint24_t address = storyBeats + static_cast<uint24_t>(frame) * 3;
    cartService();
    FX::readDataBytes(address, bytes, 3);
    return static_cast<uint24_t>(bytes[0]) | (static_cast<uint24_t>(bytes[1]) << 8) |
           (static_cast<uint24_t>(bytes[2]) << 16);
}

inline void drawFrame(uint8_t frame) {
    decode(storyBeats + frameOffset(frame), Arduboy2Base::sBuffer);
}

inline void draw(uint8_t scene, uint8_t card, uint8_t phase) {
    const uint8_t count = cardCount(scene);
    if (count == 0)
        return;
    if (card >= count)
        card = static_cast<uint8_t>(count - 1);
    uint8_t frame = cardFrame(scene, card);
    if (isImageCard(scene, card))
        frame = static_cast<uint8_t>(frame + (phase & 3));
    drawFrame(frame);
}

inline void draw(uint8_t scene) {
    draw(scene, 0, 0);
}

#endif   // __AVR__

}   // namespace story_view

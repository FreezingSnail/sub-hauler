#pragma once

#include <stdint.h>



#include "game.hpp"
#include "world.hpp"
#include "generated/exploration.hpp"

// Pure campaign rules: stable mission/survey/berth IDs, prerequisites, repair
// ordering, one-time completion flags and the bounded active-job invariants.
// No Arduino headers, heap, float or generated literal indices. Mission IDs and
// prerequisite order are frozen by docs/progression-implementation.md; story
// deliveries carry no payout.
namespace campaign {

using game::State;

constexpr uint8_t kMissionNone = game::kMissionNone;
constexpr uint8_t kMissionCount = 8;

enum MissionId : uint8_t {
    kFirst = 0,
    kPumpSurvey = 1,
    kPump = 2,
    kLiftSurvey = 3,
    kLift = 4,
    kBeacon = 5,
    kHome = 6,
    kExpedition = 7,
};

enum SurveyId : uint8_t {
    kPumpSurveyId = 0,
    kLiftSurveyId = 1,
};

// Milestone bits. The campaign route is linear at the end: BEACON needs PUMP
// and LIFT; DEEP RELAY needs BEACON plus all four tools; HOME needs BEACON and
// the completed DEEP RELAY.
constexpr uint8_t kMilestoneFirst = 1u << 0;
constexpr uint8_t kMilestonePump = 1u << 1;
constexpr uint8_t kMilestoneLift = 1u << 2;
constexpr uint8_t kMilestoneBeacon = 1u << 3;
constexpr uint8_t kMilestoneHome = 1u << 4;
constexpr uint8_t kMilestoneExpedition = 1u << 5;
constexpr uint8_t kMilestoneMask = 0x3Fu;

constexpr uint8_t kSurveyPump = 1u << kPumpSurveyId;
constexpr uint8_t kSurveyLift = 1u << kLiftSurveyId;
constexpr uint8_t kSurveyMask = 0x03u;

// Berth IDs. 0-3 are the only freight settlements; 4-6 are work/service sites.
enum Berth : uint8_t {
    kTidewall = 0,
    kBenthos = 1,
    kCormorant = 2,
    kDeepAnchor = 3,
    kPumpRefuge = 4,
    kLowerGantry = 5,
    kFreightYard = 6,
    kDeepRelay = 7,
};
constexpr uint8_t kBerthCount = 8;
constexpr uint8_t kSettlementCount = world::SETTLEMENT_COUNT;
static_assert(world::BERTH_COUNT == kBerthCount, "generated berth ids match the frozen campaign count");
static_assert(world::SERVICE_BERTH_COUNT == 4, "both restoration berths and the two work sites have generated geography");
static_assert(world::BERTH_PUMP_REFUGE == kPumpRefuge, "Pump Refuge keeps its frozen berth id");
static_assert(world::BERTH_LOWER_GANTRY == kLowerGantry, "Lower Gantry keeps its frozen berth id");
static_assert(world::SURVEY_PUMP == kPumpSurveyId, "pump survey keeps its frozen id");
static_assert(world::SURVEY_LIFT == kLiftSurveyId, "lift survey keeps its frozen id");

constexpr uint8_t kNoBit = 0xFF;

// Mission metadata is flash-resident. On AVR an odr-used constexpr array is
// copied to SRAM by the startup code, so the device reads this table through
// PROGMEM while the host keeps the plain constexpr copy.
struct MissionSpec {
    uint8_t from;
    uint8_t to;
    bool cargo;
    uint8_t milestone;   // kNoBit for survey missions
    uint8_t survey;      // kNoBit for cargo missions
};

#if defined(__AVR__)
const MissionSpec kMissions[kMissionCount] PROGMEM = {
#else
constexpr MissionSpec kMissions[kMissionCount] = {
#endif
    {kTidewall, kBenthos, true, kMilestoneFirst, kNoBit},             // FIRST
    {kBenthos, game::kJobNone, false, kNoBit, kPumpSurveyId},         // PUMP_SURVEY
    {kCormorant, kPumpRefuge, true, kMilestonePump, kNoBit},          // PUMP
    {kCormorant, game::kJobNone, false, kNoBit, kLiftSurveyId},       // LIFT_SURVEY
    {kBenthos, kFreightYard, true, kMilestoneLift, kNoBit},           // LIFT
    {kCormorant, kDeepAnchor, true, kMilestoneBeacon, kNoBit},        // BEACON
    {kDeepAnchor, kTidewall, true, kMilestoneHome, kNoBit},           // HOME
    {kDeepAnchor, kDeepRelay, true, kMilestoneExpedition, kNoBit},    // EXPEDITION
};

inline bool missionInfo(uint8_t id, MissionSpec &out) {
    if (id >= kMissionCount)
        return false;
#if defined(__AVR__)
    out.from = pgm_read_byte(&kMissions[id].from);
    out.to = pgm_read_byte(&kMissions[id].to);
    out.cargo = pgm_read_byte(&kMissions[id].cargo) != 0;
    out.milestone = pgm_read_byte(&kMissions[id].milestone);
    out.survey = pgm_read_byte(&kMissions[id].survey);
#else
    out = kMissions[id];
#endif
    return true;
}

inline bool isCargoMission(uint8_t id) {
    MissionSpec spec;
    return missionInfo(id, spec) && spec.cargo;
}

inline uint8_t missionForSurvey(uint8_t surveyId) {
    if (surveyId == kPumpSurveyId)
        return kPumpSurvey;
    if (surveyId == kLiftSurveyId)
        return kLiftSurvey;
    return kMissionNone;
}

inline uint8_t surveyForMission(uint8_t missionId) {
    if (missionId == kPumpSurvey)
        return kPumpSurveyId;
    if (missionId == kLiftSurvey)
        return kLiftSurveyId;
    return game::kJobNone;
}

inline bool sealOwned(const State &state) {
    return (state.tools & exploration_data::SEAL) != 0;
}

inline bool hasMilestone(uint8_t campaign, uint8_t bit) {
    return (campaign & bit) != 0;
}

inline bool hasMilestone(const State &state, uint8_t bit) {
    return hasMilestone(state.campaign, bit);
}

inline bool surveyRecorded(uint8_t surveys, uint8_t surveyId) {
    if (surveyId > kLiftSurveyId)
        return false;
    return (surveys & static_cast<uint8_t>(1u << surveyId)) != 0;
}

inline bool surveyRecorded(const State &state, uint8_t surveyId) {
    return surveyRecorded(state.surveys, surveyId);
}

// A pending survey ring owns its work site. The ring's wide stop-hint radius
// covers the whole inactive-berth approach, so both the repair notice and the
// inactive mooring stay hidden until the pilot records the ring.
constexpr int16_t kSurveySiteRadius = 48;
inline bool pendingSurveyAt(const State &state, int16_t x, int16_t y) {
    for (uint8_t i = 0; i < world::surveyCount(); ++i) {
        world::Survey survey;
        if (!world::surveyInfo(i, survey) || surveyRecorded(state, survey.id))
            continue;
        const int16_t dx = static_cast<int16_t>(x - survey.pixel_x);
        const int16_t dy = static_cast<int16_t>(y - survey.pixel_y);
        if (dx >= -kSurveySiteRadius && dx <= kSurveySiteRadius && dy >= -kSurveySiteRadius && dy <= kSurveySiteRadius)
            return true;
    }
    return false;
}

inline bool missionCompleted(uint8_t campaign, uint8_t surveys, uint8_t id) {
    MissionSpec spec;
    if (!missionInfo(id, spec))
        return false;
    if (spec.milestone != kNoBit)
        return hasMilestone(campaign, spec.milestone);
    return surveyRecorded(surveys, spec.survey);
}

inline bool missionCompleted(const State &state, uint8_t id) {
    return missionCompleted(state.campaign, state.surveys, id);
}

// Station-board next-stop hint after a story deck finishes. Only the two repair
// deliveries post one, and the choice is derived from the saved campaign facts
// so the notice can never disagree with the WORK rows or chart markers. The
// enum is presentation-neutral: app.hpp maps it to the shipped FX strings.
enum NextStop : uint8_t {
    kNextStopNone = 0,
    kNextStopLiftSurvey,
    kNextStopLiftSource,
    kNextStopPumpSurvey,
    kNextStopPumpSource,
    kNextStopSeal,
    kNextStopBeacon,
};

// Symmetric branch guidance after either repair deck. A missing survey is
// named before its repair source, and BEACON is never announced until both
// repairs and the pressure seal are held. The completed mission id only gates
// whether a repair deck may post a notice at all.
inline NextStop nextStopAfter(const State &state, uint8_t completedMissionId) {
    if (completedMissionId != kPump && completedMissionId != kLift)
        return kNextStopNone;
    if (!hasMilestone(state, kMilestoneLift)) {
        if (!surveyRecorded(state, kLiftSurveyId))
            return kNextStopLiftSurvey;
        return kNextStopLiftSource;
    }
    if (!hasMilestone(state, kMilestonePump)) {
        if (!surveyRecorded(state, kPumpSurveyId))
            return kNextStopPumpSurvey;
        return kNextStopPumpSource;
    }
    return sealOwned(state) ? kNextStopBeacon : kNextStopSeal;
}

// One dock-hint point per saved campaign, survey and equipment shape. The order is the canonical
// JSON column order in story/dock_hints.json; the station and WORK boards use
// the same point at every berth so a line always describes the voyage's actual
// stage. PUMP and LIFT handle either repair order. The appended equipment
// points keep the post-BEACON lead actionable until DEEP RELAY is available.
enum DockPoint : uint8_t {
    kDockPointFresh = 0,
    kDockPointFirst,
    kDockPointPump,
    kDockPointLift,
    kDockPointBoth,
    kDockPointSeal,
    kDockPointCore,
    kDockPointRelay,
    kDockPointHome,
    kDockPointCutter,
    kDockPointLens,
    kDockPointThruster,
    kDockPointPumpReady,
    kDockPointLiftReady,
};
constexpr uint8_t kDockPointCount = kDockPointLiftReady + 1;

inline DockPoint dockPoint(const State &state) {
    if (!hasMilestone(state, kMilestoneFirst))
        return kDockPointFresh;

    const bool pump = hasMilestone(state, kMilestonePump);
    const bool lift = hasMilestone(state, kMilestoneLift);
    if (!pump && !lift)
        return kDockPointFirst;
    if (pump && !lift)
        return surveyRecorded(state, kLiftSurveyId) ? kDockPointLiftReady : kDockPointPump;
    if (!pump && lift)
        return surveyRecorded(state, kPumpSurveyId) ? kDockPointPumpReady : kDockPointLift;
    // HOME is the final presentation state whenever it is set.
    if (hasMilestone(state, kMilestoneHome))
        return kDockPointHome;
    if (!sealOwned(state))
        return kDockPointBoth;
    if (!hasMilestone(state, kMilestoneBeacon))
        return kDockPointSeal;
    if (!hasMilestone(state, kMilestoneExpedition)) {
        if (!(state.tools & exploration_data::CUTTER))
            return kDockPointCutter;
        // The lens is in the cave above Deep Anchor. Name this nearby find
        // before sending a pilot back to the shallows for the thruster.
        if (!(state.tools & exploration_data::LENS))
            return kDockPointLens;
        if (!(state.tools & exploration_data::THRUSTER))
            return kDockPointThruster;
        return kDockPointCore;
    }
    return kDockPointRelay;
}

// Prerequisites without the "not already complete" test. Completion flags are
// deliberately independent of cargo; surveys can be recorded in any order.
inline bool missionPrerequisites(uint8_t campaign, uint8_t surveys, uint8_t tools, uint8_t id) {
    switch (id) {
    case kFirst:
        return true;
    case kPumpSurvey:
    case kLiftSurvey:
        return hasMilestone(campaign, kMilestoneFirst);
    case kPump:
        return hasMilestone(campaign, kMilestoneFirst) && surveyRecorded(surveys, kPumpSurveyId);
    case kLift:
        return hasMilestone(campaign, kMilestoneFirst) && surveyRecorded(surveys, kLiftSurveyId);
    case kBeacon:
        return hasMilestone(campaign, kMilestonePump) && hasMilestone(campaign, kMilestoneLift) && (tools & exploration_data::SEAL) != 0;
    case kHome:
        return hasMilestone(campaign, kMilestoneBeacon) && hasMilestone(campaign, kMilestoneExpedition);
    case kExpedition:
        // The mandatory late Deep Anchor expedition. The survey lens reads the
        // buried relay; cutter, seal and thruster make the lower-right wreck
        // reachable and survivable. All four are explicit prerequisites.
        return hasMilestone(campaign, kMilestoneBeacon) &&
               (tools & (exploration_data::CUTTER | exploration_data::SEAL | exploration_data::THRUSTER | exploration_data::LENS)) ==
                   (exploration_data::CUTTER | exploration_data::SEAL | exploration_data::THRUSTER | exploration_data::LENS);
    default:
        return false;
    }
}

inline bool missionPrerequisites(const State &state, uint8_t id) {
    return missionPrerequisites(state.campaign, state.surveys, state.tools, id);
}

inline bool missionAvailable(const State &state, uint8_t id) {
    // Prerequisites reject every unknown id, so a separate metadata read is redundant.
    return missionPrerequisites(state, id) && !missionCompleted(state, id);
}

// Focused gameplay check for an active cargo contract. It deliberately does
// not validate the whole save: a written checkpoint is loaded raw. Delivery
// still refuses to complete a job whose mission metadata, origin, destination
// or fixed crate requirement does not match.
inline bool cargoJobMatches(const game::ActiveJob &job) {
    if (job.kind != game::kJobKindCampaign)
        return false;
    MissionSpec spec;
    if (!missionInfo(job.mission, spec) || !spec.cargo)
        return false;
    return job.from == spec.from && job.to == spec.to && job.crates == game::kJobCrates;
}

// WORK page rows. Story missions are compacted in stable mission-id order at
// their clue or source berth; the two field surveys therefore appear as
// non-cargo quest rows. Deep Anchor exposes the mandatory DEEP RELAY row after
// BEACON, then HOME after the relay. After HOME, Tidewall adds a replayable VIEW
// ENDING row. kWorkEnding is outside the frozen mission ids.
constexpr uint8_t kWorkEnding = 0xFE;

inline bool endingAvailable(const State &state) {
    return missionCompleted(state, kHome);
}

inline uint8_t workMissionAt(const State &state, uint8_t berth, uint8_t row) {
    uint8_t visible = 0;
    for (uint8_t id = kFirst; id <= kExpedition; ++id) {
        MissionSpec spec;
        if (!missionInfo(id, spec) || spec.from != berth)
            continue;
        if (!missionAvailable(state, id))
            continue;
        if (visible == row)
            return id;
        ++visible;
    }
    if (berth == kTidewall && endingAvailable(state)) {
        if (visible == row)
            return kWorkEnding;
    }
    return kMissionNone;
}

// A recorded survey is a fact, not a cargo job: it never occupies the slot and
// cannot be repeated. Returns false only for an unknown survey id.
inline bool recordSurvey(State &state, uint8_t surveyId) {
    if (surveyId > kLiftSurveyId)
        return false;
    state.surveys |= static_cast<uint8_t>(1u << surveyId);
    return true;
}

inline bool completeMission(State &state, uint8_t id) {
    MissionSpec spec;
    if (!missionInfo(id, spec) || missionCompleted(state, id))
        return false;
    if (!missionPrerequisites(state, id))
        return false;
    if (!spec.cargo) {
        state.surveys |= static_cast<uint8_t>(1u << surveyForMission(id));
        return true;
    }
    // Cargo metadata already contains the exact milestone selected by this mission.
    state.campaign |= spec.milestone;
    return true;
}

inline bool berthAvailable(uint8_t campaign, uint8_t berth) {
    if (berth < world::SETTLEMENT_COUNT)
        return true;
    world::Berth info;
    if (!world::berthInfo(berth, info) || info.milestone == 0)
        return false;
    return hasMilestone(campaign, info.milestone);
}

// Restoration normally unlocks a work berth. While the matching repair load is
// aboard, that berth is also a valid destination before its own milestone: this
// is what lets the pump and lift accept delivery at the machine that they will
// restore, without a self-locking gate.
inline bool berthAvailable(const State &state, uint8_t berth) {
    if (berthAvailable(state.campaign, berth))
        return true;
    return game::hasActiveJob(state) && state.activeJob.to == berth;
}

inline bool freightBerth(uint8_t berth) {
    return berth < world::SETTLEMENT_COUNT;
}

inline bool markEndingPresented(State &state) {
    if (!hasMilestone(state, kMilestoneHome))
        return false;
    state.ending = 1;
    return true;
}

// A campaign offer can only be accepted at its source, while the single load
// slot is empty and the mission prerequisites hold. BEACON/HOME therefore need
// explicit acceptance; docking never auto-loads them.
inline bool canAccept(const State &state, uint8_t id) {
    if (state.docked < 0)
        return false;
    if (game::hasActiveJob(state))
        return false;
    MissionSpec spec;
    if (!missionInfo(id, spec) || !spec.cargo)
        return false;
    if (spec.from != static_cast<uint8_t>(state.docked))
        return false;
    return missionAvailable(state, id);
}

inline bool accept(State &state, uint8_t id) {
    if (!canAccept(state, id))
        return false;
    MissionSpec spec;
    missionInfo(id, spec);

    game::setCargo(state, 0);
    state.activeJob.kind = game::kJobKindCampaign;
    state.activeJob.mission = id;
    state.activeJob.from = spec.from;
    state.activeJob.to = spec.to;
    state.activeJob.crates = game::kJobCrates;
    game::setCargo(state, game::kJobCrates);
    state.ballast = static_cast<uint8_t>(game::neutralForNormalized(state.cargo));
    return true;
}

}   // namespace campaign


#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_CAMPAIGN

#include "game.hpp"
#include "world.hpp"
#if SH_FEATURE_EXPLORATION
#include "generated/exploration.hpp"
#endif

// Pure campaign rules: stable mission/survey/berth IDs, prerequisites, repair
// ordering, one-time completion flags and the bounded active-job invariants.
// No Arduino headers, heap, float or generated literal indices. The route
// reward is read from the generated economy; mission IDs and prerequisite
// order are frozen by docs/progression-implementation.md.
namespace campaign {

using game::State;

constexpr uint8_t kMissionNone = game::kMissionNone;
constexpr uint8_t kMissionCount = 7;
constexpr uint8_t kDeliveryNone = 0;   // jobs::DeliveryResult::kDeliveryNone

enum MissionId : uint8_t {
    kFirst = 0,
    kPumpSurvey = 1,
    kPump = 2,
    kLiftSurvey = 3,
    kLift = 4,
    kBeacon = 5,
    kHome = 6,
};

enum SurveyId : uint8_t {
    kPumpSurveyId = 0,
    kLiftSurveyId = 1,
};

// Milestone bits. Monotonic: BEACON needs PUMP and LIFT; HOME needs BEACON.
constexpr uint8_t kMilestoneFirst = 1u << 0;
constexpr uint8_t kMilestonePump = 1u << 1;
constexpr uint8_t kMilestoneLift = 1u << 2;
constexpr uint8_t kMilestoneBeacon = 1u << 3;
constexpr uint8_t kMilestoneHome = 1u << 4;
constexpr uint8_t kMilestoneMask = 0x1Fu;

constexpr uint8_t kSurveyPump = 1u << kPumpSurveyId;
constexpr uint8_t kSurveyLift = 1u << kLiftSurveyId;
constexpr uint8_t kSurveyMask = 0x03u;

// Berth IDs. 0-3 are the only freight settlements; 4-5 are service-only.
enum Berth : uint8_t {
    kTidewall = 0,
    kBenthos = 1,
    kCormorant = 2,
    kDeepAnchor = 3,
    kPumpRefuge = 4,
    kLowerGantry = 5,
};
constexpr uint8_t kBerthCount = 6;
constexpr uint8_t kSettlementCount = world::SETTLEMENT_COUNT;
static_assert(world::BERTH_COUNT == kBerthCount, "generated berth ids match the frozen campaign count");
static_assert(world::SERVICE_BERTH_COUNT == 2, "both restoration berths have generated geography");
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
    {game::kJobNone, game::kJobNone, false, kNoBit, kPumpSurveyId},   // PUMP_SURVEY
    {kCormorant, kBenthos, true, kMilestonePump, kNoBit},             // PUMP
    {game::kJobNone, game::kJobNone, false, kNoBit, kLiftSurveyId},   // LIFT_SURVEY
    {kBenthos, kCormorant, true, kMilestoneLift, kNoBit},             // LIFT
    {kCormorant, kDeepAnchor, true, kMilestoneBeacon, kNoBit},        // BEACON
    {kDeepAnchor, kTidewall, true, kMilestoneHome, kNoBit},           // HOME
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

// Generated ordered-pair reward. Campaign work pays the ordinary route reward
// once; surveys pay nothing.
inline uint8_t missionReward(uint8_t id) {
    MissionSpec spec;
    if (!missionInfo(id, spec) || !spec.cargo)
        return 0;
    uint8_t reward = 0;
    uint16_t unusedDeadline = 0;
    if (!world::routeEconomy(spec.from, spec.to, reward, unusedDeadline))
        return 0;
    return reward;
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
        return hasMilestone(campaign, kMilestoneBeacon);
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

// WORK page rows. Cargo missions are compacted in stable mission-id order at
// their source berth; surveys never occupy a row. After HOME, Tidewall adds a
// replayable VIEW ENDING row. kWorkEnding is outside the frozen mission ids.
constexpr uint8_t kWorkEnding = 0xFE;

inline bool endingAvailable(const State &state) {
    return missionCompleted(state, kHome);
}

inline uint8_t workMissionAt(const State &state, uint8_t berth, uint8_t row) {
    uint8_t visible = 0;
    for (uint8_t id = kFirst; id <= kHome; ++id) {
        MissionSpec spec;
        if (!missionInfo(id, spec) || !spec.cargo || spec.from != berth)
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
    if (!missionInfo(id, spec) || missionCompleted(state, id) || !missionPrerequisites(state, id))
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

inline bool berthAvailable(const State &state, uint8_t berth) {
    return berthAvailable(state.campaign, berth);
}

inline bool freightBerth(uint8_t berth) {
    return berth < world::SETTLEMENT_COUNT;
}

inline void markVisited(State &state, uint8_t berth) {
    if (freightBerth(berth))
        state.visited |= static_cast<uint8_t>(1u << berth);
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
#if SH_FEATURE_STATIONS
    if (state.docked < 0)
        return false;
#endif
    if (game::hasActiveJob(state))
        return false;
    MissionSpec spec;
    if (!missionInfo(id, spec) || !spec.cargo)
        return false;
#if SH_FEATURE_STATIONS
    if (spec.from != static_cast<uint8_t>(state.docked))
        return false;
#endif
    return missionAvailable(state, id);
}

inline bool accept(State &state, uint8_t id) {
    if (!canAccept(state, id))
        return false;
    MissionSpec spec;
    missionInfo(id, spec);
    const uint8_t reward = missionReward(id);
    if (reward == 0)
        return false;

    game::setCargo(state, 0);
    state.activeJob.kind = game::kJobKindCampaign;
    state.activeJob.mission = id;
    state.activeJob.from = spec.from;
    state.activeJob.to = spec.to;
    state.activeJob.crates = game::kJobCrates;
    state.activeJob.reward = reward;
    state.lastDelivery = kDeliveryNone;
    game::setCargo(state, game::kJobCrates);
    state.ballast = static_cast<uint8_t>(game::neutralForNormalized(state.cargo));
    return true;
}

// Save-schema invariants shared with the storage task. These are structural;
// they do not require a docked boat (an active checkpoint can be saved at a
// service berth).
inline bool milestonesValid(uint8_t mask) {
    if ((mask & ~kMilestoneMask) != 0)
        return false;
    if ((mask & kMilestoneHome) && !(mask & kMilestoneBeacon))
        return false;
    if ((mask & kMilestoneBeacon) && (mask & (kMilestonePump | kMilestoneLift)) != (kMilestonePump | kMilestoneLift))
        return false;
    return true;
}

inline bool surveysValid(uint8_t mask) {
    return (mask & ~kSurveyMask) == 0;
}

inline bool activeJobValid(const game::ActiveJob &job, uint8_t campaign, uint8_t surveys, uint8_t tools) {
    if (job.kind == game::kJobKindNone) {
        return job.mission == kMissionNone && job.from == game::kJobNone && job.to == game::kJobNone && job.crates == 0 && job.reward == 0;
    }
    if (job.crates != game::kJobCrates || job.to >= world::SETTLEMENT_COUNT)
        return false;

    if (job.kind == game::kJobKindCampaign) {
        MissionSpec spec;
        if (!missionInfo(job.mission, spec) || !spec.cargo)
            return false;
        return job.from == spec.from && job.to == spec.to && job.reward == missionReward(job.mission) && !missionCompleted(campaign, surveys, job.mission) &&
               missionPrerequisites(campaign, surveys, tools, job.mission);
    }

    if (job.kind != game::kJobKindOrdinary)
        return false;
    if (job.mission != kMissionNone || job.from >= world::SETTLEMENT_COUNT || job.from == job.to)
        return false;
    uint8_t routeReward = 0;
    uint16_t ignoredDeadline = 0;
    if (!world::routeEconomy(job.from, job.to, routeReward, ignoredDeadline))
        return false;
    // Exact equality also rejects every malformed reward.
    return job.reward == routeReward;
}

inline bool activeJobValid(const State &state) {
    return activeJobValid(state.activeJob, state.campaign, state.surveys, state.tools);
}

inline bool checkpointValid(uint8_t campaign, uint8_t surveys, uint8_t visited, uint8_t ending, const game::ActiveJob &job, uint8_t tools) {
    if ((visited & ~static_cast<uint8_t>((1u << world::SETTLEMENT_COUNT) - 1u)) != 0)
        return false;
    if (ending > 1 || (ending != 0 && !hasMilestone(campaign, kMilestoneHome)))
        return false;
    return milestonesValid(campaign) && surveysValid(surveys) && activeJobValid(job, campaign, surveys, tools);
}

inline bool stateValid(const State &state) {
    return checkpointValid(state.campaign, state.surveys, state.visited, state.ending, state.activeJob, state.tools);
}

}   // namespace campaign

#endif   // SH_FEATURE_CAMPAIGN

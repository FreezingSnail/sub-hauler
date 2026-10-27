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

struct MissionSpec {
    uint8_t from;
    uint8_t to;
    bool cargo;
};

inline bool missionInfo(uint8_t id, MissionSpec &out) {
    switch (id) {
    case kFirst:
        out = MissionSpec{kTidewall, kBenthos, true};
        return true;
    case kPumpSurvey:
        out = MissionSpec{game::kJobNone, game::kJobNone, false};
        return true;
    case kPump:
        out = MissionSpec{kCormorant, kBenthos, true};
        return true;
    case kLiftSurvey:
        out = MissionSpec{game::kJobNone, game::kJobNone, false};
        return true;
    case kLift:
        out = MissionSpec{kBenthos, kCormorant, true};
        return true;
    case kBeacon:
        out = MissionSpec{kCormorant, kDeepAnchor, true};
        return true;
    case kHome:
        out = MissionSpec{kDeepAnchor, kTidewall, true};
        return true;
    default:
        return false;
    }
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
    uint16_t deadline = 0;
    if (!world::routeEconomy(spec.from, spec.to, reward, deadline))
        return 0;
    return reward;
}

inline bool hasMilestone(const State &state, uint8_t bit) {
    return (state.campaign & bit) != 0;
}

inline bool surveyRecorded(const State &state, uint8_t surveyId) {
    if (surveyId > kLiftSurveyId)
        return false;
    return (state.surveys & static_cast<uint8_t>(1u << surveyId)) != 0;
}

inline bool missionCompleted(const State &state, uint8_t id) {
    switch (id) {
    case kFirst:
        return hasMilestone(state, kMilestoneFirst);
    case kPumpSurvey:
        return surveyRecorded(state, kPumpSurveyId);
    case kPump:
        return hasMilestone(state, kMilestonePump);
    case kLiftSurvey:
        return surveyRecorded(state, kLiftSurveyId);
    case kLift:
        return hasMilestone(state, kMilestoneLift);
    case kBeacon:
        return hasMilestone(state, kMilestoneBeacon);
    case kHome:
        return hasMilestone(state, kMilestoneHome);
    default:
        return false;
    }
}

// Prerequisites without the "not already complete" test. Completion flags are
// deliberately independent of cargo; surveys can be recorded in any order.
inline bool missionPrerequisites(const State &state, uint8_t id) {
    switch (id) {
    case kFirst:
        return true;
    case kPumpSurvey:
    case kLiftSurvey:
        return hasMilestone(state, kMilestoneFirst);
    case kPump:
        return hasMilestone(state, kMilestoneFirst) && surveyRecorded(state, kPumpSurveyId);
    case kLift:
        return hasMilestone(state, kMilestoneFirst) && surveyRecorded(state, kLiftSurveyId);
    case kBeacon:
        return hasMilestone(state, kMilestonePump) && hasMilestone(state, kMilestoneLift) && sealOwned(state);
    case kHome:
        return hasMilestone(state, kMilestoneBeacon);
    default:
        return false;
    }
}

inline bool missionAvailable(const State &state, uint8_t id) {
    MissionSpec spec;
    return missionInfo(id, spec) && !missionCompleted(state, id) && missionPrerequisites(state, id);
}

// First-slice WORK page rows. The lift/beacon/home sequence stays behind its
// own integration task, so these two missions are the only campaign work the
// station page exposes now. Rows are compacted in stable mission-id order.
inline uint8_t workMissionAt(const State &state, uint8_t berth, uint8_t row) {
    const uint8_t ids[2] = {kFirst, kPump};
    uint8_t visible = 0;
    for (uint8_t i = 0; i < 2; ++i) {
        MissionSpec spec;
        if (!missionInfo(ids[i], spec) || !spec.cargo || spec.from != berth)
            continue;
        if (!missionAvailable(state, ids[i]))
            continue;
        if (visible == row)
            return ids[i];
        ++visible;
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
    switch (id) {
    case kFirst:
        state.campaign |= kMilestoneFirst;
        break;
    case kPump:
        state.campaign |= kMilestonePump;
        break;
    case kLift:
        state.campaign |= kMilestoneLift;
        break;
    case kBeacon:
        state.campaign |= kMilestoneBeacon;
        break;
    case kHome:
        state.campaign |= kMilestoneHome;
        break;
    default:
        return false;
    }
    return true;
}

inline bool berthAvailable(const State &state, uint8_t berth) {
    if (berth < world::SETTLEMENT_COUNT)
        return true;
    world::Berth info;
    if (!world::berthInfo(berth, info) || info.milestone == 0)
        return false;
    return hasMilestone(state, info.milestone);
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
    state.activeJob.deadline = 0;
    state.activeJob.elapsed = 0;
    state.lastDelivery = kDeliveryNone;
    game::setCargo(state, game::kJobCrates);
    state.ballast = static_cast<uint8_t>(game::neutralFor(state.cargo));
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

inline bool routeMatches(uint8_t from, uint8_t to, uint8_t reward, uint16_t deadline) {
    uint8_t expectedReward = 0;
    uint16_t expectedDeadline = 0;
    if (!world::routeEconomy(from, to, expectedReward, expectedDeadline))
        return false;
    return reward == expectedReward && deadline == expectedDeadline;
}

inline bool activeJobValid(const State &state) {
    const game::ActiveJob &job = state.activeJob;
    if (job.kind == game::kJobKindNone) {
        return job.mission == kMissionNone && job.from == game::kJobNone && job.to == game::kJobNone && job.crates == 0 && job.reward == 0 && job.deadline == 0 && job.elapsed == 0;
    }
    if (job.crates != game::kJobCrates || job.to >= world::SETTLEMENT_COUNT)
        return false;

    if (job.kind == game::kJobKindCampaign) {
        MissionSpec spec;
        if (!missionInfo(job.mission, spec) || !spec.cargo)
            return false;
        return job.from == spec.from && job.to == spec.to && job.reward == missionReward(job.mission) && job.deadline == 0 && job.elapsed == 0 && !missionCompleted(state, job.mission) &&
               missionPrerequisites(state, job.mission);
    }

    if (job.kind != game::kJobKindOrdinary && job.kind != game::kJobKindExpress)
        return false;
    if (job.mission != kMissionNone || job.from >= world::SETTLEMENT_COUNT || job.from == job.to)
        return false;
    uint8_t routeReward = 0;
    uint16_t routeDeadline = 0;
    if (!world::routeEconomy(job.from, job.to, routeReward, routeDeadline))
        return false;
    if (job.reward < 5 || job.reward > 250 || (job.reward % 5) != 0)
        return false;
    if (job.kind == game::kJobKindOrdinary)
        return job.reward == routeReward && job.deadline == 0 && job.elapsed == 0;
    return job.reward == game::expressReward(routeReward) && job.deadline == routeDeadline;
}

inline bool stateValid(const State &state) {
    if ((state.visited & ~static_cast<uint8_t>((1u << world::SETTLEMENT_COUNT) - 1u)) != 0)
        return false;
    if (state.ending > 1 || (state.ending != 0 && !hasMilestone(state, kMilestoneHome)))
        return false;
    return milestonesValid(state.campaign) && surveysValid(state.surveys) && activeJobValid(state);
}

}   // namespace campaign

#endif   // SH_FEATURE_CAMPAIGN

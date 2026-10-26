#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_CAMPAIGN

#include "../src/campaign.hpp"
#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/world.hpp"
#include "jobs_test.hpp"
#include "test.hpp"
#include "world_test.hpp"

// Campaign rules: stable IDs, both repair orders, surveys discovered early,
// early equipment, wrong-berth stops, short cargo, one-time rewards, service
// berth eligibility, express timing and deterministic cursor restoration.
namespace campaign_test {

using Fixture = world_test::FakeWorld;
using jobs_test::dockAt;
using jobs_test::idle;
using jobs_test::otherSettlement;
using jobs_test::travelTo;

inline bool at(game::State &state, uint8_t berth) {
    return dockAt(state, berth);
}

inline bool acceptAt(game::State &state, uint8_t berth, uint8_t mission) {
    return at(state, berth) && campaign::accept(state, mission);
}

inline bool sameOffers(const game::State &a, const game::State &b) {
    if (a.offerCount != b.offerCount)
        return false;
    for (uint8_t i = 0; i < a.offerCount; ++i) {
        if (a.offers[i].from != b.offers[i].from || a.offers[i].to != b.offers[i].to || a.offers[i].crates != b.offers[i].crates || a.offers[i].reward != b.offers[i].reward ||
            a.offers[i].kind != b.offers[i].kind || a.offers[i].deadline != b.offers[i].deadline)
            return false;
    }
    return true;
}

// Bring a fresh state through FIRST so branch tests start from the accepted
// design checkpoint rather than hand-set milestone bits.
inline bool completeFirst(game::State &state) {
    if (!acceptAt(state, campaign::kTidewall, campaign::kFirst))
        return false;
    return travelTo(state, campaign::kBenthos) && campaign::missionCompleted(state, campaign::kFirst) && state.credits == campaign::missionReward(campaign::kFirst);
}

inline bool completePump(game::State &state) {
    if (campaign::surveyRecorded(state, campaign::kPumpSurveyId))
        return true;
    return campaign::recordSurvey(state, campaign::kPumpSurveyId) && acceptAt(state, campaign::kCormorant, campaign::kPump) && travelTo(state, campaign::kBenthos) &&
           campaign::missionCompleted(state, campaign::kPump);
}

inline bool completeLift(game::State &state) {
    if (campaign::surveyRecorded(state, campaign::kLiftSurveyId))
        return true;
    return campaign::recordSurvey(state, campaign::kLiftSurveyId) && acceptAt(state, campaign::kBenthos, campaign::kLift) && travelTo(state, campaign::kCormorant) &&
           campaign::missionCompleted(state, campaign::kLift);
}

}   // namespace campaign_test

inline void CampaignSuite(TestRunner &runner) {
    using namespace campaign_test;

    {
        Test test("campaign ids and generated rewards match the frozen table");
        test.assertEqual(campaign::kFirst, 0, "FIRST id");
        test.assertEqual(campaign::kPumpSurvey, 1, "PUMP_SURVEY id");
        test.assertEqual(campaign::kPump, 2, "PUMP id");
        test.assertEqual(campaign::kLiftSurvey, 3, "LIFT_SURVEY id");
        test.assertEqual(campaign::kLift, 4, "LIFT id");
        test.assertEqual(campaign::kBeacon, 5, "BEACON id");
        test.assertEqual(campaign::kHome, 6, "HOME id");
        test.assertEqual(campaign::kTidewall, 0, "Tidewall berth");
        test.assertEqual(campaign::kBenthos, 1, "Benthos berth");
        test.assertEqual(campaign::kCormorant, 2, "Cormorant berth");
        test.assertEqual(campaign::kDeepAnchor, 3, "Deep Anchor berth");
        test.assertEqual(campaign::kPumpRefuge, 4, "Pump Refuge berth");
        test.assertEqual(campaign::kLowerGantry, 5, "Lower Gantry berth");

        test.assertEqual(campaign::missionReward(campaign::kFirst), 145, "FIRST pays 145");
        test.assertEqual(campaign::missionReward(campaign::kPump), 85, "PUMP pays 85");
        test.assertEqual(campaign::missionReward(campaign::kLift), 85, "LIFT pays 85");
        test.assertEqual(campaign::missionReward(campaign::kBeacon), 155, "BEACON pays 155");
        test.assertEqual(campaign::missionReward(campaign::kHome), 120, "HOME pays 120");
        test.assertEqual(campaign::missionReward(campaign::kFirst) + campaign::missionReward(campaign::kPump) + campaign::missionReward(campaign::kLift) + campaign::missionReward(campaign::kBeacon) +
                             campaign::missionReward(campaign::kHome),
                         590, "campaign total is 590 credits");
        test.assertEqual(campaign::missionReward(campaign::kPumpSurvey), 0, "surveys pay nothing");
        test.assertEqual(campaign::missionReward(campaign::kLiftSurvey), 0, "surveys pay nothing");

        campaign::MissionSpec spec;
        test.assertTrue(campaign::missionInfo(campaign::kFirst, spec), "FIRST spec exists");
        test.assertEqual(spec.from, campaign::kTidewall, "FIRST loads at Tidewall");
        test.assertEqual(spec.to, campaign::kBenthos, "FIRST delivers at Benthos");
        test.assertTrue(campaign::isCargoMission(campaign::kFirst), "FIRST is cargo");
        test.assertTrue(!campaign::isCargoMission(campaign::kPumpSurvey), "PUMP_SURVEY is not cargo");
        test.assertTrue(!campaign::missionInfo(7, spec), "unknown mission id rejected");
        runner.add(test);
    }

    {
        Test test("reset and survey facts: recorded early, independent of cargo, never repeated");
        game::State state;
        game::reset(state);
        test.assertEqual(state.activeJob.kind, game::kJobKindNone, "reset leaves no job kind");
        test.assertEqual(state.activeJob.mission, game::kMissionNone, "reset leaves no mission");
        test.assertEqual(state.activeJob.from, game::kJobNone, "reset leaves no job origin");
        test.assertEqual(state.offerCount, 0, "reset generates no offers");
        test.assertEqual(state.campaign, 0, "reset clears milestones");
        test.assertEqual(state.surveys, 0, "reset clears surveys");

        test.assertTrue(campaign::recordSurvey(state, campaign::kPumpSurveyId), "pump survey records before FIRST");
        test.assertTrue(campaign::recordSurvey(state, campaign::kLiftSurveyId), "lift survey records before FIRST");
        test.assertTrue(campaign::surveyRecorded(state, campaign::kPumpSurveyId), "pump fact retained");
        test.assertTrue(campaign::surveyRecorded(state, campaign::kLiftSurveyId), "lift fact retained");
        test.assertTrue(campaign::recordSurvey(state, campaign::kPumpSurveyId), "repeat survey is idempotent");
        test.assertTrue(!game::hasActiveJob(state), "recordSurvey does not occupy the cargo slot");
        test.assertEqual(state.cargo, 0, "recordSurvey does not load cargo");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kPump), "PUMP still needs FIRST");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kBeacon), "BEACON needs both repairs and the seal");

        test.assertTrue(completeFirst(state), "FIRST completes after early surveys");
        test.assertTrue(campaign::missionAvailable(state, campaign::kPump), "pre-discovered pump survey satisfies the request");
        test.assertTrue(campaign::missionAvailable(state, campaign::kLift), "pre-discovered lift survey satisfies the request");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kPumpSurvey), "completed survey does not reappear");
        test.assertTrue(!campaign::recordSurvey(state, 2), "unknown survey id rejected");
        runner.add(test);
    }

    {
        Test test("FIRST: explicit acceptance, wrong-berth stop, short-cargo rejection, one-time pay");
        game::State state;
        game::reset(state);
        test.assertTrue(at(state, campaign::kTidewall), "dock at Tidewall");
        test.assertTrue(campaign::missionAvailable(state, campaign::kFirst), "FIRST is available immediately");
        test.assertTrue(!game::hasActiveJob(state), "the first board selects but does not auto-accept FIRST");
        test.assertTrue(!campaign::accept(state, campaign::kPump), "PUMP cannot be accepted out of sequence");
        test.assertEqual(state.credits, 0, "docking pays nothing");

        test.assertTrue(campaign::accept(state, campaign::kFirst), "FIRST accepted at Tidewall");
        test.assertEqual(state.activeJob.kind, game::kJobKindCampaign, "active kind is campaign");
        test.assertEqual(state.activeJob.mission, campaign::kFirst, "active mission is FIRST");
        test.assertEqual(state.activeJob.from, campaign::kTidewall, "FIRST origin");
        test.assertEqual(state.activeJob.to, campaign::kBenthos, "FIRST destination");
        test.assertEqual(state.activeJob.crates, game::kJobCrates, "four-unit load");
        test.assertEqual(state.activeJob.reward, 145, "reward fixed at acceptance");
        test.assertEqual(state.activeJob.deadline, 0, "campaign work is untimed");
        test.assertEqual(state.activeJob.elapsed, 0, "campaign age stays zero");
        test.assertEqual(state.cargo, 4, "acceptance auto-loads four units");
        test.assertEqual(state.docked, static_cast<int8_t>(campaign::kTidewall), "acceptance stays docked");
        test.assertTrue(!campaign::accept(state, campaign::kFirst), "only one load slot");
        test.assertTrue(campaign::activeJobValid(state), "accepted campaign job passes save invariants");
        test.assertTrue(campaign::stateValid(state), "campaign state invariants hold");

        test.assertTrue(travelTo(state, campaign::kCormorant), "stop at the wrong berth");
        test.assertTrue(game::hasActiveJob(state), "wrong berth retains the campaign load");
        test.assertEqual(state.cargo, 4, "wrong berth retains four units");
        test.assertEqual(state.credits, 0, "wrong berth pays nothing");

        test.assertTrue(travelTo(state, campaign::kBenthos), "arrive at Benthos loaded");
        test.assertEqual(state.credits, 145, "FIRST pays once");
        test.assertEqual(state.campaign & campaign::kMilestoneFirst, campaign::kMilestoneFirst, "FIRST milestone set");
        test.assertEqual(state.delivered, 1, "delivered count increments");
        test.assertTrue(!game::hasActiveJob(state), "delivery clears the campaign job");
        test.assertEqual(state.cargo, 0, "delivery clears the hold");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryPaid, "delivery records paid");
        test.assertTrue(campaign::missionCompleted(state, campaign::kFirst), "FIRST completion flag");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kFirst), "completed FIRST never reappears");

        test.assertTrue(station::undock(state), "undock after delivery");
        test.assertTrue(at(state, campaign::kBenthos), "dock again");
        test.assertEqual(state.credits, 145, "repeated docking never pays twice");
        test.assertEqual(state.delivered, 1, "repeated docking does not double-count");

        // Short cargo cannot set the milestone or pay, but the free source can
        // reissue the mission after the failed load is cleared.
        game::State shortState;
        game::reset(shortState);
        test.assertTrue(acceptAt(shortState, campaign::kTidewall, campaign::kFirst), "short-cargo FIRST accepted");
        shortState.cargo = 3;
        test.assertTrue(travelTo(shortState, campaign::kBenthos), "short load docks at Benthos");
        test.assertEqual(shortState.credits, 0, "short cargo pays nothing");
        test.assertEqual(shortState.campaign & campaign::kMilestoneFirst, 0, "short cargo sets no milestone");
        test.assertEqual(shortState.lastDelivery, jobs::kDeliveryFailed, "short cargo records failure");
        test.assertTrue(!game::hasActiveJob(shortState), "short cargo clears the job");
        test.assertTrue(campaign::missionAvailable(shortState, campaign::kFirst), "source can reissue after short load");
        test.assertTrue(at(shortState, campaign::kTidewall), "return to source");
        test.assertTrue(campaign::accept(shortState, campaign::kFirst), "source reissues FIRST for free");

        test.assertTrue(jobs::abandon(shortState), "campaign load can be abandoned");
        test.assertEqual(shortState.cargo, 0, "abandon clears the load");
        test.assertTrue(campaign::missionAvailable(shortState, campaign::kFirst), "abandon leaves the mission available");
        test.assertTrue(campaign::accept(shortState, campaign::kFirst), "abandoned mission reissues");
        runner.add(test);
    }

    {
        Test test("pump branch first: surveys early, both service berths, beacon then HOME");
        game::State state;
        game::reset(state);
        state.tools = exploration_data::CUTTER | exploration_data::SEAL;
        test.assertTrue(campaign::recordSurvey(state, campaign::kPumpSurveyId), "pump survey found early");
        test.assertTrue(campaign::recordSurvey(state, campaign::kLiftSurveyId), "lift survey found early");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kBeacon), "early seal alone cannot start BEACON");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kHome), "HOME needs BEACON");

        test.assertTrue(completeFirst(state), "FIRST pays 145");
        test.assertTrue(!campaign::berthAvailable(state, campaign::kPumpRefuge), "Pump Refuge inactive before repair");
        test.assertTrue(!campaign::berthAvailable(state, campaign::kLowerGantry), "Lower Gantry inactive before repair");
        test.assertTrue(campaign::berthAvailable(state, campaign::kTidewall), "existing berths remain available");
        test.assertTrue(!campaign::berthAvailable(state, 6), "no phantom berth id");

        test.assertTrue(acceptAt(state, campaign::kCormorant, campaign::kPump), "PUMP accepted at Cormorant");
        test.assertTrue(travelTo(state, campaign::kBenthos), "impeller delivered to Benthos");
        test.assertEqual(state.credits, 145 + 85, "PUMP pays 85 once");
        test.assertTrue(campaign::missionCompleted(state, campaign::kPump), "PUMP milestone set");
        test.assertTrue(campaign::berthAvailable(state, campaign::kPumpRefuge), "Pump Refuge opens after PUMP");
        test.assertTrue(!campaign::berthAvailable(state, campaign::kLowerGantry), "Lower Gantry still closed");

        test.assertTrue(acceptAt(state, campaign::kBenthos, campaign::kLift), "LIFT accepted at Benthos");
        test.assertTrue(travelTo(state, campaign::kCormorant), "cable reels delivered to Cormorant");
        test.assertEqual(state.credits, 145 + 85 + 85, "LIFT pays 85 once");
        test.assertTrue(campaign::missionCompleted(state, campaign::kLift), "LIFT milestone set");
        test.assertTrue(campaign::berthAvailable(state, campaign::kLowerGantry), "Lower Gantry opens after LIFT");

        test.assertTrue(acceptAt(state, campaign::kCormorant, campaign::kBeacon), "BEACON explicitly accepted");
        test.assertTrue(travelTo(state, campaign::kDeepAnchor), "beacon core delivered");
        test.assertEqual(state.credits, 145 + 85 + 85 + 155, "BEACON pays 155 once");
        test.assertTrue(campaign::missionCompleted(state, campaign::kBeacon), "BEACON milestone set");
        test.assertTrue(!game::hasActiveJob(state), "BEACON does not auto-load HOME");
        test.assertEqual(state.cargo, 0, "Deep Anchor docking leaves the hold empty");
        test.assertTrue(campaign::missionAvailable(state, campaign::kHome), "HOME offered after BEACON");
        test.assertTrue(!campaign::missionCompleted(state, campaign::kHome), "HOME is not auto-complete");

        test.assertTrue(acceptAt(state, campaign::kDeepAnchor, campaign::kHome), "HOME explicitly accepted");
        test.assertTrue(travelTo(state, campaign::kTidewall), "dispatch delivered home");
        test.assertEqual(state.credits, 590, "five campaign payments total 590");
        test.assertEqual(state.delivered, 5, "five deliveries counted");
        test.assertTrue(campaign::missionCompleted(state, campaign::kHome), "HOME milestone set");
        test.assertTrue(campaign::stateValid(state), "completed campaign state remains valid");
        test.assertTrue(campaign::markEndingPresented(state), "ending can be marked after HOME");
        test.assertEqual(state.ending, 1, "ending flag set");

        test.assertTrue(station::undock(state), "undock after ending");
        test.assertTrue(at(state, campaign::kTidewall), "redock at Tidewall");
        test.assertEqual(state.credits, 590, "HOME never pays twice");
        runner.add(test);
    }

    {
        Test test("lift branch first is equally valid and reaches the same total");
        game::State state;
        game::reset(state);
        test.assertTrue(campaign::recordSurvey(state, campaign::kLiftSurveyId), "lift survey found first");
        test.assertTrue(campaign::recordSurvey(state, campaign::kPumpSurveyId), "pump survey found second");
        test.assertTrue(completeFirst(state), "FIRST pays 145");

        test.assertTrue(acceptAt(state, campaign::kBenthos, campaign::kLift), "LIFT accepted first");
        test.assertTrue(travelTo(state, campaign::kCormorant), "LIFT delivered first");
        test.assertEqual(state.credits, 145 + 85, "first repair pays 85");
        test.assertTrue(campaign::berthAvailable(state, campaign::kLowerGantry), "Lower Gantry opens");

        test.assertTrue(acceptAt(state, campaign::kCormorant, campaign::kPump), "PUMP accepted second");
        test.assertTrue(travelTo(state, campaign::kBenthos), "PUMP delivered second");
        test.assertEqual(state.credits, 145 + 85 + 85, "second repair pays 85");
        test.assertTrue(campaign::berthAvailable(state, campaign::kPumpRefuge), "Pump Refuge opens");

        state.tools |= exploration_data::SEAL;
        test.assertTrue(acceptAt(state, campaign::kCormorant, campaign::kBeacon), "BEACON after both orders");
        test.assertTrue(travelTo(state, campaign::kDeepAnchor), "BEACON delivered");
        test.assertTrue(acceptAt(state, campaign::kDeepAnchor, campaign::kHome), "HOME accepted");
        test.assertTrue(travelTo(state, campaign::kTidewall), "HOME delivered");
        test.assertEqual(state.credits, 590, "reversed repair branches reach 590");
        test.assertEqual(state.delivered, 5, "five deliveries in reversed order");
        test.assertTrue(campaign::stateValid(state), "reversed-order state is valid");
        runner.add(test);
    }

    {
        Test test("ordinary work is untimed; express pays 125% and expires on docking");
        game::State state;
        game::reset(state);
        test.assertTrue(at(state, campaign::kTidewall), "dock at Tidewall");
        test.assertEqual(state.offerCount, 2, "two freight rows");
        test.assertEqual(state.offers[0].kind, game::kJobKindOrdinary, "row 0 is ordinary");
        test.assertEqual(state.offers[0].deadline, 0, "ordinary offer has no deadline");
        test.assertEqual(state.offers[1].kind, game::kJobKindExpress, "row 1 is express");
        test.assertTrue(state.offers[1].deadline > 0, "express advertises its deadline");
        const jobs::RouteEconomy expressRoute = jobs::routeEconomy(state.offers[1].from, state.offers[1].to);
        test.assertEqual(state.offers[1].reward, game::expressReward(expressRoute.reward), "express reward formula");
        test.assertEqual(state.offers[1].reward, ((expressRoute.reward * 5 + 10) / 20) * 5, "express formula is frozen");

        test.assertTrue(jobs::accept(state, 0), "accept ordinary work");
        test.assertEqual(state.activeJob.kind, game::kJobKindOrdinary, "ordinary kind stored");
        test.assertEqual(state.activeJob.deadline, 0, "ordinary active job is untimed");
        const uint8_t ordinaryDestination = state.activeJob.to;
        const uint16_t ordinaryReward = state.activeJob.reward;
        test.assertTrue(station::undock(state), "ordinary work undocks");
        for (uint16_t i = 0; i < 600; ++i)
            game::step(state, idle());
        test.assertEqual(state.activeJob.elapsed, 0, "ordinary age never advances");
        test.assertEqual(jobs::remainingTicks(state), 0, "ordinary has no remaining-time clock");
        test.assertEqual(state.voyageTicks, 600, "voyage clock advances in flight");
        test.assertTrue(travelTo(state, ordinaryDestination), "ordinary arrives after a long flight");
        test.assertEqual(state.credits, ordinaryReward, "ordinary pays despite elapsed flight time");
        test.assertEqual(state.delivered, 1, "ordinary delivery counted");

        // Dock at the destination has refreshed a full board. Express is row 1.
        test.assertTrue(jobs::accept(state, 1), "accept express work");
        test.assertEqual(state.activeJob.kind, game::kJobKindExpress, "express kind stored");
        const uint8_t expressDestination = state.activeJob.to;
        const uint16_t expressReward = state.activeJob.reward;
        const uint16_t expressDeadline = state.activeJob.deadline;
        const uint16_t creditsBefore = state.credits;
        uint8_t wrong = game::kJobNone;
        test.assertTrue(otherSettlement(static_cast<uint8_t>(state.docked), expressDestination, wrong), "a wrong berth exists");
        state.activeJob.elapsed = static_cast<uint16_t>(expressDeadline + 1);
        test.assertTrue(travelTo(state, wrong), "overdue express docks at the wrong berth");
        test.assertEqual(state.credits, creditsBefore, "expired express pays nothing");
        test.assertEqual(state.cargo, 0, "expired express clears the load");
        test.assertTrue(!game::hasActiveJob(state), "expired express clears the job");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryFailed, "expired express records the failure");

        // Exact-deadline arrival at the destination still pays.
        test.assertTrue(at(state, campaign::kTidewall), "return to Tidewall for a fresh board");
        test.assertTrue(jobs::accept(state, 1), "accept a fresh express");
        const uint8_t exactDestination = state.activeJob.to;
        const uint16_t exactReward = state.activeJob.reward;
        state.activeJob.elapsed = state.activeJob.deadline;
        const uint16_t exactBefore = state.credits;
        test.assertTrue(travelTo(state, exactDestination), "exact-deadline express docks");
        test.assertEqual(state.credits, exactBefore + exactReward, "exact deadline is inclusive");

        // Saturation is a permanent overdue state, not a wrap.
        test.assertTrue(at(state, campaign::kTidewall), "dock for saturation test");
        test.assertTrue(jobs::accept(state, 1), "accept saturating express");
        test.assertTrue(station::undock(state), "undock saturation express");
        state.activeJob.elapsed = static_cast<uint16_t>(game::kJobElapsedLate - 1);
        game::step(state, idle());
        test.assertEqual(state.activeJob.elapsed, game::kJobElapsedLate, "express age reaches the late marker");
        game::step(state, idle());
        test.assertEqual(state.activeJob.elapsed, game::kJobElapsedLate, "express age saturates");
        uint8_t saturationWrong = game::kJobNone;
        test.assertTrue(otherSettlement(static_cast<uint8_t>(0), state.activeJob.to, saturationWrong), "saturation wrong berth exists");
        test.assertTrue(travelTo(state, saturationWrong), "saturated express docks elsewhere");
        test.assertEqual(state.credits, exactBefore + exactReward, "saturated express never pays");
        test.assertTrue(!game::hasActiveJob(state), "saturated express is cleared");
        runner.add(test);
    }

    {
        Test test("deterministic offers restore from the saved cursor and respect the board seed");
        game::State a;
        game::State b;
        game::reset(a);
        game::reset(b);
        test.assertTrue(at(a, campaign::kTidewall), "first state docks");
        test.assertTrue(at(b, campaign::kTidewall), "second state docks");
        test.assertTrue(sameOffers(a, b), "identical history produces identical rows");
        const uint16_t savedCursor = a.offerCursor;
        a.offerSerial = savedCursor;
        jobs::refreshOffers(a, campaign::kTidewall);
        test.assertTrue(sameOffers(a, b), "restoring from offerCursor reproduces the board");
        test.assertEqual(a.offerCursor, savedCursor, "restoration does not advance the saved cursor");

        game::State seededA;
        game::State seededB;
        game::reset(seededA);
        game::reset(seededB);
        seededA.boardSeed = 7;
        seededA.offerSerial = seededA.boardSeed;
        seededB.boardSeed = 7;
        seededB.offerSerial = seededB.boardSeed;
        test.assertTrue(at(seededA, campaign::kTidewall), "seeded state docks");
        test.assertTrue(at(seededB, campaign::kTidewall), "second seeded state docks");
        test.assertTrue(sameOffers(seededA, seededB), "same board seed is deterministic");
        game::State seedZero;
        game::reset(seedZero);
        test.assertTrue(at(seedZero, campaign::kTidewall), "zero-seed state docks");
        test.assertEqual(seededA.offerCursor, 7, "board seed 7 is the saved cursor origin");
        test.assertEqual(seedZero.offerCursor, 0, "schema-1 board seed starts at zero");
        bool seedChangesBoard = false;
        for (uint8_t origin = 0; origin < world::settlementCount(); ++origin) {
            game::State seeded;
            game::State zero;
            game::reset(seeded);
            game::reset(zero);
            seeded.boardSeed = 7;
            seeded.offerSerial = seeded.boardSeed;
            jobs::refreshOffers(seeded, origin);
            jobs::refreshOffers(zero, origin);
            seedChangesBoard = seedChangesBoard || !sameOffers(seeded, zero);
        }
        test.assertTrue(seedChangesBoard, "a different board seed changes the offer sequence");
        runner.add(test);
    }

    {
        Test test("save invariants reject malformed campaign and freight jobs");
        game::State state;
        game::reset(state);
        test.assertTrue(campaign::stateValid(state), "fresh state is valid");
        test.assertTrue(!campaign::milestonesValid(campaign::kMilestoneHome), "HOME needs BEACON");
        test.assertTrue(!campaign::milestonesValid(campaign::kMilestoneBeacon), "BEACON needs both repairs");
        test.assertTrue(campaign::milestonesValid(campaign::kMilestoneBeacon | campaign::kMilestonePump | campaign::kMilestoneLift), "BEACON plus both repairs is valid");
        test.assertTrue(!campaign::surveysValid(0x04), "survey mask is two bits");

        game::State saturated;
        game::reset(saturated);
        test.assertTrue(at(saturated, campaign::kTidewall), "dock for credit saturation");
        test.assertTrue(jobs::accept(saturated, 0), "accept ordinary for credit saturation");
        const uint8_t saturatedDestination = saturated.activeJob.to;
        saturated.credits = static_cast<uint16_t>(game::expressReward(0) + 65530u);
        test.assertTrue(travelTo(saturated, saturatedDestination), "deliver at the credit ceiling");
        test.assertEqual(saturated.credits, 0xFFFFu, "credits saturate instead of wrapping");
        test.assertEqual(saturated.delivered, 1, "saturated payment still counts the delivery");

        test.assertTrue(at(state, campaign::kTidewall), "dock for job invariants");
        test.assertTrue(jobs::accept(state, 0), "accept ordinary");
        test.assertTrue(campaign::activeJobValid(state), "ordinary active job is valid");
        state.activeJob.deadline = 1;
        test.assertTrue(!campaign::activeJobValid(state), "ordinary cannot carry a deadline");
        state.activeJob.deadline = 0;
        state.activeJob.kind = game::kJobKindExpress;
        test.assertTrue(!campaign::activeJobValid(state), "express needs its route deadline and reward");
        state.activeJob.kind = 9;
        test.assertTrue(!campaign::activeJobValid(state), "unknown job kind is invalid");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_CAMPAIGN

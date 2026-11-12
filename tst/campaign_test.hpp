#pragma once



#include "../src/campaign.hpp"
#include "../src/exploration.hpp"
#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/work_view.hpp"
#include "../src/world.hpp"
#include "jobs_test.hpp"
#include "test.hpp"
#include "world_test.hpp"

// Campaign rules: stable IDs, both repair orders, surveys discovered early,
// early equipment, wrong-berth stops, short cargo, one-time campaign completion,
// service berth eligibility and explicit acceptance. All work is untimed and
// story deliveries pay nothing.
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

// Bring a fresh state through FIRST so branch tests start from the accepted
// design checkpoint rather than hand-set milestone bits.
inline bool completeFirst(game::State &state) {
    if (!acceptAt(state, campaign::kTidewall, campaign::kFirst))
        return false;
    return travelTo(state, campaign::kBenthos) && campaign::missionCompleted(state, campaign::kFirst);
}

inline bool completePump(game::State &state) {
    if (campaign::missionCompleted(state, campaign::kPump))
        return true;
    return campaign::recordSurvey(state, campaign::kPumpSurveyId) && acceptAt(state, campaign::kCormorant, campaign::kPump) && travelTo(state, campaign::kPumpRefuge) &&
           campaign::missionCompleted(state, campaign::kPump);
}

inline bool completeLift(game::State &state) {
    if (campaign::missionCompleted(state, campaign::kLift))
        return true;
    return campaign::recordSurvey(state, campaign::kLiftSurveyId) && acceptAt(state, campaign::kBenthos, campaign::kLift) && travelTo(state, campaign::kFreightYard) &&
           campaign::missionCompleted(state, campaign::kLift);
}

// Post-BEACON route step. The relay expedition is mandatory for HOME but still
// requires every tool; this helper only advances tests that already established
// the prerequisite checks.
inline bool completeExpedition(game::State &state) {
    if (campaign::missionCompleted(state, campaign::kExpedition))
        return true;
    state.tools |= static_cast<uint8_t>(exploration_data::CUTTER | exploration_data::SEAL | exploration_data::THRUSTER | exploration_data::LENS);
    if (!acceptAt(state, campaign::kDeepAnchor, campaign::kExpedition))
        return false;
    return travelTo(state, campaign::kDeepRelay) && campaign::missionCompleted(state, campaign::kExpedition);
}

}   // namespace campaign_test

inline void CampaignSuite(TestRunner &runner) {
    using namespace campaign_test;

    {
        Test test("mission metadata and campaign job validation cover all byte values");
        bool exact = true;
        const uint8_t bits[] = {campaign::kMilestoneFirst, campaign::kNoBit, campaign::kMilestonePump,
                               campaign::kNoBit, campaign::kMilestoneLift, campaign::kMilestoneBeacon,
                               campaign::kMilestoneHome, campaign::kMilestoneExpedition};
        for (uint16_t id = 0; id < 256; ++id) {
            campaign::MissionSpec spec;
            if (campaign::missionInfo(static_cast<uint8_t>(id), spec))
                exact = exact && spec.milestone == bits[id] && spec.cargo == (bits[id] != campaign::kNoBit);
            else {
                game::State state;
                game::reset(state);
                exact = exact && !campaign::missionAvailable(state, static_cast<uint8_t>(id));
                exact = exact && !campaign::completeMission(state, static_cast<uint8_t>(id));
            }
        }

        test.assertTrue(exact, "fixed milestone table retains its contract");
        runner.add(test);
    }

    {
        Test test("campaign ids and mission table match the frozen design");
        test.assertEqual(campaign::kFirst, 0, "FIRST id");
        test.assertEqual(campaign::kPumpSurvey, 1, "PUMP_SURVEY id");
        test.assertEqual(campaign::kPump, 2, "PUMP id");
        test.assertEqual(campaign::kLiftSurvey, 3, "LIFT_SURVEY id");
        test.assertEqual(campaign::kLift, 4, "LIFT id");
        test.assertEqual(campaign::kBeacon, 5, "BEACON id");
        test.assertEqual(campaign::kHome, 6, "HOME id");
        test.assertEqual(campaign::kExpedition, 7, "Deep Relay expedition id");
        test.assertEqual(campaign::kTidewall, 0, "Tidewall berth");
        test.assertEqual(campaign::kBenthos, 1, "Benthos berth");
        test.assertEqual(campaign::kCormorant, 2, "Cormorant berth");
        test.assertEqual(campaign::kDeepAnchor, 3, "Deep Anchor berth");
        test.assertEqual(campaign::kPumpRefuge, 4, "Pump Refuge berth");
        test.assertEqual(campaign::kLowerGantry, 5, "Lower Gantry berth");
        test.assertEqual(campaign::kFreightYard, 6, "Freight Yard lift work berth");

        campaign::MissionSpec spec;
        test.assertTrue(campaign::missionInfo(campaign::kFirst, spec), "FIRST spec exists");
        test.assertEqual(spec.from, campaign::kTidewall, "FIRST loads at Tidewall");
        test.assertEqual(spec.to, campaign::kBenthos, "FIRST delivers at Benthos");
        test.assertTrue(campaign::isCargoMission(campaign::kFirst), "FIRST is cargo");
        test.assertTrue(!campaign::isCargoMission(campaign::kPumpSurvey), "PUMP_SURVEY is not cargo");
        test.assertTrue(!campaign::missionInfo(8, spec), "unknown mission id rejected");
        runner.add(test);
    }

    {
        Test test("reset and survey facts: recorded early, independent of cargo, never repeated");
        game::State state;
        game::reset(state);
        test.assertEqual(state.activeJob.kind, game::kJobKindNone, "reset leaves no job kind");
        test.assertEqual(state.activeJob.mission, game::kMissionNone, "reset leaves no mission");
        test.assertEqual(state.activeJob.from, game::kJobNone, "reset leaves no job origin");
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
        Test test("FIRST: explicit acceptance, wrong-berth stop, short-cargo rejection, one-time completion");
        game::State state;
        game::reset(state);
        test.assertTrue(at(state, campaign::kTidewall), "dock at Tidewall");
        test.assertTrue(campaign::missionAvailable(state, campaign::kFirst), "FIRST is available immediately");
        test.assertTrue(!game::hasActiveJob(state), "the first board selects but does not auto-accept FIRST");
        test.assertTrue(!campaign::accept(state, campaign::kPump), "PUMP cannot be accepted out of sequence");

        test.assertTrue(campaign::accept(state, campaign::kFirst), "FIRST accepted at Tidewall");
        test.assertEqual(state.activeJob.kind, game::kJobKindCampaign, "active kind is campaign");
        test.assertEqual(state.activeJob.mission, campaign::kFirst, "active mission is FIRST");
        test.assertEqual(state.activeJob.from, campaign::kTidewall, "FIRST origin");
        test.assertEqual(state.activeJob.to, campaign::kBenthos, "FIRST destination");
        test.assertEqual(state.activeJob.crates, game::kJobCrates, "four-unit load");
        test.assertEqual(state.cargo, 4, "acceptance auto-loads four units");
        test.assertEqual(state.docked, static_cast<int8_t>(campaign::kTidewall), "acceptance stays docked");
        test.assertTrue(!campaign::accept(state, campaign::kFirst), "only one load slot");
        test.assertTrue(campaign::cargoJobMatches(state.activeJob), "accepted campaign job matches its mission metadata");

        test.assertTrue(travelTo(state, campaign::kCormorant), "stop at the wrong berth");
        test.assertTrue(game::hasActiveJob(state), "wrong berth retains the campaign load");
        test.assertEqual(state.cargo, 4, "wrong berth retains four units");

        test.assertTrue(travelTo(state, campaign::kBenthos), "arrive at Benthos loaded");
        test.assertEqual(state.campaign & campaign::kMilestoneFirst, campaign::kMilestoneFirst, "FIRST milestone set");
        test.assertTrue(!game::hasActiveJob(state), "delivery clears the campaign job");
        test.assertEqual(state.cargo, 0, "delivery clears the hold");
        test.assertTrue(campaign::missionCompleted(state, campaign::kFirst), "FIRST completion flag");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kFirst), "completed FIRST never reappears");

        test.assertTrue(station::undock(state), "undock after delivery");
        test.assertTrue(at(state, campaign::kBenthos), "dock again");
        test.assertTrue(campaign::missionCompleted(state, campaign::kFirst), "repeated docking does not double-complete");

        // Short cargo cannot set the milestone, but the free source can reissue
        // the mission after the failed load is cleared.
        game::State shortState;
        game::reset(shortState);
        test.assertTrue(acceptAt(shortState, campaign::kTidewall, campaign::kFirst), "short-cargo FIRST accepted");
        shortState.cargo = 3;
        test.assertTrue(travelTo(shortState, campaign::kBenthos), "short load docks at Benthos");
        test.assertEqual(shortState.campaign & campaign::kMilestoneFirst, 0, "short cargo sets no milestone");
        test.assertTrue(!game::hasActiveJob(shortState), "short cargo clears the job");
        test.assertTrue(campaign::missionAvailable(shortState, campaign::kFirst), "source can reissue after short load");
        test.assertTrue(at(shortState, campaign::kTidewall), "return to source");
        test.assertTrue(campaign::accept(shortState, campaign::kFirst), "source reissues FIRST for free");

        test.assertTrue(game::hasActiveJob(shortState), "reissued load stays active");
        test.assertTrue(!campaign::canAccept(shortState, campaign::kFirst), "the one active slot refuses another load");
        runner.add(test);
    }

    {
        Test test("pump branch first: surveys early, both service berths, beacon, relay then HOME");
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
        test.assertTrue(!campaign::berthAvailable(state, 7), "no phantom berth id");

        test.assertTrue(acceptAt(state, campaign::kCormorant, campaign::kPump), "PUMP accepted at Cormorant");
        test.assertTrue(travelTo(state, campaign::kBenthos), "old Benthos handoff does not complete PUMP");
        test.assertTrue(campaign::missionCompleted(state, campaign::kPump) == false, "PUMP still active after Benthos");
        test.assertTrue(travelTo(state, campaign::kPumpRefuge), "impeller delivered to Pump Refuge");
        test.assertTrue(campaign::missionCompleted(state, campaign::kPump), "PUMP milestone set");
        test.assertTrue(campaign::berthAvailable(state, campaign::kPumpRefuge), "Pump Refuge opens after PUMP");
        test.assertTrue(!campaign::berthAvailable(state, campaign::kLowerGantry), "Lower Gantry still closed");

        test.assertTrue(acceptAt(state, campaign::kBenthos, campaign::kLift), "LIFT accepted at Benthos");
        test.assertTrue(travelTo(state, campaign::kCormorant), "old Cormorant handoff does not complete LIFT");
        test.assertTrue(campaign::missionCompleted(state, campaign::kLift) == false, "LIFT still active after Cormorant");
        test.assertTrue(travelTo(state, campaign::kFreightYard), "cable reels delivered to the Freight Yard lift");
        test.assertTrue(campaign::missionCompleted(state, campaign::kLift), "LIFT milestone set");
        test.assertTrue(campaign::berthAvailable(state, campaign::kLowerGantry), "Lower Gantry opens after LIFT");
        test.assertTrue(campaign::berthAvailable(state, campaign::kFreightYard), "Freight Yard stays open after LIFT");

        test.assertTrue(acceptAt(state, campaign::kCormorant, campaign::kBeacon), "BEACON explicitly accepted");
        test.assertTrue(travelTo(state, campaign::kDeepAnchor), "beacon core delivered");
        test.assertTrue(campaign::missionCompleted(state, campaign::kBeacon), "BEACON milestone set");
        test.assertTrue(!game::hasActiveJob(state), "BEACON does not auto-load HOME");
        test.assertEqual(state.cargo, 0, "Deep Anchor docking leaves the hold empty");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kHome), "HOME waits for DEEP RELAY, not just BEACON");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kExpedition), "DEEP RELAY still needs the missing thruster and lens");
        state.tools |= static_cast<uint8_t>(exploration_data::THRUSTER | exploration_data::LENS);
        test.assertTrue(campaign::missionAvailable(state, campaign::kExpedition), "all four tools expose DEEP RELAY");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kHome), "HOME remains closed before the relay");
        test.assertTrue(!campaign::missionCompleted(state, campaign::kHome), "HOME is not auto-complete");

        test.assertTrue(acceptAt(state, campaign::kDeepAnchor, campaign::kExpedition), "DEEP RELAY accepted at Deep Anchor");
        test.assertEqual(state.activeJob.to, campaign::kDeepRelay, "DEEP RELAY is bound for the lower-right wreck");
        test.assertTrue(travelTo(state, campaign::kDeepRelay), "relay recovered at the work site");
        test.assertTrue(campaign::missionCompleted(state, campaign::kExpedition), "DEEP RELAY milestone set");
        test.assertTrue(campaign::missionAvailable(state, campaign::kHome), "HOME opens only after DEEP RELAY");

        test.assertTrue(acceptAt(state, campaign::kDeepAnchor, campaign::kHome), "HOME explicitly accepted");
        test.assertTrue(travelTo(state, campaign::kTidewall), "dispatch delivered home");
        test.assertTrue(campaign::missionCompleted(state, campaign::kHome), "HOME milestone set");
        test.assertTrue(campaign::markEndingPresented(state), "ending can be marked after HOME");
        test.assertEqual(state.ending, 1, "ending flag set");

        test.assertTrue(station::undock(state), "undock after ending");
        test.assertTrue(at(state, campaign::kTidewall), "redock at Tidewall");
        test.assertTrue(campaign::missionCompleted(state, campaign::kHome), "HOME never completes twice");
        runner.add(test);
    }

    {
        Test test("lift branch first is equally valid and reaches the same completion");
        game::State state;
        game::reset(state);
        test.assertTrue(campaign::recordSurvey(state, campaign::kLiftSurveyId), "lift survey found first");
        test.assertTrue(campaign::recordSurvey(state, campaign::kPumpSurveyId), "pump survey found second");
        test.assertTrue(completeFirst(state), "FIRST completes");

        test.assertTrue(acceptAt(state, campaign::kBenthos, campaign::kLift), "LIFT accepted first");
        test.assertTrue(travelTo(state, campaign::kFreightYard), "LIFT delivered first at Freight Yard");
        test.assertTrue(campaign::berthAvailable(state, campaign::kLowerGantry), "Lower Gantry opens");

        test.assertTrue(acceptAt(state, campaign::kCormorant, campaign::kPump), "PUMP accepted second");
        test.assertTrue(travelTo(state, campaign::kPumpRefuge), "PUMP delivered second at Pump Refuge");
        test.assertTrue(campaign::berthAvailable(state, campaign::kPumpRefuge), "Pump Refuge opens");

        state.tools |= exploration_data::SEAL;
        test.assertTrue(acceptAt(state, campaign::kCormorant, campaign::kBeacon), "BEACON after both orders");
        test.assertTrue(travelTo(state, campaign::kDeepAnchor), "BEACON delivered");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kHome), "reversed order still needs DEEP RELAY");
        test.assertTrue(completeExpedition(state), "DEEP RELAY completes after BEACON");
        test.assertTrue(acceptAt(state, campaign::kDeepAnchor, campaign::kHome), "HOME accepted");
        test.assertTrue(travelTo(state, campaign::kTidewall), "HOME delivered");
        test.assertTrue(campaign::missionCompleted(state, campaign::kHome), "HOME completes in reversed order");
        runner.add(test);
    }

    {
        Test test("story work is untimed and WORK lists only restoration rows");
        game::State state;
        game::reset(state);
        test.assertTrue(at(state, campaign::kTidewall), "dock at Tidewall");
        test.assertEqual(work_view::rowCount(state), 1, "FIRST is the only row");
        const work_view::Row storyRow = work_view::rowAt(state, 0);
        test.assertEqual(storyRow.kind, work_view::kRowStory, "restoration is listed");
        test.assertEqual(storyRow.mission, campaign::kFirst, "FIRST is the story row");

        test.assertTrue(work_view::accept(state, storyRow), "accept FIRST through WORK");
        test.assertEqual(state.activeJob.kind, game::kJobKindCampaign, "campaign kind stored");
        test.assertTrue(station::undock(state), "story delivery undocks");
        for (uint16_t i = 0; i < 600; ++i)
            game::step(state, idle());
        test.assertTrue(travelTo(state, campaign::kBenthos), "story arrives after a long flight");
        test.assertTrue(campaign::missionCompleted(state, campaign::kFirst), "story delivery completes");

        test.assertTrue(at(state, campaign::kTidewall), "dock at Tidewall for a fresh board");
        test.assertEqual(work_view::rowCount(state), 0, "completed FIRST leaves no Tidewall work row");
        runner.add(test);
    }

    {
        Test test("next-stop hint follows surveys, repairs and seal in both repair orders");
        game::State pumpFirst;
        game::reset(pumpFirst);
        test.assertTrue(completeFirst(pumpFirst), "FIRST completes");
        test.assertEqual(campaign::nextStopAfter(pumpFirst, campaign::kFirst), campaign::kNextStopNone, "FIRST deck posts no next-stop hint");

        test.assertTrue(completePump(pumpFirst), "PUMP completes with the lift survey still unrecorded");
        test.assertTrue(!campaign::surveyRecorded(pumpFirst, campaign::kLiftSurveyId), "lift survey missing after PUMP");
        test.assertEqual(campaign::nextStopAfter(pumpFirst, campaign::kPump), campaign::kNextStopLiftSurvey, "PUMP points at the lift survey");
        test.assertEqual(campaign::nextStopAfter(pumpFirst, campaign::kLiftSurvey), campaign::kNextStopNone, "survey deck posts no hint");

        test.assertTrue(campaign::recordSurvey(pumpFirst, campaign::kLiftSurveyId), "lift survey recorded");
        test.assertEqual(campaign::nextStopAfter(pumpFirst, campaign::kPump), campaign::kNextStopLiftSource, "PUMP with the survey points at the cable source");

        test.assertTrue(completeLift(pumpFirst), "LIFT completes after PUMP");
        test.assertTrue(!campaign::sealOwned(pumpFirst), "seal missing after both repairs");
        test.assertEqual(campaign::nextStopAfter(pumpFirst, campaign::kPump), campaign::kNextStopSeal, "PUMP deck after LIFT points at the seal");
        test.assertEqual(campaign::nextStopAfter(pumpFirst, campaign::kLift), campaign::kNextStopSeal, "LIFT deck points at the seal");

        pumpFirst.tools |= exploration_data::SEAL;
        test.assertEqual(campaign::nextStopAfter(pumpFirst, campaign::kPump), campaign::kNextStopBeacon, "PUMP deck with the seal points at BEACON");
        test.assertEqual(campaign::nextStopAfter(pumpFirst, campaign::kLift), campaign::kNextStopBeacon, "LIFT deck with the seal points at BEACON");
        test.assertEqual(campaign::nextStopAfter(pumpFirst, campaign::kBeacon), campaign::kNextStopNone, "BEACON deck posts no hint");
        test.assertEqual(campaign::nextStopAfter(pumpFirst, campaign::kHome), campaign::kNextStopNone, "HOME deck posts no hint");

        game::State liftFirst;
        game::reset(liftFirst);
        test.assertTrue(completeFirst(liftFirst), "FIRST completes");
        test.assertTrue(completeLift(liftFirst), "LIFT completes first");
        test.assertTrue(!campaign::sealOwned(liftFirst), "seal missing after LIFT");
        test.assertEqual(campaign::nextStopAfter(liftFirst, campaign::kLift), campaign::kNextStopPumpSurvey, "LIFT-first deck names the unfinished pump survey, not the seal");
        liftFirst.tools |= exploration_data::SEAL;
        test.assertEqual(campaign::nextStopAfter(liftFirst, campaign::kLift), campaign::kNextStopPumpSurvey, "a seal cannot skip the unfinished pump survey");
        liftFirst.tools &= static_cast<uint8_t>(~exploration_data::SEAL);

        test.assertTrue(campaign::recordSurvey(liftFirst, campaign::kPumpSurveyId), "pump survey recorded");
        test.assertEqual(campaign::nextStopAfter(liftFirst, campaign::kLift), campaign::kNextStopPumpSource, "LIFT-first deck then points at the Cormorant impeller");
        test.assertTrue(completePump(liftFirst), "PUMP completes second");
        test.assertEqual(campaign::nextStopAfter(liftFirst, campaign::kPump), campaign::kNextStopSeal, "PUMP after LIFT points at the seal");
        liftFirst.tools |= exploration_data::SEAL;
        test.assertEqual(campaign::nextStopAfter(liftFirst, campaign::kPump), campaign::kNextStopBeacon, "PUMP with both repairs and the seal points at BEACON");
        runner.add(test);
    }

    {
        Test test("new HOME acceptance needs the relay");
        game::State fresh;
        game::reset(fresh);
        fresh.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon);
        fresh.tools = static_cast<uint8_t>(exploration_data::CUTTER | exploration_data::SEAL | exploration_data::THRUSTER | exploration_data::LENS);
        test.assertTrue(at(fresh, campaign::kDeepAnchor), "new voyage docks at Deep Anchor");
        test.assertTrue(!campaign::missionAvailable(fresh, campaign::kHome), "new HOME is unavailable before DEEP RELAY");
        test.assertTrue(!campaign::accept(fresh, campaign::kHome), "new HOME cannot be accepted before DEEP RELAY");
        test.assertTrue(!campaign::completeMission(fresh, campaign::kHome), "a bare call cannot skip the relay");
        fresh.campaign |= campaign::kMilestoneExpedition;
        test.assertTrue(campaign::accept(fresh, campaign::kHome), "HOME accepts after the relay milestone");
        runner.add(test);
    }

    {
        Test test("survey rings dwell slowly, record once and never occupy the cargo slot");
        game::State state;
        game::reset(state);
        world::Survey survey;
        test.assertTrue(world::surveyInfo(0, survey), "generated pump survey exists");
        state.x = survey.pixel_x;
        state.y = survey.pixel_y;
        state.vxQ8 = 0;
        state.vyQ8 = 0;
        test.assertTrue(!campaign::surveyRecorded(state, campaign::kPumpSurveyId), "survey starts unrecorded");
        for (uint8_t i = 0; i < exploration::kDwellTicks; ++i)
            exploration::update(state);
        test.assertTrue(campaign::surveyRecorded(state, campaign::kPumpSurveyId), "pump survey records after the dwell");
        test.assertTrue(!game::hasActiveJob(state), "survey never occupies the cargo slot");
        test.assertEqual(state.cargo, 0, "survey loads no cargo");
        test.assertEqual(state.lastDiscovery, exploration::kNone, "survey discovery uses the survey marker");
        test.assertTrue(state.discoveryTicks != 0, "survey finding is announced");
        const uint8_t recorded = state.surveys;
        for (uint8_t i = 0; i < exploration::kDwellTicks; ++i)
            exploration::update(state);
        test.assertEqual(state.surveys, recorded, "repeat dwell cannot change the survey mask");
        test.assertTrue(campaign::recordSurvey(state, campaign::kPumpSurveyId), "survey record is repeat-safe");
        runner.add(test);
    }

    {
        Test test("after FIRST a moving pilot near an unrecorded survey gets the stop hint");
        game::State state;
        game::reset(state);
        world::Survey survey;
        test.assertTrue(world::surveyInfo(0, survey), "generated pump survey exists");
        state.x = static_cast<int16_t>(survey.pixel_x + 40);   // wide hint zone, outside the dwell zone
        state.y = survey.pixel_y;
        state.vxQ8 = exploration::kOperateSpeed + 1;           // moving, so no dwell target
        state.vyQ8 = 0;
        state.contactId = exploration::kNone;
        exploration::update(state);
        test.assertEqual(state.contactId, exploration::kNone, "no hint before FIRST");
        test.assertEqual(state.contactTicks, 0, "no dwell progress before FIRST");

        state.campaign = campaign::kMilestoneFirst;
        exploration::update(state);
        test.assertEqual(state.contactId, static_cast<uint8_t>(exploration::kSurveyBase + survey.id), "survey hint after FIRST");
        test.assertEqual(state.contactTicks, 0, "hint alone does not record");
        test.assertEqual(state.surveys, 0, "survey remains unrecorded");

        state.x = survey.pixel_x;
        state.vxQ8 = 0;
        for (uint8_t i = 0; i < exploration::kDwellTicks; ++i)
            exploration::update(state);
        test.assertTrue(campaign::surveyRecorded(state, campaign::kPumpSurveyId), "stopping still records the survey");
        exploration::update(state);
        test.assertEqual(state.contactId, exploration::kNone, "recorded survey stops hinting");
        runner.add(test);
    }

    {
        Test test("survey missions are WORK rows at their clue berths after FIRST");
        game::State state;
        game::reset(state);
        state.campaign = campaign::kMilestoneFirst;
        test.assertEqual(campaign::workMissionAt(state, campaign::kBenthos, 0), campaign::kPumpSurvey, "pump survey quest at Benthos");
        test.assertEqual(campaign::workMissionAt(state, campaign::kCormorant, 0), campaign::kLiftSurvey, "lift survey quest at Cormorant");
        test.assertTrue(!campaign::isCargoMission(campaign::kPumpSurvey), "pump survey is not cargo");
        test.assertTrue(!campaign::isCargoMission(campaign::kLiftSurvey), "lift survey is not cargo");
        test.assertTrue(!campaign::canAccept(state, campaign::kPumpSurvey), "survey quest cannot load cargo");

        test.assertTrue(campaign::recordSurvey(state, campaign::kPumpSurveyId), "pump survey recorded");
        test.assertEqual(campaign::workMissionAt(state, campaign::kCormorant, 0), campaign::kPump, "PUMP cargo row appears");
        test.assertEqual(campaign::workMissionAt(state, campaign::kCormorant, 1), campaign::kLiftSurvey, "lift survey remains a row");
        test.assertEqual(campaign::workMissionAt(state, campaign::kBenthos, 0), campaign::kMissionNone, "Benthos pump survey row is gone");

        test.assertTrue(campaign::recordSurvey(state, campaign::kLiftSurveyId), "lift survey recorded");
        test.assertEqual(campaign::workMissionAt(state, campaign::kBenthos, 0), campaign::kLift, "LIFT cargo row appears");
        test.assertEqual(campaign::workMissionAt(state, campaign::kCormorant, 1), campaign::kMissionNone, "lift survey row is gone");
        runner.add(test);
    }

    {
        Test test("WORK rows expose both repair branches, beacon, DEEP RELAY, explicit HOME and VIEW ENDING");
        game::State state;
        game::reset(state);
        test.assertTrue(at(state, campaign::kTidewall), "dock at Tidewall");
        test.assertEqual(campaign::workMissionAt(state, campaign::kTidewall, 0), campaign::kFirst, "FIRST row");
        test.assertEqual(campaign::workMissionAt(state, campaign::kTidewall, 1), campaign::kMissionNone, "no second early row");
        test.assertEqual(campaign::workMissionAt(state, campaign::kBenthos, 0), campaign::kMissionNone, "Benthos has no cargo source");

        test.assertTrue(campaign::recordSurvey(state, campaign::kPumpSurveyId), "pump survey can precede FIRST");
        test.assertTrue(campaign::recordSurvey(state, campaign::kLiftSurveyId), "lift survey can precede FIRST");
        test.assertTrue(completeFirst(state), "FIRST completes");
        test.assertEqual(campaign::workMissionAt(state, campaign::kCormorant, 0), campaign::kPump, "PUMP row at Cormorant");
        test.assertEqual(campaign::workMissionAt(state, campaign::kCormorant, 1), campaign::kMissionNone, "BEACON waits for both repairs");
        test.assertEqual(campaign::workMissionAt(state, campaign::kBenthos, 0), campaign::kLift, "LIFT row at Benthos");

        test.assertTrue(completePump(state), "PUMP completes");
        test.assertEqual(campaign::workMissionAt(state, campaign::kBenthos, 0), campaign::kLift, "LIFT remains the only repair at Benthos");
        test.assertEqual(campaign::workMissionAt(state, campaign::kCormorant, 0), campaign::kMissionNone, "BEACON still waits for LIFT");
        test.assertTrue(completeLift(state), "LIFT completes");

        test.assertEqual(campaign::workMissionAt(state, campaign::kCormorant, 0), campaign::kMissionNone, "BEACON needs the seal");
        state.tools |= exploration_data::SEAL;
        test.assertEqual(campaign::workMissionAt(state, campaign::kCormorant, 0), campaign::kBeacon, "BEACON row at Cormorant");
        test.assertEqual(campaign::workMissionAt(state, campaign::kDeepAnchor, 0), campaign::kMissionNone, "HOME is hidden before BEACON");

        test.assertTrue(acceptAt(state, campaign::kCormorant, campaign::kBeacon), "BEACON accepted explicitly");
        test.assertTrue(travelTo(state, campaign::kDeepAnchor), "BEACON delivered");
        test.assertEqual(campaign::workMissionAt(state, campaign::kDeepAnchor, 0), campaign::kMissionNone, "DEEP RELAY needs all four tools");
        state.tools |= static_cast<uint8_t>(exploration_data::CUTTER | exploration_data::THRUSTER | exploration_data::LENS);
        test.assertEqual(campaign::workMissionAt(state, campaign::kDeepAnchor, 0), campaign::kExpedition, "DEEP RELAY row appears at Deep Anchor");
        test.assertEqual(campaign::workMissionAt(state, campaign::kDeepAnchor, 1), campaign::kMissionNone, "HOME is hidden before DEEP RELAY");
        test.assertTrue(acceptAt(state, campaign::kDeepAnchor, campaign::kExpedition), "DEEP RELAY accepted through the row");
        test.assertTrue(travelTo(state, campaign::kDeepRelay), "DEEP RELAY delivered at the lower-right site");
        test.assertEqual(campaign::workMissionAt(state, campaign::kDeepAnchor, 0), campaign::kHome, "HOME row appears only after DEEP RELAY");
        test.assertTrue(travelTo(state, campaign::kDeepAnchor), "return to Deep Anchor for HOME dispatch");
        test.assertTrue(campaign::accept(state, campaign::kHome), "HOME is not auto-loaded by docking");
        test.assertEqual(state.activeJob.mission, campaign::kHome, "HOME carries its own mission id");
        test.assertTrue(travelTo(state, campaign::kTidewall), "HOME delivered");
        test.assertEqual(campaign::workMissionAt(state, campaign::kTidewall, 0), campaign::kWorkEnding, "Tidewall offers VIEW ENDING");
        test.assertEqual(campaign::workMissionAt(state, campaign::kBenthos, 0), campaign::kMissionNone, "completed LIFT never respawns");
        test.assertEqual(campaign::workMissionAt(state, campaign::kCormorant, 0), campaign::kMissionNone, "completed BEACON never respawns");
        test.assertEqual(campaign::workMissionAt(state, campaign::kDeepAnchor, 0), campaign::kMissionNone, "completed HOME never respawns");
        runner.add(test);
    }

    {
        Test test("mandatory Deep Relay expedition needs all four tools and gates HOME");
        game::State state;
        game::reset(state);
        state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift);
        state.tools = static_cast<uint8_t>(exploration_data::CUTTER | exploration_data::SEAL | exploration_data::THRUSTER | exploration_data::LENS);
        test.assertTrue(!campaign::missionAvailable(state, campaign::kExpedition), "DEEP RELAY still waits for BEACON");
        state.campaign |= campaign::kMilestoneBeacon;
        state.tools = static_cast<uint8_t>(exploration_data::CUTTER | exploration_data::SEAL | exploration_data::THRUSTER);
        test.assertTrue(!campaign::missionAvailable(state, campaign::kExpedition), "missing survey lens blocks the expedition");
        test.assertTrue(!campaign::missionAvailable(state, campaign::kHome), "HOME stays closed without the relay");
        state.tools |= exploration_data::LENS;
        test.assertTrue(campaign::missionAvailable(state, campaign::kExpedition), "all four tools open the expedition");
        test.assertTrue(acceptAt(state, campaign::kDeepAnchor, campaign::kExpedition), "expedition accepted at Deep Anchor");
        test.assertEqual(state.activeJob.to, campaign::kDeepRelay, "expedition is bound for Deep Relay");
        test.assertEqual(state.cargo, game::kJobCrates, "expedition loads its recovery cargo");
        test.assertTrue(travelTo(state, campaign::kDeepRelay), "lower-right Deep Relay work site docks");
        test.assertTrue(campaign::missionCompleted(state, campaign::kExpedition), "expedition milestone set at the wreck");
        test.assertTrue(campaign::missionAvailable(state, campaign::kHome), "HOME opens after the relay");
        test.assertTrue(campaign::berthAvailable(state, campaign::kDeepRelay), "Deep Relay stays available after completion");
        test.assertTrue(campaign::missionCompleted(state, campaign::kExpedition), "completed expedition checkpoint is valid");
        const uint8_t beforeExpedition = static_cast<uint8_t>(state.campaign & ~campaign::kMilestoneExpedition);
        test.assertEqual(story_view::cardForNewMilestone(beforeExpedition, state.campaign),
                         story_view::kCardExpedition, "fresh expedition selects the relay result deck");
        runner.add(test);
    }

    {
        Test test("loaded FIRST route spends air and keeps the frozen story job");
        game::State state;
        game::reset(state);
        test.assertTrue(acceptAt(state, campaign::kTidewall, campaign::kFirst), "accept loaded FIRST");
        test.assertTrue(station::undock(state), "depart loaded");
        const int16_t startX = state.x;
        const int16_t airBefore = state.air;
        for (uint16_t tick = 0; tick < game::kAirDrainTicks; ++tick)
            game::step(state, game::Input{false, false, false, true});
        test.assertEqual(state.cargo, game::kJobCrates, "loaded four units survive flight");
        test.assertTrue(state.air < airBefore, "loaded flight spends air");
        test.assertTrue(state.x > startX, "loaded route makes forward progress");
        test.assertTrue(campaign::cargoJobMatches(state.activeJob), "loaded route keeps matching mission metadata");
        test.assertEqual(state.activeJob.to, campaign::kBenthos, "loaded route keeps the frozen destination");
        test.assertEqual(state.activeJob.crates, game::kJobCrates, "loaded route keeps the four-unit load");
        runner.add(test);
    }

    {
        Test test("Pump Refuge stays inactive until PUMP, then docks as a service berth");
        world::Berth berth;
        test.assertTrue(world::serviceBerthInfo(0, berth), "generated Pump Refuge berth");

        game::State locked;
        game::reset(locked);
        locked.x = berth.pixel_x;
        locked.y = berth.pixel_y;
        locked.vxQ8 = 0;
        locked.vyQ8 = 0;
        test.assertTrue(!station::dock(locked), "inactive mooring cannot be docked");

        game::State state;
        game::reset(state);
        state.campaign = campaign::kMilestonePump;
        state.x = berth.pixel_x;
        state.y = berth.pixel_y;
        state.vxQ8 = 0;
        state.vyQ8 = 0;
        state.air = 1;
        state.ballast = 100;
        test.assertTrue(station::dock(state), "restored Pump Refuge docks");
        test.assertEqual(state.docked, static_cast<int8_t>(campaign::kPumpRefuge), "Pump Refuge id");
        test.assertEqual(state.x, berth.pixel_x, "dock snaps to generated x");
        test.assertEqual(state.y, berth.pixel_y, "dock snaps to generated y");
        test.assertEqual(state.air, game::airCapacity(state), "service refills air");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "service neutralizes trim");
        test.assertEqual(station::rowCount(state), 1, "smaller service board");
        test.assertEqual(station::rowKind(0, state), station::kRowUndock, "only UNDOCK is selectable");
        test.assertTrue(station::undock(state), "service berth undocks");
        runner.add(test);
    }

    {
        Test test("Lower Gantry stays service-only after LIFT and never trades");
        world::Berth berth;
        test.assertTrue(world::serviceBerthInfo(1, berth), "generated Lower Gantry berth");
        test.assertEqual(berth.id, campaign::kLowerGantry, "stable Lower Gantry id");
        test.assertTrue(berth.pixel_y < exploration_data::PRESSURE_Y, "Lower Gantry sits above the pressure boundary");

        game::State locked;
        game::reset(locked);
        locked.x = berth.pixel_x;
        locked.y = berth.pixel_y;
        locked.vxQ8 = 0;
        locked.vyQ8 = 0;
        test.assertTrue(!station::dock(locked), "restoration gate blocks the mooring");

        game::State state;
        game::reset(state);
        state.campaign = campaign::kMilestoneLift;
        state.x = berth.pixel_x;
        state.y = berth.pixel_y;
        state.vxQ8 = 0;
        state.vyQ8 = 0;
        state.air = 1;
        state.tools = exploration_data::SEAL;
        game::setCargo(state, 0);
        test.assertTrue(station::dock(state), "restored Lower Gantry docks");
        test.assertEqual(state.docked, static_cast<int8_t>(campaign::kLowerGantry), "Lower Gantry id");
        test.assertEqual(station::rowCount(state), 1, "service board stays minimal");
        test.assertEqual(station::rowKind(0, state), station::kRowUndock, "only UNDOCK is selectable");
        test.assertEqual(state.air, game::airCapacity(state), "service refills air");

        // A loaded campaign checkpoint at the service berth must restore the
        // full generated position, cargo and job without a station board.
        state.activeJob.kind = game::kJobKindCampaign;
        state.activeJob.mission = campaign::kFirst;
        state.activeJob.from = campaign::kTidewall;
        state.activeJob.to = campaign::kBenthos;
        state.activeJob.crates = game::kJobCrates;
        game::setCargo(state, game::kJobCrates);
        test.assertTrue(campaign::cargoJobMatches(state.activeJob), "loaded service-berth job matches its mission metadata");
        runner.add(test);
    }

    {
        Test test("dock point follows saved progression in both repair orders");
        test.assertEqual(campaign::kDockPointCount, 14, "campaign and missing-equipment points");

        game::State pumpFirst;
        game::reset(pumpFirst);
        test.assertEqual(campaign::dockPoint(pumpFirst), campaign::kDockPointFresh, "fresh voyage");
        pumpFirst.tools = exploration_data::SEAL;
        test.assertEqual(campaign::dockPoint(pumpFirst), campaign::kDockPointFresh, "owned tools cannot skip FIRST");

        pumpFirst.campaign = campaign::kMilestoneFirst;
        test.assertEqual(campaign::dockPoint(pumpFirst), campaign::kDockPointFirst, "FIRST opens both repairs");
        pumpFirst.surveys = campaign::kSurveyPump;
        pumpFirst.campaign |= campaign::kMilestonePump;
        test.assertEqual(campaign::dockPoint(pumpFirst), campaign::kDockPointPump, "pump-first names the open lift");
        pumpFirst.surveys = campaign::kSurveyMask;
        pumpFirst.campaign |= campaign::kMilestoneLift;
        pumpFirst.tools = 0;
        test.assertEqual(campaign::dockPoint(pumpFirst), campaign::kDockPointBoth, "both repairs still need the seal");
        pumpFirst.tools = exploration_data::SEAL;
        test.assertEqual(campaign::dockPoint(pumpFirst), campaign::kDockPointSeal, "seal makes the core ready");
        pumpFirst.campaign |= campaign::kMilestoneBeacon;
        pumpFirst.tools = exploration_data::CUTTER | exploration_data::SEAL | exploration_data::LENS | exploration_data::THRUSTER;
        test.assertEqual(campaign::dockPoint(pumpFirst), campaign::kDockPointCore, "beacon opens the relay");
        pumpFirst.campaign |= campaign::kMilestoneExpedition;
        test.assertEqual(campaign::dockPoint(pumpFirst), campaign::kDockPointRelay, "relay opens HOME");
        pumpFirst.campaign |= campaign::kMilestoneHome;
        test.assertEqual(campaign::dockPoint(pumpFirst), campaign::kDockPointHome, "HOME restores the network");

        game::State liftFirst;
        game::reset(liftFirst);
        liftFirst.campaign = campaign::kMilestoneFirst;
        liftFirst.surveys = campaign::kSurveyLift;
        liftFirst.campaign |= campaign::kMilestoneLift;
        test.assertEqual(campaign::dockPoint(liftFirst), campaign::kDockPointLift, "lift-first names the open pump");
        liftFirst.surveys = campaign::kSurveyMask;
        liftFirst.campaign |= campaign::kMilestonePump;
        test.assertEqual(campaign::dockPoint(liftFirst), campaign::kDockPointBoth, "lift-first still needs the seal");
        liftFirst.tools = exploration_data::SEAL;
        test.assertEqual(campaign::dockPoint(liftFirst), campaign::kDockPointSeal, "lift-first reaches the same sealed point");

        game::State legacyHome;
        game::reset(legacyHome);
        legacyHome.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump |
                                                 campaign::kMilestoneLift | campaign::kMilestoneBeacon | campaign::kMilestoneHome);
        test.assertEqual(campaign::dockPoint(legacyHome), campaign::kDockPointHome, "legacy HOME without EXPEDITION stays HOME");
        runner.add(test);
    }

    {
        Test test("dock hints stop sending surveyed repairs back to inspection");
        game::State state;
        game::reset(state);
        state.campaign = campaign::kMilestoneFirst | campaign::kMilestonePump;
        test.assertEqual(campaign::dockPoint(state), campaign::kDockPointPump, "lift survey missing");
        state.surveys = campaign::kSurveyLift;
        test.assertEqual(campaign::dockPoint(state), campaign::kDockPointLiftReady, "known cable fault names Benthos");
        state.campaign = campaign::kMilestoneFirst | campaign::kMilestoneLift;
        test.assertEqual(campaign::dockPoint(state), campaign::kDockPointLift, "pump survey missing");
        state.surveys |= campaign::kSurveyPump;
        test.assertEqual(campaign::dockPoint(state), campaign::kDockPointPumpReady, "known impeller fault names Cormorant");
        runner.add(test);
    }

    {
        Test test("Deep Anchor names missing tools until the relay work is available");
        game::State state;
        game::reset(state);
        state.docked = campaign::kDeepAnchor;
        state.surveys = campaign::kSurveyMask;
        const uint8_t core = campaign::kMilestoneFirst | campaign::kMilestonePump |
                             campaign::kMilestoneLift | campaign::kMilestoneBeacon;
        const uint8_t tools[] = {exploration_data::CUTTER, exploration_data::SEAL,
                                 exploration_data::LENS, exploration_data::THRUSTER};
        for (uint8_t mask = 0; mask < 16; ++mask) {
            state.campaign = core;
            state.tools = 0;
            for (uint8_t bit = 0; bit < 4; ++bit)
                if (mask & (1u << bit))
                    state.tools |= tools[bit];
            const campaign::DockPoint point = campaign::dockPoint(state);
            test.assertEqual(campaign::missionAvailable(state, campaign::kExpedition),
                             point == campaign::kDockPointCore, "ready hint agrees with actual job gate");
            // Check the player's promise: any equipment named by a hint must
            // actually be missing. Availability above independently rejects a
            // ready hint when any prerequisite is absent.
            switch (point) {
            case campaign::kDockPointBoth:
                test.assertTrue(!(state.tools & exploration_data::SEAL), "seal lead never names an owned seal");
                break;
            case campaign::kDockPointCutter:
                test.assertTrue(!(state.tools & exploration_data::CUTTER), "cutter lead never names an owned cutter");
                break;
            case campaign::kDockPointLens:
                test.assertTrue(!(state.tools & exploration_data::LENS), "lens lead never names an owned lens");
                break;
            case campaign::kDockPointThruster:
                test.assertTrue(!(state.tools & exploration_data::THRUSTER), "thruster lead never names an owned thruster");
                break;
            default:
                test.assertEqual(point, campaign::kDockPointCore, "relay stage has equipment or available work");
                break;
            }
            state.campaign |= campaign::kMilestoneExpedition;
            if (state.tools & exploration_data::SEAL)
                test.assertEqual(campaign::dockPoint(state), campaign::kDockPointRelay, "completed relay never sends player for equipment");
            state.campaign |= campaign::kMilestoneHome;
            test.assertEqual(campaign::dockPoint(state), campaign::kDockPointHome, "homecoming always wins over equipment leads");
        }
        runner.add(test);
    }
}


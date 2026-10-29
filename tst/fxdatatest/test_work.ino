// Device WORK-board test: the single station access, story-first row order,
// direct acceptance, an untimed long-style delivery, and the actual native
// draw path. Host tests cover save invalidation and every route; this suite
// keeps the device-only text/cart path honest.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/fxdata.h"
#include "src/game.hpp"
#include "src/jobs.hpp"
#include "src/station.hpp"
#include "src/text.hpp"
#include "src/work_view.hpp"
#include "src/story_view.hpp"
#include "src/world.hpp"
#include "src/campaign.hpp"
#include "src/generated/exploration.hpp"

Arduboy2Base arduboy;

static bool dockAt(game::State &state, uint8_t berth) {
    world::Settlement settlement;
    if (!world::settlementInfo(berth, settlement))
        return false;
    state.docked = -1;
    state.dockCooldown = 0;
    state.x = settlement.pixel_x;
    state.y = settlement.pixel_y;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    return station::dock(state);
}

static bool findFreight(const game::State &state, work_view::Row &row) {
    const uint8_t count = work_view::rowCount(state);
    for (uint8_t i = 0; i < count; ++i) {
        const work_view::Row candidate = work_view::rowAt(state, i);
        if (candidate.kind == work_view::kRowFreight) {
            row = candidate;
            return true;
        }
    }
    return false;
}

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);
    text::begin();

    bool ok = true;
    game::State state;
    game::resetWorld(state);

    uint8_t berth = 0;
    ok = dockAt(state, berth) && ok;
    ok = (state.offerCount == 1) && ok;
    work_view::Row freight{work_view::kRowNone, 0};
    ok = findFreight(state, freight) && ok;

    // The board must paint through the real external-font/cart path before any
    // later state mutation.
    work_view::draw(state, 0);

    // At Cormorant before the repairs, PUMP is the story row and freight is
    // directly beneath it.
    berth = campaign::kCormorant;
    ok = dockAt(state, berth) && ok;
    state.campaign = campaign::kMilestoneFirst;
    state.surveys = campaign::kSurveyMask;
    ok = (work_view::rowCount(state) == 2) && ok;
    ok = (work_view::rowAt(state, 0).kind == work_view::kRowStory) && ok;
    ok = (work_view::rowAt(state, 0).mission == campaign::kPump) && ok;
    ok = (work_view::rowAt(state, 1).kind == work_view::kRowFreight) && ok;
    work_view::draw(state, 1);

    ok = work_view::accept(state, work_view::rowAt(state, 0)) && ok;
    ok = (state.activeJob.kind == game::kJobKindCampaign) && ok;
    ok = (state.cargo == game::kJobCrates) && ok;
    work_view::draw(state, 0);
    ok = (work_view::rowCount(state) == 0) && ok;

    // Deliver through the real dock hook. The story reward is fixed and the
    // trip is untimed.
    const uint16_t creditsBefore = state.credits;
    const uint8_t campaignBefore = state.campaign;
    ok = dockAt(state, campaign::kBenthos) && ok;
    ok = (state.credits == creditsBefore + campaign::missionReward(campaign::kPump)) && ok;
    ok = (state.lastDelivery == jobs::kDeliveryPaid) && ok;

    // The PUMP milestone is a one-shot dock beat. Exercise the real panel
    // reader; a repeated dock reports no new card.
    const uint8_t pumpCard = story_view::cardForNewMilestone(campaignBefore, state.campaign);
    ok = (pumpCard == story_view::kCardPump) && ok;
    story_view::draw(pumpCard);
    ok = (story_view::cardForNewMilestone(state.campaign, state.campaign) == story_view::kCardNone) && ok;

    // All four appended panels stream through the shared opaque renderer.
    for (uint8_t card = story_view::kCardFirst; card <= story_view::kCardBeacon; ++card)
        story_view::draw(card);

    // Old timed kind values are no longer valid state.
    state.activeJob.kind = 3;
    ok = !campaign::activeJobValid(state) && ok;
    state.activeJob.kind = game::kJobKindNone;
    state.activeJob.mission = game::kMissionNone;
    state.activeJob.from = game::kJobNone;
    state.activeJob.to = game::kJobNone;
    state.activeJob.crates = 0;
    state.activeJob.reward = 0;

    // Repeatable freight is still one row beneath the story list and remains
    // payable after a long flight simulation.
    ok = findFreight(state, freight) && ok;
    if (freight.kind == work_view::kRowFreight) {
        const uint16_t before = state.credits;
        const uint8_t destination = state.offers[0].to;
        const uint16_t reward = state.offers[0].reward;
        ok = work_view::accept(state, freight) && ok;
        ok = station::undock(state) && ok;
        for (uint16_t i = 0; i < 3000; ++i)
            game::step(state, game::Input{false, false, false, false});
        ok = dockAt(state, destination) && ok;
        ok = (state.credits == before + reward) && ok;
        ok = (state.lastDelivery == jobs::kDeliveryPaid) && ok;
    } else {
        ok = false;
    }

    work_view::draw(state, 0);
    FX::display(CLEAR_BUFFER);

    Serial.println(ok ? F("P") : F("F"));
}

void loop() {
}

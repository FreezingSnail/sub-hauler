// Device WORK-board test: campaign-only story rows, direct acceptance, an
// untimed delivery with no payout, and the actual native draw path. This suite
// keeps the device-only text/cart path honest while host tests cover the rules.
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
    world::Berth settlement;
    if (!world::berthInfo(berth, settlement))
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

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    bool ok = true;
    game::State state;
    game::resetWorld(state);

    // Tidewall offers only the FIRST story row. The board paints through the
    // real external-font/cart path before later state mutation.
    ok = dockAt(state, campaign::kTidewall) && ok;
    ok = (work_view::rowCount(state) == 1) && ok;
    ok = (work_view::rowAt(state, 0).kind == work_view::kRowStory) && ok;
    ok = (work_view::rowAt(state, 0).mission == campaign::kFirst) && ok;
    work_view::draw(state, 0);

    // Accepting auto-loads the four cargo units and collapses the row list.
    ok = work_view::accept(state, work_view::rowAt(state, 0)) && ok;
    ok = (state.activeJob.kind == game::kJobKindCampaign) && ok;
    ok = (state.cargo == game::kJobCrates) && ok;
    work_view::draw(state, 0);
    ok = (work_view::rowCount(state) == 0) && ok;

    // Deliver through the real dock hook. There is no payout or clock.
    const uint8_t campaignBefore = state.campaign;
    const uint16_t deliveredBefore = state.delivered;
    ok = dockAt(state, campaign::kBenthos) && ok;
    ok = (state.lastDelivery == jobs::kDeliveryDelivered) && ok;
    ok = (state.campaign & campaign::kMilestoneFirst) && ok;
    ok = (state.delivered == deliveredBefore + 1) && ok;

    // The FIRST milestone is a one-shot dock beat. Exercise the real panel
    // reader; a repeated dock reports no new card.
    const uint8_t firstCard = story_view::cardForNewMilestone(campaignBefore, state.campaign);
    ok = (firstCard == story_view::kCardFirst) && ok;
    story_view::draw(firstCard);
    ok = (story_view::cardForNewMilestone(state.campaign, state.campaign) == story_view::kCardNone) && ok;

    // Survey decks are selected by their own recorded bit and never repeat.
    ok = (story_view::cardForNewSurvey(0, campaign::kSurveyPump) == story_view::kCardPumpSurvey) && ok;
    ok = (story_view::cardForNewSurvey(campaign::kSurveyPump, campaign::kSurveyPump) == story_view::kCardNone) && ok;

    // A recorded pump survey exposes the PUMP story row plus the lift survey.
    state.surveys = campaign::kSurveyPump;
    ok = dockAt(state, campaign::kCormorant) && ok;
    ok = (work_view::rowCount(state) == 2) && ok;
    ok = (work_view::rowAt(state, 0).mission == campaign::kPump) && ok;
    ok = (work_view::rowAt(state, 1).mission == campaign::kLiftSurvey) && ok;
    work_view::draw(state, 0);
    ok = work_view::accept(state, work_view::rowAt(state, 0)) && ok;
    ok = (state.cargo == game::kJobCrates) && ok;
    ok = dockAt(state, campaign::kPumpRefuge) && ok;
    ok = (state.lastDelivery == jobs::kDeliveryDelivered) && ok;
    ok = (state.campaign & campaign::kMilestonePump) && ok;

    // Old timed/ordinary job kinds are no longer valid state.
    state.activeJob.kind = 3;
    ok = !campaign::activeJobValid(state) && ok;
    state.activeJob.kind = game::kJobKindNone;
    state.activeJob.mission = game::kMissionNone;
    state.activeJob.from = game::kJobNone;
    state.activeJob.to = game::kJobNone;
    state.activeJob.crates = 0;

    // Every berth exposes only campaign story rows, never a repeatable job.
    for (uint8_t berth = 0; berth < world::settlementCount(); ++berth) {
        ok = dockAt(state, berth) && ok;
        for (uint8_t row = 0; row < work_view::rowCount(state); ++row)
            ok = (work_view::rowAt(state, row).kind == work_view::kRowStory) && ok;
    }

    // Stream every source scene/card through the compressed reader, including
    // consecutive text pages and reused loading/restoration art.
    uint8_t sceneCards = 0;
    for (uint8_t scene = story_view::kSceneFirst; scene <= story_view::kSceneMax; ++scene) {
        const uint8_t count = story_view::cardCount(scene);
        sceneCards = static_cast<uint8_t>(sceneCards + count);
        for (uint8_t card = 0; card < count; ++card) {
            const bool image = story_view::isImageCard(scene, card);
            const uint8_t phases = image ? 4 : 1;
            for (uint8_t phase = 0; phase < phases; ++phase)
                story_view::draw(scene, card, phase);
        }
    }
    ok = (sceneCards == STORY_CARD_COUNT) && ok;

    work_view::draw(state, 0);
    FX::display(CLEAR_BUFFER);

    Serial.println(ok ? F("P") : F("F"));
}

void loop() {
}

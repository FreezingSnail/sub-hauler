// Production story playback: exact decoded pixels, cadence, fresh button
// edges, frozen world, station/pilot returns, cold reload, HOME.
#include "boot_driver.hpp"
#include "story_frame_crc.hpp"
#include <util/crc16.h>

namespace {

uint16_t s_storyMaxUs = 0;

uint16_t storyCrc() {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < STORY_BEAT_WIDTH * STORY_BEAT_HEIGHT / 8; ++i)
        crc = _crc_ccitt_update(crc, Arduboy2Base::sBuffer[i]);
    return crc;
}

void storyTick(uint8_t held = 0) {
    Arduboy2Base::previousButtonState = Arduboy2Base::currentButtonState;
    Arduboy2Base::currentButtonState = held;
    s_just = static_cast<uint8_t>(held & ~Arduboy2Base::previousButtonState);
    s_present = true;
    music::service();
    updateStory();
    if (s_present)
        FX::display(CLEAR_BUFFER);
    music::service();
}

void advanceStory() {
    storyTick();
    storyTick(A_BUTTON);
}

bool pixels() {
    bool ok = true;
    for (uint8_t frame = 0; frame < STORY_BEAT_FRAMES; ++frame) {
        const uint32_t started = micros();
        story_view::drawFrame(frame);
        // CRC work is outside the render/display timing measurement.
        const uint32_t rendered = micros() - started;
        const bool matched = storyCrc() == pgm_read_word(kStoryFrameCrc + frame);
        const uint32_t displayStarted = micros();
        music::service();
        FX::display(CLEAR_BUFFER);
        music::service();
        const uint32_t elapsed = rendered + micros() - displayStarted;
        if (elapsed > s_storyMaxUs)
            s_storyMaxUs = static_cast<uint16_t>(elapsed);
        ok = matched && elapsed < 16000 && ok;
    }
    return ok;
}

bool playback() {
    game::reset(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kTidewall);
    s_pendingStory = story_view::kCardFirst;
    s_storyReturnPilot = 0;
    Arduboy2Base::currentButtonState = 0;
    beginStory();
    bool ok = s_screen == Screen::Story && s_storyCard == 0;
    const uint16_t ticks = s_state.ticks;
    const uint16_t air = s_state.air;
    for (uint8_t tick = 1; tick <= STORY_BEAT_CYCLE_TICKS; ++tick) {
        storyTick();
        const uint8_t phase = static_cast<uint8_t>((tick / STORY_BEAT_FRAME_TICKS) & 3);
        ok = s_storyPhase == phase && s_present == (tick % STORY_BEAT_FRAME_TICKS == 0) && ok;
    }
    ok = s_storyTicks == 0 && s_state.ticks == ticks && s_state.air == air && ok;
    storyTick(A_BUTTON);
    ok = s_storyCard == 1 && s_storyPhase == 0 && ok;
    for (uint8_t i = 0; i < STORY_BEAT_FRAME_TICKS + 1; ++i)
        storyTick(A_BUTTON);
    ok = s_storyCard == 1 && s_storyTicks == 0 && !s_present && ok;
    storyTick(A_BUTTON | B_BUTTON);
    ok = s_storyCard == 1 && ok;
    storyTick();
    storyTick(B_BUTTON);
    ok = s_storyCard == 2 && s_storyPhase == 0 && ok;
    for (uint8_t i = 2; i < story_view::cardCount(story_view::kCardFirst); ++i)
        advanceStory();
    ok = s_screen == Screen::Station && s_pendingStory == story_view::kCardNone && ok;

    s_state.docked = -1;
    s_pendingStory = story_view::kCardPumpSurvey;
    s_storyReturnPilot = 1;
    beginStory();
    for (uint8_t i = 0; i < story_view::cardCount(story_view::kCardPumpSurvey); ++i)
        advanceStory();
    ok = s_screen == Screen::Pilot && !s_storyReturnPilot && ok;

    s_state.docked = static_cast<int8_t>(campaign::kTidewall);
    s_pendingStory = story_view::kCardLift;

    s_state.campaign = campaign::kMilestoneMask;
    s_state.ending = 0;
    beginEnding();
    ok = s_screen == Screen::Story && s_pendingStory == story_view::kCardHome && ok;
    for (uint8_t i = 0; i < story_view::cardCount(story_view::kCardHome); ++i)
        advanceStory();
    ok = s_screen == Screen::Ending && s_uiAux.endingCard == 0 && ok;
    s_state.ending = 1;
    beginEnding();
    return s_screen == Screen::Ending && ok;
}

bool pixelSet(uint8_t x, uint8_t y) {
    return (Arduboy2Base::sBuffer[(y >> 3) * 128 + x] & (1 << (y & 7))) != 0;
}

// The undock notices own a solid 128x9 black backing. hud::draw only clears the
// hint plate for an active HUD notice, so drawVoyageNotice() must clear it.
bool noticeBacking() {
    bool ok = true;
    // Forced white plate: the notice clear must span the whole row and wipe any
    // stale HUD hint before the label lands.
    for (uint16_t i = 0; i < 1024; ++i)
        Arduboy2Base::sBuffer[i] = 0xFF;
    drawVoyageNotice(img_txt_saved_berth);
    uint16_t ink = 0;
    const uint8_t labelEnd = static_cast<uint8_t>(3 + text::kCellWidth * 14);
    for (uint8_t y = hud::kHintBackingY; y < hud::kHintBackingY + hud::kHintBackingH; ++y) {
        for (uint8_t x = 0; x < hud::kHintBackingW; ++x) {
            const bool cell = x >= 3 && x < labelEnd && y >= 56;
            if (cell) {
                if (pixelSet(x, y))
                    ++ink;
            } else {
                ok = !pixelSet(x, y) && ok;
            }
        }
    }
    ok = ink != 0 && ok;

    // Production frame: drawPilot() leaves the same plate black beside and above
    // the SAVED AT BERTH text.
    game::reset(s_state);
    s_screen = Screen::Pilot;
    s_savedNoticeTicks = 90;
    s_state.discoveryTicks = 0;
    drawPilot();
    for (uint8_t y = hud::kHintBackingY; y < 64; ++y) {
        for (uint8_t x = labelEnd; x < 128; ++x)
            ok = !pixelSet(x, y) && ok;
    }
    return ok;
}

// Cold-reload reproduction: a post-LIFT dock checkpoint is written through the
// real save path, the live app is reset as on power-up, and the real Continue
// path runs. Schema 8 persists the pending scene, so Continue resumes the LIFT
// result deck; because LIFT is the second repair, finishing it also hands off
// to the joint-restoration scene. A reload before the next natural save may
// replay the sequence (documented at-least-once, not exactly-once).
// Emulator/host-harness only: this erases and rewrites the cart save slot.
bool coldReload() {
    constexpr uint8_t kLiftCheckpoint = campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift;
    constexpr uint8_t kLiftSurveys = campaign::kSurveyPump | campaign::kSurveyLift;

    // Exactly the checkpoint the LIFT dock commits: Cormorant, both repairs and
    // surveys, no active job, service-full, with the selected LIFT deck pending.
    game::reset(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kCormorant);
    s_state.campaign = kLiftCheckpoint;
    s_state.surveys = kLiftSurveys;
    s_state.tools = static_cast<uint8_t>(exploration_data::CUTTER | exploration_data::SEAL);
    s_state.ending = 0;
    s_state.pendingStory = story_view::kCardLift;
    s_state.cargo = 0;
    s_state.air = game::airCapacity(s_state);
    s_state.ballast = static_cast<uint8_t>(game::neutralForNormalized(0));
    voyage_save::positionAtBerth(s_state, campaign::kCormorant);

    chart::reset(s_chart);
    chart::revealInitial(s_chart, s_state);

    // Before the cold reload the fresh LIFT bit selects the four-card deck.
    bool ok = story_view::cardForNewMilestone(static_cast<uint8_t>(kLiftCheckpoint & ~campaign::kMilestoneLift), kLiftCheckpoint) ==
              story_view::kCardLift;
    saveVoyageNow();

    // Cold boot: wipe the live run and chart, then use the real Continue path.
    resetRun();
    s_screen = Screen::Title;
    continueVoyage();

    ok = s_screen == Screen::Story && ok;
    ok = s_pendingStory == story_view::kCardLift && ok;
    ok = s_state.pendingStory == story_view::kCardLift && ok;
    ok = s_state.docked == static_cast<int8_t>(campaign::kCormorant) && ok;
    ok = s_state.campaign == kLiftCheckpoint && ok;
    ok = s_state.surveys == kLiftSurveys && ok;
    ok = !game::hasActiveJob(s_state) && ok;

    // Finish the resumed LIFT result deck: the second repair starts the joint
    // handoff. Seal ownership is saved, so the ready variant is selected.
    for (uint8_t card = 0; card < story_view::cardCount(story_view::kCardLift); ++card)
        advanceStory();
    ok = s_screen == Screen::Story && ok;
    ok = s_pendingStory == story_view::kHandoffJointReady && ok;
    ok = s_state.pendingStory == story_view::kHandoffJointReady && ok;
    for (uint8_t card = 0; card < story_view::cardCount(story_view::kHandoffJointReady); ++card)
        advanceStory();
    ok = s_screen == Screen::Station && ok;
    ok = s_pendingStory == story_view::kCardNone && ok;
    ok = s_state.pendingStory == story_view::kCardNone && ok;
    return ok;
}

// Departure and joint handoff playback. Acceptance owns the cargo departure
// scenes, including DEEP RELAY; the second repair owns the joint scene at the
// receiving settlement.
// The pending byte is the only cursor and is cleared after the last card.
bool handoffPlayback() {
    bool ok = true;
    const uint8_t missions[] = {campaign::kFirst, campaign::kPump, campaign::kLift,
                                campaign::kBeacon, campaign::kHome, campaign::kExpedition};
    for (uint8_t i = 0; i < sizeof(missions); ++i) {
        game::reset(s_state);
        s_state.docked = static_cast<int8_t>(campaign::kTidewall);
        s_pendingStory = story_view::handoffForMission(missions[i]);
        s_storyReturnPilot = 0;
        ok = s_pendingStory >= story_view::kHandoffFirst && s_pendingStory <= story_view::kSceneMax && ok;
        const uint8_t cards = story_view::cardCount(s_pendingStory);
        ok = cards >= 2 && story_view::isImageCard(s_pendingStory, 0) && ok;
        for (uint8_t card = 1; card < cards; ++card)
            ok = !story_view::isImageCard(s_pendingStory, card) && ok;
        beginStory();
        for (uint8_t card = 0; card < story_view::cardCount(s_pendingStory); ++card)
            advanceStory();
        ok = s_screen == Screen::Station && s_pendingStory == story_view::kCardNone && ok;
    }

    // PUMP completes second at Benthos without the seal: missing-seal joint.
    game::reset(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kBenthos);
    s_state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift);
    s_state.surveys = campaign::kSurveyMask;
    s_pendingStory = story_view::kScenePump;
    s_state.pendingStory = s_pendingStory;
    s_storyReturnPilot = 0;
    finishStory();
    ok = s_screen == Screen::Story && s_pendingStory == story_view::kHandoffJointMissing && ok;
    for (uint8_t card = 0; card < story_view::cardCount(s_pendingStory); ++card)
        advanceStory();
    ok = s_screen == Screen::Station && ok;

    // LIFT completes second at Cormorant with the seal: ready joint variant.
    game::reset(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kCormorant);
    s_state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift);
    s_state.surveys = campaign::kSurveyMask;
    s_state.tools = exploration_data::SEAL;
    s_pendingStory = story_view::kSceneLift;
    s_state.pendingStory = s_pendingStory;
    s_storyReturnPilot = 0;
    finishStory();
    ok = s_screen == Screen::Story && s_pendingStory == story_view::kHandoffJointReady && ok;
    for (uint8_t card = 0; card < story_view::cardCount(s_pendingStory); ++card)
        advanceStory();
    return s_screen == Screen::Station && ok;
}

// New Voyage plays the RAM-only opening deck before pilot control. Continue
// restores the committed berth checkpoint and must not replay it; a cold reload
// during the deck therefore skips it rather than resuming (at-most-once).
// Emulator/host-harness only: startNewVoyage rewrites the cart save slot.
bool openingPlayback() {
    resetRun();
    s_screen = Screen::Title;
    startNewVoyage();
    bool ok = s_screen == Screen::Story && s_pendingStory == story_view::kSceneOpening && s_storyReturnPilot == 1;
    ok = story_view::cardCount(story_view::kSceneOpening) == 2 && ok;
    ok = story_view::isImageCard(story_view::kSceneOpening, 0) && ok;
    ok = !story_view::isImageCard(story_view::kSceneOpening, 1) && ok;
    for (uint8_t card = 0; card < story_view::cardCount(story_view::kSceneOpening); ++card)
        advanceStory();
    ok = s_screen == Screen::Pilot && s_pendingStory == story_view::kSceneNone && ok;

    // The saved new-voyage record has no pending deck: Continue enters the
    // Tidewall board instead of replaying the opening.
    resetRun();
    s_screen = Screen::Title;
    continueVoyage();
    ok = s_screen == Screen::Station && s_pendingStory == story_view::kSceneNone && ok;
    ok = s_state.pendingStory == story_view::kSceneNone && ok;
    return ok;
}

} // namespace

void setup() {
    Serial.begin(9600);
    boot_driver::boot();
    music::setCue(music_logic::kHarbor);
    music::resume();
    music::service();
    const bool pixelsOk = pixels();
    const bool playbackOk = playback();
    const bool noticeOk = noticeBacking();
    const bool reloadOk = coldReload();
    const bool handoffOk = handoffPlayback();
    const bool openingOk = openingPlayback();
    Serial.print(F("story_pixels=")); Serial.println(pixelsOk ? STORY_BEAT_FRAMES : 0);
    Serial.print(F("story_playback=")); Serial.println(playbackOk ? 1 : 0);
    Serial.print(F("story_notice_backing=")); Serial.println(noticeOk ? 1 : 0);
    Serial.print(F("story_cold_reload=")); Serial.println(reloadOk ? 1 : 0);
    Serial.print(F("story_handoffs=")); Serial.println(handoffOk ? 1 : 0);
    Serial.print(F("story_opening=")); Serial.println(openingOk ? 1 : 0);
    Serial.print(F("story_max_us=")); Serial.println(s_storyMaxUs);
    Serial.println(pixelsOk && playbackOk && noticeOk && reloadOk && handoffOk && openingOk ? 'P' : 'F');
}

void loop() {}

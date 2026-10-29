// Actual combined-game stack probe: full campaign/save plus the sibling
// streaming player and real Timer3 ISR. State, FX tables and the tile cache are
// static exactly as in the shipped sketch; paint the free stack, run six cues,
// world/cart/OLED frames and every story panel, then report the reserve.

#ifndef ATM_FX_COMPACT_DIAG
#define ATM_FX_COMPACT_DIAG 0
#endif

#include <Arduboy2.h>
#include <ArduboyFX.h>
#include <avr/io.h>

#include "src/common.hpp"
#include "src/features.hpp"

#if SH_FEATURE_MUSIC

#include "src/campaign.hpp"
#include "src/campaign_view.hpp"
#include "src/work_view.hpp"
#include "src/ending_view.hpp"
#include "src/game.hpp"
#include "src/hud.hpp"
#include "src/music.hpp"
#include "src/pilot.hpp"
#include "src/roster.hpp"
#include "src/render.hpp"
#include "src/station.hpp"
#include "src/voyage_flow.hpp"
#include "src/voyage_view.hpp"
#include "src/world_fx.hpp"
#if SH_FEATURE_FOG_CHART
#include "src/chart.hpp"
#endif

Arduboy2Base arduboy;

extern "C" char __bss_end;

namespace {

constexpr uint16_t kReserveTarget = 600;
constexpr uint8_t kPattern = 0xA5;
constexpr uint8_t kFrameGuard = 64;

static game::State s_state;
static world::FxTables s_tables;
static world::TileCache s_cache;
static bool s_saveChainOk = false;
static bool s_audioAfterSaveOk = false;
#if SH_FEATURE_FOG_CHART
static chart::State s_chart;
#endif

uint16_t readStackPointer() {
    uint8_t low;
    uint8_t high;
    asm volatile("in %0, %1" : "=r"(low) : "I"(_SFR_IO_ADDR(SPL)));
    asm volatile("in %0, %1" : "=r"(high) : "I"(_SFR_IO_ADDR(SPH)));
    return static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8);
}

void worldFrame(uint8_t frame) {
    arduboy.pollButtons();
    game::Input input;
    input.up = false;
    input.down = false;
    input.left = false;
    input.right = (frame & 1) != 0;
    station::stepFlight(s_state, input, s_tables, &s_cache);
    music::service();
    view::frame(s_state, pilot_view::cameraForState(s_state), s_cache, s_tables);
    hud::draw(s_state);
    music::service();
    FX::display(CLEAR_BUFFER);
    music::service();
}

void storyPanels() {
#if SH_FEATURE_CAMPAIGN
    game::resetWorld(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kCormorant);
    s_state.tools = static_cast<uint8_t>(exploration_data::SEAL);
    s_state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon);
    s_state.surveys = campaign::kSurveyMask;
    work_view::draw(s_state, 0);
    music::service();
    FX::display(CLEAR_BUFFER);
    music::service();
    music::service();
    FX::display(CLEAR_BUFFER);
    music::service();
    for (uint8_t card = 0; card < ending_view::kCardCount; ++card) {
        ending_view::draw(card, s_state);
        music::service();
        FX::display(CLEAR_BUFFER);
        music::service();
    }
#endif
#if SH_FEATURE_SAVE_VOYAGE
    voyage_flow::BootStatus boot;
    boot.state = voyage_flow::kBootValid;
    boot.hasValid = true;
    boot.hasCorrupt = false;
    boot.hasIncompatible = false;
#if SH_FEATURE_MULTI_HULL
    const uint8_t titleHull = s_state.hullClass;
#else
    const uint8_t titleHull = static_cast<uint8_t>(roster::kDefault);
#endif
    voyage_view::drawTitle(boot, voyage_view::kTitleContinue, titleHull, voyage_view::kTitleMenu);
    music::service();
    FX::display(CLEAR_BUFFER);
    music::service();
    voyage_view::drawPauseRescue(1, true);
    music::service();
    FX::display(CLEAR_BUFFER);
    music::service();
    voyage_view::drawSaveFailed(1);
    music::service();
    FX::display(CLEAR_BUFFER);
    music::service();
#endif
}

uint16_t measureReserve() {
    game::resetWorld(s_state);
    s_state.docked = -1;
    s_state.x = 600;
    s_state.y = 300;
    s_state.cameraXQ4 = game::cameraPixelToQ4(600);
    s_state.cameraYQ4 = game::cameraPixelToQ4(300);
    world::clearCache(s_cache);
    music::setCue(music_logic::kHarbor);

    // Sustained undocked movement: refresh the collision cache and sample the
    // region plane while the real audio ISR runs.
    for (uint8_t frame = 0; frame < 60; ++frame) {
        arduboy.pollButtons();
        game::Input input;
        input.up = false;
        input.down = false;
        input.left = (frame % 60) >= 30;
        input.right = (frame % 60) < 30;
        station::stepFlight(s_state, input, s_tables, &s_cache);
        music::updateFlight(s_state);
        music::service();
        view::frame(s_state, pilot_view::cameraForState(s_state), s_cache, s_tables);
        hud::draw(s_state);
        music::service();
        FX::display(CLEAR_BUFFER);
        music::service();
    }

    const uint8_t sreg = SREG;
    cli();
    char *const base = &__bss_end + 1;
    const uint16_t sp = readStackPointer();
    char *const top = reinterpret_cast<char *>(sp - kFrameGuard);
    for (char *p = base; p < top; ++p)
        *p = static_cast<char>(kPattern);
    SREG = sreg;

    for (uint8_t cue = 0; cue < music::backend::kCueCount; ++cue) {
        music::setCue(cue);
        for (uint8_t frame = 0; frame < 3; ++frame) {
            worldFrame(frame);
            delay(12);
        }
    }
    music::pause();
    music::service();
    delay(10);
    music::resume();
    music::service();
    storyPanels();
#if SH_FEATURE_SAVE_VOYAGE
    // Production chain from a drawn frame while music is active: pause the
    // stream, erase/program/validate the real cart slot, resume, then service
    // the ring and check underrun/error counters.
    game::reset(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kTidewall);
#if SH_FEATURE_FOG_CHART
    chart::reset(s_chart);
#endif
    hud::draw(s_state);
    FX::display(CLEAR_BUFFER);
    music::service();
    voyage_save::FxStorage storage;
    const music_logic::PauseToken saveToken = music::pauseForTransaction();
    const voyage_save::SaveResult saveResult = voyage_flow::startNewVoyage(s_state
#if SH_FEATURE_FOG_CHART
                                                                           ,
                                                                           s_chart
#endif
                                                                           ,
                                                                           storage);
    music::resumeAfterTransaction(saveToken);
    s_saveChainOk = saveResult == voyage_save::kSaved;
    for (uint8_t frame = 0; frame < 20; ++frame) {
        music::service();
        delay(4);
    }
#if ATM_FX_COMPACT_DIAG
    s_audioAfterSaveOk = !music::underrun() && !music::error();
#else
    s_audioAfterSaveOk = true;
#endif
#endif
    music::stop();

    const uint8_t scanSreg = SREG;
    cli();
    char *first = base;
    while (first < top && *first == static_cast<char>(kPattern))
        ++first;
    const uint16_t reserve = static_cast<uint16_t>(first - &__bss_end);
    SREG = scanSreg;
    return reserve;
}

}   // namespace

void setup() {
    Serial.begin(9600);
    arduboy.boot();
    arduboy.audio.begin();
    delay(100);
#if SH_FEATURE_SAVE_VOYAGE
    FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE);
#else
    FX::begin(FX_DATA_PAGE);
#endif
    FX::setCursorRange(0, 32767);
    music::begin();
    const uint16_t reserve = measureReserve();
    Serial.print(F("stack_reserve="));
    Serial.println(reserve);
    Serial.print(F("save_chain="));
    Serial.println(s_saveChainOk ? 1 : 0);
    Serial.print(F("audio_after_save="));
    Serial.println(s_audioAfterSaveOk ? 1 : 0);
#if ATM_FX_COMPACT_DIAG
    Serial.print(F("save_underruns="));
    Serial.println(music::underrun() ? 1 : 0);
    Serial.print(F("save_errors="));
    Serial.println(music::error() ? 1 : 0);
#endif
    Serial.println(reserve >= kReserveTarget && s_saveChainOk && s_audioAfterSaveOk ? 'P' : 'F');
}

void loop() {
}

#else

Arduboy2Base arduboy;

void setup() {
    Serial.begin(9600);
    Serial.println(F("music disabled / skipped"));
    Serial.println('P');
}

void loop() {
}

#endif

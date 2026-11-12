// Device/emulator integration suite for the production ATMlib2FX music
// boundary.
//
// Exercises the real compact synth Timer3 ISR/Timer4 carrier, the game music
// controller, all six authored C1 cue streams, world/cart/OLED frames, idle
// menu refills, chart continuity, pause freeze, nested save/rescue pause
// tokens, MUSIC ON/OFF persistence and the service-gap/underrun diagnostics.

#include <Arduboy2.h>
#include <ArduboyFX.h>
#include <avr/io.h>
#include <EEPROM.h>

#include "src/common.hpp"


#include "src/campaign.hpp"
#include "src/campaign_view.hpp"
#include "src/work_view.hpp"
#include "src/chart.hpp"
#include "src/chart_view.hpp"
#include "src/ending_view.hpp"
#include "src/game.hpp"
#include "src/hud.hpp"
#include "src/music.hpp"
#include "src/pilot.hpp"
#include "src/render.hpp"
#include "src/voyage_flow.hpp"
#include "src/voyage_view.hpp"
#include "src/world_fx.hpp"
#include "src/music_diag.hpp"

Arduboy2Base arduboy;

namespace {

constexpr uint16_t kGapLimitTicks = 8;
constexpr uint8_t kFramesPerCue = 3;

static game::State s_state;
static world::FxTables s_tables;
static world::TileCache s_cache;
static uint16_t s_gapMax = 0;
static uint16_t s_serviceMaxUs = 0;

// A changed policy cue is insufficient: verify the synth actually loaded its
// unique FX stream. This catches controller early-outs that leave old music on.
bool playingCue(uint8_t cue) {
    const uint32_t offset = music::backend::cueOffset(cue);
    return music::active() && atm_fx_compact_state.stream_base == offset &&
           atm_fx_compact_state.stream_end == offset + music::backend::cueLength(cue);
}

uint16_t timedService() {
    const uint32_t start = micros();
    music::service();
    const uint32_t elapsed = micros() - start;
    if (elapsed > s_serviceMaxUs)
        s_serviceMaxUs = static_cast<uint16_t>(elapsed);
    return music_diag::maxGapTicks();
}

void noteGap(uint16_t gap) {
    if (gap > s_gapMax)
        s_gapMax = gap;
}

void worldFrame(uint8_t frame) {
    arduboy.pollButtons();
    game::Input input;
    input.up = false;
    input.down = false;
    input.left = false;
    input.right = (frame & 1) != 0;
    game::stepWorld(s_state, input, s_tables, &s_cache);
    noteGap(timedService());
    view::frame(s_state, pilot_view::cameraForState(s_state), s_cache, s_tables);
    hud::draw(s_state);
    noteGap(timedService());
    FX::display(CLEAR_BUFFER);
    noteGap(timedService());
}

bool eepromPrefixUntouched(const uint8_t *before) {
    for (uint8_t address = 0; address < 16; ++address) {
        if (EEPROM.read(address) != before[address])
            return false;
    }
    return true;
}

} // namespace

void setup() {
    Serial.begin(9600);
    arduboy.boot();
    arduboy.audio.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    uint8_t prefix[16];
    for (uint8_t address = 0; address < 16; ++address)
        prefix[address] = EEPROM.read(address);

    music::begin();
    // Normalise the one-byte user setting so repeated device runs are
    // deterministic regardless of the previous EEPROM state.
    if (!music::enabled())
        music::toggleEnabled();
    game::resetWorld(s_state);
    world::clearCache(s_cache);

    bool ok = true;
    for (uint8_t cue = 0; cue < music::backend::kCueCount; ++cue) {
        music::setCue(cue);
        ok = playingCue(cue) && music::currentCue() == cue && ok;
        for (uint8_t frame = 0; frame < kFramesPerCue; ++frame) {
            noteGap(timedService());
            delay(6);
            noteGap(timedService());
            worldFrame(frame);
            // Force the general terrain path (sub-byte X and sub-page Y) so
            // the music gap also covers its extra 32-byte tail bursts.
            view::frame(s_state, cave::cameraAtPixel(static_cast<int16_t>(1001 + cue * 7), static_cast<int16_t>(733 + cue * 5)), s_cache, s_tables);
            noteGap(timedService());
        }
        ok = !music::error() && !music_diag::underrun() && ok;
    }

    noteGap(timedService());
    FX::display(CLEAR_BUFFER);
    noteGap(timedService());

    // Story/menu panels are large FX bursts while music stays active.
    game::resetWorld(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kCormorant);
    s_state.tools = static_cast<uint8_t>(exploration_data::SEAL);
    s_state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon);
    s_state.surveys = campaign::kSurveyMask;
    work_view::draw(s_state, 0);
    noteGap(timedService());
    FX::display(CLEAR_BUFFER);
    noteGap(timedService());
    for (uint8_t card = 0; card < ending_view::kCardCount; ++card) {
        ending_view::draw(card);
        noteGap(timedService());
        FX::display(CLEAR_BUFFER);
        noteGap(timedService());
    }
    voyage_view::drawTitle(voyage_flow::kBootWritten, voyage_view::kTitleContinue, voyage_view::kTitleMenu);
    noteGap(timedService());
    FX::display(CLEAR_BUFFER);
    noteGap(timedService());
    voyage_view::drawPauseRescue(1, true);
    noteGap(timedService());
    FX::display(CLEAR_BUFFER);
    noteGap(timedService());

    // The chart is a frozen world, not a frozen soundtrack. Compose real chart
    // bursts against the cart while the flight cue stays active; each redraw
    // must service the ring between water reads.
    bool mapContinues = true;
    music::setCue(music_logic::kHarbor);
    const uint8_t mapCue = music::currentCue();
    chart::State chartState;
    chart::reset(chartState);
    chart::revealAtPixels(chartState, s_state.x, s_state.y, 3);
    for (uint8_t phase = 0; phase < 4; ++phase) {
        chart_view::draw(Arduboy2Base::sBuffer, chartState, s_state, s_tables, static_cast<uint8_t>(phase * chart_view::kBlinkMask));
        noteGap(timedService());
        delay(6);
        FX::display(CLEAR_BUFFER);
        noteGap(timedService());
        mapContinues = mapContinues && !music::paused() && music::active() && music::currentCue() == mapCue;
    }

    // Pause freezes the cue; a nested save transaction must not resume it.
    music::pause();
    const bool pausedBefore = music::paused();
    const music_logic::PauseToken token = music::pauseForTransaction();
    music::resumeAfterTransaction(token);
    const bool stillPaused = music::paused();
    music::resume();
    const bool resumed = !music::paused();

    // Rescue-cancel and successful-recovery cue starts.
    music::setCue(music_logic::kHarbor);
    music::pause();
    const music_logic::PauseToken rescueToken = music::pauseForTransaction();
    music::resumeAfterTransaction(rescueToken);
    const bool rescueCancelPaused = music::paused();
    music::setCue(music_logic::kTidepool);
    const bool frozenSwitch = music::paused() && music::currentCue() == music_logic::kTidepool;
    music::resume();
    const bool rescueSuccess = music::active() && music::currentCue() == music_logic::kTidepool && !music::paused();
    music::setCue(music_logic::kTidepool);

    // Idle menu refill: no render and no OLED transaction.
    for (uint8_t idle = 0; idle < 8; ++idle) {
        noteGap(timedService());
        delay(8);
    }

    // Actual backend transitions: same-cue continuity, station->flight
    // hysteresis, pause/resume and MUSIC OFF/ON.
    music::setCue(music_logic::kHarbor);
    for (uint8_t tick = 0; tick < 8; ++tick) {
        delay(6);
        noteGap(timedService());
    }
    music::pause();
    const uint32_t samePosition = atm_fx_compact_state.head_offset;
    const uint32_t sameNext = atm_fx_compact_state.next_offset;
    music::setCue(music_logic::kHarbor);
    const bool sameCue = playingCue(music_logic::kHarbor) && music::paused() &&
                         atm_fx_compact_state.head_offset == samePosition &&
                         atm_fx_compact_state.next_offset == sameNext;
    music::resume();
    music::beginFlight(s_state);
    s_state.x = 168 * 8 + 4;   // Kelp Cathedral
    s_state.y = 18 * 8 + 4;
    for (uint8_t frame = 0; frame < 130; ++frame)
        music::updateFlight(s_state);
    const bool flightSwitched = music::currentCue() == music_logic::kKelp && playingCue(music_logic::kKelp);

    // Dock override resets the pending flight candidate; undocking must not
    // inherit the old hysteresis count and switch immediately.
    music::overrideCue(music_logic::kTidepool);
    const bool dockedOverride = music::currentCue() == music_logic::kTidepool && playingCue(music_logic::kTidepool);
    for (uint8_t frame = 0; frame < 120; ++frame)
        music::updateFlight(s_state);
    const bool dockedHold = music::currentCue() == music_logic::kTidepool && playingCue(music_logic::kTidepool);
    music::updateFlight(s_state);
    const bool undockedSwitchReset = music::currentCue() == music_logic::kKelp && playingCue(music_logic::kKelp);

    music::pause();
    const bool pausedBackend = music::paused();
    music::resume();
    const bool resumedBackend = !music::paused();
    const bool enabledBefore = music::enabled();
    music::toggleEnabled();
    const bool offSilent = EEPROM.read(music::kSettingAddress) == music::kSettingOff && !music::active();
    music::setCue(music_logic::kTidepool);
    const bool offNoStart = !music::active();
    music::toggleEnabled();
    const bool onStarts = music::enabled() && playingCue(music_logic::kTidepool) && music::currentCue() == music_logic::kTidepool;
    ok = ok && sameCue && flightSwitched && dockedOverride && dockedHold && undockedSwitchReset && pausedBackend && resumedBackend && rescueCancelPaused && rescueSuccess && offSilent && offNoStart && onStarts && (music::enabled() == enabledBefore);
    ok = ok && mapContinues;

    // MUSIC ON/OFF persists outside the voyage slots/system prefix.
    const bool beforeToggle = music::enabled();
    music::toggleEnabled();
    const bool offPersisted = EEPROM.read(music::kSettingAddress) == music::kSettingOff;
    music::toggleEnabled();
    const bool onPersisted = EEPROM.read(music::kSettingAddress) == music::kSettingOn;
    const bool toggleRestored = music::enabled() == beforeToggle;

    // Device audio mute is honored without writing the system setting.
    const uint8_t systemAudio = EEPROM.read(2);
    arduboy.audio.off();
    music::refreshMute();
    const bool deviceMuted = music::muted() && music::paused() == false;
    arduboy.audio.on();
    music::refreshMute();
    const bool deviceUnmuted = !music::muted() && music::active();
    const bool systemAudioUntouched = EEPROM.read(2) == systemAudio;

    music::setCue(music_logic::kHarbor);
    noteGap(timedService());
    music::stop();

    const bool gapOk = s_gapMax <= kGapLimitTicks;
    const bool clean = !music::error() && !music_diag::underrun();
    ok = ok && pausedBefore && stillPaused && resumed && frozenSwitch && offPersisted && onPersisted && toggleRestored;
    ok = ok && deviceMuted && deviceUnmuted && systemAudioUntouched && eepromPrefixUntouched(prefix) && gapOk && clean;

    Serial.print(F("music gap="));
    Serial.print(s_gapMax);
    Serial.print(F(" svc_us="));
    Serial.print(s_serviceMaxUs);
    Serial.print(F(" underruns="));
    Serial.print(music_diag::underrun() ? 1 : 0);
    Serial.print(F(" errors="));
    Serial.println(music::error() ? 1 : 0);
    Serial.print(F(" ok="));
    Serial.print(ok ? 1 : 0);
    Serial.print(F(" same="));
    Serial.print(sameCue ? 1 : 0);
    Serial.print(F(" flight="));
    Serial.print(flightSwitched ? 1 : 0);
    Serial.print(F(" pause="));
    Serial.print(pausedBackend && resumedBackend ? 1 : 0);
    Serial.print(F(" offon="));
    Serial.print(offSilent && offNoStart && onStarts ? 1 : 0);
    Serial.print(F(" persisted="));
    Serial.print(offPersisted && onPersisted && toggleRestored ? 1 : 0);
    Serial.print(F(" dock="));
    Serial.print(dockedOverride && dockedHold && undockedSwitchReset ? 1 : 0);
    Serial.print(F(" rescue="));
    Serial.print(rescueCancelPaused && rescueSuccess ? 1 : 0);
    Serial.print(F(" map="));
    Serial.print(mapContinues ? 1 : 0);
    Serial.print(F(" dev="));
    Serial.print(deviceMuted && deviceUnmuted && systemAudioUntouched ? 1 : 0);
    Serial.print(F(" prefix="));
    Serial.print(eepromPrefixUntouched(prefix) ? 1 : 0);
    Serial.print(F(" gapok="));
    Serial.print(gapOk ? 1 : 0);
    Serial.print(F(" clean="));
    Serial.println(clean ? 1 : 0);
    Serial.println(ok ? 'P' : 'F');
}


void loop() {}

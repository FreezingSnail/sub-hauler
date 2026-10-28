// Device/emulator integration suite for the streaming music boundary.
//
// Exercises the sibling ArduboyStreamFX Timer3 ISR/Timer4 carrier, the game
// music controller, all six cue descriptors, world/cart/OLED frames, idle
// menu refills, chart continuity, pause freeze, nested save/rescue pause
// tokens, MUSIC ON/OFF persistence and the service-gap/underrun diagnostics.

#define STREAMFX_DIAG 1

#include <Arduboy2.h>
#include <ArduboyFX.h>
#include <avr/io.h>
#include <EEPROM.h>

#include "src/common.hpp"
#include "src/features.hpp"

#if SH_FEATURE_MUSIC

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

Arduboy2Base arduboy;

namespace {

constexpr uint16_t kGapLimitTicks = 128; // 16 ms at 8 kHz
constexpr uint8_t kFramesPerCue = 3;
constexpr uint16_t kIsrSanityTicks = 100;

static game::State s_state;
static world::FxTables s_tables;
static world::TileCache s_cache;
static const streamfx::Stream s_shortLoop PROGMEM = {audio_harbor_bubbles, 512, 33, 0, 6};

uint16_t crc16(uint16_t crc, int16_t sample) {
    crc ^= static_cast<uint16_t>(sample);
    for (uint8_t bit = 0; bit < 8; ++bit)
        crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001)
                        : static_cast<uint16_t>(crc >> 1);
    return crc;
}

void worldFrame(uint8_t frame) {
    arduboy.pollButtons();
    game::Input input;
    input.up = false;
    input.down = false;
    input.left = false;
    input.right = (frame & 1) != 0;
    game::stepWorld(s_state, input, s_tables, &s_cache);
    music::service();
    view::frame(s_state, pilot_view::cameraForState(s_state), s_cache, s_tables);
    hud::draw(s_state);
    music::service();
    FX::display(CLEAR_BUFFER);
    music::service();
}

bool shortLoopCheck() {
    // The deterministic decoder pass owns the sample consumer: disable the
    // real Timer3 ISR for this short FX-backed loop, then restore interrupts.
    const uint8_t sreg = SREG;
    cli();
    const bool started = streamfx::start(s_shortLoop);
    uint16_t first = 0xFFFF;
    uint16_t second = 0xFFFF;
    if (started) {
        for (uint16_t i = 0; i < 1024; ++i) {
            if ((i % 120) == 0)
                streamfx::service();
            const int16_t sample = streamfx::g_player.tick();
            if (i < 512)
                first = crc16(first, sample);
            else
                second = crc16(second, sample);
        }
        streamfx::stop();
    }
    SREG = sreg;
    return started && first == second;
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

    bool ok = shortLoopCheck();
    // The direct decoder check runs with interrupts disabled; discard its
    // wall-clock service sample and measure only the real-ISR playback path.
    streamfx::diagTicks = 0;
    streamfx::diagUnderruns = 0;
    streamfx::diagErrors = 0;
    streamfx::diagMaxGapTicks = 0;
    streamfx::diagMaxServiceUs = 0;
    streamfx::diagMaxIsrTicks = 0;
    streamfx::clearUnderrun();

    for (uint8_t cue = 0; cue < game_audio::kStreamCount; ++cue) {
        music::setCue(cue);
        ok = streamfx::active() && streamfx::streamId() == cue && ok;
        for (uint8_t frame = 0; frame < kFramesPerCue; ++frame) {
            music::service();
            delay(6);
            music::service();
            worldFrame(frame);
            music::service();
        }
        ok = !streamfx::error() && !streamfx::underrun() && ok;
    }

    music::service();
    FX::display(CLEAR_BUFFER);
    music::service();
    const uint16_t gapDisplay = streamfx::diagMaxGapTicks;

    // Story/menu panels are large FX bursts while music stays active.
#if SH_FEATURE_CAMPAIGN
    game::resetWorld(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kCormorant);
    s_state.tools = static_cast<uint8_t>(exploration_data::SEAL);
    s_state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon);
    s_state.surveys = campaign::kSurveyMask;
    work_view::draw(s_state, 0);
    music::service();
    const uint16_t gapWorklist = streamfx::diagMaxGapTicks;
    FX::display(CLEAR_BUFFER);
    music::service();
    const uint16_t gapWorkDisplay = streamfx::diagMaxGapTicks;
    const uint16_t gapDetail = streamfx::diagMaxGapTicks;
    music::service();
    FX::display(CLEAR_BUFFER);
    music::service();
    for (uint8_t card = 0; card < ending_view::kCardCount; ++card) {
        ending_view::draw(card, s_state);
        const uint16_t gapCard = streamfx::diagMaxGapTicks; (void)gapCard;
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
    voyage_view::drawTitle(boot, voyage_view::kTitleContinue, s_state.hullClass, voyage_view::kTitleMenu);
    const uint16_t gapTitle = streamfx::diagMaxGapTicks; (void)gapTitle;
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

    const uint16_t gapAfterCues = streamfx::diagMaxGapTicks;

    // The chart is a frozen world, not a frozen soundtrack. Compose real chart
    // bursts against the cart while the flight cue stays active; each redraw
    // must service the ring between water reads.
    bool mapContinues = true;
    uint16_t gapAfterMap = gapAfterCues;
#if SH_FEATURE_FOG_CHART
    music::setCue(music_logic::kHarbor);
    const uint8_t mapCue = streamfx::streamId();
    chart::State chartState;
    chart::reset(chartState);
    chart::revealAtPixels(chartState, s_state.x, s_state.y, 3);
    for (uint8_t phase = 0; phase < 4; ++phase) {
        chart_view::draw(Arduboy2Base::sBuffer, chartState, s_state, s_tables, static_cast<uint8_t>(phase * chart_view::kBlinkMask));
        music::service();
        delay(6);
        FX::display(CLEAR_BUFFER);
        music::service();
        mapContinues = mapContinues && !music::paused() && streamfx::active() && streamfx::streamId() == mapCue;
    }
    gapAfterMap = streamfx::diagMaxGapTicks;
#endif

    // Pause freezes the cue; a nested save transaction must not resume it.
    music::pause();
    const uint16_t gapAfterPanels = streamfx::diagMaxGapTicks;
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
    const bool frozenSwitch = music::paused() && streamfx::streamId() == music_logic::kTidepool;
    music::resume();
    const bool rescueSuccess = streamfx::active() && streamfx::streamId() == music_logic::kTidepool && !streamfx::paused();
    music::setCue(music_logic::kTidepool);

    // Idle menu refill: no render and no OLED transaction.
    for (uint8_t idle = 0; idle < 8; ++idle) {
        music::service();
        delay(8);
    }

    // Actual backend transitions: same-cue continuity, station->flight
    // hysteresis, pause/resume and MUSIC OFF/ON.
    music::setCue(music_logic::kHarbor);
    const uint8_t sameBefore = streamfx::streamId();
    music::setCue(music_logic::kHarbor);
    const bool sameCue = streamfx::active() && streamfx::streamId() == sameBefore;
    music::beginFlight(s_state);
    s_state.x = 168 * 8 + 4;   // Kelp Cathedral
    s_state.y = 18 * 8 + 4;
    for (uint8_t frame = 0; frame < 130; ++frame)
        music::updateFlight(s_state);
    const bool flightSwitched = streamfx::streamId() == music_logic::kKelp;

    // Dock override resets the pending flight candidate; undocking must not
    // inherit the old hysteresis count and switch immediately.
    music::overrideCue(music_logic::kTidepool);
    const bool dockedOverride = streamfx::streamId() == music_logic::kTidepool;
    for (uint8_t frame = 0; frame < 120; ++frame)
        music::updateFlight(s_state);
    const bool dockedHold = streamfx::streamId() == music_logic::kTidepool;
    music::updateFlight(s_state);
    const bool undockedSwitchReset = streamfx::streamId() == music_logic::kKelp;

    music::pause();
    const bool pausedBackend = streamfx::paused();
    music::resume();
    const bool resumedBackend = !streamfx::paused();
    const bool enabledBefore = music::enabled();
    music::toggleEnabled();
    const bool offSilent = EEPROM.read(music::kSettingAddress) == music::kSettingOff && !streamfx::active();
    music::setCue(music_logic::kTidepool);
    const bool offNoStart = !streamfx::active();
    music::toggleEnabled();
    const bool onStarts = music::enabled() && streamfx::active() && streamfx::streamId() == music_logic::kTidepool;
    ok = ok && sameCue && flightSwitched && dockedOverride && dockedHold && undockedSwitchReset && pausedBackend && resumedBackend && rescueCancelPaused && rescueSuccess && offSilent && offNoStart && onStarts && (music::enabled() == enabledBefore);
    ok = ok && mapContinues;

    const uint16_t gapAfterIdle = streamfx::diagMaxGapTicks;

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
    const bool deviceMuted = streamfx::muted();
    arduboy.audio.on();
    music::refreshMute();
    const bool deviceUnmuted = !streamfx::muted();
    const bool systemAudioUntouched = EEPROM.read(2) == systemAudio;

    music::setCue(music_logic::kHarbor);
    music::service();
    music::stop();

    const bool gapOk = streamfx::diagMaxGapTicks <= kGapLimitTicks;
    const bool isrOk = streamfx::diagMaxIsrTicks <= kIsrSanityTicks;
    const bool clean = streamfx::diagUnderruns == 0 && streamfx::diagErrors == 0;
    ok = ok && pausedBefore && stillPaused && resumed && frozenSwitch && offPersisted && onPersisted && toggleRestored;
    ok = ok && deviceMuted && deviceUnmuted && systemAudioUntouched && eepromPrefixUntouched(prefix) && gapOk && isrOk && clean;

    Serial.print(F("music gap="));
    Serial.print(streamfx::diagMaxGapTicks);
    Serial.print(F(" svc_us="));
    Serial.print(streamfx::diagMaxServiceUs);
    Serial.print(F(" isr_ticks="));
    Serial.print(streamfx::diagMaxIsrTicks);
    Serial.print(F(" underruns="));
    Serial.print(streamfx::diagUnderruns);
    Serial.print(F(" errors="));
    Serial.println(streamfx::diagErrors);
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
    Serial.print(F(" stages="));
    Serial.print(gapDisplay);
    Serial.print('/');
    Serial.print(gapWorklist);
    Serial.print('/');
    Serial.print(gapWorkDisplay);
    Serial.print('/');
    Serial.print(gapDetail);
    Serial.print('/');
    Serial.print(gapAfterCues);
    Serial.print('/');
    Serial.print(gapAfterMap);
    Serial.print('/');
    Serial.print(gapAfterPanels);
    Serial.print('/');
    Serial.print(gapAfterIdle);
    Serial.print(F(" gapok="));
    Serial.print(gapOk ? 1 : 0);
    Serial.print(F(" isrok="));
    Serial.print(isrOk ? 1 : 0);
    Serial.print(F(" clean="));
    Serial.println(clean ? 1 : 0);
    Serial.println(ok ? 'P' : 'F');
}

#else

#include <Arduboy2.h>

Arduboy2Base arduboy;

void setup() {
    Serial.begin(9600);
    Serial.println(F("music disabled / skipped"));
    Serial.println('P');
}

#endif

void loop() {}

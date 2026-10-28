// Device/emulator admission suite for the sampled IMA ADPCM player.
//
// It exercises the real Timer3 8 kHz sample ISR, Timer4 187.5 kHz PWM carrier,
// FX refill, cold then cached world rendering/cart/OLED frames, idle menus,
// pause/resume across a modeled blocking EEPROM wait, all six cues, and exact
// short-loop playback. The cache is cleared first and no warm frame is used;
// the production `SH_AUDIO_PLAYER` tile-byte service point handles cold reads.
// Diagnostics print the measured service gap, foreground service time and ISR
// duration; the Ardens gate requires the final bare P line.

#define SH_AUDIO_DIAG 1
#define SH_AUDIO_PLAYER 1

#include <Arduboy2.h>
#include <ArduboyFX.h>
#include <avr/io.h>

#include "src/common.hpp"
#include "src/features.hpp"
#include "src/game.hpp"
#include "src/hud.hpp"
#include "src/pilot.hpp"
#include "src/render.hpp"
#include "src/world_fx.hpp"
#include "src/audio/sampled_player_avr.hpp"

Arduboy2Base arduboy;

static_assert(sizeof(audio::SampledPlayer) == 152,
              "player static state is exactly the 128 B ring + 24 B state");
static_assert(audio::kRingBytes == 128, "ring storage stays 128 B");

namespace {

constexpr uint16_t kGapLimitTicks = 128; // 16 ms at 8 kHz
constexpr uint8_t kFramesPerCue = 4;
constexpr uint16_t kIsrSanityTicks = 100; // 50 us at 2 MHz Timer3

static game::State s_state;
static world::FxTables s_tables;
static world::TileCache s_cache;

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
    audio::service();
    view::frame(s_state, pilot_view::cameraForState(s_state), s_cache, s_tables);
    hud::draw(s_state);
    audio::service();
    FX::display(CLEAR_BUFFER);
}

bool shortLoopCheck() {
    // A real FX-backed 512-sample loop: the first and second passes must
    // decode identically with the authored decoder reset at the boundary.
    cli();
    bool ok = audio::g_player.start(6, audio_harbor_bubbles, 256, 512, 33, 0);
    audio::fillRing();
    audio::g_player.enable();
    uint16_t first = 0xFFFF;
    uint16_t second = 0xFFFF;
    for (uint16_t i = 0; i < 1024; ++i) {
        if ((i % 120) == 0)
            audio::fillRing();
        const int16_t sample = audio::g_player.tick();
        if (i < 512)
            first = crc16(first, sample);
        else
            second = crc16(second, sample);
    }
    audio::g_player.stop();
    sei();
    return ok && first == second;
}

} // namespace

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);
    audio::begin();

    game::resetWorld(s_state);
    world::clearCache(s_cache);

    bool ok = shortLoopCheck();

    for (uint8_t cue = 0; cue < audio::kCueCount; ++cue) {
        cli();
        ok = audio::start(cue) && ok;
        uint16_t crc = 0xFFFF;
        for (uint16_t i = 0; i < 128; ++i)
            crc = crc16(crc, audio::g_player.tick());
        ok = (crc == pgm_read_word(&audio::kCheckCrc[cue])) && ok;
        sei();

        for (uint8_t frame = 0; frame < kFramesPerCue; ++frame) {
            audio::service();
            delay(6);
            audio::service();
            worldFrame(frame);
            audio::service();
        }
    }

    // Paused blocking wait: no service call for 120 ms while paused. The
    // diagnostic must resume its active-gap clock after resume, not charge it.
    audio::start(0);
    const uint16_t underrunsAfterCues = audio::diagUnderruns;
    audio::pause();
    delay(120);
    audio::resume();
    audio::service();
    const uint16_t underrunsAfterPause = audio::diagUnderruns;

    // Mute keeps position/decoder running but silences output.
    audio::setMuted(true);
    delay(10);
    audio::service();
    audio::setMuted(false);
    delay(10);
    audio::service();
    const uint16_t underrunsAfterMute = audio::diagUnderruns;

    // Cached idle menu path: no render and no OLED transaction, only refills.
    for (uint8_t idle = 0; idle < 8; ++idle) {
        audio::service();
        delay(8);
    }
    audio::service();
    const uint16_t underrunsAfterIdle = audio::diagUnderruns;
    audio::stop();

    const bool gapOk = audio::diagMaxGapTicks <= kGapLimitTicks;
    const bool isrOk = audio::diagMaxIsrTicks <= kIsrSanityTicks;
    const bool clean = audio::diagUnderruns == 0 && audio::diagErrors == 0;
    ok = ok && gapOk && isrOk && clean && !audio::g_player.active();

    Serial.print(F("audio gap="));
    Serial.print(audio::diagMaxGapTicks);
    Serial.print(F(" svc_us="));
    Serial.print(audio::diagMaxServiceUs);
    Serial.print(F(" isr_ticks="));
    Serial.print(audio::diagMaxIsrTicks);
    Serial.print(F(" underruns="));
    Serial.print(audio::diagUnderruns);
    Serial.print(F(" stages="));
    Serial.print(underrunsAfterCues);
    Serial.print('/');
    Serial.print(underrunsAfterPause);
    Serial.print('/');
    Serial.print(underrunsAfterMute);
    Serial.print('/');
    Serial.print(underrunsAfterIdle);
    Serial.print(F(" errors="));
    Serial.println(audio::diagErrors);
    Serial.println(ok ? 'P' : 'F');
}

void loop() {}

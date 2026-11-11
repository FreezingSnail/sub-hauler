// Complete-flight timing probe with active music.
//
// Includes the real production sketch and drives its shipping flight path:
// updateFlight() (music cue/state, real stepFlight, damage), the
// production drawPilot() (scene + HUD), FX::display(CLEAR_BUFFER) and
// music::service() around a real arduboy.nextFrame() 16 ms scheduler. Timing
// starts after the scheduler returns, so frame work is measured without the
// idle wait; the actual interval is still the shipping 16 ms cadence.
//
// Coverage is asserted per case: eight Y remainders over cold and reused
// caches, six cues over cold/warm fractional and page-aligned paths, and
// steady/diagonal/reverse flight over realistic tick runs. Fractional page
// hold, world camera boundaries and notice selection have dedicated coverage in
// test_camera, test_render and the host HUD tests. Fixtures start at the
// generated Room Q navigation centre, in still water far from berths, and use
// bounded input runs so a dock cannot silently count as flight coverage.
//
// The suite is compiled with ATM_FX_COMPACT_DIAG=1, so its absolute times are
// instrumented device-suite values, not a shipping-ELF measurement. Checksums
// and all printing happen after the timed sections.

#ifndef ATM_FX_COMPACT_DIAG
#define ATM_FX_COMPACT_DIAG 1
#endif
#include <avr/io.h>

#include "src/app.hpp"
#include "src/music_diag.hpp"


extern "C" char __bss_end;

namespace {

constexpr uint8_t kStackGuardBytes = 64;
char *s_stackGuard = 0;
bool s_stackGuardOk = true;

constexpr uint32_t kFullFlightTargetUs = 13000;
constexpr uint32_t kHardDeadlineUs = 16000;
constexpr uint16_t kGapLimitTicks = 8;
uint16_t s_musicGapMax;
bool s_musicError;
bool s_musicUnderrun;
uint32_t s_frames;
uint32_t s_maxFrameUs;
uint16_t s_deadlineMisses;
uint16_t s_coldFrames;
uint8_t s_remainderMask;
uint8_t s_cueMask;
uint8_t s_previousButtons;
uint16_t s_failMask = 0;

void fail(uint16_t bit) {
    s_failMask |= bit;
}

void emit(const char *text) {
    for (;;) {
        const char c = static_cast<char>(pgm_read_byte(text++));
        if (!c)
            return;
        Serial.write(static_cast<uint8_t>(c));
    }
}

void emitNumber(uint32_t value) {
    char digits[10];
    uint8_t count = 0;
    do {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value);
    while (count)
        Serial.write(static_cast<uint8_t>(digits[--count]));
}

void record(uint32_t frameUs) {
    ++s_frames;
    if (frameUs > s_maxFrameUs)
        s_maxFrameUs = frameUs;
    if (frameUs >= kHardDeadlineUs)
        ++s_deadlineMisses;
}

// Place the boat so the shipping follow target equals the requested camera
// origin at rest. The camera is snapped once; updateCamera() owns it after.
void placeAtCamera(int16_t cameraX, int16_t cameraY, bool clearCache) {
    game::resetWorld(s_state);
    s_state.x = static_cast<int16_t>(cameraX + game::kCameraScreenX);
    s_state.y = static_cast<int16_t>(cameraY + game::kCameraScreenY);
    s_state.xFrac = 0;
    s_state.yFrac = 0;
    s_state.vxQ8 = 0;
    s_state.vyQ8 = 0;
    s_state.air = game::airCapacity(s_state);
    s_state.docked = -1;
    s_state.dockCooldown = 255;
    game::snapCamera(s_state);
    s_screen = Screen::Pilot;
    s_present = true;
    s_previousButtons = 0;
    if (clearCache)
        world::clearCache(s_cache);
}

// One real shipping frame. nextFrame() owns the 16 ms cadence; the timed span
// starts after it returns and ends after the loop's final music service.
bool flightFrame(uint8_t buttons, bool cold, bool measure) {
    music::service();
    if (!arduboy.nextFrame()) {
        music::service();
        return false;
    }

    if (cold)
        world::clearCache(s_cache);

    const uint32_t frameStart = micros();
    arduboy.pollButtons();
    Arduboy2Base::previousButtonState = s_previousButtons;
    Arduboy2Base::currentButtonState = buttons;
    s_just = static_cast<uint8_t>(buttons & ~s_previousButtons);
    s_previousButtons = buttons;
    s_present = true;

    updateFlight();

    if (s_present)
        FX::display(CLEAR_BUFFER);
    music::service();
    const uint32_t frameUs = micros() - frameStart;

    if (s_screen != Screen::Pilot)
        fail(1);

    if (measure) {
        record(frameUs);
        if (cold)
            ++s_coldFrames;
    }
    return true;
}

void runFrames(uint8_t buttons, uint16_t count, bool cold, bool measure) {
    uint16_t done = 0;
    while (done < count) {
        if (flightFrame(buttons, cold, measure))
            ++done;
    }
}

void noteMusic() {
    if (music_diag::maxGapTicks() > s_musicGapMax)
        s_musicGapMax = music_diag::maxGapTicks();
    s_musicError = s_musicError || music::error();
    s_musicUnderrun = s_musicUnderrun || music_diag::underrun();
}

void setCue(uint8_t cue) {
    noteMusic();
    music::overrideCue(cue);
    music::resume();
    if (music::currentCue() != cue)
        fail(128);
    s_cueMask |= static_cast<uint8_t>(1u << cue);
}

void remainderCase(uint8_t remainder, bool cold) {
    const int16_t originX = static_cast<int16_t>(exploration_data::ROOM_Q_X - 44 + remainder);
    const int16_t originY = static_cast<int16_t>(exploration_data::ROOM_Q_Y - 32 + remainder);
    placeAtCamera(originX, originY, cold);
    setCue(static_cast<uint8_t>(remainder % music::backend::kCueCount));
    if (cold) {
        runFrames(0, 2, true, true);
    } else {
        runFrames(0, 1, false, false);
        runFrames(0, 33, false, true);
    }
    const cave::Camera camera = pilot_view::cameraForState(s_state);
    if ((camera.subY & 7) != remainder)
        fail(2);
    s_remainderMask |= static_cast<uint8_t>(1u << remainder);
}

void cueCase(uint8_t cue) {
    setCue(cue);
    // Cold and warm, fractional and page-aligned terrain paths.
    placeAtCamera(exploration_data::ROOM_Q_X - 44 + 3, exploration_data::ROOM_Q_Y - 32 + 5, true);
    runFrames(0, static_cast<uint8_t>(cue < 2 ? 2 : 1), true, true);
    runFrames(0, 1, false, false);
    runFrames(0, 1, false, true);
    placeAtCamera(exploration_data::ROOM_Q_X - 44, exploration_data::ROOM_Q_Y - 32, true);
    runFrames(0, 1, true, true);
    runFrames(0, 1, false, true);
    if (!music::active() || music::currentCue() != cue)
        fail(128);
}

void motionCase(uint8_t mode) {
    placeAtCamera(exploration_data::ROOM_Q_X - 44, exploration_data::ROOM_Q_Y - 32, false);
    runFrames(0, 1, false, false);
    const int16_t startX = s_state.x;
    const int16_t startY = s_state.y;

    if (mode == 0) {
        s_state.vxQ8 = game::kMaxVxBaseQ8;
        runFrames(RIGHT_BUTTON, 126, false, true);
        if (s_state.x - startX < 8)
            fail(4);
    } else if (mode == 1) {
        s_state.vxQ8 = game::kMaxVxBaseQ8;
        s_state.vyQ8 = game::kMaxVyQ8;
        runFrames(static_cast<uint8_t>(RIGHT_BUTTON | DOWN_BUTTON), 120, false, true);
        if (s_state.x - startX < 8 || s_state.y - startY < 4)
            fail(4);
    } else {
        s_state.vxQ8 = 0;
        for (uint8_t i = 0; i < 10; ++i) {
            while (!flightFrame(RIGHT_BUTTON, false, true)) {
            }
        }
        runFrames(LEFT_BUTTON, 120, false, true);
        runFrames(0, 160, false, true);
    }
    if (s_state.x == exploration_data::ROOM_Q_X && s_state.y == exploration_data::ROOM_Q_Y)
        fail(64);
}

} // namespace

void setup() {
    Serial.begin(9600);
    // The production hardware init is the same one used by the shipping
    // sketch. This build-only flight probe intentionally omits voyage save so
    // the USB test image fits; the flight frame path and music are unchanged.
    arduboy.boot();
    arduboy.audio.begin();
    arduboy.setFrameRate(60);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);
    text::begin();
    music::begin();
    resetRun();
    if (!music::enabled())
        music::toggleEnabled();
    music::resume();

    s_stackGuard = &__bss_end + 1;
    for (uint8_t i = 0; i < kStackGuardBytes; ++i)
        s_stackGuard[i] = static_cast<char>(0xA5);

    for (uint8_t r = 0; r < 8; ++r)
        remainderCase(r, true);
    for (uint8_t r = 0; r < 8; ++r)
        remainderCase(r, false);
    for (uint8_t cue = 0; cue < music::backend::kCueCount; ++cue)
        cueCase(cue);
    for (uint8_t mode = 0; mode < 3; ++mode)
        motionCase(mode);

    noteMusic();
    constexpr uint16_t kExpectedFrames = 842;
    constexpr uint16_t kExpectedCold = 30;
    constexpr uint16_t kExpectedWarm = 812;
    for (uint8_t i = 0; i < kStackGuardBytes; ++i) {
        if (s_stackGuard[i] != static_cast<char>(0xA5))
            s_stackGuardOk = false;
    }
    if (!s_stackGuardOk)
        s_failMask |= 1024;
    const bool coverageOk = s_remainderMask == 0xFF && s_cueMask == 0x3F &&
                            s_frames == kExpectedFrames && s_coldFrames == kExpectedCold &&
                            static_cast<uint16_t>(s_frames - s_coldFrames) == kExpectedWarm &&
                            s_stackGuardOk && s_failMask == 0;
    const bool gapOk = s_musicGapMax <= kGapLimitTicks;
    const bool musicOk = gapOk && !s_musicUnderrun && !s_musicError;
    const bool timingOk = s_maxFrameUs <= kFullFlightTargetUs && s_deadlineMisses == 0;

    emit(PSTR("frames="));
    emitNumber(s_frames);
    emit(PSTR(" max_us="));
    emitNumber(s_maxFrameUs);
    emit(PSTR(" misses16="));
    emitNumber(s_deadlineMisses);
    emit(PSTR(" gap="));
    emitNumber(s_musicGapMax);
    emit(PSTR(" underruns="));
    emitNumber(s_musicUnderrun ? 1 : 0);
    emit(PSTR(" errors="));
    emitNumber(s_musicError ? 1 : 0);
    emit(PSTR(" fail_mask="));
    emitNumber(s_failMask);
    Serial.write('\n');
    Serial.write(coverageOk && timingOk && musicOk ? 'P' : 'F');
    Serial.write('\n');
}

void loop() {}


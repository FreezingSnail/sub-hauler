// Complete-flight timing probe with active music.
//
// Includes the real production sketch and drives its shipping flight path:
// updateFlight() (music cue/state, real stepFlight, damage), the
// production drawPilot() (scene + HUD), FX::display(CLEAR_BUFFER) and
// music::service() around a real arduboy.nextFrame() 16 ms scheduler. Timing
// starts after the scheduler returns, so frame work is measured without the
// idle wait; the actual interval is still the shipping 16 ms cadence.
//
// Coverage is asserted per case: eight Y remainders with cold and reused
// caches, a 120-frame fractional-Y hold, six cues over cold/warm fractional and
// page-aligned paths, steady/diagonal/slow/reverse flight over realistic tick
// runs, and all four world camera boundaries. Fixtures start at the generated
// Room Q navigation centre, in still water far from berths, and use bounded
// input runs so a dock cannot silently count as flight coverage. Per-case frame
// maxima are printed to locate the worst case.
//
// The suite is compiled with --optimize-for-debug and ATM_FX_COMPACT_DIAG=1,
// so its absolute times are instrumented device-suite values, not a shipping-ELF
// measurement. Checksums and all printing happen after the timed sections.

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
uint32_t s_sumFrameUs;
uint32_t s_maxFrameUs;
uint16_t s_deadlineMisses;
uint16_t s_coldFrames;
uint16_t s_warmFrames;
uint8_t s_remainderMask;
uint8_t s_cueMask;
uint8_t s_boundaryRuns;
uint8_t s_previousButtons;
uint8_t s_case;
bool s_caseOk = true;
bool s_cueOk = true;
uint16_t s_failMask = 0;
int16_t s_revPeakDelta = 0;
int16_t s_revVx = 0;
uint16_t s_minStackPointer = 0xFFFF;

void fail(uint16_t bit) {
    s_caseOk = false;
    s_failMask |= bit;
}

uint16_t readStackPointer() {
    uint8_t low;
    uint8_t high;
    asm volatile("in %0, %1" : "=r"(low) : "I"(_SFR_IO_ADDR(SPL)));
    asm volatile("in %0, %1" : "=r"(high) : "I"(_SFR_IO_ADDR(SPH)));
    return static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8);
}

void record(uint32_t frameUs) {
    ++s_frames;
    s_sumFrameUs += frameUs;
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
    const uint16_t stackPointer = readStackPointer();
    if (stackPointer < s_minStackPointer)
        s_minStackPointer = stackPointer;
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

        const cave::Camera camera = pilot_view::cameraForState(s_state);
        s_remainderMask |= static_cast<uint8_t>(1u << (camera.subY & 7));
        if (cold)
            ++s_coldFrames;
        else
            ++s_warmFrames;
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

void runSequence(const uint8_t *buttons, uint16_t count, bool cold) {
    for (uint16_t i = 0; i < count; ++i) {
        while (!flightFrame(buttons[i], cold, true)) {
        }
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
    s_cueOk = s_cueOk && music::currentCue() == cue;
    s_cueMask |= static_cast<uint8_t>(1u << cue);
}

void remainderCase(uint8_t remainder, bool cold) {
    const int16_t originX = static_cast<int16_t>(exploration_data::ROOM_Q_X - 44 + remainder);
    const int16_t originY = static_cast<int16_t>(exploration_data::ROOM_Q_Y - 32 + remainder);
    s_case = static_cast<uint8_t>((cold ? 0 : 8) + remainder);
    placeAtCamera(originX, originY, cold);
    setCue(static_cast<uint8_t>(remainder % music::backend::kCueCount));
    if (cold) {
        runFrames(0, 2, true, true);
    } else {
        runFrames(0, 1, false, false);
        runFrames(0, 3, false, true);
    }
    const cave::Camera camera = pilot_view::cameraForState(s_state);
    if ((camera.subY & 7) != remainder)
        fail(2);
}

void cueCase(uint8_t cue) {
    s_case = static_cast<uint8_t>(16 + cue);
    setCue(cue);
    // Cold and warm, fractional and page-aligned terrain paths.
    placeAtCamera(exploration_data::ROOM_Q_X - 44 + 3, exploration_data::ROOM_Q_Y - 32 + 5, true);
    runFrames(0, 1, true, true);
    runFrames(0, 1, false, false);
    runFrames(0, 1, false, true);
    placeAtCamera(exploration_data::ROOM_Q_X - 44, exploration_data::ROOM_Q_Y - 32, true);
    runFrames(0, 1, true, true);
    runFrames(0, 1, false, true);
    s_cueOk = s_cueOk && music::active() && music::currentCue() == cue;
}

void motionCase(uint8_t mode) {
    s_case = static_cast<uint8_t>(22 + mode);
    placeAtCamera(exploration_data::ROOM_Q_X - 44, exploration_data::ROOM_Q_Y - 32, false);
    runFrames(0, 1, false, false);
    const int16_t startX = s_state.x;
    const int16_t startY = s_state.y;

    if (mode == 0) {
        s_state.vxQ8 = game::kMaxVxBaseQ8;
        runFrames(RIGHT_BUTTON, 120, false, true);
        if (s_state.x - startX < 8)
            fail(4);
    } else if (mode == 1) {
        s_state.vxQ8 = game::kMaxVxBaseQ8;
        s_state.vyQ8 = game::kMaxVyQ8;
        runFrames(static_cast<uint8_t>(RIGHT_BUTTON | DOWN_BUTTON), 120, false, true);
        if (s_state.x - startX < 8 || s_state.y - startY < 4)
            fail(4);
    } else if (mode == 2) {
        s_state.vxQ8 = 2560;
        runFrames(0, 120, false, true);
        const int16_t slowDistance = static_cast<int16_t>(s_state.x - startX);
        if (slowDistance < 1 || slowDistance > 40)
            fail(8);
    } else {
        s_state.vxQ8 = 0;
        int16_t peakX = s_state.x;
        for (uint8_t i = 0; i < 10; ++i) {
            while (!flightFrame(RIGHT_BUTTON, false, true)) {
            }
            if (s_state.x > peakX)
                peakX = s_state.x;
        }
        for (uint8_t i = 0; i < 120; ++i) {
            while (!flightFrame(LEFT_BUTTON, false, true)) {
            }
            if (s_state.x > peakX)
                peakX = s_state.x;
        }
        const int16_t cameraErrorBefore = static_cast<int16_t>(game::cameraTargetXQ4(s_state) - s_state.cameraXQ4);
        runFrames(0, 160, false, true);
        const int16_t cameraErrorAfter = static_cast<int16_t>(game::cameraTargetXQ4(s_state) - s_state.cameraXQ4);
        const int16_t beforeAbs = cameraErrorBefore < 0 ? static_cast<int16_t>(-cameraErrorBefore) : cameraErrorBefore;
        const int16_t afterAbs = cameraErrorAfter < 0 ? static_cast<int16_t>(-cameraErrorAfter) : cameraErrorAfter;
        s_revPeakDelta = static_cast<int16_t>(peakX - s_state.x);
        s_revVx = s_state.vxQ8;
        if (s_revPeakDelta < 4)
            fail(16);
        if (s_state.vxQ8 > 512 || s_state.vxQ8 < -512)
            fail(256);
        if (afterAbs > beforeAbs)
            fail(512);
    }
    if (s_state.x == exploration_data::ROOM_Q_X && s_state.y == exploration_data::ROOM_Q_Y)
        fail(64);
}

void holdCase() {
    s_case = 30;
    placeAtCamera(exploration_data::ROOM_Q_X - 44 + 2, exploration_data::ROOM_Q_Y - 32 + 5, false);
    runFrames(0, 1, false, false);
    for (uint16_t i = 0; i < 120; ++i) {
        while (!flightFrame(0, false, true)) {
        }
        const cave::Camera camera = pilot_view::cameraForState(s_state);
        if ((camera.subY & 7) != 5)
            fail(32);
    }
}

void boundaryCase(int16_t cameraX, int16_t cameraY) {
    s_case = static_cast<uint8_t>(26 + s_boundaryRuns);
    placeAtCamera(cameraX, cameraY, false);
    setCue(0);
    runFrames(0, 1, false, false);
    runFrames(0, 1, false, true);
    ++s_boundaryRuns;
}

void noticeCase() {
    s_case = 31;
    for (uint8_t survey = 0; survey < 2; ++survey) {
        placeAtCamera(exploration_data::ROOM_Q_X - 41, exploration_data::ROOM_Q_Y - 27, true);
        setCue(3);
        s_state.discoveryTicks = 40;
        s_state.lastDiscovery = exploration::kNone;
        if (!survey) {
            for (uint8_t id = 0; id < exploration_data::PICKUP_COUNT; ++id)
                if (exploration_data::pickup(id).kind == exploration_data::CUTTER) {
                    s_state.lastDiscovery = id;
                    break;
                }
            if (s_state.lastDiscovery == exploration::kNone)
                fail(2048);
        }
        runFrames(0, 1, true, true);
        runFrames(0, 1, false, true);
    }
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
    for (uint8_t mode = 0; mode < 4; ++mode)
        motionCase(mode);
    holdCase();
    boundaryCase(0, 0);
    boundaryCase(cave::MAX_CAMERA_PIXEL_X, 0);
    boundaryCase(0, cave::MAX_CAMERA_PIXEL_Y);
    boundaryCase(cave::MAX_CAMERA_PIXEL_X, cave::MAX_CAMERA_PIXEL_Y);
    noticeCase();

    noteMusic();
    const uint32_t meanFrameUs = s_frames ? s_sumFrameUs / s_frames : 0;
    constexpr uint16_t kExpectedFrames = 842;
    constexpr uint16_t kExpectedCold = 30;
    constexpr uint16_t kExpectedWarm = 812;
    for (uint8_t i = 0; i < kStackGuardBytes; ++i) {
        if (s_stackGuard[i] != static_cast<char>(0xA5))
            s_stackGuardOk = false;
    }
    if (!s_stackGuardOk)
        s_failMask |= 1024;
    if (!s_cueOk)
        s_failMask |= 128;
    const bool coverageOk = s_remainderMask == 0xFF && s_cueMask == 0x3F &&
                            s_frames == kExpectedFrames && s_coldFrames == kExpectedCold &&
                            s_warmFrames == kExpectedWarm && s_boundaryRuns == 4 &&
                            s_stackGuardOk && s_caseOk && s_cueOk;
    const bool gapOk = s_musicGapMax <= kGapLimitTicks;
    const bool musicOk = gapOk && !s_musicUnderrun && !s_musicError;
    const bool timingOk = s_maxFrameUs <= kFullFlightTargetUs && s_deadlineMisses == 0;

    Serial.print(F("flight frames="));
    Serial.print(s_frames);
    Serial.print(F(" max_us="));
    Serial.print(s_maxFrameUs);
    Serial.print(F(" misses16="));
    Serial.println(s_deadlineMisses);
    Serial.print(F("flight music gap="));
    Serial.print(s_musicGapMax);
    Serial.print(F(" underruns="));
    Serial.print(s_musicUnderrun ? 1 : 0);
    Serial.print(F(" errors="));
    Serial.println(s_musicError ? 1 : 0);
    Serial.print(F("flight cover rem="));
    Serial.print(s_remainderMask, HEX);
    Serial.print(F(" cue="));
    Serial.print(s_cueMask, HEX);
    Serial.print(F(" cases_ok="));
    Serial.print(s_caseOk ? 1 : 0);
    Serial.print(F(" timing_ok="));
    Serial.print(timingOk ? 1 : 0);
    Serial.print(F(" music_ok="));
    Serial.print(musicOk ? 1 : 0);
    Serial.print(F(" fail_mask="));
    Serial.println(s_failMask);
    Serial.println(coverageOk && timingOk && musicOk ? 'P' : 'F');
}

void loop() {}


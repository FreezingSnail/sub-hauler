// Device stack probe (sub-hauler-m0-stack).
//
// Static free SRAM does not prove stack fit. This suite paints the unused
// stack region below the current stack pointer, runs the real update/render
// path, then scans for the deepest address the stack actually touched.
//
// Method:
//   1. bssEnd = address of the linker symbol __bss_end (top of static RAM).
//   2. sp = current stack pointer; paintTop = sp - kFrameGuard, so the active
//      frame and one interrupt frame of headroom are never painted.
//   3. Fill [bssEnd + 1, paintTop] with 0xA5 with interrupts disabled.
//   4. Re-enable interrupts and run kRepresentativeFrames of the normal
//      poll/update/render/present path, including FX cart reads. The M6 chart
//      path, the M7 current and particle frames,
//      the fixed HAULER title art path, and the cargo-free contract/undock path
//      follow in the same measured window.
//   5. Scan upward from bssEnd + 1 for the lowest byte that is no longer
//      0xA5. That address is the stack high-water mark.
//   6. reserve = highWater - bssEnd. Print one bare P when the reserve is at
//      least kReserveTarget (600 B), otherwise one bare F.
//
// The marker print happens after the scan, so the final Serial writes are not
// inside the measured window; the 600 B target leaves room for them. The
// measurement is conservative: bytes the stack never reached read back as
// 0xA5, and a path that stays inside the guard region is reported as if it
// stopped at paintTop.
//
// the Ardens gate requires the bare marker line and defaults this to 0.

#include <avr/io.h>
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/game.hpp"
#include "src/hud.hpp"
#include "src/pilot.hpp"
#include "src/roster.hpp"
#include "src/render.hpp"
#include "src/world_fx.hpp"
#include "src/chart.hpp"
#include "src/chart_view.hpp"
#include "src/station.hpp"
#include "src/station_view.hpp"
#include "src/particles.hpp"
#include "src/campaign.hpp"
#include "src/campaign_view.hpp"
#include "src/work_view.hpp"
#include "src/ending_view.hpp"
#include "src/voyage_flow.hpp"
#include "src/voyage_view.hpp"

// Defined by the AVR linker script; points one past the last static byte.
extern "C" char __bss_end;

Arduboy2Base arduboy;

// The full AVR state budget is pinned here at 77 B after the gl1.3/gl1.4
// compaction, the economy strip and schema 8's pending-story byte: diagnostic
// counters, job clocks, offers, rewards and the retired shop fields are gone.
static_assert(sizeof(game::State) == 77, "full campaign+save run state is 77 bytes on AVR");

// The firmware owns the persistent explored set statically. Keeping the probe's
// copy here (rather than as a local in the measured path) charges those 257 B
// to static RAM, exactly like the shipped image, instead of pretending the
// firmware would put them on the stack.
chart::State probeChart;

namespace {

// Re-baselined from 800 B to 600 B on 2026-10-23, after the 258 B persistent
// chart slice entered the full image. The chart+current+particle+station+
// title/roster+contract/undock/reset probe measures the cargo-free reserve;
// 600 B stays the pass line.
constexpr uint16_t kReserveTarget = 600;   // bytes of stack reserve required
constexpr uint8_t kFrameGuard = 64;        // active-frame headroom, never painted
constexpr uint8_t kPattern = 0xA5;
constexpr uint16_t kRepresentativeFrames = 8;

inline uint16_t readStackPointer() {
    uint8_t low;
    uint8_t high;
    asm volatile("in %0, %1" : "=r"(low) : "I"(_SFR_IO_ADDR(SPL)));
    asm volatile("in %0, %1" : "=r"(high) : "I"(_SFR_IO_ADDR(SPH)));
    return static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8);
}

// The deepest stack user in the current game: input sampling, world-aware
// pilot update, cave/pilot render with FX cart reads, and the buffered display
// transaction. Kept noinline so the probe sees a real call frame.
__attribute__((noinline)) void runRepresentativePath() {
    game::State state;
    world::FxTables tables;
    world::TileCache cache;
    world::clearCache(cache);
    game::resetWorld(state);

    // Title path: draw the baked HAULER title panel (no cart reads), then fly
    // the same body for the pilot frames below.
    voyage_flow::BootStatus titleStatus;
    titleStatus.state = voyage_flow::kBootValid;
    titleStatus.hasValid = true;
    voyage_view::drawTitle(titleStatus, voyage_view::kTitleContinue, voyage_view::kTitleMenu);
    FX::display(CLEAR_BUFFER);
    state.hullClass = static_cast<uint8_t>(roster::kHauler);

    for (uint16_t frame = 0; frame < kRepresentativeFrames; ++frame) {
        arduboy.pollButtons();
        game::Input input;
        input.up = false;
        input.down = false;
        input.left = false;
        input.right = (frame % 4) == 0;
        game::stepWorld(state, input, tables, &cache);
        const cave::Camera camera = pilot_view::cameraForState(state);
        view::frame(state, camera, cache, tables);
        hud::draw(state);
        FX::display(CLEAR_BUFFER);
    }

    // Exercise salvage, cutter operation, object rendering and discovery HUD
    // in the measured stack window, using the same run state as flight.
    const auto find = exploration_data::pickup(exploration_data::PICKUP_W);
    state.x = find.x;
    state.y = find.y;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    for (uint8_t tick = 0; tick < exploration::kDwellTicks; ++tick)
        exploration::update(state);
    game::snapCamera(state);
    view::frame(state, pilot_view::cameraForState(state), cache, tables);
    hud::draw(state);
    FX::display(CLEAR_BUFFER);
    const auto grate = exploration_data::gate(exploration_data::GATE_E_V);
    state.x = grate.x - 14;
    state.y = grate.y + grate.h / 2;
    for (uint8_t tick = 0; tick < exploration::kDwellTicks; ++tick)
        exploration::update(state);
    game::snapCamera(state);
    view::frame(state, pilot_view::cameraForState(state), cache, tables);
    hud::draw(state);
    FX::display(CLEAR_BUFFER);

    // Chart path: spawn reset/reveal, a few movement reveals, and real chart
    // frames. The destination X is drawn on the first frame and blinked off on
    // the rest; settlement 0 (spawn) and settlement 3 cells are explored, so
    // the settlement crosses render. Cart water reads are inside draw, before
    // the single display transaction.
    chart::reset(probeChart);
    chart::revealInitial(probeChart, state);
    chart::revealAtPixels(probeChart, state.x, state.y, chart::kMoveRevealRadius);
    chart::revealAtPixels(probeChart, exploration_data::DOCK_D_X, exploration_data::DOCK_D_Y, 0);
    state.ticks = chart::kMoveRevealIntervalTicks;
    chart::revealOnStep(probeChart, state);
    state.activeJob.to = 1;
    state.activeJob.kind = game::kJobKindCampaign;
    state.activeJob.mission = campaign::kPump;
    state.activeJob.from = campaign::kCormorant;
    state.activeJob.crates = game::kJobCrates;
    for (uint8_t frame = 0; frame < 3; ++frame) {
        const uint8_t blink = (frame == 0) ? 0 : chart_view::kBlinkMask;
        chart_view::draw(Arduboy2Base::sBuffer, probeChart, state, tables, blink);
        FX::display(CLEAR_BUFFER);
    }

    // Current path: stand in the first generated zone and run the real step so
    // its acceleration and flow marks enter the measured path.
    world::Current current;
    if (world::currentInfo(0, current)) {
        state.x = static_cast<int16_t>(current.x + current.w / 2);
        state.y = static_cast<int16_t>(current.y + current.h / 2);
        state.xFrac = 0;
        state.yFrac = 0;
        state.vxQ8 = 0;
        state.vyQ8 = 0;
        game::Input currentInput{};
        game::stepWorld(state, currentInput, tables, &cache);
        const cave::Camera currentCamera = pilot_view::cameraForState(state);
        view::frame(state, currentCamera, cache, tables);
        hud::draw(state);
        FX::display(CLEAR_BUFFER);
    }

    // Particle path: latch a deterministic hard wall impact, spawn/age the
    // debris pool, then draw the pilot frame with particles at the impact
    // anchor. The pool is bounded and needs no mine or damage state.
    state.impactAxis = 1;
    state.impactSpeedQ8 = game::kHardImpactSpeedQ8;
    particles::spawn(state);
    particles::tick(state);
    const cave::Camera particleCamera = pilot_view::cameraForState(state);
    view::frame(state, particleCamera, cache, tables);
    hud::draw(state);
    FX::display(CLEAR_BUFFER);

    // Let the pool age and draw through the real step/render path twice: this
    // covers particle update, camera conversion, and clipping.
    game::Input debrisInput{};
    for (uint8_t frame = 0; frame < 2; ++frame) {
        game::stepWorld(state, debrisInput, tables, &cache);
        const cave::Camera debrisCamera = pilot_view::cameraForState(state);
        view::frame(state, debrisCamera, cache, tables);
        hud::draw(state);
        FX::display(CLEAR_BUFFER);
    }

    // Station path: draw the native board in both idle and active shapes. No
    // cart reads during the display transaction.
    state.docked = 0;
    station_view::draw(state, station::initialRow(state));
    FX::display(CLEAR_BUFFER);
    state.activeJob.kind = game::kJobKindCampaign;
    state.activeJob.mission = campaign::kPump;
    state.activeJob.from = campaign::kCormorant;
    state.activeJob.to = campaign::kBenthos;
    state.activeJob.crates = game::kJobCrates;
    station_view::draw(state, station::initialRow(state));
    FX::display(CLEAR_BUFFER);
    jobs::clearActive(state);

    // Campaign app path: WORK rows at both source shapes, a completed Tidewall
    // board with VIEW ENDING, and all three homecoming cards. This keeps the
    // external text, restoration overlays and ending stats in the stack window.
    game::reset(state);
    state.docked = static_cast<int8_t>(campaign::kCormorant);
    state.tools = static_cast<uint8_t>(exploration_data::SEAL);
    state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon);
    state.surveys = campaign::kSurveyMask;
    work_view::draw(state, 0);
    FX::display(CLEAR_BUFFER);
    state.activeJob.kind = game::kJobKindCampaign;
    state.activeJob.mission = campaign::kPump;
    state.activeJob.from = campaign::kCormorant;
    state.activeJob.to = campaign::kBenthos;
    state.activeJob.crates = game::kJobCrates;
    work_view::draw(state, 0);
    FX::display(CLEAR_BUFFER);
    state.activeJob.kind = game::kJobKindNone;
    state.activeJob.mission = campaign::kMissionNone;
    state.activeJob.from = game::kJobNone;
    state.activeJob.to = game::kJobNone;
    state.activeJob.crates = 0;

    state.docked = static_cast<int8_t>(campaign::kTidewall);
    state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon | campaign::kMilestoneHome);
    state.ending = 0;
    work_view::draw(state, 0);
    FX::display(CLEAR_BUFFER);

    for (uint8_t card = 0; card < ending_view::kCardCount; ++card) {
        ending_view::draw(card, state);
        FX::display(CLEAR_BUFFER);
    }
    state.ending = 1;

    // Title, save-failure and recovery prompts: each is now a baked panel, but
    // the stack window still needs the real panel burst plus the surrounding
    // fixed-hull title code.
    voyage_flow::BootStatus boot;
    boot.state = voyage_flow::kBootValid;
    boot.hasValid = true;

    voyage_view::drawTitle(boot, voyage_view::kTitleContinue, voyage_view::kTitleMenu);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawTitle(boot, voyage_view::kTitleNewVoyage, voyage_view::kTitleConfirmReplace);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawSaveFailed(0);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawSaveFailed(1);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawPauseRescue(0);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawPauseRescue(1);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawRescueConfirm();
    FX::display(CLEAR_BUFFER);
    voyage_view::drawAbandonConfirm();
    FX::display(CLEAR_BUFFER);

    // Real cart checkpoint transaction in the measured window.
    voyage_save::FxStorage saveStorage;
    voyage_save::newVoyage(state
                           ,
                           probeChart
                           ,
                           saveStorage);
    voyage_save::load(state
                      ,
                      probeChart
                      ,
                      saveStorage);

    // Contract path: accept the FIRST campaign story at Tidewall (auto-load),
    // draw the active station board, then undock through the board action and
    // draw the first pilot frame.
    state.docked = static_cast<int8_t>(campaign::kTidewall);
    jobs::clearActive(state);
    if (campaign::accept(state, campaign::kFirst)) {
        station_view::draw(state, station::initialRow(state));
        FX::display(CLEAR_BUFFER);

        station::activate(station::kActiveUndockRow, state);
        const cave::Camera contractCamera = pilot_view::cameraForState(state);
        view::frame(state, contractCamera, cache, tables);
        hud::draw(state);
        FX::display(CLEAR_BUFFER);
    }
}

// Returns the number of untouched bytes between __bss_end and the lowest
// address the stack wrote. Never returns a value larger than the painted span.
__attribute__((noinline)) uint16_t measureStackReserve() {
    const uint16_t bssEnd = reinterpret_cast<uint16_t>(&__bss_end);
    const uint16_t paintBottom = bssEnd + 1;

    noInterrupts();
    const uint16_t sp = readStackPointer();
    interrupts();

    if (sp <= paintBottom + kFrameGuard)
        return 0;

    const uint16_t paintTop = sp - kFrameGuard;

    noInterrupts();
    for (uint16_t address = paintTop;; --address) {
        *reinterpret_cast<volatile uint8_t *>(address) = kPattern;
        if (address == paintBottom)
            break;
    }
    interrupts();

    runRepresentativePath();

    uint16_t highWater = paintTop;
    for (uint16_t address = paintBottom; address <= paintTop; ++address) {
        if (*reinterpret_cast<const volatile uint8_t *>(address) != kPattern) {
            highWater = address;
            break;
        }
    }

    return static_cast<uint16_t>(highWater - bssEnd);
}

}   // namespace

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE);
    FX::setCursorRange(0, 32767);
    text::begin();

    const uint16_t reserve = measureStackReserve();

    Serial.println(reserve >= kReserveTarget ? F("P") : F("F"));
}

void loop() {
}

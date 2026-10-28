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
//      and sonar paths, the M7 mine/current, damage/particle, and shop frames,
//      the M8 title/all-hull art path, and the cargo-free contract/undock path
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
// Set SH_STACK_DIAG=1 only for a manual run. Diagnostics are separate lines;
// the Ardens gate requires the bare marker line and defaults this to 0.

#include <avr/io.h>
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/features.hpp"
#include "src/game.hpp"
#include "src/hud.hpp"
#include "src/pilot.hpp"
#include "src/roster.hpp"
#include "src/render.hpp"
#include "src/world_fx.hpp"
#if SH_FEATURE_FOG_CHART
#include "src/chart.hpp"
#include "src/chart_view.hpp"
#endif
#if SH_FEATURE_SAVE_CHART
#include "src/chart_save.hpp"
#endif
#if SH_FEATURE_SONAR
#include "src/sonar.hpp"
#endif
#if SH_FEATURE_SHOP
#include "src/shop.hpp"
#include "src/shop_view.hpp"
#endif
#if SH_FEATURE_STATIONS
#include "src/station.hpp"
#include "src/station_view.hpp"
#endif
#if SH_FEATURE_HULL_DAMAGE
#include "src/damage.hpp"
#endif
#if SH_FEATURE_PARTICLES
#include "src/particles.hpp"
#endif
#if SH_FEATURE_MULTI_HULL
#include "src/roster_view.hpp"
#endif
#if SH_FEATURE_CAMPAIGN
#include "src/campaign.hpp"
#include "src/campaign_view.hpp"
#include "src/work_view.hpp"
#include "src/ending_view.hpp"
#endif
#if SH_FEATURE_SAVE_VOYAGE
#include "src/voyage_flow.hpp"
#include "src/voyage_view.hpp"
#endif

#ifndef SH_STACK_DIAG
#define SH_STACK_DIAG 0
#endif

// Defined by the AVR linker script; points one past the last static byte.
extern "C" char __bss_end;

Arduboy2Base arduboy;

#if SH_FEATURE_STATIONS && SH_FEATURE_JOBS && SH_FEATURE_FOG_CHART && SH_FEATURE_SONAR && SH_FEATURE_MINES && SH_FEATURE_CURRENTS && SH_FEATURE_SHOP && SH_FEATURE_HULL_DAMAGE &&                      \
    SH_FEATURE_PARTICLES && SH_FEATURE_MULTI_HULL
// The full AVR state budget is pinned here at 80 B after the gl1.3/gl1.4
// compaction: diagnostic counters and the job clock were removed and route
// reward fields were narrowed.
#if SH_FEATURE_EXPLORATION
#if SH_FEATURE_CAMPAIGN
static_assert(sizeof(game::State) == 99, "full campaign+save run state is 99 bytes on AVR");
#else
static_assert(sizeof(game::State) == 79, "full non-campaign exploration run state is 79 bytes on AVR");
#endif
#else
static_assert(sizeof(game::State) == 80, "non-exploration state remains 80 bytes");
#endif
#endif

#if SH_FEATURE_FOG_CHART
// The firmware owns the persistent explored set statically. Keeping the probe's
// copy here (rather than as a local in the measured path) charges those 257 B
// to static RAM, exactly like the shipped image, instead of pretending the
// firmware would put them on the stack.
chart::State probeChart;
#endif

namespace {

// Re-baselined from 800 B to 600 B on 2026-10-23, after the 258 B persistent
// chart slice entered the full image. The chart+sonar+hazard+damage+shop+
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

#if SH_FEATURE_MULTI_HULL
    // Title path: draw every class silhouette through the real selection screen
    // (no cart reads), then fly the largest body for the pilot frames below.
    for (uint8_t index = 0; index < roster::kClassCount; ++index) {
        roster_view::draw(static_cast<uint8_t>(roster::classAt(index)));
        FX::display(CLEAR_BUFFER);
    }
    // Include the explicit loader row in the measured title path.
    roster_view::draw(static_cast<uint8_t>(roster::kDefault), roster_view::kTitleLoader);
    FX::display(CLEAR_BUFFER);
    state.hullClass = static_cast<uint8_t>(roster::kLeviathan);
#endif

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

#if SH_FEATURE_EXPLORATION
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
#endif

#if SH_FEATURE_FOG_CHART
    // Chart path: spawn reset/reveal, a few movement reveals, and real chart
    // frames. The destination X is drawn on the first frame and blinked off on
    // the rest; settlement 0 (spawn) and settlement 3 cells are explored, so
    // the settlement crosses render. Cart water reads are inside draw, before
    // the single display transaction.
    chart::reset(probeChart);
    chart::revealInitial(probeChart, state);
    chart::revealAtPixels(probeChart, state.x, state.y, chart::kMoveRevealRadius);
#if SH_FEATURE_EXPLORATION
    chart::revealAtPixels(probeChart, exploration_data::DOCK_D_X, exploration_data::DOCK_D_Y, 0);
#else
    chart::revealAtPixels(probeChart, 1376, 1056, 0);
#endif
    state.ticks = chart::kMoveRevealIntervalTicks;
    chart::revealOnStep(probeChart, state);
#if SH_FEATURE_JOBS
    state.activeJob.to = 1;
#if SH_FEATURE_CAMPAIGN
    state.activeJob.kind = game::kJobKindOrdinary;
    state.activeJob.mission = game::kMissionNone;
    state.activeJob.from = 0;
#endif
#endif
    for (uint8_t frame = 0; frame < 3; ++frame) {
        const uint8_t blink = (frame == 0) ? 0 : chart_view::kBlinkMask;
        chart_view::draw(Arduboy2Base::sBuffer, probeChart, state, tables, blink);
        FX::display(CLEAR_BUFFER);
    }
#if SH_FEATURE_SAVE_CHART
    // Save path: persist and reload the same probe chart through the real
    // EEPROM adapter. The save is included in the measured window because it
    // runs on the chart-close transition, not inside the frame loop. A blank
    // or corrupt EEPROM block is ignored by load(), so the probe still works
    // after a fresh flash.
    chart_save::EepromStorage saveStorage;
    chart_save::load(probeChart, saveStorage);
    chart_save::save(probeChart, saveStorage);
#endif
#endif

#if SH_FEATURE_SONAR
    // Sonar path: a real ping spends air, reveals through the same probeChart
    // (no duplicated chart state), the next world step decrements the cooldown,
    // and a pilot frame draws the expanding ring under the HAULER. The cart
    // reads stay inside view::frame, before the display transaction.
    game::Input sonarInput{};
#if SH_FEATURE_SHOP
    sonar::fire(state, probeChart, state.sonarLevel);
#else
    sonar::fire(state, probeChart, 0);
#endif
    game::stepWorld(state, sonarInput, tables, &cache);
    const cave::Camera sonarCamera = pilot_view::cameraForState(state);
    view::frame(state, sonarCamera, cache, tables);
    hud::draw(state);
    FX::display(CLEAR_BUFFER);
#endif

#if SH_FEATURE_CURRENTS
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
#endif

#if SH_FEATURE_MINES
    // Mine path: place the hull on generated mine 0, step once to consume it,
    // then draw the one-frame impact spark with the mine already removed. With
    // PARTICLES on this step also spawns the debris pool.
    world::Mine mine;
    if (world::mineInfo(0, mine)) {
        state.x = mine.x;
        state.y = mine.y;
        state.xFrac = 0;
        state.yFrac = 0;
        state.vxQ8 = 0;
        state.vyQ8 = 0;
        game::Input mineInput{};
        game::stepWorld(state, mineInput, tables, &cache);
        const cave::Camera mineCamera = pilot_view::cameraForState(state);
        view::frame(state, mineCamera, cache, tables);
        hud::draw(state);
        FX::display(CLEAR_BUFFER);

#if SH_FEATURE_PARTICLES
        // Let the pool age and draw through the real step/render path twice:
        // this covers particle update, camera conversion, and clipping.
        for (uint8_t frame = 0; frame < 2; ++frame) {
            game::stepWorld(state, mineInput, tables, &cache);
            const cave::Camera debrisCamera = pilot_view::cameraForState(state);
            view::frame(state, debrisCamera, cache, tables);
            hud::draw(state);
            FX::display(CLEAR_BUFFER);
        }
#endif
    }
#endif

#if SH_FEATURE_HULL_DAMAGE || SH_FEATURE_PARTICLES
    // Damage/particle path: latch a deterministic hard wall impact, apply the
    // real damage rule and spawn/age the debris pool, then draw the pilot frame
    // with the hull-edge flash and particles at the impact anchor.
    state.impactAxis = 1;
    state.impactSpeedQ8 = game::kHardImpactSpeedQ8;
#if SH_FEATURE_HULL_DAMAGE
    damage::resolveWallImpact(state);
    damage::tick(state);
#endif
#if SH_FEATURE_PARTICLES
    particles::spawn(state);
    particles::tick(state);
#endif
    const cave::Camera damageCamera = pilot_view::cameraForState(state);
    view::frame(state, damageCamera, cache, tables);
    hud::draw(state);
    FX::display(CLEAR_BUFFER);
#endif

#if SH_FEATURE_SHOP
    // Shop path: dock, buy the air tank through the real transaction, and draw
    // the three-row board. No cart reads during the display transaction.
    state.docked = 0;
    state.credits = 500;
    shop::buy(state, shop::kAir);
    shop_view::draw(state, shop::kFirstItem);
    FX::display(CLEAR_BUFFER);
#endif

#if SH_FEATURE_CAMPAIGN
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
    state.activeJob.reward = campaign::missionReward(campaign::kPump);
    work_view::draw(state, 0);
    FX::display(CLEAR_BUFFER);
    state.activeJob.kind = game::kJobKindNone;
    state.activeJob.mission = campaign::kMissionNone;
    state.activeJob.from = game::kJobNone;
    state.activeJob.to = game::kJobNone;
    state.activeJob.crates = 0;
    state.activeJob.reward = 0;

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

#if SH_FEATURE_SAVE_VOYAGE
    // Title, save-failure and recovery prompts: each is now a baked panel, but
    // the stack window still needs the real panel burst plus the surrounding
    // status/hull selection code.
    voyage_flow::BootStatus boot;
    boot.state = voyage_flow::kBootValid;
    boot.hasValid = true;
    boot.hasCorrupt = false;
    boot.hasIncompatible = false;
    voyage_view::drawTitle(boot, voyage_view::kTitleContinue, state.hullClass, voyage_view::kTitleMenu);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawTitle(boot, voyage_view::kTitleNewVoyage, state.hullClass, voyage_view::kTitleConfirmReplace);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawSaveFailed(0);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawSaveFailed(1);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawPauseRescue(0);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawPauseRescue(1);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawWreck(0);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawWreck(1);
    FX::display(CLEAR_BUFFER);
    voyage_view::drawRescueConfirm();
    FX::display(CLEAR_BUFFER);
    voyage_view::drawAbandonConfirm();
    FX::display(CLEAR_BUFFER);

    // Real cart checkpoint transaction in the measured window.
    voyage_save::FxStorage saveStorage;
    voyage_save::newVoyage(state
#if SH_FEATURE_FOG_CHART
                           ,
                           probeChart
#endif
                           ,
                           saveStorage);
    voyage_save::load(state
#if SH_FEATURE_FOG_CHART
                      ,
                      probeChart
#endif
                      ,
                      saveStorage);
#endif
#endif

#if SH_FEATURE_STATIONS && SH_FEATURE_JOBS
    // Contract path: post offers at a real berth, accept (auto-load), draw the
    // active station board, then undock through the board action and draw the
    // first pilot frame. This replaces the removed packing screen and keeps
    // the station/job/auto-load/undock call chain inside the measured window.
    state.docked = 0;
    jobs::clearActive(state);
    if (jobs::refreshOffers(state, 0)) {
        station::activate(station::kFirstRow, state);
        station_view::draw(state, station::initialRow(state));
        FX::display(CLEAR_BUFFER);

        station::activate(station::kActiveUndockRow, state);
        const cave::Camera contractCamera = pilot_view::cameraForState(state);
        view::frame(state, contractCamera, cache, tables);
        hud::draw(state);
        FX::display(CLEAR_BUFFER);
    }
#endif
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
#if SH_FEATURE_SAVE_VOYAGE
    FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE);
#else
    FX::begin(FX_DATA_PAGE);
#endif
    FX::setCursorRange(0, 32767);
    text::begin();

    const uint16_t reserve = measureStackReserve();

    // Device coverage for the title loader row. The action itself is not
    // invoked: exitToBootloader() would leave the serial harness.
#if SH_FEATURE_MULTI_HULL
    const bool loaderRowsOk = roster_view::nextRow(roster_view::kTitleHull) == roster_view::kTitleLoader && roster_view::nextRow(roster_view::kTitleLoader) == roster_view::kTitleHull;
#else
    const bool loaderRowsOk = true;
#endif

#if SH_STACK_DIAG
    Serial.print(F("stack_reserve="));
    Serial.println(reserve);
#endif

    Serial.println(reserve >= kReserveTarget && loaderRowsOk ? F("P") : F("F"));
}

void loop() {
}

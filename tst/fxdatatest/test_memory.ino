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
//      and sonar paths and the M7 mine/current and damage/particle frames
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
#include "src/render.hpp"
#include "src/world_fx.hpp"
#if SH_FEATURE_FOG_CHART
#include "src/chart.hpp"
#include "src/chart_view.hpp"
#endif
#if SH_FEATURE_SONAR
#include "src/sonar.hpp"
#endif
#if SH_FEATURE_CARGO_LOAD
#include "src/cargo_fx.hpp"
#include "src/cargo_handoff.hpp"
#include "src/cargo_screen.hpp"
#include "src/cargo_view.hpp"
#endif
#if SH_FEATURE_HULL_DAMAGE
#include "src/damage.hpp"
#endif
#if SH_FEATURE_PARTICLES
#include "src/particles.hpp"
#endif

#ifndef SH_STACK_DIAG
#define SH_STACK_DIAG 0
#endif

// Defined by the AVR linker script; points one past the last static byte.
extern "C" char __bss_end;

Arduboy2 arduboy;

#if SH_FEATURE_STATIONS && SH_FEATURE_JOBS && SH_FEATURE_CARGO_LOAD && SH_FEATURE_FOG_CHART && SH_FEATURE_SONAR && SH_FEATURE_MINES && SH_FEATURE_CURRENTS && SH_FEATURE_HULL_DAMAGE && SH_FEATURE_PARTICLES && SH_FEATURE_MULTI_HULL
// The full AVR state budget is pinned here: M7 hazard state (71 B) plus the
// one-byte damage cooldown and the 16-byte particle pool.
static_assert(sizeof(game::State) == 88, "full game::State is 88 bytes on AVR");
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
// chart slice entered the full image. The chart+sonar+hazard+damage/particle
// probe measures 616 B, so 600 B keeps a deliberate 16 B margin over the
// measurement while remaining well above the measured stack depth.
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

#if SH_FEATURE_FOG_CHART
    // Chart path: spawn reset/reveal, a few movement reveals, and real chart
    // frames. The destination X is drawn on the first frame and blinked off on
    // the rest; settlement 0 (spawn) and settlement 3 cells are explored, so
    // the settlement crosses render. Cart water reads are inside draw, before
    // the single display transaction.
    chart::reset(probeChart);
    chart::revealInitial(probeChart, state);
    chart::revealAtPixels(probeChart, state.x, state.y, chart::kMoveRevealRadius);
    chart::revealAtPixels(probeChart, 1376, 1056, 0);
    state.ticks = chart::kMoveRevealIntervalTicks;
    chart::revealOnStep(probeChart, state);
#if SH_FEATURE_JOBS
    state.activeJob.to = 1;
#endif
    for (uint8_t frame = 0; frame < 3; ++frame) {
        const uint8_t blink = (frame == 0) ? 0 : chart_view::kBlinkMask;
        chart_view::draw(Arduboy2Base::sBuffer, probeChart, state, tables, blink);
        FX::display(CLEAR_BUFFER);
    }
#endif

#if SH_FEATURE_SONAR
    // Sonar path: a real ping spends air, reveals through the same probeChart
    // (no duplicated chart state), the next world step decrements the cooldown,
    // and a pilot frame draws the expanding ring under the HAULER. The cart
    // reads stay inside view::frame, before the display transaction.
    game::Input sonarInput{};
    sonar::fire(state, probeChart, 0);
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

#if SH_FEATURE_CARGO_LOAD
    // Cargo path: one arrangement load plus a few real input edges and renders.
    // The cart read happens in update, never during FX::display.
    cargo::FxCargoReader reader;
    cargo_screen::State cargo;
    if (cargo_screen::load(cargo, reader)) {
        for (uint8_t frame = 0; frame < 5; ++frame) {
            cargo_screen::Input input{};
            switch (frame) {
            case 0:
                input.left = true;
                break;
            case 1:
                input.a = true;
                break;
            case 2:
                input.right = true;
                break;
            case 3:
                input.b = true;
                break;
            default:
                input.left = true;
                break;
            }
            cargo_screen::apply(cargo, input);
            cargo_view::draw(cargo.puzzle, cargo.selected, cargo.message);
            FX::display(CLEAR_BUFFER);
        }

        // Commit path: solve the known required arrangement and run the real
        // handoff, including station::undock, so the probe covers the deepest
        // M5 integration frame.
        const uint8_t order[6] = {4, 5, 0, 1, 2, 3};
        bool solved = true;
        for (uint8_t i = 0; i < 6; ++i) {
            const cargo::CargoPiece &piece = cargo.puzzle.pieces[order[i]];
            if (!cargo::place(cargo.puzzle, order[i], piece.x, piece.y))
                solved = false;
        }
        if (solved) {
            state.docked = 0;
            state.activeJob.to = 0;
            state.activeJob.crates = game::kJobCrates;
            cargo_handoff::commit(state, cargo);
        }
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
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    const uint16_t reserve = measureStackReserve();

#if SH_STACK_DIAG
    Serial.print(F("stack_reserve="));
    Serial.println(reserve);
#endif

    Serial.println(reserve >= kReserveTarget ? F("P") : F("F"));
}

void loop() {
}

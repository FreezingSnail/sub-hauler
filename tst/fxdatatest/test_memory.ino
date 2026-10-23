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
//      poll/update/render/present path, including FX cart reads.
//   5. Scan upward from bssEnd + 1 for the lowest byte that is no longer
//      0xA5. That address is the stack high-water mark.
//   6. reserve = highWater - bssEnd. Print one bare P when the reserve is at
//      least kReserveTarget (800 B), otherwise one bare F.
//
// The marker print happens after the scan, so the final Serial writes are not
// inside the measured window; the 800 B target leaves room for them. The
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
#if SH_FEATURE_CARGO_LOAD
#include "src/cargo_fx.hpp"
#include "src/cargo_handoff.hpp"
#include "src/cargo_screen.hpp"
#include "src/cargo_view.hpp"
#endif

#ifndef SH_STACK_DIAG
#define SH_STACK_DIAG 0
#endif

// Defined by the AVR linker script; points one past the last static byte.
extern "C" char __bss_end;

Arduboy2 arduboy;

namespace {

constexpr uint16_t kReserveTarget = 800;   // bytes of stack reserve required
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

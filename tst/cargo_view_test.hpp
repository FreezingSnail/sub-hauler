#pragma once

#include "../src/cargo_screen.hpp"
#include "../src/cargo_view.hpp"
#include "../src/features.hpp"

#if SH_FEATURE_CARGO_LOAD

#include <string.h>

#include "cargo_test.hpp"
#include "test.hpp"

// Cargo cutaway shell tests: per-class geometry, blocked cells, crate bounds,
// tray cues, class naming, text slots, and deterministic frame composition.
// All drawing is the pure page-major composition used by the device; no
// Arduino headers.
inline void CargoViewSuite(TestRunner &runner) {
    const auto pixel = [](const uint8_t *buffer, int16_t x, int16_t y) {
        return (buffer[(static_cast<uint16_t>(y) >> 3) * cargo_view::kScreenW + static_cast<uint16_t>(x)] >> (y & 7)) & 1u;
    };

#if SH_FEATURE_MULTI_HULL
    const uint8_t hullIds[3] = {cargo::HULL_MINNOW, cargo::HULL_HAULER, cargo::HULL_LEVIATHAN};
    const uint8_t cellSizes[3] = {7, 6, 5};
    constexpr uint8_t kLayoutHulls = 3;
#else
    const uint8_t hullIds[1] = {cargo::HULL_HAULER};
    const uint8_t cellSizes[1] = {6};
    constexpr uint8_t kLayoutHulls = 1;
#endif

    {
        Test test("class holds scale, centre, and fit the 128x64 cutaway");
        for (uint8_t i = 0; i < kLayoutHulls; ++i) {
            const cargo::HullSpec &spec = cargo::HULLS[hullIds[i]];
            const cargo_view::Layout hold = cargo_view::layoutFor(spec.cols, spec.rows);
            test.assertEqual(hold.cell, cellSizes[i], "class cell scale");
            test.assertEqual(hold.w, static_cast<uint8_t>(spec.cols * hold.cell), "hold width follows columns");
            test.assertEqual(hold.h, static_cast<uint8_t>(spec.rows * hold.cell), "hold height follows rows");
            test.assertEqual(static_cast<uint8_t>(hold.x + hold.w / 2), cargo_view::kHoldCenterX, "hold is horizontally centred");
            test.assertEqual(static_cast<uint8_t>(hold.y + hold.h / 2), cargo_view::kHoldCenterY, "hold is vertically centred");
            test.assertTrue(hold.x > cargo_view::kHullX + cargo_view::kCorner, "hold clears the bow shell");
            test.assertTrue(hold.x + hold.w + cargo_view::kCorner < cargo_view::kHullX + cargo_view::kHullW,
                            "hold clears the stern shell");
            test.assertTrue(hold.y > cargo_view::kHullY, "hold sits below the deck");
            test.assertTrue(hold.y + hold.h + 4 <= cargo_view::kHullY + cargo_view::kHullH,
                            "hold and ballast stay inside the hull");
            test.assertTrue(hold.y + hold.h < cargo_view::kTrayY, "tray sits below every hold");
        }
        test.assertTrue(cargo_view::kCabinX + cargo_view::kCabinW <= cargo_view::kHaulerHold.x - 3,
                        "bow cabin sits forward of the hold");
        test.assertTrue(cargo_view::kEngineX > cargo_view::kHaulerHold.x + cargo_view::kHaulerHold.w + 2,
                        "engine room sits aft of the hold");
        test.assertTrue(cargo_view::kTrayX + cargo_view::kTraySlots * cargo_view::kTraySlot <= cargo_view::kScreenW,
                        "tray inside the display");
        test.assertTrue(cargo_view::kPromptY + 7 <= cargo_view::kScreenH, "prompt inside the display");
        runner.add(test);
    }

    cargo_test::FakeBank bank;
    cargo_test::loadAllHulls(bank);

    // Single-hull builds fold every load to HAULER, so only that arrangement
    // is exercised; the layout math above still covers all three class specs.
#if SH_FEATURE_MULTI_HULL
    constexpr uint8_t kLoadedHulls = 3;
    const uint8_t loadIds[3] = {cargo::HULL_MINNOW, cargo::HULL_HAULER, cargo::HULL_LEVIATHAN};
#else
    constexpr uint8_t kLoadedHulls = 1;
    const uint8_t loadIds[1] = {cargo::HULL_HAULER};
#endif

    cargo_screen::State shell[3];
    cargo_screen::State hauler;
    {
        Test test("each class loads its own arrangement and record set");
        test.assertTrue(cargo_screen::load(hauler, bank, cargo::HULL_HAULER, 0), "HAULER arrangement loads");
        for (uint8_t i = 0; i < kLoadedHulls; ++i) {
            const cargo::HullSpec &spec = cargo::HULLS[loadIds[i]];
            test.assertTrue(cargo_screen::load(shell[i], bank, loadIds[i], 0), "class arrangement 0 loads");
            test.assertTrue(shell[i].loaded, "loaded flag is set");
            test.assertEqual(shell[i].hull, loadIds[i], "state keeps the selected hull");
            test.assertEqual(shell[i].arrangement, 0, "arrangement index");
            test.assertEqual(shell[i].puzzle.cols, spec.cols, "hold columns");
            test.assertEqual(shell[i].puzzle.rows, spec.rows, "hold rows");
            test.assertEqual(shell[i].puzzle.pieceCount, spec.pieces, "class piece count");
            test.assertEqual(shell[i].puzzle.requiredTotal, spec.required, "required pieces");
            test.assertEqual(shell[i].puzzle.bonusTotal, spec.bonus, "bonus pieces");
            test.assertEqual(shell[i].puzzle.load, spec.load, "load cells");
            test.assertTrue(shell[i].puzzle.hold == spec.hold, "hold mask comes from HullSpec");
        }
        test.assertTrue(sizeof(cargo_screen::State) <= sizeof(cargo::CargoPuzzle) + 8,
                        "screen state stays one puzzle plus alignment");
        runner.add(test);
    }

    {
        Test test("class dimensions drive the class name");
#if SH_FEATURE_MULTI_HULL
        test.assertEqual(strcmp(cargo_view::nameFor(cargo_view::hullFor(shell[0].puzzle)), "MINNOW"), 0,
                         "MINNOW name");
        test.assertEqual(strcmp(cargo_view::nameFor(cargo_view::hullFor(shell[1].puzzle)), "HAULER"), 0,
                         "HAULER name");
        test.assertEqual(strcmp(cargo_view::nameFor(cargo_view::hullFor(shell[2].puzzle)), "LEVIATHAN"), 0,
                         "LEVIATHAN name");
#else
        test.assertEqual(strcmp(cargo_view::nameFor(cargo_view::hullFor(hauler.puzzle)), "HAULER"), 0,
                         "single-hull load draws HAULER");
        test.assertEqual(strcmp(cargo_view::nameFor(cargo_view::hullFor(shell[0].puzzle)), "HAULER"), 0,
                         "single-hull fold keeps the HAULER name");
#endif
        runner.add(test);
    }

    {
        Test test("blocked cells render for every class and open centres stay black");
        for (uint8_t i = 0; i < kLoadedHulls; ++i) {
            const cargo::HullSpec &spec = cargo::HULLS[loadIds[i]];
            const cargo_view::Layout hold = cargo_view::layoutOf(shell[i].puzzle);
            uint8_t buffer[cargo_view::kBufferBytes];
            cargo_view::drawFrame(shell[i].puzzle, shell[i].selected, buffer);

            bool blockedMarked = true;
            bool openClear = true;
            bool blockedRejected = true;
            for (uint8_t row = 0; row < spec.rows; ++row) {
                for (uint8_t col = 0; col < spec.cols; ++col) {
                    const int16_t x = static_cast<int16_t>(hold.x + col * hold.cell + hold.cell / 2);
                    const int16_t y = static_cast<int16_t>(hold.y + row * hold.cell + hold.cell / 2);
                    if (cargo::inHold(shell[i].puzzle, col, row)) {
                        openClear = openClear && pixel(buffer, x, y) == 0;
                    } else {
                        blockedMarked = blockedMarked && pixel(buffer, x, y) == 1;

                        // A single-cell probe cannot occupy a structural cell.
                        cargo::CargoPuzzle probe = shell[i].puzzle;
                        probe.pieceCount = 1;
                        probe.grid = 0;
                        probe.held = -1;
                        probe.pieces[0].mask = 0x0001;
                        probe.pieces[0].x = col;
                        probe.pieces[0].y = row;
                        probe.pieces[0].flags = 0;
                        blockedRejected = blockedRejected && !cargo::canPlace(probe, 0, col, row);
                    }
                }
            }
            test.assertTrue(blockedMarked, "every blocked cell draws a structural mark");
            test.assertTrue(openClear, "open cell centres stay black");
            test.assertTrue(blockedRejected, "blocked cells cannot be occupied");
        }
        runner.add(test);
    }

    {
        Test test("target cargo renders solid inside each class hold");
        for (uint8_t i = 0; i < kLoadedHulls; ++i) {
            cargo::CargoPuzzle puzzle = shell[i].puzzle;
            test.assertTrue(cargo_test::solveRequired(puzzle), "required target pieces install");
            test.assertTrue(cargo::requiredComplete(puzzle), "required counter reaches the contract");

            const cargo_view::Layout hold = cargo_view::layoutOf(puzzle);
            uint8_t buffer[cargo_view::kBufferBytes];
            cargo_view::drawFrame(puzzle, 0, buffer);

            bool centresSolid = true;
            bool cellsInside = true;
            for (uint8_t p = 0; p < puzzle.pieceCount; ++p) {
                if (!cargo::piecePlaced(puzzle.pieces[p]))
                    continue;
                const cargo::CargoPiece &piece = puzzle.pieces[p];
                const uint16_t mask = cargo::pieceMask(piece);
                for (uint8_t sy = 0; sy < 4; ++sy) {
                    for (uint8_t sx = 0; sx < 4; ++sx) {
                        if ((mask & (uint16_t(1) << (sy * 4 + sx))) == 0)
                            continue;
                        const uint8_t col = static_cast<uint8_t>(piece.x + sx);
                        const uint8_t row = static_cast<uint8_t>(piece.y + sy);
                        cellsInside = cellsInside && col < puzzle.cols && row < puzzle.rows;
                        const int16_t x = static_cast<int16_t>(hold.x + col * hold.cell + hold.cell / 2);
                        const int16_t y = static_cast<int16_t>(hold.y + row * hold.cell + hold.cell / 2);
                        centresSolid = centresSolid && pixel(buffer, x, y) == 1;
                    }
                }
            }
            test.assertTrue(cellsInside, "crate cells stay inside the class hold");
            test.assertTrue(centresSolid, "crate centres are solid");
        }
        runner.add(test);
    }

    {
        Test test("held crate uses a hollow class-scaled cue");
        for (uint8_t i = 0; i < kLoadedHulls; ++i) {
            cargo::CargoPuzzle puzzle = shell[i].puzzle;
            puzzle.held = 0;
            const cargo_view::Layout hold = cargo_view::layoutOf(puzzle);
            uint8_t buffer[cargo_view::kBufferBytes];
            cargo_view::drawFrame(puzzle, 0, buffer);

            const cargo::CargoPiece &piece = puzzle.pieces[0];
            const int16_t x = static_cast<int16_t>(hold.x + piece.x * hold.cell);
            const int16_t y = static_cast<int16_t>(hold.y + piece.y * hold.cell);
            test.assertEqual(pixel(buffer, x, y), 1u, "held outline is drawn");
            test.assertEqual(pixel(buffer, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + hold.cell / 2)), 0u,
                             "held cell is hollow");
            test.assertEqual(pixel(buffer, static_cast<int16_t>(x + hold.cell / 2),
                                   static_cast<int16_t>(y + hold.cell / 2)),
                             1u, "held centre cue");
        }
        runner.add(test);
    }

    {
        Test test("tray previews unplaced pieces and marks the selection");
        const cargo_screen::State &last = shell[kLoadedHulls - 1];
        uint8_t buffer[cargo_view::kBufferBytes];
        cargo_view::drawFrame(last.puzzle, 2, buffer);

        bool slotHasArt = true;
        for (uint8_t slot = 0; slot < last.puzzle.pieceCount; ++slot) {
            bool art = false;
            for (int16_t y = cargo_view::kTrayY; y < cargo_view::kTrayY + cargo_view::kTraySlot; ++y) {
                for (int16_t x = static_cast<int16_t>(cargo_view::kTrayX + slot * cargo_view::kTraySlot);
                     x < static_cast<int16_t>(cargo_view::kTrayX + (slot + 1) * cargo_view::kTraySlot); ++x)
                    art = art || pixel(buffer, x, y) != 0;
            }
            slotHasArt = slotHasArt && art;
        }
        test.assertTrue(slotHasArt, "every unplaced piece previews");

        const int16_t selX = static_cast<int16_t>(cargo_view::kTrayX + 2 * cargo_view::kTraySlot);
        test.assertEqual(pixel(buffer, selX, static_cast<int16_t>(cargo_view::kTrayY + cargo_view::kTraySlot)), 1u,
                         "selection underline starts");
        test.assertEqual(pixel(buffer, static_cast<int16_t>(selX + cargo_view::kTraySlot - 1),
                               static_cast<int16_t>(cargo_view::kTrayY + cargo_view::kTraySlot)),
                         1u, "selection underline ends");
        test.assertEqual(pixel(buffer, static_cast<int16_t>(selX + cargo_view::kTraySlot),
                               static_cast<int16_t>(cargo_view::kTrayY + cargo_view::kTraySlot)),
                         0u, "selection underline does not bleed into the next slot");
        runner.add(test);
    }

    {
        Test test("selected installed crate gets a corner cue at HAULER scale");
        cargo::CargoPuzzle puzzle = hauler.puzzle;
        test.assertTrue(cargo::place(puzzle, 3, 0, 2), "install the bottom-left single crate");

        const cargo_view::Layout hold = cargo_view::layoutOf(puzzle);
        const int16_t x = hold.x;
        const int16_t y = static_cast<int16_t>(hold.y + 2 * hold.cell);

        uint8_t unselected[cargo_view::kBufferBytes];
        cargo_view::drawFrame(puzzle, 0, unselected);
        test.assertEqual(pixel(unselected, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1)), 1u,
                         "unselected crate keeps a solid cell");

        uint8_t selected[cargo_view::kBufferBytes];
        cargo_view::drawFrame(puzzle, 3, selected);
        test.assertEqual(pixel(selected, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1)), 0u,
                         "selected crate corner cue is black");
        test.assertEqual(pixel(selected, static_cast<int16_t>(x + hold.cell / 2),
                               static_cast<int16_t>(y + hold.cell / 2)),
                         1u, "selected crate centre stays solid");
        test.assertTrue(memcmp(unselected, selected, cargo_view::kBufferBytes) != 0, "selection changes installed art");
        runner.add(test);
    }

    {
        Test test("status text slots fit the 5x7 device font");
        test.assertTrue(cargo_view::kNameX + cargo_view::kNameChars * 6 < cargo_view::kStatusX, "name clears counters");
        test.assertTrue(cargo_view::kStatusX + cargo_view::kStatusChars * 6 <= cargo_view::kScreenW - 2, "counters fit");
        test.assertTrue(cargo_view::kLoadX + cargo_view::kLoadChars * 6 <= cargo_view::kTrayX, "load status clears the tray");
        test.assertTrue(cargo_view::kMessageX + cargo_view::kMessageChars * 6 <= cargo_view::kPromptX,
                        "feedback clears the exit hint");
        test.assertTrue(cargo_view::kPromptX + cargo_view::kPromptChars * 6 <= cargo_view::kScreenW - 2, "prompt fits");
        test.assertEqual(cargo_view::kPromptY + 7, cargo_view::kScreenH, "prompt baseline sits on the bottom edge");
        test.assertTrue(cargo_view::kTrayY + cargo_view::kTraySlot <= cargo_view::kPromptY, "tray clears the prompt");
#if SH_FEATURE_MULTI_HULL
        test.assertEqual(cargo_view::kNameChars, 9, "LEVIATHAN is the widest class name");
#else
        test.assertEqual(cargo_view::kNameChars, 6, "single-hull build only needs HAULER");
#endif
        runner.add(test);
    }

    {
        Test test("frame composition is deterministic and class-sensitive");
        uint8_t first[cargo_view::kBufferBytes];
        uint8_t second[cargo_view::kBufferBytes];
        cargo_view::drawFrame(hauler.puzzle, 1, first);
        cargo_view::drawFrame(hauler.puzzle, 1, second);
        test.assertTrue(memcmp(first, second, cargo_view::kBufferBytes) == 0, "same state composes the same frame");

        cargo_view::drawFrame(hauler.puzzle, 3, second);
        test.assertTrue(memcmp(first, second, cargo_view::kBufferBytes) != 0, "selection changes the frame");

#if SH_FEATURE_MULTI_HULL
        cargo_view::drawFrame(shell[0].puzzle, 1, first);
        cargo_view::drawFrame(shell[2].puzzle, 1, second);
        test.assertTrue(memcmp(first, second, cargo_view::kBufferBytes) != 0, "class holds compose different frames");
#endif
        runner.add(test);
    }
}

#endif   // SH_FEATURE_CARGO_LOAD

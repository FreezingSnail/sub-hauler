#pragma once

#include "../src/cargo_screen.hpp"
#include "../src/cargo_view.hpp"
#include "../src/features.hpp"

#if SH_FEATURE_CARGO_LOAD

#include <string.h>

#include "cargo_test.hpp"
#include "test.hpp"

// Cargo cutaway shell tests: geometry, blocked cells, crate bounds, tray cues,
// text slots, and deterministic frame composition. All drawing is the pure
// page-major composition used by the device; no Arduino headers.
inline void CargoViewSuite(TestRunner &runner) {
    const auto pixel = [](const uint8_t *buffer, int16_t x, int16_t y) {
        return (buffer[(static_cast<uint16_t>(y) >> 3) * cargo_view::kScreenW + static_cast<uint16_t>(x)] >> (y & 7)) & 1u;
    };

    {
        Test test("HAULER shell geometry fits 128x64");
        test.assertEqual(cargo_view::kHoldW, 66, "11 cells at 6 px");
        test.assertEqual(cargo_view::kHoldH, 18, "3 cells at 6 px");
        test.assertTrue(cargo_view::kHoldX > cargo_view::kHullX, "hold starts inside the bow shell");
        test.assertTrue(cargo_view::kHoldRight < cargo_view::kHullX + cargo_view::kHullW, "hold ends inside the stern shell");
        test.assertTrue(cargo_view::kHoldBottom + 4 <= cargo_view::kHullY + cargo_view::kHullH, "hold and ballast inside the hull");
        test.assertTrue(cargo_view::kHoldBottom < cargo_view::kTrayY, "tray below the hull");
        test.assertTrue(cargo_view::kTrayX + cargo_view::kTraySlots * cargo_view::kTraySlot <= cargo_view::kScreenW, "tray inside the display");
        test.assertTrue(cargo_view::kPromptY + 7 <= cargo_view::kScreenH, "prompt inside the display");
        test.assertTrue(cargo_view::kCabinX + cargo_view::kCabinW < cargo_view::kBulkLX, "bow cabin forward of the hold");
        test.assertTrue(cargo_view::kEngineX > cargo_view::kBulkRX, "engine room aft of the hold");
        runner.add(test);
    }

    cargo_test::FakeBank bank;
    cargo_test::loadAllHulls(bank);

    cargo_screen::State shell;
    {
        Test test("HAULER arrangement 0 loads through the Reader");
        test.assertTrue(cargo_screen::load(shell, bank), "HAULER arrangement 0 loads");
        test.assertTrue(shell.loaded, "loaded flag is set");
        test.assertEqual(shell.hull, cargo::HULL_HAULER, "hull id");
        test.assertEqual(shell.arrangement, 0, "arrangement index");
        test.assertEqual(shell.puzzle.cols, 11, "hold columns");
        test.assertEqual(shell.puzzle.rows, 3, "hold rows");
        test.assertEqual(shell.puzzle.pieceCount, 7, "HAULER pieces");
        test.assertEqual(shell.puzzle.requiredTotal, 6, "required pieces");
        test.assertEqual(shell.puzzle.bonusTotal, 1, "bonus pieces");
        test.assertEqual(shell.puzzle.load, 26, "load cells");
        test.assertTrue(shell.puzzle.hold == cargo::HULLS[cargo::HULL_HAULER].hold, "hold mask comes from HullSpec");
        test.assertTrue(sizeof(cargo_screen::State) <= sizeof(cargo::CargoPuzzle) + 8, "screen state stays one puzzle plus alignment");
        runner.add(test);
    }

    {
        Test test("empty cutaway marks blocked cells distinctly");
        uint8_t buffer[cargo_view::kBufferBytes];
        cargo_view::drawFrame(shell.puzzle, shell.selected, buffer);

        const int16_t splitX = static_cast<int16_t>(cargo_view::kSplitX + 2);
        const int16_t splitMidY = static_cast<int16_t>(cargo_view::kHoldY + cargo_view::kCell + 3);
        test.assertEqual(pixel(buffer, splitX, splitMidY), 1u, "split bulkhead is solid");

        const int16_t openX = static_cast<int16_t>(cargo_view::kHoldX + 4 * cargo_view::kCell + 3);
        test.assertEqual(pixel(buffer, openX, splitMidY), 0u, "open cell centre stays black");

        const int16_t notchX = static_cast<int16_t>(cargo_view::kHoldX + 1);
        const int16_t notchY = static_cast<int16_t>(cargo_view::kHoldY + 1);
        test.assertEqual(pixel(buffer, notchX, notchY), 1u, "notch block frame is drawn");
        test.assertEqual(pixel(buffer, static_cast<int16_t>(notchX + 1), static_cast<int16_t>(notchY + 1)), 1u, "notch cross is drawn");
        test.assertEqual(pixel(buffer, static_cast<int16_t>(notchX + 2), static_cast<int16_t>(notchY + 2)), 1u, "notch cross centre");
        runner.add(test);
    }

    {
        Test test("installed crates are solid and stay inside the hold");
        cargo::CargoPuzzle puzzle = shell.puzzle;
        const cargo::CargoPiece &piece = puzzle.pieces[0];
        const uint16_t mask = cargo::pieceMask(piece);
        puzzle.pieces[0].flags |= cargo::PIECE_PLACED;
        puzzle.grid |= cargo::pieceCells(puzzle, puzzle.pieces[0], piece.x, piece.y);
        puzzle.order[0] = 0;
        puzzle.orderSize = 1;
        puzzle.requiredPlaced = 1;
        puzzle.loadedCells = cargo::maskArea(mask);

        uint8_t buffer[cargo_view::kBufferBytes];
        cargo_view::drawFrame(puzzle, 0, buffer);

        bool centresSolid = true;
        bool cellsInside = true;
        for (uint8_t sy = 0; sy < 4; ++sy) {
            for (uint8_t sx = 0; sx < 4; ++sx) {
                if ((mask & (uint16_t(1) << (sy * 4 + sx))) == 0)
                    continue;
                const uint8_t col = static_cast<uint8_t>(piece.x + sx);
                const uint8_t row = static_cast<uint8_t>(piece.y + sy);
                cellsInside = cellsInside && col < 11 && row < 3;
                const int16_t x = static_cast<int16_t>(cargo_view::kHoldX + col * cargo_view::kCell);
                const int16_t y = static_cast<int16_t>(cargo_view::kHoldY + row * cargo_view::kCell);
                centresSolid = centresSolid && pixel(buffer, static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 2)) == 1;
            }
        }
        test.assertTrue(cellsInside, "crate cells stay in the 11x3 hold");
        test.assertTrue(centresSolid, "crate centres are solid");

        // Top-left cell: no neighbour above, so its top edge is the black border.
        const int16_t topX = static_cast<int16_t>(cargo_view::kHoldX + piece.x * cargo_view::kCell);
        const int16_t topY = static_cast<int16_t>(cargo_view::kHoldY + piece.y * cargo_view::kCell);
        test.assertEqual(pixel(buffer, static_cast<int16_t>(topX + 2), topY), 0u, "crate top border reads");

        bool stackAbove = false;
        for (int16_t x = 0; x < cargo_view::kScreenW; ++x) {
            for (int16_t y = cargo_view::kHullY - 2; y < cargo_view::kHullY; ++y)
                stackAbove = stackAbove || pixel(buffer, x, y) != 0;
        }
        test.assertTrue(!stackAbove, "no cargo is stacked above the hull");
        runner.add(test);
    }

    {
        Test test("held crate uses a hollow selection cue");
        cargo::CargoPuzzle puzzle = shell.puzzle;
        puzzle.held = 0;
        uint8_t buffer[cargo_view::kBufferBytes];
        cargo_view::drawFrame(puzzle, 0, buffer);

        const cargo::CargoPiece &piece = puzzle.pieces[0];
        const int16_t x = static_cast<int16_t>(cargo_view::kHoldX + piece.x * cargo_view::kCell);
        const int16_t y = static_cast<int16_t>(cargo_view::kHoldY + piece.y * cargo_view::kCell);
        test.assertEqual(pixel(buffer, x, y), 1u, "held outline is drawn");
        test.assertEqual(pixel(buffer, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 2)), 0u, "held cell is hollow");
        test.assertEqual(pixel(buffer, static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 2)), 1u, "held centre cue");
        runner.add(test);
    }

    {
        Test test("tray previews unplaced pieces and marks the selection");
        uint8_t buffer[cargo_view::kBufferBytes];
        cargo_view::drawFrame(shell.puzzle, 2, buffer);

        bool slotHasArt = true;
        for (uint8_t slot = 0; slot < shell.puzzle.pieceCount; ++slot) {
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
        test.assertEqual(pixel(buffer, selX, static_cast<int16_t>(cargo_view::kTrayY + cargo_view::kTraySlot)), 1u, "selection underline starts");
        test.assertEqual(pixel(buffer, static_cast<int16_t>(selX + cargo_view::kTraySlot - 1),
                               static_cast<int16_t>(cargo_view::kTrayY + cargo_view::kTraySlot)),
                         1u, "selection underline ends");
        test.assertEqual(pixel(buffer, static_cast<int16_t>(selX + cargo_view::kTraySlot), static_cast<int16_t>(cargo_view::kTrayY + cargo_view::kTraySlot)),
                         0u, "selection underline does not bleed into the next slot");
        runner.add(test);
    }

    {
        Test test("selected installed crate gets an inset frame");
        cargo::CargoPuzzle puzzle = shell.puzzle;
        test.assertTrue(cargo::place(puzzle, 3, 0, 2), "install the bottom-left single crate");

        const int16_t x = cargo_view::kHoldX;
        const int16_t y = static_cast<int16_t>(cargo_view::kHoldY + 2 * cargo_view::kCell);

        uint8_t unselected[cargo_view::kBufferBytes];
        cargo_view::drawFrame(puzzle, 0, unselected);
        test.assertEqual(pixel(unselected, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1)), 1u,
                         "unselected crate keeps a solid cell");

        uint8_t selected[cargo_view::kBufferBytes];
        cargo_view::drawFrame(puzzle, 3, selected);
        test.assertEqual(pixel(selected, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1)), 0u,
                         "selected crate inset frame is black");
        test.assertEqual(pixel(selected, static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 2)), 1u,
                         "selected crate centre stays solid");
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
        runner.add(test);
    }

    {
        Test test("frame composition is deterministic and state-sensitive");
        uint8_t first[cargo_view::kBufferBytes];
        uint8_t second[cargo_view::kBufferBytes];
        cargo_view::drawFrame(shell.puzzle, 1, first);
        cargo_view::drawFrame(shell.puzzle, 1, second);
        test.assertTrue(memcmp(first, second, cargo_view::kBufferBytes) == 0, "same state composes the same frame");

        cargo_view::drawFrame(shell.puzzle, 3, second);
        test.assertTrue(memcmp(first, second, cargo_view::kBufferBytes) != 0, "selection changes the frame");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_CARGO_LOAD

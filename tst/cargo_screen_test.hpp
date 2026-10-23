#pragma once

#include "../src/cargo_screen.hpp"
#include "../src/features.hpp"

#if SH_FEATURE_CARGO_LOAD

#include "cargo_test.hpp"
#include "test.hpp"

// Pure cargo interaction rules: selection, suspended movement, support,
// blocked cells, lift blocking, rotation, undo, restart, and counters. No
// drawing and no Arduino headers.
inline void CargoScreenSuite(TestRunner &runner) {
    static_assert(sizeof(cargo_screen::State) <= sizeof(cargo::CargoPuzzle) + 8,
                  "screen state stays one puzzle plus alignment");

    const auto loadHauler = [](cargo_screen::State &state, cargo_test::FakeBank &bank) {
        cargo_test::loadAllHulls(bank);
        return cargo_screen::load(state, bank);
    };

    {
        Test test("selection cycles every piece, including installed cargo");
        cargo_test::FakeBank bank;
        cargo_screen::State state;
        test.assertTrue(loadHauler(state, bank), "load HAULER arrangement");
        test.assertEqual(state.selected, 0, "selection starts at the first piece");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::None),
                         "fresh state has no feedback");

        // Install piece 1 so installed cargo is part of the wrap.
        const cargo::CargoPiece &installed = state.puzzle.pieces[1];
        test.assertTrue(cargo::place(state.puzzle, 1, installed.x, installed.y), "install piece 1");

        uint8_t seen = 0;
        state.selected = 0;
        for (uint8_t i = 0; i < state.puzzle.pieceCount; ++i) {
            seen = static_cast<uint8_t>(seen | (1u << state.selected));
            test.assertTrue(cargo_screen::selectNext(state), "next selection");
        }
        test.assertEqual(seen, 0x7F, "the cycle visits all seven piece slots");
        test.assertEqual(state.selected, 0, "the cycle wraps to the first piece");

        test.assertTrue(cargo_screen::selectPrevious(state), "previous wraps");
        test.assertEqual(state.selected, 6, "previous from 0 selects the last piece");
        runner.add(test);
    }

    {
        Test test("pick, move, support, blocked cell, and release");
        cargo_test::FakeBank bank;
        cargo_screen::State state;
        test.assertTrue(loadHauler(state, bank), "load HAULER arrangement");

        // Piece 3 is the single crate whose bank target is mid-air at (10, 1).
        state.selected = 3;
        test.assertTrue(cargo_screen::pressA(state), "pick the single crate");
        test.assertEqual(state.puzzle.held, 3, "piece 3 is held");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::Picked), "pick feedback");

        test.assertTrue(!cargo_screen::moveHeld(state, 0, -1), "up into the blocked col 10 row 0 is refused");
        test.assertEqual(state.puzzle.pieces[3].y, 1, "blocked move leaves y unchanged");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::Blocked), "blocked feedback");

        test.assertTrue(cargo_screen::moveHeld(state, -1, 0), "move the crate left into open air");
        test.assertEqual(state.puzzle.pieces[3].x, 9, "held x moved");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::NeedsSupport),
                         "suspended over open air needs support");

        test.assertTrue(!cargo_screen::pressA(state), "unsupported release is refused");
        test.assertEqual(state.puzzle.held, 3, "refused release keeps the crate held");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::NeedsSupport),
                         "release feedback is NEEDS SUPPORT");

        test.assertTrue(cargo_screen::moveHeld(state, 0, 1), "lower the crate to the floor");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::None), "floor support clears feedback");
        test.assertTrue(cargo_screen::pressA(state), "release on the floor");
        test.assertEqual(state.puzzle.held, -1, "release clears held");
        test.assertTrue(cargo::piecePlaced(state.puzzle.pieces[3]), "released crate is installed");
        test.assertEqual(state.puzzle.requiredPlaced, 1, "required counter increments");
        test.assertEqual(state.puzzle.loadedCells, 1, "loaded counter increments");

        // Lift the installed single crate again and prove the movement clamp.
        test.assertTrue(cargo_screen::pressA(state), "lift the installed crate");
        test.assertEqual(state.puzzle.held, 3, "installed crate is held again");
        test.assertTrue(cargo_screen::moveHeld(state, 1, 0), "move to the rightmost hold column");
        test.assertEqual(state.puzzle.pieces[3].x, 10, "right edge reached");
        test.assertTrue(!cargo_screen::moveHeld(state, 1, 0), "further right is clamped");
        test.assertEqual(state.puzzle.pieces[3].x, 10, "clamp keeps x inside the hold");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::Blocked), "clamp feedback");
        runner.add(test);
    }

    {
        Test test("rotation is clockwise, reversible, and refuses no-room turns");
        cargo_test::FakeBank bank;
        cargo_screen::State state;
        test.assertTrue(loadHauler(state, bank), "load HAULER arrangement");

        state.selected = 1;
        test.assertTrue(cargo_screen::pressA(state), "pick piece 1");
        test.assertEqual(cargo::pieceRotation(state.puzzle.pieces[1]), 0, "starts unrotated");
        test.assertTrue(cargo_screen::pressB(state, true), "rotate clockwise");
        test.assertEqual(cargo::pieceRotation(state.puzzle.pieces[1]), 1, "rotation advances");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::Rotated), "rotation feedback");
        test.assertTrue(cargo_screen::pressB(state, false), "rotate counter-clockwise");
        test.assertEqual(cargo::pieceRotation(state.puzzle.pieces[1]), 0, "counter-rotation restores orientation");

        // Piece 0 is 4x3; a quarter turn is 3x4 and cannot fit a 3-row hold.
        cargo_screen::restart(state);
        state.selected = 0;
        test.assertTrue(cargo_screen::pressA(state), "pick piece 0");
        const uint16_t before = cargo::pieceMask(state.puzzle.pieces[0]);
        test.assertTrue(!cargo_screen::pressB(state, true), "no-room rotation is refused");
        test.assertEqual(cargo::pieceMask(state.puzzle.pieces[0]), before, "refused rotation keeps the mask");
        test.assertEqual(cargo::pieceRotation(state.puzzle.pieces[0]), 0, "refused rotation keeps the count");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::NoRoom), "no-room feedback");
        runner.add(test);
    }

    {
        Test test("installed lift is blocked by cargo directly above");
        cargo_test::FakeBank bank;
        cargo_screen::State state;
        test.assertTrue(loadHauler(state, bank), "load HAULER arrangement");

        test.assertTrue(cargo::place(state.puzzle, 3, 0, 2), "place bottom single");
        test.assertTrue(cargo::place(state.puzzle, 5, 0, 1), "place single above");
        state.selected = 3;
        test.assertTrue(!cargo_screen::pressA(state), "blocked lift is refused");
        test.assertEqual(state.puzzle.held, -1, "nothing is held");
        test.assertEqual(state.puzzle.requiredPlaced, 2, "both crates stay installed");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::LiftBlocked),
                         "lift-blocked feedback");

        state.selected = 5;
        test.assertTrue(cargo_screen::pressA(state), "the top crate lifts");
        test.assertEqual(state.puzzle.held, 5, "top crate is held");
        test.assertEqual(state.puzzle.requiredPlaced, 1, "top lift decrements the counter");
        test.assertTrue(cargo::setHeldPosition(state.puzzle, 10, 2), "move the held crate to the far floor");
        test.assertTrue(cargo_screen::pressA(state), "release the top crate on the far floor");
        test.assertEqual(state.puzzle.requiredPlaced, 2, "released crate counts again");

        state.selected = 3;
        test.assertTrue(cargo_screen::pressA(state), "bottom crate lifts once the top is gone");
        test.assertEqual(state.puzzle.held, 3, "bottom crate is held");
        test.assertEqual(state.puzzle.requiredPlaced, 1, "bottom lift decrements the counter");
        runner.add(test);
    }

    {
        Test test("undo, restart, and completion counters");
        cargo_test::FakeBank bank;
        cargo_screen::State state;
        test.assertTrue(loadHauler(state, bank), "load HAULER arrangement");

        const uint8_t targetOrder[7] = {4, 5, 0, 1, 2, 3, 6};
        for (uint8_t i = 0; i < 6; ++i) {
            const uint8_t index = targetOrder[i];
            const cargo::CargoPiece &piece = state.puzzle.pieces[index];
            test.assertTrue(cargo::place(state.puzzle, index, piece.x, piece.y), "target piece installs");
        }
        test.assertEqual(state.puzzle.requiredPlaced, 6, "all required crates installed");
        test.assertEqual(state.puzzle.bonusPlaced, 0, "bonus crate is still in the tray");
        test.assertEqual(state.puzzle.loadedCells, 20, "required cells loaded");

        state.selected = 6;
        test.assertTrue(cargo_screen::pressA(state), "pick the bonus crate");
        test.assertTrue(cargo_screen::pressA(state), "release the bonus crate");
        test.assertEqual(state.puzzle.requiredPlaced, 6, "required count");
        test.assertEqual(state.puzzle.bonusPlaced, 1, "bonus count");
        test.assertEqual(state.puzzle.loadedCells, 26, "loaded cells reach the target");
        test.assertTrue(cargo::requiredComplete(state.puzzle), "required contract complete");
        test.assertTrue(cargo::allComplete(state.puzzle), "bonus complete too");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::Complete),
                         "complete feedback");

        test.assertTrue(cargo_screen::pressB(state, true), "undo the last placement");
        test.assertEqual(state.puzzle.bonusPlaced, 0, "undo removed the bonus crate");
        test.assertEqual(state.puzzle.requiredPlaced, 6, "undo kept required crates");
        test.assertEqual(state.puzzle.loadedCells, 20, "undo removed six bonus cells");
        test.assertEqual(state.selected, 6, "undo selects the removed crate");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::Undone), "undo feedback");

        test.assertTrue(cargo_screen::pressB(state, true), "undo the next placement");
        test.assertEqual(state.puzzle.requiredPlaced, 5, "required count drops");
        test.assertEqual(state.puzzle.loadedCells, 19, "single crate cell removed");

        cargo_screen::restart(state);
        test.assertEqual(state.puzzle.held, -1, "restart clears held");
        test.assertEqual(state.puzzle.grid, 0, "restart clears the grid");
        test.assertEqual(state.puzzle.requiredPlaced, 0, "restart clears required count");
        test.assertEqual(state.puzzle.bonusPlaced, 0, "restart clears bonus count");
        test.assertEqual(state.puzzle.loadedCells, 0, "restart clears loaded cells");
        test.assertEqual(state.puzzle.orderSize, 0, "restart clears undo order");
        test.assertEqual(state.selected, 0, "restart resets selection");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::None), "restart clears feedback");
        test.assertEqual(cargo::pieceRotation(state.puzzle.pieces[0]), 0, "restart restores orientation");
        test.assertEqual(state.puzzle.pieces[0].x, 1, "restart restores target x");
        test.assertEqual(state.puzzle.pieces[0].y, 0, "restart restores target y");

        test.assertTrue(!cargo_screen::pressB(state, true), "nothing left to undo");
        test.assertEqual(static_cast<int>(state.message), static_cast<int>(cargo_screen::Message::NothingToUndo),
                         "no-undo feedback");
        runner.add(test);
    }

    {
        Test test("apply maps D-pad, A, B, and the CCW modifier");
        cargo_test::FakeBank bank;
        cargo_screen::State state;
        test.assertTrue(loadHauler(state, bank), "load HAULER arrangement");

        cargo_screen::Input input{};
        input.right = true;
        test.assertTrue(cargo_screen::apply(state, input), "D-pad right with nothing held selects");
        test.assertEqual(state.selected, 1, "selection advances");
        input.right = false;
        input.up = true;
        test.assertTrue(cargo_screen::apply(state, input), "D-pad up cycles backwards");
        test.assertEqual(state.selected, 0, "selection goes back");
        input.up = false;
        input.down = true;
        test.assertTrue(cargo_screen::apply(state, input), "D-pad down cycles forwards");
        test.assertEqual(state.selected, 1, "selection advances again");

        input.down = false;
        input.a = true;
        test.assertTrue(cargo_screen::apply(state, input), "A picks a piece");
        test.assertEqual(state.puzzle.held, 1, "piece 1 held");
        input.a = false;
        input.b = true;
        input.ccw = true;
        test.assertTrue(cargo_screen::apply(state, input), "B+UP rotates counter-clockwise");
        test.assertEqual(cargo::pieceRotation(state.puzzle.pieces[1]), 3, "CCW is one quarter the other way");
        input.ccw = false;
        test.assertTrue(cargo_screen::apply(state, input), "B rotates clockwise");
        test.assertEqual(cargo::pieceRotation(state.puzzle.pieces[1]), 0, "CW returns to the original orientation");

        input.b = false;
        input.right = true;
        test.assertTrue(cargo_screen::apply(state, input), "D-pad moves a held piece");
        test.assertEqual(state.puzzle.pieces[1].x, 7, "held piece moved instead of changing selection");
        test.assertEqual(state.selected, 1, "selection is stable while holding");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_CARGO_LOAD

#pragma once

#include "../src/cargo.hpp"
#include "test.hpp"

namespace cargo_test {

// Host bank: the cargo table in memory plus one observable cart-read call.
struct FakeBank {
    uint8_t bytes[CARGO_TABLE_BYTES];
    uint16_t lastOffset;
    uint16_t reads;

    FakeBank() : lastOffset(0), reads(0) {
        for (uint16_t i = 0; i < CARGO_TABLE_BYTES; ++i)
            bytes[i] = 0;
    }

    bool readCargo(uint16_t offset, uint8_t *dst, uint8_t count) {
        if (static_cast<uint32_t>(offset) + count > CARGO_TABLE_BYTES)
            return false;
        for (uint8_t i = 0; i < count; ++i)
            dst[i] = bytes[offset + i];
        lastOffset = offset;
        ++reads;
        return true;
    }

    void install(uint16_t offset, const uint8_t *record, uint8_t count) {
        for (uint8_t i = 0; i < count; ++i)
            bytes[offset + i] = record[i];
    }
};

// Real first records from fxdata/tables/cargo.bin.
constexpr uint8_t MINNOW_RECORD[12] = {0x17, 0x00, 0x02, 0x01, 0x00, 0x10, 0x27, 0x00, 0x13, 0x07, 0x00, 0x61};
constexpr uint8_t HAULER_RECORD[21] = {0x47, 0x0c, 0x01, 0x33, 0x03, 0x06, 0x33, 0x00, 0x08, 0x01, 0x00,
                                       0x1a, 0x03, 0x00, 0x28, 0x01, 0x00, 0x2a, 0x77, 0x00, 0x50};
constexpr uint8_t LEVIATHAN_RECORD[24] = {0x71, 0x74, 0x03, 0x07, 0x00, 0x08, 0x17, 0x07, 0x10, 0x63, 0x00, 0x18,
                                          0xf1, 0x00, 0x28, 0x03, 0x00, 0x3c, 0xef, 0x00, 0x5a, 0x0f, 0x00, 0x61};

inline uint8_t holdBits(uint64_t mask) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < 64; ++i) {
        if (mask & (uint64_t(1) << i))
            ++count;
    }
    return count;
}

inline void loadAllHulls(FakeBank &bank) {
    bank.install(CARGO_MINNOW_OFFSET, MINNOW_RECORD, CARGO_MINNOW_RECORD_BYTES);
    bank.install(CARGO_HAULER_OFFSET, HAULER_RECORD, CARGO_HAULER_RECORD_BYTES);
    bank.install(CARGO_LEVIATHAN_OFFSET, LEVIATHAN_RECORD, CARGO_LEVIATHAN_RECORD_BYTES);
}

// Install every required piece at its bank target. The generated target is
// supported by the floor/structural hull/another target piece, so a few passes
// in bank order settle every hull without a per-class order table. Bonus cargo
// stays optional, matching the commit rule.
inline bool solveRequired(cargo::CargoPuzzle &puzzle) {
    uint8_t remaining = 0;
    for (uint8_t i = 0; i < puzzle.pieceCount; ++i) {
        if (!cargo::pieceBonus(puzzle.pieces[i]) && !cargo::piecePlaced(puzzle.pieces[i]))
            ++remaining;
    }

    for (uint8_t pass = 0; pass < puzzle.pieceCount && remaining > 0; ++pass) {
        bool progress = false;
        for (uint8_t i = 0; i < puzzle.pieceCount; ++i) {
            if (cargo::pieceBonus(puzzle.pieces[i]) || cargo::piecePlaced(puzzle.pieces[i]))
                continue;
            const cargo::CargoPiece &piece = puzzle.pieces[i];
            if (!cargo::place(puzzle, i, piece.x, piece.y))
                continue;
            --remaining;
            progress = true;
        }
        if (!progress)
            break;
    }
    return remaining == 0;
}

}   // namespace cargo_test

inline void CargoSuite(TestRunner &runner) {
    static_assert(sizeof(cargo::CargoPiece) <= 6, "piece record stays 6 bytes");
    static_assert(sizeof(cargo::CargoPuzzle) <= 100, "puzzle state stays under 100 bytes");
    static_assert(cargo::MAX_PIECES == 8, "LEVIATHAN has the most pieces");
    static_assert(cargo::HULLS[cargo::HULL_HAULER].recordBytes == 21, "HAULER record size");

    {
        Test test("mask bounds, area, normalization, and rotation");
        const cargo::MaskBounds bounds = cargo::maskBounds(0x0C47);
        test.assertEqual(bounds.minX, 0, "HAULER piece 0 starts at column 0");
        test.assertEqual(bounds.minY, 0, "HAULER piece 0 starts at row 0");
        test.assertEqual(bounds.width, 4, "HAULER piece 0 is four wide");
        test.assertEqual(bounds.height, 3, "HAULER piece 0 is three tall");
        test.assertEqual(cargo::maskArea(0x0C47), 6, "HAULER piece 0 has six cells");
        test.assertEqual(cargo::maskArea(0x0017), 4, "MINNOW piece 0 has four cells");

        const cargo::MaskBounds empty = cargo::maskBounds(0);
        test.assertEqual(empty.width, 0, "empty mask has no width");
        test.assertEqual(empty.height, 0, "empty mask has no height");

        test.assertEqual(cargo::normalizeMask(0x1110), 0x0111, "normalization shifts to (0, 0)");
        test.assertEqual(cargo::rotateMaskCW(0x0017), 0x0223, "clockwise rotation");
        test.assertEqual(cargo::rotateMaskCCW(0x0017), 0x0311, "counter-clockwise rotation");
        test.assertEqual(cargo::rotateMaskCCW(cargo::rotateMaskCW(0x0017)), 0x0017, "CW then CCW is identity");
        const uint16_t twice = cargo::rotateMaskCW(cargo::rotateMaskCW(0x0017));
        test.assertEqual(cargo::rotateMaskCW(twice), cargo::rotateMaskCCW(0x0017), "three CW equals one CCW");
        uint16_t four = 0x0017;
        for (uint8_t i = 0; i < 4; ++i)
            four = cargo::rotateMaskCW(four);
        test.assertEqual(four, 0x0017, "four quarter turns are identity");
        runner.add(test);
    }

    {
        Test test("hull hold masks match the roster");
        test.assertEqual(cargo_test::holdBits(cargo::HULLS[cargo::HULL_MINNOW].hold), 14, "MINNOW opens 14 cells");
        test.assertEqual(cargo_test::holdBits(cargo::HULLS[cargo::HULL_HAULER].hold), 28, "HAULER opens 28 cells");
        test.assertEqual(cargo_test::holdBits(cargo::HULLS[cargo::HULL_LEVIATHAN].hold), 44, "LEVIATHAN opens 44 cells");

        cargo::HullSpec spec;
        test.assertTrue(cargo::hullSpec(cargo::HULL_HAULER, spec), "HAULER spec exists");
        test.assertEqual(spec.offset, 768, "HAULER bank offset");
        test.assertEqual(spec.recordBytes, 21, "HAULER record bytes");
        test.assertTrue(!cargo::hullSpec(cargo::HULL_COUNT, spec), "unknown hull rejected");
        runner.add(test);
    }

    {
        Test test("load parses real records at generated offsets");
        cargo_test::FakeBank bank;
        cargo_test::loadAllHulls(bank);

        cargo::CargoPuzzle minnow{};
        test.assertTrue(cargo::loadArrangement(minnow, bank, cargo::HULL_MINNOW, 0), "parse MINNOW 0");
        test.assertEqual(bank.lastOffset, CARGO_MINNOW_OFFSET, "MINNOW reads its block offset");
        test.assertEqual(bank.reads, 1, "one record is one cart read");
        test.assertEqual(minnow.pieceCount, 4, "MINNOW piece count");
        test.assertEqual(minnow.requiredTotal, 3, "MINNOW required count");
        test.assertEqual(minnow.bonusTotal, 1, "MINNOW bonus count");
        test.assertEqual(minnow.load, 12, "MINNOW load");
        test.assertEqual(minnow.pieces[0].mask, 0x0017, "MINNOW piece 0 mask");
        test.assertEqual(minnow.pieces[0].x, 2, "MINNOW piece 0 x");
        test.assertEqual(minnow.pieces[0].y, 0, "MINNOW piece 0 y");
        test.assertTrue(!cargo::pieceBonus(minnow.pieces[0]), "MINNOW piece 0 is required");
        test.assertEqual(minnow.pieces[3].mask, 0x0007, "MINNOW piece 3 mask");
        test.assertEqual(minnow.pieces[3].x, 1, "MINNOW piece 3 x");
        test.assertEqual(minnow.pieces[3].y, 2, "MINNOW piece 3 y");
        test.assertTrue(cargo::pieceBonus(minnow.pieces[3]), "MINNOW piece 3 is bonus");

        cargo::CargoPuzzle hauler{};
        test.assertTrue(cargo::loadArrangement(hauler, bank, cargo::HULL_HAULER, 0), "parse HAULER 0");
        test.assertEqual(bank.lastOffset, CARGO_HAULER_OFFSET, "HAULER reads its block offset");
        test.assertEqual(hauler.cols, 11, "HAULER columns");
        test.assertEqual(hauler.rows, 3, "HAULER rows");
        test.assertEqual(hauler.pieceCount, 7, "HAULER piece count");
        test.assertEqual(hauler.requiredTotal, 6, "HAULER required count");
        test.assertEqual(hauler.bonusTotal, 1, "HAULER bonus count");
        test.assertEqual(hauler.load, 26, "HAULER load");
        test.assertEqual(hauler.pieces[0].mask, 0x0C47, "HAULER piece 0 mask");
        test.assertEqual(hauler.pieces[0].x, 1, "HAULER piece 0 x");
        test.assertEqual(hauler.pieces[0].y, 0, "HAULER piece 0 y");
        test.assertEqual(hauler.pieces[6].mask, 0x0077, "HAULER bonus mask");
        test.assertEqual(hauler.pieces[6].x, 0, "HAULER bonus x");
        test.assertEqual(hauler.pieces[6].y, 1, "HAULER bonus y");
        test.assertTrue(cargo::pieceBonus(hauler.pieces[6]), "last HAULER piece is bonus");
        test.assertTrue(!cargo::piecePlaced(hauler.pieces[0]), "loaded pieces start unplaced");
        test.assertEqual(hauler.held, -1, "nothing starts held");
        test.assertEqual(hauler.home[6], 0x10, "home packs the bank target");

        cargo::CargoPuzzle leviathan{};
        test.assertTrue(cargo::loadArrangement(leviathan, bank, cargo::HULL_LEVIATHAN, 0), "parse LEVIATHAN 0");
        test.assertEqual(bank.lastOffset, CARGO_LEVIATHAN_OFFSET, "LEVIATHAN reads its block offset");
        test.assertEqual(leviathan.pieceCount, 8, "LEVIATHAN piece count");
        test.assertEqual(leviathan.requiredTotal, 6, "LEVIATHAN required count");
        test.assertEqual(leviathan.bonusTotal, 2, "LEVIATHAN bonus count");
        test.assertEqual(leviathan.load, 40, "LEVIATHAN load");
        test.assertEqual(leviathan.pieces[0].mask, 0x7471, "LEVIATHAN piece 0 mask");
        test.assertEqual(leviathan.pieces[0].x, 3, "LEVIATHAN piece 0 x");
        test.assertEqual(leviathan.pieces[6].mask, 0x00EF, "LEVIATHAN first bonus mask");
        test.assertTrue(cargo::pieceBonus(leviathan.pieces[6]), "LEVIATHAN piece 6 is bonus");
        test.assertEqual(leviathan.pieces[7].mask, 0x000F, "LEVIATHAN second bonus mask");
        test.assertEqual(leviathan.pieces[7].x, 1, "LEVIATHAN second bonus x");
        test.assertEqual(leviathan.pieces[7].y, 2, "LEVIATHAN second bonus y");
        test.assertTrue(cargo::pieceBonus(leviathan.pieces[7]), "LEVIATHAN piece 7 is bonus");
        runner.add(test);
    }

    {
        Test test("loader rejects bad indices, reserved bits, and malformed records");
        cargo_test::FakeBank bank;
        cargo_test::loadAllHulls(bank);
        cargo::CargoPuzzle puzzle{};
        puzzle.load = 99;

        test.assertTrue(!cargo::loadArrangement(puzzle, bank, cargo::HULL_COUNT, 0), "bad hull rejected");
        test.assertTrue(!cargo::loadArrangement(puzzle, bank, cargo::HULL_HAULER, CARGO_HAULER_ARRANGEMENTS),
                        "bad arrangement index rejected");
        test.assertEqual(puzzle.load, 99, "failed load leaves puzzle untouched");

        bank.bytes[CARGO_HAULER_OFFSET + 2] |= 0x80;
        test.assertTrue(!cargo::loadArrangement(puzzle, bank, cargo::HULL_HAULER, 0), "reserved posKind bit rejected");
        bank.bytes[CARGO_HAULER_OFFSET + 2] &= 0x7F;

        bank.bytes[CARGO_HAULER_OFFSET] = 0;
        bank.bytes[CARGO_HAULER_OFFSET + 1] = 0;
        test.assertTrue(!cargo::loadArrangement(puzzle, bank, cargo::HULL_HAULER, 0), "empty mask rejected");
        runner.add(test);
    }

    {
        Test test("support, blocked cells, bounds, and overlap");
        cargo_test::FakeBank bank;
        cargo_test::loadAllHulls(bank);
        cargo::CargoPuzzle puzzle{};
        test.assertTrue(cargo::loadArrangement(puzzle, bank, cargo::HULL_HAULER, 0), "load HAULER fixture");

        test.assertTrue(cargo::inHold(puzzle, 0, 2), "bottom-left corner is open");
        test.assertTrue(cargo::inHold(puzzle, 10, 2), "bottom-right corner is open");
        test.assertTrue(!cargo::inHold(puzzle, 5, 2), "split bulkhead is blocked");
        test.assertTrue(!cargo::inHold(puzzle, 0, 0), "top-left corner is blocked");

        // Single-cell pieces 3 and 5: floor support, including the corners.
        test.assertTrue(cargo::canPlace(puzzle, 3, 0, 2), "floor supports bottom-left corner");
        test.assertTrue(cargo::canPlace(puzzle, 5, 10, 2), "floor supports bottom-right corner");
        test.assertTrue(!cargo::canPlace(puzzle, 3, 1, 0), "mid-air placement is rejected");
        test.assertTrue(!cargo::canPlace(puzzle, 3, 5, 2), "blocked structural cell is rejected");
        test.assertTrue(!cargo::canPlace(puzzle, 3, 11, 2), "out-of-bounds x is rejected");
        test.assertTrue(!cargo::canPlace(puzzle, 3, 0, 3), "out-of-bounds y is rejected");
        test.assertTrue(!cargo::canPlace(puzzle, 0, 8, 0), "wide shape cannot cross the hold edge");

        test.assertTrue(cargo::place(puzzle, 3, 0, 2), "place first floor piece");
        test.assertTrue(cargo::canPlace(puzzle, 5, 0, 1), "cargo below supports a stacked piece");
        test.assertTrue(!cargo::canPlace(puzzle, 5, 0, 2), "overlap is rejected even on the floor");
        test.assertTrue(cargo::place(puzzle, 5, 0, 1), "place stacked piece");
        test.assertEqual(puzzle.requiredPlaced, 2, "two required pieces placed");
        test.assertEqual(puzzle.loadedCells, 2, "two cells loaded");
        runner.add(test);
    }

    {
        Test test("rotation state and held movement");
        cargo_test::FakeBank bank;
        cargo_test::loadAllHulls(bank);
        cargo::CargoPuzzle puzzle{};
        test.assertTrue(cargo::loadArrangement(puzzle, bank, cargo::HULL_HAULER, 0), "load HAULER fixture");

        test.assertTrue(cargo::pickup(puzzle, 0), "pick up tray piece");
        test.assertEqual(puzzle.held, 0, "piece 0 is held");
        test.assertEqual(cargo::pieceMask(puzzle.pieces[0]), 0x0C47, "start orientation is the bank mask");
        test.assertTrue(cargo::rotateHeld(puzzle, true), "rotate held piece clockwise");
        test.assertEqual(cargo::pieceRotation(puzzle.pieces[0]), 1, "rotation count advances");
        test.assertTrue(cargo::pieceMask(puzzle.pieces[0]) != 0x0C47, "clockwise mask changes");
        test.assertTrue(cargo::rotateHeld(puzzle, false), "rotate held piece counter-clockwise");
        test.assertEqual(cargo::pieceMask(puzzle.pieces[0]), 0x0C47, "counter rotation restores the mask");
        test.assertTrue(cargo::moveHeld(puzzle, 2, 1), "move held piece");
        test.assertEqual(puzzle.pieces[0].x, 3, "held x moved");
        test.assertEqual(puzzle.pieces[0].y, 1, "held y moved");
        test.assertTrue(cargo::setHeldPosition(puzzle, 1, 0), "set held position");

        test.assertTrue(cargo::release(puzzle, 1, 0), "release on a supported target");
        test.assertEqual(puzzle.held, -1, "release clears held");
        test.assertTrue(cargo::piecePlaced(puzzle.pieces[0]), "released piece is placed");
        test.assertEqual(puzzle.requiredPlaced, 1, "placed count increments");

        test.assertTrue(cargo::pickup(puzzle, 0), "pick up the placed piece");
        test.assertEqual(puzzle.requiredPlaced, 0, "pickup decrements placed count");
        test.assertEqual(puzzle.loadedCells, 0, "pickup clears its cells");

        test.assertTrue(cargo::setHeldPosition(puzzle, 8, 0), "leave the held piece out of bounds");
        test.assertTrue(!cargo::release(puzzle), "out-of-bounds release is rejected");
        test.assertEqual(puzzle.held, 0, "failed release keeps the piece held");
        test.assertTrue(cargo::setHeldPosition(puzzle, 1, 0), "return to the supported target");
        test.assertTrue(cargo::release(puzzle), "release at the current position");
        runner.add(test);
    }

    {
        Test test("lift is blocked by cargo directly above");
        cargo_test::FakeBank bank;
        cargo_test::loadAllHulls(bank);
        cargo::CargoPuzzle puzzle{};
        test.assertTrue(cargo::loadArrangement(puzzle, bank, cargo::HULL_HAULER, 0), "load HAULER fixture");

        test.assertTrue(cargo::place(puzzle, 3, 0, 2), "place bottom single");
        test.assertTrue(cargo::place(puzzle, 5, 0, 1), "place single above");
        test.assertTrue(cargo::liftBlocked(puzzle, 3), "bottom piece sees cargo above");
        test.assertTrue(!cargo::pickup(puzzle, 3), "blocked lift is refused");
        test.assertEqual(puzzle.requiredPlaced, 2, "blocked lift leaves both placed");

        test.assertTrue(cargo::lift(puzzle, 5), "top piece lifts");
        test.assertEqual(puzzle.held, 5, "top piece is held");
        test.assertEqual(puzzle.requiredPlaced, 1, "top lift decrements count");
        test.assertTrue(!cargo::liftBlocked(puzzle, 3), "bottom piece is clear now");
        test.assertTrue(cargo::release(puzzle, 10, 2), "release the lifted piece on the far floor");
        test.assertEqual(puzzle.requiredPlaced, 2, "released piece counts again");
        test.assertTrue(cargo::lift(puzzle, 3), "bottom piece lifts");
        test.assertEqual(puzzle.held, 3, "bottom piece is held");
        test.assertEqual(puzzle.requiredPlaced, 1, "only the far piece remains placed");
        test.assertEqual(puzzle.loadedCells, 1, "only one cell remains loaded");
        runner.add(test);
    }

    {
        Test test("undo, reset, and completion counters");
        cargo_test::FakeBank bank;
        cargo_test::loadAllHulls(bank);
        cargo::CargoPuzzle puzzle{};
        test.assertTrue(cargo::loadArrangement(puzzle, bank, cargo::HULL_HAULER, 0), "load HAULER fixture");

        // Place the real target arrangement bottom-up, bonus piece last.
        const uint8_t order[7] = {4, 5, 0, 1, 2, 3, 6};
        for (uint8_t i = 0; i < 7; ++i) {
            const uint8_t index = order[i];
            const cargo::CargoPiece &piece = puzzle.pieces[index];
            test.assertTrue(cargo::place(puzzle, index, piece.x, piece.y), "target piece places");
        }
        test.assertEqual(puzzle.grid, cargo::HULLS[cargo::HULL_HAULER].hold & puzzle.grid,
                         "target only fills open hold cells");
        test.assertEqual(cargo_test::holdBits(puzzle.grid), 26, "target fills the 26-cell load");
        test.assertTrue(puzzle.grid != cargo::HULLS[cargo::HULL_HAULER].hold, "two open cells remain slack");
        test.assertEqual(puzzle.requiredPlaced, 6, "all required pieces placed");
        test.assertEqual(puzzle.bonusPlaced, 1, "bonus piece placed");
        test.assertEqual(puzzle.loadedCells, 26, "target loads 26 cells");
        test.assertTrue(cargo::requiredComplete(puzzle), "required job is complete");
        test.assertTrue(cargo::allComplete(puzzle), "bonus is complete too");
        test.assertTrue(cargo::loadComplete(puzzle), "load target reached");

        test.assertTrue(cargo::undo(puzzle), "undo the last placement");
        test.assertEqual(puzzle.bonusPlaced, 0, "undo removed the bonus piece");
        test.assertEqual(puzzle.requiredPlaced, 6, "undo kept the required pieces");
        test.assertTrue(cargo::requiredComplete(puzzle), "required job still complete");
        test.assertTrue(!cargo::allComplete(puzzle), "bonus is no longer complete");
        test.assertEqual(puzzle.loadedCells, 20, "undo removed six bonus cells");
        test.assertTrue(cargo::undo(puzzle), "undo removes the next required placement");
        test.assertEqual(puzzle.requiredPlaced, 5, "required count drops after the second undo");

        test.assertTrue(cargo::pickup(puzzle, 0), "hold a placed piece before reset");
        test.assertTrue(cargo::rotateHeld(puzzle, true), "rotate before reset");
        cargo::reset(puzzle);
        test.assertEqual(puzzle.held, -1, "reset clears held");
        test.assertEqual(puzzle.grid, 0, "reset clears the grid");
        test.assertEqual(puzzle.requiredPlaced, 0, "reset clears required count");
        test.assertEqual(puzzle.bonusPlaced, 0, "reset clears bonus count");
        test.assertEqual(puzzle.loadedCells, 0, "reset clears loaded cells");
        test.assertEqual(puzzle.orderSize, 0, "reset clears undo order");
        test.assertEqual(cargo::pieceRotation(puzzle.pieces[0]), 0, "reset restores orientation");
        test.assertEqual(puzzle.pieces[0].x, 1, "reset restores target x");
        test.assertEqual(puzzle.pieces[0].y, 0, "reset restores target y");
        test.assertTrue(cargo::pieceBonus(puzzle.pieces[6]), "reset preserves the bonus flag");
        test.assertTrue(!cargo::piecePlaced(puzzle.pieces[6]), "reset clears placed flags");
        runner.add(test);
    }
}

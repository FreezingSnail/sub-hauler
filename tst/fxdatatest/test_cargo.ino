// Device fixture for the cargo arrangement loader (sub-hauler-m4-loader).
//
// Reads HAULER arrangement 0 through cargo::FxCargoReader in setup, outside
// every FX::display transaction, then checks the known mask, bbox, target
// position, bonus flag, and hold geometry. The M8 holds task then loads each
// class record and checks its hold scale, open floor, and a blocked cell.
// Prints exactly one bare P/F marker for the Ardens serial harness. No cargo
// UI is drawn.
#include <Arduboy2.h>
#include <ArduboyFX.h>

#include "src/cargo_fx.hpp"

Arduboy2 arduboy;

static_assert(sizeof(cargo::CargoPiece) <= 6, "CargoPiece stays compact");
static_assert(sizeof(cargo::CargoPuzzle) <= 100, "CargoPuzzle stays under ~100 B");

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    cargo::CargoPuzzle puzzle{};
    cargo::FxCargoReader reader;

    bool ok = cargo::loadArrangement(puzzle, reader, cargo::HULL_HAULER, 0);

    // Record shape and counters.
    ok = ok && puzzle.cols == CARGO_HAULER_COLS;
    ok = ok && puzzle.rows == CARGO_HAULER_ROWS;
    ok = ok && puzzle.pieceCount == CARGO_HAULER_PIECES;
    ok = ok && puzzle.requiredTotal == CARGO_HAULER_REQUIRED;
    ok = ok && puzzle.bonusTotal == CARGO_HAULER_BONUS;
    ok = ok && puzzle.load == CARGO_HAULER_LOAD;
    ok = ok && puzzle.held == -1;

    // First real piece: mask 0x0C47, x = 1, y = 0, required.
    const cargo::MaskBounds first = cargo::maskBounds(puzzle.pieces[0].mask);
    ok = ok && puzzle.pieces[0].mask == 0x0C47;
    ok = ok && first.width == 4 && first.height == 3;
    ok = ok && cargo::maskArea(puzzle.pieces[0].mask) == 6;
    ok = ok && puzzle.pieces[0].x == 1 && puzzle.pieces[0].y == 0;
    ok = ok && !cargo::pieceBonus(puzzle.pieces[0]);

    // Last piece is the bonus crate at (0, 1).
    ok = ok && puzzle.pieces[6].mask == 0x0077;
    ok = ok && puzzle.pieces[6].x == 0 && puzzle.pieces[6].y == 1;
    ok = ok && cargo::pieceBonus(puzzle.pieces[6]);
    ok = ok && puzzle.home[6] == 0x10;

    // Hull structure: corner floor cells open, split bulkhead blocked.
    ok = ok && cargo::inHold(puzzle, 0, 2);
    ok = ok && cargo::inHold(puzzle, 10, 2);
    ok = ok && !cargo::inHold(puzzle, 5, 2);
    ok = ok && !cargo::inHold(puzzle, 0, 0);

    // One real rule probe: a single-cell required piece rests on the floor at
    // the bottom-left corner and cannot sit in the blocked bulkhead.
    ok = ok && cargo::canPlace(puzzle, 3, 0, 2);
    ok = ok && !cargo::canPlace(puzzle, 3, 5, 2);

    ok = ok && !cargo::requiredComplete(puzzle);
    ok = ok && !cargo::allComplete(puzzle);

    // All three M8 class records: dimensions and hold structure match the
    // generated class contract. The bottom row is open at column 1 in every
    // hold; the top-right cell is structural in every hold.
    const uint8_t hullIds[3] = {cargo::HULL_MINNOW, cargo::HULL_HAULER, cargo::HULL_LEVIATHAN};
    const uint8_t hullCols[3] = {CARGO_MINNOW_COLS, CARGO_HAULER_COLS, CARGO_LEVIATHAN_COLS};
    const uint8_t hullRows[3] = {CARGO_MINNOW_ROWS, CARGO_HAULER_ROWS, CARGO_LEVIATHAN_ROWS};
    const uint8_t hullPieces[3] = {CARGO_MINNOW_PIECES, CARGO_HAULER_PIECES, CARGO_LEVIATHAN_PIECES};
    const uint8_t hullLoads[3] = {CARGO_MINNOW_LOAD, CARGO_HAULER_LOAD, CARGO_LEVIATHAN_LOAD};

    for (uint8_t i = 0; i < 3; ++i) {
        cargo::CargoPuzzle classPuzzle{};
        ok = ok && cargo::loadArrangement(classPuzzle, reader, hullIds[i], 0);
        ok = ok && classPuzzle.cols == hullCols[i];
        ok = ok && classPuzzle.rows == hullRows[i];
        ok = ok && classPuzzle.pieceCount == hullPieces[i];
        ok = ok && classPuzzle.load == hullLoads[i];
        ok = ok && classPuzzle.hold == cargo::HULLS[hullIds[i]].hold;
        ok = ok && classPuzzle.held == -1;

        ok = ok && cargo::inHold(classPuzzle, 1, static_cast<uint8_t>(hullRows[i] - 1));
        ok = ok && !cargo::inHold(classPuzzle, static_cast<uint8_t>(hullCols[i] - 1), 0);

        cargo::CargoPuzzle probe = classPuzzle;
        probe.pieceCount = 1;
        probe.grid = 0;
        probe.held = -1;
        probe.pieces[0].mask = 0x0001;
        probe.pieces[0].x = static_cast<uint8_t>(hullCols[i] - 1);
        probe.pieces[0].y = 0;
        probe.pieces[0].flags = 0;
        ok = ok && !cargo::canPlace(probe, 0, probe.pieces[0].x, 0);
    }

    Serial.println(ok ? F("P") : F("F"));
}

void loop() {
}

#pragma once

#include <stdint.h>

#include "cargo.hpp"
#include "features.hpp"

#if SH_FEATURE_CARGO_LOAD

// Fixed cargo screen state and pure input rules. One HAULER arrangement is
// loaded through the same Reader abstraction as cargo::loadArrangement, so
// host tests inject a bank and the device supplies cargo::FxCargoReader.
//
// Input map (Arduboy pad has no spare key, so R/N are omitted):
//   D-pad  empty-handed: cycle the selection across every piece, including
//          installed cargo; holding a piece: move it, clamped to the hold.
//   A      pick up / lift the selected piece, or release the held piece.
//          Release is refused without at least one supported bottom cell.
//   B      rotate the held piece clockwise (counter-clockwise with UP as a
//          modifier); with nothing held, undo the last placement.
//   HOLD B exits the screen (the sketch owns the hold timer).
namespace cargo_screen {

enum class Message : uint8_t {
    None,
    Picked,
    Lifted,
    Released,
    Rotated,
    Undone,
    NeedsSupport,
    Blocked,
    LiftBlocked,
    NoRoom,
    NothingToUndo,
    NoCrate,
    Complete,
};

struct State {
    cargo::CargoPuzzle puzzle;
    uint8_t hull;
    uint8_t arrangement;
    uint8_t selected;
    bool loaded;
    Message message;
};

struct Input {
    bool up;
    bool down;
    bool left;
    bool right;
    bool a;
    bool b;
    bool ccw;
};

static_assert(sizeof(State) <= sizeof(cargo::CargoPuzzle) + 8, "screen state stays within one puzzle plus alignment");

inline void reset(State &state) {
    state.puzzle = cargo::CargoPuzzle{};
    state.hull = cargo::HULL_HAULER;
    state.arrangement = 0;
    state.selected = 0;
    state.loaded = false;
    state.message = Message::None;
}

// Load HAULER arrangement 0. The caller supplies the Reader so this header
// stays free of Arduino/FX headers.
template <typename Reader> inline bool load(State &state, Reader &reader) {
    reset(state);
    if (!cargo::loadArrangement(state.puzzle, reader, cargo::HULL_HAULER, 0))
        return false;
    state.loaded = true;
    return true;
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

inline void normalizeSelection(State &state) {
    if (state.puzzle.pieceCount == 0)
        state.selected = 0;
    else if (state.selected >= state.puzzle.pieceCount)
        state.selected = 0;
}

inline bool selectNext(State &state) {
    if (state.puzzle.pieceCount == 0) {
        state.selected = 0;
        return false;
    }
    normalizeSelection(state);
    ++state.selected;
    if (state.selected >= state.puzzle.pieceCount)
        state.selected = 0;
    return true;
}

inline bool selectPrevious(State &state) {
    if (state.puzzle.pieceCount == 0) {
        state.selected = 0;
        return false;
    }
    normalizeSelection(state);
    if (state.selected == 0)
        state.selected = static_cast<uint8_t>(state.puzzle.pieceCount - 1);
    else
        --state.selected;
    return true;
}

// ---------------------------------------------------------------------------
// Held-piece geometry
// ---------------------------------------------------------------------------

// A suspended piece may be unsupported, but it must stay inside the open hold
// and clear every installed crate. Structural mask cells are never open.
inline bool heldFits(const cargo::CargoPuzzle &puzzle, uint16_t mask, uint8_t x, uint8_t y) {
    const cargo::MaskBounds bounds = cargo::maskBounds(mask);
    if (bounds.width == 0)
        return false;
    if (static_cast<uint16_t>(x) + bounds.width > puzzle.cols)
        return false;
    if (static_cast<uint16_t>(y) + bounds.height > puzzle.rows)
        return false;

    const uint64_t cells = cargo::shapeCells(puzzle, mask, x, y);
    if ((cells & ~puzzle.hold) != 0)
        return false;
    if ((cells & puzzle.grid) != 0)
        return false;
    return true;
}

inline bool heldFits(const cargo::CargoPuzzle &puzzle, uint8_t index, uint8_t x, uint8_t y) {
    return heldFits(puzzle, cargo::pieceMask(puzzle.pieces[index]), x, y);
}

// Prefer the piece's stored spot; otherwise scan the hold for any suspended
// position. Returns false only when the shape cannot fit anywhere.
inline bool findHeldSpot(const cargo::CargoPuzzle &puzzle, uint8_t index, uint8_t &x, uint8_t &y) {
    const cargo::CargoPiece &piece = puzzle.pieces[index];
    const uint16_t mask = cargo::pieceMask(piece);
    if (heldFits(puzzle, mask, piece.x, piece.y)) {
        x = piece.x;
        y = piece.y;
        return true;
    }

    for (uint8_t cy = 0; cy < puzzle.rows; ++cy) {
        for (uint8_t cx = 0; cx < puzzle.cols; ++cx) {
            if (!heldFits(puzzle, mask, cx, cy))
                continue;
            x = cx;
            y = cy;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

inline bool moveHeld(State &state, int8_t dx, int8_t dy) {
    cargo::CargoPuzzle &puzzle = state.puzzle;
    if (puzzle.held < 0)
        return false;

    cargo::CargoPiece &piece = puzzle.pieces[puzzle.held];
    const uint16_t mask = cargo::pieceMask(piece);
    const cargo::MaskBounds bounds = cargo::maskBounds(mask);
    if (bounds.width == 0)
        return false;

    const int16_t maxX = static_cast<int16_t>(puzzle.cols - bounds.width);
    const int16_t maxY = static_cast<int16_t>(puzzle.rows - bounds.height);
    int16_t nx = static_cast<int16_t>(piece.x) + dx;
    int16_t ny = static_cast<int16_t>(piece.y) + dy;

    if (nx < 0)
        nx = 0;
    if (ny < 0)
        ny = 0;
    if (nx > maxX)
        nx = maxX;
    if (ny > maxY)
        ny = maxY;

    if (nx == piece.x && ny == piece.y) {
        state.message = Message::Blocked;
        return false;
    }

    const uint8_t x = static_cast<uint8_t>(nx);
    const uint8_t y = static_cast<uint8_t>(ny);
    if (!heldFits(puzzle, mask, x, y)) {
        state.message = Message::Blocked;
        return false;
    }

    piece.x = x;
    piece.y = y;
    state.message = cargo::supported(puzzle, mask, x, y) ? Message::None : Message::NeedsSupport;
    return true;
}

inline bool rotateHeld(State &state, bool clockwise) {
    cargo::CargoPuzzle &puzzle = state.puzzle;
    if (puzzle.held < 0)
        return false;

    cargo::CargoPiece &piece = puzzle.pieces[puzzle.held];
    const uint8_t oldFlags = piece.flags;
    uint8_t rotation = cargo::pieceRotation(piece);
    rotation = clockwise ? static_cast<uint8_t>((rotation + 1) & 3)
                         : static_cast<uint8_t>((rotation + 3) & 3);
    piece.flags =
        static_cast<uint8_t>((piece.flags & ~cargo::PIECE_ROT_MASK) | (rotation << cargo::PIECE_ROT_SHIFT));

    if (!heldFits(puzzle, cargo::pieceMask(piece), piece.x, piece.y)) {
        piece.flags = oldFlags;
        state.message = Message::NoRoom;
        return false;
    }

    state.message = Message::Rotated;
    return true;
}

// A: release the held piece, or pick up / lift the selected piece.
inline bool pressA(State &state) {
    cargo::CargoPuzzle &puzzle = state.puzzle;

    if (puzzle.held >= 0) {
        const cargo::CargoPiece &piece = puzzle.pieces[puzzle.held];
        if (!cargo::supported(puzzle, cargo::pieceMask(piece), piece.x, piece.y)) {
            state.message = Message::NeedsSupport;
            return false;
        }
        if (!cargo::canPlace(puzzle, static_cast<uint8_t>(puzzle.held), piece.x, piece.y)) {
            state.message = Message::Blocked;
            return false;
        }
        if (!cargo::release(puzzle)) {
            state.message = Message::Blocked;
            return false;
        }
        state.message = cargo::allComplete(puzzle) ? Message::Complete : Message::Released;
        return true;
    }

    normalizeSelection(state);
    if (puzzle.pieceCount == 0) {
        state.message = Message::NoCrate;
        return false;
    }

    const uint8_t index = state.selected;
    cargo::CargoPiece &piece = puzzle.pieces[index];

    if (cargo::piecePlaced(piece)) {
        if (cargo::liftBlocked(puzzle, index)) {
            state.message = Message::LiftBlocked;
            return false;
        }
        if (!cargo::pickup(puzzle, index)) {
            state.message = Message::Blocked;
            return false;
        }
        state.message = Message::Lifted;
        return true;
    }

    uint8_t x = piece.x;
    uint8_t y = piece.y;
    if (!findHeldSpot(puzzle, index, x, y)) {
        state.message = Message::NoRoom;
        return false;
    }
    if (!cargo::pickup(puzzle, index)) {
        state.message = Message::Blocked;
        return false;
    }
    cargo::setHeldPosition(puzzle, x, y);
    state.message = Message::Picked;
    return true;
}

// B: rotate a held piece, otherwise undo the last placement.
inline bool pressB(State &state, bool clockwise) {
    cargo::CargoPuzzle &puzzle = state.puzzle;

    if (puzzle.held >= 0)
        return rotateHeld(state, clockwise);

    if (puzzle.orderSize == 0) {
        state.message = Message::NothingToUndo;
        return false;
    }

    const uint8_t index = puzzle.order[puzzle.orderSize - 1];
    if (!cargo::undo(puzzle)) {
        state.message = Message::NothingToUndo;
        return false;
    }
    state.selected = index;
    state.message = Message::Undone;
    return true;
}

// Restore the target placement without another cart read. The device has no
// spare key for R, so this is a pure helper for now.
inline void restart(State &state) {
    cargo::reset(state.puzzle);
    state.selected = 0;
    state.message = Message::None;
}

// One just-pressed input frame. Button actions win over a simultaneous D-pad
// edge so picking/rotating never drags the piece on the same frame.
inline bool apply(State &state, const Input &input) {
    if (input.a)
        return pressA(state);
    if (input.b)
        return pressB(state, !input.ccw);

    cargo::CargoPuzzle &puzzle = state.puzzle;
    if (puzzle.held >= 0) {
        const int8_t dx = static_cast<int8_t>((input.right ? 1 : 0) - (input.left ? 1 : 0));
        const int8_t dy = static_cast<int8_t>((input.down ? 1 : 0) - (input.up ? 1 : 0));
        if (dx == 0 && dy == 0)
            return false;
        return moveHeld(state, dx, dy);
    }

    if (input.left && !input.right)
        return selectPrevious(state);
    if (input.right && !input.left)
        return selectNext(state);
    if (input.up && !input.down)
        return selectPrevious(state);
    if (input.down && !input.up)
        return selectNext(state);
    return false;
}

}   // namespace cargo_screen

#endif   // SH_FEATURE_CARGO_LOAD

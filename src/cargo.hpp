#pragma once

#include <stdint.h>

#include "generated/cargo.hpp"

// Fixed-memory cargo arrangement loader and pure puzzle rules. This header has
// no Arduino headers: host tests inject a Reader and device code supplies the
// FX adapter in cargo_fx.hpp.
//
// Cart contract: Reader::readCargo(offset, dst, count) reads count bytes from
// the cargo bank at a bank-relative offset. Calls may touch FX flash, so the
// owner must stay in the update/render phase and never read during display.
namespace cargo {

// ---------------------------------------------------------------------------
// Fixed records
// ---------------------------------------------------------------------------

constexpr uint8_t MAX_PIECES = 8;
constexpr uint8_t MAX_RECORD_BYTES = 24;

enum HullId : uint8_t {
    HULL_MINNOW = 0,
    HULL_HAULER = 1,
    HULL_LEVIATHAN = 2,
};

constexpr uint8_t HULL_COUNT = CARGO_HULL_COUNT;

enum PieceFlag : uint8_t {
    PIECE_BONUS = 0x01,
    PIECE_PLACED = 0x02,
    PIECE_ROT_MASK = 0x0C,
    PIECE_ROT_SHIFT = 2,
};

// Runtime piece. mask is the normalized bank shape; rotation is kept in flags
// so reset can restore the original shape without another cart read. x/y are
// the current top-left of the shape's bounding box.
struct CargoPiece {
    uint16_t mask;
    uint8_t x;
    uint8_t y;
    uint8_t flags;
};

// One selected arrangement plus its play state. hold is the open-cell bitmask
// (bit index y * cols + x); cleared hold bits are structural/blocked. grid is
// the placed-cargo bitmask. home packs the bank target x (bits 0-3) and y
// (bits 4-5) so reset restores the target placement.
struct CargoPuzzle {
    uint64_t hold;
    uint64_t grid;
    CargoPiece pieces[MAX_PIECES];
    uint8_t home[MAX_PIECES];
    int8_t held;
    uint8_t cols;
    uint8_t rows;
    uint8_t pieceCount;
    uint8_t requiredTotal;
    uint8_t bonusTotal;
    uint8_t requiredPlaced;
    uint8_t bonusPlaced;
    uint8_t load;
    uint8_t loadedCells;
    uint8_t orderSize;
    uint8_t order[MAX_PIECES];
};

static_assert(sizeof(CargoPiece) <= 6, "CargoPiece must stay compact");
static_assert(sizeof(CargoPuzzle) <= 100, "puzzle state must stay under ~100 B");

// ---------------------------------------------------------------------------
// Mask geometry
// ---------------------------------------------------------------------------

struct MaskBounds {
    uint8_t minX;
    uint8_t minY;
    uint8_t width;
    uint8_t height;
};

inline MaskBounds maskBounds(uint16_t mask) {
    uint8_t minX = 4;
    uint8_t minY = 4;
    uint8_t maxX = 0;
    uint8_t maxY = 0;
    bool found = false;

    for (uint8_t y = 0; y < 4; ++y) {
        for (uint8_t x = 0; x < 4; ++x) {
            if ((mask & (uint16_t(1) << (y * 4 + x))) == 0)
                continue;
            if (x < minX)
                minX = x;
            if (y < minY)
                minY = y;
            if (x > maxX)
                maxX = x;
            if (y > maxY)
                maxY = y;
            found = true;
        }
    }

    if (!found)
        return MaskBounds{0, 0, 0, 0};
    return MaskBounds{minX, minY, static_cast<uint8_t>(maxX - minX + 1), static_cast<uint8_t>(maxY - minY + 1)};
}

inline uint8_t maskArea(uint16_t mask) {
    uint8_t area = 0;
    for (uint8_t bit = 0; bit < 16; ++bit) {
        if (mask & (uint16_t(1) << bit))
            ++area;
    }
    return area;
}

// Shift a mask's occupied cells to a zero-origin 4x4 bounding box.
inline uint16_t normalizeMask(uint16_t mask) {
    const MaskBounds bounds = maskBounds(mask);
    if (bounds.width == 0)
        return 0;

    uint16_t out = 0;
    for (uint8_t y = 0; y < 4; ++y) {
        for (uint8_t x = 0; x < 4; ++x) {
            if ((mask & (uint16_t(1) << (y * 4 + x))) == 0)
                continue;
            const uint8_t nx = static_cast<uint8_t>(x - bounds.minX);
            const uint8_t ny = static_cast<uint8_t>(y - bounds.minY);
            out |= static_cast<uint16_t>(uint16_t(1) << (ny * 4 + nx));
        }
    }
    return out;
}

// Rotate within the mask's minimal bounding box, then normalize to (0, 0).
inline uint16_t rotateMaskCW(uint16_t mask) {
    const MaskBounds bounds = maskBounds(mask);
    if (bounds.width == 0)
        return 0;

    uint16_t out = 0;
    for (uint8_t y = 0; y < 4; ++y) {
        for (uint8_t x = 0; x < 4; ++x) {
            if ((mask & (uint16_t(1) << (y * 4 + x))) == 0)
                continue;
            const uint8_t px = static_cast<uint8_t>(x - bounds.minX);
            const uint8_t py = static_cast<uint8_t>(y - bounds.minY);
            const uint8_t nx = static_cast<uint8_t>(bounds.height - 1 - py);
            const uint8_t ny = px;
            out |= static_cast<uint16_t>(uint16_t(1) << (ny * 4 + nx));
        }
    }
    return out;
}

inline uint16_t rotateMaskCCW(uint16_t mask) {
    const MaskBounds bounds = maskBounds(mask);
    if (bounds.width == 0)
        return 0;

    uint16_t out = 0;
    for (uint8_t y = 0; y < 4; ++y) {
        for (uint8_t x = 0; x < 4; ++x) {
            if ((mask & (uint16_t(1) << (y * 4 + x))) == 0)
                continue;
            const uint8_t px = static_cast<uint8_t>(x - bounds.minX);
            const uint8_t py = static_cast<uint8_t>(y - bounds.minY);
            const uint8_t nx = py;
            const uint8_t ny = static_cast<uint8_t>(bounds.width - 1 - px);
            out |= static_cast<uint16_t>(uint16_t(1) << (ny * 4 + nx));
        }
    }
    return out;
}

inline bool piecePlaced(const CargoPiece &piece) {
    return (piece.flags & PIECE_PLACED) != 0;
}

inline bool pieceBonus(const CargoPiece &piece) {
    return (piece.flags & PIECE_BONUS) != 0;
}

inline uint8_t pieceRotation(const CargoPiece &piece) {
    return static_cast<uint8_t>((piece.flags & PIECE_ROT_MASK) >> PIECE_ROT_SHIFT);
}

// Shape in the piece's current orientation (0..3 clockwise quarter turns).
inline uint16_t pieceMask(const CargoPiece &piece) {
    uint16_t mask = piece.mask;
    for (uint8_t i = 0; i < pieceRotation(piece); ++i)
        mask = rotateMaskCW(mask);
    return mask;
}

// ---------------------------------------------------------------------------
// Hull metadata
// ---------------------------------------------------------------------------

struct HullSpec {
    uint16_t offset;
    uint8_t arrangements;
    uint8_t pieces;
    uint8_t required;
    uint8_t bonus;
    uint8_t recordBytes;
    uint8_t cols;
    uint8_t rows;
    uint8_t load;
    uint64_t hold;
};

constexpr uint64_t cellBit(uint8_t x, uint8_t y, uint8_t cols) {
    return uint64_t(1) << (y * cols + x);
}

constexpr uint64_t holdMask(uint8_t cols, uint8_t rows, uint64_t blocked) {
    return ((uint64_t(1) << (cols * rows)) - uint64_t(1)) & ~blocked;
}

// Blocked cells mirror tools/gen_cargo.py HULLS and the browser roster.
constexpr uint64_t MINNOW_BLOCKED = cellBit(0, 0, 6) | cellBit(5, 0, 6) | cellBit(0, 2, 6) | cellBit(5, 2, 6);
constexpr uint64_t HAULER_BLOCKED =
    cellBit(0, 0, 11) | cellBit(5, 0, 11) | cellBit(5, 1, 11) | cellBit(5, 2, 11) | cellBit(10, 0, 11);
constexpr uint64_t LEVIATHAN_BLOCKED = cellBit(0, 0, 14) | cellBit(1, 0, 14) | cellBit(6, 0, 14) | cellBit(7, 0, 14) |
                                        cellBit(6, 1, 14) | cellBit(7, 1, 14) | cellBit(6, 2, 14) | cellBit(7, 2, 14) |
                                        cellBit(6, 3, 14) | cellBit(7, 3, 14) | cellBit(12, 0, 14) | cellBit(13, 0, 14);

constexpr HullSpec HULLS[HULL_COUNT] = {
    {CARGO_MINNOW_OFFSET, CARGO_MINNOW_ARRANGEMENTS, CARGO_MINNOW_PIECES, CARGO_MINNOW_REQUIRED,
     CARGO_MINNOW_BONUS, CARGO_MINNOW_RECORD_BYTES, CARGO_MINNOW_COLS, CARGO_MINNOW_ROWS, CARGO_MINNOW_LOAD,
     holdMask(CARGO_MINNOW_COLS, CARGO_MINNOW_ROWS, MINNOW_BLOCKED)},
    {CARGO_HAULER_OFFSET, CARGO_HAULER_ARRANGEMENTS, CARGO_HAULER_PIECES, CARGO_HAULER_REQUIRED,
     CARGO_HAULER_BONUS, CARGO_HAULER_RECORD_BYTES, CARGO_HAULER_COLS, CARGO_HAULER_ROWS, CARGO_HAULER_LOAD,
     holdMask(CARGO_HAULER_COLS, CARGO_HAULER_ROWS, HAULER_BLOCKED)},
    {CARGO_LEVIATHAN_OFFSET, CARGO_LEVIATHAN_ARRANGEMENTS, CARGO_LEVIATHAN_PIECES, CARGO_LEVIATHAN_REQUIRED,
     CARGO_LEVIATHAN_BONUS, CARGO_LEVIATHAN_RECORD_BYTES, CARGO_LEVIATHAN_COLS, CARGO_LEVIATHAN_ROWS,
     CARGO_LEVIATHAN_LOAD, holdMask(CARGO_LEVIATHAN_COLS, CARGO_LEVIATHAN_ROWS, LEVIATHAN_BLOCKED)},
};

inline bool hullSpec(uint8_t hullId, HullSpec &out) {
    if (hullId >= HULL_COUNT)
        return false;
    out = HULLS[hullId];
    return true;
}

// ---------------------------------------------------------------------------
// Placement rules
// ---------------------------------------------------------------------------

inline bool inHold(const CargoPuzzle &puzzle, uint8_t x, uint8_t y) {
    if (x >= puzzle.cols || y >= puzzle.rows)
        return false;
    return (puzzle.hold & cellBit(x, y, puzzle.cols)) != 0;
}

// Cells occupied by one shape at a top-left position. Callers bound-check the
// shape first; out-of-range cells are simply omitted here.
inline uint64_t shapeCells(const CargoPuzzle &puzzle, uint16_t mask, uint8_t x, uint8_t y) {
    uint64_t cells = 0;
    for (uint8_t sy = 0; sy < 4; ++sy) {
        for (uint8_t sx = 0; sx < 4; ++sx) {
            if ((mask & (uint16_t(1) << (sy * 4 + sx))) == 0)
                continue;
            const uint8_t gx = static_cast<uint8_t>(x + sx);
            const uint8_t gy = static_cast<uint8_t>(y + sy);
            if (gx < puzzle.cols && gy < puzzle.rows)
                cells |= cellBit(gx, gy, puzzle.cols);
        }
    }
    return cells;
}

inline uint64_t pieceCells(const CargoPuzzle &puzzle, const CargoPiece &piece, uint8_t x, uint8_t y) {
    return shapeCells(puzzle, pieceMask(piece), x, y);
}

// At least one missing cell rests on the floor, structural hull, or placed
// cargo. This matches the release rule: no mid-air placement.
inline bool supported(const CargoPuzzle &puzzle, uint16_t mask, uint8_t x, uint8_t y) {
    for (uint8_t sy = 0; sy < 4; ++sy) {
        for (uint8_t sx = 0; sx < 4; ++sx) {
            if ((mask & (uint16_t(1) << (sy * 4 + sx))) == 0)
                continue;
            const uint8_t gx = static_cast<uint8_t>(x + sx);
            const uint8_t gy = static_cast<uint8_t>(y + sy);

            if (gy + 1 == puzzle.rows)
                return true;   // floor
            const uint64_t below = cellBit(gx, static_cast<uint8_t>(gy + 1), puzzle.cols);
            if ((puzzle.hold & below) == 0)
                return true;   // blocked hull structure supports cargo
            if ((puzzle.grid & below) != 0)
                return true;   // cargo below
        }
    }
    return false;
}

inline bool canPlace(const CargoPuzzle &puzzle, uint8_t index, uint8_t x, uint8_t y) {
    if (index >= puzzle.pieceCount)
        return false;
    const CargoPiece &piece = puzzle.pieces[index];
    if (piecePlaced(piece))
        return false;

    const uint16_t mask = pieceMask(piece);
    const MaskBounds bounds = maskBounds(mask);
    if (bounds.width == 0)
        return false;
    if (static_cast<uint16_t>(x) + bounds.width > puzzle.cols)
        return false;
    if (static_cast<uint16_t>(y) + bounds.height > puzzle.rows)
        return false;

    const uint64_t cells = shapeCells(puzzle, mask, x, y);
    if ((cells & ~puzzle.hold) != 0)
        return false;   // blocked or out-of-hold cell
    if ((cells & puzzle.grid) != 0)
        return false;   // overlap

    return supported(puzzle, mask, x, y);
}

inline bool liftBlocked(const CargoPuzzle &puzzle, uint8_t index) {
    if (index >= puzzle.pieceCount)
        return false;
    const CargoPiece &piece = puzzle.pieces[index];
    if (!piecePlaced(piece))
        return false;

    const uint64_t own = pieceCells(puzzle, piece, piece.x, piece.y);
    const uint64_t other = puzzle.grid & ~own;

    for (uint8_t sy = 0; sy < 4; ++sy) {
        for (uint8_t sx = 0; sx < 4; ++sx) {
            if ((pieceMask(piece) & (uint16_t(1) << (sy * 4 + sx))) == 0)
                continue;
            const uint8_t gx = static_cast<uint8_t>(piece.x + sx);
            const uint8_t gy = static_cast<uint8_t>(piece.y + sy);
            if (gy == 0)
                continue;
            const uint64_t above = cellBit(gx, static_cast<uint8_t>(gy - 1), puzzle.cols);
            if ((other & above) != 0)
                return true;
        }
    }
    return false;
}

inline void removeFromOrder(CargoPuzzle &puzzle, uint8_t index) {
    for (uint8_t i = 0; i < puzzle.orderSize; ++i) {
        if (puzzle.order[i] != index)
            continue;
        for (uint8_t j = i; j + 1 < puzzle.orderSize; ++j)
            puzzle.order[j] = puzzle.order[j + 1];
        --puzzle.orderSize;
        return;
    }
}

inline void unplace(CargoPuzzle &puzzle, uint8_t index) {
    CargoPiece &piece = puzzle.pieces[index];
    if (!piecePlaced(piece))
        return;

    puzzle.grid &= ~pieceCells(puzzle, piece, piece.x, piece.y);
    if (pieceBonus(piece)) {
        if (puzzle.bonusPlaced > 0)
            --puzzle.bonusPlaced;
    } else if (puzzle.requiredPlaced > 0) {
        --puzzle.requiredPlaced;
    }
    puzzle.loadedCells = static_cast<uint8_t>(puzzle.loadedCells - maskArea(pieceMask(piece)));
    piece.flags &= static_cast<uint8_t>(~PIECE_PLACED);
    removeFromOrder(puzzle, index);
}

inline bool place(CargoPuzzle &puzzle, uint8_t index, uint8_t x, uint8_t y) {
    if (!canPlace(puzzle, index, x, y))
        return false;

    CargoPiece &piece = puzzle.pieces[index];
    piece.x = x;
    piece.y = y;
    puzzle.grid |= pieceCells(puzzle, piece, x, y);
    piece.flags |= PIECE_PLACED;
    if (pieceBonus(piece))
        ++puzzle.bonusPlaced;
    else
        ++puzzle.requiredPlaced;
    puzzle.loadedCells = static_cast<uint8_t>(puzzle.loadedCells + maskArea(pieceMask(piece)));
    if (puzzle.orderSize < MAX_PIECES)
        puzzle.order[puzzle.orderSize++] = index;
    if (puzzle.held == static_cast<int8_t>(index))
        puzzle.held = -1;
    return true;
}

inline bool release(CargoPuzzle &puzzle, uint8_t x, uint8_t y) {
    if (puzzle.held < 0)
        return false;
    const int8_t held = puzzle.held;
    CargoPiece &piece = puzzle.pieces[held];
    const uint8_t oldX = piece.x;
    const uint8_t oldY = piece.y;

    piece.x = x;
    piece.y = y;
    if (!place(puzzle, static_cast<uint8_t>(held), x, y)) {
        piece.x = oldX;
        piece.y = oldY;
        return false;
    }
    puzzle.held = -1;
    return true;
}

inline bool release(CargoPuzzle &puzzle) {
    if (puzzle.held < 0)
        return false;
    const CargoPiece &piece = puzzle.pieces[puzzle.held];
    return release(puzzle, piece.x, piece.y);
}

// Lift an installed piece, or pick up a tray piece. A placed piece is blocked
// when another placed asset sits directly above any of its cells.
inline bool pickup(CargoPuzzle &puzzle, uint8_t index) {
    if (puzzle.held >= 0 || index >= puzzle.pieceCount)
        return false;

    if (piecePlaced(puzzle.pieces[index])) {
        if (liftBlocked(puzzle, index))
            return false;
        unplace(puzzle, index);
    }
    puzzle.held = static_cast<int8_t>(index);
    return true;
}

inline bool lift(CargoPuzzle &puzzle, uint8_t index) {
    return pickup(puzzle, index);
}

inline bool rotateHeld(CargoPuzzle &puzzle, bool clockwise) {
    if (puzzle.held < 0)
        return false;

    CargoPiece &piece = puzzle.pieces[puzzle.held];
    uint8_t rotation = pieceRotation(piece);
    if (clockwise)
        rotation = static_cast<uint8_t>((rotation + 1) & 3);
    else
        rotation = static_cast<uint8_t>((rotation + 3) & 3);
    piece.flags = static_cast<uint8_t>((piece.flags & ~PIECE_ROT_MASK) | (rotation << PIECE_ROT_SHIFT));
    return true;
}

inline bool setHeldPosition(CargoPuzzle &puzzle, uint8_t x, uint8_t y) {
    if (puzzle.held < 0)
        return false;
    puzzle.pieces[puzzle.held].x = x;
    puzzle.pieces[puzzle.held].y = y;
    return true;
}

inline bool moveHeld(CargoPuzzle &puzzle, int8_t dx, int8_t dy) {
    if (puzzle.held < 0)
        return false;
    const CargoPiece &piece = puzzle.pieces[puzzle.held];
    const int16_t nx = static_cast<int16_t>(piece.x) + dx;
    const int16_t ny = static_cast<int16_t>(piece.y) + dy;
    if (nx < 0 || ny < 0 || nx > 255 || ny > 255)
        return false;
    return setHeldPosition(puzzle, static_cast<uint8_t>(nx), static_cast<uint8_t>(ny));
}

inline bool undo(CargoPuzzle &puzzle) {
    if (puzzle.held >= 0 || puzzle.orderSize == 0)
        return false;
    unplace(puzzle, puzzle.order[puzzle.orderSize - 1]);
    return true;
}

inline void reset(CargoPuzzle &puzzle) {
    for (uint8_t i = 0; i < puzzle.pieceCount; ++i) {
        CargoPiece &piece = puzzle.pieces[i];
        piece.flags &= PIECE_BONUS;
        piece.x = static_cast<uint8_t>(puzzle.home[i] & 0x0F);
        piece.y = static_cast<uint8_t>((puzzle.home[i] >> 4) & 0x03);
    }
    puzzle.grid = 0;
    puzzle.held = -1;
    puzzle.requiredPlaced = 0;
    puzzle.bonusPlaced = 0;
    puzzle.loadedCells = 0;
    puzzle.orderSize = 0;
}

inline bool requiredComplete(const CargoPuzzle &puzzle) {
    return puzzle.requiredPlaced >= puzzle.requiredTotal;
}

inline bool allComplete(const CargoPuzzle &puzzle) {
    return puzzle.requiredPlaced >= puzzle.requiredTotal && puzzle.bonusPlaced >= puzzle.bonusTotal;
}

inline bool loadComplete(const CargoPuzzle &puzzle) {
    return puzzle.loadedCells >= puzzle.load;
}

// ---------------------------------------------------------------------------
// Arrangement loader
// ---------------------------------------------------------------------------

template <typename Reader>
inline bool loadArrangement(CargoPuzzle &puzzle, Reader &reader, uint8_t hullId, uint8_t arrangementIndex) {
    if (hullId >= HULL_COUNT)
        return false;

    const HullSpec &spec = HULLS[hullId];
    if (arrangementIndex >= spec.arrangements || spec.recordBytes > MAX_RECORD_BYTES)
        return false;

    const uint16_t recordOffset =
        static_cast<uint16_t>(spec.offset + static_cast<uint16_t>(arrangementIndex) * spec.recordBytes);
    uint8_t raw[MAX_RECORD_BYTES];
    if (!reader.readCargo(recordOffset, raw, spec.recordBytes))
        return false;

    CargoPuzzle loaded{};
    loaded.cols = spec.cols;
    loaded.rows = spec.rows;
    loaded.hold = spec.hold;
    loaded.pieceCount = spec.pieces;
    loaded.requiredTotal = spec.required;
    loaded.bonusTotal = spec.bonus;
    loaded.load = spec.load;
    loaded.held = -1;

    uint8_t requiredSeen = 0;
    uint8_t bonusSeen = 0;
    uint64_t targetGrid = 0;

    for (uint8_t i = 0; i < spec.pieces; ++i) {
        const uint8_t *record = raw + i * CARGO_PIECE_BYTES;
        const uint16_t mask = static_cast<uint16_t>(record[0]) | (static_cast<uint16_t>(record[1]) << 8);
        const uint8_t posKind = record[2];
        if (posKind & CARGO_POS_SPARE)
            return false;

        const uint8_t x = static_cast<uint8_t>(posKind & CARGO_POS_X_MASK);
        const uint8_t y = static_cast<uint8_t>((posKind & CARGO_POS_Y_MASK) >> CARGO_POS_Y_SHIFT);
        const bool bonus = (posKind & CARGO_POS_BONUS) != 0;

        const MaskBounds bounds = maskBounds(mask);
        if (bounds.width == 0)
            return false;
        if (static_cast<uint16_t>(x) + bounds.width > spec.cols)
            return false;
        if (static_cast<uint16_t>(y) + bounds.height > spec.rows)
            return false;

        CargoPiece piece{};
        piece.mask = mask;
        piece.x = x;
        piece.y = y;
        piece.flags = bonus ? PIECE_BONUS : 0;

        const uint64_t cells = shapeCells(loaded, mask, x, y);
        if ((cells & ~spec.hold) != 0)
            return false;
        if ((cells & targetGrid) != 0)
            return false;
        targetGrid |= cells;

        loaded.pieces[i] = piece;
        loaded.home[i] = static_cast<uint8_t>(x | (y << 4));
        if (bonus)
            ++bonusSeen;
        else
            ++requiredSeen;
    }

    if (requiredSeen != spec.required || bonusSeen != spec.bonus)
        return false;

    puzzle = loaded;
    return true;
}

template <typename Reader> inline bool load(CargoPuzzle &puzzle, Reader &reader, uint8_t hullId, uint8_t arrangementIndex) {
    return loadArrangement(puzzle, reader, hullId, arrangementIndex);
}

}   // namespace cargo

#!/usr/bin/env python3
"""Generate the offline cargo arrangement bank for the FX cart.

Every arrangement is a complete, validated solution: the pieces partition a
bottom-filled target region inside a hull hold mask. Generation is a pure
offline search with a deterministic SplitMix64 stream per hull; there is no
JavaScript, browser, Node, or runtime device generator involved.

Outputs
-------
``fxdata/tables/cargo.bin``
    All arrangements, hull blocks in roster order, 64 arrangements per hull.
    No per-arrangement header. Fixed counts and offsets live in the generated
    ``src/generated/cargo.hpp``.

Piece record (3 bytes, little-endian):
    offset 0: uint16 mask   normalized 4x4 bitmask, bit set = filled cell,
                            bit index = y * 4 + x. Normalized means row 0 and
                            column 0 are occupied, so mask bit 0 is set.
    offset 2: uint8 posKind target x in bits 0-3, target y in bits 4-5,
                            bonus flag in bit 6, bit 7 reserved (zero).

``posKind`` uses four bits for x because the holds are 6, 11, and 14 cells
wide; two bits could not address them. Two bits for y are enough because every
hull has at most four rows. This matches the piece record in docs/design.md.

Arrangement records store required pieces first, then bonus pieces. The split
is fixed by the hull table in the generated header.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, replace
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

ARRANGEMENTS_PER_HULL = 64
FORMAT_VERSION = 1
MASK_BYTES = 2
POS_BYTES = 1
PIECE_BYTES = MASK_BYTES + POS_BYTES

POS_X_MASK = 0x0F
POS_Y_SHIFT = 4
POS_Y_MASK = 0x30
POS_BONUS = 0x40
POS_SPARE = 0x80

MAX_MASK = 0xFFFF
MAX_X = 15
MAX_Y = 3

MASK64 = (1 << 64) - 1
GOLDEN64 = 0x9E3779B97F4A7C15


class CargoError(ValueError):
    """Raised when a hull spec, piece, or arrangement breaks the contract."""


# ---------------------------------------------------------------------------
# Hull table. Blocked cells are hull structure: solid for support purposes and
# never part of a target hold or a piece.
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Hull:
    name: str
    cols: int
    rows: int
    blocked: frozenset[tuple[int, int]]
    load: int
    required: int
    bonus: int
    max_area: int
    max_w: int
    max_h: int
    seed: int

    @property
    def pieces(self) -> int:
        return self.required + self.bonus

    @property
    def record_bytes(self) -> int:
        return self.pieces * PIECE_BYTES


def _hull(name, cols, rows, blocked, load, required, bonus, max_area, max_w, max_h, seed):
    return Hull(
        name=name,
        cols=cols,
        rows=rows,
        blocked=frozenset(blocked),
        load=load,
        required=required,
        bonus=bonus,
        max_area=max_area,
        max_w=max_w,
        max_h=max_h,
        seed=seed,
    )


# Hold masks and load contracts mirror spike-trim/subs.js exactly.
HULLS: tuple[Hull, ...] = (
    _hull(
        "MINNOW", 6, 3,
        ((0, 0), (5, 0), (0, 2), (5, 2)),
        12, 3, 1, 4, 3, 2,
        0x4D494E4E4F57,
    ),
    _hull(
        "HAULER", 11, 3,
        ((0, 0), (5, 0), (5, 1), (5, 2), (10, 0)),
        26, 6, 1, 6, 4, 3,
        0x4841554C4552,
    ),
    _hull(
        "LEVIATHAN", 14, 4,
        (
            (0, 0), (1, 0),
            (6, 0), (6, 1), (6, 2), (6, 3),
            (7, 0), (7, 1), (7, 2), (7, 3),
            (12, 0), (13, 0),
        ),
        40, 6, 2, 8, 4, 4,
        0x4C455649415448,
    ),
)

HULL_BY_NAME = {hull.name: hull for hull in HULLS}


# ---------------------------------------------------------------------------
# Deterministic RNG. SplitMix64 is a fixed integer stream, independent of the
# host Python version (unlike hash randomization or module internals).
# ---------------------------------------------------------------------------


class Rng:
    def __init__(self, seed: int) -> None:
        self.state = seed & MASK64

    def next_u64(self) -> int:
        self.state = (self.state + GOLDEN64) & MASK64
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
        return (z ^ (z >> 31)) & MASK64

    def below(self, limit: int) -> int:
        if limit <= 0:
            raise CargoError("rng bound must be positive")
        return self.next_u64() % limit

    def shuffle(self, items: list) -> None:
        for i in range(len(items) - 1, 0, -1):
            j = self.below(i + 1)
            items[i], items[j] = items[j], items[i]


# ---------------------------------------------------------------------------
# Geometry helpers.
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Piece:
    mask: int
    x: int
    y: int
    bonus: bool

    def cells(self) -> tuple[tuple[int, int], ...]:
        return mask_cells(self.mask)

    def target_cells(self) -> tuple[tuple[int, int], ...]:
        return tuple((self.x + cx, self.y + cy) for cx, cy in self.cells())


def mask_cells(mask: int) -> tuple[tuple[int, int], ...]:
    return tuple((bit % 4, bit // 4) for bit in range(16) if mask & (1 << bit))


def mask_area(mask: int) -> int:
    return bin(mask).count("1")


def mask_bbox(mask: int) -> tuple[int, int]:
    cells = mask_cells(mask)
    if not cells:
        raise CargoError("empty shape mask")
    min_x = min(cx for cx, _ in cells)
    max_x = max(cx for cx, _ in cells)
    min_y = min(cy for _, cy in cells)
    max_y = max(cy for _, cy in cells)
    return max_x - min_x + 1, max_y - min_y + 1


def mask_connected(mask: int) -> bool:
    cells = set(mask_cells(mask))
    if not cells:
        return False
    seen = {next(iter(cells))}
    stack = list(seen)
    while stack:
        x, y = stack.pop()
        for nb in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if nb in cells and nb not in seen:
                seen.add(nb)
                stack.append(nb)
    return len(seen) == len(cells)


def normalize(cells: set[tuple[int, int]]) -> tuple[int, int, int]:
    """Return (mask, min_x, min_y) for a connected cell set."""
    if not cells:
        raise CargoError("cannot normalize an empty cell set")
    min_x = min(x for x, _ in cells)
    min_y = min(y for _, y in cells)
    mask = 0
    for x, y in cells:
        mask |= 1 << ((y - min_y) * 4 + (x - min_x))
    return mask, min_x, min_y


def open_column_ys(hull: Hull, x: int) -> tuple[int, ...]:
    """Open rows of one column, bottom first (blocked rows are skipped)."""
    checked(hull)
    return tuple(
        y for y in range(hull.rows - 1, -1, -1) if (x, y) not in hull.blocked
    )


def checked(hull: Hull) -> None:
    """Validate the static hull contract. Cheap; called from public entry points."""
    if hull.cols <= 0 or hull.rows <= 0:
        raise CargoError(f"{hull.name}: empty hold")
    if hull.load <= 0:
        raise CargoError(f"{hull.name}: empty load")
    if hull.required <= 0 or hull.bonus < 0:
        raise CargoError(f"{hull.name}: bad piece counts")
    if hull.pieces > 16:
        raise CargoError(f"{hull.name}: too many pieces for the record format")
    if hull.max_area <= 0 or hull.max_w <= 0 or hull.max_h <= 0:
        raise CargoError(f"{hull.name}: bad shape limits")
    if hull.max_w > 4 or hull.max_h > 4:
        raise CargoError(f"{hull.name}: shape limits exceed the 4x4 mask")
    for x, y in hull.blocked:
        if not (0 <= x < hull.cols and 0 <= y < hull.rows):
            raise CargoError(f"{hull.name}: blocked cell {(x, y)} out of bounds")
    capacity = sum(
        1
        for x in range(hull.cols)
        for y in range(hull.rows)
        if (x, y) not in hull.blocked
    )
    if hull.load > capacity:
        raise CargoError(f"{hull.name}: load {hull.load} exceeds capacity {capacity}")
    if capacity - hull.load > hull.cols:
        # Target holds every open column at least once; refuse hulls that cannot.
        raise CargoError(f"{hull.name}: too much slack for a bottom-filled target")


# ---------------------------------------------------------------------------
# Target hold and piece partition search.
# ---------------------------------------------------------------------------


def make_target(hull: Hull, rng: Rng) -> set[tuple[int, int]]:
    """Pick a bottom-filled target of exactly ``hull.load`` cells.

    Every open column gets at least one cell; the rest of the load is spread
    over columns that still have room. Cells in a column are always taken from
    the bottom up, so the region is stable by construction.
    """
    columns = {
        x: open_column_ys(hull, x) for x in range(hull.cols)
    }
    columns = {x: ys for x, ys in columns.items() if ys}
    if len(columns) > hull.load:
        raise CargoError(f"{hull.name}: load cannot seed every open column")

    counts = {x: 1 for x in columns}
    remaining = hull.load - len(columns)
    while remaining:
        choices = [x for x, ys in columns.items() if counts[x] < len(ys)]
        if not choices:
            raise CargoError(f"{hull.name}: load exceeds hold capacity")
        counts[choices[rng.below(len(choices))]] += 1
        remaining -= 1

    target = {
        (x, y)
        for x, ys in columns.items()
        for y in ys[: counts[x]]
    }
    if len(target) != hull.load:
        raise CargoError(f"{hull.name}: target cell count mismatch")
    return target


def _bbox(cells: set[tuple[int, int]]) -> tuple[int, int, int, int]:
    xs = [x for x, _ in cells]
    ys = [y for _, y in cells]
    return min(xs), min(ys), max(xs), max(ys)


def partition(hull: Hull, target: set[tuple[int, int]], rng: Rng) -> list[set[tuple[int, int]]] | None:
    """Merge adjacent target cells until exactly ``hull.pieces`` clusters remain.

    Each merge must keep the cluster connected (adjacent merge), within the
    area cap, and within the bounding-box caps. Returns ``None`` when the
    attempt paints itself into a corner; the caller retries with a new target.
    """
    clusters: dict[int, set[tuple[int, int]]] = {}
    owner: dict[tuple[int, int], int] = {}
    for index, cell in enumerate(sorted(target)):
        clusters[index] = {cell}
        owner[cell] = index
    next_id = len(clusters)

    while len(clusters) > hull.pieces:
        candidates: dict[tuple[int, int], tuple[int, int]] = {}
        for x, y in sorted(target):
            a = owner[(x, y)]
            for nb in ((x + 1, y), (x, y + 1)):
                if nb not in target:
                    continue
                b = owner[nb]
                if a == b:
                    continue
                key = (a, b) if a < b else (b, a)
                if key in candidates:
                    continue
                merged = clusters[a] | clusters[b]
                if len(merged) > hull.max_area:
                    continue
                min_x, min_y, max_x, max_y = _bbox(merged)
                if max_x - min_x + 1 > hull.max_w:
                    continue
                if max_y - min_y + 1 > hull.max_h:
                    continue
                candidates[key] = (len(merged), key[0], key[1])

        if not candidates:
            return None

        ordered = sorted(candidates.values())
        pool_size = min(8, len(ordered))
        _, a, b = ordered[rng.below(pool_size)]
        clusters[a].update(clusters[b])
        for cell in clusters.pop(b):
            owner[cell] = a

    return [clusters[key] for key in sorted(clusters)]


def build_arrangement(
    hull: Hull, clusters: list[set[tuple[int, int]]], rng: Rng
) -> list[Piece]:
    pieces: list[Piece] = []
    for cells in clusters:
        mask, x, y = normalize(cells)
        pieces.append(Piece(mask, x, y, False))

    rng.shuffle(pieces)
    pieces = [
        replace(piece, bonus=index >= hull.required)
        for index, piece in enumerate(pieces)
    ]
    # Canonical output order: required pieces first, then bonus, each sorted by
    # position and mask. This keeps the byte stream stable and readable.
    required = sorted((p for p in pieces if not p.bonus), key=lambda p: (p.y, p.x, p.mask))
    bonus = sorted((p for p in pieces if p.bonus), key=lambda p: (p.y, p.x, p.mask))
    return required + bonus


# ---------------------------------------------------------------------------
# Validation. Every rule from docs/design.md is enforced here.
# ---------------------------------------------------------------------------


def _occupied(pieces: list[Piece]) -> dict[tuple[int, int], int]:
    occupied: dict[tuple[int, int], int] = {}
    for index, piece in enumerate(pieces):
        for cell in piece.target_cells():
            if cell in occupied:
                raise CargoError(f"overlap at {cell} between pieces {occupied[cell]} and {index}")
            occupied[cell] = index
    return occupied


def validate_arrangement(hull: Hull, pieces: list[Piece]) -> None:
    """Raise CargoError unless ``pieces`` is a complete, stable solution."""
    checked(hull)

    if len(pieces) != hull.pieces:
        raise CargoError(f"{hull.name}: expected {hull.pieces} pieces, got {len(pieces)}")

    required = sum(1 for piece in pieces if not piece.bonus)
    bonus = sum(1 for piece in pieces if piece.bonus)
    if required != hull.required or bonus != hull.bonus:
        raise CargoError(
            f"{hull.name}: expected {hull.required} required/{hull.bonus} bonus, "
            f"got {required}/{bonus}"
        )
    # Required records must be first, then bonus, so a decoder can rely on it.
    if any(piece.bonus for piece in pieces[: hull.required]):
        raise CargoError(f"{hull.name}: required piece appears after a bonus piece")
    if any(not piece.bonus for piece in pieces[hull.required :]):
        raise CargoError(f"{hull.name}: bonus piece appears before a required piece")

    for index, piece in enumerate(pieces):
        _validate_piece(hull, index, piece)

    occupied = _occupied(pieces)

    max_x = hull.cols - 1
    max_y = hull.rows - 1
    for x, y in occupied:
        if not (0 <= x <= max_x and 0 <= y <= max_y):
            raise CargoError(f"{hull.name}: piece cell {(x, y)} out of bounds")
        if (x, y) in hull.blocked:
            raise CargoError(f"{hull.name}: piece cell {(x, y)} uses a blocked cell")

    if len(occupied) != hull.load:
        raise CargoError(f"{hull.name}: expected load {hull.load}, occupied {len(occupied)}")

    # Support: every piece cell rests on the floor, hull structure, or another
    # placed cargo cell (which may be part of the same piece).
    for x, y in occupied:
        if y + 1 == hull.rows:
            continue
        below = (x, y + 1)
        if below in hull.blocked or below in occupied:
            continue
        raise CargoError(f"{hull.name}: unsupported cargo cell {(x, y)}")

    # Bottom-filled: each open column holds a contiguous run from its lowest
    # open row upwards, and every open column is used.
    for x in range(hull.cols):
        floor = open_column_ys(hull, x)
        present = sorted(y for (cx, y) in occupied if cx == x)
        if not floor:
            if present:
                raise CargoError(f"{hull.name}: cargo in closed column {x}")
            continue
        if not present:
            raise CargoError(f"{hull.name}: open column {x} is empty")
        if present != sorted(floor[: len(present)]):
            raise CargoError(f"{hull.name}: column {x} is not bottom-filled")

    # Distinct pieces must not merely re-label one another.
    seen: set[tuple[int, int, int]] = set()
    for piece in pieces:
        key = (piece.mask, piece.x, piece.y)
        if key in seen:
            raise CargoError(f"{hull.name}: duplicate piece placement {key}")
        seen.add(key)


def _validate_piece(hull: Hull, index: int, piece: Piece) -> None:
    if not (0 <= piece.mask <= MAX_MASK):
        raise CargoError(f"{hull.name}: piece {index} mask outside uint16")
    if piece.mask == 0:
        raise CargoError(f"{hull.name}: piece {index} has an empty mask")
    if not (0 <= piece.x <= MAX_X):
        raise CargoError(f"{hull.name}: piece {index} x {piece.x} does not fit posKind")
    if not (0 <= piece.y <= MAX_Y):
        raise CargoError(f"{hull.name}: piece {index} y {piece.y} does not fit posKind")

    cells = mask_cells(piece.mask)
    if (0, 0) not in cells:
        raise CargoError(f"{hull.name}: piece {index} mask is not normalized to (0,0)")
    if len(cells) != len(set(cells)):
        raise CargoError(f"{hull.name}: piece {index} mask has duplicate cells")
    if not mask_connected(piece.mask):
        raise CargoError(f"{hull.name}: piece {index} mask is disconnected")

    area = len(cells)
    if area > hull.max_area:
        raise CargoError(f"{hull.name}: piece {index} area {area} exceeds {hull.max_area}")
    width, height = mask_bbox(piece.mask)
    if width > hull.max_w or height > hull.max_h:
        raise CargoError(
            f"{hull.name}: piece {index} bbox {width}x{height} exceeds "
            f"{hull.max_w}x{hull.max_h}"
        )
    if piece.x + width > hull.cols or piece.y + height > hull.rows:
        raise CargoError(f"{hull.name}: piece {index} bbox out of bounds")


# ---------------------------------------------------------------------------
# Encoding.
# ---------------------------------------------------------------------------


def encode_pos(piece: Piece) -> int:
    if not (0 <= piece.x <= MAX_X and 0 <= piece.y <= MAX_Y):
        raise CargoError(f"position {(piece.x, piece.y)} does not fit posKind")
    return piece.x | (piece.y << POS_Y_SHIFT) | (POS_BONUS if piece.bonus else 0)


def encode_arrangement(hull: Hull, pieces: list[Piece]) -> bytes:
    validate_arrangement(hull, pieces)
    out = bytearray()
    for piece in pieces:
        out += piece.mask.to_bytes(MASK_BYTES, "little")
        out.append(encode_pos(piece))
    if len(out) != hull.record_bytes:
        raise CargoError(f"{hull.name}: encoded record size mismatch")
    return bytes(out)


def decode_pos(value: int) -> tuple[int, int, bool]:
    if value & POS_SPARE:
        raise CargoError("piece posKind uses the reserved bit")
    return value & POS_X_MASK, (value & POS_Y_MASK) >> POS_Y_SHIFT, bool(value & POS_BONUS)


def decode_arrangement(hull: Hull, data: bytes) -> list[Piece]:
    if len(data) != hull.record_bytes:
        raise CargoError(
            f"{hull.name}: expected {hull.record_bytes} bytes, got {len(data)}"
        )
    pieces = []
    for offset in range(0, len(data), PIECE_BYTES):
        mask = int.from_bytes(data[offset : offset + MASK_BYTES], "little")
        x, y, bonus = decode_pos(data[offset + MASK_BYTES])
        pieces.append(Piece(mask, x, y, bonus))
    return pieces


# ---------------------------------------------------------------------------
# Bank generation.
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class HullStats:
    name: str
    offset: int
    record_bytes: int
    arrangements: int
    pieces: int
    attempts: int


@dataclass(frozen=True)
class Bank:
    blob: bytes
    arrangements: dict[str, list[list[Piece]]]
    stats: tuple[HullStats, ...]


def hull_offset(hull: Hull) -> int:
    offset = 0
    for candidate in HULLS:
        if candidate.name == hull.name:
            return offset
        offset += ARRANGEMENTS_PER_HULL * candidate.record_bytes
    raise CargoError(f"unknown hull {hull.name}")


def _signature(pieces: list[Piece]) -> tuple:
    required = tuple(sorted((p.mask, p.x, p.y) for p in pieces if not p.bonus))
    bonus = tuple(sorted((p.mask, p.x, p.y) for p in pieces if p.bonus))
    return required, bonus


def generate_hull(
    hull: Hull, count: int = ARRANGEMENTS_PER_HULL, max_attempts: int = 20000
) -> tuple[list[list[Piece]], int]:
    checked(hull)
    rng = Rng(hull.seed)
    seen: set[tuple] = set()
    arrangements: list[list[Piece]] = []
    attempts = 0
    while len(arrangements) < count and attempts < max_attempts:
        attempts += 1
        target = make_target(hull, rng)
        clusters = partition(hull, target, rng)
        if clusters is None:
            continue
        pieces = build_arrangement(hull, clusters, rng)
        try:
            validate_arrangement(hull, pieces)
        except CargoError:
            continue
        signature = _signature(pieces)
        if signature in seen:
            continue
        seen.add(signature)
        arrangements.append(pieces)

    if len(arrangements) != count:
        raise CargoError(
            f"{hull.name}: only {len(arrangements)}/{count} distinct arrangements "
            f"after {attempts} attempts"
        )
    return arrangements, attempts


def generate_bank() -> Bank:
    blob = bytearray()
    arrangements: dict[str, list[list[Piece]]] = {}
    stats: list[HullStats] = []
    for hull in HULLS:
        offset = len(blob)
        hull_arrangements, attempts = generate_hull(hull)
        for pieces in hull_arrangements:
            blob += encode_arrangement(hull, pieces)
        arrangements[hull.name] = hull_arrangements
        stats.append(
            HullStats(
                name=hull.name,
                offset=offset,
                record_bytes=hull.record_bytes,
                arrangements=len(hull_arrangements),
                pieces=hull.pieces,
                attempts=attempts,
            )
        )
    expected = sum(
        ARRANGEMENTS_PER_HULL * hull.record_bytes for hull in HULLS
    )
    if len(blob) != expected:
        raise CargoError(f"table size {len(blob)} does not match {expected}")
    return Bank(bytes(blob), arrangements, tuple(stats))


# ---------------------------------------------------------------------------
# Generated header.
# ---------------------------------------------------------------------------


HEADER_PREAMBLE = """\
// Generated by tools/gen_cargo.py. Do not edit.
#pragma once

#include <stdint.h>

// Cargo arrangement bank, format CARGO_FORMAT_VERSION.
//
// The bank lives in external FX flash. Hull blocks follow roster order and
// each block holds CARGO_ARRANGEMENTS_PER_HULL arrangements, stored back to
// back with no per-arrangement header.
//
// Piece record (CARGO_PIECE_BYTES = 3, little-endian):
//   mask     uint16 at offset 0: normalized 4x4 shape; bit index = y * 4 + x;
//            bit 0 (top-left) is always set.
//   posKind  uint8  at offset 2: target x in bits 0-3, target y in bits 4-5,
//            bonus flag in bit 6, reserved bit 7 must be zero.
//
// Each arrangement stores required pieces first, then bonus pieces.

constexpr uint8_t  CARGO_FORMAT_VERSION = 1;
constexpr uint8_t  CARGO_HULL_COUNT = 3;
constexpr uint8_t  CARGO_ARRANGEMENTS_PER_HULL = 64;
constexpr uint8_t  CARGO_MASK_BYTES = 2;
constexpr uint8_t  CARGO_POS_BYTES = 1;
constexpr uint8_t  CARGO_PIECE_BYTES = 3;
constexpr uint16_t CARGO_TABLE_BYTES = 3648;

constexpr uint8_t CARGO_POS_X_MASK = 0x0F;
constexpr uint8_t CARGO_POS_Y_SHIFT = 4;
constexpr uint8_t CARGO_POS_Y_MASK = 0x30;
constexpr uint8_t CARGO_POS_BONUS = 0x40;
constexpr uint8_t CARGO_POS_SPARE = 0x80;
"""


def build_header(blob_size: int = 0) -> str:
    offset = 0
    lines = [HEADER_PREAMBLE]
    for hull in HULLS:
        u = f"CARGO_{hull.name}"
        lines.append(f"\n// {hull.name}: {hull.cols}x{hull.rows} hold, load {hull.load}.\n")
        lines.append(f"constexpr uint16_t {u}_OFFSET = {offset};\n")
        lines.append(f"constexpr uint8_t  {u}_ARRANGEMENTS = {ARRANGEMENTS_PER_HULL};\n")
        lines.append(f"constexpr uint8_t  {u}_PIECES = {hull.pieces};\n")
        lines.append(f"constexpr uint8_t  {u}_REQUIRED = {hull.required};\n")
        lines.append(f"constexpr uint8_t  {u}_BONUS = {hull.bonus};\n")
        lines.append(f"constexpr uint8_t  {u}_RECORD_BYTES = {hull.record_bytes};\n")
        lines.append(f"constexpr uint8_t  {u}_COLS = {hull.cols};\n")
        lines.append(f"constexpr uint8_t  {u}_ROWS = {hull.rows};\n")
        lines.append(f"constexpr uint8_t  {u}_LOAD = {hull.load};\n")
        lines.append(f"constexpr uint8_t  {u}_MAX_AREA = {hull.max_area};\n")
        lines.append(f"constexpr uint8_t  {u}_MAX_W = {hull.max_w};\n")
        lines.append(f"constexpr uint8_t  {u}_MAX_H = {hull.max_h};\n")
        offset += ARRANGEMENTS_PER_HULL * hull.record_bytes
    expected = sum(ARRANGEMENTS_PER_HULL * h.record_bytes for h in HULLS)
    if blob_size not in (0, expected):
        raise CargoError(f"table size {blob_size} does not match {expected}")
    return "".join(lines)


# ---------------------------------------------------------------------------
# CLI.
# ---------------------------------------------------------------------------


def write_outputs(bin_path: Path, header_path: Path) -> Bank:
    bank = generate_bank()
    header = build_header(len(bank.blob))
    bin_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.parent.mkdir(parents=True, exist_ok=True)
    bin_path.write_bytes(bank.blob)
    header_path.write_text(header)
    return bank


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--bin",
        type=Path,
        default=ROOT / "fxdata" / "tables" / "cargo.bin",
        help="packed arrangement bank (default: fxdata/tables/cargo.bin)",
    )
    parser.add_argument(
        "--header",
        type=Path,
        default=ROOT / "src" / "generated" / "cargo.hpp",
        help="generated format header (default: src/generated/cargo.hpp)",
    )
    args = parser.parse_args(argv)

    bank = write_outputs(args.bin, args.header)
    total_arrangements = 0
    total_pieces = 0
    for stats in bank.stats:
        total_arrangements += stats.arrangements
        total_pieces += stats.arrangements * stats.pieces
        print(
            f"{stats.name}: {stats.arrangements} arrangements, "
            f"{stats.pieces} pieces each, {stats.record_bytes} B/arrangement, "
            f"offset {stats.offset}, {stats.attempts} attempts"
        )
    print(
        f"cargo: {len(bank.blob)} B, {total_arrangements} arrangements, "
        f"{total_pieces} validated piece records"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

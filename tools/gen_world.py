#!/usr/bin/env python3
"""Bake the fixed browser cave into raw 1-bit FX tables and metadata.

The layout is a deterministic Python port of the retired browser
``generateCave`` with the shipped seed (4242). Generation has no Node, browser,
or third-party dependency: only the Python standard library.

Binary layout
-------------
``fxdata/tables/world.bin`` -- 6,400 B
    320 x 160 tiles, 1 bit per tile, row-major.
    40 bytes per row (320 / 8); bit ``x & 7`` of byte ``y * 40 + x // 8`` is
    tile ``(x, y)``. Bit 1 = solid rock, bit 0 = open water. LSB is the
    leftmost tile in the byte. There is no file header; the layout version and
    dimensions live in ``src/generated/world.hpp``.

``fxdata/tables/world-chart.bin`` -- 256 B
    64 x 32 chart cells, 1 bit per cell, row-major.
    8 bytes per row (64 / 8); bit ``x & 7`` of byte ``y * 8 + x // 8`` is chart
    cell ``(x, y)``. Bit 1 = water, bit 0 = land. A chart cell covers exactly
    5 x 5 world tiles and is water when at least one tile in that block is
    open. This matches the browser chart's "any open tile" downsample.

``src/generated/world.hpp`` -- generated symbolic constants, metadata, and the
    baked ordered-pair contract economy (reward/deadline arrays plus bounds).
    The header is pure C++ (only ``<stdint.h>``). It deliberately does not
    include Arduino headers so host tests can include it. Any runtime-used
    metadata array must move to FX/PROGMEM in M1.2/M1.3; see the warning in the
    generated header.
"""

from __future__ import annotations

import argparse
import math
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

WORLD_TILES_X = 320
WORLD_TILES_Y = 160
TILE_PIXELS = 8
WORLD_PIXELS_X = WORLD_TILES_X * TILE_PIXELS
WORLD_PIXELS_Y = WORLD_TILES_Y * TILE_PIXELS
TILES_PER_BYTE = 8
WORLD_ROW_BYTES = WORLD_TILES_X // TILES_PER_BYTE
WORLD_BYTES = WORLD_ROW_BYTES * WORLD_TILES_Y

CHART_TILES_X = 64
CHART_TILES_Y = 32
CHART_CELL_TILES = 5
CHART_ROW_BYTES = CHART_TILES_X // TILES_PER_BYTE
CHART_BYTES = CHART_ROW_BYTES * CHART_TILES_Y

LAYOUT_VERSION = 1
DEFAULT_SEED = 4242
BORDER_TILES = 2
SETTLEMENT_CLEAR_TILES = 6
DOCK_CLEARANCE_PX = 42

# Current zones are metadata-only (no FX table). Each zone is a small
# rectangle inside one non-settlement chamber; the 16 px/s^2 Q8.8 acceleration
# drives the boat along one axis at a time. They are placed on chamber centres
# so generation stays deterministic and every zone is clear of rock, docks, and
# spawn.
CURRENT_WIDTH_PX = 32
CURRENT_HEIGHT_PX = 16
CURRENT_ACCEL_Q8 = 16 * 256

# Compact AVR current direction codes. The generated set uses one signed axis
# at a time; host records keep the full ax_q8/ay_q8 pair.
CURRENT_DIR_XPOS = 0
CURRENT_DIR_YPOS = 1
CURRENT_DIR_XNEG = 2
CURRENT_DIR_YNEG = 3


def current_direction(current: Current) -> int:
    # Direction is one compact signed-axis code. The level generator may use a
    # softer authored magnitude while the legacy world keeps CURRENT_ACCEL_Q8.
    if current.ax_q8 > 0 and current.ay_q8 == 0:
        return CURRENT_DIR_XPOS
    if current.ax_q8 == 0 and current.ay_q8 > 0:
        return CURRENT_DIR_YPOS
    if current.ax_q8 < 0 and current.ay_q8 == 0:
        return CURRENT_DIR_XNEG
    if current.ax_q8 == 0 and current.ay_q8 < 0:
        return CURRENT_DIR_YNEG
    raise ValueError("current acceleration is not a compact signed-axis code")

# Browser constants that affect the baked geometry.
HULL_WIDTH_PX = 26
HULL_HEIGHT_PX = 8
EMPTY_LOAD_HULL_HEIGHT_PX = HULL_HEIGHT_PX


@dataclass(frozen=True)
class Chamber:
    x: float
    y: float
    r: float
    name: str = ""
    settlement: bool = False


@dataclass(frozen=True)
class Settlement:
    id: int
    name: str
    tile_x: int
    tile_y: int
    pixel_x: int
    pixel_y: int


@dataclass(frozen=True)
class Current:
    x: int
    y: int
    w: int
    h: int
    ax_q8: int
    ay_q8: int


@dataclass
class World:
    seed: int
    grid: bytearray
    chart: bytes
    chambers: tuple[Chamber, ...]
    settlements: tuple[Settlement, ...]
    currents: tuple[Current, ...]
    spawn_tile_x: int
    spawn_tile_y: int
    spawn_pixel_x: int
    spawn_pixel_y: int
    spawn_sub_x: int
    spawn_sub_y: int


def _imul(a: int, b: int) -> int:
    """JavaScript Math.imul: low 32 bits of a * b, as an unsigned value."""
    return ((a & 0xFFFFFFFF) * (b & 0xFFFFFFFF)) & 0xFFFFFFFF


def mulberry32(seed: int):
    """Port of the browser's 32-bit PRNG; returns floats in [0, 1)."""
    state = seed & 0xFFFFFFFF
    mask = 0xFFFFFFFF

    def rand() -> float:
        nonlocal state
        state = (state + 0x6D2B79F5) & mask
        t = _imul((state ^ (state >> 15)) & mask, (1 | state) & mask)
        t = ((t + _imul((t ^ (t >> 7)) & mask, (61 | t) & mask)) ^ t) & mask
        return ((t ^ (t >> 14)) & mask) / 4294967296.0

    return rand


def _clamp(value, lo, hi):
    return lo if value < lo else hi if value > hi else value


def _carve_disk(grid: bytearray, cx: float, cy: float, radius: float) -> None:
    first_x = max(0, math.floor(cx - radius))
    last_x = min(WORLD_TILES_X - 1, math.ceil(cx + radius))
    first_y = max(0, math.floor(cy - radius))
    last_y = min(WORLD_TILES_Y - 1, math.ceil(cy + radius))
    radius_sq = radius * radius
    for y in range(first_y, last_y + 1):
        row = y * WORLD_TILES_X
        for x in range(first_x, last_x + 1):
            dx = x - cx
            dy = y - cy
            if dx * dx + dy * dy <= radius_sq:
                grid[row + x] = 0


def _carve_segment(
    grid: bytearray, ax: float, ay: float, bx: float, by: float, radius: float
) -> None:
    dx = bx - ax
    dy = by - ay
    distance = math.hypot(dx, dy)
    steps = max(1, math.ceil(distance / 0.4))
    for i in range(steps + 1):
        t = i / steps
        _carve_disk(grid, ax + dx * t, ay + dy * t, radius)


def tile_solid(grid: bytearray, tx: int, ty: int) -> bool:
    if tx < 0 or ty < 0 or tx >= WORLD_TILES_X or ty >= WORLD_TILES_Y:
        return True
    return grid[ty * WORLD_TILES_X + tx] == 1


def solid_at(grid: bytearray, x: float, y: float) -> bool:
    return tile_solid(grid, math.floor(x / TILE_PIXELS), math.floor(y / TILE_PIXELS))


def generate_cave(seed: int = DEFAULT_SEED) -> World:
    """Return the fixed cave for ``seed`` (4242 is the shipped layout)."""
    rand = mulberry32((seed & 0xFFFFFFFF) or 1)
    grid = bytearray([1]) * (WORLD_TILES_X * WORLD_TILES_Y)
    chambers: list[Chamber] = []

    def chamber(x, y, r, name="", is_settlement=False):
        c = Chamber(
            x=_clamp(x, r + 4, WORLD_TILES_X - r - 4),
            y=_clamp(y, r + 4, WORLD_TILES_Y - r - 4),
            r=r,
            name=name,
            settlement=is_settlement,
        )
        chambers.append(c)
        return c

    # Four settlements on a rough ring, joined by junction chambers.
    tidewall = chamber(38, 48, 10, "TIDEWALL", True)
    benthos = chamber(150, 30, 10, "BENTHOS", True)
    cormorant = chamber(282, 62, 10, "CORMORANT", True)
    deep_anchor = chamber(172, 132, 11, "DEEP ANCHOR", True)
    j0 = chamber(91, 72, 6.5)
    j1 = chamber(220, 32, 6.5)
    j2 = chamber(252, 105, 7)
    j3 = chamber(100, 138, 7)
    s0 = chamber(72, 22, 5.5)
    s1 = chamber(168, 72, 6.5)
    s2 = chamber(292, 140, 5.5)
    s3 = chamber(44, 108, 6)

    for c in chambers:
        _carve_disk(grid, c.x, c.y, c.r)

    def link(a: Chamber, b: Chamber, radius: float, wiggle: float) -> None:
        mx = _clamp(
            (a.x + b.x) / 2 + (rand() * 2 - 1) * wiggle,
            8,
            WORLD_TILES_X - 8,
        )
        my = _clamp(
            (a.y + b.y) / 2 + (rand() * 2 - 1) * wiggle,
            8,
            WORLD_TILES_Y - 8,
        )
        _carve_segment(grid, a.x, a.y, mx, my, radius)
        _carve_segment(grid, mx, my, b.x, b.y, radius)

    ring = [tidewall, j0, benthos, j1, cormorant, j2, deep_anchor, j3]
    for i in range(len(ring)):
        link(ring[i], ring[(i + 1) % len(ring)], 3.6, 7)

    # Central shortcut plus three dead-end side chambers.
    link(benthos, s1, 3.4, 4)
    link(s1, j2, 3.4, 4)
    link(j0, s0, 3.4, 3)
    link(j2, s2, 3.4, 3)
    link(j3, s3, 3.4, 3)

    # Keep the world sealed after carving.
    for y in range(WORLD_TILES_Y):
        row = y * WORLD_TILES_X
        for x in range(WORLD_TILES_X):
            if (
                x < BORDER_TILES
                or y < BORDER_TILES
                or x >= WORLD_TILES_X - BORDER_TILES
                or y >= WORLD_TILES_Y - BORDER_TILES
            ):
                grid[row + x] = 1

    settlements = tuple(
        Settlement(
            id=index,
            name=entry.name,
            tile_x=int(entry.x),
            tile_y=int(entry.y),
            pixel_x=int(entry.x) * TILE_PIXELS,
            pixel_y=int(entry.y) * TILE_PIXELS,
        )
        for index, entry in enumerate(
            (tidewall, benthos, cormorant, deep_anchor)
        )
    )

    # Current zones: one per non-settlement chamber, centred on the chamber and
    # alternating horizontal/vertical with deterministic signs. No PRNG is
    # consumed, so adding currents cannot move the baked cave.
    current_chambers = [c for c in chambers if not c.settlement]
    currents: list[Current] = []
    for index, c in enumerate(current_chambers):
        center_x = int(round(c.x * TILE_PIXELS))
        center_y = int(round(c.y * TILE_PIXELS))
        x = center_x - CURRENT_WIDTH_PX // 2
        y = center_y - CURRENT_HEIGHT_PX // 2
        ax_q8 = 0
        ay_q8 = 0
        if index % 2 == 0:
            ax_q8 = CURRENT_ACCEL_Q8 if (index // 2) % 2 == 0 else -CURRENT_ACCEL_Q8
        else:
            ay_q8 = CURRENT_ACCEL_Q8 if ((index - 1) // 2) % 2 == 0 else -CURRENT_ACCEL_Q8
        currents.append(
            Current(
                x=x,
                y=y,
                w=CURRENT_WIDTH_PX,
                h=CURRENT_HEIGHT_PX,
                ax_q8=ax_q8,
                ay_q8=ay_q8,
            )
        )

    spawn = settlements[0]
    spawn_sub_x = spawn.pixel_x - HULL_WIDTH_PX // 2
    spawn_sub_y = spawn.pixel_y + EMPTY_LOAD_HULL_HEIGHT_PX // 2

    chart = pack_chart(build_chart(grid))
    world = World(
        seed=seed,
        grid=grid,
        chart=chart,
        chambers=tuple(chambers),
        settlements=settlements,
        currents=tuple(currents),
        spawn_tile_x=spawn.tile_x,
        spawn_tile_y=spawn.tile_y,
        spawn_pixel_x=spawn.pixel_x,
        spawn_pixel_y=spawn.pixel_y,
        spawn_sub_x=spawn_sub_x,
        spawn_sub_y=spawn_sub_y,
    )
    validate_world(world)
    return world


def pack_tiles(grid: bytearray) -> bytes:
    """Pack a 320 x 160 tile grid into the 6,400 B flash ABI."""
    if len(grid) != WORLD_TILES_X * WORLD_TILES_Y:
        raise ValueError(
            f"world grid must be {WORLD_TILES_X * WORLD_TILES_Y} tiles, "
            f"got {len(grid)}"
        )
    out = bytearray(WORLD_BYTES)
    for y in range(WORLD_TILES_Y):
        row = y * WORLD_TILES_X
        out_row = y * WORLD_ROW_BYTES
        for x in range(WORLD_TILES_X):
            if grid[row + x]:
                out[out_row + (x >> 3)] |= 1 << (x & 7)
    return bytes(out)


def unpack_tiles(data: bytes) -> bytearray:
    """Inverse of :func:`pack_tiles`; used by validation and tool tests."""
    if len(data) != WORLD_BYTES:
        raise ValueError(f"world table must be {WORLD_BYTES} B, got {len(data)}")
    grid = bytearray(WORLD_TILES_X * WORLD_TILES_Y)
    for y in range(WORLD_TILES_Y):
        row = y * WORLD_TILES_X
        data_row = y * WORLD_ROW_BYTES
        for x in range(WORLD_TILES_X):
            if data[data_row + (x >> 3)] & (1 << (x & 7)):
                grid[row + x] = 1
    return grid


def build_chart(grid: bytearray) -> bytes:
    """Downsample open tiles into the 64 x 32 chart water mask (unpacked)."""
    if len(grid) != WORLD_TILES_X * WORLD_TILES_Y:
        raise ValueError("chart input must be a 320 x 160 tile grid")
    chart = bytearray(CHART_TILES_X * CHART_TILES_Y)
    for cy in range(CHART_TILES_Y):
        for cx in range(CHART_TILES_X):
            water = 0
            for ty in range(cy * CHART_CELL_TILES, (cy + 1) * CHART_CELL_TILES):
                row = ty * WORLD_TILES_X
                for tx in range(cx * CHART_CELL_TILES, (cx + 1) * CHART_CELL_TILES):
                    if not grid[row + tx]:
                        water = 1
                        break
                if water:
                    break
            chart[cy * CHART_TILES_X + cx] = water
    return bytes(chart)


def pack_chart(chart: bytes) -> bytes:
    """Pack a 64 x 32 chart mask into the 256 B flash ABI."""
    if len(chart) != CHART_TILES_X * CHART_TILES_Y:
        raise ValueError(
            f"chart mask must be {CHART_TILES_X * CHART_TILES_Y} cells, "
            f"got {len(chart)}"
        )
    out = bytearray(CHART_BYTES)
    for y in range(CHART_TILES_Y):
        row = y * CHART_TILES_X
        out_row = y * CHART_ROW_BYTES
        for x in range(CHART_TILES_X):
            if chart[row + x]:
                out[out_row + (x >> 3)] |= 1 << (x & 7)
    return bytes(out)


def unpack_chart(data: bytes) -> bytes:
    if len(data) != CHART_BYTES:
        raise ValueError(f"chart table must be {CHART_BYTES} B, got {len(data)}")
    chart = bytearray(CHART_TILES_X * CHART_TILES_Y)
    for y in range(CHART_TILES_Y):
        row = y * CHART_TILES_X
        data_row = y * CHART_ROW_BYTES
        for x in range(CHART_TILES_X):
            if data[data_row + (x >> 3)] & (1 << (x & 7)):
                chart[row + x] = 1
    return bytes(chart)


def _open_components(grid: bytearray) -> int:
    seen = bytearray(len(grid))
    components = 0
    for start in range(len(grid)):
        if grid[start] or seen[start]:
            continue
        components += 1
        stack = [start]
        seen[start] = 1
        while stack:
            index = stack.pop()
            x = index % WORLD_TILES_X
            y = index // WORLD_TILES_X
            if x > 0:
                n = index - 1
                if not grid[n] and not seen[n]:
                    seen[n] = 1
                    stack.append(n)
            if x + 1 < WORLD_TILES_X:
                n = index + 1
                if not grid[n] and not seen[n]:
                    seen[n] = 1
                    stack.append(n)
            if y > 0:
                n = index - WORLD_TILES_X
                if not grid[n] and not seen[n]:
                    seen[n] = 1
                    stack.append(n)
            if y + 1 < WORLD_TILES_Y:
                n = index + WORLD_TILES_X
                if not grid[n] and not seen[n]:
                    seen[n] = 1
                    stack.append(n)
    return components


def validate_world(world: World) -> None:
    """Raise ``ValueError`` when the baked world breaks a device invariant."""
    grid = world.grid
    if len(grid) != WORLD_TILES_X * WORLD_TILES_Y:
        raise ValueError("world dimensions are not 320 x 160 tiles")
    if any(value not in (0, 1) for value in grid):
        raise ValueError("world grid contains values other than 0 and 1")

    # Sealed border: two solid tiles on every edge.
    for y in range(WORLD_TILES_Y):
        for x in range(WORLD_TILES_X):
            if (
                x < BORDER_TILES
                or y < BORDER_TILES
                or x >= WORLD_TILES_X - BORDER_TILES
                or y >= WORLD_TILES_Y - BORDER_TILES
            ) and not grid[y * WORLD_TILES_X + x]:
                raise ValueError(f"sealed border broken at tile ({x}, {y})")

    # Spawn: Tidewall dock center open and the empty-hull footprint clear.
    if tile_solid(grid, world.spawn_tile_x, world.spawn_tile_y):
        raise ValueError("spawn tile is solid")
    first_x = math.floor(world.spawn_sub_x / TILE_PIXELS)
    first_y = math.floor(world.spawn_sub_y / TILE_PIXELS)
    last_x = math.floor((world.spawn_sub_x + HULL_WIDTH_PX - 0.001) / TILE_PIXELS)
    last_y = math.floor(
        (world.spawn_sub_y + EMPTY_LOAD_HULL_HEIGHT_PX - 0.001) / TILE_PIXELS
    )
    for ty in range(first_y, last_y + 1):
        for tx in range(first_x, last_x + 1):
            if tile_solid(grid, tx, ty):
                raise ValueError(f"spawn footprint is solid at tile ({tx}, {ty})")

    # All chambers, including dead-end branches, must be open and connected.
    for chamber in world.chambers:
        tx = int(chamber.x)
        ty = int(chamber.y)
        if tile_solid(grid, tx, ty):
            raise ValueError(f"chamber center is solid at tile ({tx}, {ty})")
    if _open_components(grid) != 1:
        raise ValueError("open cave is not one connected component")

    # Settlement centers are open and clear of walls. Tile coordinates must be
    # derivable from the pixel center for the compact AVR record.
    for settlement in world.settlements:
        if (
            settlement.tile_x != settlement.pixel_x // TILE_PIXELS
            or settlement.tile_y != settlement.pixel_y // TILE_PIXELS
        ):
            raise ValueError(f"settlement {settlement.name} tile/pixel mismatch")
        radius = SETTLEMENT_CLEAR_TILES
        for dy in range(-radius, radius + 1):
            for dx in range(-radius, radius + 1):
                if dx * dx + dy * dy > radius * radius:
                    continue
                tx = settlement.tile_x + dx
                ty = settlement.tile_y + dy
                if tile_solid(grid, tx, ty):
                    raise ValueError(
                        f"settlement {settlement.name} is blocked at "
                        f"tile ({tx}, {ty})"
                    )

    # Current zones are metadata-only rectangles. Every tile they overlap must
    # be open water, and the zone must stay outside every settlement dock ring
    # (the spawn is settlement 0, so this also protects the spawn footprint).
    if not world.currents:
        raise ValueError("world has no current zones")
    for index, current in enumerate(world.currents):
        if current.w <= 0 or current.h <= 0:
            raise ValueError(f"current {index} has an empty rectangle")
        if current.w != CURRENT_WIDTH_PX or current.h != CURRENT_HEIGHT_PX:
            raise ValueError(f"current {index} does not use the compact extent")
        if current.ax_q8 == 0 and current.ay_q8 == 0:
            raise ValueError(f"current {index} has no acceleration")
        current_direction(current)  # validates the compact signed-axis code
        if (
            current.x < 0
            or current.y < 0
            or current.x + current.w > WORLD_PIXELS_X
            or current.y + current.h > WORLD_PIXELS_Y
        ):
            raise ValueError(f"current {index} is outside the world")
        right = current.x + current.w - 1
        bottom = current.y + current.h - 1
        for ty in range(current.y // TILE_PIXELS, bottom // TILE_PIXELS + 1):
            for tx in range(current.x // TILE_PIXELS, right // TILE_PIXELS + 1):
                if tile_solid(grid, tx, ty):
                    raise ValueError(
                        f"current {index} is inside rock at tile ({tx}, {ty})"
                    )
        for settlement in world.settlements:
            nearest_x = _clamp(settlement.pixel_x, current.x, right)
            nearest_y = _clamp(settlement.pixel_y, current.y, bottom)
            dx = nearest_x - settlement.pixel_x
            dy = nearest_y - settlement.pixel_y
            if math.hypot(dx, dy) < DOCK_CLEARANCE_PX:
                raise ValueError(
                    f"current {index} is inside {settlement.name} dock clearance"
                )

    # Chart mask must be the documented any-open 5 x 5 downsample.
    expected = build_chart(grid)
    actual = unpack_chart(world.chart)
    if actual != expected:
        for index, (want, got) in enumerate(zip(expected, actual)):
            if want != got:
                raise ValueError(
                    "chart mask disagrees with the tile grid at chart cell "
                    f"({index % CHART_TILES_X}, {index // CHART_TILES_X})"
                )
        raise ValueError("chart mask disagrees with the tile grid")
    for cy in range(CHART_TILES_Y):
        for cx in range(CHART_TILES_X):
            water = actual[cy * CHART_TILES_X + cx]
            open_tiles = False
            solid_tiles = True
            for ty in range(cy * CHART_CELL_TILES, (cy + 1) * CHART_CELL_TILES):
                for tx in range(cx * CHART_CELL_TILES, (cx + 1) * CHART_CELL_TILES):
                    if not grid[ty * WORLD_TILES_X + tx]:
                        open_tiles = True
                        solid_tiles = False
            if water and not open_tiles:
                raise ValueError(f"chart water cell ({cx}, {cy}) has no open tiles")
            if not water and not solid_tiles:
                raise ValueError(f"chart land cell ({cx}, {cy}) has open tiles")


def _cpp_bool(value: bool) -> str:
    return "true" if value else "false"


def render_header(world: World, service_berths=None, surveys=None, variable_current_extents=False) -> str:
    service_berths = tuple(service_berths or ())
    surveys = tuple(surveys or ())
    berth_count = max([len(world.settlements)] + [b.id + 1 for b in service_berths])
    service_structs = ""
    service_avr = ""
    service_host = ""
    service_avr_accessors = ""
    service_host_accessors = ""
    survey_structs = ""
    survey_avr = ""
    survey_host = ""
    survey_avr_accessors = ""
    survey_host_accessors = ""
    berth_constants = "\n".join(
        f"constexpr uint8_t BERTH_{b.symbol} = {b.id};" for b in service_berths
    )
    survey_constants = "\n".join(
        f"constexpr uint8_t SURVEY_{s.symbol} = {s.id};" for s in surveys
    )
    if service_berths:
        service_meta = "\n".join(
            f"    {{{b.id}, {b.milestone}, {b.pixel_x}, {b.pixel_y}}},"
            for b in service_berths
        )
        service_rows = "\n".join(
            f"    {{{b.id}, {b.milestone}, \"{b.name}\", {b.tile_x}, {b.tile_y}, {b.pixel_x}, {b.pixel_y}}},"
            for b in service_berths
        )
        service_structs = """
// Service-only berth: a generated docking point with its restoration
// milestone bit. Berth ids 0..SETTLEMENT_COUNT-1 are freight settlements;
// these records are appended ids and never enter the offer economy.
struct Berth {
    uint8_t id;
    uint8_t milestone;
    const char* name;
    int16_t tile_x;
    int16_t tile_y;
    int16_t pixel_x;
    int16_t pixel_y;
};
"""
        service_avr = f"""
struct ServiceBerthMeta {{
    uint8_t id;
    uint8_t milestone;
    int16_t pixel_x;
    int16_t pixel_y;
}};

const ServiceBerthMeta SERVICE_BERTH_META[SERVICE_BERTH_COUNT] PROGMEM = {{
{service_meta}
}};
"""
        service_host = f"""
constexpr Berth SERVICE_BERTHS[SERVICE_BERTH_COUNT] = {{
{service_rows}
}};
"""
        service_avr_accessors = """
inline bool serviceBerthMeta(uint8_t index, Berth &out) {
    if (index >= SERVICE_BERTH_COUNT)
        return false;
    out.id = pgm_read_byte(&SERVICE_BERTH_META[index].id);
    out.milestone = pgm_read_byte(&SERVICE_BERTH_META[index].milestone);
    out.name = nullptr;
    out.pixel_x = static_cast<int16_t>(pgm_read_word(&SERVICE_BERTH_META[index].pixel_x));
    out.pixel_y = static_cast<int16_t>(pgm_read_word(&SERVICE_BERTH_META[index].pixel_y));
    out.tile_x = static_cast<int16_t>(out.pixel_x / TILE_PIXELS);
    out.tile_y = static_cast<int16_t>(out.pixel_y / TILE_PIXELS);
    return true;
}

inline bool berthMeta(uint8_t id, Berth &out) {
    if (id < SETTLEMENT_COUNT) {
        out.id = id;
        out.milestone = 0;
        out.name = nullptr;
        out.pixel_x = static_cast<int16_t>(pgm_read_word(&SETTLEMENT_META[id].pixel_x));
        out.pixel_y = static_cast<int16_t>(pgm_read_word(&SETTLEMENT_META[id].pixel_y));
        out.tile_x = static_cast<int16_t>(out.pixel_x / TILE_PIXELS);
        out.tile_y = static_cast<int16_t>(out.pixel_y / TILE_PIXELS);
        return true;
    }
    for (uint8_t i = 0; i < SERVICE_BERTH_COUNT; ++i) {
        if (pgm_read_byte(&SERVICE_BERTH_META[i].id) == id)
            return serviceBerthMeta(i, out);
    }
    return false;
}
"""
        service_host_accessors = """
inline bool serviceBerthMeta(uint8_t index, Berth &out) {
    if (index >= SERVICE_BERTH_COUNT)
        return false;
    out = SERVICE_BERTHS[index];
    return true;
}

inline bool berthMeta(uint8_t id, Berth &out) {
    if (id < SETTLEMENT_COUNT) {
        const Settlement &settlement = SETTLEMENTS[id];
        out.id = settlement.id;
        out.milestone = 0;
        out.name = settlement.name;
        out.tile_x = settlement.tile_x;
        out.tile_y = settlement.tile_y;
        out.pixel_x = settlement.pixel_x;
        out.pixel_y = settlement.pixel_y;
        return true;
    }
    for (uint8_t i = 0; i < SERVICE_BERTH_COUNT; ++i) {
        if (SERVICE_BERTHS[i].id == id) {
            out = SERVICE_BERTHS[i];
            return true;
        }
    }
    return false;
}
"""
    if surveys:
        survey_meta = "\n".join(
            f"    {{{s.id}, {s.pixel_x}, {s.pixel_y}}},"
            for s in surveys
        )
        survey_rows = "\n".join(
            f"    {{{s.id}, \"{s.name}\", {s.pixel_x}, {s.pixel_y}}},"
            for s in surveys
        )
        survey_structs = """
// Survey ring: a survey fact source in navigable water. Ids are stable and
// distinct from pickup and gate ids; a ring never occupies the cargo slot.
struct Survey {
    uint8_t id;
    const char* name;
    int16_t pixel_x;
    int16_t pixel_y;
};
"""
        survey_avr = f"""
struct SurveyMeta {{
    uint8_t id;
    int16_t pixel_x;
    int16_t pixel_y;
}};

const SurveyMeta SURVEY_META[SURVEY_COUNT] PROGMEM = {{
{survey_meta}
}};
"""
        survey_host = f"""
constexpr Survey SURVEYS[SURVEY_COUNT] = {{
{survey_rows}
}};
"""
        survey_avr_accessors = """
inline bool surveyMeta(uint8_t index, Survey &out) {
    if (index >= SURVEY_COUNT)
        return false;
    out.id = pgm_read_byte(&SURVEY_META[index].id);
    out.name = nullptr;
    out.pixel_x = static_cast<int16_t>(pgm_read_word(&SURVEY_META[index].pixel_x));
    out.pixel_y = static_cast<int16_t>(pgm_read_word(&SURVEY_META[index].pixel_y));
    return true;
}
"""
        survey_host_accessors = """
inline bool surveyMeta(uint8_t index, Survey &out) {
    if (index >= SURVEY_COUNT)
        return false;
    out = SURVEYS[index];
    return true;
}
"""
    settlements = "\n".join(
        "    {"
        f"{s.id}, \"{s.name}\", {s.tile_x}, {s.tile_y}, "
        f"{s.pixel_x}, {s.pixel_y}"
        "},"
        for s in world.settlements
    )
    currents = "\n".join(
        "    {"
        f"{current.x}, {current.y}, {current.w}, {current.h}, "
        f"{current.ax_q8}, {current.ay_q8}"
        "},"
        for current in world.currents
    )
    settlement_meta = "\n".join(
        f"    {{{s.pixel_x}, {s.pixel_y}}}," for s in world.settlements
    )
    if variable_current_extents:
        for current in world.currents:
            if any(size % TILE_PIXELS or not 0 < size // TILE_PIXELS <= 255
                   for size in (current.w, current.h)):
                raise ValueError('current extents must fit whole-tile byte counts')
        current_meta = "\n".join(
            f"    {{{current.x}, {current.y}, {current.w // TILE_PIXELS}, {current.h // TILE_PIXELS}, {current_direction(current)}}},"
            for current in world.currents
        )
        current_meta_struct = """struct CurrentMeta {
    int16_t x;
    int16_t y;
    uint8_t tileW;
    uint8_t tileH;
    uint8_t dir;
};"""
        current_meta_doc = """// Compact position-only copy. Current extents are per-zone because the
// generated zones cover their actual passage cross-sections. Tile counts keep
// extents over 255 pixels compact; direction is one compact code."""
        current_meta_read = """    out.w = static_cast<uint16_t>(pgm_read_byte(&CURRENT_META[id].tileW)) * TILE_PIXELS;
    out.h = static_cast<uint16_t>(pgm_read_byte(&CURRENT_META[id].tileH)) * TILE_PIXELS;"""
    else:
        current_meta = "\n".join(
            f"    {{{current.x}, {current.y}, {current_direction(current)}}},"
            for current in world.currents
        )
        current_meta_struct = """struct CurrentMeta {
    int16_t x;
    int16_t y;
    uint8_t dir;
};"""
        current_meta_doc = """// Compact position-only copy. Current extents are the generated constants and
// the signed axis direction is one compact code."""
        current_meta_read = """    out.w = CURRENT_WIDTH_PX;
    out.h = CURRENT_HEIGHT_PX;"""
    return f"""// Generated by tools/gen_world.py. Do not edit.
//
// Archival legacy world tables (M1); not packed.
// This header is pure C++ on the host; on AVR it adds
// one guarded <avr/pgmspace.h> include for the flash metadata path. Host tests
// may include it directly.
//
// Flash ABI:
//   world.bin        {WORLD_BYTES} B, {WORLD_TILES_X}x{WORLD_TILES_Y} tiles, 1 bpp, 1 = solid
//                    row-major, {WORLD_ROW_BYTES} B/row, LSB = leftmost tile
//   world-chart.bin  {CHART_BYTES} B, {CHART_TILES_X}x{CHART_TILES_Y} cells, 1 bpp, 1 = water
//                    cell = any open tile in a {CHART_CELL_TILES}x{CHART_CELL_TILES} block,
//                    row-major, {CHART_ROW_BYTES} B/row, LSB = leftmost cell
//
// Metadata representation:
//   AVR   position-only records in PROGMEM, read with pgm_read_byte/word.
//   host  constexpr records including settlement names.
// A namespace-scope constexpr array that is odr-used is copied into SRAM by the
// AVR startup code, and name pointers would drag their string literals along
// with it. Device code therefore never references SETTLEMENTS/CURRENTS;
// it reads the small PROGMEM copy through settlementMeta()/currentMeta().
// Names stay host-only so no string literal is linked into the device image. Current zones are metadata only: they do not change the FX
// table ABI.
#pragma once

#include <stdint.h>
#if defined(__AVR__)
#include <avr/pgmspace.h>
#endif

namespace world {{

constexpr uint8_t  LAYOUT_VERSION    = {LAYOUT_VERSION};
constexpr uint16_t SEED              = {world.seed};
constexpr uint16_t TILES_X           = {WORLD_TILES_X};
constexpr uint16_t TILES_Y           = {WORLD_TILES_Y};
constexpr uint8_t  TILE_PIXELS       = {TILE_PIXELS};
constexpr uint16_t PIXELS_X          = {WORLD_PIXELS_X};
constexpr uint16_t PIXELS_Y          = {WORLD_PIXELS_Y};
constexpr uint8_t  WORLD_ROW_BYTES   = {WORLD_ROW_BYTES};
constexpr uint16_t WORLD_BYTES       = {WORLD_BYTES};
constexpr uint8_t  CHART_TILES_X     = {CHART_TILES_X};
constexpr uint8_t  CHART_TILES_Y     = {CHART_TILES_Y};
constexpr uint8_t  CHART_CELL_TILES  = {CHART_CELL_TILES};
constexpr uint8_t  CHART_ROW_BYTES   = {CHART_ROW_BYTES};
constexpr uint16_t CHART_BYTES       = {CHART_BYTES};
constexpr uint8_t  SETTLEMENT_COUNT  = {len(world.settlements)};
constexpr uint8_t  CURRENT_COUNT     = {len(world.currents)};
constexpr uint8_t  BERTH_COUNT       = {berth_count};
constexpr uint8_t  SERVICE_BERTH_COUNT = {len(service_berths)};
constexpr uint8_t  SURVEY_COUNT      = {len(surveys)};
{berth_constants}
{survey_constants}
constexpr int16_t  DOCK_CLEARANCE_PX = {DOCK_CLEARANCE_PX};
constexpr uint8_t  CURRENT_WIDTH_PX  = {CURRENT_WIDTH_PX};
constexpr uint8_t  CURRENT_HEIGHT_PX = {CURRENT_HEIGHT_PX};
constexpr int16_t  CURRENT_ACCEL_Q8 = {CURRENT_ACCEL_Q8};
constexpr uint8_t  CURRENT_DIR_XPOS = {CURRENT_DIR_XPOS};
constexpr uint8_t  CURRENT_DIR_YPOS = {CURRENT_DIR_YPOS};
constexpr uint8_t  CURRENT_DIR_XNEG = {CURRENT_DIR_XNEG};
constexpr uint8_t  CURRENT_DIR_YNEG = {CURRENT_DIR_YNEG};
constexpr bool     CHART_ANY_OPEN    = {_cpp_bool(True)};

constexpr int16_t SPAWN_TILE_X  = {world.spawn_tile_x};
constexpr int16_t SPAWN_TILE_Y  = {world.spawn_tile_y};
constexpr int16_t SPAWN_PIXEL_X = {world.spawn_pixel_x};
constexpr int16_t SPAWN_PIXEL_Y = {world.spawn_pixel_y};
constexpr int16_t SPAWN_SUB_X   = {world.spawn_sub_x};
constexpr int16_t SPAWN_SUB_Y   = {world.spawn_sub_y};

struct Settlement {{
    uint8_t id;
    const char* name;
    int16_t tile_x;
    int16_t tile_y;
    int16_t pixel_x;
    int16_t pixel_y;
}};

// Current zone: a pixel rectangle and a signed Q8.8 acceleration. Exactly one
// acceleration axis is non-zero for the generated set.
struct Current {{
    int16_t x;
    int16_t y;
    {'uint16_t' if variable_current_extents else 'uint8_t'} w;
    {'uint16_t' if variable_current_extents else 'uint8_t'} h;
    int16_t ax_q8;
    int16_t ay_q8;
}};
{service_structs}{survey_structs}
#if defined(__AVR__)

// Compact position-only copy in program flash. Settlement ids are implicit in
// the index and tile coordinates derive from the pixel pair.
{current_meta_doc}
struct SettlementMeta {{
    int16_t pixel_x;
    int16_t pixel_y;
}};

const SettlementMeta SETTLEMENT_META[SETTLEMENT_COUNT] PROGMEM = {{
{settlement_meta}
}};

{current_meta_struct}

const CurrentMeta CURRENT_META[CURRENT_COUNT] PROGMEM = {{
{current_meta}
}};

inline bool settlementMeta(uint8_t id, Settlement &out) {{
    if (id >= SETTLEMENT_COUNT)
        return false;
    out.id = id;
    out.name = nullptr;
    out.pixel_x = static_cast<int16_t>(pgm_read_word(&SETTLEMENT_META[id].pixel_x));
    out.pixel_y = static_cast<int16_t>(pgm_read_word(&SETTLEMENT_META[id].pixel_y));
    out.tile_x = static_cast<int16_t>(out.pixel_x / TILE_PIXELS);
    out.tile_y = static_cast<int16_t>(out.pixel_y / TILE_PIXELS);
    return true;
}}

inline bool currentMeta(uint8_t id, Current &out) {{
    if (id >= CURRENT_COUNT)
        return false;
    out.x = static_cast<int16_t>(pgm_read_word(&CURRENT_META[id].x));
    out.y = static_cast<int16_t>(pgm_read_word(&CURRENT_META[id].y));
{current_meta_read}
    const uint8_t direction = pgm_read_byte(&CURRENT_META[id].dir);
    out.ax_q8 = direction == CURRENT_DIR_XPOS
                    ? CURRENT_ACCEL_Q8
                    : (direction == CURRENT_DIR_XNEG ? static_cast<int16_t>(-CURRENT_ACCEL_Q8) : 0);
    out.ay_q8 = direction == CURRENT_DIR_YPOS
                    ? CURRENT_ACCEL_Q8
                    : (direction == CURRENT_DIR_YNEG ? static_cast<int16_t>(-CURRENT_ACCEL_Q8) : 0);
    return true;
}}
{service_avr}{service_avr_accessors}{survey_avr}{survey_avr_accessors}
#else

constexpr Settlement SETTLEMENTS[SETTLEMENT_COUNT] = {{
{settlements}
}};

constexpr Current CURRENTS[CURRENT_COUNT] = {{
{currents}
}};

inline bool settlementMeta(uint8_t id, Settlement &out) {{
    if (id >= SETTLEMENT_COUNT)
        return false;
    out = SETTLEMENTS[id];
    return true;
}}

inline bool currentMeta(uint8_t id, Current &out) {{
    if (id >= CURRENT_COUNT)
        return false;
    out = CURRENTS[id];
    return true;
}}
{service_host}{service_host_accessors}{survey_host}{survey_host_accessors}
#endif

}}  // namespace world
"""


def write_world(
    world: World,
    world_path: Path = ROOT / "fxdata" / "tables" / "world.bin",
    chart_path: Path = ROOT / "fxdata" / "tables" / "world-chart.bin",
    header_path: Path = ROOT / "src" / "generated" / "world.hpp",
) -> tuple[Path, Path, Path]:
    """Write all three artifacts; returns the paths that were written."""
    world_path = Path(world_path)
    chart_path = Path(chart_path)
    header_path = Path(header_path)
    for path in (world_path, chart_path, header_path):
        path.parent.mkdir(parents=True, exist_ok=True)
    world_path.write_bytes(pack_tiles(world.grid))
    chart_path.write_bytes(world.chart)
    header_path.write_text(render_header(world), encoding="utf-8")
    return world_path, chart_path, header_path


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument(
        "--world",
        type=Path,
        default=ROOT / "fxdata" / "tables" / "world.bin",
        help="output 320x160 1-bpp solidity plane",
    )
    parser.add_argument(
        "--chart",
        type=Path,
        default=ROOT / "fxdata" / "tables" / "world-chart.bin",
        help="output 64x32 1-bpp chart water mask",
    )
    parser.add_argument(
        "--header",
        type=Path,
        default=ROOT / "src" / "generated" / "world.hpp",
        help="output C++ metadata header",
    )
    args = parser.parse_args(argv)

    world = generate_cave(args.seed)
    paths = write_world(world, args.world, args.chart, args.header)
    print(
        "gen_world: "
        f"seed={world.seed} world={paths[0].stat().st_size}B "
        f"chart={paths[1].stat().st_size}B "
        f"settlements={len(world.settlements)} "
        f"currents={len(world.currents)} "
        "validation=pass"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Generate the deterministic music region plane and its host test metadata.

The plane has one byte per 8x8 world tile. Every tile is owned by the nearest
authored room centre, so connecting passages have one owner independent of
approach history; ties use the lower room index. A room's optional
``music_region`` overrides its scenery ``region`` for the cue plane. Small
open-water components cut off by gates are merged into the neighbouring region
so the map and cue plane keep readable boundaries. Rooms are authored in
``levels/exploration.json``. The device reads the plane from FX; the generated
header keeps the room table for native tests and documents the FX symbol.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "levels/exploration.json"
HEADER = ROOT / "src/generated/music_regions.hpp"
PLANE = ROOT / "fxdata/tables/music-regions.bin"
COLLISION = ROOT / "fxdata/tables/exploration.bin"

REGION_IDS = {"tide": 0, "kelp": 1, "works": 2, "deep": 3}
TILES_X = 320
TILES_Y = 160
# Small cue fragments cut off by gates are boundary noise, not a readable
# region. Merge them into the neighbouring region with the most shared edge.
MIN_REGION_TILES = 256


def build(source):
    rooms = source["rooms"]
    if len(rooms) != 32:
        raise ValueError("expected the authored 32-room table, found {}".format(len(rooms)))
    rows = []
    for room in rooms:
        region_name = room.get("music_region", room.get("region"))
        region = REGION_IDS.get(region_name)
        if region is None:
            raise ValueError("{}: unknown music region {!r}".format(room.get("id"), region_name))
        x, y = room["position"]
        radius = int(room.get("radius", 5))
        if not (0 <= x < source["dimensions"][0] and 0 <= y < source["dimensions"][1]):
            raise ValueError("{}: room centre outside the map".format(room["id"]))
        if not (1 <= radius <= 32):
            raise ValueError("{}: room radius outside the bounded lookup".format(room["id"]))
        rows.append((region, x, y, radius))
    return rows


def owner(rows, tile_x, tile_y):
    pixel_x = tile_x * 8 + 4
    pixel_y = tile_y * 8 + 4
    best = None
    best_distance = None
    for region, x, y, _radius in rows:
        dx = pixel_x - (x * 8 + 4)
        dy = pixel_y - (y * 8 + 4)
        distance = dx * dx + dy * dy
        if best_distance is None or distance < best_distance:
            best_distance = distance
            best = region
    return best


def open_water():
    """Return the packed collision predicate; 1 = solid rock."""
    packed = COLLISION.read_bytes()
    if len(packed) != TILES_X * TILES_Y // 8:
        raise ValueError("expected a {}x{} packed collision plane".format(TILES_X, TILES_Y))

    def is_open(x, y):
        index = y * TILES_X + x
        return ((packed[index // 8] >> (index % 8)) & 1) == 0
    return is_open


def components(plane, is_open):
    """Connected open-water components of the raw cue plane."""
    seen = set()
    result = []
    for y in range(TILES_Y):
        for x in range(TILES_X):
            if (x, y) in seen or not is_open(x, y):
                continue
            region = plane[y * TILES_X + x]
            stack = [(x, y)]
            seen.add((x, y))
            cells = []
            while stack:
                cx, cy = stack.pop()
                cells.append((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if (0 <= nx < TILES_X and 0 <= ny < TILES_Y and (nx, ny) not in seen
                            and plane[ny * TILES_X + nx] == region and is_open(nx, ny)):
                        seen.add((nx, ny))
                        stack.append((nx, ny))
            result.append((region, cells))
    return result


def clean_plane(plane, is_open):
    """Relabel tiny gate-cut fragments so every cue region is readable."""
    cleaned = bytearray(plane)
    while True:
        changed = False
        for region, cells in components(cleaned, is_open):
            if len(cells) >= MIN_REGION_TILES:
                continue
            votes = {}
            for x, y in cells:
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < TILES_X and 0 <= ny < TILES_Y and is_open(nx, ny):
                        neighbour = cleaned[ny * TILES_X + nx]
                        if neighbour != region:
                            votes[neighbour] = votes.get(neighbour, 0) + 1
            if not votes:
                continue
            replacement = max(votes, key=lambda value: (votes[value], -value))
            for x, y in cells:
                cleaned[y * TILES_X + x] = replacement
            changed = True
        if not changed:
            return bytes(cleaned)


def plane(rows):
    raw = bytes(owner(rows, x, y) for y in range(TILES_Y) for x in range(TILES_X))
    return clean_plane(raw, open_water())


def render(rows):
    lines = [
        "// Generated by tools/gen_music_regions.py. Do not edit.",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "namespace music_regions {",
        "constexpr uint8_t kRoomCount = {};".format(len(rows)),
        f"constexpr uint16_t kTilesX = {TILES_X};",
        f"constexpr uint16_t kTilesY = {TILES_Y};",
        f"constexpr uint16_t kPlaneBytes = {TILES_X * TILES_Y};",
        "constexpr uint8_t kRegionTide = 0;",
        "constexpr uint8_t kRegionKelp = 1;",
        "constexpr uint8_t kRegionWorks = 2;",
        "constexpr uint8_t kRegionDeep = 3;",
        "#if !defined(__AVR__)",
        "static const uint8_t kRegion[kRoomCount] = {",
        "    " + ", ".join(str(row[0]) for row in rows) + ",",
        "};",
        "static const uint16_t kCenterX[kRoomCount] = {",
        "    " + ", ".join(str(row[1]) for row in rows) + ",",
        "};",
        "static const uint8_t kCenterY[kRoomCount] = {",
        "    " + ", ".join(str(row[2]) for row in rows) + ",",
        "};",
        "static const uint8_t kRadius[kRoomCount] = {",
        "    " + ", ".join(str(row[3]) for row in rows) + ",",
        "};",
        "#endif",
        "} // namespace music_regions",
        "",
    ]
    return "\n".join(lines)


def write(source=SOURCE, header=HEADER, plane_path=PLANE):
    source = Path(source)
    rows = build(json.loads(source.read_text()))
    header = Path(header)
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(render(rows))
    plane_path = Path(plane_path)
    plane_path.parent.mkdir(parents=True, exist_ok=True)
    plane_path.write_bytes(plane(rows))
    return header, plane_path


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", default=str(SOURCE))
    parser.add_argument("--header", default=str(HEADER))
    parser.add_argument("--plane", default=str(PLANE))
    args = parser.parse_args(argv)
    header, plane_path = write(args.source, args.header, args.plane)
    print("gen_music_regions: wrote {} and {}".format(header.relative_to(ROOT), plane_path.relative_to(ROOT)))


if __name__ == "__main__":
    main()

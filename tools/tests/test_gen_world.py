import copy
import hashlib
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import gen_world  # noqa: E402


class GenWorldTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.world = gen_world.generate_cave(4242)

    def corrupt(self):
        return copy.deepcopy(self.world)

    def test_known_layout_bytes(self):
        packed = gen_world.pack_tiles(self.world.grid)
        self.assertEqual(len(packed), gen_world.WORLD_BYTES)
        self.assertEqual(gen_world.WORLD_BYTES, 6400)
        self.assertEqual(
            hashlib.sha256(packed).hexdigest(),
            "78b6b51d763b447af299e80e80697bd6fd3d3f2bf4da20becf5d04c50ed61873",
        )
        self.assertEqual(sum(self.world.grid), 43938)
        self.assertEqual(
            hashlib.sha256(self.world.chart).hexdigest(),
            "f1e9c54a1029b8a936fffbe6842d30a3a67b47bc74817fcedcea8167c8362e03",
        )

    def test_known_tile_bits(self):
        grid = self.world.grid
        self.assertEqual(len(grid), 320 * 160)

        def tile(x, y):
            return grid[y * 320 + x]

        # Sealed two-tile border.
        self.assertEqual(tile(0, 0), 1)
        self.assertEqual(tile(1, 159), 1)
        self.assertEqual(tile(319, 80), 1)
        self.assertEqual(tile(160, 159), 1)
        # Settlement and branch chambers are open.
        for x, y in (
            (38, 48),
            (150, 30),
            (282, 62),
            (172, 132),
            (91, 72),
            (72, 22),
            (168, 72),
            (292, 140),
            (44, 108),
        ):
            self.assertEqual(tile(x, y), 0, (x, y))
        # Open spawn footprint.
        for y in (48, 49):
            for x in range(36, 40):
                self.assertEqual(tile(x, y), 0, (x, y))
        # Interior rock sample.
        self.assertEqual(tile(5, 80), 1)
        self.assertEqual(tile(160, 80), 1)

    def test_known_settlements_and_mines(self):
        self.assertEqual(
            [
                (s.id, s.name, s.tile_x, s.tile_y, s.pixel_x, s.pixel_y)
                for s in self.world.settlements
            ],
            [
                (0, "TIDEWALL", 38, 48, 304, 384),
                (1, "BENTHOS", 150, 30, 1200, 240),
                (2, "CORMORANT", 282, 62, 2256, 496),
                (3, "DEEP ANCHOR", 172, 132, 1376, 1056),
            ],
        )
        self.assertEqual(
            [(mine.x, mine.y) for mine in self.world.mines],
            [
                (690, 588),
                (1724, 273),
                (1976, 822),
                (756, 1106),
                (604, 161),
                (1318, 607),
                (2359, 1143),
                (374, 835),
            ],
        )
        self.assertEqual((self.world.spawn_sub_x, self.world.spawn_sub_y), (291, 388))
        self.assertEqual(
            [
                (c.x, c.y, c.w, c.h, c.ax_q8, c.ay_q8)
                for c in self.world.currents
            ],
            [
                (712, 568, 32, 16, 4096, 0),
                (1744, 248, 32, 16, 0, 4096),
                (2000, 832, 32, 16, -4096, 0),
                (784, 1096, 32, 16, 0, -4096),
                (560, 168, 32, 16, 4096, 0),
                (1328, 568, 32, 16, 0, 4096),
                (2320, 1112, 32, 16, -4096, 0),
                (336, 856, 32, 16, 0, -4096),
            ],
        )

    def test_known_chart_samples(self):
        chart = gen_world.unpack_chart(self.world.chart)
        self.assertEqual(len(chart), 64 * 32)

        def cell(x, y):
            return chart[y * 64 + x]

        self.assertEqual(cell(0, 0), 0)
        self.assertEqual(cell(63, 31), 0)
        self.assertEqual(cell(5, 17), 0)
        # Tidewall, Benthos, Cormorant, Deep Anchor cells.
        for x, y in ((7, 9), (30, 6), (56, 12), (34, 26)):
            self.assertEqual(cell(x, y), 1, (x, y))

    def test_chart_is_any_open_tile_downsample(self):
        chart = gen_world.build_chart(self.world.grid)
        self.assertEqual(gen_world.pack_chart(chart), self.world.chart)
        for cy in range(32):
            for cx in range(64):
                water = chart[cy * 64 + cx]
                tiles = [
                    self.world.grid[ty * 320 + tx]
                    for ty in range(cy * 5, cy * 5 + 5)
                    for tx in range(cx * 5, cx * 5 + 5)
                ]
                self.assertEqual(water, 0 if all(tiles) else 1, (cx, cy))

    def test_bitplane_roundtrip(self):
        packed = gen_world.pack_tiles(self.world.grid)
        self.assertEqual(gen_world.unpack_tiles(packed), self.world.grid)
        chart = gen_world.unpack_chart(self.world.chart)
        self.assertEqual(gen_world.pack_chart(chart), self.world.chart)

    def test_generation_is_deterministic(self):
        first = gen_world.generate_cave(4242)
        second = gen_world.generate_cave(4242)
        self.assertEqual(gen_world.pack_tiles(first.grid), gen_world.pack_tiles(second.grid))
        self.assertEqual(first.chart, second.chart)
        self.assertEqual(first.mines, second.mines)
        self.assertEqual(first.currents, second.currents)

    def test_cli_writes_byte_identical_outputs(self):
        out = ROOT / "build" / "tests" / "world"
        paths = []
        for name in ("a", "b"):
            world_path = out / f"{name}.bin"
            chart_path = out / f"{name}-chart.bin"
            header_path = out / f"{name}.hpp"
            self.assertEqual(
                gen_world.main(
                    [
                        "--seed",
                        "4242",
                        "--world",
                        str(world_path),
                        "--chart",
                        str(chart_path),
                        "--header",
                        str(header_path),
                    ]
                ),
                0,
            )
            paths.append((world_path, chart_path, header_path))
        for left, right in zip(paths[0], paths[1]):
            self.assertEqual(left.read_bytes(), right.read_bytes())
        self.assertEqual(paths[0][0].stat().st_size, 6400)
        self.assertEqual(paths[0][1].stat().st_size, 256)

    def test_header_metadata_representation(self):
        header = gen_world.render_header(self.world)
        self.assertIn("#pragma once", header)
        self.assertIn("#include <stdint.h>", header)
        self.assertNotIn("Arduino.h", header)
        # The PROGMEM include and records must stay AVR-guarded so host tests
        # compile the pure-C++ branch.
        self.assertIn("#if defined(__AVR__)", header)
        self.assertIn("#include <avr/pgmspace.h>", header)
        self.assertIn("SETTLEMENT_META[SETTLEMENT_COUNT] PROGMEM", header)
        self.assertIn("MINE_META[MINE_COUNT] PROGMEM", header)
        self.assertIn("CURRENT_META[CURRENT_COUNT] PROGMEM", header)
        self.assertIn("constexpr Settlement SETTLEMENTS", header)
        self.assertIn("constexpr Current CURRENTS", header)
        self.assertIn("inline bool settlementMeta", header)
        self.assertIn("inline bool mineMeta", header)
        self.assertIn("inline bool currentMeta", header)

        # AVR metadata is position-only: the guarded branch must not reference
        # settlement name literals, which would otherwise land in .rodata and
        # be copied into SRAM.
        device_start = header.index("#if defined(__AVR__)", header.index("struct Mine"))
        device_end = header.index("#else", device_start)
        device_section = header[device_start:device_end]
        for name in ("TIDEWALL", "BENTHOS", "CORMORANT", "DEEP ANCHOR"):
            self.assertNotIn(name, device_section)

        for symbol in (
            "LAYOUT_VERSION",
            "WORLD_BYTES",
            "CHART_BYTES",
            "SPAWN_TILE_X",
            "SETTLEMENT_COUNT",
            "MINE_COUNT",
            "CURRENT_COUNT",
        ):
            self.assertIn(symbol, header)

    def test_rejects_bad_grid_dimensions(self):
        with self.assertRaises(ValueError):
            gen_world.pack_tiles(bytearray(10))
        with self.assertRaises(ValueError):
            gen_world.unpack_tiles(b"\x00" * 10)
        with self.assertRaises(ValueError):
            gen_world.build_chart(bytearray(10))
        with self.assertRaises(ValueError):
            gen_world.pack_chart(b"\x00" * 10)

    def test_rejects_grid_value_outside_binary(self):
        bad = self.corrupt()
        bad.grid[10] = 2
        with self.assertRaisesRegex(ValueError, "0 and 1"):
            gen_world.validate_world(bad)

    def test_rejects_broken_border(self):
        bad = self.corrupt()
        bad.grid[0] = 0
        with self.assertRaisesRegex(ValueError, "sealed border"):
            gen_world.validate_world(bad)

    def test_rejects_solid_spawn(self):
        bad = self.corrupt()
        bad.grid[bad.spawn_tile_y * 320 + bad.spawn_tile_x] = 1
        with self.assertRaisesRegex(ValueError, "spawn"):
            gen_world.validate_world(bad)

    def test_rejects_blocked_settlement(self):
        bad = self.corrupt()
        bad.grid[48 * 320 + 41] = 1
        with self.assertRaisesRegex(ValueError, "settlement TIDEWALL"):
            gen_world.validate_world(bad)

    def test_rejects_disconnected_cave(self):
        bad = self.corrupt()
        for y in range(18, 78):
            index = y * 320 + 80
            if not bad.grid[index]:
                bad.grid[index] = 1
        self.assertGreater(gen_world._open_components(bad.grid), 1)
        with self.assertRaisesRegex(ValueError, "not one connected"):
            gen_world.validate_world(bad)

    def test_rejects_mine_in_rock(self):
        bad = self.corrupt()
        bad.mines = (gen_world.Mine(0, 0),) + bad.mines[1:]
        with self.assertRaisesRegex(ValueError, "inside rock"):
            gen_world.validate_world(bad)

    def test_rejects_mine_inside_dock_clearance(self):
        bad = self.corrupt()
        settlement = bad.settlements[0]
        bad.mines = (
            gen_world.Mine(settlement.pixel_x + 1, settlement.pixel_y),
        ) + bad.mines[1:]
        with self.assertRaisesRegex(ValueError, "dock clearance"):
            gen_world.validate_world(bad)

    def test_rejects_missing_mines(self):
        bad = self.corrupt()
        bad.mines = ()
        with self.assertRaisesRegex(ValueError, "no mine sites"):
            gen_world.validate_world(bad)

    def test_rejects_current_in_rock(self):
        bad = self.corrupt()
        bad.currents = (gen_world.Current(0, 0, 32, 16, 4096, 0),) + bad.currents[1:]
        with self.assertRaisesRegex(ValueError, "inside rock"):
            gen_world.validate_world(bad)

    def test_rejects_current_inside_dock_clearance(self):
        bad = self.corrupt()
        settlement = bad.settlements[0]
        bad.currents = (
            gen_world.Current(settlement.pixel_x - 8, settlement.pixel_y - 8, 32, 16, 4096, 0),
        ) + bad.currents[1:]
        with self.assertRaisesRegex(ValueError, "dock clearance"):
            gen_world.validate_world(bad)

    def test_rejects_current_without_acceleration(self):
        bad = self.corrupt()
        first = bad.currents[0]
        bad.currents = (
            gen_world.Current(first.x, first.y, first.w, first.h, 0, 0),
        ) + bad.currents[1:]
        with self.assertRaisesRegex(ValueError, "no acceleration"):
            gen_world.validate_world(bad)

    def test_rejects_missing_currents(self):
        bad = self.corrupt()
        bad.currents = ()
        with self.assertRaisesRegex(ValueError, "no current zones"):
            gen_world.validate_world(bad)

    def test_rejects_chart_mask_drift(self):
        bad = self.corrupt()
        chart = bytearray(bad.chart)
        chart[0] ^= 1
        bad.chart = bytes(chart)
        with self.assertRaisesRegex(ValueError, "chart mask disagrees"):
            gen_world.validate_world(bad)


if __name__ == "__main__":
    unittest.main()

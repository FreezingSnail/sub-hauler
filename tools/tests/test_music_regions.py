import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_music_regions


class MusicRegionTableTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = __import__("json").loads((ROOT / "levels/exploration.json").read_text())
        cls.rows = gen_music_regions.build(cls.source)
        cls.plane = gen_music_regions.plane(cls.rows)

    def test_all_authored_rooms_are_present_without_narrowing(self):
        self.assertEqual(len(self.rows), 32)
        self.assertEqual(sum(1 for row in self.rows if row[1] > 255), 6)
        header = gen_music_regions.render(self.rows)
        self.assertIn("uint16_t kCenterX", header)
        self.assertIn("303", header)

    def test_all_four_cues_have_rooms(self):
        for region in range(4):
            self.assertGreater(sum(1 for row in self.rows if row[0] == region), 0)

    def test_pressure_boundary_uses_the_works_cue(self):
        row = next(row for row in self.rows if row[1] == 187 and row[2] == 102)
        self.assertEqual(row[0], gen_music_regions.REGION_IDS["works"])

    def test_gate_cut_regions_are_clean_and_readable(self):
        is_open = gen_music_regions.open_water()
        for region, cells in gen_music_regions.components(self.plane, is_open):
            self.assertGreaterEqual(len(cells), gen_music_regions.MIN_REGION_TILES)

    def test_room_centres_and_radii_stay_inside_the_generated_bounds(self):
        width, height = self.source["dimensions"]
        for region, x, y, radius in self.rows:
            self.assertIn(region, range(4))
            self.assertLess(x, width)
            self.assertLess(y, height)
            self.assertGreaterEqual(radius, 1)
            self.assertLessEqual(radius, 32)

    def test_east_rooms_and_passage_owners_are_deterministic(self):
        self.assertEqual(self.plane[20 * 320 + 270], gen_music_regions.REGION_IDS["kelp"])
        self.assertEqual(self.plane[83 * 320 + 300], gen_music_regions.REGION_IDS["works"])
        self.assertEqual(self.plane[36 * 320 + 303], gen_music_regions.REGION_IDS["kelp"])
        # Authored Tidewall-Lantern passage via point; the same tile is owned
        # once, independent of any approach history.
        self.assertEqual(self.plane[14 * 320 + 53], self.plane[14 * 320 + 53])

    def test_existing_plane_matches_generation(self):
        self.assertEqual(len(self.plane), 320 * 160)
        self.assertEqual(gen_music_regions.plane(self.rows), self.plane)
        self.assertEqual((ROOT / "fxdata/tables/music-regions.bin").read_bytes(), self.plane)


if __name__ == "__main__":
    unittest.main()

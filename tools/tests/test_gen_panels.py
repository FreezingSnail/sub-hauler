import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_panels


class StoryPanelGenerationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.strings = gen_panels.load_strings(ROOT / "fxdata/fxdata.txt")
        cls.routes = gen_panels.load_routes(ROOT / "src/campaign.hpp")
        cls.rewards = gen_panels.load_rewards(ROOT / "src/generated/level.hpp")
        cls.panels = gen_panels.panels(cls.strings)
        cls.work = gen_panels.work_panels(cls.strings, cls.routes, cls.rewards)
        cls.titles = gen_panels.title_panels(cls.strings)
        cls.campaign = gen_panels.campaign_panels()

    def test_campaign_routes_match_the_frozen_mission_order(self):
        self.assertEqual(
            self.routes,
            [
                ("kTidewall", "kBenthos"),
                ("kCormorant", "kBenthos"),
                ("kBenthos", "kCormorant"),
                ("kCormorant", "kDeepAnchor"),
                ("kDeepAnchor", "kTidewall"),
            ],
        )
        earned = []
        for source, target in self.routes:
            index = list(gen_panels.BERTH_NAMES).index(source) * gen_panels.SETTLEMENT_COUNT
            index += list(gen_panels.BERTH_NAMES).index(target)
            earned.append(self.rewards[index])
        self.assertEqual(earned, [145, 85, 85, 155, 120])
        self.assertEqual(sum(earned), 590)

    def test_every_panel_name_is_unique_and_every_row_fits(self):
        names = [name for name, _image in self.panels + self.work + self.titles]
        self.assertEqual(len(names), len(set(names)))
        for name, image in self.panels:
            self.assertEqual(image.size, (128, 64), name)
        for name, image in self.campaign:
            self.assertEqual(image.mode, "RGBA", name)
            self.assertGreater(image.getbbox()[2], 0, name)

    def test_work_sheets_use_frozen_frame_counts(self):
        sheets = dict(self.work)
        self.assertEqual(sheets["work_details_128x64"].size, (128 * 5, 64))
        self.assertEqual(sheets["work_active_128x44"].size, (128 * 5, 44))
        self.assertEqual(sheets["work_rows_128x8"].size, (128 * 12, 8))

    def test_title_sheets_cover_status_hull_and_selection(self):
        sheets = dict(self.titles)
        self.assertEqual(sheets["title_menu_128x64"].size, (128 * 45, 64))
        self.assertEqual(sheets["title_replace_128x64"].size, (128 * 5, 64))
        self.assertEqual(sheets["title_keep_128x64"].size, (128, 64))

    def test_restoration_sprites_are_masked_and_non_empty(self):
        for name, image in self.campaign:
            self.assertIn("A", image.getbands(), name)
            self.assertGreater(image.getbbox()[2], 0, name)


if __name__ == "__main__":
    unittest.main()

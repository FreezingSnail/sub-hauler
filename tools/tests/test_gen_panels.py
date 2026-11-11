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
        cls.panels = gen_panels.panels(cls.strings)
        cls.titles = gen_panels.title_panels(cls.strings)
        cls.campaign = gen_panels.campaign_panels()
        cls.story = gen_panels.story_panels(cls.strings)
        cls.preview = gen_panels.story_preview(cls.strings)

    def test_every_panel_name_is_unique_and_every_row_fits(self):
        names = [name for name, _image in self.panels + self.titles]
        self.assertEqual(len(names), len(set(names)))
        for name, image in self.panels:
            self.assertEqual(image.size, (128, 64), name)
        for name, image in self.campaign:
            self.assertEqual(image.mode, "RGBA", name)
            self.assertGreater(image.getbbox()[2], 0, name)

    def test_retired_panels_are_not_emitted(self):
        names = {name for name, _image in self.panels}
        for retired in (
            "panel_ending_stats_128x64",
            "panel_pause_resume_128x64",
            "panel_pause_rescue_128x64",
            "title_keep_128x64",
            "work_details_128x64",
            "work_active_128x44",
            "work_rows_128x8",
        ):
            self.assertNotIn(retired, names)

    def test_title_sheets_cover_boot_state_and_selection(self):
        sheets = dict(self.titles)
        self.assertEqual(sheets["title_menu_128x64"].size, (128 * 8, 64))
        self.assertEqual(sheets["title_replace_128x64"].size, (128, 64))

    def test_restoration_sprites_are_masked_and_non_empty(self):
        for name, image in self.campaign:
            self.assertIn("A", image.getbands(), name)
            self.assertGreater(image.getbbox()[2], 0, name)

    def test_story_cards_are_fixed_size_and_match_the_authored_beats(self):
        self.assertEqual([name for name, _image in self.story],
                         ["story_first_128x64", "story_pump_128x64", "story_lift_128x64", "story_beacon_128x64"])
        expected = [
            ("BENTHOS", ("A HAND AT THE GLASS.",)),
            ("PUMP REFUGE", ("THE CREWS COME BACK.",)),
            ("CORMORANT", ("CRATES CLIMB AGAIN.",)),
            ("DEEP ANCHOR", ("A LIGHT ANSWERS.",)),
        ]
        self.assertEqual([(title, lines) for _motif, title, lines in gen_panels.STORY_CARDS], expected)
        for name, image in self.story:
            self.assertEqual(image.size, (128, 64), name)
            self.assertEqual(image.mode, "RGBA", name)
            self.assertGreater(image.getbbox()[2], 0, name)
        for _motif, title, lines in gen_panels.STORY_CARDS:
            self.assertLessEqual(len(title), 20)
            for line in lines:
                self.assertLessEqual(len(line), 20, line)

    def test_story_scene_plates_are_opaque_monochrome_and_substantial(self):
        cards = self.story + [("home", gen_panels.ending_thanks(self.strings))]
        plates = []
        for name, image in cards:
            self.assertEqual(set(image.getdata()), {gen_panels.BG_COLOR, gen_panels.TEXT_COLOR}, name)
            plate = image.crop((0, 11, 128, 44))
            self.assertGreater(sum(pixel == gen_panels.TEXT_COLOR for pixel in plate.getdata()), 240, name)
            plates.append(plate.tobytes())
        self.assertEqual(len(set(plates)), 5)

    def test_story_illustrations_leave_title_caption_and_footer_clear(self):
        for kind in [row[0] for row in gen_panels.STORY_CARDS] + ["home"]:
            image = gen_panels.Image.new("RGBA", (128, 64), gen_panels.BG_COLOR)
            gen_panels.story_motif(image, kind)
            bounds = image.convert("L").getbbox()
            self.assertIsNotNone(bounds, kind)
            self.assertGreaterEqual(bounds[1], 11, kind)
            self.assertLessEqual(bounds[3], 44, kind)

    def test_story_preview_has_all_five_beats(self):
        self.assertEqual(self.preview.size, (128 * 5, 64))
        thanks = gen_panels.ending_thanks(self.strings)
        self.assertEqual(thanks.size, (128, 64))
        self.assertEqual(self.preview.crop((128 * 4, 0, 128 * 5, 64)).tobytes(), thanks.tobytes())


if __name__ == "__main__":
    unittest.main()

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
        cls.berth_icons = gen_panels.berth_icon_sheet()

    def test_every_panel_name_is_unique_and_every_row_fits(self):
        names = [name for name, _image in self.panels + self.titles + [self.berth_icons]]
        self.assertEqual(len(names), len(set(names)))
        for name, image in self.panels:
            self.assertEqual(image.size, (128, 64), name)
        for name, image in self.campaign:
            self.assertEqual(image.mode, "RGBA", name)
            self.assertGreater(image.getbbox()[2], 0, name)

    def test_berth_icons_are_one_opaque_16x8_sheet(self):
        name, image = self.berth_icons
        self.assertEqual(name, "berth_icons_16x8")
        self.assertEqual(image.size, (16 * 8, 8))
        self.assertEqual(image.mode, "RGBA")
        self.assertEqual(set(image.getdata()), {gen_panels.BG_COLOR, gen_panels.TEXT_COLOR})
        for index in range(8):
            crop = image.crop((index * 16, 0, (index + 1) * 16, 8))
            self.assertGreater(crop.getbbox()[2], 0, index)

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

    def test_title_console_preserves_selection_and_all_save_states(self):
        frames = [[gen_panels.title_menu_frame(self.strings, status, selection)
                   for selection in range(2)] for status in range(4)]
        for first, second in frames:
            # Selection changes only the menu rows, leaving the instruments,
            # save state, title and controls unchanged.
            self.assertNotEqual(first.crop((4, 16, 84, 36)).tobytes(),
                                second.crop((4, 16, 84, 36)).tobytes())
            for region in ((0, 0, 128, 16), (88, 16, 127, 42), (0, 42, 128, 64)):
                self.assertEqual(first.crop(region).tobytes(), second.crop(region).tobytes())
        self.assertEqual(len({pair[0].crop((1, 43, 127, 54)).tobytes()
                              for pair in frames}), 4)

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

    def test_ending_uses_homecoming_art_and_keeps_station_names_apart(self):
        import json
        source = json.loads((ROOT / "story/scenes.json").read_text(encoding="utf-8"))
        row = source["art"]["rows"].index("home-lights")
        with gen_panels.Image.open(ROOT / "story" / source["art"]["file"]) as sheet:
            art = sheet.crop((0, row * 64 + 10, 128, row * 64 + 46)).convert("RGBA")
        self.assertEqual(gen_panels.ending_thanks(self.strings).crop((0, 10, 128, 46)).tobytes(), art.tobytes())
        network = gen_panels.ending_network(self.strings)
        self.assertEqual(set(network.crop((58, 30, 62, 38)).getdata()), {gen_panels.BG_COLOR})


if __name__ == "__main__":
    unittest.main()

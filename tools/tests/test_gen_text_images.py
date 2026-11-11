import re
import sys
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_text_images as gen
from dock_bark_table import DOCK_BARK_TEXT


class TextStampGenerationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.strings = gen.load_strings(gen.FXDATA)
        cls.campaign = gen.CAMPAIGN.read_text(encoding="utf-8")
        cls.rows, cls.layout = gen.build()

    def image(self, name):
        return Image.open(ROOT / "images/text" / (name + "_128x8.png")).convert("RGBA")

    def test_committed_outputs_match_the_generator(self):
        self.assertEqual(gen.main(["--check"]), 0)

    def test_one_two_frame_stamp_per_fxdata_string(self):
        names = dict(self.rows)
        for symbol in gen.ordered_strings(gen.FXDATA):
            image = names["img_" + symbol]
            self.assertEqual(image.size, (256, 8), symbol)
            frame0 = image.crop((0, 0, 128, 8))
            frame1 = image.crop((128, 0, 256, 8))
            reference = gen.stamp_image(self.strings[symbol], gen.TEXT_INSET_X)
            inverted = gen.stamp_image(self.strings[symbol], gen.TEXT_INSET_X, invert=True)
            self.assertEqual(frame0.tobytes(), reference.tobytes(), symbol)
            self.assertEqual(frame1.tobytes(), inverted.tobytes(), symbol)

    def test_text_inset_and_transparent_background(self):
        for symbol in gen.ordered_strings(gen.FXDATA):
            image = self.image("img_" + symbol)
            pixels = image.load()
            for y in range(8):
                for x in range(128):
                    if x < gen.TEXT_INSET_X:
                        self.assertEqual(pixels[x, y][3], 0, symbol)
        air = self.image("img_txt_air")
        # 'A' starts at the inset; the first cell's five columns are opaque.
        ink = [air.getpixel((x, y)) for y in range(8) for x in range(gen.TEXT_INSET_X, gen.TEXT_INSET_X + 5)]
        self.assertTrue(any(pixel[3] == 255 for pixel in ink))

    def test_berth_names_match_campaign_order_and_fallback(self):
        image = self.image("img_berth_names")
        frame = 0
        for name in gen.parse_enum(self.campaign, "Berth", gen.CAMPAIGN):
            symbol = "txt_" + gen.camel_to_snake(name)
            self.assertEqual(
                image.crop((frame * 128, 0, (frame + 1) * 128, 8)).tobytes(),
                gen.stamp_image(self.strings[symbol], 0).tobytes(),
                symbol,
            )
            frame += 1
        self.assertEqual(
            image.crop((8 * 128, 0, 9 * 128, 8)).tobytes(),
            gen.stamp_image(self.strings["txt_station"], 0).tobytes(),
        )

    def test_dock_barks_share_the_bark_table(self):
        image = self.image("img_dock_barks")
        self.assertEqual(image.width // 128, len(DOCK_BARK_TEXT))
        for index, (_name, symbol) in enumerate(DOCK_BARK_TEXT):
            text = "" if symbol is None else self.strings[symbol]
            self.assertEqual(
                image.crop((index * 128, 0, (index + 1) * 128, 8)).tobytes(),
                gen.stamp_image(text, 0).tobytes(),
                symbol,
            )

    def test_row_story_carry_and_work_composites(self):
        labels = self.image("img_row_labels")
        for index, text in enumerate(("WORK", "UNDOCK")):
            self.assertEqual(
                labels.crop((index * 128, 0, (index + 1) * 128, 8)).tobytes(),
                gen.stamp_image(text, 0).tobytes(),
                text,
            )
            self.assertEqual(
                labels.crop(((index + 2) * 128, 0, (index + 3) * 128, 8)).tobytes(),
                gen.stamp_image(text, 0, invert=True).tobytes(),
                text,
            )
        missions = gen.mission_texts(self.strings)
        story = self.image("img_story_row")
        carry = self.image("img_carry_row")
        for index, text in enumerate(missions):
            self.assertEqual(
                story.crop((index * 128, 0, (index + 1) * 128, 8)).tobytes(),
                gen.stamp_image("STORY " + text, 0).tobytes(),
            )
            self.assertEqual(
                story.crop(((index + len(missions)) * 128, 0, (index + len(missions) + 1) * 128, 8)).tobytes(),
                gen.stamp_image("STORY " + text, 0, invert=True).tobytes(),
            )
            self.assertEqual(
                carry.crop((index * 128, 0, (index + 1) * 128, 8)).tobytes(),
                gen.stamp_image("CARRY " + text, 0).tobytes(),
            )
        berths = [self.strings[symbol] for symbol in gen.berth_symbols(self.campaign, self.strings)]
        work = self.image("img_berth_work")
        for index, text in enumerate(berths):
            self.assertEqual(
                work.crop((index * 128, 0, (index + 1) * 128, 8)).tobytes(),
                gen.stamp_image(text + " WORK", 0).tobytes(),
            )

    def test_layout_constants_track_the_generated_frames(self):
        counts = {name: image.width // 128 for name, image in self.rows}
        self.assertIn("kBerthNameFrames = {}".format(counts["img_berth_names"]), self.layout)
        self.assertIn("kDockBarkFrames = {}".format(counts["img_dock_barks"]), self.layout)
        self.assertIn("kRowLabelFrames = {}".format(counts["img_row_labels"]), self.layout)
        self.assertIn("kStoryRowFrames = {}".format(counts["img_story_row"]), self.layout)
        self.assertIn("kMissionFrames = {}".format(len(gen.MISSION_STRINGS)), self.layout)
        self.assertEqual(counts["img_carry_row"], counts["img_berth_work"] + 1)

    def test_mission_labels_match_the_work_view_dispatch(self):
        source = (ROOT / "src/work_view.hpp").read_text(encoding="utf-8")
        block = re.search(r"inline uint8_t missionFrame\(uint8_t mission\) \{(.*?)\n\}", source, re.S).group(1)
        self.assertIn("campaign::kWorkEnding", block)
        self.assertIn("text_layout::kMissionFrames - 1", block)
        self.assertEqual(gen.MISSION_STRINGS[-1], "txt_view_ending")


if __name__ == "__main__":
    unittest.main()

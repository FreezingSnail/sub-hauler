import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_story_beats


class StoryBeatGenerationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.html = (ROOT / "docs/story-sketchbook.html").read_text(encoding="utf-8")
        cls.glyphs, cls.decks = gen_story_beats.parse_story(cls.html)
        cls.blob = gen_story_beats.build_blob(cls.decks, cls.glyphs, ROOT / "docs/assets/first-beat-animation/all-beats-v3")
        cls.frames = sum(4 if card["kind"] == "image" else 1 for deck in cls.decks for card in deck)
        cls.offsets = [int.from_bytes(cls.blob[index:index + 2], "little") for index in range(0, cls.frames * 2, 2)]

    def frame(self, index):
        start = self.offsets[index]
        end = self.offsets[index + 1] if index + 1 < self.frames else len(self.blob) - gen_story_beats.RLE_PAD
        return self.blob[start:end]

    def test_seven_decks_alternate_image_and_text(self):
        self.assertEqual(len(self.decks), 7)
        self.assertEqual([len(deck) for deck in self.decks], [6, 4, 4, 4, 4, 4, 4])
        for deck in self.decks:
            for index, card in enumerate(deck):
                self.assertEqual(card["kind"], "image" if index % 2 == 0 else "text")
        self.assertEqual(sum(1 for deck in self.decks for card in deck if card["kind"] == "image"), 15)
        self.assertEqual(sum(1 for deck in self.decks for card in deck if card["kind"] == "text"), 15)

    def test_native_pose_files_cover_every_scene(self):
        arts = [card["art"] for deck in self.decks for card in deck if card["kind"] == "image"]
        self.assertEqual(len(arts), 15)
        self.assertEqual(len(set(arts)), 15)
        for art in arts:
            for phase in range(4):
                path = ROOT / "docs/assets/first-beat-animation/all-beats-v3/{}-{}.png".format(art, phase)
                self.assertTrue(path.is_file(), str(path))

    def test_blob_offsets_and_frame_count_match_the_generated_header(self):
        self.assertEqual(self.frames, 75)
        self.assertEqual(self.offsets[0], self.frames * 2)
        self.assertEqual(self.offsets, sorted(self.offsets))
        self.assertLess(self.offsets[-1], len(self.blob) - gen_story_beats.RLE_PAD)

    def test_rle_round_trips_every_frame_and_padding_covers_overread(self):
        for index in range(self.frames):
            decoded = gen_story_beats.rle_decode(self.frame(index))
            self.assertEqual(len(decoded), gen_story_beats.FRAME_BYTES, "frame {}".format(index))
        self.assertGreaterEqual(len(self.blob) - self.offsets[-1], gen_story_beats.RLE_PAD)

    def test_text_cards_use_the_preview_layout(self):
        rows = gen_story_beats.text_lines(self.decks[0][1]["copy"])
        self.assertEqual(rows, ["FILTER SUPPLIES", "FROM TIDEWALL.", "YOU DOCK AT BENTHOS."])
        decoded = gen_story_beats.rle_decode(self.frame(1))
        self.assertEqual(len(decoded), gen_story_beats.FRAME_BYTES)
        self.assertGreater(sum(byte.bit_count() for byte in decoded), 60)

    def test_committed_blob_and_header_are_current(self):
        self.assertEqual((ROOT / "fxdata/tables/story-beats.bin").read_bytes(), self.blob)
        header = (ROOT / "src/generated/story_beats.hpp").read_text(encoding="utf-8")
        self.assertIn("STORY_BEAT_CARDS  = 30", header)
        self.assertIn("STORY_BEAT_IMAGES = 15", header)
        self.assertIn("STORY_BEAT_FRAMES = 75", header)


if __name__ == "__main__":
    unittest.main()

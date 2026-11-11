import json
import copy
import base64
import io
import re
import sys
import unittest
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_story_beats as gen


class StoryBeatGenerationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source_text = gen.SOURCE.read_text(encoding="utf-8")
        cls.document = gen.parse_source(cls.source_text, gen.SOURCE)
        cls.scenes = cls.document["scenes"]
        cls.sheet = gen.load_sheet(cls.document)
        cls.layout = gen.build_layout(cls.document, gen.SOURCE)
        cls.blob = cls.layout["blob"]
        cls.frames = cls.layout["frames"]
        cls.offsets = [int.from_bytes(cls.blob[index:index + 3], "little") for index in range(0, cls.frames * 3, 3)]

    def frame(self, index):
        start = self.offsets[index]
        end = self.offsets[index + 1] if index + 1 < self.frames else len(self.blob) - gen.RLE_PAD
        return self.blob[start:end]

    def test_narrative_order_is_independent_of_stable_scene_ids(self):
        self.assertEqual(set(scene["name"] for scene in self.scenes), set(name for name, _ in gen.SCENE_ORDER))
        self.assertEqual(self.scenes[0]["name"], "opening-benthos")
        self.assertEqual(self.scenes[1]["name"], "first-departure")
        self.assertEqual(self.scenes[-1]["name"], "home-result")
        image_order = [card["art"] for scene in self.scenes for card in scene["cards"] if card["kind"] == "image"]
        self.assertEqual(self.document["art"]["rows"], image_order)
        self.assertEqual(len(image_order), len(set(image_order)))
        reordered = dict(self.document, scenes=list(reversed(self.scenes)))
        self.assertEqual(gen.build_layout(reordered)["blob"], self.blob)
        self.assertEqual(len(self.scenes), 17)
        self.assertEqual(self.layout["image_cards"], 21)
        self.assertEqual(self.layout["text_cards"], 22)
        self.assertEqual(self.layout["images"], 21)
        self.assertEqual(self.frames, 106)

    def test_card_kinds_come_from_source_not_alternation(self):
        for scene in self.layout["preview_scenes"]:
            self.assertTrue(scene["cards"])
            self.assertEqual(scene["cards"][0]["kind"], "image")
            self.assertTrue(any(card["kind"] == "text" for card in scene["cards"]))
        kinds = [card["kind"] for scene in self.layout["preview_scenes"] for card in scene["cards"]]
        self.assertEqual(kinds.count("image"), self.layout["image_cards"])
        self.assertEqual(kinds.count("text"), self.layout["text_cards"])
        # The source has long handoffs that must paginate into adjacent text pages.
        for scene in self.layout["preview_scenes"]:
            for left, right in zip(scene["cards"], scene["cards"][1:]):
                if left["kind"] == right["kind"] == "text":
                    break
            else:
                continue
            break
        else:
            self.fail("expected at least one pair of consecutive text pages")

    def test_every_text_card_fits_and_preserves_all_words(self):
        source_words = []
        for scene in sorted(self.scenes, key=lambda scene: gen.SCENE_IDS[scene["name"]]):
            for card in scene["cards"]:
                if card["kind"] != "text":
                    continue
                for paragraph in card["paragraphs"]:
                    source_words.extend(gen.normalize_prose(paragraph, gen.SOURCE, scene["name"]).split())

        packed_words = []
        for scene in self.layout["preview_scenes"]:
            for card in scene["cards"]:
                if card["kind"] != "text":
                    continue
                self.assertTrue(1 <= len(card["rows"]) <= gen.ROWS_PER_CARD)
                for row in card["rows"]:
                    self.assertLessEqual(len(row), gen.COLUMNS)
                    self.assertTrue(set(row) <= gen.SUPPORTED)
                packed_words.extend(" ".join(card["rows"]).split())
        self.assertEqual(packed_words, source_words)

    def test_consecutive_text_pages_keep_paragraph_order(self):
        first = next(scene for scene in self.scenes if scene["name"] == "first-result")
        paragraphs = [paragraph for card in first["cards"] if card["kind"] == "text" for paragraph in card["paragraphs"]]
        pages = gen.paginate(paragraphs, gen.SOURCE, 1)
        self.assertGreater(len(pages), 1)
        self.assertEqual(" ".join(" ".join(page) for page in pages).split(),
                         " ".join(gen.normalize_prose(paragraph, gen.SOURCE, 1) for paragraph in paragraphs).split())

    def test_long_prose_paginates_without_dropping_words(self):
        paragraphs = ["BENTHOS NEEDS FILTER SUPPLIES FOR ITS FOOD TANKS AND DEEP ANCHOR HAS GONE QUIET " * 6]
        pages = gen.paginate(paragraphs, gen.SOURCE, 1)
        self.assertGreater(len(pages), 1)
        self.assertEqual(" ".join(" ".join(page) for page in pages).split(),
                         gen.normalize_prose(paragraphs[0], gen.SOURCE, 1).split())
        for page in pages:
            self.assertTrue(1 <= len(page) <= gen.ROWS_PER_CARD)
            for row in page:
                self.assertLessEqual(len(row), gen.COLUMNS)

    def test_paragraph_boundary_is_preserved_between_pages(self):
        pages = gen.paginate(["FIRST PARAGRAPH ENDS HERE.", "SECOND PARAGRAPH STARTS HERE."], gen.SOURCE, 1)
        self.assertEqual(" ".join(" ".join(page) for page in pages).split(),
                         "FIRST PARAGRAPH ENDS HERE. SECOND PARAGRAPH STARTS HERE.".split())
        self.assertTrue(any("" in page for page in pages), "paragraph break is retained as a blank row")

    def test_blank_and_unsupported_input_fail_clearly(self):
        with self.assertRaisesRegex(gen.StoryError, "empty paragraph"):
            gen.paginate([""], gen.SOURCE, 7)
        with self.assertRaisesRegex(gen.StoryError, "unsupported character"):
            gen.paginate(["BENTHOS; BENTHOS"], gen.SOURCE, 7)

    def test_generation_fails_before_output_when_card_limits_are_exceeded(self):
        too_many_cards = {"name": "synthetic", "title": "Synthetic", "line": 1,
                          "cards": [{"kind": "text", "paragraphs": ["A"], "line": 1} for _ in range(256)]}
        with self.assertRaisesRegex(gen.StoryError, "too many cards"):
            gen.build_layout(dict(self.document, scenes=[too_many_cards]), gen.SOURCE)

        too_many_frames = {"name": "synthetic", "title": "Synthetic", "line": 1,
                           "cards": [{"kind": "image", "art": "arrival", "line": 1}]
                                    + [{"kind": "text", "paragraphs": ["A"], "line": 1} for _ in range(254)]}
        with self.assertRaisesRegex(gen.StoryError, "frame count"):
            gen.build_layout(dict(self.document, scenes=[too_many_frames]), gen.SOURCE)

    def test_every_source_paragraph_survives_pagination(self):
        preview = {scene["id"]: scene for scene in self.layout["preview_scenes"]}
        for scene in self.scenes:
            pages = [card["copy"] for card in preview[scene["name"]]["cards"]
                     if card["kind"] == "text"]
            combined = " ".join(" ".join(copy.split()) for copy in pages)
            for card in scene["cards"]:
                if card["kind"] != "text":
                    continue
                for paragraph in card["paragraphs"]:
                    expected = " ".join(gen.normalize_prose(paragraph, gen.SOURCE, scene["name"]).split())
                    self.assertIn(expected, combined)

    def test_canonical_sheet_covers_every_referenced_image(self):
        rows = self.document["art"]["rows"]
        self.assertEqual(self.sheet.size, (gen.WIDTH * 4, gen.HEIGHT * len(rows)))
        for scene in self.scenes:
            for card in scene["cards"]:
                if card["kind"] == "image":
                    self.assertIn(card["art"], rows)

    def test_piskel_rgb_and_rgba_exports_preserve_packed_pixels(self):
        for mode in ("RGB", "RGBA", "P"):
            exported = self.sheet.convert(mode)
            layout = gen.build_layout(self.document, sheet_image=exported)
            self.assertEqual(layout["blob"], self.blob, mode)

    def test_editing_a_sheet_pixel_changes_the_corresponding_packed_pose(self):
        sheet = self.sheet.copy()
        y = self.document["art"]["rows"].index("arrival") * gen.HEIGHT
        sheet.putpixel((0, y), 0 if sheet.getpixel((0, y)) else 255)
        layout = gen.build_layout(self.document, sheet_image=sheet)
        # Packed frame zero remains arrival, regardless of the editing-sheet row.
        first = int.from_bytes(layout["blob"][:3], "little")
        end = int.from_bytes(layout["blob"][3:6], "little")
        before = gen.rle_decode(self.frame(0))
        after = gen.rle_decode(layout["blob"][first:end])
        self.assertEqual(after[0], before[0] ^ 1)
        self.assertEqual(after[1:], before[1:])
        self.assertNotEqual(layout["blob"], self.blob)

    def test_sheet_rejects_wrong_dimensions_color_and_transparency(self):
        bad = self.sheet.convert("RGBA")
        bad.putpixel((0, 0), (127, 127, 127, 255))
        transparent = self.sheet.convert("RGBA")
        transparent.putpixel((0, 0), (0, 0, 0, 0))
        for image in (bad, transparent, Image.new("1", (128, 64))):
            with self.assertRaises(gen.StoryError):
                gen.build_layout(self.document, sheet_image=image)

    def test_json_rejects_invalid_schema_and_missing_image_rows(self):
        bad_documents = []
        for field, value in (("version", 2), ("scenes", [])):
            document = copy.deepcopy(self.document)
            document[field] = value
            bad_documents.append(document)
        document = copy.deepcopy(self.document)
        document["art"]["rows"] = document["art"]["rows"][1:]
        bad_documents.append(document)
        document = copy.deepcopy(self.document)
        document["scenes"][0]["cards"][1]["paragraphs"] = []
        bad_documents.append(document)
        document = copy.deepcopy(self.document)
        document["art"]["rows"].append(document["art"]["rows"][0])
        bad_documents.append(document)
        document = copy.deepcopy(self.document)
        document["scenes"][0]["cards"][0]["typo"] = True
        bad_documents.append(document)
        for document in bad_documents:
            with self.assertRaises(gen.StoryError):
                gen.parse_source(json.dumps(document))
        with self.assertRaisesRegex(gen.StoryError, "duplicate JSON key"):
            gen.parse_source('{"version":1,"version":1}')
        with self.assertRaises(gen.StoryError):
            gen.parse_source('{')

    def test_blob_offsets_and_frame_count_match_the_generated_header(self):
        self.assertEqual(self.offsets[0], self.frames * 3)
        self.assertEqual(self.offsets, sorted(self.offsets))
        self.assertLess(self.offsets[-1], len(self.blob) - gen.RLE_PAD)
        self.assertEqual((ROOT / "fxdata/tables/story-beats.bin").read_bytes(), self.blob)

    def test_rle_round_trips_every_frame_and_padding_covers_overread(self):
        for index in range(self.frames):
            decoded = gen.rle_decode(self.frame(index))
            self.assertEqual(len(decoded), gen.FRAME_BYTES, "frame {}".format(index))
        self.assertGreaterEqual(len(self.blob) - self.offsets[-1], gen.RLE_PAD)

    def test_packed_images_match_the_native_animation_frames(self):
        card_frames = self.layout["card_frames"]
        position = 0
        for scene in self.layout["preview_scenes"]:
            for card in scene["cards"]:
                frame = card_frames[position]
                if card["kind"] == "image":
                    for phase in range(4):
                        native = gen.art_pose(self.sheet, self.document["art"]["rows"], card["art"], phase)
                        decoded = gen.rle_decode(self.frame(frame + phase))
                        for y in range(gen.HEIGHT):
                            for x in range(gen.WIDTH):
                                bit = (decoded[(y // 8) * gen.WIDTH + x] >> (y % 8)) & 1
                                self.assertEqual(bit, bool(native.getpixel((x, y))))
                position += 1

    def test_preview_data_matches_packed_card_order(self):
        text = gen.PREVIEW.read_text(encoding="utf-8")
        match = re.search(r"globalThis\.StoryData=(\{.*\});", text, re.S)
        self.assertIsNotNone(match)
        data = json.loads(match.group(1))
        self.assertEqual(len(data["scenes"]), 17)
        self.assertEqual(data["columns"], gen.COLUMNS)
        self.assertEqual(data["rows"], gen.ROWS_PER_CARD)
        self.assertEqual(
            [card["title"] for scene in data["scenes"] for card in scene["cards"]],
            [card["title"] for scene in self.layout["preview_scenes"] for card in scene["cards"]],
        )
        for scene, source in zip(data["scenes"], self.layout["preview_scenes"]):
            self.assertEqual(scene["id"], source["id"])
            self.assertEqual([card["kind"] for card in scene["cards"]], [card["kind"] for card in source["cards"]])
            for card, source_card in zip(scene["cards"], source["cards"]):
                if card["kind"] == "text":
                    self.assertEqual(card["rows"], source_card["rows"])

    def test_preview_image_pixels_come_from_the_canonical_sheet(self):
        text = gen.PREVIEW.read_text(encoding="utf-8")
        animation = json.loads(re.search(r"globalThis\.StoryAnimation=(\{.*\});", text).group(1))
        for art, poses in animation["scenes"].items():
            for phase, url in enumerate(poses):
                self.assertTrue(url.startswith("data:image/png;base64,"))
                image = Image.open(io.BytesIO(base64.b64decode(url.split(",", 1)[1])))
                expected = gen.art_pose(self.sheet, self.document["art"]["rows"], art, phase)
                self.assertIsNone(ImageChops.difference(image, expected).getbbox())

    def test_complete_card_sheet_matches_each_packed_frame(self):
        sheet = Image.open(gen.CARD_SHEET)
        mapping = json.loads(gen.CARD_MAP.read_text())
        self.assertEqual(sheet.size, (gen.WIDTH * 4, gen.HEIGHT * len(self.layout["card_frames"])))
        self.assertEqual(mapping["cards"][0]["scene"], "opening-benthos")
        self.assertEqual(mapping["cards"][-1]["scene"], "home-result")
        self.assertEqual(sorted(e["packed_card"] for e in mapping["cards"]),
                         list(range(len(self.layout["card_frames"]))))
        for entry in mapping["cards"]:
            row = entry["row"]
            first = self.layout["card_frames"][entry["packed_card"]]
            for phase in range(entry["poses"]):
                crop = sheet.crop((phase * gen.WIDTH, row * gen.HEIGHT,
                                   (phase + 1) * gen.WIDTH, (row + 1) * gen.HEIGHT))
                self.assertEqual(gen.image_frame(crop), gen.rle_decode(self.frame(first + phase)))

    def test_committed_blob_and_header_are_current(self):
        header = (ROOT / "src/generated/story_beats.hpp").read_text(encoding="utf-8")
        self.assertIn("STORY_CARD_COUNT = 43", header)
        self.assertIn("STORY_IMAGE_CARD_COUNT = 21", header)
        self.assertIn("STORY_TEXT_CARD_COUNT = 22", header)
        self.assertIn("STORY_IMAGE_COUNT = 21", header)
        self.assertIn("STORY_FRAME_COUNT = 106", header)
        self.assertIn("STORY_SCENE_FIRST_RESULT = 1", header)
        self.assertIn("STORY_SCENE_HOME_DEPARTURE = 14", header)
        self.assertIn("STORY_SCENE_EXPEDITION_RESULT = 15", header)
        self.assertIn("STORY_SCENE_EXPEDITION_DEPARTURE = 16", header)
        self.assertIn("STORY_SCENE_OPENING = 17", header)
        self.assertIn("STORY_META_SCENE_CARDS = 0", header)
        self.assertIn("STORY_META_SCENE_FIRST = STORY_META_SCENE_CARDS + STORY_SCENE_COUNT", header)
        self.assertIn("STORY_META_BYTES = STORY_META_CARD_KIND + STORY_CARD_COUNT", header)
        self.assertNotIn("PROGMEM", header)

    def test_cold_metadata_matches_the_packed_layout(self):
        meta = gen.build_metadata(self.layout)
        self.assertEqual(len(meta), 122)
        self.assertEqual(meta, gen.META_OUTPUT.read_bytes())
        self.assertEqual(meta[:18], bytes(self.layout["scene_cards"]))
        self.assertEqual(meta[18:36], bytes(self.layout["scene_first"]))
        self.assertEqual(meta[36:79], bytes(self.layout["card_frames"]))
        self.assertEqual(meta[79:], bytes(self.layout["card_kinds"]))


if __name__ == "__main__":
    unittest.main()

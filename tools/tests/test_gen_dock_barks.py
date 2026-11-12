import json
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_dock_barks as gen
import gen_text_images as text_images


class DockHintPreviewTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = gen.build_document()
        cls.hints = json.loads((ROOT / "story/dock_hints.json").read_text(encoding="utf-8"))
        cls.points = [point["id"] for point in cls.hints["points"]]
        cls.berths = [berth["id"] for berth in cls.document["berths"]]

    def bark(self, point, berth):
        return self.document["berths"][self.berths.index(berth)]["barks"][self.points.index(point)]

    def test_committed_preview_matches_the_generator(self):
        self.assertEqual(gen.OUTPUT.read_text(encoding="utf-8"), gen.render(self.document))
        self.assertEqual(gen.main(["--check"]), 0)

    def test_every_berth_and_point_has_one_line(self):
        self.assertEqual(len(self.berths), 8)
        self.assertEqual(len(self.points), 14)
        for berth in self.document["berths"]:
            self.assertEqual(len(berth["barks"]), len(self.points), berth["id"])
            for row in berth["barks"]:
                self.assertTrue(row["text"], berth["id"])
                self.assertTrue(row["symbol"].startswith("img_dock_barks["), row["symbol"])
                self.assertGreaterEqual(row["frame"], 1)
                self.assertLessEqual(row["frame"], len(self.berths) * len(self.points))

    def test_preview_text_is_the_json_not_a_copy(self):
        for berth_id, points in self.hints["berths"].items():
            enum_berth = self._enum_berth(berth_id)
            for point_id, text in points.items():
                self.assertEqual(self.bark(point_id, enum_berth)["text"], text, (berth_id, point_id))

    def _enum_berth(self, berth_id):
        for berth in self.document["berths"]:
            if text_images.enum_id(berth["id"], "") == berth_id:
                return berth["id"]
        raise AssertionError("missing berth {}".format(berth_id))

    def test_point_order_matches_dock_point_enum(self):
        campaign = gen.CAMPAIGN.read_text(encoding="utf-8")
        enum = re.search(r"enum\s+DockPoint\s*:[^{]*\{(.*?)\};", campaign, re.S).group(1)
        names = re.findall(r"\b(kDockPoint[A-Za-z0-9_]+)\b", enum)
        ids = [text_images.enum_id(name, "dock_point_") for name in names]
        self.assertEqual([state["id"] for state in self.document["states"]], ids)

    def test_berth_order_matches_berth_enum(self):
        campaign = gen.CAMPAIGN.read_text(encoding="utf-8")
        enum = re.search(r"enum\s+Berth\s*:[^{]*\{(.*?)\};", campaign, re.S).group(1)
        names = re.findall(r"\b(k[A-Za-z0-9_]+)\b", enum)
        self.assertEqual(self.berths, names)

    def test_every_berth_line_is_present(self):
        texts = [row["text"] for berth in self.document["berths"] for row in berth["barks"]]
        self.assertEqual(len(texts), len(self.berths) * len(self.points))
        self.assertTrue(all(texts))

    def test_native_layout_constants(self):
        self.assertEqual(self.document["font"]["cellWidth"], 6)
        self.assertEqual(self.document["font"]["cellHeight"], 8)
        self.assertEqual(self.document["layout"]["hintX"], 3)
        self.assertEqual(self.document["layout"]["barkY"], 56)
        for berth in self.document["berths"]:
            for row in berth["barks"]:
                self.assertLessEqual(len(row["text"]), self.document["layout"]["hintChars"])
                self.assertRegex(row["text"], r"^[A-Z0-9 :./'-]+$")

    def test_frames_are_berth_major_and_point_minor(self):
        for berth_index, berth in enumerate(self.document["berths"]):
            for point_index, row in enumerate(berth["barks"]):
                self.assertEqual(row["frame"], 1 + berth_index * len(self.points) + point_index)

    def test_generation_is_deterministic(self):
        self.assertEqual(gen.render(gen.build_document()), gen.render(gen.build_document()))


if __name__ == "__main__":
    unittest.main()

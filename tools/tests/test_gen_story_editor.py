import copy
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_story_editor as gen


class StoryEditorGenerationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.schema, cls.missions = gen.build_schema()

    def test_schema_covers_the_fixed_story_structure(self):
        self.assertEqual(len(self.schema["scenes"]), len(self.schema["sceneOrder"]))
        self.assertEqual(len(self.schema["berths"]), 8)
        self.assertEqual(len(self.schema["points"]), 9)
        self.assertEqual(self.schema["hintMaxChars"], 20)
        self.assertEqual(len(self.schema["missionIds"]), 8)
        self.assertEqual(len(self.missions["missions"]), 8)
        self.assertEqual(
            set(self.schema["missionIds"]),
            {mission["id"] for mission in self.missions["missions"]},
        )
        for scene in self.schema["scenes"]:
            self.assertTrue(scene["cards"])
            self.assertIn(scene["mission"], self.schema["missionIds"] + [""])

    def test_check_mode_matches_committed_outputs(self):
        self.assertEqual(gen.main(["--check"]), 0)

    def test_grouping_rejects_duplicate_scene_and_missing_scene(self):
        mission_doc = copy.deepcopy(self.missions)
        mission_doc["missions"][1]["scenes"].append(mission_doc["missions"][0]["scenes"][0])
        with self.assertRaisesRegex(gen.EditorError, "more than one mission"):
            gen.validate_missions(mission_doc, self.schema["sceneOrder"])

        mission_doc = copy.deepcopy(self.missions)
        mission_doc["unassigned"].pop()
        with self.assertRaisesRegex(gen.EditorError, "not grouped"):
            gen.validate_missions(mission_doc, self.schema["sceneOrder"])

    def test_grouping_rejects_empty_mission_copy(self):
        mission_doc = copy.deepcopy(self.missions)
        mission_doc["missions"][0]["summary"] = ""
        with self.assertRaisesRegex(gen.EditorError, "empty summary"):
            gen.validate_missions(mission_doc, self.schema["sceneOrder"])


if __name__ == "__main__":
    unittest.main()

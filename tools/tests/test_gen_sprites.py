import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import gen_sprites  # noqa: E402


class GenSpritesTest(unittest.TestCase):
    def test_known_pixels_pack_as_triplane_plus_mask(self):
        sprite = gen_sprites.pack_sheet(
            ROOT / "tools" / "tests" / "fixtures" / "tiny_2x2.png", 4
        )
        self.assertEqual(sprite.name, "tiny")
        self.assertEqual((sprite.width, sprite.height, sprite.frames), (2, 2, 1))
        self.assertTrue(sprite.masked)
        self.assertEqual(
            list(sprite.data),
            [2, 2, 2, 2, 3, 3, 2, 2, 2, 3, 0, 2, 2, 3],
        )

    def test_output_is_deterministic(self):
        output = ROOT / "build" / "tests" / "sprites-a.txt"
        second = ROOT / "build" / "tests" / "sprites-b.txt"
        gen_sprites.write_arrays(ROOT / "tools" / "tests" / "fixtures", output, 4)
        gen_sprites.write_arrays(ROOT / "tools" / "tests" / "fixtures", second, 4)
        self.assertEqual(output.read_bytes(), second.read_bytes())

    def test_rejects_a_sheet_without_dimensions(self):
        bad = ROOT / "build" / "tests" / "bad.png"
        bad.parent.mkdir(parents=True, exist_ok=True)
        bad.write_bytes((ROOT / "tools" / "tests" / "fixtures" / "tiny_2x2.png").read_bytes())
        with self.assertRaises(ValueError):
            gen_sprites.pack_sheet(bad, 4)


if __name__ == "__main__":
    unittest.main()

import sys
import json
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_splash
import gen_sprites


class SplashGenerationTest(unittest.TestCase):
    def test_splash_uses_canonical_arrival_art_between_text_bands(self):
        _, game = gen_splash.frames()
        source = json.loads((ROOT / "story/scenes.json").read_text(encoding="utf-8"))
        row = source["art"]["rows"].index("arrival")
        with Image.open(ROOT / "story" / source["art"]["file"]) as sheet:
            art = sheet.crop((0, row * 64 + 9, 128, row * 64 + 55)).convert("RGBA")
        self.assertEqual(game.crop((0, 9, 128, 55)).tobytes(), art.tobytes())

    def test_original_logo_pixels_are_centered_at_native_size(self):
        brand, _ = gen_splash.frames()
        source = Image.open(ROOT / "images/splash/source/snailware_64x64.png").convert("L")
        expected = source.point(lambda p: 255 if p >= 128 else 0)
        self.assertEqual(brand.crop((32, 0, 96, 64)).convert("L").tobytes(), expected.tobytes())
        self.assertEqual(brand.crop((0, 20, 32, 64)).convert("L").getextrema(), (0, 0))
        self.assertEqual(brand.crop((96, 20, 128, 64)).convert("L").getextrema(), (0, 0))

    def test_two_opaque_native_frames_match_the_generated_sheet(self):
        path = ROOT / "images/splash/startup_128x64.png"
        sheet = Image.open(path).convert("RGBA")
        for index, frame in enumerate(gen_splash.frames()):
            self.assertEqual(sheet.crop((index * 128, 0, (index + 1) * 128, 64)).tobytes(), frame.tobytes())
        self.assertEqual(set(sheet.getdata()), {(0, 0, 0, 255), (255, 255, 255, 255)})
        sprite = gen_sprites.pack_sheet(path, 2)
        self.assertEqual((sprite.width, sprite.height, sprite.frames, sprite.masked), (128, 64, 2, False))
        self.assertEqual(len(sprite.data), 2050)


if __name__ == "__main__":
    unittest.main()

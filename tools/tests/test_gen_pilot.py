import sys
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import gen_pilot  # noqa: E402

FIXTURES = ROOT / "build" / "tests" / "pilot"


def make_sheet(path, *, height=16, content_top=0, content_bottom=12, mirror=True, color=(255, 255, 255, 255)):
    """Write a four-frame synthetic pilot sheet."""
    width = gen_pilot.FRAME_WIDTH * gen_pilot.FRAMES
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    frames = []
    for frame in range(gen_pilot.FRAMES):
        cell = Image.new("RGBA", (gen_pilot.FRAME_WIDTH, height), (0, 0, 0, 0))
        for y in range(content_top, content_bottom):
            for x in range(gen_pilot.FRAME_WIDTH):
                cell.putpixel((x, y), color if (x + y) % 3 else (0, 0, 0, 255))
        if frame == 1 or frame == 3:
            # Loaded cue: make one pixel white that is black in the empty frame.
            cell.putpixel((1, content_bottom - 1), (255, 255, 255, 255))
        frames.append(cell)
        image.paste(cell, (frame * gen_pilot.FRAME_WIDTH, 0))
    if mirror:
        image.paste(frames[0].transpose(Image.Transpose.FLIP_LEFT_RIGHT), (2 * gen_pilot.FRAME_WIDTH, 0))
        image.paste(frames[1].transpose(Image.Transpose.FLIP_LEFT_RIGHT), (3 * gen_pilot.FRAME_WIDTH, 0))
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)
    return path


class GenPilotTest(unittest.TestCase):
    def test_authored_sheet_normalizes_to_four_horizontal_frames(self):
        sheet = gen_pilot.normalize()
        self.assertEqual(sheet.width, gen_pilot.FRAME_WIDTH * gen_pilot.FRAMES)
        self.assertGreaterEqual(sheet.height, gen_pilot.MIN_HEIGHT)
        self.assertEqual(sheet.mode, "RGBA")
        for frame in range(gen_pilot.FRAMES):
            cell = sheet.crop((frame * gen_pilot.FRAME_WIDTH, 0, (frame + 1) * gen_pilot.FRAME_WIDTH, sheet.height))
            self.assertIsNotNone(cell.getbbox())

    def test_write_art_trims_rows_and_emits_host_arrays(self):
        source = make_sheet(FIXTURES / "synthetic_104x16.png")
        output = FIXTURES / "out.png"
        header = FIXTURES / "pilot_host.hpp"
        written, written_header = gen_pilot.write_art(source, FIXTURES, header)
        self.assertEqual(Image.open(written).size, (104, 12))
        self.assertEqual(written_header.read_text(), gen_pilot.host_header(gen_pilot.normalize(source)))
        text = written_header.read_text()
        self.assertIn("HAULER_ART_PAGES = 2", text)
        self.assertIn("haulerHostData[4][2][26]", text)
        self.assertIn("haulerHostMask[4][2][26]", text)
        self.assertEqual(output.exists(), False)

    def test_rejects_art_that_does_not_span_the_frame_width(self):
        source = make_sheet(FIXTURES / "narrow.png")
        image = Image.open(source).convert("RGBA")
        image.putpixel((0, 0), (0, 0, 0, 0))
        image.putalpha(image.getchannel("A"))
        # Clear the leftmost column of frame 0 only.
        for y in range(image.height):
            image.putpixel((0, y), (0, 0, 0, 0))
        image.save(source)
        with self.assertRaises(ValueError):
            gen_pilot.normalize(source)

    def test_rejects_broken_left_facing_mirror(self):
        source = make_sheet(FIXTURES / "broken-mirror.png", mirror=False)
        image = Image.open(source).convert("RGBA")
        image.putpixel((2 * gen_pilot.FRAME_WIDTH + 4, 4), (255, 255, 255, 255))
        image.save(source)
        with self.assertRaises(ValueError):
            gen_pilot.normalize(source)

    def test_rejects_non_monochrome_opaque_pixels(self):
        source = make_sheet(FIXTURES / "gray.png", color=(128, 128, 128, 255))
        with self.assertRaises(ValueError):
            gen_pilot.normalize(source)

    def test_rejects_art_shorter_than_the_collision_hull(self):
        source = make_sheet(FIXTURES / "short.png", content_top=0, content_bottom=7)
        with self.assertRaises(ValueError):
            gen_pilot.normalize(source)


if __name__ == "__main__":
    unittest.main()

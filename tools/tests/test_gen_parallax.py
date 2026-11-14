import json
import sys
import unittest
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_parallax
import parallax_art


def block(data, offset):
    result = bytearray(gen_parallax.BLOCK_WIDTH)
    mask = data[offset]
    offset += 1
    for bit in range(gen_parallax.BLOCK_WIDTH):
        if mask & (1 << bit):
            result[bit] = data[offset]
            offset += 1
    return bytes(result), offset


class ParallaxPackingTest(unittest.TestCase):
    def test_four_native_area_drawings_are_distinct_and_pack_without_pixel_loss(self):
        drawings = parallax_art.plates()
        self.assertEqual(len(drawings), len(parallax_art.AREAS))
        self.assertEqual(len({image.tobytes() for image in drawings}), len(drawings))
        for image in drawings:
            self.assertEqual(image.mode, "1")
            self.assertEqual(image.size, (256, 128))
            data = gen_parallax.pack(image)
            for y in range(128):
                for x in range(256):
                    self.assertEqual(bool(data[(y // 8) * 256 + x] & (1 << (y % 8))),
                                     bool(image.getpixel((x, y))))

    def test_every_cart_block_preserves_all_pixels_phases_and_wrapped_columns(self):
        data = gen_parallax.PLANE.read_bytes()
        table = 320 * 160
        entry = 0
        for original in parallax_art.plates():
            image = gen_parallax.dotted(original)
            native = gen_parallax.pack(image)
            expected = gen_parallax.shifted(native)
            decoded = bytearray()
            for _ in range(len(expected) // gen_parallax.BLOCK_WIDTH):
                offset = int.from_bytes(data[table + entry * 3:table + entry * 3 + 3], "little")
                output, end = block(data, offset)
                self.assertEqual(len(output), gen_parallax.BLOCK_WIDTH)
                decoded.extend(output)
                entry += 1
                if entry < len(parallax_art.AREAS) * 8 * 16 * gen_parallax.BLOCKS_PER_ROW:
                    following = int.from_bytes(data[table + entry * 3:table + entry * 3 + 3], "little")
                    self.assertEqual(end, following, "indexed blocks are contiguous and bounded")
            self.assertEqual(bytes(decoded), expected)

    def test_dots_are_anchored_to_the_art_and_never_add_pixels(self):
        for original in parallax_art.plates():
            image = gen_parallax.dotted(original)
            for y in range(128):
                for x in range(256):
                    self.assertEqual(bool(image.getpixel((x, y))),
                                     bool(original.getpixel((x, y))) and (x + y) % 2 == 0)

    def test_scenery_areas_follow_authored_rooms_including_music_override_room(self):
        source = json.loads(gen_parallax.SOURCE.read_text())
        data = gen_parallax.PLANE.read_bytes()
        for room in source["rooms"]:
            x, y = room["position"]
            self.assertEqual(data[y * source["dimensions"][0] + x],
                             parallax_art.AREAS.index(room["region"]), room["name"])

    def test_empty_full_and_mixed_block_round_trips(self):
        for source in (bytes(8), bytes([255] * 8), bytes(range(1, 9)),
                       bytes([0, 0, 3, 3, 4, 5, 0, 7])):
            encoded = gen_parallax.encode_row(source)
            decoded, end = block(encoded, 0)
            self.assertEqual(decoded, source)
            self.assertEqual(end, len(encoded))

    def test_rejects_non_native_sources(self):
        with self.assertRaises(ValueError):
            gen_parallax.pack(Image.new("1", (128, 64)))

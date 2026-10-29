import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_hud


class HudGenerationTest(unittest.TestCase):
    def test_notice_frames_match_device_selectors(self):
        source = (ROOT / "src/hud.hpp").read_text()
        block = re.search(r"enum Notice : uint8_t \{(.*?)\};", source, re.S).group(1)
        selectors = re.findall(r"\bkNotice\w+", block)
        expected = ["kNoticeNone"] + [
            "kNotice" + "".join(word.capitalize() for word in symbol[4:].split("_"))
            for symbol in gen_hud.NOTICE_STRINGS
        ]
        self.assertEqual(selectors, expected)

    def test_generated_sheet_contains_each_exact_text_row(self):
        from PIL import Image

        strings = gen_hud.load_strings(ROOT / "fxdata/fxdata.txt")
        sheet = Image.open(ROOT / "images/hud/hud_notices_128x8.png").convert("RGBA")
        self.assertEqual(sheet.size, (128 * len(gen_hud.NOTICE_STRINGS), 8))
        for index, symbol in enumerate(gen_hud.NOTICE_STRINGS):
            self.assertEqual(
                sheet.crop((index * 128, 0, (index + 1) * 128, 8)).tobytes(),
                gen_hud.text_row(strings[symbol]).tobytes(),
                symbol,
            )


if __name__ == "__main__":
    unittest.main()

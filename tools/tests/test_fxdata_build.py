import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build" / "tests" / "fxdata-build"


class FxDataBuildTest(unittest.TestCase):
    def build(self, source):
        BUILD.mkdir(parents=True, exist_ok=True)
        script = BUILD / "fxdata.txt"
        script.write_text(source)
        subprocess.run(
            [sys.executable, str(ROOT / "tools" / "fxdata-build.py"), str(script)],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
        header = (BUILD / "fxdata.h").read_text()
        image = (BUILD / "fxdata.bin").read_bytes()
        return header, image

    def test_save_section_reserves_erased_cart_pages(self):
        header, image = self.build("string a = 'A'\nsavesection 4096\n")

        self.assertIn("constexpr uint16_t FX_DATA_PAGE  = 0xffef;", header)
        self.assertIn("constexpr uint16_t FX_SAVE_PAGE  = 0xfff0;", header)
        self.assertIn("constexpr uint24_t FX_SAVE_BYTES = 4096;", header)
        self.assertEqual(len(image), 256 + 4096)
        self.assertEqual(image[:2], b"A\x00")
        self.assertEqual(image[256:], b"\xFF" * 4096)

    def test_save_section_rejects_a_missing_size(self):
        # The bare marker remains a start marker, not an implicit allocation:
        # with no following bytes there is no save partition and no crash.
        header, image = self.build("string a = 'A'\nsavesection\n")
        self.assertIn("constexpr uint16_t FX_DATA_PAGE  = 0xffff;", header)
        self.assertNotIn("FX_SAVE_PAGE", header)
        self.assertEqual(len(image), 256)
        self.assertEqual(image[:2], b"A\x00")
        self.assertEqual(image[2:], b"\xFF" * 254)


if __name__ == "__main__":
    unittest.main()

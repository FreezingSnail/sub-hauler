import re
import sys
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_menu


class MenuGenerationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.strings = gen_menu.load_strings(ROOT / "fxdata/fxdata.txt")
        cls.station = gen_menu.load_constants(ROOT / "src/station_view.hpp", gen_menu.STATION_CONSTANTS)
        cls.shop = dict(cls.station)
        cls.shop.update(gen_menu.load_constants(ROOT / "src/shop_view.hpp", gen_menu.SHOP_CONSTANTS))

    def test_rules_exclude_enum_sentinel_and_preserve_economy(self):
        prices, maximum, items = gen_menu.load_shop_rules(ROOT / "src/shop.hpp")
        self.assertEqual((prices, maximum, items), ([60, 70], 3, 2))

    def test_notice_selectors_match_generated_hint_order(self):
        for path, symbols in (("station_view.hpp", gen_menu.STATION_HINTS), ("shop_view.hpp", gen_menu.SHOP_HINTS)):
            source = (ROOT / "src" / path).read_text()
            block = re.search(r"enum PanelHint : uint8_t \{(.*?)\};", source, re.S).group(1)
            names = re.findall(r"\bkPanel\w+", block)
            self.assertEqual(len(names), len(symbols))
        self.assertEqual(gen_menu.STATION_HINTS[0], "txt_service_berth_hint")
        self.assertEqual(gen_menu.SHOP_HINTS, ("txt_buy_hint", "txt_low_credits", "txt_max_level"))

    def test_all_generated_panels_have_the_exact_state_frames(self):
        prices, maximum, items = gen_menu.load_shop_rules(ROOT / "src/shop.hpp")
        groups = (
            ("station_menu_128x64.png", gen_menu.station_panels(self.strings, self.station), 30),
            ("shop_menu_128x64.png", gen_menu.shop_panels(self.strings, self.shop, prices, maximum, items), 32),
        )
        for name, frames, count in groups:
            self.assertEqual(len(frames), count)
            sheet = Image.open(ROOT / "images/menu" / name).convert("RGBA")
            self.assertEqual(sheet.size, (count * 128, 64))
            for index, expected in enumerate(frames):
                self.assertEqual(sheet.crop((index * 128, 0, (index + 1) * 128, 64)).tobytes(), expected.tobytes(), (name, index))

    def test_baked_shop_selection_is_roomy_and_price_comes_from_rules(self):
        prices, maximum, items = gen_menu.load_shop_rules(ROOT / "src/shop.hpp")
        normal = gen_menu.shop_panels(self.strings, self.shop, prices, maximum, items)[0]
        changed = gen_menu.shop_panels(self.strings, self.shop, [61, prices[1]], maximum, items)[0]
        self.assertNotEqual(normal.tobytes(), changed.tobytes())
        x = 127
        top = self.shop["kPanelRowY0"] - self.shop["kPanelInsetY"]
        bottom = top + self.shop["kPanelInvertH"]
        self.assertTrue(all(normal.getpixel((x, y)) == gen_menu.WHITE for y in range(top, bottom)))
        self.assertEqual(normal.getpixel((x, bottom)), gen_menu.BLACK)
        self.assertGreaterEqual(self.shop["kPanelRowStep"], 12)

    def test_hint_rows_stop_before_the_last_action_pixel(self):
        for symbols, layout, file in (
            (gen_menu.STATION_HINTS, self.station, "station_hints_128x7.png"),
            (gen_menu.SHOP_HINTS, self.shop, "shop_hints_128x7.png"),
        ):
            sheet = gen_menu.hint_rows(self.strings, symbols, layout)
            self.assertEqual(sheet.size, (128 * len(symbols), 7))
            self.assertEqual(layout["kHintY"], 57)
            self.assertEqual(Image.open(ROOT / "images/menu" / file).tobytes(), sheet.tobytes())


if __name__ == "__main__":
    unittest.main()

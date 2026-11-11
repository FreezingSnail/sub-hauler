import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_dock_barks as gen


class DockBarkPreviewTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = gen.build_document()
        cls.states = [state["id"] for state in cls.document["states"]]
        cls.berths = [berth["id"] for berth in cls.document["berths"]]

    def bark(self, state, berth):
        return self.document["berths"][self.berths.index(berth)]["barks"][self.states.index(state)]

    def test_committed_preview_matches_the_generator(self):
        self.assertEqual(gen.OUTPUT.read_text(encoding="utf-8"), gen.render(self.document))
        self.assertEqual(gen.main(["--check"]), 0)

    def test_every_berth_and_checkpoint_has_one_shipped_line(self):
        self.assertEqual(len(self.berths), 8)
        self.assertEqual(len(self.states), 8)
        for berth in self.document["berths"]:
            self.assertEqual(len(berth["barks"]), len(self.states), berth["id"])
            for row in berth["barks"]:
                self.assertTrue(row["text"], berth["id"])
                self.assertTrue(row["symbol"].startswith("txt_"), row["symbol"])
                self.assertTrue(row["bark"].startswith("kDockBark"), row["bark"])
                self.assertNotEqual(row["bark"], "kDockBarkNone")

    def test_preview_text_is_the_fx_string_not_a_copy(self):
        # Parse the shipping layout independently of the generator so a prose
        # edit without `make gen` fails here as well as in gen-check.
        fxtext = gen.FXDATA.read_text(encoding="utf-8")
        strings = dict(re.findall(r"^\s*string\s+(\w+)\s*=\s*'([^']*)'\s*$", fxtext, re.M))
        for berth in self.document["berths"]:
            for row in berth["barks"]:
                self.assertEqual(row["text"], strings[row["symbol"]], row["symbol"])

    def test_bark_table_order_still_matches_the_dock_bark_enum(self):
        campaign = gen.CAMPAIGN.read_text(encoding="utf-8")
        station = gen.STATION_VIEW.read_text(encoding="utf-8")
        enum = re.search(r"enum\s+DockBark\s*:[^{]*\{(.*?)\};", campaign, re.S).group(1)
        symbols = re.findall(r"\b(kDockBark[A-Za-z0-9_]+)\b", enum)
        table = re.search(r"kDockBarkText\s*\[\s*\]\s*PROGMEM\s*=\s*\{(.*?)\};", station, re.S).group(1)
        entries = re.findall(r"\b(?:0|txt_[A-Za-z0-9_]+)\b", table)
        self.assertEqual(len(symbols), len(entries))
        for berth in self.document["berths"]:
            for row in berth["barks"]:
                self.assertEqual(entries[symbols.index(row["bark"])], row["symbol"], row["bark"])

    def test_native_row_constants_are_used(self):
        self.assertEqual(self.document["font"]["cellWidth"], 6)
        self.assertEqual(self.document["font"]["cellHeight"], 8)
        self.assertEqual(self.document["layout"]["hintX"], 3)
        self.assertEqual(self.document["layout"]["barkY"], 56)
        for berth in self.document["berths"]:
            for row in berth["barks"]:
                self.assertLessEqual(len(row["text"]), self.document["layout"]["hintChars"])
                self.assertRegex(row["text"], r"^[A-Z0-9 :./'-]+$")

    def test_known_checkpoint_transitions(self):
        expected = {
            ("first", "kTidewall"): "txt_benthos_lamps_on",
            ("first", "kBenthos"): "txt_pump_survey_clue",
            ("first", "kCormorant"): "txt_survey_lift_clue",
            ("first", "kDeepAnchor"): "txt_beacon_clue",
            ("first", "kPumpRefuge"): "txt_pump_source_clue",
            ("first", "kLowerGantry"): "txt_lift_source_clue",
            ("first", "kFreightYard"): "txt_lift_source_clue",
            ("first", "kDeepRelay"): "txt_deep_relay",
            ("pump", "kBenthos"): "txt_survey_lift_clue",
            ("pump", "kCormorant"): "txt_survey_lift_clue",
            ("pump", "kPumpRefuge"): "txt_survey_lift_clue",
            ("lift", "kBenthos"): "txt_lift_restored",
            ("lift", "kCormorant"): "txt_beacon_needs_seal",
            ("lift", "kPumpRefuge"): "txt_beacon_needs_seal",
            ("seal", "kCormorant"): "txt_beacon_ready",
            ("seal", "kPumpRefuge"): "txt_beacon_ready",
            ("core", "kDeepAnchor"): "txt_deep_relay",
            ("core", "kPumpRefuge"): "txt_deep_relay",
            ("core", "kDeepRelay"): "txt_deep_relay",
            ("relay", "kDeepAnchor"): "txt_beacon_lit",
            ("relay", "kDeepRelay"): "txt_beacon_lit",
            ("relay", "kPumpRefuge"): "txt_beacon_lit",
            ("home", "kTidewall"): "txt_network_on",
            ("home", "kDeepAnchor"): "txt_network_on",
            ("home", "kDeepRelay"): "txt_network_on",
            ("done", "kTidewall"): "txt_network_on",
            ("done", "kDeepAnchor"): "txt_network_on",
            ("done", "kDeepRelay"): "txt_network_on",
        }
        for key, symbol in expected.items():
            self.assertEqual(self.bark(*key)["symbol"], symbol, key)

    def test_generation_is_deterministic(self):
        self.assertEqual(gen.render(gen.build_document()), gen.render(gen.build_document()))


if __name__ == "__main__":
    unittest.main()

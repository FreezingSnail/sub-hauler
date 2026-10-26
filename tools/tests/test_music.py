"""Asset contracts for the original underwater cues and their viewer catalog."""

import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MusicAssetsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = json.loads((ROOT / "music/catalog.json").read_text())
        cls.manifest = json.loads((ROOT / "fxdata/music/manifest.json").read_text())
        cls.header = (ROOT / "src/fxdata.h").read_text()
        cls.cart = (ROOT / "fxdata/fxdata-data.bin").read_bytes()

    def test_catalog_matches_exported_sources(self):
        self.assertEqual(len(self.catalog["entries"]), 6)
        self.assertEqual([e["id"] for e in self.catalog["entries"]],
                         [s["id"] for s in self.manifest["songs"]])
        for song in self.manifest["songs"]:
            with self.subTest(song=song["id"]):
                raw = (ROOT / song["source"]).read_bytes()
                self.assertEqual(hashlib.sha256(raw).hexdigest(), song["source_sha256"])
                source = json.loads(raw)
                self.assertEqual(source["id"], song["id"])
                self.assertTrue(source["loop"])
                clock = source["clock"]
                rate = 2000 / (2000 // clock["tick_rate"])
                self.assertAlmostEqual(song["clock"]["effective_bpm"],
                                       rate * 60 / clock["ticks_per_beat"])
                self.assertTrue(song["folder"])

    def test_water_variations_cover_distinct_tempos_and_native_effects(self):
        variations = {"sh_abyssal_glass", "sh_tidepool_waltz", "sh_bubblejet_run"}
        tempos, effects, lfo_modes, meters = set(), set(), set(), set()
        for song in self.manifest["songs"]:
            if song["id"] not in variations:
                continue
            source = json.loads((ROOT / song["source"]).read_text())
            tempos.add(song["clock"]["effective_bpm"])
            meters.add(source["clock"]["beats_per_bar"])
            for pattern in source["patterns"].values():
                for effect in pattern.get("effects", []):
                    effects.add(effect["type"])
                    self.assertLessEqual(effect["until"], pattern["ticks"])
                    if effect["type"] == "lfo":
                        lfo_modes.add(effect["mode"])
        self.assertEqual(tempos, {40, 75, 100})
        self.assertEqual(meters, {3, 4})
        self.assertEqual(effects, {"arpeggio", "note_cut", "lfo", "slide",
                                   "glissando", "noise_retrig"})
        self.assertEqual(lfo_modes, {"vibrato", "tremolo", "modulation"})

    def test_complete_streams_are_in_the_single_cart(self):
        for song in self.manifest["songs"]:
            with self.subTest(song=song["id"]):
                stream = (ROOT / "fxdata/music" / (song["id"] + ".bin")).read_bytes()
                offset = int(re.search(r"\b" + song["id"] + r"\s*=\s*(0x[0-9A-Fa-f]+)", self.header)[1], 0)
                self.assertEqual(len(stream), song["bytes"])
                self.assertEqual(hashlib.sha256(stream).hexdigest(), song["sha256"])
                self.assertEqual(self.cart[offset:offset + len(stream)], stream)
                self.assertEqual(stream[0], 0xC1)
                self.assertLess(int.from_bytes(stream[1:4], "little"), len(stream) - 1)
                self.assertEqual(stream[-1], 0)

    def test_decoder_admission_and_loop_period(self):
        for song in self.manifest["songs"]:
            with self.subTest(song=song["id"]):
                stream = song["stream"]
                self.assertTrue(stream["loop_exported"])
                self.assertLessEqual(stream["max_record_bytes"], self.manifest["ring"])
                self.assertLessEqual(stream["required_window_bytes"], self.manifest["ring"])
                self.assertEqual(stream["loop_cycle_ticks"], song["total_ticks"])


if __name__ == "__main__":
    unittest.main()

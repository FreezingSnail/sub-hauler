"""Contracts for the deterministic offline reference-audio pipeline."""

import hashlib
import json
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_audio_ref


class AudioReferenceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = json.loads((ROOT / "fxdata/music/manifest.json").read_text())
        cls.manifest = json.loads((ROOT / "fxdata/music/reference-manifest.json").read_text())

    def test_capture_plan_uses_samples_not_seconds_for_step_budget(self):
        plan = gen_audio_ref.capture_plan(3072, 50)
        self.assertEqual(plan["warmup_raw"], 3072000)
        self.assertEqual(plan["loop_raw"], 3072000)
        self.assertEqual(plan["guard_raw"], 2500)
        self.assertEqual(plan["capture_raw"], 6146500)
        self.assertEqual(plan["out_samples"], 491520)
        self.assertEqual(gen_audio_ref.capture_step_budget(plan["capture_raw"]), 125000)
        self.assertEqual(gen_audio_ref.capture_step_budget(50000), 3000)
        last_base = (plan["out_samples"] - 1) * gen_audio_ref.CAPTURE_RATE // gen_audio_ref.OUTPUT_RATE
        self.assertGreater(plan["warmup_raw"], 95)
        self.assertGreaterEqual(plan["loop_raw"] + plan["guard_raw"] - (last_base + 96), 0)

    def test_capture_plan_rejects_non_integer_sample_counts(self):
        with self.assertRaises(ValueError):
            gen_audio_ref.capture_plan(1, 3)

    def test_manifest_counts_match_the_six_expected_loops(self):
        expected = {
            "sh_harbor_bubbles": 61.44,
            "sh_kelp_drift": 69.12,
            "sh_sonar_lullaby": 76.80,
            "sh_abyssal_glass": 72.00,
            "sh_tidepool_waltz": 38.40,
            "sh_bubblejet_run": 38.40,
        }
        songs = {song["id"]: song for song in self.source["songs"]}
        self.assertEqual([cue["id"] for cue in self.manifest["cues"]], list(expected))
        total = 0
        for cue in self.manifest["cues"]:
            with self.subTest(cue=cue["id"]):
                song = songs[cue["id"]]
                plan = gen_audio_ref.capture_plan(song["total_ticks"],
                                                  song["clock"]["effective_tick_rate"])
                self.assertAlmostEqual(cue["seconds"], expected[cue["id"]])
                self.assertEqual(cue["loop_samples"], plan["out_samples"])
                self.assertEqual(cue["capture_samples_50k"], plan["capture_raw"])
                self.assertEqual(cue["source_sha256"], song["source_sha256"])
                self.assertEqual(cue["stream_sha256"], song["sha256"])
                total += cue["loop_samples"]
        self.assertEqual(total, 2849280)
        self.assertEqual(self.manifest["total_samples"], total)
        self.assertAlmostEqual(self.manifest["total_seconds"], 356.16)

    def test_calibration_is_global_and_unambiguous(self):
        calibration = self.manifest["calibration"]
        self.assertEqual(calibration["emulator_pwm_full_scale"], 2000)
        self.assertEqual(calibration["gain_numerator"], 32767)
        self.assertEqual(calibration["gain_denominator"], 2000)
        self.assertFalse(calibration["per_cue_normalization"])
        self.assertEqual(calibration["pwm8_to_playback"],
                         "pwm8 = clamp(128 + (ref >> 8), 0, 255)")
        for cue in self.manifest["cues"]:
            with self.subTest(cue=cue["id"]):
                self.assertEqual(cue["clipped_samples"], 0)
                self.assertLess(abs(cue["dc_mean"]), 64.0)
                self.assertGreater(max(abs(cue["raw_peak_min"]), abs(cue["raw_peak_max"])), 50)
                # Canonical peak is the fixed global gain of the raw PWM peak.
                raw_peak = max(abs(cue["raw_peak_min"]), abs(cue["raw_peak_max"]))
                expected = raw_peak * 32767 // 2000
                canonical_peak = max(abs(cue["peak_min"]), abs(cue["peak_max"]))
                self.assertLessEqual(abs(canonical_peak - expected), 1)

    def test_pcm_and_wav_are_exact_length_when_present(self):
        for cue in self.manifest["cues"]:
            pcm = ROOT / "build/audio-reference" / (cue["id"] + ".pcm")
            wav = ROOT / "build/audio-reference" / (cue["id"] + ".wav")
            if not pcm.exists() or not wav.exists():
                continue
            with self.subTest(cue=cue["id"]):
                pcm_bytes = pcm.read_bytes()
                self.assertEqual(len(pcm_bytes), cue["loop_samples"] * 2)
                self.assertEqual(hashlib.sha256(pcm_bytes).hexdigest(), cue["pcm_sha256"])
                wav_bytes = wav.read_bytes()
                self.assertEqual(hashlib.sha256(wav_bytes).hexdigest(), cue["wav_sha256"])
                self.assertEqual(wav_bytes[:4], b"RIFF")
                self.assertEqual(wav_bytes[8:12], b"WAVE")
                channels, rate, _, _, bits = struct.unpack_from("<HHIIH", wav_bytes, 22)
                self.assertEqual((channels, rate, bits), (1, 8000, 16))
                self.assertEqual(wav_bytes[44:], pcm_bytes)
                self.assertEqual(len(wav_bytes), 44 + cue["loop_samples"] * 2)
                raw = ROOT / "build/audio-reference/raw" / (cue["id"] + ".pcm")
                if raw.exists():
                    self.assertEqual(hashlib.sha256(raw.read_bytes()).hexdigest(),
                                     cue["raw_pcm_sha256"])

    def test_manifest_provenance_is_complete(self):
        provenance = self.manifest["provenance"]
        for key in ("atmllib2fx", "ardens", "avr_gcc", "host_compiler",
                    "generator_sha256", "capture_tool_sha256",
                    "coefficient_source_sha256"):
            self.assertIn(key, provenance)
        self.assertEqual(len(provenance["generator_sha256"]), 64)
        self.assertEqual(len(provenance["capture_tool_sha256"]), 64)
        self.assertEqual(len(provenance["coefficient_source_sha256"]), 64)
        self.assertIn("osc.c", "".join(provenance["atmllib2fx"]["sources"]))
        self.assertIn("absim_sound.hpp", "".join(provenance["ardens"]["sources"]))

    def test_parse_diag_round_trip(self):
        values = (gen_audio_ref.DIAG_MAGIC, 6146, 0, 1, 0, 0, 1, 0, 1, 1, 0)
        raw = struct.pack(gen_audio_ref.DIAG_FORMAT, *values)
        parsed = gen_audio_ref.parse_diag(raw.hex())
        self.assertEqual(parsed["magic"], gen_audio_ref.DIAG_MAGIC)
        self.assertEqual(parsed["ticks"], 6146)
        self.assertEqual(parsed["underruns"], 0)
        self.assertEqual(parsed["loop_streaming"], 1)


if __name__ == "__main__":
    unittest.main()

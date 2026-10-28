#!/usr/bin/env python3
"""Tests for the sampled IMA ADPCM assets and generator."""

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/gen_audio_adpcm.py"
MANIFEST = ROOT / "fxdata/music/adpcm-manifest.json"
REFERENCE = ROOT / "fxdata/music/reference-manifest.json"
OUT = ROOT / "fxdata/music/adpcm"


def load_tool():
    spec = importlib.util.spec_from_file_location("gen_audio_adpcm", TOOL)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class AudioAdpcmTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tool = load_tool()
        cls.manifest = json.loads(MANIFEST.read_text())
        cls.reference = json.loads(REFERENCE.read_text())

    def test_manifest_shape_and_totals(self):
        self.assertEqual(self.manifest["format"], "sub-hauler-adpcm-v1")
        self.assertEqual(self.manifest["sample_rate"], 8000)
        self.assertEqual(self.manifest["channels"], 1)
        self.assertEqual(len(self.manifest["cues"]), 6)
        self.assertEqual(self.manifest["total_bytes"], 1424640)
        self.assertEqual(self.manifest["total_samples"], 2849280)
        self.assertAlmostEqual(self.manifest["total_seconds"], 356.16)
        self.assertEqual(sum(c["bytes"] for c in self.manifest["cues"]), 1424640)
        self.assertEqual(sum(c["samples"] for c in self.manifest["cues"]), 2849280)

    def test_every_asset_is_exact_and_hashed(self):
        reference = {c["id"]: c for c in self.reference["cues"]}
        for cue in self.manifest["cues"]:
            path = OUT / (cue["id"].removeprefix("sh_") + ".bin")
            data = path.read_bytes()
            self.assertEqual(len(data), cue["bytes"])
            self.assertEqual(cue["bytes"] * 2, cue["samples"])
            self.assertEqual(cue["samples"] % 2, 0)
            self.assertEqual(hashlib.sha256(data).hexdigest(), cue["encoded_sha256"])
            self.assertEqual(cue["reference_pcm_sha256"],
                             reference[cue["id"]]["pcm_sha256"])
            self.assertLessEqual(cue["bytes"], 0xFFFFFF)
            self.assertLessEqual(cue["samples"], 0xFFFFFF)

    def test_decoded_assets_match_manifest_and_reference(self):
        reference = {c["id"]: c for c in self.reference["cues"]}
        for cue in self.manifest["cues"]:
            raw = (ROOT / "build/audio-reference" / (cue["id"] + ".pcm")).read_bytes()
            samples = list(struct.unpack("<%dh" % (len(raw) // 2), raw))
            self.assertEqual(len(samples), cue["samples"])
            data = (OUT / (cue["id"].removeprefix("sh_") + ".bin")).read_bytes()
            decoded = []
            predictor = cue["initial_predictor"]
            index = cue["initial_index"]
            self.assertEqual(predictor, samples[0])
            for byte in data:
                for nibble in (byte & 0x0F, byte >> 4):
                    predictor, index = self.tool.decode_nibble(nibble, predictor, index)
                    decoded.append(predictor)
            self.assertEqual(decoded[0], samples[0])
            self.assertEqual(cue["quality"]["decoded_first_error"], 0)
            packed = struct.pack("<%dh" % len(decoded), *decoded)
            self.assertEqual(hashlib.sha256(packed).hexdigest(),
                             cue["decoded_pcm_sha256"])
            self.assertEqual(self.tool.crc16_128(decoded),
                             cue["check_crc16_first128"])
            self.assertEqual(cue["quality"]["max_abs_error"],
                             max(abs(a - b) for a, b in zip(decoded, samples)))

    def test_known_vectors_and_extremes(self):
        tool = self.tool
        predictor, index = 0, 0
        values = []
        for nibble in (0x0, 0x4, 0x7):
            predictor, index = tool.decode_nibble(nibble, predictor, index)
            values.append((predictor, index))
        self.assertEqual(values, [(0, 0), (7, 2), (23, 10)])

        predictor, index = 0, 88
        sample, index = tool.decode_nibble(0x7, predictor, index)
        self.assertEqual((sample, index), (32767, 88))
        predictor, index = 0, 88
        sample, index = tool.decode_nibble(0xF, predictor, index)
        self.assertEqual((sample, index), (-32768, 88))
        predictor, index = 32767, 88
        self.assertEqual(tool.decode_nibble(0x7, predictor, index)[0], 32767)
        predictor, index = -32768, 88
        self.assertEqual(tool.decode_nibble(0xF, predictor, index)[0], -32768)
        predictor, index = 0, 88
        self.assertEqual(tool.decode_nibble(0x0, predictor, index), (4095, 87))

    def test_encode_is_deterministic_and_low_nibble_first(self):
        samples = [0, 0, 3000, -3000, 32767, -32768] * 8
        first = self.tool.encode_pcm(samples)
        second = self.tool.encode_pcm(samples)
        self.assertEqual(first, second)
        encoded, decoded, predictor, index = first
        self.assertEqual(len(encoded), len(samples) // 2)
        self.assertEqual(predictor, samples[0])
        self.assertEqual(index, 0)
        self.assertEqual(decoded[0], samples[0])
        # The first sample is sample 0 and must occupy the low nibble.
        low = encoded[0] & 0x0F
        high = encoded[0] >> 4
        state = (samples[0], 0)
        low_sample, low_index = self.tool.decode_nibble(low, *state)
        self.assertEqual(low_sample, decoded[0])
        high_sample, _ = self.tool.decode_nibble(high, low_sample, low_index)
        self.assertEqual(high_sample, decoded[1])

    def test_wav_header_contract(self):
        decoded = struct.pack("<8h", *range(8))
        wav = self.tool.make_wav(decoded)
        self.assertEqual(wav[:4], b"RIFF")
        self.assertEqual(wav[8:12], b"WAVE")
        fields = struct.unpack("<HHIIHH", wav[20:36])
        self.assertEqual(fields[0], 1)
        self.assertEqual(fields[1], 1)
        self.assertEqual(fields[2], 8000)
        self.assertEqual(fields[3], 16000)
        self.assertEqual(fields[5], 16)

    def test_fx_image_within_two_mib(self):
        payload = (ROOT / "fxdata/fxdata-data.bin").stat().st_size
        image = (ROOT / "fxdata/fxdata.bin").stat().st_size
        self.assertLessEqual(payload, 2 * 1024 * 1024)
        self.assertLessEqual(image, 2 * 1024 * 1024)
        header = (ROOT / "src/fxdata.h").read_text()
        self.assertIn("constexpr uint24_t audio_harbor_bubbles", header)
        self.assertIn("constexpr uint24_t audio_bubblejet_run", header)
        self.assertIn(f"constexpr uint24_t FX_DATA_BYTES = {payload};", header)


if __name__ == "__main__":
    unittest.main()

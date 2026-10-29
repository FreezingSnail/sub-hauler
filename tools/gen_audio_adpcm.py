#!/usr/bin/env python3
"""Encode the six 8 kHz reference loops as raw 4-bit IMA ADPCM.

Input is the deterministic reference set written by tools/gen_audio_ref.py
(build/audio-reference/*.pcm and fxdata/music/reference-manifest.json). Output
is one blockless nibble stream per cue under fxdata/music/adpcm/, a compact
manifest, a generated descriptor header, and decoded listening WAV/PCM files
under build/audio-adpcm/.

The codec is the standard IMA/DVI ADPCM update. Nibbles are packed low sample
first. Every cue starts with predictor = first reference sample and step index
0, and loops by resetting that exact state at the authored sample boundary.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
REFERENCE_DIR = ROOT / "build/audio-reference"
REFERENCE_MANIFEST = ROOT / "fxdata/music/reference-manifest.json"
OUT_DIR = ROOT / "fxdata/music/adpcm"
MANIFEST = ROOT / "fxdata/music/adpcm-manifest.json"
HEADER = ROOT / "src/generated/audio.hpp"
LISTEN_DIR = ROOT / "build/audio-adpcm"
SAMPLE_RATE = 8000

STEP_TABLE = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
    598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707,
    1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
    5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635,
    13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
)
INDEX_TABLE = (-1, -1, -1, -1, 2, 4, 6, 8)


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def clamp16(value):
    if value > 32767:
        return 32767
    if value < -32768:
        return -32768
    return value


def decode_nibble(nibble, predictor, index):
    step = STEP_TABLE[index]
    diff = step >> 3
    if nibble & 1:
        diff += step >> 2
    if nibble & 2:
        diff += step >> 1
    if nibble & 4:
        diff += step
    if nibble & 8:
        predictor -= diff
    else:
        predictor += diff
    predictor = clamp16(predictor)
    index += INDEX_TABLE[nibble & 7]
    if index < 0:
        index = 0
    elif index > 88:
        index = 88
    return predictor, index


def encode_pcm(samples):
    """Return (packed bytes, decoded samples, initial predictor, initial index)."""
    initial_predictor = samples[0]
    initial_index = 0
    predictor = initial_predictor
    index = initial_index
    packed = bytearray()
    decoded = []
    low = 0
    for position, sample in enumerate(samples):
        step = STEP_TABLE[index]
        diff = sample - predictor
        sign = 8 if diff < 0 else 0
        if sign:
            diff = -diff
        base_code = (diff << 2) // step
        if base_code > 7:
            base_code = 7
        best_code = base_code
        best_error = 1 << 30
        for code in (base_code - 1, base_code, base_code + 1):
            if code < 0 or code > 7:
                continue
            quantized = step >> 3
            if code & 1:
                quantized += step >> 2
            if code & 2:
                quantized += step >> 1
            if code & 4:
                quantized += step
            candidate = predictor - quantized if sign else predictor + quantized
            if candidate > 32767:
                candidate = 32767
            elif candidate < -32768:
                candidate = -32768
            error = abs(sample - candidate)
            if error < best_error:
                best_error = error
                best_code = code
        code = best_code
        nibble = code | sign
        if position & 1:
            packed.append(low | (nibble << 4))
        else:
            low = nibble
        predictor, index = decode_nibble(nibble, predictor, index)
        decoded.append(predictor)
    if len(samples) & 1:
        raise ValueError("odd sample count cannot nibble-pack exactly")
    return bytes(packed), decoded, initial_predictor, initial_index


def crc16_128(samples):
    crc = 0xFFFF
    for sample in samples[:128]:
        crc ^= sample & 0xFFFF
        for _ in range(8):
            crc = ((crc >> 1) ^ 0xA001) if (crc & 1) else (crc >> 1)
    return crc


def quality_metrics(reference, decoded):
    count = len(reference)
    if count != len(decoded):
        raise ValueError("reference/decoded length mismatch")
    sum_ref2 = 0
    sum_err2 = 0
    sum_err = 0
    max_abs = 0
    for ref, out in zip(reference, decoded):
        error = out - ref
        sum_ref2 += ref * ref
        sum_err2 += error * error
        sum_err += error
        if abs(error) > max_abs:
            max_abs = abs(error)
    mse = sum_err2 / count
    snr = 99.0 if sum_err2 == 0 else 10.0 * math.log10(sum_ref2 / sum_err2)
    return {
        "mse": round(mse, 4),
        "rmse": round(math.sqrt(mse), 4),
        "max_abs_error": max_abs,
        "mean_error": round(sum_err / count, 4),
        "snr_db": round(snr, 3),
        "peak_abs_reference": max(abs(value) for value in reference),
        "decoded_first_error": decoded[0] - reference[0],
        "decoded_last_error": decoded[-1] - reference[-1],
    }


def make_wav(pcm):
    size = len(pcm)
    return b"".join([
        b"RIFF", struct.pack("<I", 36 + size), b"WAVE",
        b"fmt ", struct.pack("<IHHIIHH", 16, 1, 1, SAMPLE_RATE,
                             SAMPLE_RATE * 2, 2, 16),
        b"data", struct.pack("<I", size),
    ]) + pcm


def cue_symbol(cue_id):
    return "audio_" + cue_id.removeprefix("sh_")


def load_reference(cue):
    pcm_path = REFERENCE_DIR / (cue["id"] + ".pcm")
    if not pcm_path.is_file():
        raise SystemExit(f"gen-audio-adpcm: missing {pcm_path}; run gen-audio-ref")
    raw = pcm_path.read_bytes()
    if len(raw) != cue["pcm_bytes"]:
        raise SystemExit(f"gen-audio-adpcm: {cue['id']} PCM size changed")
    if sha256_file(pcm_path) != cue["pcm_sha256"]:
        raise SystemExit(f"gen-audio-adpcm: {cue['id']} PCM hash changed")
    samples = list(struct.unpack("<%dh" % (len(raw) // 2), raw))
    if len(samples) != cue["loop_samples"]:
        raise SystemExit(f"gen-audio-adpcm: {cue['id']} sample count changed")
    return raw, samples


def run():
    reference_manifest_hash = sha256_file(REFERENCE_MANIFEST)
    reference = json.loads(REFERENCE_MANIFEST.read_text())
    if reference.get("format") != "sub-hauler-reference-audio-v1":
        raise SystemExit("gen-audio-adpcm: unexpected reference manifest")
    if reference.get("sample_rate") != SAMPLE_RATE:
        raise SystemExit("gen-audio-adpcm: reference rate is not 8 kHz")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    LISTEN_DIR.mkdir(parents=True, exist_ok=True)
    cues = []
    total_samples = 0
    total_bytes = 0
    for cue in reference["cues"]:
        raw_pcm, samples = load_reference(cue)
        encoded, decoded, predictor, index = encode_pcm(samples)
        if len(encoded) * 2 != len(samples):
            raise SystemExit(f"gen-audio-adpcm: {cue['id']} nibble count mismatch")
        if len(encoded) > 0xFFFFFF or len(samples) > 0xFFFFFF:
            raise SystemExit(f"gen-audio-adpcm: {cue['id']} exceeds 24-bit descriptor")
        if decoded[0] != samples[0]:
            raise SystemExit(f"gen-audio-adpcm: {cue['id']} loop start is not exact")
        if len(samples) & 1:
            raise SystemExit(f"gen-audio-adpcm: {cue['id']} odd loop sample count")
        metrics = quality_metrics(samples, decoded)
        metric_bytes = struct.pack("<%dh" % len(decoded), *decoded)
        bin_path = OUT_DIR / (cue["id"].removeprefix("sh_") + ".bin")
        bin_path.write_bytes(encoded)
        (LISTEN_DIR / (cue["id"] + ".pcm")).write_bytes(metric_bytes)
        (LISTEN_DIR / (cue["id"] + ".wav")).write_bytes(make_wav(metric_bytes))
        record = {
            "id": cue["id"],
            "symbol": cue_symbol(cue["id"]),
            "reference_pcm_sha256": cue["pcm_sha256"],
            "samples": len(samples),
            "bytes": len(encoded),
            "initial_predictor": predictor,
            "initial_index": index,
            "encoded_sha256": hashlib.sha256(encoded).hexdigest(),
            "decoded_pcm_sha256": hashlib.sha256(metric_bytes).hexdigest(),
            "check_crc16_first128": crc16_128(decoded),
            "quality": metrics,
        }
        cues.append(record)
        total_samples += len(samples)
        total_bytes += len(encoded)
        print(f"gen-audio-adpcm: {cue['id']}: {len(encoded)} B, "
              f"{len(samples)} samples, SNR {metrics['snr_db']:.2f} dB, "
              f"max |e| {metrics['max_abs_error']}")

    manifest = {
        "format": "sub-hauler-adpcm-v1",
        "codec": "IMA/DVI ADPCM, 4-bit, blockless, low nibble first",
        "channels": 1,
        "sample_rate": SAMPLE_RATE,
        "sample_format": "s16le",
        "reference_manifest_sha256": reference_manifest_hash,
        "generator_sha256": sha256_file(Path(__file__)),
        "initial_state": "predictor = first reference sample, step index = 0",
        "loop_reset": "predictor/index reset to initial state at the exact authored sample boundary",
        "total_samples": total_samples,
        "total_seconds": round(total_samples / SAMPLE_RATE, 6),
        "total_bytes": total_bytes,
        "cues": cues,
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n")

    lines = [
        "// Generated by tools/gen_audio_adpcm.py. Do not edit.",
        "#pragma once",
        "#include <stdint.h>",
        "#if defined(__AVR__)",
        "#include <avr/pgmspace.h>",
        "#define SH_AUDIO_DATA PROGMEM",
        "#else",
        "#define SH_AUDIO_DATA",
        "#endif",
        '#include "../fxdata.h"',
        "",
        "namespace audio {",
        "struct Cue {",
        "    uint24_t base;",
        "    uint24_t samples;",
        "};",
        f"constexpr uint8_t kCueCount = {len(cues)};",
        "constexpr uint24_t kBytes[kCueCount] = {",
        "    " + ", ".join(f"{cue['bytes']}u" for cue in cues) + ",",
        "};",
        "constexpr uint24_t kSamples[kCueCount] = {",
        "    " + ", ".join(f"{cue['samples']}u" for cue in cues) + ",",
        "};",
        f"constexpr int16_t kInitialPredictor = {cues[0]['initial_predictor']};",
        f"constexpr uint8_t kInitialIndex = {cues[0]['initial_index']};",
        "static const Cue kCues[kCueCount] SH_AUDIO_DATA = {",
    ]
    for cue in cues:
        if (cue['initial_predictor'] != cues[0]['initial_predictor'] or
                cue['initial_index'] != cues[0]['initial_index']):
            raise SystemExit("gen-audio-adpcm: per-cue initial state differs")
        lines.append(f"    {{ {cue['symbol']}, {cue['samples']}u }},")
    lines += [
        "};",
        f"static const uint16_t kCheckCrc[kCueCount] SH_AUDIO_DATA = {{",
        "    " + ", ".join(f"0x{cue['check_crc16_first128']:04X}u" for cue in cues) + ",",
        "};",
        "} // namespace audio",
        "#undef SH_AUDIO_DATA",
        "",
    ]
    HEADER.write_text("\n".join(lines))

    print(f"gen-audio-adpcm: {len(cues)} cues, {total_bytes} B, "
          f"{total_samples} samples, {manifest['total_seconds']:.2f} s")
    print(f"gen-audio-adpcm: manifest {MANIFEST.relative_to(ROOT)}")
    print(f"gen-audio-adpcm: header {HEADER.relative_to(ROOT)}")


def check():
    data = json.loads(MANIFEST.read_text())
    if data.get("format") != "sub-hauler-adpcm-v1":
        raise SystemExit("gen-audio-adpcm: manifest format mismatch")
    for cue in data["cues"]:
        path = OUT_DIR / (cue["id"].removeprefix("sh_") + ".bin")
        if not path.is_file():
            raise SystemExit(f"gen-audio-adpcm: missing {path}")
        if sha256_file(path) != cue["encoded_sha256"]:
            raise SystemExit(f"gen-audio-adpcm: {cue['id']} encoded hash mismatch")
        if len(path.read_bytes()) != cue["bytes"]:
            raise SystemExit(f"gen-audio-adpcm: {cue['id']} encoded size mismatch")
    print(f"gen-audio-adpcm: manifest valid, {len(data['cues'])} cues, "
          f"{data['total_bytes']} B")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.check:
        check()
    else:
        run()


if __name__ == "__main__":
    main()

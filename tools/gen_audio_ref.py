#!/usr/bin/env python3
"""Generate deterministic offline reference audio for the six cues.

Pipeline: real AVR ATMlib2FX synth -> Ardens absim (50 kHz PWM capture) ->
fixed-point 192-tap polyphase low-pass -> mono 8 kHz s16le reference PCM/WAV.

The 50 kHz capture and resampled PCM are build outputs. The compact provenance
manifest is written to fxdata/music/reference-manifest.json for gen-check.
No game firmware or player code is involved.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/audio-reference"
MANIFEST = ROOT / "fxdata/music/reference-manifest.json"
SOURCE_MANIFEST = ROOT / "fxdata/music/manifest.json"
CAPTURE_RATE = 50000
OUTPUT_RATE = 8000
GUARD_SECONDS = 0.05
DIAG_MAGIC = 0x52454631
DIAG_FORMAT = "<IHHHHBBBBBB2x"


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def find_root(env_name, candidates, marker):
    explicit = os.environ.get(env_name)
    if explicit:
        path = Path(explicit).expanduser().resolve()
        if not (path / marker).is_file():
            raise SystemExit(f"gen-audio-ref: {env_name}={path} lacks {marker}")
        return path
    for candidate in candidates:
        path = Path(candidate).expanduser()
        if (path / marker).is_file():
            return path.resolve()
    raise SystemExit(f"gen-audio-ref: set {env_name} to the checkout root")


def find_ardens():
    home = Path.home()
    return find_root(
        "ARDENS_ROOT",
        [ROOT.parent / "Ardens", home / "code/arduboy/Ardens", home / "code/Ardens"],
        "src/absim.hpp",
    )


def find_atmlib2fx():
    home = Path.home()
    return find_root(
        "ATMLIB2FX_ROOT",
        [ROOT.parent / "ATMlib2FX", home / "code/arduboy/ATMlib2FX"],
        "src/atm_synth.c",
    )


def find_avr_tools():
    candidates = []
    explicit = os.environ.get("AVR_GCC_ROOT")
    if explicit:
        candidates.append(Path(explicit).expanduser())
    arduino = Path.home() / "Library/Arduino15/packages/arduino/tools/avr-gcc"
    if arduino.is_dir():
        candidates.extend(sorted(arduino.glob("*/bin"), reverse=True))
    for name in ("avr-g++",):
        found = shutil.which(name)
        if found:
            candidates.append(Path(found).resolve().parent)
    for path in candidates:
        if (path / "avr-g++").is_file() and (path / "avr-nm").is_file():
            return path.resolve()
    raise SystemExit("gen-audio-ref: set AVR_GCC_ROOT to the avr-gcc bin directory")


def run(command, cwd=None):
    result = subprocess.run(
        [str(item) for item in command], cwd=cwd, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if result.returncode:
        sys.stderr.write("command failed: " + " ".join(str(item) for item in command) + "\n")
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    return result.stdout.strip()


def version_line(tool):
    return run([tool, "--version"]).splitlines()[0]


def git_state(path):
    head = run(["git", "-C", str(path), "rev-parse", "HEAD"])
    dirty = bool(run(["git", "-C", str(path), "status", "--porcelain", "--untracked-files=no"]))
    return {"head": head, "dirty": dirty}


def tree_hashes(path, relative_paths):
    return {rel: sha256_file(path / rel) for rel in relative_paths}


def coefficient_digest():
    header = ROOT / "tools/audio_reference/reference_pcm.hpp"
    return sha256_file(header)


def capture_plan(total_ticks, tick_rate):
    """Return exact 50 kHz capture and 8 kHz output counts for one loop.

    A cue is warmed up for one full intro cycle and then one loop-region cycle
    is captured. The guard is extra source samples on both sides of the loop
    for the polyphase filter.
    """
    loop_raw_numerator = total_ticks * CAPTURE_RATE
    loop_out_numerator = total_ticks * OUTPUT_RATE
    if loop_raw_numerator % tick_rate or loop_out_numerator % tick_rate:
        raise ValueError("loop does not map to integer samples")
    loop_raw = loop_raw_numerator // tick_rate
    loop_out = loop_out_numerator // tick_rate
    guard = int(round(GUARD_SECONDS * CAPTURE_RATE))
    return {
        "warmup_raw": loop_raw,
        "loop_raw": loop_raw,
        "guard_raw": guard,
        "capture_raw": loop_raw * 2 + guard,
        "out_samples": loop_out,
    }


def capture_step_budget(samples, margin_ms=2000):
    """1 ms capture steps needed for a sample target, plus a bounded margin."""
    return ((samples + CAPTURE_RATE - 1) // CAPTURE_RATE) * 1000 + margin_ms


def stamp_for(parts):
    return sha256_bytes(json.dumps(parts, sort_keys=True).encode())


def ensure_miniz_export(build_dir):
    include = build_dir / "include"
    include.mkdir(parents=True, exist_ok=True)
    header = include / "miniz_export.h"
    header.write_text(
        "#pragma once\n"
        "#define MINIZ_EXPORT\n"
        "#define MINIZ_NO_EXPORT\n"
        "#define MINIZ_DEPRECATED\n"
        "#define MINIZ_DEPRECATED_EXPORT\n"
        "#define MINIZ_DEPRECATED_NO_EXPORT\n"
    )
    return include


def absim_sources(ardens):
    src = ardens / "src"
    deps = ardens / "deps"
    sources = [
        src / "absim_cpu_data.cpp",
        src / "absim_arduboy.cpp",
        src / "absim_decode.cpp",
        src / "absim_merge_instrs.cpp",
        src / "absim_execute.cpp",
        src / "absim_disassemble.cpp",
        src / "absim_load_file.cpp",
        src / "absim_reset.cpp",
        src / "absim_snapshot.cpp",
        src / "absim_led.cpp",
    ]
    sources += sorted((src / "boot").glob("*.c"))
    sources += [
        deps / "miniz/miniz.c",
        deps / "miniz/miniz_zip.c",
        deps / "miniz/miniz_tdef.c",
        deps / "miniz/miniz_tinfl.c",
        deps / "yyjson/yyjson.c",
    ]
    return sources


def build_capture_tool(ardens, build_dir, force=False):
    cxx = os.environ.get("CXX", "c++")
    cc = os.environ.get("CC", "cc")
    cxx_version = version_line(cxx)
    cc_version = version_line(cc)
    tool_header = ROOT / "tools/audio_reference/reference_pcm.hpp"
    sources = absim_sources(ardens) + [ROOT / "tools/audio_reference/capture.cpp"]
    inputs = {
        "sources": {str(path): sha256_file(path) for path in sources + [tool_header]},
        "cxx": [cxx, cxx_version],
        "cc": [cc, cc_version],
        "includes": str(ardens / "src"),
    }
    stamp = stamp_for(inputs)
    native = build_dir / "native"
    binary = native / "capture"
    stamp_file = native / "stamp"
    if not force and binary.is_file() and stamp_file.is_file() and \
            stamp_file.read_text().strip() == stamp:
        return binary, {"cxx": cxx_version, "cc": cc_version}

    if native.exists():
        shutil.rmtree(native)
    objects = native / "obj"
    objects.mkdir(parents=True)
    include = ensure_miniz_export(native)
    common = ["-O2", "-DNDEBUG", "-DARDENS_NO_DEBUGGER", "-DARDENS_NO_SNAPSHOTS",
              "-DARDENS_VERSION_MAJOR=0", "-DARDENS_VERSION_MINOR=0",
              "-DARDENS_VERSION_PATCH=0", "-DMINIZ_STATIC_DEFINE",
              "-I", str(ardens / "src"), "-I", str(ardens / "deps"),
              "-I", str(ardens / "deps/bitsery/include"),
              "-I", str(ardens / "deps/miniz"), "-I", str(include)]
    obj_paths = []
    for source in sources:
        obj = objects / (source.stem + ".o")
        if source.suffix == ".c":
            run([cc, "-std=c11", *common, "-c", str(source), "-o", str(obj)])
        else:
            run([cxx, "-std=c++17", *common, "-c", str(source), "-o", str(obj)])
        obj_paths.append(str(obj))
    run([cxx, "-O2", *obj_paths, "-o", str(binary), "-lm"])
    stamp_file.write_text(stamp + "\n")
    return binary, {"cxx": cxx_version, "cc": cc_version}


def write_stream_header(path, data):
    lines = ["#pragma once", "#include <stdint.h>", "#include <avr/pgmspace.h>",
             f"static const uint32_t STREAM_LENGTH = {len(data)};",
             "static const uint8_t STREAM_DATA[] PROGMEM = {"]
    for i in range(0, len(data), 16):
        lines.append(",".join(str(byte) for byte in data[i:i + 16]) + ",")
    lines.append("};")
    path.write_text("\n".join(lines) + "\n")


def build_reference_image(atmlib, avr, work_dir, stream_data):
    work_dir.mkdir(parents=True, exist_ok=True)
    write_stream_header(work_dir / "stream_data.h", stream_data)
    src = atmlib / "src"
    defines = ["-DATM_FX_COMPACT_RING=64", "-DATM_FX_COMPACT_LOOP=1",
               "-DATM_FX_COMPACT_LOOP_STREAMING=1", "-DATM_FX_COMPACT_DIAG=1"]
    common = ["-mmcu=atmega32u4", "-DF_CPU=16000000UL", "-Os",
              "-ffunction-sections", "-fdata-sections", *defines,
              "-I", str(work_dir), "-I", str(src)]
    objects = []
    for name in ("ref_synth.cpp",):
        obj = work_dir / (Path(name).stem + ".o")
        run([avr / "avr-g++", "-std=gnu++11", "-fno-exceptions",
             "-fno-threadsafe-statics", "-g0", *common,
             "-c", str(ROOT / "tools/audio_reference/avr" / name), "-o", str(obj)])
        objects.append(obj)
    for name in ("osc.c", "atm_synth.c"):
        obj = work_dir / (Path(name).stem + ".o")
        run([avr / "avr-gcc", "-std=gnu11", "-g0", *common,
             "-c", str(src / name), "-o", str(obj)])
        objects.append(obj)
    elf = work_dir / "ref_synth.elf"
    hex_file = work_dir / "ref_synth.hex"
    run([avr / "avr-g++", "-mmcu=atmega32u4", "-Wl,--gc-sections", "-Wl,--relax",
         *[str(obj) for obj in objects], "-o", str(elf)])
    run([avr / "avr-objcopy", "-O", "ihex", "-R", ".eeprom", str(elf), str(hex_file)])
    nm = run([avr / "avr-nm", str(elf)])
    match = re.search(r"^([0-9a-fA-F]+)\s+[BbDd]\s+ref_diag$", nm, re.MULTILINE)
    if not match:
        raise SystemExit("gen-audio-ref: ref_diag symbol missing from reference ELF")
    address = int(match.group(1), 16)
    if address >= 0x800000:
        address -= 0x800000
    size = run([avr / "avr-size", "-A", str(elf)])
    text = int(re.search(r"^\.text\s+(\d+)", size, re.MULTILINE).group(1))
    data = int((re.search(r"^\.data\s+(\d+)", size, re.MULTILINE) or [0, "0"])[1])
    bss = int(re.search(r"^\.bss\s+(\d+)", size, re.MULTILINE).group(1))
    return {
        "hex": hex_file,
        "diag_addr": address,
        "sha256": sha256_file(hex_file),
        "elf_sha256": sha256_file(elf),
        "flash_bytes": text + data,
        "text_bytes": text,
        "data_bytes": data,
        "bss_bytes": bss,
    }


def parse_diag(hex_text):
    raw = bytes.fromhex(hex_text)
    if len(raw) != struct.calcsize(DIAG_FORMAT):
        raise ValueError("diag size mismatch")
    fields = struct.unpack(DIAG_FORMAT, raw)
    keys = ("magic", "ticks", "underruns", "max_gap", "stop_tick", "error",
            "active", "paused", "loopable", "loop_streaming", "loop_pinned")
    return dict(zip(keys, fields))


def make_wav(pcm):
    data_size = len(pcm)
    header = b"".join([
        b"RIFF", struct.pack("<I", 36 + data_size), b"WAVE",
        b"fmt ", struct.pack("<IHHIIHH", 16, 1, 1, OUTPUT_RATE, OUTPUT_RATE * 2, 2, 16),
        b"data", struct.pack("<I", data_size),
    ])
    return header + pcm


def run_cue(cue, tool, atmlib, avr, build_dir, provenance):
    song_id = cue["id"]
    stream_meta = cue["stream"]
    tick_rate = cue["clock"]["effective_tick_rate"]
    total_ticks = cue["total_ticks"]
    if stream_meta.get("loop_cycle_ticks") != total_ticks:
        raise SystemExit(f"gen-audio-ref: {song_id} loop cycle differs from total ticks")
    loop_raw = total_ticks * CAPTURE_RATE
    loop_out = total_ticks * OUTPUT_RATE
    if loop_raw % tick_rate or loop_out % tick_rate:
        raise SystemExit(f"gen-audio-ref: {song_id} does not map to integer samples")
    loop_raw //= tick_rate
    loop_out //= tick_rate
    guard = int(round(GUARD_SECONDS * CAPTURE_RATE))
    capture_raw = loop_raw * 2 + guard

    work = build_dir / "work" / song_id
    stream_path = ROOT / "fxdata/music" / f"{song_id}.bin"
    stream_data = stream_path.read_bytes()
    if sha256_bytes(stream_data) != cue["sha256"]:
        raise SystemExit(f"gen-audio-ref: {song_id} stream hash changed")
    image = build_reference_image(atmlib, avr, work, stream_data)

    pcm_path = build_dir / f"{song_id}.pcm"
    raw_pcm_path = build_dir / "raw" / f"{song_id}.pcm"
    raw_pcm_path.parent.mkdir(parents=True, exist_ok=True)
    command = [tool, "--hex", image["hex"], "--samples", str(capture_raw),
               "--diag-addr", str(image["diag_addr"]),
               "--pcm-out", str(pcm_path), "--raw-pcm-out", str(raw_pcm_path),
               "--start", str(loop_raw), "--count", str(loop_raw),
               "--out-samples", str(loop_out),
               "--gain-num", "32767", "--gain-den", "2000"]
    captured = json.loads(run(command))
    diag = parse_diag(captured["diag"])
    expected_ticks = round(capture_raw * tick_rate / CAPTURE_RATE)
    problems = []
    if captured["samples"] != capture_raw:
        problems.append(f"raw samples {captured['samples']} != {capture_raw}")
    if abs(captured["rate_delta"] - CAPTURE_RATE) > 2:
        problems.append(f"rate delta {captured['rate_delta']}")
    if abs(captured["cycle_delta"] - 16000000) > 20000:
        problems.append(f"cycles/s {captured['cycle_delta']}")
    if captured["nonzero"] < 1000 or captured["first_nonzero"] < 0 or captured["first_nonzero"] > 5000:
        problems.append("capture has no early non-zero audio")
    if captured["pcm_samples"] != loop_out:
        problems.append(f"pcm samples {captured['pcm_samples']} != {loop_out}")
    if captured["pcm_clipped"]:
        problems.append(f"calibration clipped {captured['pcm_clipped']} samples")
    if diag["magic"] != DIAG_MAGIC:
        problems.append("diag magic missing")
    if diag["error"] or diag["underruns"]:
        problems.append(f"decoder error={diag['error']} underruns={diag['underruns']}")
    if not diag["active"] or diag["paused"] or not diag["loopable"] or not diag["loop_streaming"]:
        problems.append(f"decoder state active={diag['active']} paused={diag['paused']} "
                        f"loopable={diag['loopable']} streaming={diag['loop_streaming']}")
    if abs(diag["ticks"] - expected_ticks) > 3:
        problems.append(f"ticks {diag['ticks']} != {expected_ticks}")
    if problems:
        raise SystemExit(f"gen-audio-ref: {song_id}: " + "; ".join(problems))

    pcm = pcm_path.read_bytes()
    raw_pcm = raw_pcm_path.read_bytes()
    wav = make_wav(pcm)
    wav_path = build_dir / f"{song_id}.wav"
    wav_path.write_bytes(wav)
    seconds = total_ticks / tick_rate
    return {
        "id": song_id,
        "name": cue["name"],
        "source": cue["source"],
        "source_sha256": cue["source_sha256"],
        "stream_sha256": cue["sha256"],
        "seconds": round(seconds, 6),
        "total_ticks": total_ticks,
        "tick_rate": tick_rate,
        "loop_samples": loop_out,
        "pcm_bytes": len(pcm),
        "pcm_sha256": sha256_bytes(pcm),
        "wav_sha256": sha256_bytes(wav),
        "peak_min": captured["pcm_min"],
        "peak_max": captured["pcm_max"],
        "dc_mean": round(captured["pcm_sum"] / loop_out, 6),
        "clipped_samples": captured["pcm_clipped"],
        "raw_pcm_sha256": sha256_bytes(raw_pcm),
        "raw_pcm_bytes": len(raw_pcm),
        "raw_peak_min": captured["raw_pcm_min"],
        "raw_peak_max": captured["raw_pcm_max"],
        "warmup_samples_50k": loop_raw,
        "guard_samples_50k": guard,
        "capture_samples_50k": capture_raw,
        "diag": diag,
        "image": {
            "hex_sha256": image["sha256"],
            "elf_sha256": image["elf_sha256"],
            "text_bytes": image["text_bytes"],
            "bss_bytes": image["bss_bytes"],
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true",
                        help="validate the existing manifest without regenerating")
    parser.add_argument("--force", action="store_true",
                        default=os.environ.get("AUDIO_REFERENCE_FORCE") == "1",
                        help="rebuild the capture tool and re-run every cue")
    args = parser.parse_args()
    if args.check:
        data = json.loads(MANIFEST.read_text())
        if data.get("format") != "sub-hauler-reference-audio-v1":
            raise SystemExit("gen-audio-ref: manifest format mismatch")
        print(f"audio-reference: manifest valid, {len(data['cues'])} cues")
        return

    atmlib = find_atmlib2fx()
    ardens = find_ardens()
    avr = find_avr_tools()
    BUILD.mkdir(parents=True, exist_ok=True)
    source_manifest = json.loads(SOURCE_MANIFEST.read_text())
    tool, host_tool = build_capture_tool(ardens, BUILD, force=args.force)

    provenance = {
        "atmllib2fx": {
            **git_state(atmlib),
            "sources": tree_hashes(atmlib, [
                "src/osc.c", "src/osc.h", "src/atm_synth.c", "src/atm_synth.h",
                "src/cmd_parse.c", "src/atm_cmd_constants.h",
                "src/atm_fx_compact.c", "src/atm_fx_compact.h",
            ]),
        },
        "ardens": {
            **git_state(ardens),
            "sources": tree_hashes(ardens, [
                "src/absim.hpp", "src/absim_config.hpp", "src/absim_instructions.hpp",
                "src/absim_regs.hpp", "src/absim_pqueue.hpp", "src/absim_strstream.hpp",
                "src/absim_atmega32u4.hpp", "src/absim_display.hpp", "src/absim_w25q128.hpp",
                "src/absim_timer.hpp", "src/absim_adc.hpp", "src/absim_pll.hpp",
                "src/absim_spi.hpp", "src/absim_twi.hpp", "src/absim_eeprom.hpp",
                "src/absim_sound.hpp", "src/absim_usb.hpp",
                "src/absim_cpu_data.cpp", "src/absim_arduboy.cpp", "src/absim_decode.cpp",
                "src/absim_merge_instrs.cpp", "src/absim_execute.cpp",
                "src/absim_disassemble.cpp", "src/absim_load_file.cpp",
                "src/absim_reset.cpp", "src/absim_snapshot.cpp", "src/absim_led.cpp",
            ]),
        },
        "avr_gcc": {
            "version": version_line(avr / "avr-g++"),
            "sha256": sha256_file(avr / "avr-g++"),
        },
        "host_compiler": host_tool,
        "generator_sha256": sha256_file(Path(__file__)),
        "capture_tool": {
            "sha256": sha256_file(tool),
            "sources": {
                "tools/audio_reference/capture.cpp": sha256_file(
                    ROOT / "tools/audio_reference/capture.cpp"),
                "tools/audio_reference/reference_pcm.hpp": sha256_file(
                    ROOT / "tools/audio_reference/reference_pcm.hpp"),
            },
        },
        "capture_tool_sha256": sha256_file(tool),
        "coefficient_source_sha256": coefficient_digest(),
    }

    cues = []
    total_samples = 0
    for cue in source_manifest["songs"]:
        record = run_cue(cue, tool, atmlib, avr, BUILD, provenance)
        cues.append(record)
        total_samples += record["loop_samples"]
        print(f"audio-reference: {cue['id']}: {record['loop_samples']} samples, "
              f"{record['seconds']:.2f} s, peak {record['peak_min']}..{record['peak_max']}")

    manifest = {
        "format": "sub-hauler-reference-audio-v1",
        "channels": 1,
        "sample_format": "s16le",
        "sample_rate": OUTPUT_RATE,
        "source_rate": CAPTURE_RATE,
        "total_seconds": round(sum(c["seconds"] for c in cues), 6),
        "total_samples": total_samples,
        "render": {
            "engine": "AVR ATMlib2FX via Ardens absim",
            "warmup": "one full authored loop (intro) discarded",
            "capture": "one loop-region cycle",
            "guard_seconds": GUARD_SECONDS,
            "advance_ps": 1000000000,
        },
        "dsp": {
            "lowpass_hz": 3400,
            "taps": 192,
            "phases": 4,
            "coefficient_bits": 20,
            "polyphase_source_rate": CAPTURE_RATE,
            "output_rate": OUTPUT_RATE,
        },
        "calibration": {
            "emulator_pwm_full_scale": 2000,
            "gain_numerator": 32767,
            "gain_denominator": 2000,
            "rounding": "nearest, half away from zero",
            "clipping": "saturate to int16 [-32768, 32767]",
            "dc": "none: absim PWM is bipolar around zero; no per-cue centering",
            "canonical": "8 kHz s16le PCM and listening WAV use calibrated values",
            "raw": "build/audio-reference/raw/*.pcm keeps resampled emulator units",
            "inverse_pwm_units": "pwm = round(ref * 2000 / 32767)",
            "pwm8_to_playback": "pwm8 = clamp(128 + (ref >> 8), 0, 255)",
            "per_cue_normalization": False,
        },
        "provenance": provenance,
        "cues": cues,
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"audio-reference: {len(cues)} cues, {total_samples} samples, "
          f"{manifest['total_seconds']:.2f} s -> {MANIFEST.relative_to(ROOT)}")


if __name__ == "__main__":
    main()

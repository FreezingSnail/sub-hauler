#!/usr/bin/env python3
"""Ardens save-file hash migration.

Ardens keys each save file by an FNV-1a hash of the program flash and the
16 MiB FX chip image. Rebuilding the game or regenerating the cart changes that
key. `migrate` copies an emulator save under the new key and rewrites only the
embedded 64-bit hash, leaving the save payload and the source file untouched.

This is a container-key copy, not a payload converter or validator. The source
save must already contain a record whose payload is exactly compatible with the
target build. The `sub-hauler-bwg` pre-release cleanup changed the raw save
format and removed old-save compatibility, so development saves from the former
schema/header era are discarded and must be recreated rather than migrated.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ARDENS_DATA_BYTES = 4096 * 4096
ARDENS_BOOT_BYTES = 1536
ARDENS_PROGRAM_BYTES = 32 * 1024 - 3 * 1024
ARDENS_FX_MARKER = b"ARDUBOY"
PAGE_BYTES = 256
SAVE_NAME = "absim_{:016x}.save"
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
FNV_MASK = 0xFFFFFFFFFFFFFFFF


class SaveError(Exception):
    pass


def round_up(value: int, align: int) -> int:
    return (value + align - 1) // align * align


def parse_ihex(text: str) -> bytes:
    """Parse an Intel HEX image into Ardens' zero-filled 32 KiB program."""
    program = bytearray(32 * 1024)
    upper = 0
    records = 0
    for line_number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line.startswith(":"):
            continue
        try:
            count = int(line[1:3], 16)
            address = int(line[3:7], 16)
            record_type = int(line[7:9], 16)
            data = bytes.fromhex(line[9 : 9 + 2 * count])
        except ValueError as exc:
            raise SaveError(f"line {line_number}: malformed Intel HEX record") from exc
        if len(data) != count:
            raise SaveError(f"line {line_number}: short Intel HEX record")
        records += 1
        if record_type == 0:
            start = address + (upper << 16)
            end = start + count
            if end > len(program):
                raise SaveError(f"line {line_number}: program address out of range")
            program[start:end] = data
        elif record_type == 1:
            break
        elif record_type == 3:
            continue
        elif record_type == 4:
            if count != 2:
                raise SaveError(f"line {line_number}: malformed extended address")
            upper = (data[0] << 8) | data[1]
        else:
            raise SaveError(f"line {line_number}: unsupported Intel HEX record {record_type}")
    if records == 0:
        raise SaveError("no Intel HEX records")
    return bytes(program)


def fnv1a(data) -> int:
    value = FNV_OFFSET
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & FNV_MASK
    return value


def game_hash(program: bytes, fxdata: bytes, data_bytes: int = ARDENS_DATA_BYTES) -> int:
    """Return Ardens' save key for a program image and an FX data image."""
    if len(program) < ARDENS_PROGRAM_BYTES:
        raise SaveError("program image is too small")
    if len(fxdata) > data_bytes:
        raise SaveError("FX data image is larger than the FX chip")
    value = fnv1a(program[:ARDENS_PROGRAM_BYTES])
    for byte in ARDENS_FX_MARKER:
        value ^= byte
        value = (value * FNV_PRIME) & FNV_MASK
    for _ in range(7, ARDENS_BOOT_BYTES):
        value ^= 0xFF
        value = (value * FNV_PRIME) & FNV_MASK
    fx = bytearray(b"\xFF" * data_bytes)
    offset = data_bytes - round_up(len(fxdata), PAGE_BYTES)
    fx[offset : offset + len(fxdata)] = fxdata
    for byte in fx[ARDENS_BOOT_BYTES:]:
        value ^= byte
        value = (value * FNV_PRIME) & FNV_MASK
    return value


def read_hash(save: bytes) -> int:
    if len(save) < 8:
        raise SaveError("save file is shorter than its hash field")
    return struct.unpack_from("<Q", save)[0]


def rewrite_hash(save: bytes, target_hash: int) -> bytes:
    if len(save) < 8:
        raise SaveError("save file is shorter than its hash field")
    return struct.pack("<Q", target_hash) + save[8:]


def migrate_save(source: Path, directory: Path, target_hash: int, force: bool = False) -> Path:
    save = source.read_bytes()
    source_hash = read_hash(save)
    if source_hash == target_hash:
        return source
    target = directory / SAVE_NAME.format(target_hash)
    if target.exists() and not force:
        raise SaveError(f"target save already exists: {target} (use --force to replace)")
    target.write_bytes(rewrite_hash(save, target_hash))
    return target


def target_hash(hex_path: Path, fxdata_path: Path) -> int:
    program = parse_ihex(hex_path.read_text())
    return game_hash(program, fxdata_path.read_bytes())


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    hash_parser = sub.add_parser("hash", help="print the Ardens game hash")
    hash_parser.add_argument("--hex", required=True, type=Path)
    hash_parser.add_argument("--fxdata", required=True, type=Path)

    migrate_parser = sub.add_parser(
        "migrate",
        help="copy an Ardens save to the current game hash (payload must already match)",
    )
    migrate_parser.add_argument(
        "--save",
        required=True,
        type=Path,
        help="source save whose payload already matches the target build",
    )
    migrate_parser.add_argument("--hex", required=True, type=Path)
    migrate_parser.add_argument("--fxdata", required=True, type=Path)
    migrate_parser.add_argument("--force", action="store_true")

    args = parser.parse_args(argv)
    try:
        if args.command == "hash":
            print(f"{target_hash(args.hex, args.fxdata):016x}")
            return 0
        digest = target_hash(args.hex, args.fxdata)
        destination = migrate_save(args.save, args.save.parent, digest, args.force)
        print(f"ardens-save: {args.save} -> {destination} (hash {digest:016x})")
        return 0
    except (OSError, SaveError) as exc:
        print(f"ardens-save: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

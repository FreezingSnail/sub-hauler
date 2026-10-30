#!/usr/bin/env python3
"""Pack the finalized seven story-beat decks for the FX cart.

Source of truth is ``docs/story-sketchbook.html`` plus the native 128x64 poses
in ``docs/assets/first-beat-animation/all-beats-v3``. Every beat alternates an
animated image card (four poses at 3.3 fps) with a static story-text card. The
FIRST beat has three image/text pairs; the other six beats have two.

The cart is nearly full, so each 1,024-byte page-major frame is run-length
encoded. The blob starts with one little-endian uint16 offset per frame,
relative to the blob base. No frame header is needed: card order always
alternates image/text starting with an image, so the device derives the first
frame from the global card index.

Text cards are baked with the same 5x7 glyphs, 20-column wrap, six-row limit,
10 px leading and centring used by the sketchbook preview. That keeps the game
pixel-identical to the approved preview and avoids shipping a second layout
engine.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
HTML = ROOT / "docs/story-sketchbook.html"
IMAGES = ROOT / "docs/assets/first-beat-animation/all-beats-v3"
OUTPUT = ROOT / "fxdata/tables/story-beats.bin"
HEADER = ROOT / "src/generated/story_beats.hpp"

WIDTH = 128
HEIGHT = 64
FRAME_BYTES = WIDTH * HEIGHT // 8
RLE_PAD = 32
BEAT_BLOCKS = (
    "pumpFrames",
    "deliveryFrames",
    "liftFrames",
    "liftDeliveryFrames",
    "beaconFrames",
    "homeFrames",
)
BEAT_NAMES = (
    "FIRST",
    "PUMP SURVEY",
    "PUMP",
    "LIFT SURVEY",
    "LIFT",
    "BEACON",
    "HOME",
)
STRING_RE = re.compile(r"'((?:[^'\\]|\\.)*)'")


def decode_js_string(value: str) -> str:
    return value.replace("\\n", "\n").replace("\\t", "\t").replace("\\'", "'").replace("\\\\", "\\")


def parse_story(html: str):
    glyph_match = re.search(r"const glyphs=(\{.*?\});", html, re.S)
    if not glyph_match:
        raise ValueError("story-sketchbook.html: glyph table not found")
    glyphs = {key: list(value) for key, value in json.loads(glyph_match.group(1)).items()}

    text_match = re.search(r"const storyText=\[(.*?)\];", html, re.S)
    if not text_match:
        raise ValueError("story-sketchbook.html: first-beat storyText not found")
    first_text = [decode_js_string(value) for value in STRING_RE.findall(text_match.group(1))]
    if len(first_text) != 3:
        raise ValueError("story-sketchbook.html: expected three first-beat text cards")

    images_match = re.search(r"const images=\[(.*?)\];", html, re.S)
    if not images_match:
        raise ValueError("story-sketchbook.html: first-beat images not found")
    first_images = re.findall(r"art:'([^']+)'", images_match.group(1))
    if len(first_images) != 3:
        raise ValueError("story-sketchbook.html: expected three first-beat images")

    decks = []
    first_deck = []
    for image_art, copy in zip(first_images, first_text):
        first_deck.append({"kind": "image", "art": image_art})
        first_deck.append({"kind": "text", "copy": copy})
    decks.append(first_deck)

    for block_name in BEAT_BLOCKS:
        block = re.search(r"const " + block_name + r"=\[(.*?)\];", html, re.S)
        if not block:
            raise ValueError("story-sketchbook.html: {} not found".format(block_name))
        entries = re.findall(r"\{(.*?)\}", block.group(1), re.S)
        deck = []
        for entry in entries:
            if "kind:'image'" in entry:
                art = re.search(r"art:'([^']+)'", entry)
                if not art:
                    raise ValueError("{}: image card has no art name".format(block_name))
                deck.append({"kind": "image", "art": art.group(1)})
            elif "kind:'text'" in entry:
                copy = re.search(r"copy:'((?:[^'\\]|\\.)*)'", entry, re.S)
                if not copy:
                    raise ValueError("{}: text card has no copy".format(block_name))
                deck.append({"kind": "text", "copy": decode_js_string(copy.group(1))})
            else:
                raise ValueError("{}: unknown card kind".format(block_name))
        if len(deck) != 4 or any(deck[index]["kind"] != ("image" if index % 2 == 0 else "text") for index in range(4)):
            raise ValueError("{}: deck must be image/text/image/text".format(block_name))
        decks.append(deck)

    if len(decks) != len(BEAT_NAMES):
        raise ValueError("expected seven story-beat decks")
    return glyphs, decks


def text_lines(copy: str):
    """Mirror the sketchbook's 20-column, six-row text layout."""
    rows = []
    cleaned = re.sub(r"[^A-Z0-9 .,:/\-\n]", "", copy.upper())
    for paragraph in cleaned.split("\n"):
        row = ""
        for word in paragraph.split():
            while len(word) > 20:
                if row:
                    rows.append(row)
                    row = ""
                rows.append(word[:20])
                word = word[20:]
            if row and len(row) + 1 + len(word) > 20:
                rows.append(row)
                row = ""
            if word:
                row += (" " if row else "") + word
        rows.append(row)
    return rows


def text_frame(copy: str, glyphs) -> bytes:
    rows = text_lines(copy)[:6]
    pixels = bytearray(FRAME_BYTES)
    top = (HEIGHT - ((len(rows) - 1) * 10 + 7)) // 2
    for line, row in enumerate(rows):
        left = (WIDTH - (len(row) * 6 - 1)) // 2
        for index, char in enumerate(row):
            for column, bits in enumerate(glyphs.get(char, glyphs[" "])):
                for y in range(7):
                    if bits & (1 << y):
                        pixel_y = top + line * 10 + y
                        pixel_x = left + index * 6 + column
                        if 0 <= pixel_y < HEIGHT and 0 <= pixel_x < WIDTH:
                            pixels[(pixel_y >> 3) * WIDTH + pixel_x] |= 1 << (pixel_y & 7)
    return bytes(pixels)


def image_frame(path: Path) -> bytes:
    image = Image.open(path).convert("RGB")
    if image.size != (WIDTH, HEIGHT):
        raise ValueError("{}: expected 128x64, found {}".format(path, image.size))
    source = image.load()
    pixels = bytearray(FRAME_BYTES)
    for y in range(0, HEIGHT, 8):
        for x in range(WIDTH):
            value = 0
            for bit in range(8):
                if source[x, y + bit][1] > 64:
                    value |= 1 << bit
            pixels[(y >> 3) * WIDTH + x] = value
    return bytes(pixels)


def rle_encode(data: bytes) -> bytes:
    """Run/literal stream: high bit set means (run-1) copies of the next byte."""
    out = bytearray()
    index = 0
    while index < len(data):
        end = index + 1
        while end < len(data) and data[end] == data[index] and end - index < 128:
            end += 1
        run = end - index
        if run >= 3:
            out.append(0x80 | (run - 1))
            out.append(data[index])
            index = end
            continue
        literal = bytearray()
        while index < len(data) and len(literal) < 128:
            if index + 2 < len(data) and data[index] == data[index + 1] == data[index + 2]:
                break
            literal.append(data[index])
            index += 1
        out.append(len(literal) - 1)
        out.extend(literal)
    return bytes(out)


def rle_decode(data: bytes) -> bytes:
    out = bytearray()
    index = 0
    while index < len(data):
        token = data[index]
        index += 1
        if token & 0x80:
            value = data[index]
            index += 1
            out.extend(bytes([value]) * ((token & 0x7F) + 1))
        else:
            count = token + 1
            out.extend(data[index:index + count])
            index += count
    return bytes(out)


def build_blob(decks, glyphs, images_root: Path) -> bytes:
    frames = []
    for beat_index, deck in enumerate(decks):
        for card_index, card in enumerate(deck):
            if card["kind"] == "image":
                for phase in range(4):
                    path = images_root / "{}-{}.png".format(card["art"], phase)
                    if not path.is_file():
                        raise ValueError("missing story pose {}".format(path))
                    frames.append(rle_encode(image_frame(path)))
            else:
                frames.append(rle_encode(text_frame(card["copy"], glyphs)))
    offsets = []
    position = len(frames) * 2
    for frame in frames:
        offsets.append(position)
        position += len(frame)
    if position >= 0x10000:
        raise ValueError("story-beat blob does not fit the uint16 offset table")
    blob = bytearray()
    for offset in offsets:
        blob += struct.pack("<H", offset)
    for frame in frames:
        blob += frame
    blob += b"\0" * RLE_PAD
    return bytes(blob)


def write_header(path: Path, cards: int, images: int, frames: int, size: int) -> None:
    path.write_text(
        "// Generated by tools/gen_story_beats.py. Do not edit.\n"
        "#pragma once\n\n"
        "#include <stdint.h>\n\n"
        "constexpr uint8_t STORY_BEAT_WIDTH  = {};\n"
        "constexpr uint8_t STORY_BEAT_HEIGHT = {};\n"
        "constexpr uint8_t STORY_BEAT_CARDS  = {};\n"
        "constexpr uint8_t STORY_BEAT_IMAGES = {};\n"
        "constexpr uint8_t STORY_BEAT_FRAMES = {};\n"
        "constexpr uint16_t STORY_BEAT_BYTES = {};\n"
        "constexpr uint8_t STORY_BEAT_FRAME_TICKS = 18;\n"
        "constexpr uint8_t STORY_BEAT_CYCLE_TICKS = 72;\n"
        "constexpr uint8_t STORY_BEAT_MAX_CARDS = 6;\n".format(WIDTH, HEIGHT, cards, images, frames, size),
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--html", type=Path, default=HTML)
    parser.add_argument("--images", type=Path, default=IMAGES)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--header", type=Path, default=HEADER)
    args = parser.parse_args()

    glyphs, decks = parse_story(args.html.read_text(encoding="utf-8"))
    blob = build_blob(decks, glyphs, args.images)
    cards = sum(len(deck) for deck in decks)
    images = sum(1 for deck in decks for card in deck if card["kind"] == "image")
    frames = sum(4 if card["kind"] == "image" else 1 for deck in decks for card in deck)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(blob)
    write_header(args.header, cards, images, frames, len(blob))
    print("gen_story_beats: {} cards / {} images / {} frames -> {} B".format(cards, images, frames, len(blob)))


if __name__ == "__main__":
    main()

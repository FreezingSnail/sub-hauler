#!/usr/bin/env python3
"""Pack the editable story source into native 128x64 cards for the FX cart.

Canonical inputs are ``story/scenes.json`` (scene order, prose and sheet rows)
and ``story/art.png`` (one illustration per row, four 128x64 poses across).
The editable PNG is never overwritten by generation. Retained pixelgen bundles
are provenance, not active inputs. Prose wraps and automatically paginates.

Outputs:
  fxdata/tables/story-beats.bin
  fxdata/tables/story-meta.bin
  src/generated/story_beats.hpp
  tst/fxdatatest/story_frame_crc.hpp
  docs/assets/first-beat-animation/frames.js
"""

from __future__ import annotations

import argparse
import base64
import io
import json
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "story/scenes.json"
CARD_SHEET = ROOT / "docs/assets/story-cards.png"
CARD_MAP = ROOT / "docs/assets/story-cards.json"
OUTPUT = ROOT / "fxdata/tables/story-beats.bin"
META_OUTPUT = ROOT / "fxdata/tables/story-meta.bin"
HEADER = ROOT / "src/generated/story_beats.hpp"
PREVIEW = ROOT / "docs/assets/first-beat-animation/frames.js"

WIDTH = 128
HEIGHT = 64
FRAME_BYTES = WIDTH * HEIGHT // 8
ROWS_PER_CARD = 6
COLUMNS = 20
RLE_PAD = 32

# Packed scene IDs/order are frozen for save compatibility. JSON scenes and
# editable-sheet rows follow narrative order independently.
SCENE_ORDER = (
    ("first-result", "STORY_SCENE_FIRST_RESULT"),
    ("pump-survey", "STORY_SCENE_PUMP_SURVEY"),
    ("pump-result", "STORY_SCENE_PUMP_RESULT"),
    ("lift-survey", "STORY_SCENE_LIFT_SURVEY"),
    ("lift-result", "STORY_SCENE_LIFT_RESULT"),
    ("beacon-result", "STORY_SCENE_BEACON_RESULT"),
    ("home-result", "STORY_SCENE_HOME_RESULT"),
    ("first-departure", "STORY_SCENE_FIRST_DEPARTURE"),
    ("pump-pickup", "STORY_SCENE_PUMP_PICKUP"),
    ("lift-pickup", "STORY_SCENE_LIFT_PICKUP"),
    ("joint-missing", "STORY_SCENE_JOINT_MISSING"),
    ("joint-ready", "STORY_SCENE_JOINT_READY"),
    ("beacon-departure", "STORY_SCENE_BEACON_DEPARTURE"),
    ("home-departure", "STORY_SCENE_HOME_DEPARTURE"),
    # Appended late optional expedition result; earlier ids stay frozen.
    ("deep-relay-result", "STORY_SCENE_EXPEDITION_RESULT"),
    ("deep-relay-departure", "STORY_SCENE_EXPEDITION_DEPARTURE"),
    # Appended opening deck; shown once on New Voyage before pilot control.
    ("opening-benthos", "STORY_SCENE_OPENING"),
)
SCENE_IDS = {name: index + 1 for index, (name, _) in enumerate(SCENE_ORDER)}

# Existing 5x7 story font columns, plus an apostrophe. Keep this table in sync
# with the preview glyph rendering. The native text cards use these bytes.
GLYPHS = {
    " ": [0, 0, 0, 0, 0],
    "'": [0, 0, 3, 0, 0],
    "-": [8, 8, 8, 8, 8],
    "/": [32, 16, 8, 4, 2],
    "0": [62, 81, 73, 69, 62],
    "1": [0, 66, 127, 64, 0],
    "2": [114, 73, 73, 73, 70],
    "3": [33, 65, 73, 77, 51],
    "4": [24, 20, 18, 127, 16],
    "5": [39, 69, 69, 69, 57],
    "6": [60, 74, 73, 73, 49],
    "7": [65, 33, 17, 9, 7],
    "8": [54, 73, 73, 73, 54],
    "9": [70, 73, 73, 41, 30],
    ":": [0, 0, 20, 0, 0],
    "A": [124, 18, 17, 18, 124],
    "B": [127, 73, 73, 73, 54],
    "C": [62, 65, 65, 65, 34],
    "D": [127, 65, 65, 65, 62],
    "E": [127, 73, 73, 73, 65],
    "F": [127, 9, 9, 9, 1],
    "G": [62, 65, 65, 81, 115],
    "H": [127, 8, 8, 8, 127],
    "I": [0, 65, 127, 65, 0],
    "J": [32, 64, 65, 63, 1],
    "K": [127, 8, 20, 34, 65],
    "L": [127, 64, 64, 64, 64],
    "M": [127, 2, 28, 2, 127],
    "N": [127, 4, 8, 16, 127],
    "O": [62, 65, 65, 65, 62],
    "P": [127, 9, 9, 9, 6],
    "Q": [62, 65, 81, 33, 94],
    "R": [127, 9, 25, 41, 70],
    "S": [38, 73, 73, 73, 50],
    "T": [3, 1, 127, 1, 3],
    "U": [63, 64, 64, 64, 63],
    "V": [31, 32, 64, 32, 31],
    "W": [63, 64, 56, 64, 63],
    "X": [99, 20, 8, 20, 99],
    "Y": [3, 4, 120, 4, 3],
    "Z": [97, 89, 73, 77, 67],
    ".": [0, 96, 96, 0, 0],
    ",": [0, 80, 48, 0, 0],
}
SUPPORTED = frozenset(GLYPHS)


class StoryError(ValueError):
    pass


def fail(path: Path, line, message: str) -> None:
    raise StoryError("{}:{}: {}".format(path, line, message))


def parse_source(text: str, path: Path = SOURCE):
    """Validate the canonical JSON, retaining prose as editable paragraphs."""
    def unique_keys(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise StoryError("{}: duplicate JSON key {!r}".format(path, key))
            result[key] = value
        return result

    try:
        document = json.loads(text, object_pairs_hook=unique_keys)
    except json.JSONDecodeError as error:
        fail(path, error.lineno, error.msg)

    def fields(value, expected, location):
        if not isinstance(value, dict) or set(value) != set(expected):
            fail(path, location, "expected fields {}".format(", ".join(expected)))

    fields(document, ("version", "art", "scenes"), "root")
    if type(document["version"]) is not int or document["version"] != 1:
        fail(path, "version", "expected story format version 1")
    art = document["art"]
    fields(art, ("file", "tile_width", "tile_height", "poses", "rows"), "art")
    if not isinstance(art["file"], str) or not art["file"]:
        fail(path, "art.file", "expected a sheet filename")
    if any(type(art[key]) is not int or art[key] != value
           for key, value in (("tile_width", WIDTH), ("tile_height", HEIGHT), ("poses", 4))):
        fail(path, "art", "expected four 128x64 poses per row")
    rows = art["rows"]
    if (not isinstance(rows, list) or not rows or
            any(not isinstance(name, str) or not name or name.strip() != name for name in rows)):
        fail(path, "art.rows", "expected nonempty image names")
    if len(set(rows)) != len(rows):
        fail(path, "art.rows", "duplicate image name")
    scenes = document["scenes"]
    if not isinstance(scenes, list):
        fail(path, "scenes", "expected a scene array")
    for scene in scenes:
        fields(scene, ("name", "title", "cards"), "scene")
        if not isinstance(scene["name"], str) or not isinstance(scene["title"], str) or not scene["title"].strip():
            fail(path, "scene", "expected a scene name and nonempty title")
        location = scene["name"]
        if not isinstance(scene["cards"], list) or not scene["cards"]:
            fail(path, location, "scene has no cards")
        for index, card in enumerate(scene["cards"]):
            where = "{}/card{}".format(location, index + 1)
            if not isinstance(card, dict):
                fail(path, where, "expected a card object")
            if card.get("kind") == "image":
                fields(card, ("kind", "art"), where)
                if not isinstance(card["art"], str) or card["art"] not in rows:
                    fail(path, where, "unknown sheet art {!r}".format(card["art"]))
            elif card.get("kind") == "text":
                fields(card, ("kind", "paragraphs"), where)
                paragraphs = card["paragraphs"]
                if (not isinstance(paragraphs, list) or not paragraphs or
                        any(not isinstance(paragraph, str) or not paragraph.strip() for paragraph in paragraphs)):
                    fail(path, where, "expected nonempty paragraph strings")
            else:
                fail(path, where, "unknown card kind")
    expected = [name for name, _ in SCENE_ORDER]
    names = [scene["name"] for scene in scenes]
    if len(names) != len(expected) or set(names) != set(expected):
        fail(path, "scenes", "scenes must contain each stable scene exactly once: {}".format(expected))
    return document


def load_sheet(document, source_path=SOURCE, image=None):
    """Accept opaque black/white PNG exports, including Piskel RGBA exports."""
    art = document["art"]
    path = source_path.parent / art["file"]
    if image is None:
        with Image.open(path) as source:
            if source.format != "PNG":
                raise StoryError("{}: expected a PNG sheet".format(path))
            image = source.copy()
    expected = (WIDTH * 4, HEIGHT * len(art["rows"]))
    if image.size != expected:
        raise StoryError("{}: expected sheet size {}, found {}".format(path, expected, image.size))
    rgba = image.convert("RGBA")
    colors = rgba.getcolors(3)
    if colors is None or any(color not in ((0, 0, 0, 255), (255, 255, 255, 255)) for _, color in colors):
        raise StoryError("{}: sheet must contain only opaque black and white pixels".format(path))
    return rgba.convert("1", dither=Image.Dither.NONE)


def art_pose(sheet, rows, art, phase):
    if art not in rows or phase not in range(4):
        raise StoryError("unknown sheet art or pose: {} / {}".format(art, phase))
    top = rows.index(art) * HEIGHT
    return sheet.crop((phase * WIDTH, top, (phase + 1) * WIDTH, top + HEIGHT))


def normalize_prose(paragraph: str, path: Path, line: int) -> str:
    text = " ".join(paragraph.split()).upper()
    if not text:
        fail(path, line, "empty paragraph")
    for char in text:
        if char not in SUPPORTED:
            fail(path, line, "unsupported character {!r} (U+{:04X})".format(char, ord(char)))
    return text


def wrap_paragraph(text: str, path: Path, line: int):
    rows = []
    row = ""
    for word in text.split():
        if len(word) > COLUMNS:
            fail(path, line, "word '{}' exceeds {} columns".format(word, COLUMNS))
        if not row:
            row = word
        elif len(row) + 1 + len(word) <= COLUMNS:
            row += " " + word
        else:
            rows.append(row)
            row = word
    if row:
        rows.append(row)
    return rows


def paginate(paragraphs, path: Path, line: int):
    pages = []
    page = []
    for index, paragraph in enumerate(paragraphs):
        rows = wrap_paragraph(normalize_prose(paragraph, path, line), path, line)
        if index != 0:
            if len(page) < ROWS_PER_CARD:
                page.append("")
            elif page:
                pages.append(page)
                page = []
        for row in rows:
            if len(page) >= ROWS_PER_CARD:
                pages.append(page)
                page = []
            page.append(row)
    if page:
        pages.append(page)
    return pages


def text_frame(rows) -> bytes:
    if not rows or len(rows) > ROWS_PER_CARD:
        raise ValueError("text card rows must be 1..{}".format(ROWS_PER_CARD))
    pixels = bytearray(FRAME_BYTES)
    top = (HEIGHT - ((len(rows) - 1) * 10 + 7)) // 2
    for line, row in enumerate(rows):
        left = (WIDTH - (len(row) * 6 - 1)) // 2
        for index, char in enumerate(row):
            for column, bits in enumerate(GLYPHS[char]):
                for y in range(7):
                    if bits & (1 << y):
                        pixel_y = top + line * 10 + y
                        pixel_x = left + index * 6 + column
                        if 0 <= pixel_y < HEIGHT and 0 <= pixel_x < WIDTH:
                            pixels[(pixel_y >> 3) * WIDTH + pixel_x] |= 1 << (pixel_y & 7)
    return bytes(pixels)


def image_frame(image) -> bytes:
    if image.mode != "1" or "transparency" in image.info:
        raise ValueError("expected opaque native 1-bit image")
    if image.size != (WIDTH, HEIGHT):
        raise ValueError("expected 128x64, found {}".format(image.size))
    source = image.load()
    pixels = bytearray(FRAME_BYTES)
    for y in range(0, HEIGHT, 8):
        for x in range(WIDTH):
            value = 0
            for bit in range(8):
                if source[x, y + bit]:
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


def build_layout(document, source_path: Path = SOURCE, sheet_image=None):
    sheet = load_sheet(document, source_path, sheet_image)
    rows = document["art"]["rows"]
    scenes = sorted(document["scenes"], key=lambda scene: SCENE_IDS.get(scene["name"], 0))
    frames = []
    image_frames = {}
    scene_cards = [0]
    scene_first = [0]
    card_frames = []
    card_kinds = []
    preview_scenes = []
    image_cards = 0
    text_cards = 0

    for scene in scenes:
        preview = {"id": scene["name"], "title": scene["title"], "cards": []}
        scene_first.append(len(card_frames))
        count = 0
        scene_text = 0
        for card in scene["cards"]:
            if card["kind"] == "image":
                art = card["art"]
                if art not in image_frames:
                    first = len(frames)
                    for phase in range(4):
                        frames.append(rle_encode(image_frame(art_pose(sheet, rows, art, phase))))
                    image_frames[art] = first
                card_frames.append(image_frames[art])
                card_kinds.append(0)
                image_cards += 1
                preview["cards"].append({
                    "kind": "image",
                    "title": "Image - {}".format(art),
                    "intent": "Native image scene: {}".format(art),
                    "art": art,
                    "copy": "",
                })
            else:
                pages = paginate(card["paragraphs"], source_path, scene["name"])
                for page in pages:
                    card_frames.append(len(frames))
                    frames.append(rle_encode(text_frame(page)))
                    card_kinds.append(1)
                    text_cards += 1
                    scene_text += 1
                    copy = "\n".join(page)
                    preview["cards"].append({
                        "kind": "text",
                        "title": "Text page {}".format(scene_text),
                        "intent": "Generated from story/scenes.json",
                        "copy": copy,
                        "rows": page,
                    })
            count = len(preview["cards"])
            if count > 0xFF:
                raise StoryError("scene {} has too many cards".format(scene["name"]))
        scene_cards.append(len(preview["cards"]))
        preview_scenes.append(preview)

    if len(frames) > 0xFF:
        raise StoryError("story frame count {} exceeds one-byte card frame metadata".format(len(frames)))
    if len(card_frames) != sum(scene_cards):
        raise StoryError("internal card count mismatch")

    offsets = []
    position = len(frames) * 2
    for frame in frames:
        offsets.append(position)
        position += len(frame)
    if position >= 0x10000:
        raise StoryError("story blob {} B exceeds the uint16 offset table".format(position))
    blob = bytearray()
    for offset in offsets:
        blob += struct.pack("<H", offset)
    for frame in frames:
        blob += frame
    blob += b"\0" * RLE_PAD

    return {
        "blob": bytes(blob),
        "frames": len(frames),
        "images": len(image_frames),
        "scene_cards": scene_cards,
        "scene_first": scene_first,
        "card_frames": card_frames,
        "card_kinds": card_kinds,
        "image_cards": image_cards,
        "text_cards": text_cards,
        "max_cards": max(scene_cards),
        "preview_scenes": preview_scenes,
        "arts": list(image_frames.keys()),
        "story_order": [scene["name"] for scene in document["scenes"]],
        "sheet": sheet,
        "art_rows": rows,
    }


def build_metadata(layout) -> bytes:
    """Cold AVR lookup table: scene cards, first card, card frame, card kind."""
    meta = bytes(
        list(layout["scene_cards"])
        + list(layout["scene_first"])
        + list(layout["card_frames"])
        + list(layout["card_kinds"])
    )
    expected = (len(layout["scene_cards"]) + len(layout["scene_first"])
                + len(layout["card_frames"]) + len(layout["card_kinds"]))
    if len(meta) != expected:
        raise StoryError("story metadata is {} B, expected {} B".format(len(meta), expected))
    return meta


def write_header(path: Path, layout, size: int) -> None:
    scene_constants = "\n".join(
        "constexpr uint8_t {} = {};".format(cpp_name, index + 1)
        for index, (_, cpp_name) in enumerate(SCENE_ORDER)
    )
    scene_cards = ", ".join(str(value) for value in layout["scene_cards"])
    scene_first = ", ".join(str(value) for value in layout["scene_first"])
    card_frames = ", ".join(str(value) for value in layout["card_frames"])
    card_kinds = ", ".join(str(value) for value in layout["card_kinds"])
    path.write_text(
        "// Generated by tools/gen_story_beats.py. Do not edit.\n"
        "#pragma once\n\n"
        "#include <stdint.h>\n\n"
        "constexpr uint8_t STORY_BEAT_WIDTH  = {};\n"
        "constexpr uint8_t STORY_BEAT_HEIGHT = {};\n"
        "constexpr uint8_t STORY_SCENE_MAX  = {};\n"
        "constexpr uint8_t STORY_SCENE_COUNT = {};\n"
        "constexpr uint8_t STORY_CARD_COUNT = {};\n"
        "constexpr uint8_t STORY_IMAGE_CARD_COUNT = {};\n"
        "constexpr uint8_t STORY_TEXT_CARD_COUNT = {};\n"
        "constexpr uint8_t STORY_IMAGE_COUNT = {};\n"
        "constexpr uint8_t STORY_FRAME_COUNT = {};\n"
        "constexpr uint16_t STORY_BEAT_BYTES = {};\n"
        "constexpr uint8_t STORY_BEAT_FRAME_TICKS = 18;\n"
        "constexpr uint8_t STORY_BEAT_CYCLE_TICKS = 72;\n"
        "constexpr uint8_t STORY_BEAT_MAX_CARDS = {};\n"
        "constexpr uint8_t STORY_KIND_IMAGE = 0;\n"
        "constexpr uint8_t STORY_KIND_TEXT = 1;\n"
        "// Compatibility aliases used by frame/CRC fixtures.\n"
        "constexpr uint8_t STORY_BEAT_IMAGES = STORY_IMAGE_COUNT;\n"
        "constexpr uint8_t STORY_BEAT_FRAMES = STORY_FRAME_COUNT;\n"
        "constexpr uint8_t STORY_BEAT_CARDS = STORY_CARD_COUNT;\n"
        "\n"
        "{}\n"
        "\n"
        "// Story metadata byte offsets inside fxdata/tables/story-meta.bin:\n"
        "// SCENE_CARDS, SCENE_FIRST_CARD, CARD_FRAME, CARD_KIND. The AVR build reads\n"
        "// this table from the FX cart, so no program-flash copy is linked.\n"
        "constexpr uint16_t STORY_META_SCENE_CARDS = 0;\n"
        "constexpr uint16_t STORY_META_SCENE_FIRST = STORY_META_SCENE_CARDS + STORY_SCENE_COUNT;\n"
        "constexpr uint16_t STORY_META_CARD_FRAME = STORY_META_SCENE_FIRST + STORY_SCENE_COUNT;\n"
        "constexpr uint16_t STORY_META_CARD_KIND = STORY_META_CARD_FRAME + STORY_CARD_COUNT;\n"
        "constexpr uint16_t STORY_META_BYTES = STORY_META_CARD_KIND + STORY_CARD_COUNT;\n"
        "\n"
        "#if !defined(__AVR__)\n"
        "constexpr uint8_t STORY_SCENE_CARDS[STORY_SCENE_COUNT] = {{{}}};\n"
        "constexpr uint8_t STORY_SCENE_FIRST_CARD[STORY_SCENE_COUNT] = {{{}}};\n"
        "constexpr uint8_t STORY_CARD_FRAME[STORY_CARD_COUNT] = {{{}}};\n"
        "constexpr uint8_t STORY_CARD_KIND[STORY_CARD_COUNT] = {{{}}};\n"
        "#endif\n".format(
            WIDTH,
            HEIGHT,
            len(SCENE_ORDER),
            len(SCENE_ORDER) + 1,
            layout["image_cards"] + layout["text_cards"],
            layout["image_cards"],
            layout["text_cards"],
            layout["images"],
            layout["frames"],
            size,
            layout["max_cards"],
            scene_constants,
            scene_cards,
            scene_first,
            card_frames,
            card_kinds,
        ),
        encoding="utf-8",
    )


def write_test_crcs(blob: bytes, count: int) -> None:
    """Independent device reference for every complete page-major frame."""
    offsets = [struct.unpack_from("<H", blob, i * 2)[0] for i in range(count)]
    values = []
    for i, start in enumerate(offsets):
        end = offsets[i + 1] if i + 1 < count else len(blob) - RLE_PAD
        crc = 0
        for byte in rle_decode(blob[start:end]):
            crc ^= byte
            for _ in range(8):
                crc = (crc >> 1) ^ (0x8408 if crc & 1 else 0)
        values.append("0x{:04X}".format(crc))
    (ROOT / "tst/fxdatatest/story_frame_crc.hpp").write_text(
        "// Generated by tools/gen_story_beats.py. Do not edit.\n"
        "#pragma once\n#include <avr/pgmspace.h>\n"
        '#include "src/generated/story_beats.hpp"\n'
        "const uint16_t kStoryFrameCrc[STORY_FRAME_COUNT] PROGMEM = {\n    "
        + ", ".join(values) + "\n};\n", encoding="utf-8")


def png_data_url(image):
    buffer = io.BytesIO()
    image.save(buffer, format="PNG")
    return "data:image/png;base64," + base64.b64encode(buffer.getvalue()).decode("ascii")


def native_image(data):
    image = Image.new("1", (WIDTH, HEIGHT))
    pixels = image.load()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            pixels[x, y] = 255 if (data[(y // 8) * WIDTH + x] >> (y % 8)) & 1 else 0
    return image


def write_card_sheet(layout):
    """All cards in narrative order, independent of packed save-scene order."""
    count = len(layout["card_frames"])
    sheet = Image.new("1", (WIDTH * 4, HEIGHT * count))
    offsets = [struct.unpack_from("<H", layout["blob"], i * 2)[0] for i in range(layout["frames"])]
    cards = []
    row = 0
    by_name = {scene["id"]: scene for scene in layout["preview_scenes"]}
    for name in layout["story_order"]:
        scene = by_name[name]
        for card_index, card in enumerate(scene["cards"]):
            packed_card = layout["scene_first"][SCENE_IDS[name]] + card_index
            first = layout["card_frames"][packed_card]
            poses = 4 if card["kind"] == "image" else 1
            for phase in range(poses):
                frame = first + phase
                end = offsets[frame + 1] if frame + 1 < layout["frames"] else len(layout["blob"]) - RLE_PAD
                image = native_image(rle_decode(layout["blob"][offsets[frame]:end]))
                sheet.paste(image, (phase * WIDTH, row * HEIGHT))
            cards.append({"row": row, "scene": scene["id"], "card": card_index,
                          "kind": card["kind"], "poses": poses, "title": card["title"],
                          "packed_card": packed_card})
            row += 1
    CARD_SHEET.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(CARD_SHEET)
    CARD_MAP.write_text(json.dumps({"tile_width": WIDTH, "tile_height": HEIGHT,
                                   "columns": 4, "cards": cards}, indent=2) + "\n", encoding="utf-8")


def write_preview(layout) -> None:
    animation = {
        "period": 300,
        "scenes": {
            art: [
                png_data_url(art_pose(layout["sheet"], layout["art_rows"], art, phase))
                for phase in range(4)
            ] for art in layout["arts"]
        },
    }
    data = {
        "period": 300,
        "columns": COLUMNS,
        "rows": ROWS_PER_CARD,
        "glyphs": GLYPHS,
        "scenes": layout["preview_scenes"],
    }
    PREVIEW.write_text(
        "globalThis.StoryAnimation={};\n"
        "globalThis.StoryData={};\n".format(
            json.dumps(animation, separators=(",", ":")),
            json.dumps(data, separators=(",", ":"), ensure_ascii=False),
        ),
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--meta-output", type=Path, default=META_OUTPUT)
    parser.add_argument("--header", type=Path, default=HEADER)
    args = parser.parse_args()

    scenes = parse_source(args.source.read_text(encoding="utf-8"), args.source)
    layout = build_layout(scenes, args.source)
    blob = layout["blob"]
    metadata = build_metadata(layout)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(blob)
    args.meta_output.parent.mkdir(parents=True, exist_ok=True)
    args.meta_output.write_bytes(metadata)
    write_header(args.header, layout, len(blob))
    write_test_crcs(blob, layout["frames"])
    write_preview(layout)
    write_card_sheet(layout)
    print("gen_story_beats: {} scenes / {} cards ({} image, {} text) / {} frames -> {} B".format(
        len(scenes["scenes"]),
        layout["image_cards"] + layout["text_cards"],
        layout["image_cards"],
        layout["text_cards"],
        layout["frames"],
        len(blob),
    ))


if __name__ == "__main__":
    main()

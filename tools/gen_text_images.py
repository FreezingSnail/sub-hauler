#!/usr/bin/env python3
"""Bake every runtime UI string into 128x8 plus-mask FX stamps.

All shipping text is static: station air status and every live number were
retired, so the device no longer needs the external font renderer at runtime.
This tool rasterises the same 6x8 glyph table used by ``gen_font``,
``gen_hud`` and ``gen_panels`` into two-frame 128x8 stamps:

* frame 0 - white glyphs on a transparent background;
* frame 1 - black glyphs on a transparent background (the selection-bar
  variant).

``gen_sprites.py`` packs them as plus-mask blobs with a two-byte header, so
``Blit::draw`` consumes ``image + 2`` with a 256 B frame stride.

Individual ``img_<string>`` rows keep their glyph origin at x=0 so the caller
passes the same screen x as the retired ``text::at(x, y)`` path. Composites and sheets
(``img_berth_names``, ``img_dock_barks``, ``img_row_labels``, ``img_story_row``,
``img_carry_row``, ``img_berth_work``) are left-aligned at pixel 0 because the
runtime passes their exact screen x to ``text::stampFrame``.

Text is read from ``fxdata/fxdata.txt`` and enum order from
``src/campaign.hpp``. The per-berth dock hints are read from the human-editable
``story/dock_hints.json``; every berth gets one frame at every ``DockPoint``.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

from PIL import Image

from gen_font import FRAME_HEIGHT, FRAME_WIDTH, GLYPHS
from gen_panels import EXTRA_GLYPHS

ROOT = Path(__file__).resolve().parents[1]
FXDATA = ROOT / "fxdata/fxdata.txt"
CAMPAIGN = ROOT / "src/campaign.hpp"
DOCK_HINTS = ROOT / "story/dock_hints.json"
OUTPUT = ROOT / "images/text"
LAYOUT = ROOT / "src/generated/text_layout.hpp"

STAMP = (128, 8)
TEXT_INSET_X = 0
WHITE = (255, 255, 255, 255)
BLACK = (0, 0, 0, 255)
CLEAR = (0, 0, 0, 0)

STRING_RE = re.compile(r"^string\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*'(?P<text>[^']*)'\s*$", re.M)

# Mission ids 0..7 plus kWorkEnding render one STORY/CARRY row each. The
# display strings are the existing fxdata labels (DEEP RELAY and VIEW ENDING do
# not follow camel-case derivation).
MISSION_STRINGS = (
    "txt_first",
    "txt_pump_survey",
    "txt_pump",
    "txt_lift_survey",
    "txt_lift",
    "txt_beacon",
    "txt_home",
    "txt_deep_relay",
    "txt_view_ending",
)

ROW_LABELS = ("txt_work", "txt_undock")


def load_strings(path):
    text = Path(path).read_text(encoding="utf-8")
    return {match.group("name"): match.group("text") for match in STRING_RE.finditer(text)}


def ordered_strings(path):
    text = Path(path).read_text(encoding="utf-8")
    return [match.group("name") for match in STRING_RE.finditer(text)]


def parse_enum(text: str, name: str, source: Path) -> list[str]:
    match = re.search(r"enum\s+%s\s*:[^{]*\{(.*?)\};" % re.escape(name), text, re.S)
    if not match:
        raise ValueError("{}: missing enum {}".format(source, name))
    names = re.findall(r"\b(k[A-Za-z0-9_]+)\b", match.group(1))
    if not names:
        raise ValueError("{}: enum {} has no symbols".format(source, name))
    return names


def camel_to_snake(name: str) -> str:
    name = name[1:] if name.startswith("k") else name
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def enum_id(name: str, prefix: str) -> str:
    snake = camel_to_snake(name)
    return snake[len(prefix):] if snake.startswith(prefix) else snake


DOCK_TEXT_RE = re.compile(r"^[A-Z0-9 :./'-]+$")


def load_dock_hints() -> dict:
    document = json.loads(DOCK_HINTS.read_text(encoding="utf-8"))
    if document.get("schema") != 1:
        raise ValueError("{}: expected schema 1".format(DOCK_HINTS))
    points = document.get("points")
    berths = document.get("berths")
    if not isinstance(points, list) or not points:
        raise ValueError("{}: points must be a non-empty list".format(DOCK_HINTS))
    if not isinstance(berths, dict) or not berths:
        raise ValueError("{}: berths must be a non-empty object".format(DOCK_HINTS))
    for point in points:
        if not isinstance(point, dict) or not all(isinstance(point.get(key), str) for key in ("id", "label", "facts")):
            raise ValueError("{}: every point needs id, label and facts".format(DOCK_HINTS))
    return document


def dock_hint_layout(campaign_text: str, document: dict):
    berth_ids = [enum_id(name, "") for name in parse_enum(campaign_text, "Berth", CAMPAIGN)]
    point_ids = [enum_id(name, "dock_point_") for name in parse_enum(campaign_text, "DockPoint", CAMPAIGN)]
    json_points = [point["id"] for point in document["points"]]
    if json_points != point_ids:
        raise ValueError("{}: point order {} does not match campaign::DockPoint {}".format(DOCK_HINTS, json_points, point_ids))
    if set(document["berths"].keys()) != set(berth_ids):
        raise ValueError("{}: berths {} do not match campaign::Berth {}".format(DOCK_HINTS, sorted(document["berths"]), berth_ids))
    return berth_ids, point_ids


def dock_hint_texts(campaign_text: str, document: dict) -> list[str]:
    berth_ids, point_ids = dock_hint_layout(campaign_text, document)
    texts = []
    for berth in berth_ids:
        row = document["berths"][berth]
        if set(row) != set(point_ids):
            raise ValueError("{}: berth {} must have exactly {}".format(DOCK_HINTS, berth, point_ids))
        for point in point_ids:
            text = row[point]
            if not isinstance(text, str) or not text:
                raise ValueError("{}: berth {} point {} has no text".format(DOCK_HINTS, berth, point))
            if len(text) > 20:
                raise ValueError("{}: {}/{} = {!r} exceeds 20 columns".format(DOCK_HINTS, berth, point, text))
            if not DOCK_TEXT_RE.fullmatch(text):
                raise ValueError("{}: {}/{} uses characters outside the 6x8 font: {!r}".format(DOCK_HINTS, berth, point, text))
            texts.append(text)
    return texts


def glyph_columns(char: str):
    try:
        code = ord(char)
        if code in EXTRA_GLYPHS:
            return EXTRA_GLYPHS[code]
        return GLYPHS[code]
    except KeyError as exc:
        raise ValueError("stamp text uses unsupported character {!r}".format(char)) from exc


def stamp_image(text: str, inset: int, invert: bool = False) -> Image.Image:
    """Return one 128x8 transparent-background text row."""
    # Five glyph columns per cell; the trailing spacing column may be clipped.
    if inset < 0 or inset + (len(text) - 1) * FRAME_WIDTH + 5 > STAMP[0]:
        raise ValueError("stamp text {!r} does not fit 128 px at x={}".format(text, inset))
    image = Image.new("RGBA", STAMP, CLEAR)
    pixels = image.load()
    color = BLACK if invert else WHITE
    for index, char in enumerate(text):
        base = inset + index * FRAME_WIDTH
        for column, bits in enumerate(glyph_columns(char)):
            for row in range(FRAME_HEIGHT):
                if bits & (1 << row):
                    pixels[base + column, row] = color
    return image


def two_frame_row(text: str, inset: int = TEXT_INSET_X) -> Image.Image:
    sheet = Image.new("RGBA", (STAMP[0] * 2, STAMP[1]), CLEAR)
    sheet.paste(stamp_image(text, inset), (0, 0))
    sheet.paste(stamp_image(text, inset, invert=True), (STAMP[0], 0))
    return sheet


def sheet(rows) -> Image.Image:
    image = Image.new("RGBA", (STAMP[0] * len(rows), STAMP[1]), CLEAR)
    for index, row in enumerate(rows):
        image.paste(row, (STAMP[0] * index, 0))
    return image


def checked(strings, symbol: str) -> str:
    try:
        return strings[symbol]
    except KeyError as exc:
        raise ValueError("missing fxdata string {}".format(symbol)) from exc


def berth_symbols(campaign_text: str, strings) -> list[str]:
    symbols = []
    for name in parse_enum(campaign_text, "Berth", CAMPAIGN):
        symbol = "txt_" + camel_to_snake(name)
        checked(strings, symbol)
        symbols.append(symbol)
    return symbols


def mission_texts(strings) -> list[str]:
    return [checked(strings, symbol) for symbol in MISSION_STRINGS]


def build_rows(strings, campaign_text, dock_hints):
    rows = []
    for symbol in ordered_strings(FXDATA):
        rows.append(("img_" + symbol, two_frame_row(checked(strings, symbol))))

    berths = berth_symbols(campaign_text, strings)
    if len(berths) != 8:
        raise ValueError("expected eight campaign berths, found {}".format(len(berths)))
    berth_texts = [checked(strings, symbol) for symbol in berths]
    berth_texts.append(checked(strings, "txt_station"))
    rows.append(("img_berth_names", sheet([stamp_image(text, 0) for text in berth_texts])))

    # Frame 0 is the empty bark; the remaining frames are berth-major and
    # point-minor so text_layout::dockBarkFrame() is one multiply and add.
    bark_texts = [""] + dock_hint_texts(campaign_text, dock_hints)
    rows.append(("img_dock_barks", sheet([stamp_image(text, 0) for text in bark_texts])))

    label_texts = [checked(strings, symbol) for symbol in ROW_LABELS]
    rows.append(
        (
            "img_row_labels",
            sheet(
                [stamp_image(text, 0) for text in label_texts]
                + [stamp_image(text, 0, invert=True) for text in label_texts]
            ),
        )
    )

    missions = mission_texts(strings)
    rows.append(("img_story_row", sheet([stamp_image("STORY " + text, 0) for text in missions] + [stamp_image("STORY " + text, 0, invert=True) for text in missions])))
    rows.append(("img_carry_row", sheet([stamp_image("CARRY " + text, 0) for text in missions])))
    rows.append(("img_berth_work", sheet([stamp_image(text + " WORK", 0) for text in berth_texts[:-1]])))
    return rows


LAYOUT_TEMPLATE = """// Generated by tools/gen_text_images.py. Do not edit.
#pragma once

#include <stdint.h>

// Baked 128x8 plus-mask text stamps. Frame stride is 256 B (one 128-px page
// with a mask byte); Blit::draw skips the two-byte sprite header.
namespace text_layout {{

constexpr uint8_t kStampWidth = {width};
constexpr uint8_t kStampHeight = {height};
constexpr uint8_t kStampBytes = {stride};
constexpr uint8_t kTextInsetX = {inset};

// img_berth_names: eight campaign::Berth names in enum order, then STATION.
constexpr uint8_t kBerthNameFrames = {berth_frames};
constexpr uint8_t kBerthNameFallback = {berth_fallback};

// img_dock_barks: frame 0 is empty, then one frame per berth and DockPoint in
// story/dock_hints.json. Frames are berth-major and point-minor; every berth
// owns one line at every saved progression point.
constexpr uint8_t kDockBarkBerths = {bark_berths};
constexpr uint8_t kDockBarkPoints = {bark_points};
constexpr uint8_t kDockBarkFrames = {bark_frames};
static_assert(1 + (kDockBarkBerths - 1) * kDockBarkPoints + (kDockBarkPoints - 1) + 1 == kDockBarkFrames,
              "dock bark frame count must match the berth x point grid");

constexpr uint8_t dockBarkFrame(uint8_t berth, uint8_t point) {{
    return berth < kDockBarkBerths && point < kDockBarkPoints
               ? static_cast<uint8_t>(1 + berth * kDockBarkPoints + point)
               : 0;
}}

// img_row_labels: WORK, UNDOCK, then the two inverted selection variants.
constexpr uint8_t kRowLabelFrames = {row_label_frames};
constexpr uint8_t kRowLabelWork = 0;
constexpr uint8_t kRowLabelUndock = 1;
constexpr uint8_t kRowLabelInvertedBase = 2;

// img_story_row: nine normal mission rows, then nine inverted rows.
constexpr uint8_t kMissionFrames = {mission_frames};
constexpr uint8_t kStoryRowFrames = {story_frames};
constexpr uint8_t kStoryRowInvertedBase = {mission_frames};
constexpr uint8_t kCarryRowFrames = {mission_frames};

// img_berth_work: one "<berth> WORK" header per campaign berth.
constexpr uint8_t kBerthWorkFrames = {berth_work_frames};

}}   // namespace text_layout
"""


def render_layout(rows, dock_hints, campaign_text) -> str:
    counts = {name: image.width // STAMP[0] for name, image in rows}
    berth_ids, point_ids = dock_hint_layout(campaign_text, dock_hints)
    return LAYOUT_TEMPLATE.format(
        width=STAMP[0],
        height=STAMP[1],
        stride=STAMP[0] * (STAMP[1] // 8) * 2,
        inset=TEXT_INSET_X,
        berth_frames=counts["img_berth_names"],
        berth_fallback=counts["img_berth_names"] - 1,
        bark_berths=len(berth_ids),
        bark_points=len(point_ids),
        bark_frames=counts["img_dock_barks"],
        row_label_frames=counts["img_row_labels"],
        mission_frames=len(MISSION_STRINGS),
        story_frames=counts["img_story_row"],
        berth_work_frames=counts["img_berth_work"],
    )


def write_images(directory, rows) -> list[Path]:
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    expected = set()
    for name, image in rows:
        path = directory / ("{}_128x8.png".format(name))
        image.save(path)
        expected.add(path.name)
    for path in directory.glob("*.png"):
        if path.name not in expected:
            path.unlink()
    return [directory / ("{}_128x8.png".format(name)) for name, _image in rows]


def build(campaign_text: str | None = None):
    strings = load_strings(FXDATA)
    campaign = CAMPAIGN.read_text(encoding="utf-8") if campaign_text is None else campaign_text
    dock_hints = load_dock_hints()
    rows = build_rows(strings, campaign, dock_hints)
    return rows, render_layout(rows, dock_hints, campaign)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default=str(OUTPUT), help="directory of generated PNG stamps")
    parser.add_argument("--layout", default=str(LAYOUT), help="generated C++ layout constants")
    parser.add_argument("--check", action="store_true", help="fail if committed outputs are stale")
    args = parser.parse_args(argv)

    rows, layout = build()
    output = Path(args.output)
    layout_path = Path(args.layout)
    if args.check:
        stale = False
        for name, image in rows:
            path = output / ("{}_128x8.png".format(name))
            if not path.exists() or Image.open(path).convert("RGBA").tobytes() != image.tobytes():
                print("gen_text_images: {} is stale; run make gen".format(path), file=sys.stderr)
                stale = True
        if not layout_path.exists() or layout_path.read_text(encoding="utf-8") != layout:
            print("gen_text_images: {} is stale; run make gen".format(layout_path), file=sys.stderr)
            stale = True
        return 1 if stale else 0

    write_images(output, rows)
    layout_path.parent.mkdir(parents=True, exist_ok=True)
    layout_path.write_text(layout, encoding="utf-8")
    print("gen_text_images: wrote {} stamps and {}".format(len(rows), layout_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

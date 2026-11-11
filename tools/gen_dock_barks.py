#!/usr/bin/env python3
"""Generate the dock-hint documentation preview.

``story/dock_hints.json`` is the human-editable source: one line per berth at
every saved completion point. The same JSON is baked into the ``img_dock_barks``
FX stamp sheet by ``gen_text_images.py``. This preview owns no shipping prose;
it validates the JSON against ``campaign::Berth`` and ``campaign::DockPoint``,
then emits the exact text and frame index used by the device.

Output:
  docs/assets/dock-barks.js
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

import gen_text_images as text_images

ROOT = Path(__file__).resolve().parents[1]
CAMPAIGN = ROOT / "src/campaign.hpp"
STATION_VIEW = ROOT / "src/station_view.hpp"
TEXT = ROOT / "src/text.hpp"
FXDATA = ROOT / "fxdata/fxdata.txt"
OUTPUT = ROOT / "docs/assets/dock-barks.js"

SOURCES = (
    "story/dock_hints.json",
    "src/campaign.hpp",
    "src/station_view.hpp",
    "src/text.hpp",
    "fxdata/fxdata.txt",
)

LAYOUT_CONSTANTS = (
    "kNameX",
    "kHeaderY",
    "kDividerY",
    "kHintX",
    "kBarkY",
    "kHintChars",
)


class DockHintError(ValueError):
    pass


def parse_enum(text: str, name: str, source: Path) -> list[str]:
    match = re.search(r"enum\s+%s\s*:[^{]*\{(.*?)\};" % re.escape(name), text, re.S)
    if not match:
        raise DockHintError("{}: missing enum {}".format(source, name))
    names = re.findall(r"\b(k[A-Za-z0-9_]+)\b", match.group(1))
    if not names:
        raise DockHintError("{}: enum {} has no symbols".format(source, name))
    return names


def parse_fx_strings(text: str, source: Path) -> dict[str, str]:
    strings = {}
    for match in re.finditer(r"^\s*string\s+(\w+)\s*=\s*'([^']*)'\s*$", text, re.M):
        symbol, value = match.groups()
        if symbol in strings:
            raise DockHintError("{}: duplicate string {}".format(source, symbol))
        strings[symbol] = value
    if not strings:
        raise DockHintError("{}: no string declarations found".format(source))
    return strings


def parse_uint8(text: str, name: str, source: Path) -> int:
    match = re.search(r"constexpr\s+uint8_t\s+%s\s*=\s*(\d+)\s*;" % re.escape(name), text)
    if not match:
        raise DockHintError("{}: missing constexpr {}".format(source, name))
    return int(match.group(1))


def parse_layout(station_text: str) -> dict[str, int]:
    return {
        name[1:2].lower() + name[2:]: parse_uint8(station_text, name, STATION_VIEW)
        for name in LAYOUT_CONSTANTS
    }


def camel_to_snake(name: str) -> str:
    name = name[1:] if name.startswith("k") else name
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def enum_id(name: str, prefix: str) -> str:
    snake = camel_to_snake(name)
    return snake[len(prefix):] if snake.startswith(prefix) else snake


def build_document() -> dict:
    campaign_text = CAMPAIGN.read_text(encoding="utf-8")
    station_text = STATION_VIEW.read_text(encoding="utf-8")
    text_text = TEXT.read_text(encoding="utf-8")
    fxdata_text = FXDATA.read_text(encoding="utf-8")

    try:
        hints = text_images.load_dock_hints()
        berth_ids, point_ids = text_images.dock_hint_layout(campaign_text, hints)
        texts = text_images.dock_hint_texts(campaign_text, hints)
    except ValueError as exc:
        raise DockHintError(str(exc)) from exc

    berth_names = parse_enum(campaign_text, "Berth", CAMPAIGN)
    if [enum_id(name, "") for name in berth_names] != berth_ids:
        raise DockHintError("campaign::Berth order changed while reading the hint layout")
    point_names = parse_enum(campaign_text, "DockPoint", CAMPAIGN)
    if [enum_id(name, "dock_point_") for name in point_names] != point_ids:
        raise DockHintError("campaign::DockPoint order changed while reading the hint layout")

    strings = parse_fx_strings(fxdata_text, FXDATA)
    layout = parse_layout(station_text)
    cell_width = parse_uint8(text_text, "kCellWidth", TEXT)
    cell_height = parse_uint8(text_text, "kCellHeight", TEXT)

    text_index = 0
    berth_rows = []
    for berth_index, (enum_name, berth_id) in enumerate(zip(berth_names, berth_ids)):
        name_symbol = "txt_" + berth_id
        if name_symbol not in strings:
            raise DockHintError("missing berth label string {}".format(name_symbol))
        barks = []
        for point_index, point_id in enumerate(point_ids):
            frame = 1 + berth_index * len(point_ids) + point_index
            text = texts[text_index]
            text_index += 1
            barks.append(
                {
                    "point": point_id,
                    "frame": frame,
                    "symbol": "img_dock_barks[{}]".format(frame),
                    "text": text,
                }
            )
        berth_rows.append({"id": enum_name, "name": strings[name_symbol], "barks": barks})

    return {
        "schema": 2,
        "sources": list(SOURCES),
        "font": {
            "image": "../fxdata/fonts/font_6x8.png",
            "cellWidth": cell_width,
            "cellHeight": cell_height,
        },
        "layout": layout,
        "states": [
            {"id": point["id"], "label": point["label"], "facts": point["facts"]}
            for point in hints["points"]
        ],
        "berths": berth_rows,
    }


def render(document: dict) -> str:
    header = (
        "// Generated by tools/gen_dock_barks.py from story/dock_hints.json\n"
        "// and src/campaign.hpp. Read-only preview data; do not edit by hand.\n"
        "// Run `make gen`.\n"
    )
    return header + "globalThis.DockBarks = " + json.dumps(document, indent=2, ensure_ascii=True) + ";\n"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if the committed output is stale")
    args = parser.parse_args(argv)
    try:
        output = render(build_document())
    except DockHintError as exc:
        print("gen_dock_barks: {}".format(exc), file=sys.stderr)
        return 1
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != output:
            print("gen_dock_barks: {} is stale; run make gen".format(OUTPUT), file=sys.stderr)
            return 1
        return 0
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(output, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

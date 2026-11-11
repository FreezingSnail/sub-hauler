#!/usr/bin/env python3
"""Generate the read-only dock-bark preview data for the story sketchbook.

The preview owns no shipping prose. It reads the berth and ``DockBark`` order
from ``src/campaign.hpp``, the bark -> FX string table from
``src/station_view.hpp`` and every line from ``fxdata/fxdata.txt``. The saved
progression checkpoints below mirror ``campaign::dockBark``; the host campaign
tests pin the runtime selector to the same observable transitions.

Output:
  docs/assets/dock-barks.js
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CAMPAIGN = ROOT / "src/campaign.hpp"
STATION_VIEW = ROOT / "src/station_view.hpp"
TEXT = ROOT / "src/text.hpp"
FXDATA = ROOT / "fxdata/fxdata.txt"
OUTPUT = ROOT / "docs/assets/dock-barks.js"

SOURCES = (
    "src/campaign.hpp",
    "src/station_view.hpp",
    "src/text.hpp",
    "fxdata/fxdata.txt",
)

# Preview-only checkpoint names. A checkpoint is a set of saved campaign/tool
# facts, not a new runtime state or save field. HOME and DONE have identical
# selector output because DONE only adds already-implied survey/tool bits.
STATES = (
    {
        "id": "first",
        "label": "FIRST",
        "facts": "FIRST milestone recorded; no surveys, seal or later milestones.",
        "milestones": {"first"},
        "surveys": set(),
        "seal": False,
    },
    {
        "id": "pump",
        "label": "PUMP",
        "facts": "FIRST + PUMP milestones; pump survey recorded; lift survey still open.",
        "milestones": {"first", "pump"},
        "surveys": {"pump"},
        "seal": False,
    },
    {
        "id": "lift",
        "label": "LIFT",
        "facts": "FIRST + PUMP + LIFT milestones; both surveys recorded.",
        "milestones": {"first", "pump", "lift"},
        "surveys": {"pump", "lift"},
        "seal": False,
    },
    {
        "id": "seal",
        "label": "SEAL",
        "facts": "Both repairs complete and the pressure seal is owned.",
        "milestones": {"first", "pump", "lift"},
        "surveys": {"pump", "lift"},
        "seal": True,
    },
    {
        "id": "core",
        "label": "CORE",
        "facts": "BEACON delivered; the beacon core is lit at Deep Anchor.",
        "milestones": {"first", "pump", "lift", "beacon"},
        "surveys": {"pump", "lift"},
        "seal": True,
    },
    {
        "id": "relay",
        "label": "RELAY",
        "facts": "DEEP RELAY expedition complete; HOME is now available.",
        "milestones": {"first", "pump", "lift", "beacon", "expedition"},
        "surveys": {"pump", "lift"},
        "seal": True,
    },
    {
        "id": "home",
        "label": "HOME",
        "facts": "HOME delivered; the network is restored and free exploration continues.",
        "milestones": {"first", "pump", "lift", "beacon", "expedition", "home"},
        "surveys": {"pump", "lift"},
        "seal": True,
    },
    {
        "id": "done",
        "label": "DONE",
        "facts": "Every checkpoint bit recorded, including optional surveys and tools (same bark as HOME).",
        "milestones": {"first", "pump", "lift", "beacon", "expedition", "home"},
        "surveys": {"pump", "lift"},
        "seal": True,
    },
)

LAYOUT_CONSTANTS = (
    "kNameX",
    "kHeaderY",
    "kDividerY",
    "kStatusX",
    "kStatusY",
    "kHintX",
    "kBarkY",
    "kHintChars",
)


class DockBarkError(ValueError):
    pass


def parse_enum(text: str, name: str, source: Path) -> list[str]:
    """Return the enum's symbol names in declaration order."""
    match = re.search(r"enum\s+%s\s*:[^{]*\{(.*?)\};" % re.escape(name), text, re.S)
    if not match:
        raise DockBarkError("{}: missing enum {}".format(source, name))
    names = re.findall(r"\b(k[A-Za-z0-9_]+)\b", match.group(1))
    if not names:
        raise DockBarkError("{}: enum {} has no symbols".format(source, name))
    return names


def parse_bark_table(text: str, source: Path) -> list[str | None]:
    """Return the bark table indexed by campaign::DockBark."""
    match = re.search(r"kDockBarkText\s*\[\s*\]\s*PROGMEM\s*=\s*\{(.*?)\};", text, re.S)
    if not match:
        raise DockBarkError("{}: missing kDockBarkText table".format(source))
    entries = re.findall(r"\b(?:0|txt_[A-Za-z0-9_]+)\b", match.group(1))
    if not entries or entries[0] != "0":
        raise DockBarkError("{}: kDockBarkText index 0 must be the empty bark".format(source))
    return [None if entry == "0" else entry for entry in entries]


def parse_fx_strings(text: str, source: Path) -> dict[str, str]:
    """Parse ``string symbol = 'TEXT'`` declarations from the FX layout."""
    strings = {}
    for match in re.finditer(r"^\s*string\s+(\w+)\s*=\s*'([^']*)'\s*$", text, re.M):
        symbol, value = match.groups()
        if symbol in strings:
            raise DockBarkError("{}: duplicate string {}".format(source, symbol))
        strings[symbol] = value
    if not strings:
        raise DockBarkError("{}: no string declarations found".format(source))
    return strings


def parse_uint8(text: str, name: str, source: Path) -> int:
    match = re.search(r"constexpr\s+uint8_t\s+%s\s*=\s*(\d+)\s*;" % re.escape(name), text)
    if not match:
        raise DockBarkError("{}: missing constexpr {}".format(source, name))
    return int(match.group(1))


def parse_layout(station_text: str) -> dict[str, int]:
    return {
        name[1:2].lower() + name[2:]: parse_uint8(station_text, name, STATION_VIEW)
        for name in LAYOUT_CONSTANTS
    }


def camel_to_snake(name: str) -> str:
    name = name[1:] if name.startswith("k") else name
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def select_after_lift(state) -> str:
    if not state["seal"]:
        return "kDockBarkBeaconNeedsSeal"
    if "beacon" not in state["milestones"]:
        return "kDockBarkBeaconReady"
    if "expedition" not in state["milestones"]:
        return "kDockBarkDeepRelay"
    if "home" not in state["milestones"]:
        return "kDockBarkBeaconLit"
    return "kDockBarkNetworkOn"


def select_bark(state, berth: str) -> str | None:
    """Mirror campaign::dockBark for the saved facts in ``state``."""
    milestones = state["milestones"]
    surveys = state["surveys"]
    if berth == "kTidewall":
        if "home" in milestones:
            return "kDockBarkNetworkOn"
        if "first" in milestones:
            return "kDockBarkBenthosLamps"
        return "kDockBarkFirst"
    if berth == "kBenthos":
        if "first" not in milestones:
            return "kDockBarkNoWork"
        if "pump" not in surveys:
            return "kDockBarkPumpSurvey"
        if "pump" not in milestones:
            return "kDockBarkPumpSource"
        if "lift" not in surveys:
            return "kDockBarkLiftSurvey"
        if "lift" not in milestones:
            return "kDockBarkLiftSource"
        return "kDockBarkLiftRestored"
    if berth == "kCormorant":
        if "first" not in milestones:
            return "kDockBarkNoWork"
        if "lift" not in surveys:
            return "kDockBarkLiftSurvey"
        if "pump" not in surveys:
            return "kDockBarkPumpSurvey"
        if "pump" not in milestones:
            return "kDockBarkPumpPartReady"
        if "lift" not in milestones:
            return "kDockBarkLiftSource"
        if not state["seal"]:
            return "kDockBarkBeaconNeedsSeal"
        return "kDockBarkBeaconReady"
    if berth == "kDeepAnchor":
        if "first" not in milestones:
            return "kDockBarkNoWork"
        if "beacon" not in milestones:
            return "kDockBarkBeaconPlan"
        if "expedition" not in milestones:
            return "kDockBarkDeepRelay"
        if "home" not in milestones:
            return "kDockBarkBeaconLit"
        return "kDockBarkNetworkOn"
    if berth == "kPumpRefuge":
        if "pump" not in milestones:
            return "kDockBarkPumpSource"
        if "lift" not in surveys:
            return "kDockBarkLiftSurvey"
        if "lift" not in milestones:
            return "kDockBarkLiftSource"
        return select_after_lift(state)
    if berth in ("kLowerGantry", "kFreightYard"):
        if "lift" not in milestones:
            return "kDockBarkLiftSource"
        return select_after_lift(state)
    if berth == "kDeepRelay":
        if "expedition" not in milestones:
            return "kDockBarkDeepRelay"
        return select_after_lift(state)
    return None


def build_document() -> dict:
    campaign_text = CAMPAIGN.read_text(encoding="utf-8")
    station_text = STATION_VIEW.read_text(encoding="utf-8")
    text_text = TEXT.read_text(encoding="utf-8")
    fxdata_text = FXDATA.read_text(encoding="utf-8")

    dock_barks = parse_enum(campaign_text, "DockBark", CAMPAIGN)
    berths = parse_enum(campaign_text, "Berth", CAMPAIGN)
    bark_symbols = parse_bark_table(station_text, STATION_VIEW)
    strings = parse_fx_strings(fxdata_text, FXDATA)
    layout = parse_layout(station_text)
    cell_width = parse_uint8(text_text, "kCellWidth", TEXT)
    cell_height = parse_uint8(text_text, "kCellHeight", TEXT)

    if len(bark_symbols) != len(dock_barks):
        raise DockBarkError(
            "kDockBarkText has {} entries but DockBark has {} symbols".format(
                len(bark_symbols), len(dock_barks)
            )
        )
    bark_index = {name: index for index, name in enumerate(dock_barks)}

    berth_rows = []
    for berth in berths:
        name_symbol = "txt_" + camel_to_snake(berth)
        if name_symbol not in strings:
            raise DockBarkError("missing berth label string {}".format(name_symbol))
        barks = []
        for state in STATES:
            bark = select_bark(state, berth)
            if bark is None or bark == "kDockBarkNone":
                raise DockBarkError("{} has no bark at {}".format(berth, state["id"]))
            symbol = bark_symbols[bark_index[bark]]
            if symbol is None or symbol not in strings:
                raise DockBarkError("{} has no shipped FX string".format(bark))
            text = strings[symbol]
            if not text or len(text) > layout["hintChars"]:
                raise DockBarkError(
                    "{} = {!r} does not fit the {} column bark row".format(
                        symbol, text, layout["hintChars"]
                    )
                )
            if not re.fullmatch(r"[A-Z0-9 :./'-]+", text):
                raise DockBarkError("{} uses characters outside the 6x8 font: {!r}".format(symbol, text))
            barks.append({"bark": bark, "symbol": symbol, "text": text})
        berth_rows.append({"id": berth, "name": strings[name_symbol], "barks": barks})

    return {
        "schema": 1,
        "sources": list(SOURCES),
        "font": {
            "image": "../fxdata/fonts/font_6x8.png",
            "cellWidth": cell_width,
            "cellHeight": cell_height,
        },
        "layout": layout,
        "states": [
            {"id": state["id"], "label": state["label"], "facts": state["facts"]}
            for state in STATES
        ],
        "berths": berth_rows,
    }


def render(document: dict) -> str:
    header = (
        "// Generated by tools/gen_dock_barks.py from src/campaign.hpp,\n"
        "// src/station_view.hpp, src/text.hpp and fxdata/fxdata.txt.\n"
        "// Read-only preview data; do not edit by hand. Run `make gen`.\n"
    )
    return header + "globalThis.DockBarks = " + json.dumps(document, indent=2, ensure_ascii=True) + ";\n"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if the committed output is stale")
    args = parser.parse_args(argv)
    output = render(build_document())
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

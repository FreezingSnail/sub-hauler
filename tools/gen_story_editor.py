#!/usr/bin/env python3
"""Generate the local story-editor schema and mission preview data.

Canonical inputs:
  story/scenes.json       fixed scene/card structure and editable story prose
  story/missions.json     editable mission summaries/intents and scene grouping
  story/dock_hints.json   editable berth/point hint text
  src/campaign.hpp        frozen berth, mission and DockPoint order
  fxdata/fxdata.txt       berth display names

Outputs:
  docs/assets/story-editor/schema.js
  docs/assets/story-editor/missions.js

The browser editor loads the canonical JSON through the File System Access API
and writes only those JSON files. This generated data supplies the fixed
structure, glyph table, wrapping limits and enum order; it never contains
shipping prose.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import gen_story_beats as story
import gen_text_images as text_images

SCENES = ROOT / "story/scenes.json"
MISSIONS = ROOT / "story/missions.json"
DOCK_HINTS = ROOT / "story/dock_hints.json"
CAMPAIGN = ROOT / "src/campaign.hpp"
FXDATA = ROOT / "fxdata/fxdata.txt"
OUTPUT = ROOT / "docs/assets/story-editor/schema.js"
MISSIONS_OUTPUT = ROOT / "docs/assets/story-editor/missions.js"


class EditorError(ValueError):
    pass


def parse_enum(text: str, name: str, source: Path) -> list[str]:
    match = re.search(r"enum\s+%s\s*:[^{]*\{(.*?)\};" % re.escape(name), text, re.S)
    if not match:
        raise EditorError("{}: missing enum {}".format(source, name))
    names = re.findall(r"\b(k[A-Za-z0-9_]+)\b", match.group(1))
    if not names:
        raise EditorError("{}: enum {} has no symbols".format(source, name))
    return names


def enum_id(name: str, prefix: str) -> str:
    snake = text_images.camel_to_snake(name)
    return snake[len(prefix):] if snake.startswith(prefix) else snake


def parse_fx_strings(text: str, source: Path) -> dict[str, str]:
    strings = {}
    for match in re.finditer(r"^\s*string\s+(\w+)\s*=\s*'([^']*)'\s*$", text, re.M):
        symbol, value = match.groups()
        if symbol in strings:
            raise EditorError("{}: duplicate string {}".format(source, symbol))
        strings[symbol] = value
    return strings


def load_missions(scene_names: list[str]) -> dict:
    try:
        document = json.loads(MISSIONS.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise EditorError("{}:{}: {}".format(MISSIONS, error.lineno, error.msg)) from error
    return validate_missions(document, scene_names)


def validate_missions(document, scene_names: list[str]) -> dict:
    if not isinstance(document, dict):
        raise EditorError("{}: expected an object".format(MISSIONS))
    if set(document) != {"schema", "unassigned", "missions"}:
        raise EditorError("{}: expected schema, unassigned and missions".format(MISSIONS))
    if document["schema"] != 1:
        raise EditorError("{}: expected schema 1".format(MISSIONS))
    unassigned = document["unassigned"]
    missions = document["missions"]
    if not isinstance(unassigned, list) or not isinstance(missions, list):
        raise EditorError("{}: unassigned and missions must be arrays".format(MISSIONS))

    assigned = []
    ids = []
    for mission in missions:
        if not isinstance(mission, dict) or set(mission) != {"id", "title", "summary", "intent", "scenes"}:
            raise EditorError("{}: each mission needs id, title, summary, intent and scenes".format(MISSIONS))
        for key in ("id", "title", "summary", "intent"):
            if not isinstance(mission[key], str) or not mission[key].strip():
                raise EditorError("{}: mission {} has an empty {}".format(MISSIONS, mission.get("id"), key))
        if mission["id"] in ids:
            raise EditorError("{}: duplicate mission id {}".format(MISSIONS, mission["id"]))
        if not isinstance(mission["scenes"], list) or not mission["scenes"]:
            raise EditorError("{}: mission {} has no scenes".format(MISSIONS, mission["id"]))
        local = []
        for name in mission["scenes"]:
            if not isinstance(name, str) or name not in scene_names:
                raise EditorError("{}: mission {} names unknown scene {!r}".format(MISSIONS, mission["id"], name))
            if name in local:
                raise EditorError("{}: mission {} repeats scene {}".format(MISSIONS, mission["id"], name))
            local.append(name)
        ids.append(mission["id"])
        assigned.extend(local)

    if not isinstance(unassigned, list) or any(not isinstance(name, str) or name not in scene_names for name in unassigned):
        raise EditorError("{}: unassigned names an unknown scene".format(MISSIONS))
    if len(set(unassigned)) != len(unassigned):
        raise EditorError("{}: unassigned repeats a scene".format(MISSIONS))
    if len(set(assigned)) != len(assigned):
        raise EditorError("{}: a scene is linked to more than one mission".format(MISSIONS))
    if set(assigned) | set(unassigned) != set(scene_names):
        missing = sorted(set(scene_names) - set(assigned) - set(unassigned))
        raise EditorError("{}: scenes are not grouped: {}".format(MISSIONS, missing))
    if set(assigned) & set(unassigned):
        raise EditorError("{}: a scene is both mission-linked and unassigned".format(MISSIONS))
    return document


def build_schema() -> tuple[dict, dict]:
    scenes_text = SCENES.read_text(encoding="utf-8")
    scenes = story.parse_source(scenes_text, SCENES)
    scene_names = [scene["name"] for scene in scenes["scenes"]]
    missions = load_missions(scene_names)

    campaign = CAMPAIGN.read_text(encoding="utf-8")
    mission_ids = [enum_id(name, "") for name in parse_enum(campaign, "MissionId", CAMPAIGN)]
    if set(mission["id"] for mission in missions["missions"]) != set(mission_ids):
        raise EditorError("{}: mission ids {} do not match campaign::MissionId {}".format(
            MISSIONS, [mission["id"] for mission in missions["missions"]], mission_ids))
    berth_ids = [enum_id(name, "") for name in parse_enum(campaign, "Berth", CAMPAIGN)]
    point_names = parse_enum(campaign, "DockPoint", CAMPAIGN)
    point_ids = [enum_id(name, "dock_point_") for name in point_names]

    strings = parse_fx_strings(FXDATA.read_text(encoding="utf-8"), FXDATA)
    berths = []
    for berth_id in berth_ids:
        symbol = "txt_" + berth_id
        if symbol not in strings:
            raise EditorError("{}: missing berth label {}".format(FXDATA, symbol))
        berths.append({"id": berth_id, "label": strings[symbol]})

    hints = text_images.load_dock_hints()
    hint_berths, hint_points = text_images.dock_hint_layout(campaign, hints)
    if hint_berths != berth_ids or hint_points != point_ids:
        raise EditorError("{}: hint order does not match campaign enums".format(DOCK_HINTS))
    text_images.dock_hint_texts(campaign, hints)

    mission_for_scene = {}
    for mission in missions["missions"]:
        for name in mission["scenes"]:
            mission_for_scene[name] = mission["id"]

    schema = {
        "schema": 1,
        "scenes": [
            {
                "id": scene["name"],
                "title": scene["title"],
                "mission": mission_for_scene.get(scene["name"], ""),
                "cards": [
                    {"kind": "image", "art": card["art"]}
                    if card["kind"] == "image"
                    else {"kind": "text", "paragraphs": len(card["paragraphs"])}
                    for card in scene["cards"]
                ],
            }
            for scene in scenes["scenes"]
        ],
        "sceneOrder": scene_names,
        "unassignedScenes": missions["unassigned"],
        "missionIds": mission_ids,
        "missions": [
            {"id": mission["id"], "title": mission["title"], "scenes": mission["scenes"]}
            for mission in missions["missions"]
        ],
        "art": {
            "file": scenes["art"]["file"],
            "tileWidth": scenes["art"]["tile_width"],
            "tileHeight": scenes["art"]["tile_height"],
            "poses": scenes["art"]["poses"],
            "rows": scenes["art"]["rows"],
        },
        "glyphs": story.GLYPHS,
        "wrapColumns": story.COLUMNS,
        "rowsPerCard": story.ROWS_PER_CARD,
        "berths": berths,
        "points": hints["points"],
        "hintMaxChars": 20,
        "hintPattern": text_images.DOCK_TEXT_RE.pattern,
    }

    mission_data = {
        "schema": 1,
        "unassigned": missions["unassigned"],
        "missions": missions["missions"],
    }
    return schema, mission_data


def render(name: str, value: dict) -> str:
    header = (
        "// Generated by tools/gen_story_editor.py from story/scenes.json,\n"
        "// story/missions.json, story/dock_hints.json and src/campaign.hpp.\n"
        "// Do not edit by hand; run `make gen`.\n"
    )
    return header + "globalThis.{} = {};\n".format(name, json.dumps(value, indent=2, ensure_ascii=False))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if committed outputs are stale")
    args = parser.parse_args(argv)
    try:
        schema, missions = build_schema()
    except (EditorError, story.StoryError, ValueError) as error:
        print("gen_story_editor: {}".format(error), file=sys.stderr)
        return 1
    outputs = (
        (OUTPUT, render("StoryEditorSchema", schema)),
        (MISSIONS_OUTPUT, render("StoryMissions", missions)),
    )
    if args.check:
        stale = [path for path, text in outputs if not path.exists() or path.read_text(encoding="utf-8") != text]
        if stale:
            print("gen_story_editor: stale outputs: {}".format(", ".join(str(path) for path in stale)), file=sys.stderr)
            return 1
        return 0
    for path, text in outputs:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    print("gen_story_editor: wrote {} and {}".format(OUTPUT, MISSIONS_OUTPUT))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

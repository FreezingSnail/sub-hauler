#!/usr/bin/env python3
"""Point the library's existing FX jukebox at the songs in our game cart."""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    library = Path(os.environ.get("ATMLIB2FX_ROOT", ROOT.parent / "ATMlib2FX")).resolve()
    # Import maintained header rendering without creating files in the library.
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(library / "tools"))
    from music_authoring.fx_jukebox import SongRecord, render_header

    manifest = json.loads((ROOT / "fxdata/music/manifest.json").read_text())
    fx_header = (ROOT / "src/fxdata.h").read_text()
    page = int(re.search(r"FX_DATA_PAGE\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)", fx_header)[1], 0)
    records = []
    for index, song in enumerate(manifest["songs"]):
        offset = int(re.search(r"\b" + song["id"] + r"\s*=\s*(0x[0-9A-Fa-f]+)", fx_header)[1], 0)
        records.append(SongRecord(
            index=index, id=song["id"], name=song["name"], folder=song["folder"],
            source=song["source"], stream_name=song["id"], offset=offset,
            length=song["bytes"], stream_loop=True, loop=True,
            loop_tenths=round(song["total_ticks"] * 10 / song["clock"]["effective_tick_rate"]),
        ))
    stage = ROOT / "build/music-viewer/jukebox_fx"
    stage.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(library / "examples/arduino/jukebox_fx/jukebox_fx.ino", stage / "jukebox_fx.ino")
    digest = hashlib.sha256((ROOT / "music/catalog.json").read_bytes()).hexdigest()
    header = render_header(records, digest, page, (ROOT / "fxdata/fxdata-data.bin").stat().st_size)
    header = header.replace(" *   python3 tools/music_authoring/fx_jukebox.py", " *   make gen (Sub-hauler)")
    (stage / "jukebox_fx_songs.h").write_text(header)
    print(f"music-viewer: {len(records)} songs in the library FX jukebox (make music-viewer)")


if __name__ == "__main__":
    main()

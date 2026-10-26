#!/bin/bash
# Single FX generation entry. Every generated artifact must be reproducible by
# running this script from a clean checkout.
set -euo pipefail
cd "$(dirname "$0")/.."

python3 tools/gen_font.py --output fxdata/fonts/font_6x8.png
python3 tools/gen_pilot.py --output images/pilot
python3 tools/gen_sprites.py --input images --output fxdata/Sprites.txt --header src/generated/sprites.hpp --shades 2
python3 tools/gen_sprites.py --input images/pilot --output fxdata/PilotSprites.txt --header src/generated/pilot_sprites.hpp --shades 2
python3 tools/gen_world.py
python3 tools/gen_level.py
python3 tools/gen_cargo.py
python3 tools/gen_music.py
python3 tools/fxdata-build.py fxdata/fxdata.txt
cp fxdata/fxdata.h src/fxdata.h
python3 tools/gen_music_viewer.py

echo "gen: fxdata/fxdata.bin + src/fxdata.h regenerated"

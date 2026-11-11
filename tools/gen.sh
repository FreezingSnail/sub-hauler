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
python3 tools/gen_music_regions.py
python3 tools/gen_panels.py --panels images/panels --campaign images/campaign --pause images/pause
python3 tools/gen_sprites.py --input images/panels --output fxdata/PanelSprites.txt --header src/generated/panel_sprites.hpp --shades 2
python3 tools/gen_sprites.py --input images/story --output fxdata/StorySprites.txt --header src/generated/story_sprites.hpp --shades 2
python3 tools/gen_sprites.py --input images/campaign --output fxdata/CampaignSprites.txt --header src/generated/campaign_sprites.hpp --shades 2
python3 tools/gen_sprites.py --input images/pause --output fxdata/PauseSprites.txt --header src/generated/pause_sprites.hpp --shades 2
python3 tools/gen_hud.py --output images/hud --fxdata fxdata/fxdata.txt
python3 tools/gen_sprites.py --input images/hud --output fxdata/HudSprites.txt --header src/generated/hud_sprites.hpp --shades 2
python3 tools/gen_menu.py
python3 tools/gen_sprites.py --input images/menu --output fxdata/MenuSprites.txt --header src/generated/menu_sprites.hpp --shades 2
python3 tools/gen_splash.py
python3 tools/gen_sprites.py --input images/splash --output fxdata/SplashSprites.txt --header src/generated/splash_sprites.hpp --shades 2
python3 tools/gen_story_beats.py
python3 tools/gen_dock_barks.py
python3 tools/gen_cargo.py
python3 tools/gen_music.py
# Streaming-audio reference PCM and ADPCM are archival comparison evidence, not
# shipping inputs. Rebuild them explicitly with `make gen-audio`.
python3 tools/fxdata-build.py fxdata/fxdata.txt
cp fxdata/fxdata.h src/fxdata.h
python3 tools/gen_music_viewer.py

echo "gen: fxdata/fxdata.bin + src/fxdata.h regenerated"

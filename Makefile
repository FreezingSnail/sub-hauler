.PHONY: help full gen gen-check gen-audio build mini size size-line run debug save-migrate test test-tools docs-check fxtest fxtest-flight fxtest-build fxtest-run clean format music-viewer music-viewer-build render-world

# ----------------------------------------------------------------------------
# Project
# ----------------------------------------------------------------------------
SKETCH        ?= $(basename $(notdir $(firstword $(wildcard *.ino))))
CXX           ?= g++
ARDUINO_CLI   ?= arduino-cli
ATMLIB2FX_ROOT ?= $(firstword $(wildcard ../ATMlib2FX $(HOME)/code/arduboy/ATMlib2FX))
export ATMLIB2FX_ROOT
FQBN          ?= arduboy-homemade:avr:arduboy-fx
MINI_FQBN     ?= arduboy-homemade:avr:arduboy-mini

# One fixed game configuration. Dependencies below are tool/board inputs, not
# gameplay variants. DEBUG=1 is the developer-only map-inspection image; it
# uses isolated output directories so release objects are never reused.
DEBUG         ?= 0
ifneq ($(filter-out 0 1,$(DEBUG)),)
$(error DEBUG must be 0 or 1)
endif
DEBUG_FLAGS   := $(if $(filter 1,$(DEBUG)),-DSH_DEV_MODE=1)
BUILD_DIR     ?= $(if $(filter 1,$(DEBUG)),build/debug,build/arduino)
DIST_DIR      ?= $(if $(filter 1,$(DEBUG)),dist/debug,dist/fx)
MINI_BUILD_DIR ?= build/arduino-mini
MINI_DIST_DIR ?= dist/mini
FXTEST_BUILD_DIR ?= build/fxtest
ARDENS_DEFAULT := $(firstword $(wildcard \
                 ../Ardens/build/Ardens.app/Contents/MacOS/Ardens \
                 $(HOME)/code/arduboy/Ardens/build/Ardens.app/Contents/MacOS/Ardens \
                 $(HOME)/code/Ardens/build/Ardens.app/Contents/MacOS/Ardens))
ARDENS        ?= $(ARDENS_DEFAULT)
# A stale exported ARDENS must not force fxtest to skip when the sibling
# checkout is present. Keep a valid explicit override, otherwise fall back.
ifeq ($(wildcard $(ARDENS)),)
ARDENS := $(ARDENS_DEFAULT)
endif
# Ardens source tree for native absim harness builds. The executable lives at
# build/Ardens.app/Contents/MacOS/Ardens; accept either an app path or a root.
ARDENS_ROOT   ?= $(if $(wildcard $(dir $(ARDENS))/../../../../src/absim.hpp),$(abspath $(dir $(ARDENS))/../../../..),$(abspath $(ARDENS)))

FXDATA_BIN    ?= fxdata/fxdata.bin
FXTEST_MS     ?= 3000
FXTEST_ONLY   ?=
FXTEST_OPTIMIZE ?= --optimize-for-debug
FXTEST_MUSIC_FLAGS ?= -DATM_FX_COMPACT_DIAG=1 -DSERIAL_BUFFER_SIZE=8
FXTEST_SIZE_FLAGS = --build-property compiler.cpp.extra_flags="$(CPP_EXTRA_FLAGS) $(FXTEST_MUSIC_FLAGS)" \
                     --build-property compiler.c.extra_flags="$(C_EXTRA_FLAGS) $(FXTEST_MUSIC_FLAGS)" \
                     --build-property compiler.c.elf.extra_flags="-mrelax"

# -mrelax and shared prologues are the measured shipping flags used by the
# source projects. LTO means only whole-image measurements are meaningful;
# use `make size` after every flash-sensitive change.
RELAX_FLAGS   ?= -mrelax -mcall-prologues
MUSIC_CPP_FLAGS := -DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=32
MUSIC_C_FLAGS   := -DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=32
MUSIC_LIB       := --library "$(ATMLIB2FX_ROOT)"
CPP_EXTRA_FLAGS := $(RELAX_FLAGS) $(DEBUG_FLAGS) $(MUSIC_CPP_FLAGS)
C_EXTRA_FLAGS   := -mrelax $(MUSIC_C_FLAGS)
SIZE_FLAGS    ?= --build-property compiler.cpp.extra_flags="$(CPP_EXTRA_FLAGS)" \
                 --build-property compiler.c.extra_flags="$(C_EXTRA_FLAGS)" \
                 --build-property compiler.c.elf.extra_flags="-mrelax"

AVR_SIZE      ?= $(firstword $(wildcard $(HOME)/Library/Arduino15/packages/arduino/tools/avr-gcc/*/bin/avr-size) $(shell command -v avr-size 2>/dev/null))

# ----------------------------------------------------------------------------
# Generation
# ----------------------------------------------------------------------------
IMAGES        := $(wildcard images/*.png)
PILOT_IMAGES  := $(wildcard images/pilot/*.png)
WORLD_TABLES  := fxdata/tables/world.bin fxdata/tables/world-chart.bin
LEVEL_OUTPUTS := fxdata/tables/exploration.bin fxdata/tables/exploration-chart.bin fxdata/tables/exploration-objects.bin fxdata/tables/exploration-terrain.bin src/generated/level.hpp src/generated/exploration.hpp docs/assets/exploration-terrain.svg docs/assets/exploration-scenery.json
CARGO_TABLES  := fxdata/tables/cargo.bin
MUSIC_SOURCES := music/catalog.json $(wildcard music/*/song.json)
ADPCM_BINS    := $(addprefix fxdata/music/adpcm/,harbor_bubbles.bin kelp_drift.bin sonar_lullaby.bin abyssal_glass.bin tidepool_waltz.bin bubblejet_run.bin)
REGION_OUTPUTS := src/generated/music_regions.hpp fxdata/tables/music-regions.bin
MUSIC_OUTPUTS := $(addprefix fxdata/music/sh_,harbor_bubbles.bin kelp_drift.bin sonar_lullaby.bin abyssal_glass.bin tidepool_waltz.bin bubblejet_run.bin) fxdata/music/manifest.json
STORY_BEAT_INPUTS := tools/gen_story_beats.py story/scenes.json story/art.png
STORY_BEAT_OUTPUTS := fxdata/tables/story-beats.bin src/generated/story_beats.hpp docs/assets/first-beat-animation/frames.js tst/fxdatatest/story_frame_crc.hpp docs/assets/story-cards.png docs/assets/story-cards.json
DOCK_BARK_INPUTS := tools/gen_dock_barks.py src/campaign.hpp src/station_view.hpp src/text.hpp fxdata/fxdata.txt
DOCK_BARK_OUTPUTS := docs/assets/dock-barks.js
FX_INPUTS     := $(IMAGES) fxdata/fxdata.txt tools/gen.sh tools/gen_sprites.py \
                 tools/gen_hud.py images/hud/*.png \
                 tools/gen_menu.py src/station_view.hpp images/menu/*.png \
                 tools/gen_splash.py images/splash/source/snailware_64x64.png images/splash/*.png \
                 tools/gen_panels.py images/panels/*.png images/campaign/*.png images/pause/*.png \
                 images/story/*.png docs/assets/story-flourishes-preview.png \
                 docs/assets/story-flourishes-preview-4x.png \
                 $(STORY_BEAT_INPUTS) $(STORY_BEAT_OUTPUTS) \
                 $(DOCK_BARK_INPUTS) $(DOCK_BARK_OUTPUTS) \
                 src/campaign.hpp src/generated/exploration.hpp \
                 tools/gen_music_regions.py levels/exploration.json $(REGION_OUTPUTS) \
                 tools/gen_font.py tools/gen_pilot.py tools/gen_world.py \
                 tools/gen_cargo.py tools/gen_level.py tools/gen_scenery.py tools/gen_contours.py tools/gen_settlements.py levels/exploration.json $(WORLD_TABLES) $(CARGO_TABLES) \
                 tools/fxdata-build.py tools/gen_music.py tools/gen_music_viewer.py $(MUSIC_SOURCES) $(ADPCM_BINS)

full: gen build

gen:
	./tools/gen.sh

# Archival streaming-audio evidence only. The game ships the ATMlib2FX synth,
# so this is not part of `gen`, `build`, `run` or `gen-check`.
gen-audio:
	python3 tools/gen_audio_ref.py
	python3 tools/gen_audio_adpcm.py

# Determinism gate: regenerate from committed inputs and require every
# generated artifact to be byte-identical. Run `make gen` once before using it
# for the first time.
gen-check:
	@mkdir -p $(BUILD_DIR)/gen-check
	@mkdir -p $(BUILD_DIR)/gen-check/level
	@cp $(LEVEL_OUTPUTS) $(BUILD_DIR)/gen-check/level/
	@mkdir -p $(BUILD_DIR)/gen-check/panels $(BUILD_DIR)/gen-check/campaign
	@cp images/panels/*.png $(BUILD_DIR)/gen-check/panels/
	@cp images/campaign/*.png $(BUILD_DIR)/gen-check/campaign/
	@mkdir -p $(BUILD_DIR)/gen-check/pause
	@cp images/pause/*.png $(BUILD_DIR)/gen-check/pause/
	@mkdir -p $(BUILD_DIR)/gen-check/story
	@cp images/story/*.png $(BUILD_DIR)/gen-check/story/
	@mkdir -p $(BUILD_DIR)/gen-check/story-beats
	@cp $(STORY_BEAT_OUTPUTS) $(BUILD_DIR)/gen-check/story-beats/
	@cp docs/assets/dock-barks.js $(BUILD_DIR)/gen-check/dock-barks.js
	@cp docs/assets/story-flourishes-preview.png $(BUILD_DIR)/gen-check/story-preview.png
	@cp docs/assets/story-flourishes-preview-4x.png $(BUILD_DIR)/gen-check/story-preview-4x.png
	@mkdir -p $(BUILD_DIR)/gen-check/music
	@cp $(MUSIC_OUTPUTS) src/generated/music.hpp $(BUILD_DIR)/gen-check/music/
	@cp fxdata/fxdata.h $(BUILD_DIR)/gen-check/fxdata.h
	@cp fxdata/fxdata.bin $(BUILD_DIR)/gen-check/fxdata.bin
	@cp fxdata/fxdata-save.bin $(BUILD_DIR)/gen-check/fxdata-save.bin
	@cp fxdata/Sprites.txt $(BUILD_DIR)/gen-check/Sprites.txt
	@cp src/generated/sprites.hpp $(BUILD_DIR)/gen-check/sprites.hpp
	@cp fxdata/tables/world.bin $(BUILD_DIR)/gen-check/world.bin
	@cp fxdata/tables/world-chart.bin $(BUILD_DIR)/gen-check/world-chart.bin
	@cp src/generated/world.hpp $(BUILD_DIR)/gen-check/world.hpp
	@cp fxdata/tables/cargo.bin $(BUILD_DIR)/gen-check/cargo.bin
	@cp src/generated/cargo.hpp $(BUILD_DIR)/gen-check/cargo.hpp
	@cp fxdata/fonts/font_6x8.png $(BUILD_DIR)/gen-check/font_6x8.png
	@cp images/pilot/hauler_26x8.png $(BUILD_DIR)/gen-check/hauler_26x8.png
	@cp images/pilot/leviathan_26x8.png $(BUILD_DIR)/gen-check/leviathan_26x8.png
	@cp images/pilot/minnow_26x8.png $(BUILD_DIR)/gen-check/minnow_26x8.png
	@cp fxdata/PilotSprites.txt $(BUILD_DIR)/gen-check/PilotSprites.txt
	@cp src/generated/pilot_sprites.hpp $(BUILD_DIR)/gen-check/pilot_sprites.hpp
	@cp fxdata/PanelSprites.txt $(BUILD_DIR)/gen-check/PanelSprites.txt
	@cp src/generated/panel_sprites.hpp $(BUILD_DIR)/gen-check/panel_sprites.hpp
	@cp fxdata/StorySprites.txt $(BUILD_DIR)/gen-check/StorySprites.txt
	@cp src/generated/story_sprites.hpp $(BUILD_DIR)/gen-check/story_sprites.hpp
	@cp fxdata/CampaignSprites.txt $(BUILD_DIR)/gen-check/CampaignSprites.txt
	@cp src/generated/campaign_sprites.hpp $(BUILD_DIR)/gen-check/campaign_sprites.hpp
	@cp fxdata/PauseSprites.txt $(BUILD_DIR)/gen-check/PauseSprites.txt
	@cp src/generated/pause_sprites.hpp $(BUILD_DIR)/gen-check/pause_sprites.hpp
	@mkdir -p $(BUILD_DIR)/gen-check/hud
	@cp images/hud/*.png $(BUILD_DIR)/gen-check/hud/
	@cp fxdata/HudSprites.txt $(BUILD_DIR)/gen-check/HudSprites.txt
	@cp src/generated/hud_sprites.hpp $(BUILD_DIR)/gen-check/hud_sprites.hpp
	@mkdir -p $(BUILD_DIR)/gen-check/menu
	@cp images/menu/*.png $(BUILD_DIR)/gen-check/menu/
	@cp fxdata/MenuSprites.txt $(BUILD_DIR)/gen-check/MenuSprites.txt
	@cp src/generated/menu_sprites.hpp $(BUILD_DIR)/gen-check/menu_sprites.hpp
	@cp images/splash/startup_128x64.png $(BUILD_DIR)/gen-check/startup_128x64.png
	@cp fxdata/SplashSprites.txt $(BUILD_DIR)/gen-check/SplashSprites.txt
	@cp src/generated/splash_sprites.hpp $(BUILD_DIR)/gen-check/splash_sprites.hpp
	@cp src/generated/music_regions.hpp $(BUILD_DIR)/gen-check/music_regions.hpp
	@cp fxdata/tables/music-regions.bin $(BUILD_DIR)/gen-check/music-regions.bin
	@$(MAKE) --no-print-directory gen
	@cmp -s fxdata/fxdata.h src/fxdata.h || { echo "gen-check: src/fxdata.h is stale; run make gen" >&2; exit 1; }
	@cmp -s fxdata/fxdata.h $(BUILD_DIR)/gen-check/fxdata.h || { echo "gen-check: fxdata header changed" >&2; exit 1; }
	@cmp -s fxdata/fxdata.bin $(BUILD_DIR)/gen-check/fxdata.bin || { echo "gen-check: packed image changed" >&2; exit 1; }
	@cmp -s fxdata/fxdata-save.bin $(BUILD_DIR)/gen-check/fxdata-save.bin || { echo "gen-check: save partition changed" >&2; exit 1; }
	@cmp -s fxdata/Sprites.txt $(BUILD_DIR)/gen-check/Sprites.txt || { echo "gen-check: sprite include changed" >&2; exit 1; }
	@cmp -s src/generated/sprites.hpp $(BUILD_DIR)/gen-check/sprites.hpp || { echo "gen-check: sprite header changed" >&2; exit 1; }
	@cmp -s fxdata/tables/world.bin $(BUILD_DIR)/gen-check/world.bin || { echo "gen-check: world table changed" >&2; exit 1; }
	@cmp -s fxdata/tables/world-chart.bin $(BUILD_DIR)/gen-check/world-chart.bin || { echo "gen-check: world chart table changed" >&2; exit 1; }
	@cmp -s src/generated/world.hpp $(BUILD_DIR)/gen-check/world.hpp || { echo "gen-check: world header changed" >&2; exit 1; }
	@cmp -s fxdata/tables/cargo.bin $(BUILD_DIR)/gen-check/cargo.bin || { echo "gen-check: cargo table changed" >&2; exit 1; }
	@cmp -s src/generated/cargo.hpp $(BUILD_DIR)/gen-check/cargo.hpp || { echo "gen-check: cargo header changed" >&2; exit 1; }
	@cmp -s fxdata/fonts/font_6x8.png $(BUILD_DIR)/gen-check/font_6x8.png || { echo "gen-check: font image changed" >&2; exit 1; }
	@cmp -s images/pilot/hauler_26x8.png $(BUILD_DIR)/gen-check/hauler_26x8.png || { echo "gen-check: hauler art changed" >&2; exit 1; }
	@cmp -s images/pilot/leviathan_26x8.png $(BUILD_DIR)/gen-check/leviathan_26x8.png || { echo "gen-check: leviathan art changed" >&2; exit 1; }
	@cmp -s images/pilot/minnow_26x8.png $(BUILD_DIR)/gen-check/minnow_26x8.png || { echo "gen-check: minnow art changed" >&2; exit 1; }
	@cmp -s fxdata/PilotSprites.txt $(BUILD_DIR)/gen-check/PilotSprites.txt || { echo "gen-check: pilot sprite include changed" >&2; exit 1; }
	@cmp -s src/generated/pilot_sprites.hpp $(BUILD_DIR)/gen-check/pilot_sprites.hpp || { echo "gen-check: pilot sprite header changed" >&2; exit 1; }
	@cmp -s fxdata/PanelSprites.txt $(BUILD_DIR)/gen-check/PanelSprites.txt || { echo "gen-check: panel sprite include changed" >&2; exit 1; }
	@cmp -s src/generated/panel_sprites.hpp $(BUILD_DIR)/gen-check/panel_sprites.hpp || { echo "gen-check: panel sprite header changed" >&2; exit 1; }
	@cmp -s fxdata/StorySprites.txt $(BUILD_DIR)/gen-check/StorySprites.txt || { echo "gen-check: story sprite include changed" >&2; exit 1; }
	@cmp -s src/generated/story_sprites.hpp $(BUILD_DIR)/gen-check/story_sprites.hpp || { echo "gen-check: story sprite header changed" >&2; exit 1; }
	@cmp -s fxdata/CampaignSprites.txt $(BUILD_DIR)/gen-check/CampaignSprites.txt || { echo "gen-check: campaign sprite include changed" >&2; exit 1; }
	@cmp -s src/generated/campaign_sprites.hpp $(BUILD_DIR)/gen-check/campaign_sprites.hpp || { echo "gen-check: campaign sprite header changed" >&2; exit 1; }
	@cmp -s fxdata/PauseSprites.txt $(BUILD_DIR)/gen-check/PauseSprites.txt || { echo "gen-check: pause sprite include changed" >&2; exit 1; }
	@cmp -s src/generated/pause_sprites.hpp $(BUILD_DIR)/gen-check/pause_sprites.hpp || { echo "gen-check: pause sprite header changed" >&2; exit 1; }
	@cmp -s fxdata/HudSprites.txt $(BUILD_DIR)/gen-check/HudSprites.txt || { echo "gen-check: HUD sprites changed" >&2; exit 1; }
	@cmp -s src/generated/hud_sprites.hpp $(BUILD_DIR)/gen-check/hud_sprites.hpp || { echo "gen-check: HUD header changed" >&2; exit 1; }
	@cmp -s fxdata/MenuSprites.txt $(BUILD_DIR)/gen-check/MenuSprites.txt || { echo "gen-check: menu sprites changed" >&2; exit 1; }
	@cmp -s src/generated/menu_sprites.hpp $(BUILD_DIR)/gen-check/menu_sprites.hpp || { echo "gen-check: menu header changed" >&2; exit 1; }
	@cmp -s images/splash/startup_128x64.png $(BUILD_DIR)/gen-check/startup_128x64.png || { echo "gen-check: startup art changed" >&2; exit 1; }
	@cmp -s fxdata/SplashSprites.txt $(BUILD_DIR)/gen-check/SplashSprites.txt || { echo "gen-check: startup data changed" >&2; exit 1; }
	@cmp -s src/generated/splash_sprites.hpp $(BUILD_DIR)/gen-check/splash_sprites.hpp || { echo "gen-check: startup header changed" >&2; exit 1; }
	@for file in images/menu/*.png; do \
		cmp -s "$$file" "$(BUILD_DIR)/gen-check/menu/$${file##*/}" || { echo "gen-check: $$file changed" >&2; exit 1; }; \
	done
	@for file in images/hud/*.png; do \
		cmp -s "$$file" "$(BUILD_DIR)/gen-check/hud/$${file##*/}" || { echo "gen-check: $$file changed" >&2; exit 1; }; \
	done
	@cmp -s src/generated/music_regions.hpp $(BUILD_DIR)/gen-check/music_regions.hpp || { echo "gen-check: music region header changed" >&2; exit 1; }
	@cmp -s fxdata/tables/music-regions.bin $(BUILD_DIR)/gen-check/music-regions.bin || { echo "gen-check: music region plane changed" >&2; exit 1; }
	@for file in $(MUSIC_OUTPUTS) src/generated/music.hpp; do \
		cmp -s "$$file" "$(BUILD_DIR)/gen-check/music/$${file##*/}" || { echo "gen-check: $$file changed" >&2; exit 1; }; \
	done
	@for file in $(LEVEL_OUTPUTS); do \
		cmp -s "$$file" "$(BUILD_DIR)/gen-check/level/$${file##*/}" || { echo "gen-check: $$file changed" >&2; exit 1; }; \
	done
	@for file in images/panels/*.png; do \
		cmp -s "$$file" "$(BUILD_DIR)/gen-check/panels/$${file##*/}" || { echo "gen-check: $$file changed" >&2; exit 1; }; \
	done
	@for file in images/campaign/*.png; do \
		cmp -s "$$file" "$(BUILD_DIR)/gen-check/campaign/$${file##*/}" || { echo "gen-check: $$file changed" >&2; exit 1; }; \
	done
	@for file in images/pause/*.png; do \
		cmp -s "$$file" "$(BUILD_DIR)/gen-check/pause/$${file##*/}" || { echo "gen-check: $$file changed" >&2; exit 1; }; \
	done
	@for file in images/story/*.png; do \
		cmp -s "$$file" "$(BUILD_DIR)/gen-check/story/$${file##*/}" || { echo "gen-check: $$file changed" >&2; exit 1; }; \
	done
	@cmp -s docs/assets/story-flourishes-preview.png $(BUILD_DIR)/gen-check/story-preview.png || { echo "gen-check: story preview changed" >&2; exit 1; }
	@cmp -s docs/assets/story-flourishes-preview-4x.png $(BUILD_DIR)/gen-check/story-preview-4x.png || { echo "gen-check: story 4x preview changed" >&2; exit 1; }
	@cmp -s fxdata/tables/story-beats.bin $(BUILD_DIR)/gen-check/story-beats/story-beats.bin || { echo "gen-check: story beat blob changed" >&2; exit 1; }
	@cmp -s src/generated/story_beats.hpp $(BUILD_DIR)/gen-check/story-beats/story_beats.hpp || { echo "gen-check: story beat header changed" >&2; exit 1; }
	@cmp -s docs/assets/first-beat-animation/frames.js $(BUILD_DIR)/gen-check/story-beats/frames.js || { echo "gen-check: story preview frame map changed" >&2; exit 1; }
	@cmp -s tst/fxdatatest/story_frame_crc.hpp $(BUILD_DIR)/gen-check/story-beats/story_frame_crc.hpp || { echo "gen-check: story device CRC reference changed" >&2; exit 1; }
	@cmp -s docs/assets/story-cards.png $(BUILD_DIR)/gen-check/story-beats/story-cards.png || { echo "gen-check: story card sheet changed" >&2; exit 1; }
	@cmp -s docs/assets/story-cards.json $(BUILD_DIR)/gen-check/story-beats/story-cards.json || { echo "gen-check: story card map changed" >&2; exit 1; }
	@cmp -s docs/assets/dock-barks.js $(BUILD_DIR)/gen-check/dock-barks.js || { echo "gen-check: dock bark preview changed" >&2; exit 1; }
	@echo "gen-check: generated artifacts are deterministic"

# Generation is a prerequisite of the firmware build, so a missing or stale
# src/fxdata.h cannot silently ship.
src/fxdata.h: $(FX_INPUTS)
	./tools/gen.sh

# ----------------------------------------------------------------------------
# Firmware
# ----------------------------------------------------------------------------
music-check:
	@test -f "$(ATMLIB2FX_ROOT)/library.properties" || { \
		echo "music-check: set ATMLIB2FX_ROOT to the ATMlib2FX checkout" >&2; exit 1; }

build: music-check src/fxdata.h
	mkdir -p $(DIST_DIR)
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --optimize-for-debug \
		$(MUSIC_LIB) --build-path $(BUILD_DIR) --output-dir $(DIST_DIR) \
		$(SIZE_FLAGS) "$(SKETCH).ino"

mini: music-check src/fxdata.h
	mkdir -p $(MINI_DIST_DIR)
	$(ARDUINO_CLI) compile --fqbn "$(MINI_FQBN)" --optimize-for-debug \
		$(MUSIC_LIB) --build-path $(MINI_BUILD_DIR) --output-dir $(MINI_DIST_DIR) \
		$(SIZE_FLAGS) "$(SKETCH).ino"

ELF := $(DIST_DIR)/$(SKETCH).ino.elf

size: build
	@printf 'size: image=%s\n' "$(ELF)"
	@$(AVR_SIZE) -A "$(ELF)" | awk '$$1==".text"{t=$$2} $$1==".data"{d=$$2} $$1==".bss"{b=$$2} END { flash=t+d; ram=d+b; printf "size: .text=%d .data=%d .bss=%d\n", t, d, b; printf "size: flash=%d/29696 (%d free)  ram=%d/2560\n", flash, 29696-flash, ram }'

size-line: build
	@$(AVR_SIZE) -A "$(ELF)" | awk '$$1==".text"{t=$$2} $$1==".data"{d=$$2} $$1==".bss"{b=$$2} END { flash=t+d; ram=d+b; printf "size: flash=%d/29696 (%d free)  ram=%d/2560\n", flash, 29696-flash, ram }'

run: build
	@test -x "$(ARDENS)" || { echo "run: Ardens not found at $(ARDENS); set ARDENS=/path/to/Ardens" >&2; exit 1; }
	@test -f "$(FXDATA_BIN)" || { echo "run: FX image missing at $(FXDATA_BIN); run make gen" >&2; exit 1; }
	"$(ARDENS)" display=ssd1306 fxport=d1 file="$(ELF)" file="$(FXDATA_BIN)"

debug:
	$(MAKE) --no-print-directory run DEBUG=1

# Ardens keys saves by a hash of the program and cart. Copy an existing save
# under the current image's key without changing its payload or the source.
save-migrate: build
	@test -n "$(SAVE)" || { echo "save-migrate: set SAVE=/path/to/absim_*.save" >&2; exit 1; }
	python3 tools/ardens_save.py migrate --save "$(SAVE)" --hex "$(DIST_DIR)/$(SKETCH).ino.hex" --fxdata "$(FXDATA_BIN)"

# ----------------------------------------------------------------------------
# Host tests
# ----------------------------------------------------------------------------
music-viewer-build: gen
	mkdir -p build/music-viewer/out
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --library "$(ATMLIB2FX_ROOT)" \
		--build-path "$(CURDIR)/build/music-viewer/avr" --output-dir build/music-viewer/out \
		--build-property "compiler.cpp.extra_flags=-DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=32" \
		--build-property "compiler.c.extra_flags=-DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=32" \
		build/music-viewer/jukebox_fx

music-viewer: music-viewer-build
	@test -x "$(ARDENS)" || { echo "music-viewer: set ARDENS=/path/to/Ardens" >&2; exit 1; }
	"$(ARDENS)" display=ssd1306 fxport=d1 file="$(CURDIR)/build/music-viewer/out/jukebox_fx.ino.hex" file="$(CURDIR)/fxdata/fxdata.bin"

# Preview export only: reads generated game data without changing FX artifacts.
WORLD_RENDER_DIR ?= build/world-render
render-world:
	mkdir -p $(WORLD_RENDER_DIR)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Isrc tools/render_world.cpp -o $(WORLD_RENDER_DIR)/render-world
	$(WORLD_RENDER_DIR)/render-world fxdata/tables/exploration-terrain.bin $(WORLD_RENDER_DIR)/world.pbm
	python3 tools/render_world.py --input $(WORLD_RENDER_DIR)/world.pbm --output-dir $(WORLD_RENDER_DIR)
	cp $(WORLD_RENDER_DIR)/game-world.png docs/assets/exploration-world-2026-11-09.png
	cp $(WORLD_RENDER_DIR)/game-world-music.png docs/assets/exploration-world-music-2026-11-09.png
	cp $(WORLD_RENDER_DIR)/game-world-native.png docs/assets/exploration-world-native-2026-11-09.png

TEST_OUT      := build/tests/host
TEST_SOURCES  := tst/main.cpp
CXXFLAGS      ?= -std=c++17 -O0 -g3 -Wall -Wextra

test:
	@mkdir -p $(dir $(TEST_OUT))
	$(CXX) $(CXXFLAGS) -Isrc -Itools/audio_reference -DTEST $(TEST_SOURCES) -o $(TEST_OUT)
	./$(TEST_OUT)

test-tools:
	@test -f build/audio-reference/sh_harbor_bubbles.pcm || $(MAKE) --no-print-directory gen-audio
	python3 -m unittest discover -s tools/tests -p 'test_*.py' -v

docs-check:
	python3 tools/check_docs.py
	node --test tools/tests/story_animation.test.cjs

# ----------------------------------------------------------------------------
# Device tests (Ardens serial capture)
# ----------------------------------------------------------------------------
FXTEST_INOS   := $(wildcard tst/fxdatatest/test_*.ino)
FXTEST_NAMES  := $(basename $(notdir $(FXTEST_INOS)))
# The integrated synth/music suite replaces the retired standalone player
# admission suite. The non-music stack probe is kept as an explicit target.
FXTEST_NAMES  := $(filter-out test_memory,$(FXTEST_NAMES))
FXTEST_DEFAULT := $(filter-out test_flight test_save_runtime_page,$(FXTEST_NAMES))
FXTEST_RUN    := $(if $(strip $(FXTEST_ONLY)),$(filter $(FXTEST_ONLY),$(FXTEST_NAMES)),$(FXTEST_DEFAULT))

fxtest: music-check fxtest-build fxtest-run

# Emulator-only: points FX::programSavePage at the 4 KiB data tail to prove
# seekSave/erase/page writes share the runtime page. It erases that alternate
# page; never run it against a physical cart.
fxtest-save-runtime-page:
	@printf '%s\n' "WARNING: emulator-only runtime save-page test; erases alternate cart page. Do not run on hardware."
	@$(MAKE) --no-print-directory fxtest FXTEST_ONLY=test_save_runtime_page \
		FXTEST_BUILD_DIR=build/fxtest-save-runtime-page FXTEST_MS=12000

# Exact production-chain device probe (updateStation -> requestSave ->
# saveVoyageNow) with the real sketch and synth music. Emulator-only real cart
# writes.
fxtest-save-chain:
	@$(MAKE) --no-print-directory fxtest FXTEST_ONLY=test_save_chain \
		FXTEST_BUILD_DIR=build/fxtest-save-chain FXTEST_MS=15000

# Complete-flight timing/coverage probe with active music. It drives the real
# production application for ~11 s of scheduled frames, so it needs a longer
# serial window than the short smoke suites.
fxtest-flight:
	@$(MAKE) --no-print-directory fxtest FXTEST_ONLY=test_flight \
		FXTEST_BUILD_DIR=build/fxtest-flight FXTEST_MS=25000 \
		FXTEST_OPTIMIZE=

fxtest-build: $(addprefix fxtest-build-,$(FXTEST_RUN))

fxtest-build-%:
	@stage="$(FXTEST_BUILD_DIR)/$*"; \
	rm -rf "$$stage"; \
	mkdir -p "$$stage"; \
	cp -R src "$$stage/src"; \
	cp "tst/fxdatatest/$*.ino" "$$stage/"; \
	cp tst/fxdatatest/*.hpp "$$stage/" 2>/dev/null || true; \
	echo "build: $*"; \
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" $(FXTEST_OPTIMIZE) \
		$(MUSIC_LIB) --build-path "$$stage/arduino" --output-dir "$$stage/output" \
		$(FXTEST_SIZE_FLAGS) "$$stage/$*.ino"

fxtest-run:
	@if [ -z "$(ARDENS)" ] || [ ! -x "$(ARDENS)" ]; then \
		echo "fxtest-run: SKIPPED (Ardens unavailable; set ARDENS=/path/to/Ardens)"; \
	else \
		failed=0; \
		for name in $(FXTEST_RUN); do \
			stage="$(FXTEST_BUILD_DIR)/$$name"; \
			cp -f "$(FXDATA_BIN)" "$$stage/fxdata.bin"; \
			ms="$(FXTEST_MS)"; \
			case "$$name" in test_startup|test_save_chain|test_story) \
				if [ "$$ms" -lt 8000 ]; then ms=8000; fi ;; \
			esac; \
			echo "=== $$name ==="; \
			if out="$$("$(ARDENS)" captureserial=$$ms fxport=d1 display=ssd1306 file="$$stage/output/$$name.ino.hex" file="$$stage/fxdata.bin" 2>&1)"; then \
				printf '%s\n' "$$out"; \
			else \
				printf '%s\n' "$$out"; failed=1; continue; \
			fi; \
			if printf '%s\n' "$$out" | tr -d '\r' | grep -qx 'P'; then \
				echo "$$name: PASS"; \
			else \
				echo "$$name: FAIL (missing P marker)"; failed=1; \
			fi; \
		done; \
		test $$failed -eq 0; \
	fi

# ----------------------------------------------------------------------------
# Utility
# ----------------------------------------------------------------------------
clean:
	rm -rf build dist $(BUILD_DIR) $(DIST_DIR)
	find tools tst -type d -name __pycache__ -prune -exec rm -rf {} +

format:
	@find src tst -type f \( -name '*.hpp' -o -name '*.cpp' -o -name '*.ino' \) \
		! -path 'src/external/*' ! -path 'src/fxdata.h' ! -path 'src/generated/*' \
		| xargs clang-format -i --style=file

help:
	@printf '%s\n' \
		'Targets:' \
		'  make full       generate FX data, then build the FX firmware' \
		'  make gen        regenerate fxdata/Sprites.txt, fxdata.bin and headers' \
		'  make gen-check  prove generation is deterministic (run make gen first)' \
		'  make gen-audio  rebuild archival reference PCM + ADPCM evidence only' \
		'  make build      compile the fixed FX firmware into dist/fx/' \
		'  make mini       compile the fixed Arduboy Mini firmware into dist/mini/' \
		'  make size       whole-image flash/RAM report' \
		'  make run        build, then launch Ardens with the ELF + FX image' \
		'  make debug      build the developer no-clip image and launch Ardens (DEBUG=1)' \
		'  make save-migrate SAVE=...  copy an Ardens save to the current image hash' \
		'  make render-world export the native world, annotated overview and music map' \
		'  make music-viewer audition the underwater tracks in the ATMlib2FX jukebox' \
		'  make test       host C++ tests (g++, no device required)' \
		'  make test-tools Python tooling unittest suite' \
		'  make docs-check local links, active selectors and story-preview tests' \
		'  make fxtest     compile and run device suites through Ardens' \
		'  make fxtest-save-chain  exact production save-chain stack/audio probe' \
		'  make fxtest-flight  complete-flight timing/coverage probe with active music' \
		'  make fxtest-save-runtime-page  emulator-only runtime page probe (erases alternate page)' \
		'  make clean      remove build/ and dist/' \
		'' \
		'Overrides: DEBUG=0|1, SAVE, SKETCH, FQBN, MINI_FQBN, ARDUINO_CLI, ARDENS, FXTEST_ONLY, FXTEST_MS, CXX.'

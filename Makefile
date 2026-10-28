.PHONY: help full gen gen-check features-check feature-report build mini size size-line size-matrix run dev debug test test-tools fxtest fxtest-build fxtest-run clean format music-viewer music-viewer-build render-world

# ----------------------------------------------------------------------------
# Project
# ----------------------------------------------------------------------------
SKETCH        ?= $(basename $(notdir $(firstword $(wildcard *.ino))))
CXX           ?= g++
ARDUINO_CLI   ?= arduino-cli
ATMLIB2FX_ROOT ?= $(firstword $(wildcard ../ATMlib2FX $(HOME)/code/arduboy/ATMlib2FX))
ARDUBOYSTREAMFX_ROOT ?= $(firstword $(wildcard ../ArduboyStreamFX $(HOME)/code/arduboy/ArduboyStreamFX))
export ATMLIB2FX_ROOT
export ARDUBOYSTREAMFX_ROOT
FQBN          ?= arduboy-homemade:avr:arduboy-fx
MINI_FQBN     ?= arduboy-homemade:avr:arduboy-mini

# Feature profiles are owned by tools/features.py. Profile-specific build and
# output directories prevent objects compiled with one flag set from being
# reused by another.
PROFILE       ?= full
DEV_MODE      ?= 0
ifneq ($(DEV_MODE),0)
ifneq ($(DEV_MODE),1)
$(error DEV_MODE must be 0 or 1)
endif
endif
DEV_FLAGS     := -DSH_DEV_MODE=$(DEV_MODE)
# Shipping builds use the Arduboy2 USB-free entry point. Development builds
# default to the USB runtime so serial diagnostics keep working; a USB release
# image can also be measured explicitly with RELEASE_NO_USB=0.
RELEASE_NO_USB ?= $(if $(filter 1,$(DEV_MODE)),0,1)
ifneq ($(RELEASE_NO_USB),0)
ifneq ($(RELEASE_NO_USB),1)
$(error RELEASE_NO_USB must be 0 or 1)
endif
endif
USB_FLAGS     := -DSH_RELEASE_NO_USB=$(RELEASE_NO_USB)
FEATURES_PY   ?= python3 tools/features.py
PROFILES      := $(shell $(FEATURES_PY) --list 2>/dev/null)
FEATURE_FLAGS := $(shell $(FEATURES_PY) --profile "$(PROFILE)" --flags 2>/dev/null)
STREAMFX_LIB := $(if $(findstring -DSH_FEATURE_MUSIC=1,$(FEATURE_FLAGS)),--library "$(ARDUBOYSTREAMFX_ROOT)",)
BUILD_DIR     ?= $(if $(filter 1,$(DEV_MODE)),build/dev/$(PROFILE),build/$(PROFILE))
DIST_DIR      ?= $(if $(filter 1,$(DEV_MODE)),dist/dev/$(PROFILE),dist/$(PROFILE))
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
FXTEST_BUILD_DIR ?= $(BUILD_DIR)/fxtest
FXTEST_STREAMFX_FLAGS ?= -DSTREAMFX_DIAG=1
FXTEST_SIZE_FLAGS = --build-property compiler.cpp.extra_flags="$(CPP_EXTRA_FLAGS) $(FXTEST_STREAMFX_FLAGS)" \
                     --build-property compiler.c.extra_flags="$(C_EXTRA_FLAGS) $(FXTEST_STREAMFX_FLAGS)" \
                     --build-property compiler.c.elf.extra_flags="-mrelax"

# -mrelax and shared prologues are the measured shipping flags used by the
# source projects. LTO means only whole-image measurements are meaningful;
# use `make size` after every flash-sensitive change.
RELAX_FLAGS   ?= -mrelax -mcall-prologues
CPP_EXTRA_FLAGS := $(RELAX_FLAGS) $(FEATURE_FLAGS) $(DEV_FLAGS) $(USB_FLAGS)
C_EXTRA_FLAGS   := -mrelax $(FEATURE_FLAGS) $(DEV_FLAGS) $(USB_FLAGS)
SIZE_FLAGS    ?= --build-property compiler.cpp.extra_flags="$(CPP_EXTRA_FLAGS)" \
                 --build-property compiler.c.extra_flags="$(C_EXTRA_FLAGS)" \
                 --build-property compiler.c.elf.extra_flags="-mrelax"

AVR_SIZE      ?= $(firstword $(wildcard $(HOME)/Library/Arduino15/packages/arduino/tools/avr-gcc/*/bin/avr-size) $(shell command -v avr-size 2>/dev/null))
AVR_NM        ?= $(firstword $(wildcard $(HOME)/Library/Arduino15/packages/arduino/tools/avr-gcc/*/bin/avr-nm) $(shell command -v avr-nm 2>/dev/null))
AUDIO_COMPARE_DIR ?= build/audio-compare
AUDIO_COMPARE_EVIDENCE ?= $(AUDIO_COMPARE_DIR)/evidence
AUDIO_COMPARE_ABSIM_OBJS := $(filter-out build/audio-reference/native/obj/capture.o,$(wildcard build/audio-reference/native/obj/*.o))
AUDIO_COMPARE_FLAGS := -mrelax -mcall-prologues -DSH_RELEASE_NO_USB=0

# ----------------------------------------------------------------------------
# Generation
# ----------------------------------------------------------------------------
IMAGES        := $(wildcard images/*.png)
PILOT_IMAGES  := $(wildcard images/pilot/*.png)
WORLD_TABLES  := fxdata/tables/world.bin fxdata/tables/world-chart.bin
LEVEL_OUTPUTS := fxdata/tables/exploration.bin fxdata/tables/exploration-chart.bin fxdata/tables/exploration-objects.bin fxdata/tables/exploration-terrain.bin src/generated/level.hpp src/generated/exploration.hpp docs/assets/exploration-terrain.svg docs/assets/exploration-scenery.json
CARGO_TABLES  := fxdata/tables/cargo.bin
MUSIC_SOURCES := music/catalog.json $(wildcard music/*/song.json)
ADPCM_OUTPUTS := $(addprefix fxdata/music/adpcm/,harbor_bubbles.bin kelp_drift.bin sonar_lullaby.bin abyssal_glass.bin tidepool_waltz.bin bubblejet_run.bin) fxdata/music/adpcm-manifest.json src/generated/audio.hpp src/generated/streamfx.hpp
REGION_OUTPUTS := src/generated/music_regions.hpp fxdata/tables/music-regions.bin
MUSIC_OUTPUTS := $(addprefix fxdata/music/sh_,harbor_bubbles.bin kelp_drift.bin sonar_lullaby.bin abyssal_glass.bin tidepool_waltz.bin bubblejet_run.bin) fxdata/music/manifest.json fxdata/music/reference-manifest.json $(ADPCM_OUTPUTS)
FX_INPUTS     := $(IMAGES) fxdata/fxdata.txt tools/gen.sh tools/gen_sprites.py \
                 tools/gen_panels.py images/panels/*.png images/campaign/*.png images/pause/*.png \
                 src/campaign.hpp src/generated/level.hpp \
                 tools/gen_music_regions.py levels/exploration.json $(REGION_OUTPUTS) \
                 tools/gen_font.py tools/gen_pilot.py tools/gen_world.py \
                 tools/gen_cargo.py tools/gen_level.py tools/gen_scenery.py tools/gen_contours.py tools/gen_settlements.py levels/exploration.json $(WORLD_TABLES) $(CARGO_TABLES) \
                 tools/fxdata-build.py tools/gen_music.py tools/gen_music_viewer.py tools/gen_audio_adpcm.py $(MUSIC_SOURCES) $(ADPCM_OUTPUTS)

full: gen build

gen:
	./tools/gen.sh

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
	@cp fxdata/CampaignSprites.txt $(BUILD_DIR)/gen-check/CampaignSprites.txt
	@cp src/generated/campaign_sprites.hpp $(BUILD_DIR)/gen-check/campaign_sprites.hpp
	@cp fxdata/PauseSprites.txt $(BUILD_DIR)/gen-check/PauseSprites.txt
	@cp src/generated/pause_sprites.hpp $(BUILD_DIR)/gen-check/pause_sprites.hpp
	@cp src/generated/music_regions.hpp $(BUILD_DIR)/gen-check/music_regions.hpp
	@cp fxdata/tables/music-regions.bin $(BUILD_DIR)/gen-check/music-regions.bin
	@cp src/generated/streamfx.hpp $(BUILD_DIR)/gen-check/streamfx.hpp
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
	@cmp -s fxdata/CampaignSprites.txt $(BUILD_DIR)/gen-check/CampaignSprites.txt || { echo "gen-check: campaign sprite include changed" >&2; exit 1; }
	@cmp -s src/generated/campaign_sprites.hpp $(BUILD_DIR)/gen-check/campaign_sprites.hpp || { echo "gen-check: campaign sprite header changed" >&2; exit 1; }
	@cmp -s fxdata/PauseSprites.txt $(BUILD_DIR)/gen-check/PauseSprites.txt || { echo "gen-check: pause sprite include changed" >&2; exit 1; }
	@cmp -s src/generated/pause_sprites.hpp $(BUILD_DIR)/gen-check/pause_sprites.hpp || { echo "gen-check: pause sprite header changed" >&2; exit 1; }
	@cmp -s src/generated/music_regions.hpp $(BUILD_DIR)/gen-check/music_regions.hpp || { echo "gen-check: music region header changed" >&2; exit 1; }
	@cmp -s fxdata/tables/music-regions.bin $(BUILD_DIR)/gen-check/music-regions.bin || { echo "gen-check: music region plane changed" >&2; exit 1; }
	@cmp -s src/generated/streamfx.hpp $(BUILD_DIR)/gen-check/streamfx.hpp || { echo "gen-check: streamfx header changed" >&2; exit 1; }
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
	@echo "gen-check: generated artifacts are deterministic"

# Generation is a prerequisite of the firmware build, so a missing or stale
# src/fxdata.h cannot silently ship.
src/fxdata.h: $(FX_INPUTS)
	./tools/gen.sh

# ----------------------------------------------------------------------------
# Feature profiles
# ----------------------------------------------------------------------------
features-check:
	@$(FEATURES_PY) --profile "$(PROFILE)" --check

feature-report: features-check
	@$(FEATURES_PY) --profile "$(PROFILE)" --report

# ----------------------------------------------------------------------------
# Firmware
# ----------------------------------------------------------------------------
streamfx-check:
	@if printf '%s' "$(FEATURE_FLAGS)" | grep -q -- "-DSH_FEATURE_MUSIC=1"; then \
		test -f "$(ARDUBOYSTREAMFX_ROOT)/library.properties" || { \
			echo "streamfx-check: set ARDUBOYSTREAMFX_ROOT to the ArduboyStreamFX checkout" >&2; exit 1; }; \
	fi

build: features-check streamfx-check src/fxdata.h
	mkdir -p $(DIST_DIR)
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --optimize-for-debug \
		$(STREAMFX_LIB) --build-path $(BUILD_DIR)/arduino --output-dir $(DIST_DIR) \
		$(SIZE_FLAGS) "$(SKETCH).ino"

mini: features-check streamfx-check src/fxdata.h
	mkdir -p $(DIST_DIR)
	$(ARDUINO_CLI) compile --fqbn "$(MINI_FQBN)" --optimize-for-debug \
		$(STREAMFX_LIB) --build-path $(BUILD_DIR)/arduino-mini --output-dir $(DIST_DIR) \
		$(SIZE_FLAGS) "$(SKETCH).ino"

ELF := $(DIST_DIR)/$(SKETCH).ino.elf

size: build
	@printf 'size: image=%s\n' "$(ELF)"
	@$(AVR_SIZE) -A "$(ELF)" | awk '$$1==".text"{t=$$2} $$1==".data"{d=$$2} $$1==".bss"{b=$$2} END { flash=t+d; ram=d+b; printf "size: .text=%d .data=%d .bss=%d\n", t, d, b; printf "size: flash=%d/29696 (%d free)  ram=%d/2560\n", flash, 29696-flash, ram }'

size-line: build
	@$(AVR_SIZE) -A "$(ELF)" | awk '$$1==".text"{t=$$2} $$1==".data"{d=$$2} $$1==".bss"{b=$$2} END { flash=t+d; ram=d+b; printf "size: flash=%d/29696 (%d free)  ram=%d/2560\n", flash, 29696-flash, ram }'

# Release gate: every profile builds in its own directory, so no object file
# can be shared across feature sets.
size-matrix:
	@test -n "$(PROFILES)" || { echo "size-matrix: no profiles reported by $(FEATURES_PY)" >&2; exit 1; }
	@set -e; \
	for profile in $(PROFILES); do \
		printf '%s\n' "=== $$profile ==="; \
		$(MAKE) --no-print-directory size PROFILE=$$profile; \
	done

run: build
	@test -x "$(ARDENS)" || { echo "run: Ardens not found at $(ARDENS); set ARDENS=/path/to/Ardens" >&2; exit 1; }
	@test -f "$(FXDATA_BIN)" || { echo "run: FX image missing at $(FXDATA_BIN); run make gen" >&2; exit 1; }
	"$(ARDENS)" display=ssd1306 fxport=d1 file="$(ELF)" file="$(FXDATA_BIN)"

dev:
	$(MAKE) run DEV_MODE=1

debug: build
	@test -x "$(ARDENS)" || { echo "debug: Ardens not found at $(ARDENS); set ARDENS=/path/to/Ardens" >&2; exit 1; }
	"$(ARDENS)" display=ssd1306 fxport=d1 file="$(ELF)" file="$(FXDATA_BIN)"

# ----------------------------------------------------------------------------
# Host tests
# ----------------------------------------------------------------------------
music-viewer-build: gen
	mkdir -p build/music-viewer/out
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --library "$(ATMLIB2FX_ROOT)" \
		--build-path "$(CURDIR)/build/music-viewer/avr" --output-dir build/music-viewer/out \
		--build-property "compiler.cpp.extra_flags=-DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=64" \
		--build-property "compiler.c.extra_flags=-DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=64" \
		build/music-viewer/jukebox_fx

music-viewer: music-viewer-build
	@test -x "$(ARDENS)" || { echo "music-viewer: set ARDENS=/path/to/Ardens" >&2; exit 1; }
	"$(ARDENS)" display=ssd1306 fxport=d1 file="$(CURDIR)/build/music-viewer/out/jukebox_fx.ino.hex" file="$(CURDIR)/fxdata/fxdata.bin"

# ----------------------------------------------------------------------------
# Emulator comparison: two real backends, one shared UI and one complete cart
# ----------------------------------------------------------------------------
AUDIO_COMPARE_LIVE_OUT   := $(AUDIO_COMPARE_DIR)/live/out
AUDIO_COMPARE_SAMPLED_OUT := $(AUDIO_COMPARE_DIR)/sampled/out

audio-compare-build: audio-compare-build-live audio-compare-build-sampled

audio-compare-build-live: gen
	@test -f "$(ATMLIB2FX_ROOT)/library.properties" || { \
		echo "audio-compare-build-live: set ATMLIB2FX_ROOT to the ATMlib2FX checkout" >&2; exit 1; }
	mkdir -p "$(AUDIO_COMPARE_LIVE_OUT)"
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --optimize-for-debug \
		--library "$(ATMLIB2FX_ROOT)" \
		--build-path "$(AUDIO_COMPARE_DIR)/live/arduino" \
		--output-dir "$(AUDIO_COMPARE_LIVE_OUT)" \
		--build-property "compiler.cpp.extra_flags=$(AUDIO_COMPARE_FLAGS) -I$(CURDIR)/tools/audio_compare -DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=64 -DATM_FX_COMPACT_LOOP_STREAMING=1 -DATM_FX_COMPACT_DIAG=1" \
		--build-property "compiler.c.extra_flags=-mrelax -DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=64 -DATM_FX_COMPACT_LOOP_STREAMING=1 -DATM_FX_COMPACT_DIAG=1" \
		tools/audio_compare/live

audio-compare-build-sampled: gen
	mkdir -p "$(AUDIO_COMPARE_SAMPLED_OUT)"
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --optimize-for-debug \
		--build-path "$(AUDIO_COMPARE_DIR)/sampled/arduino" \
		--output-dir "$(AUDIO_COMPARE_SAMPLED_OUT)" \
		--build-property "compiler.cpp.extra_flags=$(AUDIO_COMPARE_FLAGS) -I$(CURDIR)/tools/audio_compare -DSH_AUDIO_DIAG=1" \
		--build-property "compiler.c.extra_flags=-mrelax" \
		tools/audio_compare/sampled

audio-compare-live: audio-compare-build-live
	@test -x "$(ARDENS)" || { echo "audio-compare-live: set ARDENS=/path/to/Ardens" >&2; exit 1; }
	"$(ARDENS)" volume=100 display=ssd1306 fxport=d1 file="$(CURDIR)/$(AUDIO_COMPARE_LIVE_OUT)/live.ino.elf" file="$(CURDIR)/$(FXDATA_BIN)"

audio-compare-sampled: audio-compare-build-sampled
	@test -x "$(ARDENS)" || { echo "audio-compare-sampled: set ARDENS=/path/to/Ardens" >&2; exit 1; }
	"$(ARDENS)" volume=100 display=ssd1306 fxport=d1 file="$(CURDIR)/$(AUDIO_COMPARE_SAMPLED_OUT)/sampled.ino.elf" file="$(CURDIR)/$(FXDATA_BIN)"

# Build-only serial evidence variants. They never replace the delivered images;
# the auto-demo hook exercises all six cues and A/B controls without a GUI.
audio-compare-serial: gen
	@test -f "$(ATMLIB2FX_ROOT)/library.properties" || { \
		echo "audio-compare-serial: set ATMLIB2FX_ROOT to the ATMlib2FX checkout" >&2; exit 1; }
	@test -x "$(ARDENS)" || { echo "audio-compare-serial: set ARDENS=/path/to/Ardens" >&2; exit 1; }
	@mkdir -p "$(AUDIO_COMPARE_DIR)/serial-live/out" "$(AUDIO_COMPARE_DIR)/serial-sampled/out" "$(AUDIO_COMPARE_EVIDENCE)"
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --optimize-for-debug \
		--library "$(ATMLIB2FX_ROOT)" \
		--build-path "$(AUDIO_COMPARE_DIR)/serial-live/arduino" \
		--output-dir "$(AUDIO_COMPARE_DIR)/serial-live/out" \
		--build-property "compiler.cpp.extra_flags=$(AUDIO_COMPARE_FLAGS) -I$(CURDIR)/tools/audio_compare -DSH_AUDIO_COMPARE_AUTODEMO=1 -DSH_AUDIO_COMPARE_SERIAL=1 -DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=64 -DATM_FX_COMPACT_LOOP_STREAMING=1 -DATM_FX_COMPACT_DIAG=1" \
		--build-property "compiler.c.extra_flags=-mrelax -DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=64 -DATM_FX_COMPACT_LOOP_STREAMING=1 -DATM_FX_COMPACT_DIAG=1" \
		tools/audio_compare/live
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --optimize-for-debug \
		--build-path "$(AUDIO_COMPARE_DIR)/serial-sampled/arduino" \
		--output-dir "$(AUDIO_COMPARE_DIR)/serial-sampled/out" \
		--build-property "compiler.cpp.extra_flags=$(AUDIO_COMPARE_FLAGS) -I$(CURDIR)/tools/audio_compare -DSH_AUDIO_COMPARE_AUTODEMO=1 -DSH_AUDIO_COMPARE_SERIAL=1 -DSH_AUDIO_DIAG=1" \
		--build-property "compiler.c.extra_flags=-mrelax" \
		tools/audio_compare/sampled
	@set -e; \
	for kind in live sampled; do \
		echo "=== audio-compare serial $$kind ==="; \
		log="$(AUDIO_COMPARE_EVIDENCE)/$$kind-ardens-serial.log"; \
		if "$(ARDENS)" captureserial=28000 display=ssd1306 fxport=d1 \
			file="$(CURDIR)/$(AUDIO_COMPARE_DIR)/serial-$$kind/out/$$kind.ino.hex" \
			file="$(CURDIR)/$(FXDATA_BIN)" >"$$log" 2>&1; then \
			cat "$$log"; \
		else \
			cat "$$log"; exit 1; \
		fi; \
	done

# Native absim harness: injects buttons, captures raw PWM, checks the six cues
# and the restart/pause/resume paths, and reads audioCompareDiag from SRAM.
audio-compare-harness:
	@test -d build/audio-reference/native/obj || { \
		echo "audio-compare-harness: build absim objects first (see tools/audio_reference/README.md)" >&2; exit 1; }
	@mkdir -p "$(AUDIO_COMPARE_DIR)/native"
	c++ -std=c++17 -O2 -DNDEBUG -DARDENS_NO_DEBUGGER -DARDENS_NO_SNAPSHOTS \
		-DARDENS_VERSION_MAJOR=0 -DARDENS_VERSION_MINOR=0 -DARDENS_VERSION_PATCH=0 \
		-DMINIZ_STATIC_DEFINE -I "$(ARDENS_ROOT)/src" -I "$(ARDENS_ROOT)/deps" \
		-I "$(ARDENS_ROOT)/deps/bitsery/include" -I "$(ARDENS_ROOT)/deps/miniz" \
		-I build/audio-reference/native/include \
		-c tools/audio_compare/harness.cpp -o "$(AUDIO_COMPARE_DIR)/native/harness.o"
	c++ -O2 "$(AUDIO_COMPARE_DIR)/native/harness.o" $(AUDIO_COMPARE_ABSIM_OBJS) \
		-o "$(AUDIO_COMPARE_DIR)/native/harness" -lm

audio-compare-verify: audio-compare-build audio-compare-harness
	@mkdir -p "$(AUDIO_COMPARE_EVIDENCE)"
	@set -e; \
	for kind in live sampled; do \
		out="$(AUDIO_COMPARE_DIR)/$$kind/out"; \
		elf="$$out/$$kind.ino.elf"; \
		raw=$$($(AVR_NM) "$$elf" | awk '$$3=="audioCompareDiag"{print $$1}'); \
		test -n "$$raw" || { echo "audio-compare-verify: audioCompareDiag missing in $$elf" >&2; exit 1; }; \
		addr=$$((0x$$raw)); \
		if [ $$addr -ge 8388608 ]; then addr=$$((addr - 8388608)); fi; \
		diag=$$(printf '0x%x' $$addr); \
		player_arg=; \
		if [ "$$kind" = sampled ]; then \
			praw=$$($(AVR_NM) -C "$$elf" | awk '$$3=="audio::g_player"{print $$1}'); \
			test -n "$$praw" || { echo "audio-compare-verify: audio::g_player missing in $$elf" >&2; exit 1; }; \
			paddr=$$((0x$$praw)); \
			if [ $$paddr -ge 8388608 ]; then paddr=$$((paddr - 8388608)); fi; \
			player_arg="--player-addr $$(printf '0x%x' $$paddr)"; \
		fi; \
		echo "=== audio-compare verify $$kind diag=$$diag $$player_arg ==="; \
		log="$(AUDIO_COMPARE_EVIDENCE)/$$kind-harness.log"; \
		if "$(AUDIO_COMPARE_DIR)/native/harness" --hex "$$out/$$kind.ino.hex" \
			--fx "$(FXDATA_BIN)" --diag-addr "$$diag" --label "$$kind" \
			$$player_arg --out-dir "$(AUDIO_COMPARE_EVIDENCE)" >"$$log" 2>&1; then \
			cat "$$log"; \
		else \
			cat "$$log"; exit 1; \
		fi; \
	done

# Preview export only: reads generated game data without changing FX artifacts.
WORLD_RENDER_DIR ?= build/world-render
render-world:
	mkdir -p $(WORLD_RENDER_DIR)
	$(CXX) -std=c++17 -O2 -Wall -Wextra $(shell $(FEATURES_PY) --profile full --flags) -Isrc tools/render_world.cpp -o $(WORLD_RENDER_DIR)/render-world
	$(WORLD_RENDER_DIR)/render-world fxdata/tables/exploration-terrain.bin $(WORLD_RENDER_DIR)/world.pbm
	python3 tools/render_world.py --input $(WORLD_RENDER_DIR)/world.pbm --output-dir $(WORLD_RENDER_DIR)

TEST_OUT      := $(BUILD_DIR)/tests/host
TEST_SOURCES  := tst/main.cpp
CXXFLAGS      ?= -std=c++17 -O0 -g3 -Wall -Wextra
TEST_STREAMFX_INCLUDE := -I"$(ARDUBOYSTREAMFX_ROOT)/src"

test: features-check
	@mkdir -p $(dir $(TEST_OUT))
	$(CXX) $(CXXFLAGS) $(FEATURE_FLAGS) $(DEV_FLAGS) -Isrc -Itools/audio_reference $(TEST_STREAMFX_INCLUDE) -DTEST $(TEST_SOURCES) -o $(TEST_OUT)
	./$(TEST_OUT)

test-tools:
	python3 -m unittest discover -s tools/tests -p 'test_*.py' -v

# ----------------------------------------------------------------------------
# Device tests (Ardens serial capture)
# ----------------------------------------------------------------------------
FXTEST_INOS   := $(wildcard tst/fxdatatest/test_*.ino)
FXTEST_NAMES  := $(basename $(notdir $(FXTEST_INOS)))
# test_music is the integrated sibling-library gate. test_audio remains the
# standalone local-player admission suite when music is off; linking both
# players would duplicate the Timer3 vector.
ifneq ($(findstring -DSH_FEATURE_MUSIC=1,$(FEATURE_FLAGS)),)
FXTEST_NAMES  := $(filter-out test_audio test_memory,$(FXTEST_NAMES))
else
FXTEST_NAMES  := $(filter-out test_music test_memory_music,$(FXTEST_NAMES))
endif
# The production-chain probe includes the real sketch and needs both save and
# music; other profiles use the explicit fxtest-save-chain target.
ifneq ($(findstring -DSH_FEATURE_MUSIC=1,$(FEATURE_FLAGS)),)
ifneq ($(findstring -DSH_FEATURE_SAVE_VOYAGE=1,$(FEATURE_FLAGS)),)
else
FXTEST_NAMES  := $(filter-out test_save_chain,$(FXTEST_NAMES))
endif
else
FXTEST_NAMES  := $(filter-out test_save_chain,$(FXTEST_NAMES))
endif
FXTEST_RUN    := $(if $(strip $(FXTEST_ONLY)),$(filter $(FXTEST_ONLY),$(FXTEST_NAMES)),$(FXTEST_NAMES))

fxtest: features-check streamfx-check fxtest-build fxtest-run

# Emulator-only: points FX::programSavePage at the 4 KiB data tail to prove
# seekSave/erase/page writes share the runtime page. It erases that alternate
# page; never run it against a physical cart.
fxtest-save-runtime-page:
	@printf '%s\n' "WARNING: emulator-only runtime save-page test; erases alternate cart page. Do not run on hardware."
	@$(MAKE) --no-print-directory fxtest FXTEST_ONLY=test_save PROFILE=full \
		FEATURE_FLAGS="$$(python3 tools/features.py --profile full --set CAMPAIGN=1 --set SAVE_VOYAGE=1 --flags)" \
		BUILD_DIR=build/fxtest-save-runtime-page \
		RELAX_FLAGS="-mrelax -mcall-prologues -DSH_SAVE_RUNTIME_PAGE_TEST=1" \
		FXTEST_MS=12000

# Exact production-chain device probe (updateStation -> requestSave ->
# saveVoyageNow) with the real sketch and streaming music. Emulator-only real
# cart writes. Keeps USB so Ardens can capture the serial marker.
fxtest-save-chain:
	@$(MAKE) --no-print-directory fxtest FXTEST_ONLY=test_save_chain PROFILE=full \
		FEATURE_FLAGS="$$(python3 tools/features.py --profile full --set CAMPAIGN=1 --set SAVE_VOYAGE=1 --set MUSIC=1 --flags)" \
		RELEASE_NO_USB=0 BUILD_DIR=build/fxtest-save-chain FXTEST_MS=15000

fxtest-build: features-check $(addprefix fxtest-build-,$(FXTEST_RUN))

fxtest-build-%:
	@stage="$(FXTEST_BUILD_DIR)/$*"; \
	rm -rf "$$stage"; \
	mkdir -p "$$stage"; \
	cp -R src "$$stage/src"; \
	cp "tst/fxdatatest/$*.ino" "$$stage/"; \
	cp tst/fxdatatest/*.hpp "$$stage/" 2>/dev/null || true; \
	if [ "$*" = "test_save_chain" ]; then cp "$(SKETCH).ino" "$$stage/production_sketch.hpp"; fi; \
	echo "build: $*"; \
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --optimize-for-debug \
		$(STREAMFX_LIB) --build-path "$$stage/arduino" --output-dir "$$stage/output" \
		$(FXTEST_SIZE_FLAGS) "$$stage/$*.ino"

fxtest-run:
	@if [ -z "$(ARDENS)" ] || [ ! -x "$(ARDENS)" ]; then \
		echo "fxtest-run: SKIPPED (Ardens unavailable; set ARDENS=/path/to/Ardens)"; \
	else \
		failed=0; \
		for name in $(FXTEST_RUN); do \
			stage="$(FXTEST_BUILD_DIR)/$$name"; \
			cp -f "$(FXDATA_BIN)" "$$stage/fxdata.bin"; \
			echo "=== $$name ==="; \
			if out="$$("$(ARDENS)" captureserial=$(FXTEST_MS) fxport=d1 display=ssd1306 file="$$stage/output/$$name.ino.hex" file="$$stage/fxdata.bin" 2>&1)"; then \
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
		'  make build      compile PROFILE firmware into dist/PROFILE/' \
		'  make mini       compile the Arduboy Mini firmware for PROFILE' \
		'  make size       whole-image flash/RAM report for PROFILE' \
		'  make size-matrix build and report full, lean, core, and min' \
		'  make feature-report  list active/inactive flags for PROFILE' \
		'  make run        build, then launch Ardens with the ELF + FX image' \
		'  make dev        launch developer noclip, full chart, A speed boost' \
		'  make render-world export the native world and annotated PNG overview' \
		'  make music-viewer audition the underwater tracks in the ATMlib2FX jukebox' \
		'  make test       host C++ tests (g++, no device required)' \
		'  make test-tools Python tooling unittest suite' \
		'  make fxtest     compile and run device suites through Ardens' \
		'  make fxtest-save-chain  exact production save-chain stack/audio probe' \
		'  make fxtest-save-runtime-page  emulator-only runtime page probe (erases alternate page)' \
		'  make clean      remove build/ and dist/' \
		'' \
		'Profiles: $(PROFILES) (default: $(PROFILE)). Selected with PROFILE=NAME.' \
		'  make build PROFILE=lean' \
		'  make feature-report PROFILE=core' \
		'' \
		'Overrides: DEV_MODE=0|1, RELEASE_NO_USB=0|1, PROFILE, SKETCH, FQBN, MINI_FQBN, ARDUINO_CLI, ARDENS, FXTEST_ONLY, FXTEST_MS, CXX.'

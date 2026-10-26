.PHONY: help full gen gen-check features-check feature-report build mini size size-line size-matrix run dev debug test test-tools fxtest fxtest-build fxtest-run clean format music-viewer music-viewer-build render-world

# ----------------------------------------------------------------------------
# Project
# ----------------------------------------------------------------------------
SKETCH        ?= $(basename $(notdir $(firstword $(wildcard *.ino))))
CXX           ?= g++
ARDUINO_CLI   ?= arduino-cli
ATMLIB2FX_ROOT ?= ../ATMlib2FX
export ATMLIB2FX_ROOT
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
FEATURES_PY   ?= python3 tools/features.py
PROFILES      := $(shell $(FEATURES_PY) --list 2>/dev/null)
FEATURE_FLAGS := $(shell $(FEATURES_PY) --profile "$(PROFILE)" --flags 2>/dev/null)
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

FXDATA_BIN    ?= fxdata/fxdata.bin
FXTEST_MS     ?= 3000
FXTEST_ONLY   ?=
FXTEST_BUILD_DIR ?= $(BUILD_DIR)/fxtest

# -mrelax and shared prologues are the measured shipping flags used by the
# source projects. LTO means only whole-image measurements are meaningful;
# use `make size` after every flash-sensitive change.
RELAX_FLAGS   ?= -mrelax -mcall-prologues
CPP_EXTRA_FLAGS := $(RELAX_FLAGS) $(FEATURE_FLAGS) $(DEV_FLAGS)
C_EXTRA_FLAGS   := -mrelax $(FEATURE_FLAGS) $(DEV_FLAGS)
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
MUSIC_OUTPUTS := $(addprefix fxdata/music/sh_,harbor_bubbles.bin kelp_drift.bin sonar_lullaby.bin abyssal_glass.bin tidepool_waltz.bin bubblejet_run.bin) fxdata/music/manifest.json
FX_INPUTS     := $(IMAGES) fxdata/fxdata.txt tools/gen.sh tools/gen_sprites.py \
                 tools/gen_font.py tools/gen_pilot.py tools/gen_world.py \
                 tools/gen_cargo.py tools/gen_level.py tools/gen_scenery.py tools/gen_contours.py tools/gen_settlements.py levels/exploration.json $(WORLD_TABLES) $(CARGO_TABLES) \
                 tools/fxdata-build.py tools/gen_music.py tools/gen_music_viewer.py $(MUSIC_SOURCES)

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
	@mkdir -p $(BUILD_DIR)/gen-check/music
	@cp $(MUSIC_OUTPUTS) src/generated/music.hpp $(BUILD_DIR)/gen-check/music/
	@cp fxdata/fxdata.h $(BUILD_DIR)/gen-check/fxdata.h
	@cp fxdata/fxdata.bin $(BUILD_DIR)/gen-check/fxdata.bin
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
	@$(MAKE) --no-print-directory gen
	@cmp -s fxdata/fxdata.h src/fxdata.h || { echo "gen-check: src/fxdata.h is stale; run make gen" >&2; exit 1; }
	@cmp -s fxdata/fxdata.h $(BUILD_DIR)/gen-check/fxdata.h || { echo "gen-check: fxdata header changed" >&2; exit 1; }
	@cmp -s fxdata/fxdata.bin $(BUILD_DIR)/gen-check/fxdata.bin || { echo "gen-check: packed image changed" >&2; exit 1; }
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
	@for file in $(MUSIC_OUTPUTS) src/generated/music.hpp; do \
		cmp -s "$$file" "$(BUILD_DIR)/gen-check/music/$${file##*/}" || { echo "gen-check: $$file changed" >&2; exit 1; }; \
	done
	@for file in $(LEVEL_OUTPUTS); do \
		cmp -s "$$file" "$(BUILD_DIR)/gen-check/level/$${file##*/}" || { echo "gen-check: $$file changed" >&2; exit 1; }; \
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
build: features-check src/fxdata.h
	mkdir -p $(DIST_DIR)
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --optimize-for-debug \
		--build-path $(BUILD_DIR)/arduino --output-dir $(DIST_DIR) \
		$(SIZE_FLAGS) "$(SKETCH).ino"

mini: features-check src/fxdata.h
	mkdir -p $(DIST_DIR)
	$(ARDUINO_CLI) compile --fqbn "$(MINI_FQBN)" --optimize-for-debug \
		--build-path $(BUILD_DIR)/arduino-mini --output-dir $(DIST_DIR) \
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

test: features-check
	@mkdir -p $(dir $(TEST_OUT))
	$(CXX) $(CXXFLAGS) $(FEATURE_FLAGS) $(DEV_FLAGS) -Isrc -DTEST $(TEST_SOURCES) -o $(TEST_OUT)
	./$(TEST_OUT)

test-tools:
	python3 -m unittest discover -s tools/tests -p 'test_*.py' -v

# ----------------------------------------------------------------------------
# Device tests (Ardens serial capture)
# ----------------------------------------------------------------------------
FXTEST_INOS   := $(wildcard tst/fxdatatest/test_*.ino)
FXTEST_NAMES  := $(basename $(notdir $(FXTEST_INOS)))
FXTEST_RUN    := $(if $(strip $(FXTEST_ONLY)),$(filter $(FXTEST_ONLY),$(FXTEST_NAMES)),$(FXTEST_NAMES))

fxtest: features-check fxtest-build fxtest-run

fxtest-build: features-check $(addprefix fxtest-build-,$(FXTEST_RUN))

fxtest-build-%:
	@stage="$(FXTEST_BUILD_DIR)/$*"; \
	rm -rf "$$stage"; \
	mkdir -p "$$stage"; \
	cp -R src "$$stage/src"; \
	cp "tst/fxdatatest/$*.ino" "$$stage/"; \
	cp tst/fxdatatest/*.hpp "$$stage/" 2>/dev/null || true; \
	echo "build: $*"; \
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --optimize-for-debug \
		--build-path "$$stage/arduino" --output-dir "$$stage/output" \
		$(SIZE_FLAGS) "$$stage/$*.ino"

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
		'  make clean      remove build/ and dist/' \
		'' \
		'Profiles: $(PROFILES) (default: $(PROFILE)). Selected with PROFILE=NAME.' \
		'  make build PROFILE=lean' \
		'  make feature-report PROFILE=core' \
		'' \
		'Overrides: DEV_MODE=0|1, PROFILE, SKETCH, FQBN, MINI_FQBN, ARDUINO_CLI, ARDENS, FXTEST_ONLY, FXTEST_MS, CXX.'

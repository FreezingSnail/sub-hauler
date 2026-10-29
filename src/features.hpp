#pragma once

// Compile-time feature namespace. This header is the single source of feature
// names, defaults, and dependency rules; every flag is numeric 0/1 and is
// meant for #if at code, data, string, call, and render boundaries.
//
// No Arduino headers: this file is included by the pure host-tested rules.

#ifndef SH_FEATURE_STATIONS
#define SH_FEATURE_STATIONS 1
#endif

#ifndef SH_FEATURE_JOBS
#define SH_FEATURE_JOBS 1
#endif

#ifndef SH_FEATURE_FOG_CHART
#define SH_FEATURE_FOG_CHART 1
#endif

#ifndef SH_FEATURE_MINES
#define SH_FEATURE_MINES 1
#endif

#ifndef SH_FEATURE_CURRENTS
#define SH_FEATURE_CURRENTS 1
#endif

#ifndef SH_FEATURE_SHOP
#define SH_FEATURE_SHOP 1
#endif

#ifndef SH_FEATURE_HULL_DAMAGE
#define SH_FEATURE_HULL_DAMAGE 1
#endif

#ifndef SH_FEATURE_PARTICLES
#define SH_FEATURE_PARTICLES 1
#endif

#ifndef SH_FEATURE_MULTI_HULL
#define SH_FEATURE_MULTI_HULL 1
#endif

#ifndef SH_FEATURE_SAVE_CHART
#define SH_FEATURE_SAVE_CHART 0
#endif

// Dependency checks. These direct checks also enforce the aggregate rules:
// STATIONS=0 implies JOBS and SHOP are 0; FOG_CHART=0 implies SAVE_CHART is 0.
#if SH_FEATURE_JOBS && !SH_FEATURE_STATIONS
#error "SH_FEATURE_JOBS requires SH_FEATURE_STATIONS"
#endif

#if SH_FEATURE_SAVE_CHART && !SH_FEATURE_FOG_CHART
#error "SH_FEATURE_SAVE_CHART requires SH_FEATURE_FOG_CHART"
#endif

#if SH_FEATURE_SHOP && !SH_FEATURE_STATIONS
#error "SH_FEATURE_SHOP requires SH_FEATURE_STATIONS"
#endif

// Contract acceptance owns the coarse four-unit cargo load directly. The
// packing minigame and its runtime were removed in sub-hauler-gl1.1 because
// its value did not justify the flash/SRAM cost; the shipped cargoBank FX
// declaration remains only for flash ABI compatibility.

// Authored map tools/gates/rewards. The minimum pilot profile leaves gates open.
#ifndef SH_FEATURE_EXPLORATION
#define SH_FEATURE_EXPLORATION 1
#endif
#if SH_FEATURE_EXPLORATION && !SH_FEATURE_JOBS
#error "SH_FEATURE_EXPLORATION requires SH_FEATURE_JOBS"
#endif

// Pure campaign/untimed-freight rules. On by default for the full profile and
// off in lean/core/min; the profile generator passes an explicit 0/1 for every
// shipped build. Requires the station, job and authored-map rules.
#ifndef SH_FEATURE_CAMPAIGN
#define SH_FEATURE_CAMPAIGN 1
#endif

// Atomic voyage save. It owns the same state as campaign and replaces the
// standalone chart writer, so the two save features are mutually exclusive.
// On by default for the full profile only.
#ifndef SH_FEATURE_SAVE_VOYAGE
#define SH_FEATURE_SAVE_VOYAGE 1
#endif

// Streaming music requires the voyage pause menu, and therefore atomic save.
// On by default for the full profile only; lean/core/min pass an explicit 0.
#ifndef SH_FEATURE_MUSIC
#define SH_FEATURE_MUSIC 1
#endif

// Music backend selection. Production uses the ATMlib2FX compact synth; set to
// 1 only for the explicit ArduboyStreamFX comparison build.
#ifndef SH_MUSIC_STREAMING
#define SH_MUSIC_STREAMING 0
#endif

// Campaign requires the full station/contract/authored-map stack. The save
// feature then requires campaign; it also replaces the standalone chart save.
#if SH_FEATURE_CAMPAIGN && !SH_FEATURE_JOBS
#error "SH_FEATURE_CAMPAIGN requires SH_FEATURE_JOBS"
#endif
#if SH_FEATURE_CAMPAIGN && !SH_FEATURE_STATIONS
#error "SH_FEATURE_CAMPAIGN requires SH_FEATURE_STATIONS"
#endif
#if SH_FEATURE_CAMPAIGN && !SH_FEATURE_EXPLORATION
#error "SH_FEATURE_CAMPAIGN requires SH_FEATURE_EXPLORATION"
#endif
#if SH_FEATURE_SAVE_VOYAGE && !SH_FEATURE_CAMPAIGN
#error "SH_FEATURE_SAVE_VOYAGE requires SH_FEATURE_CAMPAIGN"
#endif
#if SH_FEATURE_SAVE_VOYAGE && SH_FEATURE_SAVE_CHART
#error "SH_FEATURE_SAVE_VOYAGE requires !SH_FEATURE_SAVE_CHART"
#endif
#if SH_FEATURE_MUSIC && !SH_FEATURE_SAVE_VOYAGE && !SH_FLIGHT_PROBE
#error "SH_FEATURE_MUSIC requires SH_FEATURE_SAVE_VOYAGE for the pause MUSIC control"
#endif

// Development override, separate from shipped feature profiles.
#ifndef SH_DEV_MODE
#define SH_DEV_MODE 0
#endif
#if SH_DEV_MODE != 0 && SH_DEV_MODE != 1
#error "SH_DEV_MODE must be 0 or 1"
#endif

// Release runtime switch. The sketch places ARDUBOY_NO_USB immediately before
// setup() when this is 1; the bootloader and its supported startup/button
// entry remain available. Set RELEASE_NO_USB=0 for a runtime-USB development
// image. The Makefile defaults DEV_MODE=1 to RELEASE_NO_USB=0 so serial
// development stays usable.
#ifndef SH_RELEASE_NO_USB
#define SH_RELEASE_NO_USB 1
#endif
#if SH_RELEASE_NO_USB != 0 && SH_RELEASE_NO_USB != 1
#error "SH_RELEASE_NO_USB must be 0 or 1"
#endif

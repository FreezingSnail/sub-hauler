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

#ifndef SH_FEATURE_SONAR
#define SH_FEATURE_SONAR 1
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
// STATIONS=0 implies JOBS and SHOP are 0; FOG_CHART=0 implies SONAR and
// SAVE_CHART are 0.
#if SH_FEATURE_JOBS && !SH_FEATURE_STATIONS
#error "SH_FEATURE_JOBS requires SH_FEATURE_STATIONS"
#endif

#if SH_FEATURE_SONAR && !SH_FEATURE_FOG_CHART
#error "SH_FEATURE_SONAR requires SH_FEATURE_FOG_CHART"
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

// Development override, separate from shipped feature profiles.
#ifndef SH_DEV_MODE
#define SH_DEV_MODE 0
#endif
#if SH_DEV_MODE != 0 && SH_DEV_MODE != 1
#error "SH_DEV_MODE must be 0 or 1"
#endif

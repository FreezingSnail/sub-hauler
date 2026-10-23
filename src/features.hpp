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

#ifndef SH_FEATURE_CARGO_LOAD
#define SH_FEATURE_CARGO_LOAD 1
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
// STATIONS=0 implies JOBS, SHOP, and CARGO_LOAD are 0; FOG_CHART=0 implies
// SONAR and SAVE_CHART are 0.
#if SH_FEATURE_JOBS && !SH_FEATURE_STATIONS
#error "SH_FEATURE_JOBS requires SH_FEATURE_STATIONS"
#endif

#if SH_FEATURE_CARGO_LOAD && !SH_FEATURE_JOBS
#error "SH_FEATURE_CARGO_LOAD requires SH_FEATURE_JOBS"
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

// CARGO_LOAD=0 fallback: accepting a job still sets the four cargo units and
// undocks; the loading UI and arrangement loader are not compiled.

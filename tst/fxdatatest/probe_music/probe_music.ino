// Bounded AVR resource probe: whole-image marginal cost of the ATMlib2FX
// compact music decoder (sub-hauler-7px.1).
//
// This is a build-only size probe, not a runnable device suite. It lives in a
// subdirectory so `make fxtest` (which globs tst/fxdatatest/test_*.ino) never
// picks it up and no Ardens marker is implied.
//
// Build the same sketch twice with -DPROBE_MUSIC=0 and -DPROBE_MUSIC=1, then
// subtract the two `avr-size` results. Both variants link the same Arduboy2 /
// ArduboyFX baseline and the same platform byte source; the only difference is
// the music setup/play call chain. Use the shipping optimization flags and the
// ring/loop settings the accepted design requires:
//
//   arduino-cli compile --fqbn arduboy-homemade:avr:arduboy-fx \
//     --optimize-for-debug --library ../ATMlib2FX \
//     --build-path build/probe-music/off --output-dir build/probe-music/off-out \
//     --build-property "compiler.cpp.extra_flags=-mrelax -mcall-prologues -DPROBE_MUSIC=0 -DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=64" \
//     --build-property "compiler.c.extra_flags=-mrelax -DPROBE_MUSIC=0 -DATM_FX_COMPACT_LOOP=1 -DATM_FX_COMPACT_RING=64" \
//     --build-property "compiler.c.elf.extra_flags=-mrelax" \
//     tst/fxdatatest/probe_music/probe_music.ino
//
// Repeat with PROBE_MUSIC=1 and a different build/output path. Report the
// flash and static deltas, not the absolute probe totals.

#include <Arduboy2.h>
#include <ArduboyFX.h>

#if PROBE_MUSIC
extern "C" {
#include <atm_synth.h>
#include <atm_fx_compact.h>
}
#endif

Arduboy2 arduboy;

// The platform byte source both variants keep linked. It is never executed:
// this sketch exists only to measure linked code and static data.
extern "C" void atm_fx_compact_fetch(uint32_t offset, uint8_t *dst, uint8_t n) {
    FX::readDataBytes((uint24_t)offset, dst, n);
}

static volatile uint8_t probeByte;

void setup() {
    arduboy.begin();
    atm_fx_compact_fetch(0, (uint8_t *)&probeByte, 1);

#if PROBE_MUSIC
    atm_synth_setup();
    atm_fx_compact_play(0, 0);
#endif
}

void loop() {
}

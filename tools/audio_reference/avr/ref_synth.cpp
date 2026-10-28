// Bare-metal reference player for the pinned ATMlib2FX synth.
//
// This image exists only under tools/gen_audio_ref.py. It embeds one generated
// compact stream in PROGMEM and runs the real oscillator/timer ISR and compact
// decoder unchanged. The native capture tool executes it through absim.
//
// It deliberately avoids Arduino/Arduboy libraries so the only audio code is
// ATMlib2FX itself. The generated stream_data.h supplies STREAM_DATA and
// STREAM_LENGTH.

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>

extern "C" {
#include <atm_fx_compact.h>
#include <atm_synth.h>
}

#include "stream_data.h"

// Read by tools/audio_reference/capture.cpp from absim SRAM. Keep this layout
// stable: the generator parses these 20 bytes by symbol address.
struct RefDiag {
    uint32_t magic;
    uint16_t ticks;
    uint16_t underruns;
    uint16_t max_gap;
    uint16_t stop_tick;
    uint8_t error;
    uint8_t active;
    uint8_t paused;
    uint8_t loopable;
    uint8_t loop_streaming;
    uint8_t loop_pinned;
    uint8_t reserved[2];
};

extern "C" {
volatile RefDiag ref_diag;
}

constexpr uint32_t kDiagMagic = 0x52454631u; // "REF1"

extern "C" void atm_fx_compact_fetch(uint32_t offset, uint8_t* dst, uint8_t n) {
    for (uint8_t i = 0; i < n; ++i)
        dst[i] = pgm_read_byte(&STREAM_DATA[offset + i]);
}

static void snapshot_diag() {
    ref_diag.ticks = atm_fx_compact_state.ticks;
    ref_diag.underruns = atm_fx_compact_state.underruns;
    ref_diag.max_gap = atm_fx_compact_state.max_service_gap;
    ref_diag.stop_tick = atm_fx_compact_state.stop_tick;
    ref_diag.error = atm_fx_compact_state.error;
    ref_diag.active = atm_fx_compact_is_active();
    ref_diag.paused = atm_fx_compact_is_paused();
    ref_diag.loopable = atm_fx_compact_state.loopable;
    ref_diag.loop_streaming = atm_fx_compact_state.loop_streaming;
    ref_diag.loop_pinned = atm_fx_compact_state.loop_pinned;
}

int main() {
    cli();
    ref_diag.magic = kDiagMagic;

    // Arduboy2 marks the two speaker pins as outputs; absim only emits PWM
    // samples when this speaker mask is non-zero. No other pin is touched.
    DDRC |= (1 << 6) | (1 << 7);

    atm_synth_setup();
    atm_fx_compact_play(0, STREAM_LENGTH);
    snapshot_diag();
    sei();

    for (;;) {
        atm_fx_compact_service();
        snapshot_diag();
    }
}

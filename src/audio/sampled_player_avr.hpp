#pragma once

// Device boundary for the bounded IMA ADPCM player.
//
// Clocks:
//   Timer4: 96 MHz PLL / 1 / (TOP 511 + 1) = 187,500 Hz PWM carrier.
//           duty = 256 + (signed16 >> 7), a documented 9-bit equivalent of the
//           accepted 8-bit pwm8 = 128 + (ref >> 8) mapping. No multiply.
//   Timer3: 16 MHz / 8 / (OCR3A 249 + 1) = 8,000 Hz CTC sample interrupt.
//
// The Timer3 ISR consumes only SRAM and writes OCR4A. All FX/SPI/OLED work is
// in service(), which the foreground calls before the frame gate and after
// presentation. The Timer4 carrier runs while stopped/paused/muted at 50%
// duty; it is above the audible band, unlike an 8 kHz PWM carrier.

#include <Arduino.h>
#include <ArduboyFX.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

#include "../generated/audio.hpp"
#include "sampled_player.hpp"

#ifndef SH_AUDIO_DIAG
#define SH_AUDIO_DIAG 0
#endif

namespace audio {

static SampledPlayer g_player;

constexpr uint16_t kPwmTop = 511;
constexpr uint16_t kPwmCenter = 256;
constexpr uint8_t kTimer3Top = 249;

inline uint16_t toDuty(int16_t sample) {
    return static_cast<uint16_t>(kPwmCenter + (sample >> 7));
}

inline uint24_t readCueBase(uint8_t index) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&kCues[index].base);
    return static_cast<uint24_t>(pgm_read_byte(p)) |
           (static_cast<uint24_t>(pgm_read_byte(p + 1)) << 8) |
           (static_cast<uint24_t>(pgm_read_byte(p + 2)) << 16);
}

inline uint24_t readCueSamples(uint8_t index) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&kCues[index].samples);
    return static_cast<uint24_t>(pgm_read_byte(p)) |
           (static_cast<uint24_t>(pgm_read_byte(p + 1)) << 8) |
           (static_cast<uint24_t>(pgm_read_byte(p + 2)) << 16);
}

#if SH_AUDIO_DIAG
volatile uint16_t diagTicks;
volatile uint16_t diagUnderruns;
volatile uint16_t diagErrors;
volatile uint16_t diagMaxGapTicks;
volatile uint16_t diagMaxServiceUs;
volatile uint16_t diagMaxIsrTicks;
volatile uint16_t diagLastServiceTick;
#endif

inline void resetOutput() {
    TC4H = static_cast<uint8_t>(kPwmCenter >> 8);
    OCR4A = static_cast<uint8_t>(kPwmCenter & 0xFF);
}

inline void begin() {
    DDRC |= (1 << 6) | (1 << 7);
    PLLFRQ = 0x5A;
    PLLCSR = _BV(PLLE);
    while (!(PLLCSR & _BV(PLOCK))) {
    }
    TCCR4A = _BV(COM4A0) | _BV(PWM4A);
    TCCR4B = 0x01;
    TCCR4C = 0;
    TCCR4D = 0;
    TCCR4E = 0;
    TC4H = static_cast<uint8_t>(kPwmTop >> 8);
    OCR4C = static_cast<uint8_t>(kPwmTop & 0xFF);
    resetOutput();
    TCCR3A = 0;
    TCCR3B = _BV(WGM32) | _BV(CS31);
    OCR3A = kTimer3Top;
    TCNT3 = 0;
    TIFR3 = _BV(OCF3A);
    TIMSK3 = 0;
}

// Read only free ring bytes. At most 127 B per pass; never inside an OLED
// transaction.
inline void fillRing() {
    if (g_player.sourceBytesLeft() == 0)
        g_player.wrapSource();
    const uint8_t chunk = g_player.contiguousFree();
    if (chunk == 0)
        return;
    const uint24_t left = g_player.sourceBytesLeft();
    const uint8_t count = left < chunk ? static_cast<uint8_t>(left) : chunk;
    if (count == 0)
        return;
    FX::readDataBytes(g_player.nextAddress(), g_player.writePtr(), count);
    g_player.commit(count);
    g_player.advance(count);
}

inline bool start(uint8_t cue) {
    if (cue >= kCueCount) {
#if SH_AUDIO_DIAG
        ++diagErrors;
#endif
        return false;
    }
    const uint24_t base = readCueBase(cue);
    const uint24_t samples = readCueSamples(cue);
    const uint24_t bytes = samples >> 1;
    const int16_t predictor = kInitialPredictor;
    const uint8_t index = kInitialIndex;
    const uint8_t sreg = SREG;
    cli();
    if (!g_player.start(cue, base, bytes, samples, predictor, index)) {
        SREG = sreg;
        return false;
    }
    fillRing();
    g_player.enable();
    TIMSK3 |= _BV(OCIE3A);
    SREG = sreg;
#if SH_AUDIO_DIAG
    {
        AtomicSection lock;
        diagLastServiceTick = diagTicks;
    }
#endif
    return true;
}

inline void stop() {
    g_player.stop();
    TIMSK3 &= static_cast<uint8_t>(~_BV(OCIE3A));
    resetOutput();
#if SH_AUDIO_DIAG
    {
        AtomicSection lock;
        diagLastServiceTick = diagTicks;
    }
#endif
}

inline void pause() {
    g_player.pause();
#if SH_AUDIO_DIAG
    {
        AtomicSection lock;
        diagLastServiceTick = diagTicks;
    }
#endif
}

inline void resume() {
    g_player.resume();
#if SH_AUDIO_DIAG
    {
        AtomicSection lock;
        diagLastServiceTick = diagTicks;
    }
#endif
}

inline void setMuted(bool muted) {
    g_player.setMuted(muted);
    if (muted)
        resetOutput();
}

inline void service() {
#if SH_AUDIO_DIAG
    bool measure = false;
    {
        AtomicSection lock;
        if (g_player.active() && !g_player.paused()) {
            const uint16_t now = diagTicks;
            const uint16_t gap = static_cast<uint16_t>(now - diagLastServiceTick);
            diagLastServiceTick = now;
            if (gap > diagMaxGapTicks)
                diagMaxGapTicks = gap;
            measure = true;
        } else {
            diagLastServiceTick = diagTicks;
        }
    }
    uint32_t started = 0;
    if (measure)
        started = micros();
#endif
    if (!g_player.active() || g_player.paused())
        return;
    fillRing();
#if SH_AUDIO_DIAG
    if (measure) {
        const uint16_t elapsed = static_cast<uint16_t>(micros() - started);
        if (elapsed > diagMaxServiceUs)
            diagMaxServiceUs = elapsed;
    }
#endif
}

} // namespace audio

ISR(TIMER3_COMPA_vect) {
#if SH_AUDIO_DIAG
    const uint16_t entry = TCNT3;
#endif
    const uint16_t duty = audio::toDuty(audio::g_player.tick());
    TC4H = static_cast<uint8_t>(duty >> 8);
    OCR4A = static_cast<uint8_t>(duty & 0xFF);
#if SH_AUDIO_DIAG
    if (audio::g_player.underrun()) {
        audio::g_player.clearUnderrun();
        ++audio::diagUnderruns;
    }
    ++audio::diagTicks;
    const uint16_t exit = TCNT3;
    const uint16_t delta = exit >= entry ? static_cast<uint16_t>(exit - entry)
                                         : static_cast<uint16_t>((audio::kTimer3Top + 1 - entry) + exit);
    if (delta > audio::diagMaxIsrTicks)
        audio::diagMaxIsrTicks = delta;
#endif
}

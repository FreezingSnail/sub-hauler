#pragma once

// Bounded sampled-audio player core. The 128-byte compressed ring and every
// cursor/decoder byte live in this object; there is no heap and no score or
// oscillator engine. Device glue supplies the 8 kHz Timer3 ISR and the
// high-frequency Timer4 PWM carrier; host tests drive the same core directly.
//
// Concurrency: the ISR is the only nibble consumer and the foreground is the
// only byte producer. readPos_/writePos_/flags_ are volatile single-byte
// stores. The producer publishes writePos only after the ring data is visible;
// one ring slot is kept vacant so empty and full are unambiguous without a
// shared full flag. Foreground flag/position updates are interrupt-masked.

#include <stdint.h>

#include "ima_adpcm.hpp"
#include "../lib/uint24.h"

#if defined(__AVR__)
#include <avr/interrupt.h>
#endif

namespace audio {

constexpr uint8_t kRingBytes = 128;
constexpr uint8_t kRingUsable = 127; // one sentinel slot keeps states disjoint
constexpr uint8_t kFlagActive = 0x01;
constexpr uint8_t kFlagMuted = 0x02;
constexpr uint8_t kFlagPaused = 0x04;
constexpr uint8_t kFlagError = 0x08;
constexpr uint8_t kFlagUnderrun = 0x80;
constexpr uint8_t kCueShift = 4;
constexpr uint8_t kCueMask = 0x07;

#if defined(__AVR__)
#define SH_AUDIO_BARRIER() __asm__ __volatile__("" ::: "memory")
#else
#define SH_AUDIO_BARRIER() ((void)0)
#endif

class AtomicSection {
#if defined(__AVR__)
  public:
    AtomicSection() : sreg_(SREG) { cli(); }
    ~AtomicSection() { SREG = sreg_; }

  private:
    uint8_t sreg_;
#endif
};

class SampledPlayer {
  public:
    constexpr SampledPlayer()
        : ring_(), predictor_(0), loopPredictor_(0), stepIndex_(0),
          loopIndex_(0), readPos_(0), writePos_(0), samplesLeft_(0),
          loopSamples_(0), fxCursor_(0), fxBytesLeft_(0), fxBase_(0), flags_(0) {}

    // Resets stream state and selects a cue, but leaves output disabled until
    // enable() is called. The device wrapper pre-fills the ring in between.
    bool start(uint8_t cue, uint24_t base, uint24_t bytes, uint24_t samples,
               int16_t predictor, uint8_t index) {
        if (bytes == 0 || samples == 0 || (bytes < (samples >> 1)))
            return false;
        if ((flags_ & kFlagActive) && ((flags_ >> kCueShift) & kCueMask) == cue)
            return true;
        const uint8_t muted = flags_ & kFlagMuted;
        flags_ = static_cast<uint8_t>(muted | ((cue & kCueMask) << kCueShift));
        predictor_ = predictor;
        loopPredictor_ = predictor;
        stepIndex_ = index > ima::kMaxIndex ? ima::kMaxIndex : index;
        loopIndex_ = stepIndex_;
        samplesLeft_ = samples;
        loopSamples_ = samples;
        fxBase_ = base;
        fxCursor_ = base;
        fxBytesLeft_ = bytes;
        readPos_ = 0;
        writePos_ = 0;
        return true;
    }

    void enable() { flags_ |= kFlagActive; }

    void stop() {
        AtomicSection lock;
        flags_ &= kFlagMuted;
        readPos_ = 0;
        writePos_ = 0;
        samplesLeft_ = 0;
    }

    void pause() {
        AtomicSection lock;
        if (flags_ & kFlagActive)
            flags_ |= kFlagPaused;
    }

    void resume() {
        AtomicSection lock;
        flags_ &= static_cast<uint8_t>(~kFlagPaused);
    }

    void setMuted(bool muted) {
        AtomicSection lock;
        if (muted)
            flags_ |= kFlagMuted;
        else
            flags_ &= static_cast<uint8_t>(~kFlagMuted);
    }

    bool active() const { return (flags_ & kFlagActive) != 0; }
    bool paused() const { return (flags_ & kFlagPaused) != 0; }
    bool muted() const { return (flags_ & kFlagMuted) != 0; }
    bool error() const { return (flags_ & kFlagError) != 0; }
    bool underrun() const { return (flags_ & kFlagUnderrun) != 0; }
    void clearUnderrun() {
        AtomicSection lock;
        flags_ &= static_cast<uint8_t>(~kFlagUnderrun);
    }
    uint8_t cue() const { return (flags_ >> kCueShift) & kCueMask; }

    uint8_t freeBytes() const {
        const uint8_t readByte = static_cast<uint8_t>(readPos_ >> 1);
        const uint8_t write = writePos_;
        return static_cast<uint8_t>((readByte - write - 1) & 0x7F);
    }

    uint8_t contiguousFree() const {
        const uint8_t free = freeBytes();
        if (free == 0)
            return 0;
        const uint8_t contiguous = static_cast<uint8_t>(kRingBytes - writePos_);
        return free < contiguous ? free : contiguous;
    }

    uint8_t *writePtr() { return &ring_[writePos_]; }
    uint24_t nextAddress() const { return fxCursor_; }
    uint24_t sourceBytesLeft() const { return fxBytesLeft_; }

    void commit(uint8_t count) {
        const uint8_t write = static_cast<uint8_t>((writePos_ + count) & 0x7F);
        SH_AUDIO_BARRIER();
        writePos_ = write;
    }

    void advance(uint8_t count) {
        fxCursor_ += count;
        fxBytesLeft_ -= count;
    }

    void wrapSource() {
        if (fxBytesLeft_ == 0) {
            fxCursor_ = fxBase_;
            fxBytesLeft_ = loopSamples_ >> 1;
        }
    }

    bool pushByte(uint8_t value) {
        if (freeBytes() == 0)
            return false;
        ring_[writePos_] = value;
        commit(1);
        return true;
    }

    // Timer-consumer step. SRAM only: no FX, SPI, OLED or descriptor reads
    // beyond the fixed codec step/index table. Pause and mute return 0
    // without advancing; both retain position/predictor/ring. The ISR needs
    // only one branchless duty conversion.
    int16_t tick() {
        const uint8_t flags = flags_;
        if (!(flags & kFlagActive) || (flags & (kFlagPaused | kFlagMuted)))
            return 0;
        SH_AUDIO_BARRIER();
        const uint8_t write = writePos_;
        SH_AUDIO_BARRIER();
        if (static_cast<uint8_t>(readPos_ >> 1) == write && (readPos_ & 1) == 0) {
            flags_ |= kFlagError | kFlagUnderrun;
            return 0;
        }
        const uint8_t byte = ring_[readPos_ >> 1];
        const uint8_t nibble = (readPos_ & 1) ? static_cast<uint8_t>(byte >> 4)
                                              : static_cast<uint8_t>(byte & 0x0F);
        readPos_ = static_cast<uint8_t>(readPos_ + 1);
        ima::State state{predictor_, stepIndex_};
        const int16_t sample = ima::decode(nibble, state);
        predictor_ = state.predictor;
        stepIndex_ = state.index;
        if (--samplesLeft_ == 0) {
            predictor_ = loopPredictor_;
            stepIndex_ = loopIndex_;
            samplesLeft_ = loopSamples_;
        }
        return sample;
    }

  private:
    uint8_t ring_[kRingBytes];
    int16_t predictor_;
    int16_t loopPredictor_;
    uint8_t stepIndex_;
    uint8_t loopIndex_;
    volatile uint8_t readPos_;   // nibble index 0..255, bit 0 = high/low half
    volatile uint8_t writePos_;  // byte index 0..127
    uint24_t samplesLeft_;
    uint24_t loopSamples_;
    uint24_t fxCursor_;
    uint24_t fxBytesLeft_;
    uint24_t fxBase_;
    volatile uint8_t flags_;
};

#if defined(__AVR__)
static_assert(sizeof(SampledPlayer) <= 152,
              "player static state must stay within the 152 B allocation");
#endif

#undef SH_AUDIO_BARRIER

} // namespace audio

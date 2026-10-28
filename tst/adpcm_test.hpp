#pragma once

// Permanent host tests for the pure IMA ADPCM codec and the bounded player
// core. The player tests drive the same ring/decoder code that the AVR Timer4
// ISR uses; only the FX reader and PWM glue are device-specific.

#include <cstdint>
#include <vector>

#include "audio/ima_adpcm.hpp"
#include "audio/sampled_player.hpp"
#include "test.hpp"

namespace adpcm_test {

inline uint8_t encodeCode(int16_t sample, audio::ima::State &state) {
    const uint16_t step = audio::ima::kStepTable[state.index];
    int32_t difference = static_cast<int32_t>(sample) - state.predictor;
    const uint8_t sign = difference < 0 ? 8 : 0;
    if (sign)
        difference = -difference;
    int32_t code = (difference << 2) / step;
    if (code > 7)
        code = 7;
    uint8_t bestCode = static_cast<uint8_t>(code);
    int32_t bestError = 1 << 30;
    for (int32_t candidate = code - 1; candidate <= code + 1; ++candidate) {
        if (candidate < 0 || candidate > 7)
            continue;
        int32_t quantized = step >> 3;
        if (candidate & 1)
            quantized += step >> 2;
        if (candidate & 2)
            quantized += step >> 1;
        if (candidate & 4)
            quantized += step;
        const int32_t probe = sign ? state.predictor - quantized : state.predictor + quantized;
        const int32_t error = sample > probe ? sample - probe : probe - sample;
        if (error < bestError) {
            bestError = error;
            bestCode = static_cast<uint8_t>(candidate);
        }
    }
    const uint8_t nibble = static_cast<uint8_t>(bestCode | sign);
    audio::ima::State next{state.predictor, state.index};
    audio::ima::decode(nibble, next);
    state = next;
    return nibble;
}

inline std::vector<uint8_t> encode(const std::vector<int16_t> &samples,
                                   audio::ima::State &state) {
    std::vector<uint8_t> packed;
    uint8_t low = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
        const uint8_t nibble = encodeCode(samples[i], state);
        if (i & 1)
            packed.push_back(static_cast<uint8_t>(low | (nibble << 4)));
        else
            low = nibble;
    }
    if (samples.size() & 1)
        packed.push_back(low);
    return packed;
}

inline void suite(TestRunner &runner) {
    Test vectors("adpcm_known_vectors");
    {
        audio::ima::State state{0, 0};
        vectors.assertEqual(audio::ima::decode(0x0, state), 0, "step 7 code 0");
        vectors.assertEqual(state.index, 0, "code 0 keeps index at floor");
        vectors.assertEqual(audio::ima::decode(0x4, state), 7, "step 7 code 4 +7");
        vectors.assertEqual(state.index, 2, "code 4 raises index by 2");
        vectors.assertEqual(audio::ima::decode(0x7, state), 23, "step 9 code 7 accumulates");
        vectors.assertEqual(state.index, 10, "code 7 raises index by 8");
    }
    runner.add(vectors);

    Test negative("adpcm_negative_vectors");
    {
        audio::ima::State state{0, 0};
        vectors.assertEqual(audio::ima::decode(0x8, state), 0, "step 7 negative code 0 is zero");
        vectors.assertEqual(audio::ima::decode(0xC, state), -7, "step 7 negative code 4 is -7");
        vectors.assertEqual(state.index, 2, "negative code updates index");
    }
    runner.add(negative);

    Test maximum("adpcm_maximum_step_saturation");
    {
        audio::ima::State positive{0, 88};
        // step 32767, magnitude 7 reconstructs 61,436, which must clamp rather
        // than wrap through int16.
        maximum.assertEqual(audio::ima::decode(0x7, positive), 32767,
                            "positive maximum clamps to int16 max");
        maximum.assertEqual(positive.index, 88, "maximum index stays at 88");
        audio::ima::State negative{0, 88};
        maximum.assertEqual(audio::ima::decode(0xF, negative), -32768,
                            "negative maximum clamps to int16 min");
        maximum.assertEqual(negative.index, 88, "negative maximum index stays at 88");
        audio::ima::State upper{32767, 88};
        maximum.assertEqual(audio::ima::decode(0x7, upper), 32767,
                            "saturated positive predictor stays saturated");
        audio::ima::State lower{-32768, 88};
        maximum.assertEqual(audio::ima::decode(0xF, lower), -32768,
                            "saturated negative predictor stays saturated");
        audio::ima::State smallPositive{0, 88};
        maximum.assertEqual(audio::ima::decode(0x0, smallPositive), 4095,
                            "step 32767 code 0 is +4095");
        maximum.assertEqual(smallPositive.index, 87, "code 0 lowers index by 1");
        audio::ima::State smallNegative{0, 88};
        maximum.assertEqual(audio::ima::decode(0x8, smallNegative), -4095,
                            "step 32767 negative code 0 is -4095");
        maximum.assertEqual(smallNegative.index, 87, "negative code 0 lowers index");
    }
    runner.add(maximum);

    Test playerTest("adpcm_player_decode");
    {
        std::vector<int16_t> samples(64);
        for (size_t i = 0; i < samples.size(); ++i)
            samples[i] = static_cast<int16_t>(static_cast<int>(i % 16) * 300 - 2000);
        audio::ima::State encoderState{samples[0], 0};
        const std::vector<uint8_t> encoded = encode(samples, encoderState);
        audio::SampledPlayer player;
        playerTest.assertTrue(player.start(2, 0, static_cast<uint24_t>(encoded.size()),
                                       static_cast<uint24_t>(samples.size()),
                                       samples[0], 0),
                          "player accepts a valid stream");
        for (uint8_t byte : encoded)
            playerTest.assertTrue(player.pushByte(byte), "push fits in ring");
        player.enable();
        audio::ima::State expectedState{samples[0], 0};
        bool matches = true;
        for (size_t i = 0; i < samples.size(); ++i) {
            const uint8_t nibble = (i & 1) ? static_cast<uint8_t>(encoded[i >> 1] >> 4)
                                           : static_cast<uint8_t>(encoded[i >> 1] & 0x0F);
            const int16_t expected = audio::ima::decode(nibble, expectedState);
            matches = matches && player.tick() == expected;
        }
        playerTest.assertTrue(matches, "player output matches the standalone decoder");
        playerTest.assertTrue(!player.error(), "no underrun while fed");
    }
    runner.add(playerTest);

    Test loop("adpcm_player_exact_loop");
    {
        std::vector<int16_t> samples(16);
        for (size_t i = 0; i < samples.size(); ++i)
            samples[i] = static_cast<int16_t>(static_cast<int>(i) * 700 - 5000);
        audio::ima::State encoderState{samples[0], 0};
        const std::vector<uint8_t> encoded = encode(samples, encoderState);
        audio::SampledPlayer player;
        loop.assertTrue(player.start(1, 0, static_cast<uint24_t>(encoded.size()),
                                       static_cast<uint24_t>(samples.size()),
                                       samples[0], 0),
                          "short loop starts");
        for (int copy = 0; copy < 3; ++copy)
            for (uint8_t byte : encoded)
                loop.assertTrue(player.pushByte(byte), "loop data fits");
        player.enable();
        audio::ima::State expectedState{samples[0], 0};
        bool matches = true;
        for (size_t i = 0; i < samples.size() * 3; ++i) {
            if (i && (i % samples.size()) == 0)
                expectedState = audio::ima::State{samples[0], 0};
            const size_t within = i % samples.size();
            const uint8_t nibble = (within & 1)
                                       ? static_cast<uint8_t>(encoded[within >> 1] >> 4)
                                       : static_cast<uint8_t>(encoded[within >> 1] & 0x0F);
            const int16_t expected = audio::ima::decode(nibble, expectedState);
            matches = matches && player.tick() == expected;
        }
        loop.assertTrue(matches, "decoder state resets at every exact loop boundary");
    }
    runner.add(loop);

    Test wrapping("adpcm_player_ring_wrap");
    {
        std::vector<int16_t> samples(256);
        for (size_t i = 0; i < samples.size(); ++i)
            samples[i] = static_cast<int16_t>((static_cast<int>(i) * 37) % 4000 - 2000);
        audio::ima::State encoderState{samples[0], 0};
        const std::vector<uint8_t> encoded = encode(samples, encoderState);
        audio::SampledPlayer player;
        wrapping.assertTrue(player.start(0, 0, static_cast<uint24_t>(encoded.size()),
                                       static_cast<uint24_t>(samples.size()),
                                       samples[0], 0),
                          "wrapping stream starts");
        player.enable();
        audio::ima::State expectedState{samples[0], 0};
        size_t produced = 0;
        bool matches = true;
        for (size_t i = 0; i < samples.size(); ++i) {
            while (produced < encoded.size() && player.pushByte(encoded[produced]))
                ++produced;
            const uint8_t nibble = (i & 1) ? static_cast<uint8_t>(encoded[i >> 1] >> 4)
                                           : static_cast<uint8_t>(encoded[i >> 1] & 0x0F);
            const int16_t expected = audio::ima::decode(nibble, expectedState);
            matches = matches && player.tick() == expected;
        }
        wrapping.assertTrue(matches, "read/write positions wrap without losing samples");
        wrapping.assertEqual(produced, encoded.size(), "all bytes eventually accepted");
    }
    runner.add(wrapping);

    Test halfbyteTest("adpcm_player_full_and_half_byte");
    {
        audio::SampledPlayer player;
        halfbyteTest.assertTrue(player.start(5, 0, 127, 254, 0, 0),
                          "full-ring stream starts");
        for (uint8_t i = 0; i < 127; ++i)
            halfbyteTest.assertTrue(player.pushByte(i), "usable ring accepts 127 bytes");
        halfbyteTest.assertEqual(player.freeBytes(), 0, "one sentinel slot keeps full explicit");
        halfbyteTest.assertTrue(!player.pushByte(0), "full ring rejects another byte");
        player.enable();
        player.tick();
        halfbyteTest.assertEqual(player.freeBytes(), 0,
                                 "low nibble does not release its byte");
        player.tick();
        halfbyteTest.assertEqual(player.freeBytes(), 1,
                                 "high nibble releases exactly one byte");
        halfbyteTest.assertTrue(player.pushByte(0x55), "freed byte can be published");
        halfbyteTest.assertEqual(player.freeBytes(), 0, "ring is full again");

        audio::SampledPlayer small;
        halfbyteTest.assertTrue(small.start(5, 0, 1, 2, 0, 0), "single-byte stream starts");
        halfbyteTest.assertTrue(small.pushByte(0x12), "single byte accepted");
        halfbyteTest.assertEqual(small.freeBytes(), 126, "one byte occupies one slot");
        small.enable();
        small.tick();
        halfbyteTest.assertEqual(small.freeBytes(), 126, "half byte still occupies the slot");
        small.tick();
        halfbyteTest.assertEqual(small.freeBytes(), 127, "both nibbles release the slot");
    }
    runner.add(halfbyteTest);

    Test lifecycle("adpcm_player_lifecycle");
    {
        std::vector<int16_t> samples(32, 1200);
        audio::ima::State encoderState{samples[0], 0};
        const std::vector<uint8_t> encoded = encode(samples, encoderState);
        audio::SampledPlayer player;
        lifecycle.assertTrue(player.start(3, 0, static_cast<uint24_t>(encoded.size()),
                                       static_cast<uint24_t>(samples.size()),
                                       samples[0], 0),
                          "lifecycle stream starts");
        for (uint8_t byte : encoded)
            player.pushByte(byte);
        player.enable();
        for (int i = 0; i < 8; ++i)
            player.tick();
        player.pause();
        const int16_t paused = player.tick();
        lifecycle.assertEqual(paused, 0, "pause outputs silence");
        player.resume();
        lifecycle.assertTrue(player.tick() == 1200, "resume retains position and predictor");
        player.setMuted(true);
        lifecycle.assertEqual(player.tick(), 0, "mute silences output without advancing");
        player.setMuted(false);
        lifecycle.assertTrue(player.start(3, 0, static_cast<uint24_t>(encoded.size()),
                                       static_cast<uint24_t>(samples.size()),
                                       samples[0], 0),
                          "same-cue start is accepted as a no-op");
        lifecycle.assertTrue(player.tick() == 1200, "same-cue start does not restart");
        player.stop();
        lifecycle.assertTrue(!player.active(), "stop clears active");
        lifecycle.assertEqual(player.tick(), 0, "stopped player is silent");
    }
    runner.add(lifecycle);

    Test errors("adpcm_player_underrun");
    {
        audio::SampledPlayer player;
        errors.assertTrue(player.start(4, 0, 4, 8, 0, 0), "valid empty stream arms");
        player.enable();
        errors.assertEqual(player.tick(), 0, "empty ring outputs silence");
        errors.assertTrue(player.error(), "empty ring raises an error flag");
        errors.assertTrue(player.underrun(), "empty ring records underrun");
        player.clearUnderrun();
        errors.assertTrue(!player.underrun(), "underrun latch clears");
        errors.assertTrue(!player.start(4, 0, 0, 0, 0, 0), "zero-length cue rejected");
    }
    runner.add(errors);
}

} // namespace adpcm_test

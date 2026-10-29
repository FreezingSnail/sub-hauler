#pragma once

// Permanent host tests for the pure IMA ADPCM codec used by the generation
// reference/asset pipeline. The retired local sampled player is not tested.
// ISR uses; only the FX reader and PWM glue are device-specific.

#include <cstdint>
#include <vector>

#include "audio/ima_adpcm.hpp"
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

}

} // namespace adpcm_test

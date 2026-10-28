// Fixed-point polyphase low-pass for 50 kHz -> 8 kHz reference audio.
// Generated once from a 192-tap Blackman-windowed sinc; do not retune
// without regenerating the reference manifest and hashes.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace audio_reference {

constexpr int kSourceRate = 50000;
constexpr int kOutputRate = 8000;
constexpr uint32_t kCyclesPerMillisecond = 16000;
constexpr uint32_t kCyclesPerSecond = kCyclesPerMillisecond * 1000;
constexpr std::size_t kCaptureStepMilliseconds = 1;
constexpr int kTaps = 192;
constexpr int kPhases = 4;
constexpr int kCoefficientBits = 20;
constexpr int kHalfTaps = kTaps / 2;
constexpr int32_t kCoefficientScale = 1 << kCoefficientBits;

// 1 ms capture steps: convert an exact sample target to a bounded step budget.
constexpr std::size_t capture_step_budget(std::size_t samples, std::size_t margin_ms = 2000) {
    return ((samples + kSourceRate - 1) / kSourceRate) * (1000 / kCaptureStepMilliseconds) +
           margin_ms;
}

// Expected emulated cycles for one second of 1 ms advances (16 MHz).
constexpr uint32_t cycles_for_capture_step() {
    return kCyclesPerMillisecond;
}

// Rows are quarter-sample phases; each row sums to kCoefficientScale.
inline constexpr int32_t kCoefficients[kPhases][kTaps] = {
    {
        0, 1, 3, 6, 8, 9, 6, -2,
        -16, -32, -47, -56, -55, -38, -5, 41,
        94, 141, 170, 168, 128, 49, -62, -187,
        -303, -380, -394, -327, -177, 43, 298, 543,
        724, 790, 706, 461, 78, -388, -857, -1232,
        -1422, -1355, -1006, -399, 382, 1208, 1919, 2355,
        2388, 1952, 1066, -156, -1519, -2772, -3653, -3935,
        -3484, -2288, -485, 1651, 3745, 5380, 6178, 5874,
        4384, 1846, -1381, -4758, -7649, -9429, -9607, -7934,
        -4488, 307, 5712, 10768, 14451, 15851, 14351, 9784,
        2525, -6495, -15872, -23892, -28766, -28900, -23157, -11072,
        7010, 29919, 55748, 82070, 106224, 125655, 138248, 142604,
        138248, 125655, 106224, 82070, 55748, 29919, 7010, -11072,
        -23157, -28900, -28766, -23892, -15872, -6495, 2525, 9784,
        14351, 15851, 14451, 10768, 5712, 307, -4488, -7934,
        -9607, -9429, -7649, -4758, -1381, 1846, 4384, 5874,
        6178, 5380, 3745, 1651, -485, -2288, -3484, -3935,
        -3653, -2772, -1519, -156, 1066, 1952, 2388, 2355,
        1919, 1208, 382, -399, -1006, -1355, -1422, -1232,
        -857, -388, 78, 461, 706, 790, 724, 543,
        298, 43, -177, -327, -394, -380, -303, -187,
        -62, 49, 128, 168, 170, 141, 94, 41,
        -5, -38, -55, -56, -47, -32, -16, -2,
        6, 9, 8, 6, 3, 1, 0, 0,
    },
    {
        0, 1, 2, 5, 8, 9, 7, 0,
        -12, -28, -44, -55, -56, -44, -15, 29,
        81, 130, 165, 172, 142, 72, -32, -156,
        -276, -366, -398, -352, -222, -17, 233, 486,
        688, 787, 742, 537, 185, -268, -745, -1153,
        -1396, -1398, -1120, -572, 177, 1006, 1762, 2280,
        2422, 2105, 1325, 172, -1178, -2484, -3481, -3930,
        -3668, -2652, -980, 1105, 3248, 5036, 6074, 6061,
        4864, 2563, -537, -3934, -7008, -9119, -9731, -8528,
        -5498, -982, 4353, 9593, 13713, 15756, 15016, 11202,
        4550, -4145, -13585, -22113, -27936, -29384, -25182, -14683,
        1978, 23832, 49143, 75588, 100533, 121366, 135832, 142335,
        140146, 129513, 111617, 88416, 62385, 36190, 12348, -7082,
        -20731, -28047, -29298, -25473, -18075, -8869, 387, 8194,
        13491, 15763, 15046, 11856, 7049, 1636, -3391, -7228,
        -9365, -9638, -8221, -5552, -2235, 1085, 3837, 5611,
        6212, 5671, 4213, 2194, 31, -1886, -3252, -3894,
        -3787, -3037, -1853, -492, 786, 1770, 2323, 2405,
        2060, 1403, 591, -216, -876, -1295, -1431, -1300,
        -964, -509, -33, 377, 659, 784, 753, 597,
        363, 105, -128, -297, -385, -390, -327, -218,
        -93, 24, 112, 162, 173, 150, 106, 54,
        6, -31, -52, -57, -50, -36, -19, -5,
        4, 9, 9, 6, 3, 1, 0, 0,
    },
    {
        0, 0, 2, 4, 7, 9, 8, 2,
        -8, -23, -40, -53, -57, -48, -23, 17,
        67, 119, 158, 173, 153, 93, -3, -124,
        -248, -348, -396, -371, -262, -74, 169, 426,
        645, 774, 768, 603, 284, -149, -628, -1063,
        -1355, -1423, -1216, -731, -23, 799, 1589, 2181,
        2428, 2229, 1560, 487, -834, -2176, -3275, -3880,
        -3805, -2974, -1448, 563, 2729, 4645, 5904, 6174,
        5274, 3228, 288, -3090, -6305, -8714, -9741, -9005,
        -6414, -2219, 2990, 8348, 12843, 15484, 15486, 12440,
        6445, -1843, -11241, -20165, -26830, -29511, -26810, -17905,
        -2726, 17965, 42611, 69012, 94584, 116676, 132913, 141507,
        141511, 132913, 116676, 94584, 69012, 42611, 17965, -2726,
        -17905, -26810, -29511, -26830, -20165, -11241, -1843, 6445,
        12440, 15486, 15484, 12843, 8348, 2990, -2219, -6414,
        -9005, -9741, -8714, -6305, -3090, 288, 3228, 5274,
        6174, 5904, 4645, 2729, 563, -1448, -2974, -3805,
        -3880, -3275, -2176, -834, 487, 1560, 2229, 2428,
        2181, 1589, 799, -23, -731, -1216, -1423, -1355,
        -1063, -628, -149, 284, 603, 768, 774, 645,
        426, 169, -74, -262, -371, -396, -348, -248,
        -124, -3, 93, 153, 173, 158, 119, 67,
        17, -23, -48, -57, -53, -40, -23, -8,
        2, 8, 9, 7, 4, 2, 0, 0,
    },
    {
        0, 0, 1, 3, 6, 9, 9, 4,
        -5, -19, -36, -50, -57, -52, -31, 6,
        54, 106, 150, 173, 162, 112, 24, -93,
        -218, -327, -390, -385, -297, -128, 105, 363,
        597, 753, 784, 659, 377, -33, -509, -964,
        -1300, -1431, -1295, -876, -216, 591, 1403, 2060,
        2405, 2323, 1770, 786, -492, -1853, -3037, -3787,
        -3894, -3252, -1886, 31, 2194, 4213, 5671, 6212,
        5611, 3837, 1085, -2235, -5552, -8221, -9638, -9365,
        -7228, -3391, 1636, 7049, 11856, 15046, 15763, 13491,
        8194, 387, -8869, -18075, -25473, -29298, -28047, -20731,
        -7082, 12348, 36190, 62385, 88416, 111617, 129513, 140147,
        142334, 135832, 121366, 100533, 75588, 49143, 23832, 1978,
        -14683, -25182, -29384, -27936, -22113, -13585, -4145, 4550,
        11202, 15016, 15756, 13713, 9593, 4353, -982, -5498,
        -8528, -9731, -9119, -7008, -3934, -537, 2563, 4864,
        6061, 6074, 5036, 3248, 1105, -980, -2652, -3668,
        -3930, -3481, -2484, -1178, 172, 1325, 2105, 2422,
        2280, 1762, 1006, 177, -572, -1120, -1398, -1396,
        -1153, -745, -268, 185, 537, 742, 787, 688,
        486, 233, -17, -222, -352, -398, -366, -276,
        -156, -32, 72, 142, 172, 165, 130, 81,
        29, -15, -44, -56, -55, -44, -28, -12,
        0, 7, 9, 8, 5, 2, 1, 0,
    },
};

// Resample input[start, start + count) to output_count 8 kHz samples.
// The window needs kHalfTaps samples before start and after the final
// source position; callers capture guard samples on both sides.
inline bool resample(const std::vector<int16_t>& input, std::size_t start,
                     std::size_t count, std::size_t output_count,
                     std::vector<int16_t>& output, std::string& error) {
    output.clear();
    if (count == 0 || output_count == 0) {
        error = "resample: empty input or output";
        return false;
    }
    if (start >= input.size() || count > input.size() - start) {
        error = "resample: source range outside input";
        return false;
    }
    const int64_t ratio_num = kSourceRate;
    const int64_t ratio_den = kOutputRate;
    const int64_t last_pos = static_cast<int64_t>(output_count - 1) * ratio_num;
    const int64_t last_base = last_pos / ratio_den;
    if (last_base >= static_cast<int64_t>(count) ||
        static_cast<int64_t>(start) < kHalfTaps - 1 ||
        static_cast<int64_t>(start) + last_base + kHalfTaps >=
            static_cast<int64_t>(input.size())) {
        error = "resample: insufficient guard samples";
        return false;
    }
    output.reserve(output_count);
    for (std::size_t j = 0; j < output_count; ++j) {
        const int64_t pos = static_cast<int64_t>(j) * ratio_num;
        const int64_t base = pos / ratio_den;
        const int phase = static_cast<int>((pos % ratio_den) * kPhases / ratio_den);
        const int64_t first = static_cast<int64_t>(start) + base - (kHalfTaps - 1);
        int64_t acc = 0;
        const int32_t* coeff = kCoefficients[phase];
        for (int i = 0; i < kTaps; ++i)
            acc += static_cast<int64_t>(input[static_cast<std::size_t>(first + i)]) * coeff[i];
        int64_t value = (acc + (int64_t(1) << (kCoefficientBits - 1))) >> kCoefficientBits;
        value = std::max<int64_t>(-32768, std::min<int64_t>(32767, value));
        output.push_back(static_cast<int16_t>(value));
    }
    error.clear();
    return true;
}

} // namespace audio_reference

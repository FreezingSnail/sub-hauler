#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "reference_pcm.hpp"
#include "test.hpp"

// Native checks for the offline reference-audio DSP and capture arithmetic.
// These stay host-only and never enter the AVR image.
namespace audio_reference_test {

inline void suite(TestRunner& runner) {
    Test samples("audio_reference_samples");
    samples.assertEqual(audio_reference::kSourceRate, 50000, "source rate is 50 kHz");
    samples.assertEqual(audio_reference::kOutputRate, 8000, "output rate is 8 kHz");
    samples.assertEqual(audio_reference::kCyclesPerMillisecond, 16000, "1 ms is 16,000 cycles");
    samples.assertEqual(audio_reference::kCyclesPerSecond, 16000000, "1 s is 16,000,000 cycles");
    // 6,146,500 source samples = 122.93 s = 122,930 one-ms steps.
    samples.assertEqual(audio_reference::capture_step_budget(6146500), 125000,
                        "capture budget converts samples to 1 ms steps");
    samples.assertEqual(audio_reference::capture_step_budget(0), 2000,
                        "capture budget keeps a bounded margin");
    runner.add(samples);

    Test dc("audio_reference_resample_dc");
    {
        std::vector<int16_t> input(400, 1000);
        std::vector<int16_t> output;
        std::string error;
        const bool ok = audio_reference::resample(input, 100, 200, 20, output, error);
        dc.assertTrue(ok, "DC resample succeeds");
        dc.assertEqual(output.size(), 20, "DC output count");
        bool all_dc = true;
        for (int16_t sample : output)
            all_dc = all_dc && sample == 1000;
        dc.assertTrue(all_dc, "DC gain is exactly one");
    }
    runner.add(dc);

    Test ramp("audio_reference_quarter_phases_ramp");
    {
        std::vector<int16_t> input(2000);
        for (std::size_t i = 0; i < input.size(); ++i)
            input[i] = static_cast<int16_t>(i * 10);
        std::vector<int16_t> output;
        std::string error;
        const bool ok = audio_reference::resample(input, 200, 1600, 64, output, error);
        ramp.assertTrue(ok, "ramp resample succeeds");
        bool linear = output.size() == 64;
        for (std::size_t j = 0; j < output.size(); ++j) {
            const double expected = 2000.0 + 62.5 * static_cast<double>(j);
            if (std::fabs(static_cast<double>(output[j]) - expected) > 2.5)
                linear = false;
        }
        ramp.assertTrue(linear, "ramp interpolates through all four quarter phases");
    }
    runner.add(ramp);

    Test impulse("audio_reference_quarter_phases_impulse");
    {
        std::vector<int16_t> input(1000, 0);
        input[500] = 30000;
        std::vector<int16_t> output;
        std::string error;
        const bool ok = audio_reference::resample(input, 200, 700, 64, output, error);
        impulse.assertTrue(ok, "impulse resample succeeds");
        bool matches = true;
        bool saw_phase[4] = {false, false, false, false};
        for (std::size_t j = 20; j < 60; ++j) {
            const int64_t pos = static_cast<int64_t>(j) * audio_reference::kSourceRate;
            const int64_t base = pos / audio_reference::kOutputRate;
            const int phase = static_cast<int>((pos % audio_reference::kOutputRate) *
                                               audio_reference::kPhases /
                                               audio_reference::kOutputRate);
            const int64_t tap = 300 - (base - (audio_reference::kHalfTaps - 1));
            if (tap < 0 || tap >= audio_reference::kTaps)
                continue;
            saw_phase[phase] = true;
            const int64_t expected =
                (30000LL * audio_reference::kCoefficients[phase][tap] +
                 (1LL << (audio_reference::kCoefficientBits - 1))) >>
                audio_reference::kCoefficientBits;
            if (std::llabs(static_cast<long long>(output[j]) - expected) > 1)
                matches = false;
        }
        impulse.assertTrue(matches, "impulse taps use the matching quarter phase");
        impulse.assertTrue(saw_phase[0] && saw_phase[1] && saw_phase[2] && saw_phase[3],
                           "test exercised all four quarter phases");
    }
    runner.add(impulse);

    Test filter("audio_reference_lowpass");
    {
        auto tone = [](double hz, std::size_t count, double amplitude) {
            std::vector<int16_t> data(count);
            for (std::size_t i = 0; i < count; ++i) {
                const double t = static_cast<double>(i) / audio_reference::kSourceRate;
                data[i] = static_cast<int16_t>(
                    std::lround(amplitude * std::sin(2.0 * 3.14159265358979323846 * hz * t)));
            }
            return data;
        };
        auto rms = [](const std::vector<int16_t>& data, std::size_t start, std::size_t count) {
            double sum = 0.0;
            for (std::size_t i = 0; i < count; ++i) {
                const double v = static_cast<double>(data[start + i]);
                sum += v * v;
            }
            return std::sqrt(sum / static_cast<double>(count));
        };
        std::vector<int16_t> output;
        std::string error;
        const auto pass = tone(1000.0, 5000, 10000.0);
        const bool pass_ok =
            audio_reference::resample(pass, 200, 4000, 600, output, error);
        const double pass_out = rms(output, 100, 500) / rms(pass, 200, 4000);
        const auto stop = tone(3900.0, 5000, 10000.0);
        const bool stop_ok =
            audio_reference::resample(stop, 200, 4000, 600, output, error);
        const double stop_out = rms(output, 100, 500) / rms(stop, 200, 4000);
        filter.assertTrue(pass_ok && stop_ok, "low-pass resamples succeed");
        filter.assertTrue(pass_out > 0.90, "1 kHz stays in the passband");
        filter.assertTrue(stop_out < 0.05, "3.9 kHz is rejected below 5%");
    }
    runner.add(filter);

    Test guards("audio_reference_guards");
    {
        std::vector<int16_t> input(200, 1);
        std::vector<int16_t> output;
        std::string error;
        guards.assertTrue(!audio_reference::resample(input, 0, 200, 20, output, error),
                          "missing leading guard is rejected");
        guards.assertTrue(!audio_reference::resample(input, 100, 100, 20, output, error),
                          "missing trailing guard is rejected");
        guards.assertTrue(!audio_reference::resample(input, 200, 0, 20, output, error),
                          "empty range is rejected");
    }
    runner.add(guards);
}

} // namespace audio_reference_test

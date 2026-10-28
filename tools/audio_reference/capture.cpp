// Deterministic AVR audio capture through the pinned Ardens absim core.
//
// Loads a bare-metal ATmega32u4 HEX, runs the real ATMlib2FX oscillator and
// compact decoder unchanged, and records the emulated Timer4 PWM output at
// absim's 50 kHz sample rate (SOUND_CYCLES = 320 at 16 MHz). This is a host
// reference generator only; it is not linked into the game.

#include <absim.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "reference_pcm.hpp"

namespace {

constexpr uint64_t kOneMillisecondPs = 1'000'000'000ull;
constexpr int kSourceRate = audio_reference::kSourceRate;
constexpr int kOutputRate = audio_reference::kOutputRate;

struct Options {
    std::string hex;
    std::string raw_out;
    std::string pcm_out;
    std::string raw_pcm_out;
    std::size_t samples = 0;
    std::size_t resample_start = 0;
    std::size_t resample_count = 0;
    std::size_t out_samples = 0;
    int64_t gain_num = 32767;
    int64_t gain_den = 2000;
    bool diag = false;
    std::size_t diag_addr = 0;
};

void usage() {
    std::fprintf(stderr,
                 "capture --hex FILE --samples N [--diag-addr ADDR]\n"
                 "        [--raw-out FILE] [--pcm-out FILE --start N --count N --out-samples N]\n");
}

bool parse_u64(const std::string& text, std::size_t& value) {
    if (text.empty())
        return false;
    char* end = nullptr;
    unsigned long long parsed = std::strtoull(text.c_str(), &end, 0);
    if (end == text.c_str() || *end != '\0')
        return false;
    value = static_cast<std::size_t>(parsed);
    return true;
}

bool parse_args(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto need = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "capture: %s needs a value\n", name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--hex")
            options.hex = need("--hex");
        else if (arg == "--samples") {
            if (!parse_u64(need("--samples"), options.samples))
                return false;
        } else if (arg == "--raw-out")
            options.raw_out = need("--raw-out");
        else if (arg == "--pcm-out")
            options.pcm_out = need("--pcm-out");
        else if (arg == "--raw-pcm-out")
            options.raw_pcm_out = need("--raw-pcm-out");
        else if (arg == "--gain-num") {
            std::size_t value = 0;
            if (!parse_u64(need("--gain-num"), value))
                return false;
            options.gain_num = static_cast<int64_t>(value);
        } else if (arg == "--gain-den") {
            std::size_t value = 0;
            if (!parse_u64(need("--gain-den"), value) || value == 0)
                return false;
            options.gain_den = static_cast<int64_t>(value);
        } else if (arg == "--start") {
            if (!parse_u64(need("--start"), options.resample_start))
                return false;
        } else if (arg == "--count") {
            if (!parse_u64(need("--count"), options.resample_count))
                return false;
        } else if (arg == "--out-samples") {
            if (!parse_u64(need("--out-samples"), options.out_samples))
                return false;
        } else if (arg == "--diag-addr") {
            if (!parse_u64(need("--diag-addr"), options.diag_addr))
                return false;
            options.diag = true;
        } else if (arg == "--help" || arg == "-h") {
            usage();
            std::exit(0);
        } else {
            std::fprintf(stderr, "capture: unknown argument %s\n", arg.c_str());
            return false;
        }
    }
    return !options.hex.empty() && options.samples > 0;
}

bool write_pcm(const std::string& path, const std::vector<int16_t>& samples) {
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    for (int16_t sample : samples) {
        const uint16_t bits = static_cast<uint16_t>(sample);
        const char bytes[2] = {static_cast<char>(bits & 0xFF), static_cast<char>(bits >> 8)};
        f.write(bytes, 2);
    }
    return static_cast<bool>(f);
}

std::vector<int16_t> calibrate(const std::vector<int16_t>& raw, int64_t gain_num,
                               int64_t gain_den, std::size_t& clipped) {
    std::vector<int16_t> out;
    out.reserve(raw.size());
    clipped = 0;
    for (int16_t sample : raw) {
        int64_t scaled = static_cast<int64_t>(sample) * gain_num;
        scaled = scaled >= 0 ? (scaled + gain_den / 2) / gain_den
                             : (scaled - gain_den / 2) / gain_den;
        if (scaled > 32767) {
            scaled = 32767;
            ++clipped;
        } else if (scaled < -32768) {
            scaled = -32768;
            ++clipped;
        }
        out.push_back(static_cast<int16_t>(scaled));
    }
    return out;
}

std::string json_escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\\' || c == '"')
            out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_args(argc, argv, options)) {
        usage();
        return 2;
    }

    std::ifstream stream(options.hex);
    if (!stream) {
        std::fprintf(stderr, "capture: cannot open %s\n", options.hex.c_str());
        return 2;
    }

    absim::arduboy_t device;
    const std::string load_error = device.load_file(options.hex.c_str(), stream);
    if (!load_error.empty()) {
        std::fprintf(stderr, "capture: load failed: %s\n", load_error.c_str());
        return 2;
    }

    // The bare-metal reference image has no Arduboy bootloader or cartridge.
    // absim otherwise starts at the emulated bootloader and appears silent.
    device.program_state.cfg.bootloader = false;
    device.program_state.cfg.boot_to_menu = false;
    device.program_state.cfg.usb_bus_state = absim::USB_BUS_DISCONNECTED;
    device.program_state.cfg.display_type = absim::display_t::type_t::SSD1306;
    device.program_state.cfg.fxport_reg = absim::reg::addr::PORTD;
    device.program_state.cfg.fxport_mask = absim::reg::bit::PORTD::PORTD1;
    device.reset();

    std::vector<int16_t> raw;
    raw.reserve(options.samples + 256);
    std::size_t steps = 0;
    std::size_t samples_at_1s = 0;
    std::size_t samples_at_2s = 0;
    uint64_t cycles_at_1s = 0;
    uint64_t cycles_at_2s = 0;
    const std::size_t step_budget = audio_reference::capture_step_budget(options.samples);

    while (raw.size() < options.samples) {
        device.advance(kOneMillisecondPs);
        auto& buffer = device.core_state.cpu.sound_buffer;
        raw.insert(raw.end(), buffer.begin(), buffer.end());
        buffer.clear();
        ++steps;
        if (steps == 1000) {
            samples_at_1s = raw.size();
            cycles_at_1s = device.core_state.cpu.cycle_count;
        } else if (steps == 2000) {
            samples_at_2s = raw.size();
            cycles_at_2s = device.core_state.cpu.cycle_count;
        }
        if (steps > step_budget) {
            std::fprintf(stderr, "capture: step budget exhausted (%zu steps, %zu samples)\n",
                         steps, raw.size());
            return 3;
        }
    }
    raw.resize(options.samples);

    if (samples_at_1s == 0 || samples_at_2s == 0) {
        std::fprintf(stderr, "capture: capture too short for the 1 s/2 s rate check\n");
        return 3;
    }
    const long long rate_delta = static_cast<long long>(samples_at_2s) -
                                 static_cast<long long>(samples_at_1s);
    const long long cycle_delta = static_cast<long long>(cycles_at_2s) -
                                  static_cast<long long>(cycles_at_1s);
    if (rate_delta < kSourceRate - 2 || rate_delta > kSourceRate + 2) {
        std::fprintf(stderr, "capture: 50 kHz timebase check failed: %lld samples/s\n",
                     rate_delta);
        return 3;
    }
    // steps 1000..2000 span one second (1,000 x 1 ms), not one millisecond.
    // absim batches in PS_BUFFER-sized chunks, so allow a small coarse remainder.
    constexpr long long kCycleTolerance = 20000;
    if (cycle_delta < static_cast<long long>(audio_reference::kCyclesPerSecond) - kCycleTolerance ||
        cycle_delta > static_cast<long long>(audio_reference::kCyclesPerSecond) + kCycleTolerance) {
        std::fprintf(stderr, "capture: one-second advance check failed: %lld cycles/s\n",
                     cycle_delta);
        return 3;
    }
    const long long expected_cycles =
        static_cast<long long>(steps) * audio_reference::kCyclesPerMillisecond;
    const long long total_cycle_delta =
        static_cast<long long>(device.core_state.cpu.cycle_count);
    if (total_cycle_delta < expected_cycles - kCycleTolerance ||
        total_cycle_delta > expected_cycles + kCycleTolerance) {
        std::fprintf(stderr, "capture: total advance check failed: %lld cycles for %zu ms\n",
                     total_cycle_delta, steps);
        return 3;
    }

    int16_t peak_min = std::numeric_limits<int16_t>::max();
    int16_t peak_max = std::numeric_limits<int16_t>::min();
    std::size_t nonzero = 0;
    std::size_t first_nonzero = raw.size();
    std::size_t last_nonzero = raw.size();
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const int16_t v = raw[i];
        peak_min = std::min(peak_min, v);
        peak_max = std::max(peak_max, v);
        if (v != 0) {
            ++nonzero;
            if (first_nonzero == raw.size())
                first_nonzero = i;
            last_nonzero = i;
        }
    }

    if (!options.raw_out.empty() && !write_pcm(options.raw_out, raw)) {
        std::fprintf(stderr, "capture: cannot write %s\n", options.raw_out.c_str());
        return 4;
    }

    std::vector<int16_t> pcm;
    std::vector<int16_t> resampled;
    std::size_t clipped = 0;
    if (!options.pcm_out.empty()) {
        std::string error;
        if (!audio_reference::resample(raw, options.resample_start, options.resample_count,
                                       options.out_samples, resampled, error)) {
            std::fprintf(stderr, "capture: %s\n", error.c_str());
            return 4;
        }
        if (!options.raw_pcm_out.empty() && !write_pcm(options.raw_pcm_out, resampled)) {
            std::fprintf(stderr, "capture: cannot write %s\n", options.raw_pcm_out.c_str());
            return 4;
        }
        pcm = calibrate(resampled, options.gain_num, options.gain_den, clipped);
        if (!write_pcm(options.pcm_out, pcm)) {
            std::fprintf(stderr, "capture: cannot write %s\n", options.pcm_out.c_str());
            return 4;
        }
    }

    std::string diag_hex;
    if (options.diag) {
        constexpr std::size_t kDiagBytes = 20;
        static const char* hex = "0123456789abcdef";
        for (std::size_t i = 0; i < kDiagBytes; ++i) {
            const uint8_t byte = options.diag_addr + i < device.core_state.cpu.data.size()
                                     ? device.core_state.cpu.data[options.diag_addr + i]
                                     : 0;
            diag_hex.push_back(hex[byte >> 4]);
            diag_hex.push_back(hex[byte & 0xF]);
        }
    }

    int16_t out_min = 0;
    int16_t out_max = 0;
    std::size_t out_nonzero = 0;
    int16_t raw_out_min = 0;
    int16_t raw_out_max = 0;
    int64_t pcm_sum = 0;
    if (!pcm.empty()) {
        out_min = std::numeric_limits<int16_t>::max();
        out_max = std::numeric_limits<int16_t>::min();
        raw_out_min = std::numeric_limits<int16_t>::max();
        raw_out_max = std::numeric_limits<int16_t>::min();
        for (std::size_t i = 0; i < pcm.size(); ++i) {
            const int16_t v = pcm[i];
            out_min = std::min(out_min, v);
            out_max = std::max(out_max, v);
            pcm_sum += v;
            if (v != 0)
                ++out_nonzero;
            raw_out_min = std::min(raw_out_min, resampled[i]);
            raw_out_max = std::max(raw_out_max, resampled[i]);
        }
    }

    std::cout << "{\"hex\":\"" << json_escape(options.hex) << "\""
              << ",\"samples\":" << raw.size()
              << ",\"steps\":" << steps
              << ",\"rate_delta\":" << rate_delta
              << ",\"cycle_delta\":" << cycle_delta
              << ",\"min\":" << static_cast<int>(peak_min)
              << ",\"max\":" << static_cast<int>(peak_max)
              << ",\"nonzero\":" << nonzero
              << ",\"first_nonzero\":" << (first_nonzero == raw.size() ? -1 : static_cast<long long>(first_nonzero))
              << ",\"last_nonzero\":" << (last_nonzero == raw.size() ? -1 : static_cast<long long>(last_nonzero))
              << ",\"pcm_samples\":" << pcm.size()
              << ",\"pcm_min\":" << static_cast<int>(out_min)
              << ",\"pcm_max\":" << static_cast<int>(out_max)
              << ",\"pcm_nonzero\":" << out_nonzero
              << ",\"pcm_sum\":" << pcm_sum
              << ",\"pcm_clipped\":" << clipped
              << ",\"raw_pcm_min\":" << static_cast<int>(raw_out_min)
              << ",\"raw_pcm_max\":" << static_cast<int>(raw_out_max)
              << ",\"diag\":\"" << diag_hex << "\""
              << "}" << std::endl;
    return 0;
}

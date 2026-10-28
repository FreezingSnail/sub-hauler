// Headless comparison verifier for the two real backends.
//
// Loads the actual Arduino HEX and the complete fxdata/fxdata.bin into the
// pinned Ardens absim core, drives the D-pad/A/B pins, captures the emulated
// PWM output without gain or normalization, and reads the firmware's SRAM
// diagnostic record. The cart is installed in the emulator's FX model; this is
// not the bare-metal reference runner and it does not disable the cart.
//
// Build/run through `make audio-compare-verify`.

#include <absim.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr uint64_t kOneMillisecondPs = 1'000'000'000ull;
constexpr int kSourceRate = 50000; // absim SOUND_CYCLES=320 at 16 MHz

constexpr uint8_t kPinB = 0x23;
constexpr uint8_t kPinE = 0x2C;
constexpr uint8_t kPinF = 0x2F;

enum Button : uint8_t {
    BTN_NONE = 0,
    BTN_UP,
    BTN_RIGHT,
    BTN_DOWN,
    BTN_LEFT,
    BTN_A,
    BTN_B,
};

struct __attribute__((packed)) AudioCompareDiag {
    uint16_t magic;
    uint8_t backend;
    uint8_t cue;
    uint8_t status;
    uint8_t error;
    uint8_t underrun;
    uint16_t ticks;
    uint16_t max_gap;
    uint16_t starts;
    uint16_t pauses;
    uint16_t resumes;
    uint8_t draws;
};

static_assert(sizeof(AudioCompareDiag) == 18, "device diag ABI is 18 bytes");
static_assert(offsetof(AudioCompareDiag, ticks) == 7, "diag ticks offset");
static_assert(offsetof(AudioCompareDiag, max_gap) == 9, "diag max_gap offset");
static_assert(offsetof(AudioCompareDiag, starts) == 11, "diag starts offset");
static_assert(offsetof(AudioCompareDiag, pauses) == 13, "diag pauses offset");
static_assert(offsetof(AudioCompareDiag, resumes) == 15, "diag resumes offset");
static_assert(offsetof(AudioCompareDiag, draws) == 17, "diag draws offset");

// SampledPlayer byte offsets. Kept local so the permanent player header does
// not grow a debug-only accessor; the harness only reads these two volatile
// position bytes while paused.
constexpr std::size_t kPlayerReadPosOffset = 134;
constexpr std::size_t kPlayerWritePosOffset = 135;

struct Options {
    std::string hex;
    std::string fx;
    std::string out_dir = "build/audio-compare/evidence";
    std::string label = "backend";
    std::size_t diag_addr = 0;
    std::size_t player_addr = 0;
    std::size_t cue_ms = 1500;
};

struct Stats {
    std::size_t samples = 0;
    std::size_t nonzero = 0;
    int16_t min = 0;
    int16_t max = 0;
    double mean = 0.0;
    double rms = 0.0;
    double ac_peak = 0.0;
    std::size_t sign_changes = 0;
};

static void usage() {
    std::fprintf(stderr,
                 "harness --hex FILE --fx FILE --diag-addr ADDR --label NAME "
                 "[--player-addr ADDR] [--out-dir DIR] [--cue-ms N]\n");
}

static bool parse_u64(const std::string &text, std::size_t &value) {
    if (text.empty()) return false;
    char *end = nullptr;
    unsigned long long parsed = std::strtoull(text.c_str(), &end, 0);
    if (end == text.c_str() || *end != '\0') return false;
    value = static_cast<std::size_t>(parsed);
    return true;
}

static bool parse_args(int argc, char **argv, Options &options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need = [&](const char *name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "harness: %s needs a value\n", name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--hex") options.hex = need("--hex");
        else if (arg == "--fx") options.fx = need("--fx");
        else if (arg == "--out-dir") options.out_dir = need("--out-dir");
        else if (arg == "--label") options.label = need("--label");
        else if (arg == "--diag-addr") {
            if (!parse_u64(need("--diag-addr"), options.diag_addr)) return false;
        } else if (arg == "--player-addr") {
            if (!parse_u64(need("--player-addr"), options.player_addr)) return false;
        } else if (arg == "--cue-ms") {
            if (!parse_u64(need("--cue-ms"), options.cue_ms)) return false;
        } else if (arg == "--help" || arg == "-h") {
            usage();
            std::exit(0);
        } else {
            std::fprintf(stderr, "harness: unknown argument %s\n", arg.c_str());
            return false;
        }
    }
    return !options.hex.empty() && !options.fx.empty() && options.diag_addr != 0 &&
           options.cue_ms > 0;
}

static void set_pins(absim::arduboy_t &device, Button button) {
    uint8_t pinb = 0xFF;
    uint8_t pine = 0xFF;
    uint8_t pinf = 0xFF;
    switch (button) {
    case BTN_UP: pinf &= ~0x80; break;
    case BTN_RIGHT: pinf &= ~0x40; break;
    case BTN_DOWN: pinf &= ~0x10; break;
    case BTN_LEFT: pinf &= ~0x20; break;
    case BTN_A: pine &= ~0x40; break;
    case BTN_B: pinb &= ~0x10; break;
    case BTN_NONE: break;
    }
    auto &data = device.core_state.cpu.data;
    data[kPinB] = pinb;
    data[kPinE] = pine;
    data[kPinF] = pinf;
}

static void advance(absim::arduboy_t &device, std::size_t ms,
                    std::vector<int16_t> *capture = nullptr) {
    auto &buffer = device.core_state.cpu.sound_buffer;
    for (std::size_t i = 0; i < ms; ++i) {
        device.advance(kOneMillisecondPs);
        if (capture) {
            capture->insert(capture->end(), buffer.begin(), buffer.end());
        }
        buffer.clear();
    }
}

static bool read_diag(absim::arduboy_t &device, std::size_t addr,
                      AudioCompareDiag &out) {
    auto &data = device.core_state.cpu.data;
    if (addr + sizeof(AudioCompareDiag) > data.size()) return false;
    std::memcpy(&out, &data[addr], sizeof(AudioCompareDiag));
    return out.magic == 0xAC01;
}

static Stats stats(const std::vector<int16_t> &samples) {
    Stats s;
    s.samples = samples.size();
    if (samples.empty()) return s;
    s.min = std::numeric_limits<int16_t>::max();
    s.max = std::numeric_limits<int16_t>::min();
    long double sum = 0.0;
    long double sum_sq = 0.0;
    int last_sign = 0;
    for (int16_t v : samples) {
        s.min = std::min(s.min, v);
        s.max = std::max(s.max, v);
        if (v != 0) ++s.nonzero;
        sum += v;
        sum_sq += static_cast<long double>(v) * v;
        const int sign = (v > 0) - (v < 0);
        if (sign != 0) {
            if (last_sign != 0 && sign != last_sign) ++s.sign_changes;
            last_sign = sign;
        }
    }
    s.mean = static_cast<double>(sum / samples.size());
    s.rms = std::sqrt(static_cast<double>(sum_sq / samples.size()));
    for (int16_t v : samples)
        s.ac_peak = std::max(s.ac_peak, std::fabs(static_cast<double>(v) - s.mean));
    return s;
}

static bool read_player_positions(absim::arduboy_t &device, std::size_t addr,
                                  uint8_t &read_pos, uint8_t &write_pos) {
    auto &data = device.core_state.cpu.data;
    if (addr == 0 || addr + kPlayerWritePosOffset >= data.size()) return false;
    read_pos = data[addr + kPlayerReadPosOffset];
    write_pos = data[addr + kPlayerWritePosOffset];
    return true;
}

static void write_wav(const std::string &path, const std::vector<int16_t> &samples) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;
    const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * 2);
    auto u32 = [&](uint32_t v) {
        char b[4] = {char(v & 0xff), char((v >> 8) & 0xff),
                     char((v >> 16) & 0xff), char((v >> 24) & 0xff)};
        f.write(b, 4);
    };
    auto u16 = [&](uint16_t v) {
        char b[2] = {char(v & 0xff), char((v >> 8) & 0xff)};
        f.write(b, 2);
    };
    f.write("RIFF", 4);
    u32(36 + data_bytes);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    u32(16);
    u16(1);
    u16(1);
    u32(kSourceRate);
    u32(kSourceRate * 2);
    u16(2);
    u16(16);
    f.write("data", 4);
    u32(data_bytes);
    if (!samples.empty())
        f.write(reinterpret_cast<const char *>(samples.data()), data_bytes);
}

static void write_pbm(const std::string &path, const uint8_t *ram) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;
    f << "P4\n128 64\n";
    for (int y = 0; y < 64; ++y) {
        for (int xb = 0; xb < 16; ++xb) {
            uint8_t packed = 0;
            for (int bit = 0; bit < 8; ++bit) {
                const int x = xb * 8 + bit;
                const uint8_t page = ram[(y / 8) * 128 + x];
                const bool lit = (page >> (y % 8)) & 1;
                if (!lit) packed |= static_cast<uint8_t>(0x80 >> bit);
            }
            f.put(static_cast<char>(packed));
        }
    }
}

static bool wait_for_cue(absim::arduboy_t &device, std::size_t addr,
                         uint8_t cue, AudioCompareDiag &diag) {
    for (int i = 0; i < 40; ++i) {
        advance(device, 10);
        if (!read_diag(device, addr, diag)) return false;
        if (diag.cue == cue) return true;
    }
    return false;
}

} // namespace

int main(int argc, char **argv) {
    Options options;
    if (!parse_args(argc, argv, options)) {
        usage();
        return 2;
    }

    std::ifstream hex_stream(options.hex, std::ios::binary);
    if (!hex_stream) {
        std::fprintf(stderr, "harness: cannot open %s\n", options.hex.c_str());
        return 2;
    }
    absim::arduboy_t device;
    const std::string load_error = device.load_file(options.hex.c_str(), hex_stream);
    if (!load_error.empty()) {
        std::fprintf(stderr, "harness: load failed: %s\n", load_error.c_str());
        return 2;
    }

    std::ifstream fx_stream(options.fx, std::ios::binary);
    if (!fx_stream) {
        std::fprintf(stderr, "harness: cannot open %s\n", options.fx.c_str());
        return 2;
    }
    device.program_state.fxdata.assign(std::istreambuf_iterator<char>(fx_stream),
                                       std::istreambuf_iterator<char>());
    device.program_state.fxsave.clear();
    device.program_state.flashcart_loaded = false;

    device.program_state.cfg.bootloader = true;
    device.program_state.cfg.boot_to_menu = false;
    device.program_state.cfg.usb_bus_state = absim::USB_BUS_CONNECTED;
    device.program_state.cfg.display_type = absim::display_t::type_t::SSD1306;
    device.program_state.cfg.fxport_reg = absim::reg::addr::PORTD;
    device.program_state.cfg.fxport_mask = absim::reg::bit::PORTD::PORTD1;
    device.reload_fx();
    device.reset();
    set_pins(device, BTN_NONE);

    std::error_code fs_error;
    std::filesystem::create_directories(options.out_dir, fs_error);
    if (fs_error) {
        std::fprintf(stderr, "harness: cannot create %s: %s\n", options.out_dir.c_str(),
                     fs_error.message().c_str());
        return 2;
    }

    // Boot through Arduboy2's logo/waitNoButtons, then wait for setup().
    AudioCompareDiag diag{};
    const uint8_t expected_backend = options.label == "live" ? 1 : 2;
    bool booted = false;
    for (int i = 0; i < 800; ++i) {
        advance(device, 10);
        if (read_diag(device, options.diag_addr, diag) &&
            diag.backend == expected_backend && diag.status == 1) {
            booted = true;
            break;
        }
    }
    if (!booted) {
        std::fprintf(stderr,
                     "harness: setup/backend not reached at 0x%zx (magic=%04x backend=%u status=%u)\n",
                     options.diag_addr, unsigned(diag.magic), unsigned(diag.backend),
                     unsigned(diag.status));
        std::fprintf(stderr, "harness: pc=%04x cycles=%llu loaded=%d decoded=%d display=%d\n",
                     unsigned(device.core_state.cpu.pc),
                     (unsigned long long)device.core_state.cpu.cycle_count,
                     int(device.core_state.cpu.program_loaded),
                     int(device.core_state.cpu.decoded),
                     int(device.peripherals.display.display_on));
        std::fprintf(stderr, "harness: SREG=%02x TCCR0B=%02x TIMSK0=%02x TIFR0=%02x "
                             "PINB=%02x PINE=%02x PINF=%02x serial=%zu\n",
                     unsigned(device.core_state.cpu.data[0x5f]),
                     unsigned(device.core_state.cpu.data[0x45]),
                     unsigned(device.core_state.cpu.data[0x6e]),
                     unsigned(device.core_state.cpu.data[0x35]),
                     unsigned(device.core_state.cpu.data[kPinB]),
                     unsigned(device.core_state.cpu.data[kPinE]),
                     unsigned(device.core_state.cpu.data[kPinF]),
                     device.core_state.cpu.serial_bytes.size());
        return 3;
    }

    static const char *const names[6] = {
        "HARBOR_BUBBLES", "KELP_DRIFT", "SONAR_LULLABY",
        "ABYSSAL_GLASS", "TIDEPOOL_WALTZ", "BUBBLEJET_RUN",
    };

    bool ok = true;
    std::vector<Stats> cue_stats(6);
    uint16_t max_gap = 0;
    uint8_t max_draws = 0;

    for (uint8_t cue = 0; cue < 6; ++cue) {
        if (cue != 0) {
            set_pins(device, BTN_DOWN);
            advance(device, 80);
            set_pins(device, BTN_NONE);
            advance(device, 120);
            if (!wait_for_cue(device, options.diag_addr, cue, diag)) {
                std::fprintf(stderr, "harness: did not reach cue %u\n", unsigned(cue));
                ok = false;
                break;
            }
        }
        if (!read_diag(device, options.diag_addr, diag)) {
            ok = false;
            break;
        }
        if (diag.cue != cue || diag.status != 1 || diag.error != 0 || diag.underrun != 0) {
            std::fprintf(stderr,
                         "harness: cue %u state bad: got cue=%u status=%u err=%u ur=%u\n",
                         unsigned(cue), unsigned(diag.cue), unsigned(diag.status),
                         unsigned(diag.error), unsigned(diag.underrun));
            ok = false;
            break;
        }
        std::vector<int16_t> samples;
        samples.reserve(options.cue_ms * 55);
        advance(device, options.cue_ms, &samples);
        if (!read_diag(device, options.diag_addr, diag)) {
            ok = false;
            break;
        }
        max_gap = std::max(max_gap, diag.max_gap);
        max_draws = std::max(max_draws, diag.draws);
        if (diag.error != 0 || diag.underrun != 0) {
            std::fprintf(stderr, "harness: cue %u reported err=%u ur=%u\n",
                         unsigned(cue), unsigned(diag.error), unsigned(diag.underrun));
            ok = false;
        }
        const Stats s = stats(samples);
        cue_stats[cue] = s;
        // A constant DC level is not music. Require real AC variation and at
        // least one zero crossing over the representative capture.
        if (s.nonzero == 0 || s.ac_peak < 4.0 || s.sign_changes == 0) {
            std::fprintf(stderr,
                         "harness: cue %u produced no AC waveform "
                         "(nonzero=%zu ac_peak=%.2f sign_changes=%zu)\n",
                         unsigned(cue), s.nonzero, s.ac_peak, s.sign_changes);
            ok = false;
        }
        write_wav(options.out_dir + "/" + options.label + "-cue" +
                      std::to_string(cue) + ".wav", samples);
        write_pbm(options.out_dir + "/" + options.label + "-cue" +
                      std::to_string(cue) + ".pbm",
                  device.peripherals.display.ram.data());
        std::printf(
            "RESULT %s cue=%u name=%s samples=%zu nonzero=%zu min=%d max=%d "
            "mean=%.2f rms=%.2f ac_peak=%.2f sign_changes=%zu ticks=%u "
            "max_gap=%u starts=%u draws=%u\n",
            options.label.c_str(), unsigned(cue), names[cue], s.samples, s.nonzero,
            int(s.min), int(s.max), s.mean, s.rms, s.ac_peak, s.sign_changes,
            unsigned(diag.ticks), unsigned(diag.max_gap), unsigned(diag.starts),
            unsigned(diag.draws));
        std::fflush(stdout);
    }

    bool restart_ok = false;
    bool pause_ok = false;
    bool resume_ok = false;
    bool frozen_ok = false;
    Stats paused_stats{};

    if (ok) {
        // Restart cue 3 and confirm the start counter advanced and audio stayed live.
        if (!read_diag(device, options.diag_addr, diag)) ok = false;
        const uint16_t starts_before = diag.starts;
        set_pins(device, BTN_A);
        advance(device, 80);
        set_pins(device, BTN_NONE);
        advance(device, 120);
        if (read_diag(device, options.diag_addr, diag))
            restart_ok = diag.starts == static_cast<uint16_t>(starts_before + 1) &&
                         diag.status == 1;
        std::vector<int16_t> samples;
        advance(device, 300, &samples);
        const Stats restarted = stats(samples);
        restart_ok = restart_ok && restarted.ac_peak >= 4.0 &&
                     restarted.sign_changes > 0;

        // Pause: UI status, no AC music, and a frozen consumer position. The
        // baseline is taken after the UI reports PAUSED, so input latency is not
        // charged to the freeze check.
        set_pins(device, BTN_B);
        advance(device, 80);
        set_pins(device, BTN_NONE);
        advance(device, 120);
        if (!read_diag(device, options.diag_addr, diag)) ok = false;
        const bool pause_status = diag.status == 2;
        const uint16_t pause_ticks_before = diag.ticks;
        uint8_t read_before = 0;
        uint8_t write_before = 0;
        const bool have_player =
            read_player_positions(device, options.player_addr, read_before, write_before);
        std::vector<int16_t> paused;
        advance(device, 400, &paused);
        paused_stats = stats(paused);
        if (!read_diag(device, options.diag_addr, diag)) ok = false;
        uint8_t read_paused = 0;
        uint8_t write_paused = 0;
        const bool have_player_paused =
            read_player_positions(device, options.player_addr, read_paused, write_paused);
        const bool diag_frozen = diag.ticks == pause_ticks_before;
        const bool player_frozen = have_player && have_player_paused &&
                                   read_paused == read_before &&
                                   write_paused == write_before;
        frozen_ok = options.player_addr != 0 ? player_frozen : diag_frozen;
        // A paused PWM carrier may sit at a constant DC level. That is silence
        // for this check; only AC variation would be audible music.
        pause_ok = pause_status && paused_stats.ac_peak <= 2.0;

        // Resume: UI status and nonconstant audio returns.
        set_pins(device, BTN_B);
        advance(device, 80);
        set_pins(device, BTN_NONE);
        advance(device, 120);
        if (!read_diag(device, options.diag_addr, diag)) ok = false;
        const bool resume_status = diag.status == 1;
        std::vector<int16_t> resumed;
        advance(device, 400, &resumed);
        const Stats resumed_stats = stats(resumed);
        resume_ok = resume_status && resumed_stats.ac_peak >= 4.0 &&
                    resumed_stats.sign_changes > 0;

        std::printf(
            "CONTROL %s restart=%d pause=%d resume=%d frozen=%d paused_samples=%zu "
            "paused_nonzero=%zu paused_min=%d paused_max=%d paused_mean=%.2f "
            "paused_ac_peak=%.2f paused_sign_changes=%zu paused_ticks=%u "
            "max_gap=%u max_draws=%u\n",
            options.label.c_str(), restart_ok ? 1 : 0, pause_ok ? 1 : 0,
            resume_ok ? 1 : 0, frozen_ok ? 1 : 0, paused_stats.samples,
            paused_stats.nonzero, int(paused_stats.min), int(paused_stats.max),
            paused_stats.mean, paused_stats.ac_peak, paused_stats.sign_changes,
            unsigned(diag.ticks), unsigned(max_gap), unsigned(max_draws));
    }

    for (uint8_t cue = 0; cue < 6; ++cue) {
        const Stats &s = cue_stats[cue];
        if (s.nonzero == 0) ok = false;
    }
    if (!restart_ok || !pause_ok || !resume_ok || !frozen_ok) ok = false;

    std::printf("SUMMARY %s cues=6 restart=%d pause=%d resume=%d frozen=%d ok=%d\n",
                options.label.c_str(), restart_ok ? 1 : 0, pause_ok ? 1 : 0,
                resume_ok ? 1 : 0, frozen_ok ? 1 : 0, ok ? 1 : 0);
    return ok ? 0 : 1;
}

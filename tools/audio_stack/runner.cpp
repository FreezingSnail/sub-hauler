// Bounded native stack probe runner for Ardens absim.
//
// Loads a noUSB game ELF/HEX plus its fxdata/fxdata.bin, advances a fixed
// number of milliseconds, then prints the bytes of an SRAM diagnostic symbol.
// It is a build/measurement tool only and is never linked into the game.

#include <absim.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr uint64_t kOneMillisecondPs = 1'000'000'000ull;

struct Options {
    std::string hex;
    std::string fx;
    std::size_t milliseconds = 0;
    std::size_t diag_addr = 0;
    std::size_t diag_bytes = 0;
};

void usage() {
    std::fprintf(stderr,
                 "runner --hex FILE --fx FILE --ms N --diag-addr ADDR --diag-bytes N\n");
}

bool parse_u64(const std::string &text, std::size_t &value) {
    if (text.empty())
        return false;
    char *end = nullptr;
    unsigned long long parsed = std::strtoull(text.c_str(), &end, 0);
    if (end == text.c_str() || *end != '\0')
        return false;
    value = static_cast<std::size_t>(parsed);
    return true;
}

bool parse_args(int argc, char **argv, Options &options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need = [&](const char *name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "runner: %s needs a value\n", name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--hex")
            options.hex = need("--hex");
        else if (arg == "--fx")
            options.fx = need("--fx");
        else if (arg == "--ms") {
            if (!parse_u64(need("--ms"), options.milliseconds))
                return false;
        } else if (arg == "--diag-addr") {
            if (!parse_u64(need("--diag-addr"), options.diag_addr))
                return false;
        } else if (arg == "--diag-bytes") {
            if (!parse_u64(need("--diag-bytes"), options.diag_bytes))
                return false;
        } else if (arg == "--help" || arg == "-h") {
            usage();
            std::exit(0);
        } else {
            std::fprintf(stderr, "runner: unknown argument %s\n", arg.c_str());
            return false;
        }
    }
    return !options.hex.empty() && !options.fx.empty() && options.milliseconds > 0 &&
           options.diag_bytes > 0;
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
        std::fprintf(stderr, "runner: cannot open %s\n", options.hex.c_str());
        return 2;
    }
    absim::arduboy_t device;
    const std::string load_error = device.load_file(options.hex.c_str(), hex_stream);
    if (!load_error.empty()) {
        std::fprintf(stderr, "runner: load failed: %s\n", load_error.c_str());
        return 2;
    }

    std::ifstream fx_stream(options.fx, std::ios::binary);
    if (!fx_stream) {
        std::fprintf(stderr, "runner: cannot open %s\n", options.fx.c_str());
        return 2;
    }
    device.program_state.fxdata.assign(std::istreambuf_iterator<char>(fx_stream),
                                       std::istreambuf_iterator<char>());
    device.program_state.fxsave.clear();
    device.program_state.flashcart_loaded = false;

    device.program_state.cfg.bootloader = false;
    device.program_state.cfg.boot_to_menu = false;
    device.program_state.cfg.usb_bus_state = absim::USB_BUS_DISCONNECTED;
    device.program_state.cfg.display_type = absim::display_t::type_t::SSD1306;
    device.program_state.cfg.fxport_reg = absim::reg::addr::PORTD;
    device.program_state.cfg.fxport_mask = absim::reg::bit::PORTD::PORTD1;
    device.reload_fx();
    device.reset();

    for (std::size_t step = 0; step < options.milliseconds; ++step)
        device.advance(kOneMillisecondPs);

    std::printf("cycles=%llu diag=", static_cast<unsigned long long>(device.core_state.cpu.cycle_count));
    for (std::size_t i = 0; i < options.diag_bytes; ++i) {
        const std::size_t address = options.diag_addr + i;
        const uint8_t value = address < device.core_state.cpu.data.size()
                                  ? device.core_state.cpu.data[address]
                                  : 0xFF;
        std::printf("%02x", value);
    }
    std::printf("\n");
    return 0;
}

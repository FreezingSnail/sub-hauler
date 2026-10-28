#pragma once

// Native full-loop check for the sibling ArduboyStreamFX pure player core.
// Each cue is decoded for two complete authored loops with simulated
// foreground refills; the two loop CRCs must match and no underrun/error may
// latch. This complements the AVR ISR/timing suite.

#include <fstream>
#include <string>
#include <vector>

#include <streamfx/sampled_player.hpp>

#include "test.hpp"

namespace streamfx_loop_test {

inline bool readFile(const char *path, std::vector<uint8_t> &out) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return false;
    out.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !out.empty();
}

inline uint16_t crc16(uint16_t crc, int16_t sample) {
    crc ^= static_cast<uint16_t>(sample);
    for (uint8_t bit = 0; bit < 8; ++bit)
        crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001) : static_cast<uint16_t>(crc >> 1);
    return crc;
}

inline bool decodeTwoLoops(const char *path, uint16_t &first, uint16_t &second) {
    std::vector<uint8_t> bytes;
    if (!readFile(path, bytes))
        return false;
    const uint32_t samples = static_cast<uint32_t>(bytes.size()) * 2u;
    streamfx::SampledPlayer player;
    if (!player.start(0, 0, samples, 33, 0))
        return false;
    player.enable();
    size_t position = 0;
    first = 0xFFFF;
    second = 0xFFFF;
    for (uint32_t tick = 0; tick < samples * 2u; ++tick) {
        while (player.freeBytes() != 0 && position < bytes.size()) {
            player.pushByte(bytes[position++]);
            if (position == bytes.size())
                position = 0;
        }
        const int16_t sample = player.tick();
        if (tick < samples)
            first = crc16(first, sample);
        else
            second = crc16(second, sample);
    }
    const bool clean = !player.error() && !player.underrun();
    player.stop();
    return clean && first == second;
}

inline void suite(TestRunner &runner) {
    Test test("streamfx_full_loops_all_six_cues");
    const char *paths[6] = {
        "fxdata/music/adpcm/harbor_bubbles.bin",
        "fxdata/music/adpcm/kelp_drift.bin",
        "fxdata/music/adpcm/sonar_lullaby.bin",
        "fxdata/music/adpcm/abyssal_glass.bin",
        "fxdata/music/adpcm/tidepool_waltz.bin",
        "fxdata/music/adpcm/bubblejet_run.bin",
    };
    for (uint8_t cue = 0; cue < 6; ++cue) {
        uint16_t first = 0;
        uint16_t second = 0;
        test.assertTrue(decodeTwoLoops(paths[cue], first, second), "two complete loops match and stay clean");
        test.assertEqual(first, second, "first and second loop CRCs match");
    }
    runner.add(test);
}

}   // namespace streamfx_loop_test

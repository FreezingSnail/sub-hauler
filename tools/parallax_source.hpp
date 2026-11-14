#pragma once

#include <cstring>
#include <fstream>
#include <vector>
#include "../src/generated/parallax.hpp"

// Host counterpart of the FX background reader, shared by the preview and
// native renderer tests. Bounds-checked reads expose bad cart offsets early.
struct ParallaxSource {
    std::vector<uint8_t> backgrounds;
    std::vector<uint8_t> opaque;
    std::vector<uint8_t> mask;
    uint32_t backgroundCursor = 0;
    ParallaxSource() {
        std::ifstream input("fxdata/tables/parallax.bin", std::ios::binary);
        backgrounds.assign(std::istreambuf_iterator<char>(input), {});
        std::ifstream flags("fxdata/tables/foreground-opaque.bin", std::ios::binary);
        opaque.assign(std::istreambuf_iterator<char>(flags), {});
        std::ifstream shapes("fxdata/tables/foreground-mask.bin", std::ios::binary);
        mask.assign(std::istreambuf_iterator<char>(shapes), {});
    }
    void opaqueFlags(uint16_t index, uint8_t *buffer, uint8_t length) {
        std::memcpy(buffer, opaque.data() + index, length);
    }
    void opaqueMaskBytes(uint32_t index, uint8_t *buffer) {
        std::memcpy(buffer, mask.data() + index, 8);
    }
    uint8_t backgroundArea(uint16_t index) { return backgrounds.at(index); }
    void beginBackground(uint16_t row, uint8_t block) {
        const uint32_t index = parallax_data::kMapBytes +
            (static_cast<uint32_t>(row) * parallax_data::kBlocksPerRow + block) * 3;
        backgroundCursor = static_cast<uint32_t>(backgrounds.at(index)) |
            (static_cast<uint32_t>(backgrounds.at(index + 1)) << 8) |
            (static_cast<uint32_t>(backgrounds.at(index + 2)) << 16);
    }
    uint8_t nextBackground() { return backgrounds.at(backgroundCursor++); }
    void endBackground() {}
};

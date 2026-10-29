// Host-only panorama export using the same pure 1-bit scene layers as the game.
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include "cave_render.hpp"
#include "exploration_view.hpp"
#include "hazard_view.hpp"
#include "pilot_view.hpp"

struct PackedTerrain {
    std::vector<uint8_t> data;
    void terrainBytes(uint32_t offset, uint8_t *buffer, uint16_t length) {
        std::memcpy(buffer, data.data() + offset, length);
    }
};

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "usage: render-world exploration-terrain.bin world.pbm\n";
        return 1;
    }
    PackedTerrain terrain{};
    terrain.data.resize(cave::TERRAIN_BYTES);
    std::ifstream input(argv[1], std::ios::binary);
    if (!input.read(reinterpret_cast<char *>(terrain.data.data()), terrain.data.size()) || input.peek() != std::char_traits<char>::eof()) {
        std::cerr << "render-world: expected exactly " << cave::TERRAIN_BYTES << " native terrain bytes\n";
        return 1;
    }
    constexpr uint16_t stride = (world::PIXELS_X + 7) / 8;
    std::vector<uint8_t> image(static_cast<size_t>(stride) * world::PIXELS_Y, 0);
    game::State state;
    game::resetWorld(state);
    uint8_t buffer[1024];
    for (int16_t ty = 0; ty < world::TILES_Y; ty += cave::VIEW_TILES_Y) {
        for (int16_t tx = 0; tx < world::TILES_X; tx += cave::VIEW_TILES_X) {
            const cave::Camera camera = cave::cameraAtTile(tx, ty);
            cave::streamFrame(camera, terrain, buffer);
            hazard_view::drawCurrents(buffer, state, camera);
            hazard_view::drawMines(buffer, state, camera);
            exploration_view::draw(buffer, state, camera);
            pilot_view::draw(buffer, state, camera);
            for (int16_t y = 0; y < 64; ++y) {
                for (int16_t x = 0; x < 128; ++x) {
                    const bool white = (buffer[(y / 8) * 128 + x] & (1u << (y % 8))) != 0;
                    const uint16_t px = tx * 8 + x, py = ty * 8 + y;
                    // PBM uses 1 for black, the reverse of the OLED buffer.
                    if (!white)
                        image[static_cast<size_t>(py) * stride + px / 8] |= static_cast<uint8_t>(0x80u >> (px % 8));
                }
            }
        }
    }
    std::ofstream output(argv[2], std::ios::binary);
    output << "P4\n" << world::PIXELS_X << ' ' << world::PIXELS_Y << '\n';
    output.write(reinterpret_cast<const char *>(image.data()), image.size());
    if (!output) {
        std::cerr << "render-world: could not write panorama\n";
        return 1;
    }
    std::cout << "render-world: " << world::PIXELS_X << 'x' << world::PIXELS_Y << " native scene pixels\n";
}

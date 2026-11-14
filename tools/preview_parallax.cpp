// Host preview of the shipping cave, parallax and foreground rules. No art
// approximation or shipping generated files; write a self-contained HTML page.
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include "parallax_view.hpp"
#include "exploration_view.hpp"
#include "pilot_view.hpp"
#include "parallax_source.hpp"

struct PreviewTerrain : ParallaxSource {
    std::vector<uint8_t> pixels, tiles;
    void terrainBytes(uint32_t offset, uint8_t *dest, uint16_t length) {
        std::memcpy(dest, pixels.data() + offset, length);
    }
    uint8_t tileByte(uint16_t index) { return tiles[index]; }
};

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: preview-parallax output.html\n";
        return 1;
    }
    PreviewTerrain terrain;
    std::ifstream pixels("fxdata/tables/exploration-terrain.bin", std::ios::binary);
    terrain.pixels.assign(std::istreambuf_iterator<char>(pixels), {});
    std::ifstream tiles("fxdata/tables/exploration.bin", std::ios::binary);
    terrain.tiles.assign(std::istreambuf_iterator<char>(tiles), {});
    if (terrain.pixels.size() != cave::TERRAIN_BYTES || terrain.tiles.size() != world::WORLD_BYTES) {
        std::cerr << "preview-parallax: canonical terrain/collision data missing\n";
        return 1;
    }
    std::ofstream out(argv[1]);
    out << R"(<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Sub-hauler cave parallax</title>
<style>body{margin:24px;background:#111;color:#eee;font:16px monospace}main{max-width:1650px;margin:auto}
.views{display:flex;gap:24px;flex-wrap:wrap}canvas{display:block;margin:12px 0;image-rendering:pixelated;background:black}
.large{width:512px;max-width:100%;height:auto}button,select{font:inherit;margin:4px;padding:6px}
</style><main><h1>Four area backgrounds</h1><p>Hand-drawn harbour modules, kelp forest, industrial works and basalt/crystal caverns, with dotted strokes scrolling at half camera speed.</p>
<label>Area <select id="route"><option>Tidewall / submerged harbour</option><option>Kelp Galleries / kelp forest</option><option>Broken Works / pipes and gantries</option><option>Glass Trench / basalt and crystals</option></select></label>
<button id="play">Play</button><button id="step">Step</button><button id="reset">Restart</button><span id="position"></span>
<div class="views"><section><h2>Scrolling background</h2><canvas width="128" height="64"></canvas><canvas class="large" width="128" height="64"></canvas></section>
<section><h2>Original cave</h2><canvas width="128" height="64"></canvas><canvas class="large" width="128" height="64"></canvas></section>
<section><h2>Area background in game</h2><canvas width="128" height="64"></canvas><canvas class="large" width="128" height="64"></canvas></section></div>
<p>Forward and reverse camera motion. Terrain, equipment and submarine use the C++ game renderers; HUD and time-based effects are omitted for comparison.</p>
<script>const frames=[)";
    constexpr char hex[] = "0123456789abcdef";
    for (uint8_t route = 0; route < parallax_data::kAreaCount; ++route) {
        if (route) out << ',';
        out << '[';
        for (uint8_t frame = 0; frame < 128; ++frame) {
            if (frame) out << ',';
            const int16_t travel = frame < 64 ? frame : 127 - frame;
            const auto camera = cave::cameraAtPixel(parallax_data::kCheckX[route] + travel,
                parallax_data::kCheckY[route] + (route == parallax_data::kAreaKelp ? travel / 2 : 0));
            world::TileCache cache{};
            world::refreshCache(cache, terrain, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera));
            out << '[';
            for (uint8_t layer = 0; layer < 3; ++layer) {
                if (layer) out << ',';
                uint8_t buffer[1024];
                if (layer == 0) {
                    memset(buffer, 0, sizeof(buffer));
                    world::TileCache water = cache;
                    memset(water.bits, 0, sizeof(water.bits));
                    parallax_view::draw(buffer, camera, water, terrain);
                } else {
                  cave::streamTerrain(camera, terrain, buffer);
                  if (layer == 2) {
                      parallax_view::draw(buffer, camera, cache, terrain);
                      parallax_view::clearBlack(buffer, camera, cache, terrain);
                  }
                cave::drawSettlements(buffer, camera);
                game::State state;
                game::resetWorld(state);
                state.x = cave::pixelOriginX(camera) + 48;
                state.y = cave::pixelOriginY(camera) + 32;
                exploration_view::draw(buffer, state, camera);
                pilot_view::draw(buffer, state, camera);
                }
                out << '"';
                for (uint8_t byte : buffer) out << hex[byte >> 4] << hex[byte & 15];
                out << '"';
            }
            out << ']';
        }
        out << ']';
    }
    out << R"(];let frame=0,playing=false,last=0;
const route=document.querySelector('#route'),canvases=[...document.querySelectorAll('canvas')];
function draw(){const pair=frames[route.selectedIndex][frame];canvases.forEach((c,i)=>{
 const ctx=c.getContext('2d'),img=ctx.createImageData(128,64),bytes=pair[i>>1];
 for(let y=0;y<64;y++)for(let x=0;x<128;x++){const b=parseInt(bytes.slice(((y>>3)*128+x)*2,((y>>3)*128+x)*2+2),16);
 const p=(y*128+x)*4,v=b&(1<<(y&7))?255:0;img.data[p]=img.data[p+1]=img.data[p+2]=v;img.data[p+3]=255}ctx.putImageData(img,0,0)});
 document.querySelector('#position').textContent=`${frame+1}/128`;}
function stop(){playing=false;document.querySelector('#play').textContent='Play'}
document.querySelector('#play').onclick=()=>{playing=!playing;document.querySelector('#play').textContent=playing?'Pause':'Play';last=performance.now()};
document.querySelector('#step').onclick=()=>{stop();frame=(frame+1)%128;draw()};
document.querySelector('#reset').onclick=()=>{stop();frame=0;draw()};route.onchange=()=>{stop();frame=0;draw()};
document.addEventListener('visibilitychange',()=>{if(document.hidden)stop()});
function tick(t){if(playing&&t-last>=50){last=t;frame=(frame+1)%128;draw()}requestAnimationFrame(tick)}draw();requestAnimationFrame(tick);
</script></main></html>)";
    if (!out) return 1;
    std::cout << "preview-parallax: " << argv[1] << " (4 areas, 128 frames each, background/original/composite)\n";
}

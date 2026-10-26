// Repeatable AVR render timings. Run with make fxtest FXTEST_ONLY=test_render,
// optionally DEV_MODE=1 to include the developer banner from the profiler.
// Times include interrupts, actual FX reads and OLED presentation; serial
// printing and checksum work are outside the measured sections.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/render.hpp"
#include "src/hud.hpp"

Arduboy2Base arduboy;
static game::State state;
static world::TileCache cache;
static world::FxTables tables;
constexpr uint8_t kSamples = 8;
constexpr uint16_t kRenderBudgetUs = 15000;   // leave time within a 60 Hz frame for update

static uint16_t checksum() {
    uint16_t value = 0xFFFF;
    for (uint16_t i = 0; i < 1024; ++i) {
        value ^= Arduboy2Base::sBuffer[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xA001 : 0);
    }
    return value;
}

static bool sample(uint8_t id, int16_t x, int16_t y, bool cold) {
    game::resetWorld(state);
    state.x = x;
    state.y = y;
    const cave::Camera camera = cave::cameraCenteredAtPixel(x, y);
    world::clearCache(cache);
    view::frame(state, camera, cache, tables);
    hud::draw(state);
    const uint16_t expected = checksum();
    FX::display(CLEAR_BUFFER);
    uint32_t scene = 0, overlay = 0, display = 0;
    bool ok = true;
    for (uint8_t n = 0; n < kSamples; ++n) {
        if (cold)
            world::clearCache(cache);
        const uint32_t start = micros();
        view::frame(state, camera, cache, tables);
        const uint32_t afterScene = micros();
        hud::draw(state);
        const uint32_t afterHud = micros();
        scene += afterScene - start;
        overlay += afterHud - afterScene;
        ok = (checksum() == expected) && ok;
        const uint32_t beforeDisplay = micros();
        FX::display(CLEAR_BUFFER);
        display += micros() - beforeDisplay;
    }
    Serial.print(F("render case="));
    Serial.print(id);
    Serial.print(cold ? F(" cold") : F(" cached"));
    Serial.print(F(" scene_us="));
    Serial.print(scene / kSamples);
    Serial.print(F(" hud_us="));
    Serial.print(overlay / kSamples);
    Serial.print(F(" display_us="));
    Serial.print(display / kSamples);
    Serial.print(F(" crc="));
    Serial.println(expected, HEX);
    return ok && scene + overlay + display < static_cast<uint32_t>(kRenderBudgetUs) * kSamples;
}

void setup() {
    Serial.begin(9600);
    arduboy.boot();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);
    text::begin();
    bool ok = true;
    for (uint8_t cold = 0; cold < 2; ++cold) {
        ok = sample(0, world::SPAWN_SUB_X, world::SPAWN_SUB_Y, cold) && ok;
        ok = sample(1, 64, 64, cold) && ok;   // solid rock, worst cache lookup load
#if SH_FEATURE_EXPLORATION
        ok = sample(2, exploration_data::ROOM_F_X, exploration_data::ROOM_F_Y, cold) && ok;
        const auto gate = exploration_data::gate(exploration_data::GATE_E_V);
        ok = sample(3, gate.x + gate.w / 2, gate.y + gate.h / 2, cold) && ok;
        const auto find = exploration_data::pickup(exploration_data::PICKUP_W);
        ok = sample(4, find.x, find.y, cold) && ok;
        ok = sample(5, exploration_data::DOCK_D_X, exploration_data::DOCK_D_Y, cold) && ok;
#endif
    }
    Serial.println(ok ? F("P") : F("F"));
}
void loop() {
}

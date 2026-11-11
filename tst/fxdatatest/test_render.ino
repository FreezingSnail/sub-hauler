// Repeatable AVR render timings. Run with make fxtest FXTEST_ONLY=test_render.
// The fixed build has no developer banner or mode override.
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

// Executes the actual AVR helper, not the host formula. Each source byte is
// isolated against zero for all seven shifts, then a mixed deterministic pattern
// checks both sources together.
static bool kernelCheck() {
    bool ok = true;
    for (uint8_t shift = 1; shift < 8; ++shift) {
        const uint8_t factor = static_cast<uint8_t>(1u << (8 - shift));
        for (uint16_t value = 0; value < 256; ++value) {
            const uint8_t v = static_cast<uint8_t>(value);
            if (cave::combineShiftedBytes(v, 0, factor) != static_cast<uint8_t>(v >> shift))
                ok = false;
            if (cave::combineShiftedBytes(0, v, factor) != static_cast<uint8_t>(v << (8 - shift)))
                ok = false;
        }
        for (uint16_t i = 0; i < 256; ++i) {
            const uint8_t a = static_cast<uint8_t>(i * 37 + shift * 11);
            const uint8_t b = static_cast<uint8_t>(i * 91 + shift * 7);
            const uint8_t expected = static_cast<uint8_t>((a >> shift) | (b << (8 - shift)));
            if (cave::combineShiftedBytes(a, b, factor) != expected)
                ok = false;
        }
    }
    return ok;
}

// The pilot frame has no persistent control row: with no contextual notice the
// full 9-px bottom band must keep the scene pixels it started with.
static bool hudNoControlsCheck() {
    game::State state;
    game::resetWorld(state);
    state.discoveryTicks = 0;
    state.contactId = exploration::kNone;
    uint8_t expectedPage6[128];
    uint8_t expectedPage7[128];
    bool ok = true;
    for (uint8_t pattern = 0; pattern < 3; ++pattern) {
        for (uint16_t i = 0; i < 1024; ++i)
            Arduboy2Base::sBuffer[i] = static_cast<uint8_t>(pattern == 0 ? 0x00 : pattern == 1 ? 0xFF : (i * 73 + 19));
        for (uint8_t i = 0; i < 128; ++i) {
            expectedPage6[i] = Arduboy2Base::sBuffer[6 * 128 + i];
            expectedPage7[i] = Arduboy2Base::sBuffer[7 * 128 + i];
        }

        hud::draw(state);

        for (uint8_t i = 0; i < 128; ++i) {
            // y55 is bit 7 of page 6; page 7 starts at y56.
            if ((Arduboy2Base::sBuffer[6 * 128 + i] & 0x80) != (expectedPage6[i] & 0x80) ||
                Arduboy2Base::sBuffer[7 * 128 + i] != expectedPage7[i]) {
                Serial.print(F("hud footer mismatch pattern="));
                Serial.print(pattern);
                Serial.print(F(" x="));
                Serial.println(i);
                ok = false;
            }
        }
    }
    return ok;
}

static bool hudNoticeCheck() {
    static const uint24_t kNoticeText[] = {
        txt_survey_recorded, txt_found_cutter, txt_found_seal, txt_found_thruster,
        txt_found_lens, txt_salvage_hint, txt_need_seal, txt_need_thruster,
        txt_need_cutter, txt_repair_pump, txt_repair_lift,
    };
    uint8_t expected[128];
    bool ok = true;
    for (uint8_t frame = 0; frame < sizeof(kNoticeText) / sizeof(kNoticeText[0]); ++frame) {
        for (uint16_t i = 0; i < 1024; ++i)
            Arduboy2Base::sBuffer[i] = static_cast<uint8_t>(i * 73 + 19);
        cave::fillRect(Arduboy2Base::sBuffer, hud::kHintBackingX, hud::kHintBackingY, hud::kHintBackingW, hud::kHintBackingH, false);
        text::normal();
        text::at(hud::kHintX, hud::kHintY);
        text::print(kNoticeText[frame]);
        for (uint8_t i = 0; i < 128; ++i)
            expected[i] = Arduboy2Base::sBuffer[7 * 128 + i];

        for (uint16_t i = 0; i < 1024; ++i)
            Arduboy2Base::sBuffer[i] = static_cast<uint8_t>(i * 73 + 19);
        cave::fillRect(Arduboy2Base::sBuffer, hud::kHintBackingX, hud::kHintBackingY, hud::kHintBackingW, 1, false);
        hud::drawNotice(static_cast<uint8_t>(frame + 1));
        for (uint8_t i = 0; i < 128; ++i) {
            if (Arduboy2Base::sBuffer[7 * 128 + i] != expected[i]) {
                Serial.print(F("hud notice mismatch frame="));
                Serial.print(frame);
                Serial.print(F(" x="));
                Serial.print(i);
                Serial.print(F(" got="));
                Serial.print(Arduboy2Base::sBuffer[7 * 128 + i], HEX);
                Serial.print(F(" want="));
                Serial.println(expected[i], HEX);
                ok = false;
            }
        }
    }
    return ok;
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
    bool ok = kernelCheck();
    ok = hudNoControlsCheck() && ok;
    ok = hudNoticeCheck() && ok;
    for (uint8_t cold = 0; cold < 2; ++cold) {
        ok = sample(0, world::SPAWN_SUB_X, world::SPAWN_SUB_Y, cold) && ok;
        ok = sample(1, 64, 64, cold) && ok;   // solid rock, worst cache lookup load
        // Fractional origin: exercises the vertical page shift and the last
        // page's bounded upper-row chunks in the device render path.
        ok = sample(6, static_cast<int16_t>(world::SPAWN_SUB_X + 3), static_cast<int16_t>(world::SPAWN_SUB_Y + 5), cold) && ok;
        ok = sample(2, exploration_data::ROOM_F_X, exploration_data::ROOM_F_Y, cold) && ok;
        const auto gate = exploration_data::gate(exploration_data::GATE_E_V);
        ok = sample(3, gate.x + gate.w / 2, gate.y + gate.h / 2, cold) && ok;
        const auto find = exploration_data::pickup(exploration_data::PICKUP_W);
        ok = sample(4, find.x, find.y, cold) && ok;
        ok = sample(5, exploration_data::DOCK_D_X, exploration_data::DOCK_D_Y, cold) && ok;
    }
    Serial.println(ok ? F("P") : F("F"));
}
void loop() {
}

// Actual-cart chart timing and CRCs; checksum/serial work is outside timings.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/chart_view.hpp"
#include "src/pilot.hpp"
#include "src/world_fx.hpp"

Arduboy2Base arduboy;
#if SH_FEATURE_FOG_CHART
static chart::State fog;
static game::State state;
static world::FxTables tables;

static uint16_t checksum() {
    uint16_t value = 0xFFFF;
    for (uint16_t i = 0; i < 1024; ++i) {
        value ^= Arduboy2Base::sBuffer[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xA001 : 0);
    }
    return value;
}

static bool sample(bool revealed, uint8_t phase) {
    game::resetWorld(state);
    chart::reset(fog);
    chart::revealInitial(fog, state);
    if (revealed)
        memset(fog.explored, 0xFF, sizeof(fog.explored));
#if SH_FEATURE_JOBS
    state.activeJob.to = 2;
#if SH_FEATURE_CAMPAIGN
    state.activeJob.kind = game::kJobKindOrdinary;
    state.activeJob.mission = game::kMissionNone;
    state.activeJob.from = 0;
#endif
#endif
#if SH_FEATURE_EXPLORATION
    state.seenGates = 0x1FF;
#endif
    chart_view::draw(Arduboy2Base::sBuffer, fog, state, tables, phase);
    const uint16_t expected = checksum();
    FX::display(CLEAR_BUFFER);
    bool ok = true;
    uint32_t render = 0, display = 0;
    constexpr uint8_t samples = 8;
    for (uint8_t n = 0; n < samples; ++n) {
        const uint32_t start = micros();
        chart_view::draw(Arduboy2Base::sBuffer, fog, state, tables, phase);
        render += micros() - start;
        ok = (checksum() == expected) && ok;
        const uint32_t beforeDisplay = micros();
        FX::display(CLEAR_BUFFER);
        display += micros() - beforeDisplay;
    }
    Serial.print(revealed ? F("chart full") : F("chart fog"));
    Serial.print(F(" phase=")); Serial.print(phase);
    Serial.print(F(" render_us=")); Serial.print(render / samples);
    Serial.print(F(" display_us=")); Serial.print(display / samples);
    Serial.print(F(" crc=")); Serial.println(expected, HEX);
    return ok && render + display < static_cast<uint32_t>(7000) * samples;
}
#endif

void setup() {
    Serial.begin(9600);
    arduboy.boot();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    bool ok = true;
#if SH_FEATURE_FOG_CHART
    ok = sample(false, 0) && ok;
    ok = sample(false, chart_view::kBlinkMask) && ok;
    ok = sample(true, 0) && ok;
    ok = sample(true, chart_view::kBlinkMask) && ok;
#endif
    Serial.println(ok ? F("P") : F("F"));
}
void loop() {}

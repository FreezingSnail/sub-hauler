// Independent live-font reference for the native station board. The reference
// composes labels, status and bars natively; the candidate is station_view::draw
// with the same generated hint row. Compare complete frames.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/station_view.hpp"

Arduboy2Base arduboy;

namespace {
game::State state;

uint16_t checksum() {
    uint16_t value = 0xFFFF;
    for (uint16_t i = 0; i < 1024; ++i) {
        value ^= Arduboy2Base::sBuffer[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xA001 : 0);
    }
    return value;
}

void background(uint8_t value) {
    for (uint16_t i = 0; i < 1024; ++i)
        Arduboy2Base::sBuffer[i] = value;
}

uint24_t stationHint() {
    if (station::serviceBerth(state)) return txt_service_berth_hint;
    if (jobs::deliveryFailed(state)) return txt_short_load;
    if (!station::jobActive(state)) {
        if (!(state.tools & exploration_data::CUTTER)) return txt_cutter_rumor;
        if (state.docked == 1 && !(state.tools & exploration_data::THRUSTER)) return txt_thruster_rumor;
        if (state.docked == 2 && !(state.tools & exploration_data::SEAL)) return txt_seal_rumor;
    }
    return station::jobActive(state) ? txt_autoload_hint : txt_station_hint;
}

void header(uint24_t title) {
    text::normal();
    text::at(station_view::kNameX, station_view::kHeaderY);
    text::print(title);
    station_view::drawBerthIcon(static_cast<uint8_t>(state.docked));
    arduboy.drawFastHLine(0, station_view::kDividerY, 128, WHITE);
    arduboy.drawFastVLine(0, static_cast<int16_t>(station_view::kDividerY - 2), 3, WHITE);
    arduboy.drawFastVLine(127, static_cast<int16_t>(station_view::kDividerY - 2), 3, WHITE);
    station_view::drawStatus(state);
}

void stationReference(uint8_t selected) {
    header(station_view::nameFor(state.docked));
    for (uint8_t row = 0; row < station::rowCount(state); ++row) {
        const uint8_t y = station_view::rowY(row);
        const station::RowKind kind = station::rowKind(row, state);
        if (row == selected && kind != station::kRowJob) {
            arduboy.fillRect(0, y, 128, station_view::kRowInvertH, WHITE);
            text::inverted();
        } else {
            text::normal();
        }
        if (kind == station::kRowJob)
            station_view::drawSummaryMark(y);
        text::at(station_view::kRowX, y);
        if (kind == station::kRowJob)
            text::print(station_view::nameFor(state.activeJob.to));
        else
            text::print(station_view::rowLabel(row, state));
        if (row == selected && kind != station::kRowJob)
            station_view::drawSelectionMark(y);
    }
    text::normal();
    text::at(station_view::kHintX, station_view::kHintY);
    text::print(stationHint());
}

bool stationCheck(uint8_t row, uint8_t pattern) {
    background(0);
    stationReference(row);
    const uint16_t expected = checksum();
    background(pattern);
    station_view::draw(state, row);
    return checksum() == expected;
}
} // namespace

void setup() {
    Serial.begin(9600);
    arduboy.boot();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);
    text::begin();
    bool ok = true;
    uint16_t checks = 0;
    game::resetWorld(state);
    state.air = 7;
    state.tools = exploration_data::CUTTER | exploration_data::SEAL | exploration_data::THRUSTER;
    for (uint8_t berth = 0; berth < campaign::kBerthCount; ++berth) {
        state.docked = berth;
        for (uint8_t active = 0; active < 2; ++active) {
            state.activeJob.kind = active ? game::kJobKindCampaign : game::kJobKindNone;
            state.activeJob.mission = active ? campaign::kPump : game::kMissionNone;
            state.activeJob.from = active ? campaign::kCormorant : game::kJobNone;
            state.activeJob.to = active ? campaign::kBenthos : game::kJobNone;
            state.activeJob.crates = active ? game::kJobCrates : 0;
            for (uint8_t row = station::firstSelectable(state); row < station::rowCount(state); ++row) {
                ok = stationCheck(row, checks & 1 ? 0xFF : 0x55) && ok;
                ++checks;
            }
        }
    }
    state.activeJob.kind = game::kJobKindNone;
    state.activeJob.mission = game::kMissionNone;
    state.activeJob.from = game::kJobNone;
    state.activeJob.to = game::kJobNone;
    state.activeJob.crates = 0;
    state.tools = 0;
    for (uint8_t berth = 0; berth < world::SETTLEMENT_COUNT; ++berth) {
        state.docked = berth;
        ok = stationCheck(0, 0xFF) && ok;
        ++checks;
        state.tools = exploration_data::CUTTER;
    }
    state.docked = 0;
    state.lastDelivery = jobs::kDeliveryFailed;
    ok = stationCheck(0, 0xFF) && ok;
    ++checks;
    state.lastDelivery = jobs::kDeliveryNone;
    Serial.print(F("menu pixel cases="));
    Serial.println(checks);
    ok = checks == 29 && ok;
    Serial.println(ok ? F("P") : F("F"));
}

void loop() {}

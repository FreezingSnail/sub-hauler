// Independent baked-stamp reference for the native station board. The
// reference chooses the generated berth-name / row-label / bark frames and
// composes bars natively; the candidate is station_view::draw. Compare
// complete frames.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/station_view.hpp"
#include "src/generated/text_layout.hpp"

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

void header(uint8_t frame) {
    text::stampFrame(img_berth_names, frame, station_view::kNameX, station_view::kHeaderY);
    station_view::drawBerthIcon(static_cast<uint8_t>(state.docked));
    arduboy.drawFastHLine(0, station_view::kDividerY, 128, WHITE);
    arduboy.drawFastVLine(0, static_cast<int16_t>(station_view::kDividerY - 2), 3, WHITE);
    arduboy.drawFastVLine(127, static_cast<int16_t>(station_view::kDividerY - 2), 3, WHITE);
}

void stationReference(uint8_t selected) {
    header(station_view::nameFor(static_cast<uint8_t>(state.docked)));
    for (uint8_t row = 0; row < station::rowCount(state); ++row) {
        const uint8_t y = station_view::rowY(row);
        const station::RowKind kind = station::rowKind(row, state);
        const bool highlighted = row == selected && kind != station::kRowJob;
        if (highlighted)
            arduboy.fillRect(0, y, 128, station_view::kRowInvertH, WHITE);
        if (kind == station::kRowJob) {
            station_view::drawSummaryMark(y);
            text::stampFrame(img_berth_names, station_view::nameFor(state.activeJob.to), station_view::kRowX, y);
        } else {
            const uint8_t frame = static_cast<uint8_t>(station_view::rowLabel(row, state) +
                                                       (highlighted ? text_layout::kRowLabelInvertedBase : 0));
            text::stampFrame(img_row_labels, frame, station_view::kRowX, y);
        }
        if (highlighted)
            station_view::drawSelectionMark(y);
    }
    const uint8_t bark = station_view::dockBarkFrame(campaign::dockBark(state, static_cast<uint8_t>(state.docked)));
    if (bark != campaign::kDockBarkNone)
        text::stampFrame(img_dock_barks, bark, station_view::kHintX, station_view::kBarkY);
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

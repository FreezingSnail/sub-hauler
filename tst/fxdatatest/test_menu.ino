// Independent live-font reference for the baked station/shop screens. The
// reference composes labels, prices, values and bars natively; the candidate
// reads generated panels. Compare complete frames, including clipped hints.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/station_view.hpp"
#include "src/shop_view.hpp"

Arduboy2Base arduboy;

#if SH_STATION_MENU_PANELS && SH_SHOP_MENU_PANELS
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
    text::at(station_view::kCreditsX, station_view::kHeaderY);
    text::print(txt_credits);
    text::number(state.credits);
    arduboy.drawFastHLine(0, station_view::kDividerY, 128, WHITE);
    station_view::drawStatus(state);
}

void stationReference(uint8_t selected) {
    header(station_view::nameFor(state.docked));
    for (uint8_t row = 0; row < station::rowCount(state); ++row) {
        const uint8_t y = station_view::rowY(row);
        if (row == selected) {
            arduboy.fillRect(0, y, 128, station_view::kRowInvertH, WHITE);
            text::inverted();
        } else text::normal();
        text::at(station_view::kRowX, y);
        station_view::printRow(row, state);
    }
    text::normal();
    text::at(station_view::kHintX, station_view::kHintY);
    text::print(stationHint());
}

void shopReference(uint8_t selected) {
    header(txt_shop);
    for (uint8_t row = 0; row < shop::kItemCount; ++row) {
        const shop::Item item = static_cast<shop::Item>(row);
        const uint8_t y = shop_view::panelRowY(row);
        if (row == selected) {
            arduboy.fillRect(0, y - shop_view::kPanelInsetY, 128, shop_view::kPanelInvertH, WHITE);
            text::inverted();
        } else text::normal();
        text::at(shop_view::kPanelRowX, y);
        text::print(shop_view::itemLabel(item));
        text::at(shop_view::kPanelLevelX, y);
        text::character('L');
        const uint8_t level = shop::levelFor(state, item);
        text::number(level);
        text::at(shop_view::kPanelPriceX, y);
        if (level == shop::kMaxLevel) text::print(txt_max);
        else text::number(shop::priceFor(item));
    }
    text::normal();
    text::at(shop_view::kHintX, shop_view::kHintY);
    const shop::Result status = shop::canBuy(state, static_cast<shop::Item>(selected));
    text::print(status == shop::kSoldOut ? txt_max_level : status == shop::kNoCredits ? txt_low_credits : txt_buy_hint);
}

bool stationCheck(uint8_t row, uint8_t pattern) {
    background(0);
    stationReference(row);
    const uint16_t expected = checksum();
    background(pattern);
    station_view::draw(state, row);
    return checksum() == expected;
}

bool shopCheck(uint8_t row, uint8_t pattern) {
    background(0);
    shopReference(row);
    const uint16_t expected = checksum();
    background(pattern);
    shop_view::draw(state, row);
    return checksum() == expected;
}
} // namespace
#endif

void setup() {
    Serial.begin(9600);
    arduboy.boot();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);
    text::begin();
    bool ok = true;
#if SH_STATION_MENU_PANELS && SH_SHOP_MENU_PANELS
    uint16_t checks = 0;
    game::resetWorld(state);
    state.credits = 65535;
    state.airLevel = state.hullLevel = shop::kMaxLevel;
    state.air = 7;
    state.hull = 1;
    state.tools = exploration_data::CUTTER | exploration_data::SEAL | exploration_data::THRUSTER;
    for (uint8_t berth = 0; berth < campaign::kBerthCount; ++berth) {
        state.docked = berth;
        for (uint8_t active = 0; active < 2; ++active) {
            state.activeJob.kind = active ? game::kJobKindOrdinary : game::kJobKindNone;
            state.activeJob.to = (berth + 1) % world::SETTLEMENT_COUNT;
            state.activeJob.reward = 145;
            for (uint8_t row = station::firstSelectable(state); row < station::rowCount(state); ++row) {
                ok = stationCheck(row, checks & 1 ? 0xFF : 0x55) && ok;
                ++checks;
            }
        }
    }
    state.activeJob.kind = game::kJobKindNone;
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
    for (uint8_t air = 0; air <= shop::kMaxLevel; ++air)
        for (uint8_t hull = 0; hull <= shop::kMaxLevel; ++hull)
            for (uint8_t row = 0; row < shop::kItemCount; ++row) {
                state.airLevel = air;
                state.hullLevel = hull;
                state.credits = checks & 1 ? 0 : 65535;
                ok = shopCheck(row, 0xAA) && ok;
                ++checks;
            }
    Serial.print(F("menu pixel cases="));
    Serial.println(checks);
    ok = checks == 69 && ok;
#endif
    Serial.println(ok ? F("P") : F("F"));
}

void loop() {}

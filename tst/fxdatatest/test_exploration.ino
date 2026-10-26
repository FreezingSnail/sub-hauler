// AVR traversal rules, actual generated object records and 1-bit gate/pickup draw.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/exploration.hpp"
#include "src/exploration_view.hpp"
#include "src/pilot.hpp"
#include "src/world_fx.hpp"
#if SH_DEV_MODE
#include "src/dev_mode.hpp"
#endif
Arduboy2Base arduboy;
void setup() {
    Serial.begin(9600);
    arduboy.boot();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    bool ok = true;
#if SH_FEATURE_EXPLORATION
    game::State state;
    game::resetWorld(state);
    const auto cutter = exploration_data::pickup(exploration_data::PICKUP_W);
    state.x = cutter.x;
    state.y = cutter.y;
    for (uint8_t tick = 0; tick < exploration::kDwellTicks; ++tick)
        exploration::update(state);
    ok = ok && (state.tools & exploration_data::CUTTER);
    const auto gate = exploration_data::gate(exploration_data::GATE_E_V);
    ok = ok && exploration::closed(state, exploration_data::GATE_E_V, gate);
    state.x = gate.x - 14;
    state.y = gate.y + gate.h / 2;
    for (uint8_t tick = 0; tick < exploration::kDwellTicks; ++tick)
        exploration::update(state);
    ok = ok && !exploration::closed(state, exploration_data::GATE_E_V, gate);
    const auto purse = exploration_data::pickup(exploration_data::PICKUP_TC);
    state.x = purse.x;
    state.y = purse.y;
    for (uint8_t tick = 0; tick < exploration::kDwellTicks * 2; ++tick)
        exploration::update(state);
    ok = ok && state.credits == purse.amount;
    const auto pressure = exploration_data::gate(exploration_data::GATE_H_J);
    ok = ok && exploration::blockingGate(state, pressure.x + pressure.w / 2, pressure.y + pressure.h / 2) != exploration::kNone;
    state.tools |= exploration_data::SEAL;
    ok = ok && exploration::blockingGate(state, pressure.x + pressure.w / 2, pressure.y + pressure.h / 2) == exploration::kNone;
    const cave::Camera camera = cave::cameraCenteredAtPixel(purse.x, purse.y);
    exploration_view::draw(Arduboy2Base::sBuffer, state, camera);
    FX::display(CLEAR_BUFFER);
#endif
#if SH_DEV_MODE
    game::State inspect;
    game::resetWorld(inspect);
    inspect.air = 0;
    inspect.x = 100;
    inspect.y = 100;
    dev_mode::step(inspect, game::Input{false, false, false, true}, true);
    ok = ok && inspect.x == 100 + dev_mode::kFastStepPixels && inspect.air == game::airCapacity(inspect);
#if SH_FEATURE_STATIONS
    ok = ok && inspect.docked == -1;
#endif
#if SH_FEATURE_FOG_CHART
    chart::State chart;
    chart::reset(chart);
    dev_mode::revealChart(chart);
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
        ok = ok && chart.explored[i] == 0xFF;
#endif
#endif
    Serial.println(ok ? F("P") : F("F"));
}
void loop() {
}

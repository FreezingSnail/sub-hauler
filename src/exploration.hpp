#pragma once
#include "campaign.hpp"
#include "game.hpp"
#include "generated/exploration.hpp"
#include "world.hpp"

// Pure run-local exploration rules; immutable records stay in program flash.
namespace exploration {
using game::State;
namespace data = exploration_data;
constexpr uint8_t kNone = 0xFF;
constexpr uint8_t kDwellTicks = 30;
// Contact-id namespace: pickup ids first, then gate dwell targets, then the
// campaign survey rings. The ranges never share a value.
constexpr uint8_t kSurveyBase = static_cast<uint8_t>(data::PICKUP_COUNT + data::GATE_COUNT);
constexpr uint8_t kSurveyLast = static_cast<uint8_t>(kSurveyBase + world::SURVEY_COUNT - 1);
constexpr int16_t kOperateSpeed = 10 * game::kQ8One;

inline bool slow(const State &s) {
    return s.vxQ8 >= -kOperateSpeed && s.vxQ8 <= kOperateSpeed && s.vyQ8 >= -kOperateSpeed && s.vyQ8 <= kOperateSpeed;
}
inline bool closed(const State &s, uint8_t id, const data::Gate &g) {
    if (g.required == data::CUTTER)
        return (s.openGates & (static_cast<uint16_t>(1u) << id)) == 0;
    return (s.tools & g.required) != g.required;
}
inline uint8_t blockingGate(const State &s, int16_t x, int16_t y) {
    for (uint8_t i = 0; i < data::GATE_COUNT; ++i) {
        const data::Gate g = data::gate(i);
        if (x + game::kHullHalfW > g.x && x - game::kHullHalfW < g.x + g.w && y + game::kHullHalfH > g.y && y - game::kHullHalfH < g.y + g.h && closed(s, i, g))
            return i;
    }
    return kNone;
}
inline bool nearGate(const State &s, const data::Gate &g, int16_t margin) {
    return s.x >= g.x - margin && s.x <= g.x + g.w + margin && s.y >= g.y - margin && s.y <= g.y + g.h + margin;
}
inline void beginStep(State &s) {
    if (s.discoveryTicks)
        --s.discoveryTicks;
    if (s.impactAxis == 4)
        s.impactAxis = 0;
}
inline void claim(State &s, uint8_t id, const data::Pickup &p) {
    const uint8_t mask = static_cast<uint8_t>(1u << id);
    if (s.pickups & mask)
        return;
    s.pickups |= mask;
    if (p.kind == 1)
        s.tools |= data::CUTTER;
    else if (p.kind == 2)
        s.tools |= data::SEAL;
    else if (p.kind == 3)
        s.tools |= data::THRUSTER;
    else if (p.kind == 6)
        s.tools |= data::LENS;
    s.lastDiscovery = id;
    s.discoveryTicks = 180;
}
inline void update(State &s) {
    if (s.docked >= 0) {
        s.contactId = kNone;
        s.contactTicks = 0;
        return;
    }
    uint8_t target = kNone;
    uint8_t hint = kNone;
    // Survey rings record a fact without occupying the cargo slot. They can be
    // found before the campaign asks for them and never repeat. After FIRST the
    // wider hint radius tells a moving pilot to stop near the ring.
    for (uint8_t i = 0; i < world::surveyCount(); ++i) {
        world::Survey survey;
        if (!world::surveyInfo(i, survey))
            continue;
        if (s.surveys & (static_cast<uint8_t>(1u) << survey.id))
            continue;
        const int16_t dx = static_cast<int16_t>(s.x - survey.pixel_x);
        const int16_t dy = static_cast<int16_t>(s.y - survey.pixel_y);
        if (dx >= -18 && dx <= 18 && dy >= -12 && dy <= 12 && slow(s))
            target = static_cast<uint8_t>(kSurveyBase + i);
        else if (dx >= -48 && dx <= 48 && dy >= -48 && dy <= 48 && campaign::hasMilestone(s, campaign::kMilestoneFirst))
            hint = static_cast<uint8_t>(kSurveyBase + i);
    }
    for (uint8_t i = 0; i < data::GATE_COUNT; ++i) {
        const data::Gate g = data::gate(i);
        if (nearGate(s, g, 48))
            s.seenGates |= static_cast<uint16_t>(1u) << i;
        if (g.required == data::CUTTER && (s.tools & data::CUTTER) && closed(s, i, g) && nearGate(s, g, 18) && slow(s))
            target = static_cast<uint8_t>(data::PICKUP_COUNT + i);
    }
    for (uint8_t i = 0; i < data::PICKUP_COUNT; ++i) {
        if (s.pickups & (1u << i))
            continue;
        const data::Pickup p = data::pickup(i);
        if (s.x >= p.x - 18 && s.x <= p.x + 18 && s.y >= p.y - 12 && s.y <= p.y + 12 && slow(s))
            target = i;
    }
    if (target == kNone) {
        s.contactId = hint;
        s.contactTicks = 0;
        return;
    }
    if (s.contactId != target) {
        s.contactId = target;
        s.contactTicks = 0;
    }
    if (++s.contactTicks < kDwellTicks)
        return;
    s.contactTicks = 0;
    if (target < data::PICKUP_COUNT) {
        claim(s, target, data::pickup(target));
    } else {
        if (target >= kSurveyBase) {
            world::Survey survey;
            if (world::surveyInfo(static_cast<uint8_t>(target - kSurveyBase), survey)) {
                s.surveys |= static_cast<uint8_t>(1u) << survey.id;
                s.lastDiscovery = kNone;
                s.discoveryTicks = 180;
            }
            return;
        }
        s.openGates |= static_cast<uint16_t>(1u) << (target - data::PICKUP_COUNT);
    }
}
}   // namespace exploration

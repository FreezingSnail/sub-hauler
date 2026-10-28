#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_CAMPAIGN

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <queue>
#include <vector>

#include "../src/campaign.hpp"
#include "../src/exploration.hpp"
#include "../src/game.hpp"
#include "../src/hazards.hpp"
#include "../src/pilot.hpp"
#include "../src/station.hpp"
#include "../src/world.hpp"
#include "test.hpp"

// End-to-end first-delivery route on the generated map. The test loads the
// real 320x160 collision plane from the generated cart table, starts at the
// Tidewall dock anchor (not reset()'s synthetic origin), accepts FIRST with
// four cargo units, and flies with whole-pixel hull collision, ballast,
// currents, mines and auto-docking. A deterministic carrot-following autopilot
// represents relaxed play: it corrects heading and depth but does not take
// expert shortcuts or optimise ballast against air.
namespace campaign_route_test {

constexpr int16_t kLookaheadPx = 24;
constexpr int16_t kVerticalTolerancePx = 2;
constexpr int16_t kCruiseSpeedQ8 = 12 * game::kQ8One;
constexpr uint16_t kTimeoutTicks = 6 * 60 * 60;   // six relaxed minutes
constexpr int16_t kRequiredAirMargin = 60;        // 20% of the 300-unit tank

struct RealWorld {
    std::array<uint8_t, world::WORLD_BYTES> tiles{};

    bool load() {
        std::ifstream file("fxdata/tables/exploration.bin", std::ios::binary);
        if (!file)
            return false;
        file.read(reinterpret_cast<char *>(tiles.data()), static_cast<std::streamsize>(tiles.size()));
        return file.good() || file.eof();
    }

    uint8_t tileByte(uint16_t index) const {
        return tiles[index];
    }
};

struct Point {
    int16_t x;
    int16_t y;
};

inline bool hullClear(const RealWorld &world, int16_t pixelX, int16_t pixelY, game::State &state) {
    const int16_t left = static_cast<int16_t>(pixelX - game::kHullHalfW);
    const int16_t top = static_cast<int16_t>(pixelY - game::kHullHalfH);
    if (world::rectHits(world, left, top, game::kHullW, game::kHullH))
        return false;
    if (exploration::blockingGate(state, pixelX, pixelY) != exploration::kNone)
        return false;
#if SH_FEATURE_MINES
    // The relaxed route keeps clear of armed mines. A real pilot can see the
    // generated mine markers; the test route should not prove air margin by
    // deliberately soaking a mine hit.
    const int16_t right = static_cast<int16_t>(pixelX + game::kHullHalfW - 1);
    const int16_t bottom = static_cast<int16_t>(pixelY + game::kHullHalfH - 1);
    for (uint8_t id = 0; id < world::mineCount(); ++id) {
        if (!hazards::alive(state, id))
            continue;
        world::Mine mine;
        if (!world::mineInfo(id, mine))
            continue;
        const int16_t nearestX = mine.x < left ? left : (mine.x > right ? right : mine.x);
        const int16_t nearestY = mine.y < top ? top : (mine.y > bottom ? bottom : mine.y);
        const int32_t dx = static_cast<int32_t>(mine.x - nearestX);
        const int32_t dy = static_cast<int32_t>(mine.y - nearestY);
        if (dx * dx + dy * dy <= world::MINE_RADIUS_PX * world::MINE_RADIUS_PX)
            return false;
    }
#endif
    return true;
}

// Breadth-first route over whole tile centres with the real 26x8 hull and the
// closed generated gates. The routed path is only an input to the autopilot;
// movement still runs through the production collision and docking rules.
inline bool buildRoute(const RealWorld &world, game::State &state, std::vector<Point> &path) {
    const int16_t startX = static_cast<int16_t>(exploration_data::DOCK_T_X / world::TILE_PIXELS);
    const int16_t startY = static_cast<int16_t>(exploration_data::DOCK_T_Y / world::TILE_PIXELS);
    const int16_t goalX = static_cast<int16_t>(exploration_data::DOCK_B_X / world::TILE_PIXELS);
    const int16_t goalY = static_cast<int16_t>(exploration_data::DOCK_B_Y / world::TILE_PIXELS);

    std::vector<int32_t> previous(static_cast<size_t>(world::TILES_X) * world::TILES_Y, -1);
    std::queue<Point> pending;
    const int32_t start = static_cast<int32_t>(startY * world::TILES_X + startX);
    previous[static_cast<size_t>(start)] = start;
    pending.push({startX, startY});

    const int16_t dx[] = {1, -1, 0, 0};
    const int16_t dy[] = {0, 0, 1, -1};
    while (!pending.empty()) {
        const Point current = pending.front();
        pending.pop();
        if (current.x == goalX && current.y == goalY)
            break;
        for (uint8_t direction = 0; direction < 4; ++direction) {
            const int16_t nx = static_cast<int16_t>(current.x + dx[direction]);
            const int16_t ny = static_cast<int16_t>(current.y + dy[direction]);
            if (nx < 2 || nx > 317 || ny < 1 || ny > 158)
                continue;
            const int32_t index = static_cast<int32_t>(ny * world::TILES_X + nx);
            if (previous[static_cast<size_t>(index)] != -1)
                continue;
            if (!hullClear(world, static_cast<int16_t>(nx * world::TILE_PIXELS), static_cast<int16_t>(ny * world::TILE_PIXELS), state))
                continue;
            previous[static_cast<size_t>(index)] = current.y * world::TILES_X + current.x;
            pending.push({nx, ny});
        }
    }

    const int32_t goal = static_cast<int32_t>(goalY * world::TILES_X + goalX);
    if (previous[static_cast<size_t>(goal)] == -1)
        return false;

    path.clear();
    int32_t cursor = goal;
    while (cursor != start) {
        path.push_back({static_cast<int16_t>(cursor % world::TILES_X), static_cast<int16_t>(cursor / world::TILES_X)});
        cursor = previous[static_cast<size_t>(cursor)];
    }
    path.push_back({startX, startY});
    std::reverse(path.begin(), path.end());
    return true;
}

inline int32_t distanceSq(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
    const int32_t dx = ax - bx;
    const int32_t dy = ay - by;
    return dx * dx + dy * dy;
}

// Closest route tile near the cursor, then walk forward by the lookahead.
inline Point carrot(const std::vector<Point> &path, size_t &cursor, int16_t pixelX, int16_t pixelY) {
    size_t best = cursor;
    int32_t bestDistance = 0x7FFFFFFF;
    const size_t limit = std::min(path.size(), cursor + 40);
    for (size_t i = cursor; i < limit; ++i) {
        const int32_t distance = distanceSq(path[i].x * world::TILE_PIXELS, path[i].y * world::TILE_PIXELS, pixelX, pixelY);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }

    size_t ahead = best;
    int32_t travelled = 0;
    while (ahead + 1 < path.size() && travelled < kLookaheadPx) {
        travelled += std::abs(static_cast<int32_t>(path[ahead + 1].x - path[ahead].x) * world::TILE_PIXELS) +
                      std::abs(static_cast<int32_t>(path[ahead + 1].y - path[ahead].y) * world::TILE_PIXELS);
        ++ahead;
    }
    cursor = std::max(cursor, ahead > 4 ? ahead - 4 : 0);
    return {static_cast<int16_t>(path[ahead].x * world::TILE_PIXELS), static_cast<int16_t>(path[ahead].y * world::TILE_PIXELS)};
}

struct FlightResult {
    bool routeBuilt = false;
    bool docked = false;
    bool delivered = false;
    int16_t arrivalAir = 0;
    int16_t serviceAir = 0;
    uint8_t serviceBallast = 0;
    uint16_t ticks = 0;
    uint8_t minimumHull = 0xFF;
};

// Relaxed autopilot: pursue a carrot with horizontal thrust and bang-bang
// ballast. It never deliberately grazes a wall and never taps the blow valve
// when already moving toward the requested depth.
inline FlightResult flyFirstDelivery() {
    FlightResult result;
    RealWorld world;
    if (!world.load())
        return result;

    game::State state;
    game::reset(state);
    state.docked = static_cast<int8_t>(campaign::kTidewall);
    state.x = exploration_data::DOCK_T_X;
    state.y = exploration_data::DOCK_T_Y;
    state.ballast = static_cast<uint8_t>(game::neutralFor(0));
    state.air = game::airCapacity(state);
    game::snapCamera(state);

    if (!campaign::accept(state, campaign::kFirst))
        return result;
    if (!station::undock(state))
        return result;

    std::vector<Point> path;
    if (!buildRoute(world, state, path))
        return result;
    result.routeBuilt = true;

    size_t cursor = 0;
    world::TileCache cache;
    world::clearCache(cache);
    while (result.ticks < kTimeoutTicks) {
        if (state.docked >= 0) {
            result.docked = state.docked == static_cast<int8_t>(campaign::kBenthos);
            break;
        }

        const Point target = carrot(path, cursor, state.x, state.y);
        const int16_t dx = static_cast<int16_t>(target.x - state.x);
        const int16_t dy = static_cast<int16_t>(target.y - state.y);
        const int16_t neutral = game::neutralFor(state.cargo);

        game::Input input{};
        if (dy < -kVerticalTolerancePx) {
            if (state.vyQ8 > -10 * game::kQ8One ||
                (state.vyQ8 > -16 * game::kQ8One && state.ballast > static_cast<uint8_t>(neutral + 10)))
                input.up = true;
        } else if (dy > kVerticalTolerancePx) {
            if (state.vyQ8 < kCruiseSpeedQ8)
                input.down = true;
        }
        // A relaxed pilot keeps a comfortable cruise below the hard-impact
        // threshold so a late turn scrapes instead of wrecking.
        if (dx > 2 && state.vxQ8 < kCruiseSpeedQ8)
            input.right = true;
        else if (dx < -2 && state.vxQ8 > -kCruiseSpeedQ8)
            input.left = true;

        result.arrivalAir = state.air;
#if SH_FEATURE_HULL_DAMAGE
        result.minimumHull = std::min(result.minimumHull, state.hull);
#endif
        station::stepFlight(state, input, world, &cache);
        ++result.ticks;
#if SH_FEATURE_HULL_DAMAGE
        if (state.docked < 0)
            result.minimumHull = std::min(result.minimumHull, state.hull);
#endif
    }

#if SH_FEATURE_HULL_DAMAGE
    if (state.docked == static_cast<int8_t>(campaign::kBenthos)) {
        result.docked = true;
        result.minimumHull = std::min(result.minimumHull, state.hull);
    }
#else
    if (state.docked == static_cast<int8_t>(campaign::kBenthos))
        result.docked = true;
    result.minimumHull = 1;
#endif
    result.delivered = campaign::missionCompleted(state, campaign::kFirst) && state.credits == campaign::missionReward(campaign::kFirst) && state.cargo == 0;
    result.serviceAir = state.air;
    result.serviceBallast = state.ballast;
    return result;
}

}   // namespace campaign_route_test

inline void CampaignRouteSuite(TestRunner &runner) {
    using namespace campaign_route_test;

    {
        Test test("FIRST delivery is reachable from Tidewall dock with relaxed-play air margin");
        const FlightResult result = flyFirstDelivery();
        test.assertTrue(result.routeBuilt, "generated collision plane opens a Tidewall-to-Benthos route");
        test.assertTrue(result.docked, "relaxed autopilot docks at Benthos");
        test.assertTrue(result.ticks <= kTimeoutTicks, "first delivery stays inside six relaxed minutes");
        test.assertTrue(result.arrivalAir >= kRequiredAirMargin, "arrival air keeps a 20% reserve");
        test.assertTrue(result.minimumHull > 0, "route does not wreck the hull");
        test.assertTrue(result.delivered, "docking resolves FIRST exactly once");
        test.assertEqual(result.serviceAir, game::kAirCapacity, "berth service refills the tank");
        test.assertEqual(result.serviceBallast, game::neutralFor(0), "berth service neutralises the empty hold");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_CAMPAIGN

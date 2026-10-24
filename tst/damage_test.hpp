#pragma once

#include "../src/damage.hpp"
#include "../src/damage_view.hpp"
#include "../src/game.hpp"
#include "../src/pilot.hpp"
#include "test.hpp"
#include "world_test.hpp"

#if SH_FEATURE_MINES
#include "../src/hazards.hpp"
#endif

#if SH_FEATURE_PARTICLES
#include "../src/particles.hpp"
#endif

// M7 damage and debris rules. The threshold and cooldown are exercised directly
// on game::State, then once through the real free-flight world step so the
// integration cannot drift from the rule.
namespace damage_test {

using Fixture = world_test::FakeWorld;

inline game::Input idle() {
    return game::Input{false, false, false, false};
}

inline void place(game::State &state, int16_t x, int16_t y) {
    state.x = x;
    state.y = y;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    game::snapCamera(state);
}

inline void fillTiles(Fixture &fake, int16_t tx0, int16_t ty0, int16_t tx1, int16_t ty1, bool solid) {
    for (int16_t ty = ty0; ty <= ty1; ++ty)
        for (int16_t tx = tx0; tx <= tx1; ++tx)
            fake.setTile(tx, ty, solid);
}

inline bool pixelSet(const uint8_t *buffer, int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= cave::VIEW_WIDTH || y >= cave::VIEW_HEIGHT)
        return false;
    return ((buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u) != 0;
}

}   // namespace damage_test

inline void DamageSuite(TestRunner &runner) {
    using namespace damage_test;

    {
        // Compile independence: when HULL_DAMAGE is off the field and rules are
        // absent, but the boat still resets and steps. The profile matrix builds
        // the other branch; these assertions keep this branch honest.
#if SH_FEATURE_HULL_DAMAGE
        game::State state;
        game::reset(state);
        Test test("hull damage clamps at zero and repeatedly applies");
        test.assertEqual(state.hull, game::kHullMax, "reset starts at full hull");
        damage::applyDamage(state, 2);
        test.assertEqual(state.hull, static_cast<uint8_t>(game::kHullMax - 2), "damage subtracts");
        damage::applyDamage(state, 40);
        test.assertEqual(state.hull, 0, "large damage clamps to zero");
        damage::applyDamage(state, 1);
        test.assertEqual(state.hull, 0, "zero hull is stable, no underflow");
        runner.add(test);
#else
        game::State state;
        game::reset(state);
        Test test("HULL_DAMAGE=0 keeps the boat steppable");
        game::Input input = idle();
        input.right = true;
        for (uint8_t i = 0; i < 120; ++i)
            game::step(state, input);
        test.assertTrue(state.x > game::kSpawnX, "movement is unaffected without hull state");
        runner.add(test);
#endif
    }

#if SH_FEATURE_HULL_DAMAGE
    {
        Test test("repair clamps at kHullMax, including out-of-range hull");
        game::State state;
        game::reset(state);
        state.hull = 1;
        damage::repair(state, 2);
        test.assertEqual(state.hull, 3, "partial repair adds");
        damage::repair(state, 20);
        test.assertEqual(state.hull, game::kHullMax, "repair caps at max");
        state.hull = static_cast<uint8_t>(game::kHullMax + 3);
        damage::repair(state, 1);
        test.assertEqual(state.hull, game::kHullMax, "out-of-range hull is clamped on repair");
        state.hull = 2;
        damage::repairFull(state);
        test.assertEqual(state.hull, game::kHullMax, "station service refills hull");
        runner.add(test);
    }

    {
        Test test("wall damage threshold is below/at/above 16 px/s");
        test.assertTrue(!damage::hardWallSpeed(damage::kWallDamageSpeedQ8 - 1), "below threshold is a scrape");
        test.assertTrue(damage::hardWallSpeed(damage::kWallDamageSpeedQ8), "at threshold damages");
        test.assertTrue(damage::hardWallSpeed(damage::kWallDamageSpeedQ8 + 1), "above threshold damages");

        game::State state;
        game::reset(state);
        state.impactAxis = 1;
        state.impactSpeedQ8 = static_cast<int16_t>(damage::kWallDamageSpeedQ8 - 1);
        test.assertTrue(!game::hardWallImpact(state), "integration helper rejects a scrape");
        state.impactSpeedQ8 = damage::kWallDamageSpeedQ8;
        test.assertTrue(game::hardWallImpact(state), "integration helper accepts the threshold");
        state.impactAxis = 2;
        test.assertTrue(game::hardWallImpact(state), "vertical wall impacts count");
        state.impactAxis = 3;
        test.assertTrue(!game::hardWallImpact(state), "mine axis is not a wall impact");
        runner.add(test);
    }

    {
        Test test("wall damage applies once then waits out the cooldown");
        game::State state;
        game::reset(state);
        state.impactAxis = 1;
        state.impactSpeedQ8 = damage::kWallDamageSpeedQ8;

        test.assertTrue(damage::resolveWallImpact(state), "first hard impact damages");
        test.assertEqual(state.hull, static_cast<uint8_t>(game::kHullMax - 1), "one point");
        test.assertEqual(state.damageCooldown, damage::kWallDamageCooldownTicks, "cooldown starts");

        test.assertTrue(!damage::resolveWallImpact(state), "second hit inside the cooldown is ignored");
        test.assertEqual(state.hull, static_cast<uint8_t>(game::kHullMax - 1), "hull unchanged by the cooldown");

        for (uint8_t i = 0; i + 1 < damage::kWallDamageCooldownTicks; ++i)
            damage::tick(state);
        test.assertEqual(state.damageCooldown, 1, "one tick short of ready");
        test.assertTrue(!damage::resolveWallImpact(state), "still blocked with one tick left");
        damage::tick(state);
        test.assertTrue(damage::resolveWallImpact(state), "ready after the full cooldown");
        test.assertEqual(state.hull, static_cast<uint8_t>(game::kHullMax - 2), "second point after cooldown");
        runner.add(test);
    }

    {
        Test test("zero hull stays controllable and cannot underflow");
        Fixture fake;
        game::State state;
        game::resetWorld(state);
        damage::applyDamage(state, 250);
        test.assertEqual(state.hull, 0, "hull floors at zero");

        game::Input input = idle();
        input.right = true;
        const int16_t startX = state.x;
        for (uint8_t i = 0; i < 30; ++i)
            game::stepWorld(state, input, fake);
        test.assertEqual(state.hull, 0, "stepping a wreck does not underflow");
        test.assertTrue(state.x > startX, "the boat remains controllable at zero hull");
        runner.add(test);
    }

    {
        Test test("zero hull is a free-flight failure and restart recovers");
        game::State state;
        game::resetWorld(state);
        state.hull = 1;
        damage::applyDamage(state, 1);
        test.assertTrue(damage::lost(state), "zero hull is lost");
        test.assertTrue(damage::lostInFlight(state), "an undocked zero-hull boat fails");
#if SH_FEATURE_STATIONS
        state.docked = 0;
        test.assertTrue(!damage::lostInFlight(state), "a docked boat is serviced before failure");
        state.docked = -1;
#endif
#if SH_FEATURE_JOBS
        state.activeJob.to = 0;
        state.credits = 500;
        state.lastDelivery = 2;
#endif
        state.cargo = 3;
        game::resetWorld(state); // the failure screen's restart path
        test.assertTrue(!damage::lost(state), "restart restores a live hull");
        test.assertEqual(state.hull, game::hullCapacity(state), "restart fills the hull");
        test.assertEqual(state.air, game::airCapacity(state), "restart fills the air");
        test.assertEqual(state.cargo, 0, "restart clears the hold");
#if SH_FEATURE_JOBS
        test.assertTrue(!game::hasActiveJob(state), "restart clears the contract");
        test.assertEqual(state.credits, 0, "restart clears credits");
        test.assertEqual(state.lastDelivery, 0, "restart clears the last delivery");
#endif
        runner.add(test);
    }

    {
        Test test("free-flight world step applies wall damage and latches feedback");
        Fixture fake;
        fillTiles(fake, 10, 0, 10, 159, true);   // solid face at x=80
        game::State state;
        game::resetWorld(state);
        place(state, 67, 80);
        state.xFrac = game::kSubpixelOne - 1;
        state.vxQ8 = game::topSpeedQ8For(0);

        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.impactAxis, 1, "wall impact feedback is still recorded");
        test.assertTrue(state.impactSpeedQ8 >= game::kHardImpactSpeedQ8, "hard speed captured");
        test.assertEqual(state.hull, static_cast<uint8_t>(game::kHullMax - 1), "hard wall hit costs one point");
        test.assertEqual(state.damageCooldown, damage::kWallDamageCooldownTicks, "world step starts the cooldown");
        runner.add(test);
    }

    {
        Test test("a slow wall contact does not damage the hull");
        Fixture fake;
        fillTiles(fake, 10, 0, 10, 159, true);
        game::State state;
        game::resetWorld(state);
        place(state, 67, 80);
        state.xFrac = game::kSubpixelOne - 1;
        // Compensate the stepPhysics drag so the blocked-axis speed lands one
        // Q8 unit below the threshold.
        state.vxQ8 = static_cast<int16_t>(damage::kWallDamageSpeedQ8 - 1);
        state.vxQ8 = static_cast<int16_t>(state.vxQ8 + game::dragStep(state.vxQ8));

        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.hull, game::kHullMax, "scrape leaves the hull intact");
        test.assertEqual(state.damageCooldown, 0, "scrape does not arm the cooldown");
        runner.add(test);
    }

#if SH_FEATURE_STATIONS
    {
        Test test("docked steps do not tick damage or debris");
        game::State state;
        game::resetWorld(state);
        state.docked = 0;
#if SH_FEATURE_PARTICLES
        particles::spawn(state);
        const uint8_t lifeBefore = state.particles.particle[0].life;
#endif
        state.damageCooldown = 10;

        Fixture fake;
        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.damageCooldown, 10, "cooldown does not tick while docked");
#if SH_FEATURE_PARTICLES
        test.assertEqual(state.particles.particle[0].life, lifeBefore, "particle lifetime does not tick while docked");
#endif
        runner.add(test);
    }
#endif

#if SH_FEATURE_MINES
    {
        Test test("mine damage stays one point and removes one mine");
        Fixture fake;
        game::State state;
        game::resetWorld(state);
        world::Mine mine;
        test.assertTrue(world::mineInfo(0, mine), "generated mine 0 exists");
        place(state, mine.x, mine.y);

        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.hull, static_cast<uint8_t>(game::kHullMax - 1), "mine costs exactly one point");
        test.assertTrue(!hazards::alive(state, 0), "the mine is consumed");
        test.assertEqual(state.impactAxis, hazards::kMineImpactAxis, "mine feedback axis");
        runner.add(test);
    }
#endif
#endif   // SH_FEATURE_HULL_DAMAGE

#if SH_FEATURE_HULL_DAMAGE
    {
        Test test("damage flash brackets appear only during the flash window");
        Fixture fake;
        game::State state;
        game::resetWorld(state);
        place(state, 100, 100);
        state.impactAxis = 1;
        state.impactSpeedQ8 = damage::kWallDamageSpeedQ8;
        damage::resolveWallImpact(state);
        test.assertTrue(damage::flashing(state), "flash starts on the damage tick");

        const cave::Camera camera = pilot_view::cameraForState(state);
        const int16_t left = static_cast<int16_t>(pilot_view::screenX(state, camera) - game::kHullHalfW);
        const int16_t top = static_cast<int16_t>(pilot_view::screenY(state, camera) - game::kHullHalfH);

        uint8_t buffer[cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8 + 8];
        for (uint16_t i = 0; i < sizeof(buffer); ++i)
            buffer[i] = 0;
        damage_view::drawFlash(buffer, state, camera);
        test.assertTrue(pixelSet(buffer, static_cast<int16_t>(left - 1), static_cast<int16_t>(top - 1)), "top-left bracket");
        test.assertTrue(pixelSet(buffer, left, static_cast<int16_t>(top - 1)), "top edge bracket");

        for (uint8_t i = 0; i < damage::kWallDamageCooldownTicks; ++i)
            damage::tick(state);
        test.assertTrue(!damage::flashing(state), "flash ends with the cooldown");

        for (uint16_t i = 0; i < sizeof(buffer); ++i)
            buffer[i] = 0;
        damage_view::drawFlash(buffer, state, camera);
        test.assertTrue(!pixelSet(buffer, static_cast<int16_t>(left - 1), static_cast<int16_t>(top - 1)), "no bracket after the flash");
        runner.add(test);
    }
#endif
}

#pragma once

#include "../src/cave.hpp"
#include "../src/game.hpp"
#include "../src/particles.hpp"
#include "../src/particles_view.hpp"
#include "../src/pilot.hpp"
#include "../src/pilot_view.hpp"
#include "test.hpp"
#include "world_test.hpp"


// Particle pool tests: fixed size, deterministic spawn/update, bounded offsets
// and lifetime, clipped pilot-view drawing, and the real hard-wall spawn
// trigger. The pool is pure state; these tests never allocate.
namespace particles_test {

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

inline bool pixelSet(const uint8_t *buffer, int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= cave::VIEW_WIDTH || y >= cave::VIEW_HEIGHT)
        return false;
    return ((buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u) != 0;
}

inline void clearBuffer(uint8_t *buffer) {
    for (uint16_t i = 0; i < static_cast<uint16_t>(cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8); ++i)
        buffer[i] = 0;
}

}   // namespace particles_test

inline void ParticlesSuite(TestRunner &runner) {
    using namespace particles_test;

    static_assert(game::kParticleCount == 4, "the pool is four fixed slots");
    static_assert(sizeof(game::ParticlePool) == 16, "pool state stays at 16 bytes");

    {
        Test test("spawn fills all four slots at the hull anchor");
        game::State state;
        game::reset(state);
        place(state, 1000, 500);
        particles::spawn(state);

        test.assertEqual(state.particles.x, static_cast<int16_t>(1000), "anchor x");
        test.assertEqual(state.particles.y, static_cast<int16_t>(500), "anchor y");
        for (uint8_t i = 0; i < game::kParticleCount; ++i) {
            const game::Particle &particle = state.particles.particle[i];
            test.assertTrue(particle.life > 0, "slot starts alive");
            test.assertTrue(particle.life <= particles::kMaxLife, "lifetime is bounded");
            test.assertTrue(particle.dx >= -particles::kMaxOffset && particle.dx <= particles::kMaxOffset, "spawn x bounded");
            test.assertTrue(particle.dy >= -particles::kMaxOffset && particle.dy <= particles::kMaxOffset, "spawn y bounded");
        }
        runner.add(test);
    }

    {
        Test test("update is deterministic and identical for identical spawns");
        game::State a;
        game::State b;
        game::reset(a);
        game::reset(b);
        place(a, 700, 300);
        place(b, 700, 300);
        particles::spawn(a);
        particles::spawn(b);

        for (uint8_t tick = 0; tick < 5; ++tick) {
            particles::tick(a);
            particles::tick(b);
        }

        test.assertEqual(a.particles.x, b.particles.x, "anchor x matches");
        test.assertEqual(a.particles.y, b.particles.y, "anchor y matches");
        for (uint8_t i = 0; i < game::kParticleCount; ++i) {
            test.assertEqual(a.particles.particle[i].dx, b.particles.particle[i].dx, "dx matches");
            test.assertEqual(a.particles.particle[i].dy, b.particles.particle[i].dy, "dy matches");
            test.assertEqual(a.particles.particle[i].life, b.particles.particle[i].life, "life matches");
        }
        runner.add(test);
    }

    {
        Test test("lifetime expires and offsets stay inside the bound");
        game::State state;
        game::reset(state);
        place(state, 200, 200);
        particles::spawn(state);
        test.assertTrue(particles::anyAlive(state), "fresh pool is alive");

        for (uint16_t tick = 0; tick < 64; ++tick) {
            particles::tick(state);
            for (uint8_t i = 0; i < game::kParticleCount; ++i) {
                const game::Particle &particle = state.particles.particle[i];
                test.assertTrue(particle.dx >= -particles::kMaxOffset && particle.dx <= particles::kMaxOffset, "dx bounded");
                test.assertTrue(particle.dy >= -particles::kMaxOffset && particle.dy <= particles::kMaxOffset, "dy bounded");
                test.assertTrue(particle.life <= particles::kMaxLife, "life bounded");
            }
        }
        test.assertTrue(!particles::anyAlive(state), "all slots expire");
        const game::Particle before = state.particles.particle[0];
        particles::tick(state);
        test.assertEqual(state.particles.particle[0].life, before.life, "dead slots stay dead");
        runner.add(test);
    }

    {
        Test test("a second spawn replaces the pool without growth");
        game::State state;
        game::reset(state);
        place(state, 100, 100);
        particles::spawn(state);
        for (uint8_t tick = 0; tick < 5; ++tick)
            particles::tick(state);

        place(state, 900, 800);
        particles::spawn(state);
        test.assertEqual(state.particles.x, static_cast<int16_t>(900), "anchor moves to the new hit");
        test.assertEqual(state.particles.y, static_cast<int16_t>(800), "anchor y moves");
        for (uint8_t i = 0; i < game::kParticleCount; ++i)
            test.assertTrue(state.particles.particle[i].life > 0, "every slot restarts");
        runner.add(test);
    }

    {
        Test test("hard wall step spawns debris at the hull");
        Fixture fake;
        for (int16_t ty = 0; ty < world::TILES_Y; ++ty)
            fake.setTile(10, ty, true);

        game::State state;
        game::resetWorld(state);
        place(state, 67, 80);
        state.xFrac = static_cast<int16_t>(game::kSubpixelOne - 1);
        state.vxQ8 = game::topSpeedQ8For(0);

        game::stepWorld(state, idle(), fake);
        test.assertTrue(particles::anyAlive(state), "hard impact spawns the pool");
        test.assertEqual(state.particles.x, state.x, "anchor is the post-step hull x");
        test.assertEqual(state.particles.y, state.y, "anchor is the post-step hull y");
        runner.add(test);
    }

    {
        Test test("a slow wall scrape does not spawn debris");
        Fixture fake;
        for (int16_t ty = 0; ty < world::TILES_Y; ++ty)
            fake.setTile(10, ty, true);

        game::State state;
        game::resetWorld(state);
        place(state, 67, 80);
        state.vxQ8 = static_cast<int16_t>(game::kHardImpactSpeedQ8 - 1);
        state.xFrac = static_cast<int16_t>(game::kSubpixelOne - 1);

        game::stepWorld(state, idle(), fake);
        test.assertTrue(!particles::anyAlive(state), "below-threshold contact emits nothing");
        runner.add(test);
    }

    static_assert(game::kHardFloorAirCost == 5, "floor slam cost is fixed at five air units");

    {
        Test test("hard descending floor impact spends exactly five air");
        Fixture fake;
        for (int16_t tx = 0; tx < world::TILES_X; ++tx)
            fake.setTile(tx, 12, true);

        game::State state;
        game::resetWorld(state);
        place(state, 104, 92);
        state.air = game::airCapacity(state);
        state.vyQ8 = game::kMaxVyQ8;

        int16_t airBefore = state.air;
        bool hit = false;
        for (uint8_t tick = 0; tick < 40 && !hit; ++tick) {
            airBefore = state.air;
            game::stepWorld(state, idle(), fake);
            hit = state.impactAxis == 2 && game::hardWallImpact(state);
        }
        test.assertTrue(hit, "hard descending floor impact recorded");
        test.assertEqual(state.air, static_cast<int16_t>(airBefore - game::kHardFloorAirCost), "exactly five units spent once");
        runner.add(test);
    }

    {
        Test test("soft floor contact spends no air");
        Fixture fake;
        for (int16_t tx = 0; tx < world::TILES_X; ++tx)
            fake.setTile(tx, 12, true);

        game::State state;
        game::resetWorld(state);
        place(state, 104, 92);
        state.air = game::airCapacity(state);
        state.vyQ8 = static_cast<int16_t>(game::kHardImpactSpeedQ8 - 1);

        const int16_t airBefore = state.air;
        bool hit = false;
        for (uint8_t tick = 0; tick < 40 && !hit; ++tick) {
            game::stepWorld(state, idle(), fake);
            hit = state.impactAxis == 2;
        }
        test.assertTrue(hit, "soft floor contact recorded");
        test.assertTrue(!game::hardWallImpact(state), "contact stays below the hard threshold");
        test.assertEqual(state.air, airBefore, "soft scrape is free");
        runner.add(test);
    }

    {
        Test test("hard ceiling impact spends no air");
        Fixture fake;
        for (int16_t tx = 0; tx < world::TILES_X; ++tx)
            fake.setTile(tx, 0, true);

        game::State state;
        game::resetWorld(state);
        place(state, 104, 12);
        state.air = game::airCapacity(state);
        state.vyQ8 = static_cast<int16_t>(-game::kMaxVyQ8);

        const int16_t airBefore = state.air;
        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.impactAxis, 2, "ceiling contact is a vertical impact");
        test.assertTrue(game::hardWallImpact(state), "ceiling speed clears the hard threshold");
        test.assertEqual(state.air, airBefore, "ceiling hit is free");
        runner.add(test);
    }

    {
        Test test("hard side impact spends no air");
        Fixture fake;
        for (int16_t ty = 0; ty < world::TILES_Y; ++ty)
            fake.setTile(14, ty, true);

        game::State state;
        game::resetWorld(state);
        place(state, 99, 80);
        state.air = game::airCapacity(state);
        state.vxQ8 = game::kMaxVxBaseQ8;
        state.xFrac = static_cast<int16_t>(game::kSubpixelOne - 1);

        const int16_t airBefore = state.air;
        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.impactAxis, 1, "side contact is a horizontal impact");
        test.assertTrue(game::hardWallImpact(state), "side speed clears the hard threshold");
        test.assertEqual(state.air, airBefore, "side hit is free");
        runner.add(test);
    }

    {
        Test test("blocked gate contact spends no air");
        Fixture fake;   // empty terrain: the closed gate alone blocks
        const auto gate = exploration_data::gate(0);

        game::State state;
        game::resetWorld(state);
        place(state, static_cast<int16_t>(gate.x + gate.w / 2), static_cast<int16_t>(gate.y - game::kHullHalfH - 1));
        state.tools = 0;
        state.openGates = 0;
        state.air = game::airCapacity(state);
        state.vyQ8 = game::kMaxVyQ8;

        const int16_t airBefore = state.air;
        bool hit = false;
        for (uint8_t tick = 0; tick < 40 && !hit; ++tick) {
            game::stepWorld(state, idle(), fake);
            hit = state.impactAxis == 4;
        }
        test.assertTrue(hit, "closed gate records its dedicated impact axis");
        test.assertEqual(state.air, airBefore, "gate block is free");
        runner.add(test);
    }

    {
        Test test("hard floor impact floors air at zero");
        Fixture fake;
        for (int16_t tx = 0; tx < world::TILES_X; ++tx)
            fake.setTile(tx, 12, true);

        game::State state;
        game::resetWorld(state);
        place(state, 104, 92);
        state.air = 3;
        state.vyQ8 = game::kMaxVyQ8;

        bool hit = false;
        for (uint8_t tick = 0; tick < 40 && !hit; ++tick) {
            game::stepWorld(state, idle(), fake);
            hit = state.impactAxis == 2 && game::hardWallImpact(state);
        }
        test.assertTrue(hit, "hard descending floor impact recorded");
        test.assertEqual(state.air, 0, "tank never goes negative");
        runner.add(test);
    }

    {
        Test test("pilot-view particles are clipped to the visible buffer");
        game::State state;
        game::reset(state);
        place(state, 13, 4);
        particles::spawn(state);
        const cave::Camera camera = pilot_view::cameraForState(state);

        constexpr uint16_t kGuard = 4;
        constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;
        uint8_t storage[kGuard + kBufferBytes + kGuard];
        for (uint16_t i = 0; i < sizeof(storage); ++i)
            storage[i] = 0x5A;
        uint8_t *buffer = storage + kGuard;
        clearBuffer(buffer);

        particles_view::draw(buffer, state, camera);
        test.assertTrue(pixelSet(buffer, 0, 0), "corner slot is visible");
        for (uint16_t i = 0; i < kGuard; ++i) {
            test.assertEqual(storage[i], 0x5A, "prefix guard untouched");
            test.assertEqual(storage[kGuard + kBufferBytes + i], 0x5A, "suffix guard untouched");
        }

        clearBuffer(buffer);
        for (uint8_t tick = 0; tick < particles::kMaxLife; ++tick)
            particles::tick(state);
        particles_view::draw(buffer, state, camera);
        bool drawn = false;
        for (uint16_t i = 0; i < kBufferBytes; ++i)
            drawn = drawn || buffer[i] != 0;
        test.assertTrue(!drawn, "expired particles draw nothing");
        runner.add(test);
    }
}


#pragma once

#include "../src/game.hpp"
#include "../src/pause.hpp"
#include "test.hpp"

// Pause contract: A+B enters, either A or B resumes, and a paused frame never
// reaches the world step. The sketch owns the Screen byte; this suite uses the
// same pure resolve() rule the sketch calls.
inline void PauseSuite(TestRunner &runner) {
    {
        Test test("pause: A+B enters, A or B resumes, paused frames do not tick");
        test.assertEqual(pause::resolve(false, false, false), pause::kNone, "idle flight is not a pause");
        test.assertEqual(pause::resolve(false, true, false), pause::kNone, "A alone is not a pause");
        test.assertEqual(pause::resolve(false, false, true), pause::kNone, "B alone is not a pause");
        test.assertEqual(pause::resolve(false, true, true), pause::kEnter, "A+B enters pause");
        test.assertEqual(pause::resolve(true, false, false), pause::kNone, "paused with no key stays paused");
        test.assertEqual(pause::resolve(true, true, false), pause::kLeave, "A resumes");
        test.assertEqual(pause::resolve(true, false, true), pause::kLeave, "B resumes");

        game::State state;
        game::reset(state);
        const uint16_t started = state.ticks;
        bool paused = false;

        // Mirrors updateFlight/updatePause: enter and resume frames do not step;
        // only a normal unpaused flight frame advances the world.
        const auto frame = [&](bool a, bool b) {
            const pause::Action action = pause::resolve(paused, a, b);
            if (action == pause::kEnter) {
                paused = true;
                return;
            }
            if (action == pause::kLeave) {
                paused = false;
                return;
            }
            if (!paused)
                game::step(state, game::Input{false, false, false, false});
        };

        frame(true, true);
        test.assertTrue(paused, "A+B enters the paused state");
        test.assertEqual(state.ticks, started, "the pause frame does not tick");

        for (uint8_t i = 0; i < 30; ++i)
            frame(false, false);
        test.assertEqual(state.ticks, started, "a paused stretch does not tick");
        test.assertTrue(paused, "no key keeps the pause open");

        frame(true, false);
        test.assertTrue(!paused, "A resumes");
        test.assertEqual(state.ticks, started, "the resume frame does not tick");

        frame(false, false);
        test.assertEqual(state.ticks, static_cast<uint16_t>(started + 1), "the next flight frame ticks");
        runner.add(test);
    }
}

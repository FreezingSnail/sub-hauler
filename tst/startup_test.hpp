#pragma once

#include "../src/startup_logic.hpp"
#include "test.hpp"

// The game splash must wait indefinitely, ignore a button held from the logo,
// and dismiss only on a fresh press whose release the caller consumes.
inline void StartupSuite(TestRunner &runner) {
    {
        Test test("splash waits indefinitely without input");
        startup_logic::Splash splash;
        startup_logic::reset(splash);
        for (uint16_t i = 0; i < 2000; ++i)
            test.assertTrue(!startup_logic::tick(splash, false), "no input never dismisses");
        test.assertTrue(!startup_logic::done(splash), "splash is still waiting");
        runner.add(test);
    }
    {
        Test test("held logo input cannot skip the game splash until released");
        startup_logic::Splash splash;
        startup_logic::reset(splash);
        for (uint16_t i = 0; i < 200; ++i)
            test.assertTrue(!startup_logic::tick(splash, true), "held input is ignored");
        test.assertTrue(!startup_logic::done(splash), "held input does not dismiss");
        test.assertTrue(!startup_logic::tick(splash, false), "release arms the splash");
        test.assertTrue(!startup_logic::done(splash), "release alone does not dismiss");
        test.assertTrue(startup_logic::tick(splash, true), "fresh press dismisses");
        test.assertTrue(startup_logic::done(splash), "splash is done");
        test.assertTrue(!startup_logic::tick(splash, true), "the dismiss edge fires once");
        runner.add(test);
    }
}

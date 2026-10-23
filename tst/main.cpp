// Host test entry point. `make test` compiles this with the native compiler.
#include "cargo_test.hpp"
#include "cave_test.hpp"
#include "collision_test.hpp"
#include "features.hpp"
#include "game_test.hpp"
#include "pilot_view_test.hpp"
#include "test.hpp"
#include "world_test.hpp"

#if SH_FEATURE_FOG_CHART
#include "chart_test.hpp"
#include "chart_view_test.hpp"
#endif

#if SH_FEATURE_CARGO_LOAD
#include "cargo_screen_test.hpp"
#include "cargo_view_test.hpp"
#endif

#if SH_FEATURE_STATIONS
#include "station_test.hpp"
#endif
#if SH_FEATURE_JOBS
#include "jobs_test.hpp"
#endif
#if SH_FEATURE_STATIONS && SH_FEATURE_JOBS
#include "loop_test.hpp"
#endif

int main() {
    TestRunner runner;
    CargoSuite(runner);
#if SH_FEATURE_CARGO_LOAD
    CargoScreenSuite(runner);
    CargoViewSuite(runner);
#endif
    CaveSuite(runner);
    CollisionSuite(runner);
    GameSuite(runner);
    PilotViewSuite(runner);
    WorldSuite(runner);
#if SH_FEATURE_FOG_CHART
    ChartSuite(runner);
    ChartViewSuite(runner);
#endif
#if SH_FEATURE_STATIONS
    StationSuite(runner);
#endif
#if SH_FEATURE_JOBS
    JobsSuite(runner);
#endif
#if SH_FEATURE_STATIONS && SH_FEATURE_JOBS
    LoopSuite(runner);
#endif
    return runner.printSummary();
}

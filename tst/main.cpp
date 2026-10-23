// Host test entry point. `make test` compiles this with the native compiler.
#include "game_test.hpp"
#include "test.hpp"
#include "world_test.hpp"

int main() {
    TestRunner runner;
    GameSuite(runner);
    WorldSuite(runner);
    return runner.printSummary();
}

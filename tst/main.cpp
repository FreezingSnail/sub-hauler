// Host test entry point. `make test` compiles this with the native compiler.
#include "cargo_test.hpp"
#include "game_test.hpp"
#include "test.hpp"
#include "world_test.hpp"

int main() {
    TestRunner runner;
    CargoSuite(runner);
    GameSuite(runner);
    WorldSuite(runner);
    return runner.printSummary();
}

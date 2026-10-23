// Host test entry point. `make test` compiles this with the native compiler.
#include "game_test.hpp"
#include "test.hpp"

int main() {
    TestRunner runner;
    GameSuite(runner);
    return runner.printSummary();
}

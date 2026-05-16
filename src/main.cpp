// Stub entry point.
//
// M0.5 wires the RNG service in: build a GameState, advance time,
// take a few draws, and print a sanity-check histogram. No higher-
// level simulation systems run yet.

#include <array>
#include <cstdlib>
#include <iostream>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/random_service.hpp"
#include "leviathan/systems/time_system.hpp"

namespace {

constexpr const char* kProjectName    = "Project Leviathan";
constexpr const char* kProjectVersion = "0.1.0";

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
    using namespace leviathan::core;
    namespace lt = leviathan::systems::time;
    namespace lr = leviathan::systems::random;

    std::cout << kProjectName << " " << kProjectVersion << "\n"
              << "Milestone 0.5 - RNG service.\n"
              << "No high-level simulation logic is wired up yet.\n";

    SimulationConfig config;
    config.seed = 19300101u;
    GameState state = make_game_state(config);

    lt::advance_days(state, 30);

    std::cout << "Seed              : " << state.rng.seed     << "\n"
              << "Counter (post-tick): " << state.rng.counter << "\n";

    // A few representative draws so the demo output shows what each
    // helper returns. Tags are passed to exercise the trace API even
    // though we have no trace sink installed.
    const int    d_int    = lr::draw_int(state.rng, 0, 99,  "demo.int");
    const double d_unit   = lr::draw_unit(state.rng,        "demo.unit");
    const double d_double = lr::draw_double(state.rng, -1.0, 1.0, "demo.double");
    const bool   d_bool   = lr::draw_bool(state.rng, 0.5,   "demo.bool");

    std::cout << "draw_int [0,99]   : " << d_int               << "\n"
              << "draw_unit         : " << d_unit              << "\n"
              << "draw_double [-1,1): " << d_double            << "\n"
              << "draw_bool p=0.5   : " << (d_bool ? "true" : "false") << "\n";

    // 6000 draws of `draw_int(0, 5)` -> histogram. Expected ~1000 per
    // bucket; visible drift > ~5% would indicate a regression.
    std::array<int, 6> hist{};
    for (int i = 0; i < 6000; ++i) {
        ++hist[static_cast<std::size_t>(lr::draw_int(state.rng, 0, 5, "demo.hist"))];
    }
    std::cout << "draw_int [0,5] x 6000 histogram:\n";
    for (std::size_t i = 0; i < hist.size(); ++i) {
        std::cout << "  bucket " << i << ": " << hist[i] << "\n";
    }
    std::cout << "Counter (post-demo): " << state.rng.counter << "\n";

    return EXIT_SUCCESS;
}

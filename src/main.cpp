// Stub entry point.
//
// M0.3 demos the new GameState container: build a SimulationConfig,
// hand it to make_game_state(), and print a short summary. No
// simulation systems run yet - TimeSystem lands in M0.4, RNG in M0.5,
// LoggingSystem in M0.6, the real CLI runner in M0.9.

#include <cstdlib>
#include <iostream>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/simulation_config.hpp"

namespace {

constexpr const char* kProjectName    = "Project Leviathan";
constexpr const char* kProjectVersion = "0.1.0";

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
    using namespace leviathan::core;

    std::cout << kProjectName << " " << kProjectVersion << "\n"
              << "Milestone 0.3 - GameState container only.\n"
              << "No simulation logic is wired up yet.\n";

    SimulationConfig config;
    config.seed = 19300101u;

    GameState state = make_game_state(config);

    std::cout << "Start date       : " << state.current_date.to_string() << "\n"
              << "RNG seed         : " << state.rng.seed << "\n"
              << "RNG counter      : " << state.rng.counter << "\n"
              << "Countries        : " << state.countries.size() << "\n"
              << "Provinces        : " << state.provinces.size() << "\n"
              << "Factions         : " << state.factions.size() << "\n"
              << "Policies         : " << state.policies.size() << "\n"
              << "Events           : " << state.events.size() << "\n"
              << "Logs             : " << state.logs.size() << "\n";

    return EXIT_SUCCESS;
}

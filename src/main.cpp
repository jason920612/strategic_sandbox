// Stub entry point.
//
// M0.4 wires TimeSystem in: build a GameState, advance it by 365 days,
// and report the start/end dates plus how many month and year
// boundaries were crossed. No other simulation logic runs yet.

#include <cstdlib>
#include <iostream>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/time_system.hpp"

namespace {

constexpr const char* kProjectName    = "Project Leviathan";
constexpr const char* kProjectVersion = "0.1.0";

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
    using namespace leviathan::core;
    namespace lt = leviathan::systems::time;

    std::cout << kProjectName << " " << kProjectVersion << "\n"
              << "Milestone 0.4 - TimeSystem.\n"
              << "No simulation logic beyond date advance is wired up yet.\n";

    SimulationConfig config;
    config.seed = 19300101u;
    GameState state = make_game_state(config);

    const GameDate start = state.current_date;
    int month_changes = 0;
    int year_changes  = 0;
    for (int day = 0; day < 365; ++day) {
        const lt::TickResult r = lt::advance_one_day(state);
        if (r.month_changed) ++month_changes;
        if (r.year_changed)  ++year_changes;
    }

    std::cout << "Start date       : " << start.to_string() << "\n"
              << "End date         : " << state.current_date.to_string() << "\n"
              << "Days advanced    : 365\n"
              << "Month boundaries : " << month_changes << "\n"
              << "Year boundaries  : " << year_changes  << "\n";

    return EXIT_SUCCESS;
}

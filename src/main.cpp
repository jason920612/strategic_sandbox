// Stub entry point.
//
// M0.6 demos LoggingSystem: build a GameState, advance 10 days
// logging each tick, then print the JSONL export and a recent(3)
// snapshot. No higher-level simulation logic runs yet.

#include <cstdlib>
#include <iostream>
#include <sstream>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/logging_system.hpp"
#include "leviathan/systems/time_system.hpp"

namespace {

constexpr const char* kProjectName    = "Project Leviathan";
constexpr const char* kProjectVersion = "0.1.0";

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
    using namespace leviathan::core;
    namespace lt = leviathan::systems::time;
    namespace lg = leviathan::systems::logging;

    std::cout << kProjectName << " " << kProjectVersion << "\n"
              << "Milestone 0.6 - LoggingSystem.\n"
              << "Running a 10-day demo with explicit per-tick logs.\n\n";

    SimulationConfig config;
    config.start_date = GameDate(1930, 1, 28);  // hit a month boundary
    config.seed       = 19300101u;
    GameState state = make_game_state(config);

    // Pre-loop marker.
    lg::log_info(state, "lifecycle", "main", "simulation start");

    for (int day = 0; day < 10; ++day) {
        LogMetadata md;
        md.emplace_back("day_index", std::to_string(day));
        lg::log_debug(state, "time", "TimeSystem",
                      "About to advance one day", md);

        const auto r = lt::advance_one_day(state);
        if (r.month_changed) {
            lg::log_info(state, "time", "TimeSystem",
                         "Month rolled over");
        }
        if (r.year_changed) {
            lg::log_info(state, "time", "TimeSystem",
                         "Year rolled over");
        }
    }

    lg::log_info(state, "lifecycle", "main", "simulation end");

    std::cout << "--- JSONL log export ---\n";
    lg::export_jsonl(std::cout, state);

    std::cout << "\n--- recent(3) ---\n";
    for (const auto& e : lg::recent(state, 3)) {
        std::cout << e.date.to_string()
                  << "  [" << lg::severity_to_string(e.severity) << "] "
                  << e.source << "  " << e.message << "\n";
    }

    std::cout << "\nTotal log entries: " << state.logs.size() << "\n";

    return EXIT_SUCCESS;
}

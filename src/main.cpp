// Stub entry point.
//
// M0.7 demos the JSON data loader: parse simulation.json + a country
// JSON, build a GameState via make_game_state, tick a few days, and
// dump the JSONL log. Failed reads are routed through LoggingSystem
// by the caller - DataLoader itself never touches the log.

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/data_loader.hpp"
#include "leviathan/systems/logging_system.hpp"
#include "leviathan/systems/time_system.hpp"

namespace {

constexpr const char* kProjectName    = "Project Leviathan";
constexpr const char* kProjectVersion = "0.1.0";

// Resolve a path relative to the project root or fall back to CWD.
// The headless runner in M0.9 will replace this with a real --config
// argument. For now we accept an optional argv[1] override.
std::filesystem::path config_path(int argc, char** argv) {
    if (argc > 1) return argv[1];
    return std::filesystem::path("data") / "config" / "simulation.json";
}

std::filesystem::path country_path() {
    return std::filesystem::path("data") / "countries" / "germany.json";
}

}  // namespace

int main(int argc, char** argv) {
    using namespace leviathan::core;
    namespace lt  = leviathan::systems::time;
    namespace lg  = leviathan::systems::logging;
    namespace dl  = leviathan::systems::data_loader;

    std::cout << kProjectName << " " << kProjectVersion << "\n"
              << "Milestone 0.7 - JSON data loader.\n";

    // ---- Load the simulation config -------------------------------------
    const auto cfg_path = config_path(argc, argv);
    auto cfg_result = dl::load_simulation_config(cfg_path);

    SimulationConfig cfg;  // falls back to struct defaults on failure
    if (cfg_result.ok()) {
        cfg = std::move(cfg_result).value();
        std::cout << "Loaded config from " << cfg_path.string() << "\n";
    } else {
        // The loader returned Result::failure; OUR job (not the loader's)
        // is to route that into the log. DataLoader and LoggingSystem
        // remain decoupled.
        std::cerr << "WARN: " << cfg_result.error() << "\n"
                  << "Falling back to default simulation config.\n";
    }

    GameState state = make_game_state(cfg);
    if (cfg_result.failed()) {
        lg::log_warn(state, "config", "main",
                     "Falling back to default config",
                     {{"loader_error", cfg_result.error()}});
    } else {
        lg::log_info(state, "config", "main",
                     "Simulation config loaded",
                     {{"path", cfg_path.string()}});
    }

    // ---- Load a country -------------------------------------------------
    const auto country_p = country_path();
    auto country_result = dl::load_country(country_p);
    if (country_result.ok()) {
        CountryState country = std::move(country_result).value();
        // The numeric id is assigned by us, the caller. DataLoader does
        // not pick one - that's not its job.
        country.id = CountryId{0};
        state.countries.push_back(std::move(country));

        const auto& c = state.countries.back();
        lg::log_info(state, "country", "main",
                     "Country loaded",
                     {{"id_code", c.id_code}, {"name", c.name}});
        std::cout << "Loaded country: " << c.id_code
                  << " (" << c.name << ", GDP "
                  << c.initial_gdp << ", stability "
                  << c.initial_stability << ")\n";
    } else {
        lg::log_error(state, "country", "main",
                      "Country load failed",
                      {{"loader_error", country_result.error()}});
        std::cerr << "ERROR: " << country_result.error() << "\n";
    }

    // ---- Tick a few days so the log carries a small story ---------------
    for (int i = 0; i < 5; ++i) {
        lt::advance_one_day(state);
    }
    lg::log_info(state, "time", "main",
                 "Advanced 5 days from start");

    std::cout << "\n--- JSONL log ---\n";
    lg::export_jsonl(std::cout, state);
    std::cout << "\nFinal current_date: " << state.current_date.to_string()
              << "\nLog entries       : " << state.logs.size() << "\n";

    return EXIT_SUCCESS;
}

// Stub entry point.
//
// M0.8 extends the M0.7 demo with save / load: after loading config
// and a country, ticking, and emitting logs, the state is written to
// out/save.json, then read back, and the round-trip is verified.

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/data_loader.hpp"
#include "leviathan/systems/logging_system.hpp"
#include "leviathan/systems/save_system.hpp"
#include "leviathan/systems/time_system.hpp"

namespace {

constexpr const char* kProjectName    = "Project Leviathan";
constexpr const char* kProjectVersion = "0.1.0";

std::filesystem::path config_path(int argc, char** argv) {
    if (argc > 1) return argv[1];
    return std::filesystem::path("data") / "config" / "simulation.json";
}

std::filesystem::path country_path() {
    return std::filesystem::path("data") / "countries" / "germany.json";
}

std::filesystem::path save_path(int argc, char** argv) {
    if (argc > 2) return argv[2];
    return std::filesystem::path("out") / "save.json";
}

}  // namespace

int main(int argc, char** argv) {
    using namespace leviathan::core;
    namespace lt  = leviathan::systems::time;
    namespace lg  = leviathan::systems::logging;
    namespace dl  = leviathan::systems::data_loader;
    namespace ss  = leviathan::systems::save_system;

    std::cout << kProjectName << " " << kProjectVersion << "\n"
              << "Milestone 0.8 - save / load.\n";

    // ---- Load config ----------------------------------------------------
    const auto cfg_p = config_path(argc, argv);
    auto cfg_result  = dl::load_simulation_config(cfg_p);
    SimulationConfig cfg;
    if (cfg_result.ok()) {
        cfg = std::move(cfg_result).value();
        std::cout << "Loaded config from " << cfg_p.string() << "\n";
    } else {
        std::cerr << "WARN: " << cfg_result.error() << "\n";
    }
    GameState state = make_game_state(cfg);

    // ---- Load a country -------------------------------------------------
    auto country_result = dl::load_country(country_path());
    if (country_result.ok()) {
        CountryState c = std::move(country_result).value();
        c.id = CountryId{0};
        state.countries.push_back(std::move(c));
        std::cout << "Loaded country: " << state.countries.front().id_code << "\n";
    } else {
        std::cerr << "WARN: " << country_result.error() << "\n";
    }

    // ---- Tick a few days and log ----------------------------------------
    lg::log_info(state, "lifecycle", "main", "simulation start");
    for (int i = 0; i < 5; ++i) {
        lt::advance_one_day(state);
    }
    lg::log_info(state, "time", "main", "Advanced 5 days from start");

    // ---- Save -----------------------------------------------------------
    const auto sp = save_path(argc, argv);
    const auto save_r = ss::save(state, sp);
    if (!save_r.ok()) {
        std::cerr << "ERROR: " << save_r.error() << "\n";
        return EXIT_FAILURE;
    }
    std::cout << "Saved to: " << sp.string() << "\n";

    // ---- Load it back and verify ----------------------------------------
    const auto load_r = ss::load(sp);
    if (!load_r.ok()) {
        std::cerr << "ERROR: " << load_r.error() << "\n";
        return EXIT_FAILURE;
    }
    const GameState& reloaded = load_r.value();

    const bool date_ok      = (reloaded.current_date == state.current_date);
    const bool seed_ok      = (reloaded.rng.seed     == state.rng.seed);
    const bool counter_ok   = (reloaded.rng.counter  == state.rng.counter);
    const bool countries_ok = (reloaded.countries.size() == state.countries.size());
    const bool logs_ok      = (reloaded.logs.size()      == state.logs.size());

    std::cout << "\n--- Round-trip check ---\n"
              << "  current_date matches : " << (date_ok      ? "yes" : "NO") << "\n"
              << "  rng.seed matches     : " << (seed_ok      ? "yes" : "NO") << "\n"
              << "  rng.counter matches  : " << (counter_ok   ? "yes" : "NO") << "\n"
              << "  countries.size match : " << (countries_ok ? "yes" : "NO") << "\n"
              << "  logs.size match      : " << (logs_ok      ? "yes" : "NO") << "\n";

    return (date_ok && seed_ok && counter_ok && countries_ok && logs_ok)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

// M0 exit-gate integration test.
//
// End-to-end exercise of every system M0 ships:
//
//   DataLoader  -> load simulation.json + 3 countries
//   GameState   -> make_game_state(config) and push the countries in
//   TimeSystem  -> advance one day at a time for 365 days
//   Logging     -> explicit lifecycle + boundary logs through the loop
//   Diagnostics -> sanity_check after the run completes
//   SaveSystem  -> save() to a tmp file
//   SaveSystem  -> load() the same file back
//
// The asserts at the end verify that current_date, rng.seed,
// rng.counter, the country count, and each country's id / id_code /
// name survive the round trip. The runner is deliberately NOT used
// here - M0.9 ships a runner that does not load countries; this test
// proves the underlying systems compose the same flow manually.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/data_loader.hpp"
#include "leviathan/systems/diagnostics.hpp"
#include "leviathan/systems/logging_system.hpp"
#include "leviathan/systems/save_system.hpp"
#include "leviathan/systems/time_system.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameDate;
using leviathan::core::GameState;
namespace dl = leviathan::systems::data_loader;
namespace lg = leviathan::systems::logging;
namespace lt = leviathan::systems::time;
namespace ss = leviathan::systems::save_system;
namespace dg = leviathan::systems::diagnostics;

namespace {

struct TempFile {
    fs::path path;
    explicit TempFile(std::string name)
        : path(fs::temp_directory_path() / name) {
        std::error_code ec;
        fs::remove(path, ec);
    }
    ~TempFile() {
        std::error_code ec;
        fs::remove(path, ec);
    }
};

}  // namespace

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("M0 end-to-end: load -> tick 365 -> save -> load -> round-trip") {
    const fs::path data_root = LEVIATHAN_TEST_DATA_DIR;

    // ---- Step 1: load simulation config -------------------------------
    auto cfg_r = dl::load_simulation_config(data_root / "config" / "simulation.json");
    REQUIRE(cfg_r.ok());
    auto cfg = std::move(cfg_r).value();
    CHECK(cfg.start_date == GameDate(1930, 1, 1));
    CHECK(cfg.seed       == 19300101u);

    // ---- Step 2: load three country JSON files ------------------------
    const std::vector<fs::path> country_paths = {
        data_root / "countries" / "germany.json",
        data_root / "countries" / "france.json",
        data_root / "countries" / "japan.json",
    };

    std::vector<CountryState> loaded_countries;
    loaded_countries.reserve(country_paths.size());
    for (const auto& p : country_paths) {
        auto r = dl::load_country(p);
        REQUIRE_MESSAGE(r.ok(), p.string());
        loaded_countries.push_back(std::move(r).value());
    }
    REQUIRE(loaded_countries.size() == 3);

    // ---- Step 3: build GameState; assign numeric ids ------------------
    auto state = leviathan::core::make_game_state(cfg);
    for (int i = 0; i < static_cast<int>(loaded_countries.size()); ++i) {
        loaded_countries[i].id = CountryId{i};
        state.countries.push_back(std::move(loaded_countries[i]));
    }
    REQUIRE(state.countries.size() == 3);

    // ---- Step 3a (M1.2): load three Germany factions ------------------
    // The runner doesn't load factions either (by M1.2 design); this
    // integration test exercises the manual composition path.
    const std::vector<fs::path> faction_paths = {
        data_root / "factions" / "ger_military.json",
        data_root / "factions" / "ger_bureaucracy.json",
        data_root / "factions" / "ger_workers.json",
    };
    for (int i = 0; i < static_cast<int>(faction_paths.size()); ++i) {
        auto r = dl::load_faction(faction_paths[i]);
        REQUIRE_MESSAGE(r.ok(), faction_paths[i].string());
        auto f = std::move(r).value();
        // Caller assigns numeric ids and resolves the country link by
        // matching country_id_code against state.countries.
        f.id      = FactionId{i};
        f.country = state.countries.front().id;   // all three belong to GER (index 0)
        state.factions.push_back(std::move(f));
    }
    REQUIRE(state.factions.size() == 3);
    CHECK(state.factions[0].id_code == "GER_military");
    CHECK(state.factions[1].id_code == "GER_bureaucracy");
    CHECK(state.factions[2].id_code == "GER_workers");

    // ---- Step 4: tick 365 days with explicit logging ------------------
    lg::log_info(state, "lifecycle", "integration_test", "simulation start",
                 {{"days_requested", "365"},
                  {"seed",           std::to_string(state.rng.seed)},
                  {"country_count",  std::to_string(state.countries.size())}});

    int month_crossings = 0;
    int year_crossings  = 0;
    for (int i = 0; i < 365; ++i) {
        const auto r = lt::advance_one_day(state);
        if (r.month_changed) {
            ++month_crossings;
            lg::log_info(state, "time", "integration_test", "month rolled over");
        }
        if (r.year_changed) {
            ++year_crossings;
            lg::log_info(state, "time", "integration_test", "year rolled over");
        }
    }
    lg::log_info(state, "lifecycle", "integration_test", "simulation end");

    CHECK(month_crossings == 12);     // 12 month boundaries in a year
    CHECK(year_crossings  == 1);
    CHECK(state.current_date == GameDate(1931, 1, 1));

    // ---- Step 5: sanity check should find nothing ---------------------
    const auto issues = dg::sanity_check(state);
    CHECK(issues.empty());

    // ---- Step 6: save to a tmp file ----------------------------------
    TempFile tmp("leviathan_m0_endtoend.json");
    REQUIRE(ss::save(state, tmp.path).ok());
    REQUIRE(fs::exists(tmp.path));

    // ---- Step 7: also export the JSONL log (M0.6) --------------------
    {
        std::ostringstream out;
        lg::export_jsonl(out, state);
        CHECK_FALSE(out.str().empty());
    }

    // ---- Step 8: load the save back ----------------------------------
    auto loaded_r = ss::load(tmp.path);
    REQUIRE(loaded_r.ok());
    const GameState& reloaded = loaded_r.value();

    // ---- Step 9: round-trip assertions -------------------------------
    // M0.11 spec acceptance: date, seed, country count consistent.
    CHECK(reloaded.current_date == state.current_date);
    CHECK(reloaded.current_date == GameDate(1931, 1, 1));
    CHECK(reloaded.rng.seed     == state.rng.seed);
    CHECK(reloaded.rng.seed     == 19300101u);
    CHECK(reloaded.rng.counter  == state.rng.counter);
    CHECK(reloaded.countries.size() == 3);
    CHECK(reloaded.factions.size()  == 3);
    CHECK(reloaded.logs.size()      == state.logs.size());

    // Each country's identity (numeric id, on-disk code, names) must
    // survive the round trip.
    REQUIRE(reloaded.countries.size() == state.countries.size());
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const auto& before = state.countries[i];
        const auto& after  = reloaded.countries[i];
        CHECK(after.id.value() == before.id.value());
        CHECK(after.id_code    == before.id_code);
        CHECK(after.name       == before.name);
        CHECK(after.display_name == before.display_name);
    }

    // Names lining up against the on-disk shape - belt and suspenders.
    CHECK(reloaded.countries[0].id_code == "GER");
    CHECK(reloaded.countries[1].id_code == "FRA");
    CHECK(reloaded.countries[2].id_code == "JPN");

    // Factions also round-trip with their numeric ids, country link,
    // type strings, and preferred_policies order intact.
    REQUIRE(reloaded.factions.size() == state.factions.size());
    for (std::size_t i = 0; i < state.factions.size(); ++i) {
        const auto& before = state.factions[i];
        const auto& after  = reloaded.factions[i];
        CHECK(after.id.value()      == before.id.value());
        CHECK(after.country.value() == before.country.value());
        CHECK(after.id_code         == before.id_code);
        CHECK(after.type            == before.type);
        REQUIRE(after.preferred_policies.size() == before.preferred_policies.size());
        for (std::size_t k = 0; k < before.preferred_policies.size(); ++k) {
            CHECK(after.preferred_policies[k] == before.preferred_policies[k]);
        }
    }
    CHECK(reloaded.factions[0].id_code == "GER_military");
    CHECK(reloaded.factions[1].id_code == "GER_bureaucracy");
    CHECK(reloaded.factions[2].id_code == "GER_workers");
}

#endif  // LEVIATHAN_TEST_DATA_DIR

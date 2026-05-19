// Issue #112: runner-path integration test for the player-choice
// command surface.
//
// Proves the `ChooseEventOption` command is reachable from the
// existing `--commands` script path (NOT just from a unit-level
// dispatch call). Workflow: ONE `rn::run()` invocation with
// `--days 31 --player ... --commands script.json` — the day-loop
// fires the player-country option event into a pending entry,
// then the post-loop --commands hook applies the player's choice
// via `commands::apply_command_script`. This proves the full
// chain: JSON parse → command dispatch → effects apply →
// applied_commands appended → save round-trip.
//
// The test uses a HAND-BUILT scenario fixture so the event
// firing order is deterministic and the event_history_index can
// be predicted ahead of time (index 0: the single matching event
// fires on the first monthly tick for the single country).

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/player_commands.hpp"
#include "leviathan/systems/runner.hpp"
#include "leviathan/systems/save_system.hpp"

namespace fs = std::filesystem;
namespace rn = leviathan::systems::runner;
namespace ss = leviathan::systems::save_system;

namespace {

struct TempDir {
    fs::path path;
    explicit TempDir(std::string name)
        : path(fs::temp_directory_path() / name) {
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_file(const fs::path& p, const std::string& body) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(body.data(), static_cast<std::streamsize>(body.size()));
}

// Synthetic scenario builder: writes fixture files into `td` and
// returns the scenario manifest path. Caller keeps `td` in scope
// for the duration of the run (TempDir destructor wipes the
// directory).
fs::path write_synthetic_scenario(const TempDir& td) {
    // Country JSON: legitimacy = 0.10 (well below the event's
    // trigger threshold of 0.20). All other fields set to
    // mid-range so monthly pipeline doesn't error.
    const std::string country_json = R"({
      "id": "TST",
      "name": "Test Country",
      "initial_gdp": 100.0,
      "initial_stability": 0.60,
      "legal_tax_burden": 0.20,
      "fiscal_capacity": 0.60,
      "administrative_efficiency": 0.60,
      "central_control": 0.60,
      "corruption": 0.15,
      "legitimacy": 0.10,
      "military_power": 0.40,
      "threat_perception": 0.20,
      "budget": {
        "administration": 0.20, "military": 0.20, "education": 0.15,
        "welfare": 0.15, "intelligence": 0.10, "infrastructure": 0.10,
        "industry": 0.10
      }
    })";
    write_file(td.path / "data" / "countries" / "test.json", country_json);

    // Single event: matches initial state (legitimacy < 0.20),
    // single option that nudges legitimacy upward.
    const std::string events_json = R"({
      "events": [
        {
          "id": "test_crisis",
          "name": "Test Crisis",
          "description": "Synthetic crisis for the runner-path test.",
          "visible_report": "Synthetic public-facing report.",
          "true_cause": "Synthetic true cause.",
          "category": "political",
          "triggers": [
            { "target": "country.legitimacy", "op": "lt", "value": 0.20 }
          ],
          "effects": [],
          "options": [
            { "id_code": "test_act",
              "label": "Take the test action",
              "effects": [
                { "target": "country.legitimacy", "op": "add",
                  "value": 0.30 }
              ] }
          ],
          "option_effect_mode": "option_only"
        }
      ]
    })";
    write_file(td.path / "data" / "events" / "test.json", events_json);

    // Scenario manifest.
    const std::string manifest_json = R"({
      "scenario": {
        "countries": [ "countries/test.json" ],
        "factions":  [],
        "policies":  [],
        "events":    [ "events/test.json" ]
      }
    })";
    const fs::path scenario_path =
        td.path / "data" / "scenarios" / "test.json";
    write_file(scenario_path, manifest_json);
    return scenario_path;
}

fs::path canonical_config_path() {
    return fs::path(LEVIATHAN_TEST_DATA_DIR) / "config" / "simulation.json";
}

}  // namespace

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("Issue #112 runner: --commands ChooseEventOption resolves a pending "
          "player-country event in a single rn::run invocation") {
    TempDir td("issue112_runner_choice_happy");
    const fs::path scenario_path = write_synthetic_scenario(td);

    // Author the commands script BEFORE the run. We know the
    // event_history_index will be 0 because:
    //   - 1 country, 1 event, 1 player => the only event that fires
    //     in this run is the one we authored
    //   - the first monthly tick (month boundary on 1930-02-01)
    //     records it at event_history[0]
    const fs::path script_path = td.path / "choice.json";
    write_file(script_path,
        R"({ "commands": [
            { "kind": "ChooseEventOption",
              "event_history_index": 0,
              "option_id_code": "test_act" }
        ] })");

    TempDir out("issue112_runner_choice_happy_out");
    rn::RunnerOptions opts;
    opts.config_path    = canonical_config_path();
    opts.scenario_path  = scenario_path;
    opts.days           = 31;                  // crosses one monthly tick
    opts.output_dir     = out.path;
    opts.player_id_code = "TST";
    opts.commands_path  = script_path;

    const auto r = rn::run(opts);
    if (!r.ok()) { MESSAGE("rn::run error: " << r.error()); }
    REQUIRE(r.ok());

    // Inspect the saved state: pending cleared, effects applied,
    // applied_commands contains the ChooseEventOption entry.
    const auto loaded_r = ss::load(out.path / "save.json");
    REQUIRE(loaded_r);
    const auto& s = loaded_r.value();

    CHECK(s.pending_player_events.empty());

    // applied_commands has the ChooseEventOption entry.
    bool saw_choose = false;
    for (const auto& ac : s.applied_commands) {
        if (ac.command.kind ==
                leviathan::core::PlayerCommandKind::ChooseEventOption &&
            ac.command.event_history_index == 0u &&
            ac.command.option_id_code == "test_act") {
            saw_choose = true;
            break;
        }
    }
    CHECK(saw_choose);

    // Country legitimacy moved (option_only mode applied +0.30 from
    // 0.10 → 0.40). We allow a small drift since the monthly tick
    // may have nudged legitimacy via stability::tick or similar;
    // we just require the option's effect is observable.
    REQUIRE(s.countries.size() == 1u);
    CHECK(s.countries[0].legitimacy > 0.20);   // option applied
}

TEST_CASE("Issue #112 runner: --commands ChooseEventOption with invalid "
          "option_id_code is rejected via run() failure") {
    TempDir td("issue112_runner_choice_bad_option");
    const fs::path scenario_path = write_synthetic_scenario(td);

    const fs::path script_path = td.path / "bad_choice.json";
    write_file(script_path, R"({ "commands": [
        { "kind": "ChooseEventOption",
          "event_history_index": 0,
          "option_id_code": "does_not_exist" }
    ] })");

    TempDir out("issue112_runner_choice_bad_option_out");
    rn::RunnerOptions opts;
    opts.config_path    = canonical_config_path();
    opts.scenario_path  = scenario_path;
    opts.days           = 31;
    opts.output_dir     = out.path;
    opts.player_id_code = "TST";
    opts.commands_path  = script_path;

    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    // Error names the bad option for the player to diagnose.
    CHECK(r.error().find("does_not_exist") != std::string::npos);
}

TEST_CASE("Issue #112 runner: --commands script JSON parser rejects "
          "unknown command kind") {
    TempDir td("issue112_runner_bad_kind");
    const fs::path scenario_path = write_synthetic_scenario(td);

    const fs::path script_path = td.path / "bad_kind.json";
    write_file(script_path, R"({ "commands": [
        { "kind": "TeleportEntireCountry",
          "policy_id_code": "ignore" }
    ] })");

    TempDir out("issue112_runner_bad_kind_out");
    rn::RunnerOptions opts;
    opts.config_path    = canonical_config_path();
    opts.scenario_path  = scenario_path;
    opts.days           = 1;
    opts.output_dir     = out.path;
    opts.player_id_code = "TST";
    opts.commands_path  = script_path;

    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("TeleportEntireCountry") != std::string::npos);
    CHECK(r.error().find("unknown")               != std::string::npos);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

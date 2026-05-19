// M2 exit-gate integration test.
//
// End-to-end exercise of the M2 player-operation surface as it
// is meant to be invoked at the seam between M2 and any future
// milestone. Three cases mirror the M1.17 contract but pivoted
// from "monthly pipeline + scenario load + 5-artefact
// determinism" toward "player command path + gate + replay +
// 5-artefact determinism":
//
//   1. Command script + replay + verify equivalence.
//      Drives M2.21 `apply_command_script` on a fresh source
//      state, then M2.7 `replay_with_time` on an independent
//      target state, then M2.10 `compare_states` to confirm
//      the two states converge across every gameplay-relevant
//      field. Pins the M2.4 / M2.7 / M2.20 / M2.21 contract
//      that a successful script round-trips through the replay
//      log without drift.
//
//   2. Order-execution gate atomicity across command kinds.
//      Drives a mixed script through `apply_command_script`
//      against a country whose `bureaucratic_compliance` is
//      below the M2.18 / M2.19 threshold but whose
//      `military_loyalty` is above it. Pins that the
//      `AdjustBudget("military")` command lands while the
//      `EnactPolicy` is rejected by the gate, and that the
//      trailing `AdjustBudget("welfare")` is left untouched
//      (M2.3 mid-list-failure atomicity inherited through
//      M2.18 / M2.19 / M2.20 / M2.21).
//
//   3. 8-artefact byte-identical determinism with M2 commands.
//      Composes M2.2 `begin_tick` + M2.21 `apply_command_script`
//      + `step_one_day` * 31 + M2.2 `end_tick` twice into two
//      independent temp dirs and asserts save.json /
//      events.jsonl / summary.csv / countries.csv /
//      factions.csv / interest_groups.csv /
//      interest_group_country_feedback.csv /
//      interest_group_authority_pressure.csv all match
//      byte-for-byte. M1.17's determinism contract carried
//      through M2; M3.5 added the sixth artefact and M3.6
//      added the seventh + eighth. M3.8 then added one
//      Bureaucracy interest group per canonical country, so
//      the three M3 files now contain real data rows here
//      (no longer header-only) — the byte-identical contract
//      itself is unchanged.

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/player_commands.hpp"
#include "leviathan/systems/commands.hpp"
#include "leviathan/systems/data_loader.hpp"
#include "leviathan/systems/diagnostics.hpp"
#include "leviathan/systems/runner.hpp"
#include "leviathan/systems/scenario_loader.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::PlayerCommand;
using leviathan::core::PlayerCommandKind;
namespace cmd = leviathan::systems::commands;
namespace dg  = leviathan::systems::diagnostics;
namespace rn  = leviathan::systems::runner;

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

std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

#ifdef LEVIATHAN_TEST_DATA_DIR
const char* kCanonicalConfig =
    LEVIATHAN_TEST_DATA_DIR "/config/simulation.json";
const char* kCanonicalScenario =
    LEVIATHAN_TEST_DATA_DIR "/scenarios/1930_minimal.json";
#endif

#ifdef LEVIATHAN_TEST_DATA_DIR
// Build a fresh GameState with the canonical 1930_minimal
// scenario loaded and `player_country` set to GER (index 0).
// Authority fields keep the DataLoader defaults (all 0.5).
GameState fresh_state_with_ger_player() {
    namespace dl = leviathan::systems::data_loader;
    namespace sl = leviathan::systems::scenario_loader;
    auto cfg_r = dl::load_simulation_config(kCanonicalConfig);
    REQUIRE(cfg_r.ok());
    auto state = leviathan::core::make_game_state(cfg_r.value());
    REQUIRE(sl::load_into_state(state, kCanonicalScenario).ok());
    state.player_country = CountryId{0};
    return state;
}

// Two-command demo: a policy enactment + a budget adjustment.
// Both kinds exercised, both gates exercised, both M2.18 and
// M2.19 code paths walked.
std::vector<PlayerCommand> demo_script() {
    std::vector<PlayerCommand> s;
    PlayerCommand enact;
    enact.kind            = PlayerCommandKind::EnactPolicy;
    enact.policy_id_code = "raise_taxes";
    s.push_back(enact);
    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "military";
    adjust.budget_delta    = 0.02;
    s.push_back(adjust);
    return s;
}
#endif

}  // namespace

#ifdef LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// 1. Command script + replay + verify equivalence
// =====================================================================
TEST_CASE("M2 end-to-end: command script + replay reproduces source via compare_states") {
    // ---- Source: drive the script directly ---------------------------------
    GameState source = fresh_state_with_ger_player();
    REQUIRE(source.applied_commands.empty());

    const auto script = demo_script();
    auto source_r = cmd::apply_command_script(source, script);
    REQUIRE(source_r.ok());
    CHECK_FALSE(source_r.value().rejection.has_value());
    CHECK(source_r.value().apply.commands_applied == 2);
    REQUIRE(source.applied_commands.size() == 2u);
    CHECK(source.applied_commands[0].command.kind ==
          PlayerCommandKind::EnactPolicy);
    CHECK(source.applied_commands[1].command.kind ==
          PlayerCommandKind::AdjustBudget);

    // ---- Target: replay the source's log into an independent state ---------
    GameState target = fresh_state_with_ger_player();
    rn::TickController target_ctrl;
    rn::RunnerOptions target_opts;
    target_opts.config_path = kCanonicalConfig;
    target_opts.days        = 0;
    // No CSV / save paths; this test stays in-memory and compares
    // via diagnostics::compare_states (M2.10).

    REQUIRE(rn::begin_tick(target, target_opts, target_ctrl).ok());
    auto replay_r =
        cmd::replay_with_time(target, target_opts, target_ctrl,
                              source.applied_commands);
    REQUIRE(replay_r.ok());
    CHECK(replay_r.value().commands_replayed == 2);

    // ---- Compare ------------------------------------------------------------
    // M2.10 compare_states deliberately skips state.logs (begin_tick
    // adds a "simulation start" entry to target only) and rng (no
    // RNG advancement on either side), so the remaining fields —
    // current_date, player_country, every country numeric +
    // government_authority + budget + active_policies, every
    // faction, and applied_commands entries — should match.
    const auto mismatches = dg::compare_states(source, target);
    if (!mismatches.empty()) {
        // Make CI logs surface the first mismatch verbatim.
        for (const auto& m : mismatches) {
            INFO("mismatch: " << m.field_path << " : " << m.detail);
        }
    }
    CHECK(mismatches.empty());

    // Spot-check the M2.18 / M2.19 visible effects landed on both
    // states identically. raise_taxes raises legal_tax_burden;
    // AdjustBudget("military", +0.02) raises budget.military.
    CHECK(target.countries[0].legal_tax_burden ==
          doctest::Approx(source.countries[0].legal_tax_burden));
    CHECK(target.countries[0].budget.military ==
          doctest::Approx(source.countries[0].budget.military));
    REQUIRE(target.applied_commands.size() == 2u);
    CHECK(target.applied_commands[0].command.kind ==
          PlayerCommandKind::EnactPolicy);
    CHECK(target.applied_commands[1].command.kind ==
          PlayerCommandKind::AdjustBudget);
}

// =====================================================================
// 2. Order-execution gate atomicity across command kinds
// =====================================================================
TEST_CASE("M2 end-to-end: gate atomicity across EnactPolicy and AdjustBudget") {
    // Low bureaucratic_compliance traps EnactPolicy and
    // AdjustBudget(non-military); high military_loyalty rescues
    // AdjustBudget("military").
    GameState state = fresh_state_with_ger_player();
    state.countries[0].government_authority.bureaucratic_compliance = 0.05;
    state.countries[0].government_authority.military_loyalty        = 0.9;

    const double mil_before    = state.countries[0].budget.military;
    const double wel_before    = state.countries[0].budget.welfare;
    const double tax_before    = state.countries[0].legal_tax_burden;
    const std::size_t log_size_before = state.applied_commands.size();

    std::vector<PlayerCommand> script;
    {
        PlayerCommand mil;
        mil.kind            = PlayerCommandKind::AdjustBudget;
        mil.budget_category = "military";
        mil.budget_delta    = 0.03;
        script.push_back(mil);
    }
    {
        PlayerCommand enact;
        enact.kind            = PlayerCommandKind::EnactPolicy;
        enact.policy_id_code = "raise_taxes";
        script.push_back(enact);
    }
    {
        PlayerCommand wel;
        wel.kind            = PlayerCommandKind::AdjustBudget;
        wel.budget_category = "welfare";
        wel.budget_delta    = 0.04;
        script.push_back(wel);
    }

    auto r = cmd::apply_command_script(state, script);
    REQUIRE(r.ok());
    REQUIRE(r.value().rejection.has_value());
    CHECK(r.value().rejection.value().kind ==
          PlayerCommandKind::EnactPolicy);
    CHECK(r.value().rejection.value().policy_id_code == "raise_taxes");
    CHECK(r.value().apply.commands_applied == 1);

    // Military adjustment landed (gate let it through via
    // military_loyalty=0.9). Mechanical asymptotic-add:
    //   mil_before + 0.03 * (1 - mil_before)
    CHECK(state.countries[0].budget.military ==
          doctest::Approx(mil_before + 0.03 * (1.0 - mil_before)));
    // EnactPolicy rejected: no legal_tax_burden mutation, no
    // active_policies entry.
    CHECK(state.countries[0].legal_tax_burden == doctest::Approx(tax_before));
    CHECK(state.countries[0].active_policies.empty());
    // Trailing AdjustBudget("welfare") never ran.
    CHECK(state.countries[0].budget.welfare == doctest::Approx(wel_before));

    // applied_commands records the military adjustment only.
    REQUIRE(state.applied_commands.size() == log_size_before + 1u);
    CHECK(state.applied_commands.back().command.kind ==
          PlayerCommandKind::AdjustBudget);
    CHECK(state.applied_commands.back().command.budget_category ==
          "military");
}

// =====================================================================
// 3. 5-artefact byte-identical determinism with M2 commands
// =====================================================================
TEST_CASE("M2 end-to-end: same script + same setup produces byte-identical 8 artefacts") {
    auto run_once = [](const fs::path& output_dir) {
        // Same builder both runs use; the deterministic seed lives
        // in the canonical simulation.json that DataLoader reads.
        GameState state = fresh_state_with_ger_player();

        rn::RunnerOptions opts;
        opts.config_path        = kCanonicalConfig;
        opts.days               = 31;
        opts.output_dir         = output_dir;
        opts.scenario_path      = kCanonicalScenario;
        opts.summary_csv_path   = output_dir / "summary.csv";
        opts.countries_csv_path = output_dir / "countries.csv";
        opts.factions_csv_path  = output_dir / "factions.csv";

        rn::TickController ctrl;
        REQUIRE(rn::begin_tick(state, opts, ctrl).ok());
        // Apply the script ONCE between begin_tick and the first
        // step_one_day. The runner exposes no script flag yet
        // (M2.22 keeps that out of scope); composing the M2.2
        // primitives directly is the determinism contract a
        // future runner-script integration must preserve.
        REQUIRE(cmd::apply_command_script(state, demo_script()).ok());
        for (int i = 0; i < opts.days; ++i) {
            REQUIRE(rn::step_one_day(state, opts, ctrl).ok());
        }
        REQUIRE(rn::end_tick(state, opts, ctrl).ok());
    };

    TempDir td_a("leviathan_m2_endtoend_det_a");
    TempDir td_b("leviathan_m2_endtoend_det_b");
    run_once(td_a.path);
    run_once(td_b.path);

    // The 8 artefacts (M1.17's five + M3.5's interest_groups.csv +
    // M3.6's two formula-trace CSVs) must continue to survive M2's
    // command + gate path.
    CHECK(read_file(td_a.path / "save.json") ==
          read_file(td_b.path / "save.json"));
    CHECK(read_file(td_a.path / "events.jsonl") ==
          read_file(td_b.path / "events.jsonl"));
    CHECK(read_file(td_a.path / "summary.csv") ==
          read_file(td_b.path / "summary.csv"));
    CHECK(read_file(td_a.path / "countries.csv") ==
          read_file(td_b.path / "countries.csv"));
    CHECK(read_file(td_a.path / "factions.csv") ==
          read_file(td_b.path / "factions.csv"));
    // M3.5: interest_groups.csv joins the byte-identical contract.
    CHECK(read_file(td_a.path / "interest_groups.csv") ==
          read_file(td_b.path / "interest_groups.csv"));
    // M3.6: two formula-trace CSVs round out the 8-artefact set.
    CHECK(read_file(td_a.path / "interest_group_country_feedback.csv") ==
          read_file(td_b.path / "interest_group_country_feedback.csv"));
    CHECK(read_file(td_a.path / "interest_group_authority_pressure.csv") ==
          read_file(td_b.path / "interest_group_authority_pressure.csv"));
    // M4.2: ninth artefact — provinces.svg.
    CHECK(read_file(td_a.path / "provinces.svg") ==
          read_file(td_b.path / "provinces.svg"));
    // M4.5: tenth artefact — map.html.
    CHECK(read_file(td_a.path / "map.html") ==
          read_file(td_b.path / "map.html"));
}

#endif  // LEVIATHAN_TEST_DATA_DIR

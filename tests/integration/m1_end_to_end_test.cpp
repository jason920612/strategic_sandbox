// M1 exit-gate integration test.
//
// End-to-end exercise of the M1 single-country internal-politics
// pipeline as it is meant to be invoked at the seam between M1 and
// any future milestone. Drives the runner with the canonical
// `1930_with_start_policies.json` scenario, ticks a full year, opts
// in to every diagnostic CSV, and verifies:
//
//   * the scenario loader populates 3 countries + 3 factions + 10
//     policies and applies the two day-0 starting policies on GER
//     (M1.11 + M1.13);
//   * `policy::apply_policy_effects` recorded an `ActivePolicy` for
//     each day-0 enactment with `expires_on = current_date +
//     duration_days` (M1.15);
//   * the monthly pipeline runs exactly 12 times across 1930
//     (one per month boundary, M1.9 + M1.10);
//   * the save round-trips with `active_policies` and
//     `last_gdp_growth_rate` intact (M1.15, M1.12);
//   * the determinism contract extends to EIGHT byte-identical
//     artefacts: save.json, events.jsonl, summary.csv,
//     countries.csv, factions.csv, interest_groups.csv (M3.5),
//     interest_group_country_feedback.csv (M3.6),
//     interest_group_authority_pressure.csv (M3.6). M3.8 added
//     one Bureaucracy interest group per canonical country, so
//     the three M3 files now contain real data rows here (no
//     longer header-only); the byte-identical contract is the
//     same shape — two same-seed runs match byte-for-byte
//     whether the rows are present or not.
//
// Unlike `m0_end_to_end_test.cpp`, this test goes through the
// runner. By M1.11 the runner accepts `--scenario` and loads the
// canonical world itself, so the integration check is "did the
// orchestrator wire every system together correctly", not "do the
// systems compose by hand".

#include <doctest/doctest.h>

#include <cmath>
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
#include "leviathan/systems/runner.hpp"
#include "leviathan/systems/save_system.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::GameDate;
using leviathan::core::GameState;
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

std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

#ifdef LEVIATHAN_TEST_DATA_DIR
const char* kCanonicalConfig =
    LEVIATHAN_TEST_DATA_DIR "/config/simulation.json";
const char* kStartingPoliciesScenario =
    LEVIATHAN_TEST_DATA_DIR "/scenarios/1930_with_start_policies.json";
#endif

// Build runner options for a 365-day full-year run that opts in to
// every diagnostic artefact. Output directory is supplied by the
// caller so tests can compare two runs across independent temp dirs.
rn::RunnerOptions make_full_year_opts(const fs::path& output_dir) {
    rn::RunnerOptions opts;
    opts.config_path        = kCanonicalConfig;
    opts.days               = 365;
    opts.output_dir         = output_dir;
    opts.seed_override      = std::uint64_t{0xDA7AC011U};   // any fixed value
    opts.scenario_path      = kStartingPoliciesScenario;
    opts.summary_csv_path   = output_dir / "summary.csv";
    opts.countries_csv_path = output_dir / "countries.csv";
    opts.factions_csv_path  = output_dir / "factions.csv";
    return opts;
}

}  // namespace

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("M1 end-to-end: scenario load -> day-0 enactment -> 365-day tick -> save / load round-trip") {
    TempDir td("leviathan_m1_endtoend");
    const auto opts = make_full_year_opts(td.path);

    // ---- Step 1: drive the runner ----------------------------------------
    const auto run_r = rn::run(opts);
    REQUIRE(run_r.ok());
    const auto& outcome = run_r.value();

    // Start / end dates: simulation.json pins start_date = 1930-01-01.
    CHECK(outcome.start_date == GameDate(1930, 1, 1));
    CHECK(outcome.end_date   == GameDate(1931, 1, 1));
    CHECK(outcome.days_advanced == 365);

    // M1.10: month_changed fires 12 times across 1930 (Jan->Feb,
    // Feb->Mar, ..., Dec->Jan-of-1931). Each one runs the M1.9
    // monthly pipeline.
    CHECK(outcome.monthly_ticks == 12);

    // Every opt-in CSV path is echoed back through the outcome.
    REQUIRE(outcome.summary_csv_path.has_value());
    REQUIRE(outcome.countries_csv_path.has_value());
    REQUIRE(outcome.factions_csv_path.has_value());

    // Snapshot cadence: start + 12 month_changed + final post-sanity = 14.
    // - summary CSV is 1 row per snapshot point.
    // - countries CSV is 3 countries per snapshot point  = 42.
    // - factions CSV  is 3 factions per snapshot point   = 42.
    CHECK(outcome.summary_rows       == 14u);
    CHECK(outcome.countries_csv_rows == 42u);
    CHECK(outcome.factions_csv_rows  == 42u);

    // All three CSV files exist on disk.
    REQUIRE(fs::exists(td.path / "summary.csv"));
    REQUIRE(fs::exists(td.path / "countries.csv"));
    REQUIRE(fs::exists(td.path / "factions.csv"));

    // ---- Step 2: load the save back and verify shape ---------------------
    const auto loaded_r = ss::load(outcome.save_path);
    REQUIRE(loaded_r.ok());
    const GameState& reloaded = loaded_r.value();

    CHECK(reloaded.current_date    == GameDate(1931, 1, 1));
    REQUIRE(reloaded.countries.size() == 3u);
    REQUIRE(reloaded.factions.size()  == 3u);
    REQUIRE(reloaded.policies.size()  == 10u);
    CHECK(reloaded.countries[0].id_code == "GER");
    CHECK(reloaded.countries[1].id_code == "FRA");
    CHECK(reloaded.countries[2].id_code == "JPN");

    // ---- Step 3: M1.13 + M1.15 day-0 active_policies survived ------------
    // The starting-policies scenario enacts:
    //   raise_taxes              on GER (duration_days = 60)
    //   increase_military_budget on GER (duration_days = 30)
    // in that order, on day 0 (1930-01-01). Each enactment records an
    // ActivePolicy entry on GER; FRA and JPN should have empty lists.
    const auto& ger = reloaded.countries[0];
    REQUIRE(ger.active_policies.size() == 2u);
    CHECK(ger.active_policies[0].policy_id_code == "raise_taxes");
    CHECK(ger.active_policies[0].expires_on     == GameDate(1930, 3, 2));
    CHECK(ger.active_policies[1].policy_id_code == "increase_military_budget");
    CHECK(ger.active_policies[1].expires_on     == GameDate(1930, 1, 31));
    CHECK(reloaded.countries[1].active_policies.empty());
    CHECK(reloaded.countries[2].active_policies.empty());

    // ---- Step 4: M1.12 coupling produced a non-zero growth signal --------
    // After 12 economy ticks the GDP and last_gdp_growth_rate must
    // have moved. We don't pin specific numerics (formulas may be
    // rebalanced later); we just assert "the pipeline did something".
    CHECK(ger.gdp                   != doctest::Approx(100.0));
    CHECK(ger.last_gdp_growth_rate  != doctest::Approx(0.0));
}

TEST_CASE("M1 end-to-end: 10-year soak run completes without sanity issues") {
    // RFC-090 §1.17 acceptance criterion: 跑 10 年單國測試. 3652 days
    // takes 1930-01-01 -> 1940-01-01 (years 1932 and 1936 are leap).
    // The simulation must:
    //   * run 120 monthly pipelines without any system returning failure;
    //   * leave every country's gdp / stability / legitimacy /
    //     last_gdp_growth_rate finite (no NaN, no Inf);
    //   * end with sanity_issues_logged == 0.
    TempDir td("leviathan_m1_endtoend_soak");
    auto opts = make_full_year_opts(td.path);
    opts.days = 3652;
    // The soak run is about durability, not file-size budget. Don't
    // accumulate per-month CSVs across 10 years -- those are O(10x)
    // bigger and exercised by the 1-year tests above.
    opts.summary_csv_path.reset();
    opts.countries_csv_path.reset();
    opts.factions_csv_path.reset();

    const auto run_r = rn::run(opts);
    REQUIRE(run_r.ok());
    const auto& outcome = run_r.value();

    CHECK(outcome.end_date            == GameDate(1940, 1, 1));
    CHECK(outcome.days_advanced       == 3652);
    CHECK(outcome.monthly_ticks       == 120);          // 12 months × 10 years
    CHECK(outcome.sanity_issues_logged == 0u);

    // Load back to confirm every numeric field survived 10 years of
    // exponential compounding in economy::tick + 10 years of drift in
    // stability::tick. We don't pin values (formulas can be rebalanced
    // later); we pin finiteness.
    const auto loaded_r = ss::load(outcome.save_path);
    REQUIRE(loaded_r.ok());
    const GameState& reloaded = loaded_r.value();
    REQUIRE(reloaded.countries.size() == 3u);
    for (const auto& c : reloaded.countries) {
        CHECK(std::isfinite(c.gdp));
        CHECK(std::isfinite(c.tax_revenue));
        CHECK(std::isfinite(c.budget_balance));
        CHECK(std::isfinite(c.stability));
        CHECK(std::isfinite(c.legitimacy));
        CHECK(std::isfinite(c.last_gdp_growth_rate));
        // Ratio fields stay clamped.
        CHECK(c.stability  >= 0.0);
        CHECK(c.stability  <= 1.0);
        CHECK(c.legitimacy >= 0.0);
        CHECK(c.legitimacy <= 1.0);
    }
}

TEST_CASE("M1 end-to-end: same seed produces byte-identical save / log / all three CSVs") {
    // M0.10's determinism contract was extended through M1.12 (v6),
    // M1.14 (--countries-csv), M1.15 (v7), M1.16 (--factions-csv),
    // and M2.1 (v8 + --player). The contract is independent of
    // milestone.
    // Pin the final shape here so a future system that quietly
    // introduces non-determinism on the monthly pipeline path
    // (RNG misuse, iteration-order dependence, path metadata
    // leaking into logs) fails this gate loudly.
    TempDir td_a("leviathan_m1_endtoend_det_a");
    TempDir td_b("leviathan_m1_endtoend_det_b");

    REQUIRE(rn::run(make_full_year_opts(td_a.path)).ok());
    REQUIRE(rn::run(make_full_year_opts(td_b.path)).ok());

    CHECK(read_file(td_a.path / "save.json")    ==
          read_file(td_b.path / "save.json"));
    CHECK(read_file(td_a.path / "events.jsonl") ==
          read_file(td_b.path / "events.jsonl"));
    CHECK(read_file(td_a.path / "summary.csv")  ==
          read_file(td_b.path / "summary.csv"));
    CHECK(read_file(td_a.path / "countries.csv") ==
          read_file(td_b.path / "countries.csv"));
    CHECK(read_file(td_a.path / "factions.csv") ==
          read_file(td_b.path / "factions.csv"));
    // M3.5: interest_groups.csv joins the byte-identical contract.
    // M3.8 added one Bureaucracy group per canonical country, so the
    // file now carries real data rows on this run; the byte-identical
    // contract is unchanged in shape.
    CHECK(read_file(td_a.path / "interest_groups.csv") ==
          read_file(td_b.path / "interest_groups.csv"));
    // M3.6: two formula-trace CSVs round out the 8-artefact set.
    // M3.8: same shift from header-only to data rows applies — both
    // M3.3 country_feedback and M3.4 authority_pressure fire on the
    // Bureaucracy groups every monthly tick.
    CHECK(read_file(td_a.path / "interest_group_country_feedback.csv") ==
          read_file(td_b.path / "interest_group_country_feedback.csv"));
    // M4.2: provinces.svg is the ninth unconditional artefact;
    // renders deterministic <circle> markers from state.provinces.
    // M4.1 fixtures put 3 nodes in the canonical scenario, so the
    // file carries real data (not the header-only path).
    CHECK(read_file(td_a.path / "provinces.svg") ==
          read_file(td_b.path / "provinces.svg"));
    CHECK(read_file(td_a.path / "interest_group_authority_pressure.csv") ==
          read_file(td_b.path / "interest_group_authority_pressure.csv"));
}

#endif  // LEVIATHAN_TEST_DATA_DIR

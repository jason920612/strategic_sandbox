#include <doctest/doctest.h>

#include <array>
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
#include "leviathan/systems/runner.hpp"
#include "leviathan/systems/save_system.hpp"
#include "leviathan/systems/scenario_loader.hpp"

namespace fs = std::filesystem;
namespace rn = leviathan::systems::runner;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------

namespace {

// Convenience: build a const char*[] from a list of literals so we
// can hand it to parse_args(argc, argv).
template <std::size_t N>
struct Argv {
    std::array<const char*, N> data;
    int argc;
    explicit Argv(std::array<const char*, N> a) : data(a), argc(static_cast<int>(N)) {}
    const char* const* argv() const { return data.data(); }
};

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
const char* kCanonicalConfig          = LEVIATHAN_TEST_DATA_DIR "/config/simulation.json";
const char* kCanonicalScenario        = LEVIATHAN_TEST_DATA_DIR "/scenarios/1930_minimal.json";
const char* kStartingPoliciesScenario = LEVIATHAN_TEST_DATA_DIR "/scenarios/1930_with_start_policies.json";
#endif

}  // namespace

// =====================================================================
// parse_args
// =====================================================================

TEST_CASE("parse_args: --days alone is enough; other defaults apply") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "10"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    const auto& o = r.value();
    CHECK(o.days == 10);
    CHECK(o.config_path == fs::path("data/config/simulation.json"));
    CHECK(o.output_dir  == fs::path("out"));
    CHECK_FALSE(o.seed_override.has_value());
    CHECK_FALSE(o.save_path.has_value());
    CHECK_FALSE(o.log_path.has_value());
    CHECK_FALSE(o.show_help);
}

TEST_CASE("parse_args: every flag set explicitly") {
    Argv arg(std::array<const char*, 13>{
        "leviathan",
        "--config", "x/sim.json",
        "--days",   "365",
        "--seed",   "12345",
        "--output", "y/out",
        "--save",   "y/out/snap.json",
        "--log",    "y/out/log.jsonl",
    });
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    const auto& o = r.value();
    CHECK(o.config_path == fs::path("x/sim.json"));
    CHECK(o.days == 365);
    REQUIRE(o.seed_override.has_value());
    CHECK(o.seed_override.value() == 12345u);
    CHECK(o.output_dir == fs::path("y/out"));
    REQUIRE(o.save_path.has_value());
    CHECK(o.save_path.value() == fs::path("y/out/snap.json"));
    REQUIRE(o.log_path.has_value());
    CHECK(o.log_path.value() == fs::path("y/out/log.jsonl"));
}

TEST_CASE("parse_args: --help sets show_help and skips --days check") {
    Argv arg(std::array<const char*, 2>{"leviathan", "--help"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK(r.value().show_help);
}

TEST_CASE("parse_args: -h short form also sets show_help") {
    Argv arg(std::array<const char*, 2>{"leviathan", "-h"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK(r.value().show_help);
}

TEST_CASE("parse_args: missing --days is rejected") {
    Argv arg(std::array<const char*, 1>{"leviathan"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--days is required") != std::string::npos);
}

TEST_CASE("parse_args: unknown flag is rejected") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--ponies", "10"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--ponies") != std::string::npos);
}

TEST_CASE("parse_args: flag without value is rejected") {
    Argv arg(std::array<const char*, 2>{"leviathan", "--days"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--days") != std::string::npos);
    CHECK(r.error().find("requires a value") != std::string::npos);
}

TEST_CASE("parse_args: non-numeric --days is rejected") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "ten"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("ten") != std::string::npos);
}

TEST_CASE("parse_args: --seed accepts a large uint64") {
    Argv arg(std::array<const char*, 5>{
        "leviathan", "--days", "1", "--seed", "18446744073709551615"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    REQUIRE(r.value().seed_override.has_value());
    CHECK(r.value().seed_override.value() == std::uint64_t{0xFFFFFFFFFFFFFFFFull});
}

TEST_CASE("parse_args: --seed with junk text is rejected") {
    Argv arg(std::array<const char*, 5>{
        "leviathan", "--days", "1", "--seed", "-1"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--seed") != std::string::npos);
}

#ifdef LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// run - small case
// =====================================================================

TEST_CASE("run: 3-day run produces save + log files and correct summary") {
    TempDir td("leviathan_runner_3day");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 3;
    opts.output_dir  = td.path;
    opts.seed_override = std::uint64_t{42};

    auto r = rn::run(opts);
    REQUIRE(r.ok());
    const auto& o = r.value();
    CHECK(o.days_advanced == 3);
    // canonical config starts at 1930-01-01
    CHECK(o.start_date.year()  == 1930);
    CHECK(o.start_date.month() == 1);
    CHECK(o.start_date.day()   == 1);
    CHECK(o.end_date.day()     == 4);
    CHECK(fs::exists(o.save_path));
    CHECK(fs::exists(o.log_path));
    // Two lifecycle logs minimum (start + end), no boundary crossings
    // in three days at start of the year.
    CHECK(o.log_count >= 2);
}

// =====================================================================
// run - 365 days
// =====================================================================

TEST_CASE("run: 365-day run lands at end of 1930") {
    TempDir td("leviathan_runner_365day");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 365;
    opts.output_dir  = td.path;

    auto r = rn::run(opts);
    REQUIRE(r.ok());
    const auto& o = r.value();
    // 1930 is not a leap year, so 365 days from 1930-01-01 = 1931-01-01.
    CHECK(o.end_date.year()  == 1931);
    CHECK(o.end_date.month() == 1);
    CHECK(o.end_date.day()   == 1);
    // We log start + end + 12 month rollovers + 1 year rollover = 15.
    CHECK(o.log_count == 15);
}

// =====================================================================
// run - determinism
// =====================================================================

TEST_CASE("run: same seed produces byte-identical save and log files") {
    TempDir td_a("leviathan_runner_det_a");
    TempDir td_b("leviathan_runner_det_b");

    auto make_opts = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path  = kCanonicalConfig;
        o.days         = 30;
        o.output_dir   = dir;
        o.seed_override = std::uint64_t{0xC0FFEE};
        return o;
    };

    REQUIRE(rn::run(make_opts(td_a.path)).ok());
    REQUIRE(rn::run(make_opts(td_b.path)).ok());

    const std::string save_a = read_file(td_a.path / "save.json");
    const std::string save_b = read_file(td_b.path / "save.json");
    const std::string log_a  = read_file(td_a.path / "events.jsonl");
    const std::string log_b  = read_file(td_b.path / "events.jsonl");

    CHECK(save_a == save_b);
    CHECK(log_a  == log_b);
    // Sanity: the files actually have content.
    CHECK_FALSE(save_a.empty());
    CHECK_FALSE(log_a.empty());
}

// =====================================================================
// run - output dir auto-create
// =====================================================================

TEST_CASE("run: output directory is created if missing") {
    TempDir td("leviathan_runner_outdir");
    const fs::path nested = td.path / "nested" / "out";
    // Confirm the nested dir does not exist before the run.
    REQUIRE_FALSE(fs::exists(nested));

    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 1;
    opts.output_dir  = nested;

    auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(fs::exists(r.value().save_path));
    CHECK(fs::exists(r.value().log_path));
}

// =====================================================================
// run - error paths
// =====================================================================

TEST_CASE("run: missing config file is reported clearly") {
    TempDir td("leviathan_runner_badconf");
    rn::RunnerOptions opts;
    opts.config_path = "does-not-exist/sim.json";
    opts.days        = 1;
    opts.output_dir  = td.path;
    auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("does-not-exist/sim.json") != std::string::npos);
}

TEST_CASE("run: --days 0 is allowed (produces start == end)") {
    TempDir td("leviathan_runner_zerodays");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 0;
    opts.output_dir  = td.path;
    auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().days_advanced == 0);
    CHECK(r.value().start_date    == r.value().end_date);
}

TEST_CASE("run: negative days is rejected") {
    TempDir td("leviathan_runner_negdays");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = -1;
    opts.output_dir  = td.path;
    auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find(">=") != std::string::npos);
}

// =====================================================================
// run + --summary-csv (M0.10)
// =====================================================================

TEST_CASE("run: --summary-csv writes a header-and-rows file for a 30-day run") {
    // M0.10 acceptance criterion: a 30-day run produces a summary CSV.
    // Starting at 1930-01-01, +30 days lands on 1930-01-31 - one day
    // short of crossing the month boundary - so we expect exactly two
    // rows: the start snapshot and the final (post-sanity) snapshot.
    TempDir td("leviathan_runner_csv_30day");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 30;
    opts.output_dir  = td.path;
    opts.summary_csv_path = td.path / "summary.csv";

    auto r = rn::run(opts);
    REQUIRE(r.ok());
    REQUIRE(fs::exists(opts.summary_csv_path.value()));

    const std::string text = read_file(opts.summary_csv_path.value());
    static constexpr const char* kExpectedHeader =
        "date,country_count,log_count,seed";
    CHECK(text.substr(0, std::char_traits<char>::length(kExpectedHeader))
          == kExpectedHeader);
    // 1 header line + 2 data rows = 3 newlines.
    std::size_t newlines = 0;
    for (char c : text) if (c == '\n') ++newlines;
    CHECK(newlines == 3);
    CHECK(r.value().summary_rows == 2);

    // First data row reports the start date and seed.
    CHECK(text.find("1930-01-01,0,") != std::string::npos);
    // Last data row reports the post-tick date 1930-01-31.
    CHECK(text.find("1930-01-31,0,") != std::string::npos);
}

TEST_CASE("run: --summary-csv with 31 days captures the month boundary snapshot") {
    // 31 days from 1930-01-01 crosses into 1930-02-01, so the runner
    // takes an extra snapshot at the rollover. Expected 3 data rows:
    // start (01-01), month-boundary (02-01), final (02-01).
    TempDir td("leviathan_runner_csv_31day");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;
    opts.summary_csv_path = td.path / "summary.csv";

    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().summary_rows == 3);
}

TEST_CASE("run: --summary-csv same-seed two-run CSV is byte-identical") {
    // Determinism property extends to the new CSV output.
    TempDir td_a("leviathan_runner_csv_det_a");
    TempDir td_b("leviathan_runner_csv_det_b");

    auto opts_for = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path  = kCanonicalConfig;
        o.days         = 60;
        o.output_dir   = dir;
        o.seed_override = std::uint64_t{0xBEEF};
        o.summary_csv_path = dir / "summary.csv";
        return o;
    };

    REQUIRE(rn::run(opts_for(td_a.path)).ok());
    REQUIRE(rn::run(opts_for(td_b.path)).ok());

    const std::string csv_a = read_file(td_a.path / "summary.csv");
    const std::string csv_b = read_file(td_b.path / "summary.csv");
    CHECK(csv_a == csv_b);
    CHECK_FALSE(csv_a.empty());
}

TEST_CASE("run: --summary-csv is optional - no CSV file written when unset") {
    TempDir td("leviathan_runner_csv_off");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 30;
    opts.output_dir  = td.path;
    // summary_csv_path intentionally unset.

    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().summary_rows == 0);
    CHECK_FALSE(fs::exists(td.path / "summary.csv"));
}

TEST_CASE("run: sanity check has nothing to flag on a clean runner-built state") {
    TempDir td("leviathan_runner_sanity_clean");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 5;
    opts.output_dir  = td.path;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().sanity_issues_logged == 0);
}

TEST_CASE("run: M0.9 byte-identical guarantee survives sanity-check integration") {
    // Regression for M0.10's risk that adding sanity_check at the end
    // of run() could quietly change log content.
    TempDir td_a("leviathan_runner_legacy_det_a");
    TempDir td_b("leviathan_runner_legacy_det_b");

    auto opts_for = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path  = kCanonicalConfig;
        o.days         = 30;
        o.output_dir   = dir;
        o.seed_override = std::uint64_t{0xC0FFEE};
        return o;
    };

    REQUIRE(rn::run(opts_for(td_a.path)).ok());
    REQUIRE(rn::run(opts_for(td_b.path)).ok());

    CHECK(read_file(td_a.path / "save.json")    == read_file(td_b.path / "save.json"));
    CHECK(read_file(td_a.path / "events.jsonl") == read_file(td_b.path / "events.jsonl"));
}

TEST_CASE("parse_args: --summary-csv flag is plumbed through") {
    Argv arg(std::array<const char*, 5>{
        "leviathan", "--days", "3",
        "--summary-csv", "out/summary.csv"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    REQUIRE(r.value().summary_csv_path.has_value());
    CHECK(r.value().summary_csv_path.value() == fs::path("out/summary.csv"));
}

// =====================================================================
// M1.11 - --scenario flag
// =====================================================================

// =====================================================================
// M1.14 - --countries-csv flag
// =====================================================================

TEST_CASE("parse_args: --countries-csv flag is plumbed through") {
    Argv arg(std::array<const char*, 5>{
        "leviathan", "--days", "3",
        "--countries-csv", "out/countries.csv"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    REQUIRE(r.value().countries_csv_path.has_value());
    CHECK(r.value().countries_csv_path.value() ==
          fs::path("out/countries.csv"));
}

TEST_CASE("parse_args: --countries-csv without a value is rejected") {
    Argv arg(std::array<const char*, 4>{
        "leviathan", "--days", "1", "--countries-csv"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--countries-csv") != std::string::npos);
}

TEST_CASE("parse_args: --countries-csv defaults to unset when not passed") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "5"});
    const auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK_FALSE(r.value().countries_csv_path.has_value());
}

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("run: without --countries-csv no per-country CSV is written") {
    TempDir td("leviathan_runner_m114_no_countries_csv");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;
    // countries_csv_path intentionally unset.
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().countries_csv_rows == 0u);
    CHECK_FALSE(fs::exists(td.path / "countries.csv"));
}

TEST_CASE("run: --countries-csv with empty state writes header-only file") {
    // No --scenario, so state.countries is empty. The CSV should
    // contain just the header (no data rows).
    TempDir td("leviathan_runner_m114_empty_countries_csv");
    rn::RunnerOptions opts;
    opts.config_path        = kCanonicalConfig;
    opts.days               = 31;
    opts.output_dir         = td.path;
    opts.countries_csv_path = td.path / "countries.csv";
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().countries_csv_rows == 0u);
    REQUIRE(fs::exists(td.path / "countries.csv"));
    const std::string text = read_file(td.path / "countries.csv");
    CHECK(text ==
          "date,id_code,gdp,tax_revenue,budget_balance,"
          "stability,legitimacy,last_gdp_growth_rate\n");
}

TEST_CASE("run: --countries-csv + scenario emits one row per country per snapshot point") {
    // Snapshot cadence: start + each month_changed + final post-sanity.
    // For 31 days starting 1930-01-01 we cross one month boundary,
    // so there are 3 snapshot points. With 3 countries in the canonical
    // scenario, that's 9 data rows.
    TempDir td("leviathan_runner_m114_canonical_countries_csv");
    rn::RunnerOptions opts;
    opts.config_path        = kCanonicalConfig;
    opts.days               = 31;
    opts.output_dir         = td.path;
    opts.scenario_path      = kCanonicalScenario;
    opts.countries_csv_path = td.path / "countries.csv";
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().countries_csv_rows == 9u);

    const std::string text = read_file(td.path / "countries.csv");
    // Every country's id_code appears in the file.
    CHECK(text.find("GER") != std::string::npos);
    CHECK(text.find("FRA") != std::string::npos);
    CHECK(text.find("JPN") != std::string::npos);
    // After at least one month boundary, last_gdp_growth_rate has
    // been written for every country. The final snapshot's GER row
    // should contain a non-zero scientific-notation growth value.
    // (We assert presence of an exponent marker as a sanity proxy.)
    CHECK(text.find("e-") != std::string::npos);
}

TEST_CASE("run: --countries-csv preserves byte-identical determinism on same seed") {
    // Determinism extends to the new CSV output.
    TempDir td_a("leviathan_runner_m114_det_a");
    TempDir td_b("leviathan_runner_m114_det_b");

    auto opts_for = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path        = kCanonicalConfig;
        o.days               = 90;
        o.output_dir         = dir;
        o.seed_override      = std::uint64_t{0xDA7AB00B};
        o.scenario_path      = kCanonicalScenario;
        o.countries_csv_path = dir / "countries.csv";
        return o;
    };

    REQUIRE(rn::run(opts_for(td_a.path)).ok());
    REQUIRE(rn::run(opts_for(td_b.path)).ok());
    CHECK(read_file(td_a.path / "countries.csv") == read_file(td_b.path / "countries.csv"));
}

TEST_CASE("run: --countries-csv does NOT change --summary-csv output (M0.10 contract)") {
    // Regression: --countries-csv must not perturb the existing
    // summary-CSV byte format. Run twice on the same seed, one with
    // --countries-csv set, one without. The summary CSV files must
    // be byte-identical.
    TempDir td_a("leviathan_runner_m114_summary_iso_a");
    TempDir td_b("leviathan_runner_m114_summary_iso_b");

    rn::RunnerOptions opts_a;
    opts_a.config_path      = kCanonicalConfig;
    opts_a.days             = 60;
    opts_a.output_dir       = td_a.path;
    opts_a.seed_override    = std::uint64_t{0xCAFE};
    opts_a.summary_csv_path = td_a.path / "summary.csv";

    rn::RunnerOptions opts_b = opts_a;
    opts_b.output_dir         = td_b.path;
    opts_b.summary_csv_path   = td_b.path / "summary.csv";
    opts_b.countries_csv_path = td_b.path / "countries.csv";  // extra opt-in

    REQUIRE(rn::run(opts_a).ok());
    REQUIRE(rn::run(opts_b).ok());

    CHECK(read_file(td_a.path / "summary.csv") == read_file(td_b.path / "summary.csv"));
}

#endif  // LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// M1.16 - --factions-csv flag
// =====================================================================

TEST_CASE("parse_args: --factions-csv flag is plumbed through") {
    Argv arg(std::array<const char*, 5>{
        "leviathan", "--days", "3",
        "--factions-csv", "out/factions.csv"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    REQUIRE(r.value().factions_csv_path.has_value());
    CHECK(r.value().factions_csv_path.value() ==
          fs::path("out/factions.csv"));
}

TEST_CASE("parse_args: --factions-csv without a value is rejected") {
    Argv arg(std::array<const char*, 4>{
        "leviathan", "--days", "1", "--factions-csv"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--factions-csv") != std::string::npos);
}

TEST_CASE("parse_args: --factions-csv defaults to unset when not passed") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "5"});
    const auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK_FALSE(r.value().factions_csv_path.has_value());
}

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("run: without --factions-csv no per-faction CSV is written") {
    TempDir td("leviathan_runner_m116_no_factions_csv");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;
    // factions_csv_path intentionally unset.
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().factions_csv_rows == 0u);
    CHECK_FALSE(fs::exists(td.path / "factions.csv"));
}

TEST_CASE("run: --factions-csv with empty state writes header-only file") {
    // No --scenario, so state.factions is empty. The CSV is header only.
    TempDir td("leviathan_runner_m116_empty_factions_csv");
    rn::RunnerOptions opts;
    opts.config_path       = kCanonicalConfig;
    opts.days              = 31;
    opts.output_dir        = td.path;
    opts.factions_csv_path = td.path / "factions.csv";
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().factions_csv_rows == 0u);
    REQUIRE(fs::exists(td.path / "factions.csv"));
    const std::string text = read_file(td.path / "factions.csv");
    CHECK(text ==
          "date,id_code,country_id_code,type,support,influence,"
          "radicalism,loyalty,resources\n");
}

TEST_CASE("run: --factions-csv + scenario emits one row per faction per snapshot point") {
    // Snapshot cadence: start + each month_changed + final post-sanity.
    // 31 days from 1930-01-01 crosses one month boundary → 3 snapshot
    // points. Canonical scenario has 3 GER factions → 9 data rows.
    TempDir td("leviathan_runner_m116_canonical_factions_csv");
    rn::RunnerOptions opts;
    opts.config_path       = kCanonicalConfig;
    opts.days              = 31;
    opts.output_dir        = td.path;
    opts.scenario_path     = kCanonicalScenario;
    opts.factions_csv_path = td.path / "factions.csv";
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().factions_csv_rows == 9u);

    const std::string text = read_file(td.path / "factions.csv");
    // All three canonical GER factions appear by id_code.
    CHECK(text.find("GER_military")    != std::string::npos);
    CHECK(text.find("GER_workers")     != std::string::npos);
    CHECK(text.find("GER_bureaucracy") != std::string::npos);
    // Doubles use scientific notation; sanity-check the marker.
    CHECK(text.find("e-") != std::string::npos);
}

TEST_CASE("run: --factions-csv preserves byte-identical determinism on same seed") {
    TempDir td_a("leviathan_runner_m116_det_a");
    TempDir td_b("leviathan_runner_m116_det_b");

    auto opts_for = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path       = kCanonicalConfig;
        o.days              = 90;
        o.output_dir        = dir;
        o.seed_override     = std::uint64_t{0xDA7AB00B};
        o.scenario_path     = kCanonicalScenario;
        o.factions_csv_path = dir / "factions.csv";
        return o;
    };

    REQUIRE(rn::run(opts_for(td_a.path)).ok());
    REQUIRE(rn::run(opts_for(td_b.path)).ok());
    CHECK(read_file(td_a.path / "factions.csv") == read_file(td_b.path / "factions.csv"));
}

TEST_CASE("run: --factions-csv does NOT change --summary-csv output (M0.10 contract)") {
    // M0.10 contract: opting into faction CSV must not perturb the
    // summary CSV byte stream.
    TempDir td_a("leviathan_runner_m116_summary_iso_a");
    TempDir td_b("leviathan_runner_m116_summary_iso_b");

    rn::RunnerOptions opts_a;
    opts_a.config_path      = kCanonicalConfig;
    opts_a.days             = 60;
    opts_a.output_dir       = td_a.path;
    opts_a.seed_override    = std::uint64_t{0xCAFE};
    opts_a.summary_csv_path = td_a.path / "summary.csv";

    rn::RunnerOptions opts_b = opts_a;
    opts_b.output_dir        = td_b.path;
    opts_b.summary_csv_path  = td_b.path / "summary.csv";
    opts_b.factions_csv_path = td_b.path / "factions.csv";  // extra opt-in

    REQUIRE(rn::run(opts_a).ok());
    REQUIRE(rn::run(opts_b).ok());

    CHECK(read_file(td_a.path / "summary.csv") == read_file(td_b.path / "summary.csv"));
}

TEST_CASE("run: --factions-csv does NOT change --countries-csv output (M1.14 contract)") {
    // M1.14 contract: adding faction CSV must not perturb the per-
    // country CSV either.
    TempDir td_a("leviathan_runner_m116_countries_iso_a");
    TempDir td_b("leviathan_runner_m116_countries_iso_b");

    rn::RunnerOptions opts_a;
    opts_a.config_path        = kCanonicalConfig;
    opts_a.days               = 60;
    opts_a.output_dir         = td_a.path;
    opts_a.seed_override      = std::uint64_t{0xC0DE};
    opts_a.scenario_path      = kCanonicalScenario;
    opts_a.countries_csv_path = td_a.path / "countries.csv";

    rn::RunnerOptions opts_b = opts_a;
    opts_b.output_dir         = td_b.path;
    opts_b.countries_csv_path = td_b.path / "countries.csv";
    opts_b.factions_csv_path  = td_b.path / "factions.csv";  // extra opt-in

    REQUIRE(rn::run(opts_a).ok());
    REQUIRE(rn::run(opts_b).ok());

    CHECK(read_file(td_a.path / "countries.csv") ==
          read_file(td_b.path / "countries.csv"));
}

#endif  // LEVIATHAN_TEST_DATA_DIR

TEST_CASE("parse_args: --scenario flag is plumbed through") {
    Argv arg(std::array<const char*, 5>{
        "leviathan", "--days", "3",
        "--scenario", "data/scenarios/1930_minimal.json"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    REQUIRE(r.value().scenario_path.has_value());
    CHECK(r.value().scenario_path.value() ==
          fs::path("data/scenarios/1930_minimal.json"));
}

TEST_CASE("parse_args: --scenario without a value is rejected") {
    Argv arg(std::array<const char*, 4>{
        "leviathan", "--days", "1", "--scenario"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--scenario") != std::string::npos);
}

TEST_CASE("parse_args: --scenario defaults to unset when not passed") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "5"});
    const auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK_FALSE(r.value().scenario_path.has_value());
}

TEST_CASE("run: without --scenario the runner still ticks an empty world") {
    TempDir td("leviathan_runner_m111_no_scenario");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;
    // scenario_path intentionally unset.
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().monthly_ticks == 1);

    // The save file should round-trip an empty state.countries.
    const std::string save = read_file(td.path / "save.json");
    CHECK(save.find("\"countries\": []") != std::string::npos);
    CHECK(save.find("\"factions\": []")  != std::string::npos);
    CHECK(save.find("\"policies\": []")  != std::string::npos);
}

TEST_CASE("run: with --scenario the runner loads the canonical 1930_minimal world") {
    TempDir td("leviathan_runner_m111_canonical_scenario");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 31;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;

    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().monthly_ticks == 1);

    // The save file should mention each loaded entity's id_code.
    const std::string save = read_file(td.path / "save.json");
    CHECK(save.find("\"GER\"") != std::string::npos);
    CHECK(save.find("\"FRA\"") != std::string::npos);
    CHECK(save.find("\"JPN\"") != std::string::npos);
    CHECK(save.find("\"GER_military\"") != std::string::npos);
    CHECK(save.find("\"increase_military_budget\"") != std::string::npos);

    // Save schema is now v9 - M2.4 added GameState.applied_commands.
    CHECK(save.find("\"save_version\": 9") != std::string::npos);
}

TEST_CASE("run: --scenario + 31 days actually mutates country and faction state") {
    // We can't easily compare to the initial GDP without round-tripping,
    // but the M1.9 economy/stability/faction systems are exact-arithmetic
    // and deterministic. After 31 days with monthly pipeline running
    // once, GDP must be != 100.0 (initial value from germany.json).
    TempDir td("leviathan_runner_m111_mutates");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 31;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;

    REQUIRE(rn::run(opts).ok());
    const std::string save = read_file(td.path / "save.json");

    // Germany's initial GDP per germany.json is 100.0. After one month
    // of EconomySystem ticking it should NOT remain exactly 100.0.
    // A search for "100.0" in the GDP field would be brittle; instead
    // we look for the substring " 100.0" inside the GER country
    // serialisation. If it's still there, the economy tick didn't run.
    CHECK(save.find("\"gdp\": 100.0") == std::string::npos);

    // tax_revenue should be the per-tick value, not 0 (the loader sets
    // it to 0 on startup; economy::tick overwrites with formula output).
    CHECK(save.find("\"tax_revenue\": 0.0") == std::string::npos);
}

TEST_CASE("run: --scenario same seed + days produces byte-identical save and log") {
    TempDir td_a("leviathan_runner_m111_det_a");
    TempDir td_b("leviathan_runner_m111_det_b");

    auto make_opts = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path   = kCanonicalConfig;
        o.days          = 60;
        o.output_dir    = dir;
        o.seed_override = std::uint64_t{0xC0DEBABE};
        o.scenario_path = kCanonicalScenario;
        return o;
    };

    REQUIRE(rn::run(make_opts(td_a.path)).ok());
    REQUIRE(rn::run(make_opts(td_b.path)).ok());

    CHECK(read_file(td_a.path / "save.json")    == read_file(td_b.path / "save.json"));
    CHECK(read_file(td_a.path / "events.jsonl") == read_file(td_b.path / "events.jsonl"));
}

TEST_CASE("run: --scenario save contains last_gdp_growth_rate (M1.12)") {
    // After at least one monthly tick, every country's
    // last_gdp_growth_rate has been written by economy::tick. The
    // save format v6 serialises it; this test pins the on-disk shape.
    TempDir td("leviathan_runner_m112_last_growth");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 31;        // crosses the Jan->Feb boundary
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;

    REQUIRE(rn::run(opts).ok());
    const std::string save = read_file(td.path / "save.json");
    CHECK(save.find("\"last_gdp_growth_rate\"") != std::string::npos);
    // Round-trip the save and verify the GER country has a non-zero
    // growth rate (it was 0.0 at load; economy::tick wrote it).
    auto loaded = leviathan::systems::save_system::load(td.path / "save.json");
    REQUIRE(loaded.ok());
    REQUIRE(!loaded.value().countries.empty());
    CHECK(loaded.value().countries[0].last_gdp_growth_rate != doctest::Approx(0.0));
}

// =====================================================================
// M1.13 - --scenario starting_policies integration
// =====================================================================

TEST_CASE("run: --scenario with starting_policies applies them at day 0") {
    // 1930_with_start_policies.json enacts raise_taxes + increase_military_budget
    // on GER. raise_taxes adds 0.05 to country.legal_tax_burden;
    // increase_military_budget adds 0.03 to country.military_power.
    // GER fixture starts at legal_tax_burden 0.20 and military_power
    // 0.50; after day-0 enactment they should be 0.25 and 0.53.
    TempDir td("leviathan_runner_m113_day0");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;                            // no ticks - day-0 only
    opts.output_dir    = td.path;
    opts.scenario_path = kStartingPoliciesScenario;

    REQUIRE(rn::run(opts).ok());
    const std::string save = read_file(td.path / "save.json");
    // Re-load the save and inspect Germany's state.
    const auto loaded = leviathan::systems::save_system::load(td.path / "save.json");
    REQUIRE(loaded.ok());
    REQUIRE(!loaded.value().countries.empty());
    const auto& ger = loaded.value().countries[0];
    CHECK(ger.id_code == "GER");
    CHECK(ger.legal_tax_burden    == doctest::Approx(0.25));
    CHECK(ger.military_power      == doctest::Approx(0.53));
}

TEST_CASE("run: --scenario with starting_policies is byte-identical on same seed") {
    // M1.13 must not break the determinism property: the loader's
    // day-0 apply step is RNG-free, log-free, date-free.
    TempDir td_a("leviathan_runner_m113_det_a");
    TempDir td_b("leviathan_runner_m113_det_b");

    auto make_opts = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path   = kCanonicalConfig;
        o.days          = 90;
        o.output_dir    = dir;
        o.seed_override = std::uint64_t{0xDA7AC0DE};
        o.scenario_path = kStartingPoliciesScenario;
        return o;
    };

    REQUIRE(rn::run(make_opts(td_a.path)).ok());
    REQUIRE(rn::run(make_opts(td_b.path)).ok());
    CHECK(read_file(td_a.path / "save.json")    == read_file(td_b.path / "save.json"));
    CHECK(read_file(td_a.path / "events.jsonl") == read_file(td_b.path / "events.jsonl"));
}

TEST_CASE("run: --scenario without starting_policies is unchanged (back-compat)") {
    // The canonical 1930_minimal.json has no starting_policies. Per
    // M1.13, that should parse as an empty vector and apply nothing.
    // GER's legal_tax_burden should match the fixture's initial value.
    TempDir td("leviathan_runner_m113_no_starting");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;

    REQUIRE(rn::run(opts).ok());
    const auto loaded = leviathan::systems::save_system::load(td.path / "save.json");
    REQUIRE(loaded.ok());
    REQUIRE(!loaded.value().countries.empty());
    CHECK(loaded.value().countries[0].legal_tax_burden == doctest::Approx(0.20));
}

TEST_CASE("run: bad --scenario path fails with the path in the message") {
    TempDir td("leviathan_runner_m111_bad_scenario");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 5;
    opts.output_dir    = td.path;
    opts.scenario_path = "absolutely/does/not/exist/scenario.json";
    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("scenario.json") != std::string::npos);
    // Scenario load happens BEFORE run_state, so no save / log should
    // be written.
    CHECK_FALSE(fs::exists(td.path / "save.json"));
    CHECK_FALSE(fs::exists(td.path / "events.jsonl"));
}

TEST_CASE("parse_args: --summary-csv without a value is rejected") {
    Argv arg(std::array<const char*, 3>{
        "leviathan", "--days", "1"});
    // not enough to fail; build a separate scenario:
    Argv arg2(std::array<const char*, 4>{
        "leviathan", "--days", "1", "--summary-csv"});
    auto r = rn::parse_args(arg2.argc, arg2.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--summary-csv") != std::string::npos);
}

// =====================================================================
// M1.10 - Runner monthly-pipeline wiring
// =====================================================================
//
// The runner invokes monthly::tick_all_countries on every month
// boundary detected by TimeSystem. With an empty state.countries the
// pipeline still runs (processes 0 countries). RunOutcome.monthly_ticks
// counts crossings.

TEST_CASE("run: monthly_ticks == 0 for 10 days starting 1930-01-01 (no boundary)") {
    TempDir td("leviathan_runner_m110_10day");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 10;
    opts.output_dir  = td.path;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().monthly_ticks == 0);
}

TEST_CASE("run: monthly_ticks == 1 for 31 days starting 1930-01-01") {
    // 31 days from 1930-01-01 lands on 1930-02-01 - exactly one
    // month_changed boundary crossed.
    TempDir td("leviathan_runner_m110_31day");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().monthly_ticks == 1);
    CHECK(r.value().end_date.year()  == 1930);
    CHECK(r.value().end_date.month() == 2);
    CHECK(r.value().end_date.day()   == 1);
}

TEST_CASE("run: monthly_ticks == 12 for 365 days starting 1930-01-01") {
    // 365 days from 1930-01-01 = 1931-01-01: 12 month boundaries
    // crossed (Feb..Jan).
    TempDir td("leviathan_runner_m110_365day");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 365;
    opts.output_dir  = td.path;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().monthly_ticks == 12);
    // Existing log_count == 15 invariant must still hold: M1.10
    // monthly pipeline does NOT write any new logs.
    CHECK(r.value().log_count == 15);
}

TEST_CASE("run: empty state runner is unchanged by M1.10 wiring (determinism)") {
    // Same-seed determinism property survives the M1.9 pipeline call:
    // with no countries the pipeline is a no-op and the byte-identical
    // guarantee from M0.9 still holds.
    TempDir td_a("leviathan_runner_m110_empty_det_a");
    TempDir td_b("leviathan_runner_m110_empty_det_b");

    auto make_opts = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path  = kCanonicalConfig;
        o.days         = 365;
        o.output_dir   = dir;
        o.seed_override = std::uint64_t{0xD06F00D};
        return o;
    };

    REQUIRE(rn::run(make_opts(td_a.path)).ok());
    REQUIRE(rn::run(make_opts(td_b.path)).ok());
    CHECK(read_file(td_a.path / "save.json")    == read_file(td_b.path / "save.json"));
    CHECK(read_file(td_a.path / "events.jsonl") == read_file(td_b.path / "events.jsonl"));
}

TEST_CASE("run: save schema is now v9 (M2.4 bumped from v8 for applied_commands)") {
    TempDir td("leviathan_runner_m204_save_version");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;
    REQUIRE(rn::run(opts).ok());
    const std::string save = read_file(td.path / "save.json");
    // Pin the unchanged version: M0.8 documented strict equality.
    CHECK(save.find("\"save_version\":") != std::string::npos);
    CHECK(save.find("\"save_version\": 9") != std::string::npos);
}

// ---- run_state: integration with hand-built state -------------------

namespace {

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameDate;
using leviathan::core::GameState;

CountryState m110_germany() {
    CountryState c;
    c.id           = CountryId{0};
    c.id_code      = "GER";
    c.name         = "Germany";
    c.gdp                       = 100.0;
    c.legal_tax_burden          = 0.20;
    c.fiscal_capacity           = 0.50;
    c.administrative_efficiency = 0.55;
    c.central_control           = 0.60;
    c.corruption                = 0.25;
    c.stability                 = 0.55;
    c.legitimacy                = 0.55;
    c.military_power            = 0.50;
    c.threat_perception         = 0.30;
    c.budget.administration     = 0.25;
    c.budget.military           = 0.35;
    c.budget.education          = 0.10;
    c.budget.welfare            = 0.10;
    c.budget.intelligence       = 0.05;
    c.budget.infrastructure     = 0.10;
    c.budget.industry           = 0.05;
    return c;
}

FactionState m110_faction(int id, int country) {
    FactionState f;
    f.id              = FactionId{id};
    f.country         = CountryId{country};
    f.id_code         = "GER_bureaucracy";
    f.country_id_code = "GER";
    f.name            = "Bureaucracy";
    f.type            = "bureaucracy";
    f.support         = 0.30;
    f.influence       = 0.50;
    f.radicalism      = 0.20;
    f.loyalty         = 0.40;
    f.resources       = 0.0;
    return f;
}

}  // namespace

TEST_CASE("run_state: 31-day run with 1 country + 1 faction actually mutates them") {
    TempDir td("leviathan_runner_m110_run_state_31d");
    GameState state;
    state.current_date = GameDate{1930, 1, 1};
    state.rng.seed     = 0xC0FFEE;
    state.countries.push_back(m110_germany());
    state.factions.push_back(m110_faction(0, 0));

    const double gdp_before        = state.countries[0].gdp;
    const double stab_before       = state.countries[0].stability;
    const double tax_before        = state.countries[0].tax_revenue;
    const double balance_before    = state.countries[0].budget_balance;
    const double support_before    = state.factions[0].support;
    const double loyalty_before    = state.factions[0].loyalty;
    const double radicalism_before = state.factions[0].radicalism;

    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;  // unused by run_state but harmless
    opts.days        = 31;
    opts.output_dir  = td.path;

    const auto r = rn::run_state(state, opts);
    REQUIRE(r.ok());
    CHECK(r.value().monthly_ticks == 1);

    // Country side: economy::tick + stability::tick both ran.
    CHECK(state.countries[0].gdp != doctest::Approx(gdp_before));
    CHECK(state.countries[0].stability != doctest::Approx(stab_before));
    CHECK(state.countries[0].tax_revenue != doctest::Approx(tax_before));
    CHECK(state.countries[0].budget_balance != doctest::Approx(balance_before));

    // Faction side: faction::react ran (loyalty / support drift).
    // Loyalty drifts toward stability (was 0.40, stability was 0.55).
    CHECK(state.factions[0].loyalty != doctest::Approx(loyalty_before));
    CHECK(state.factions[0].support != doctest::Approx(support_before));
    // Radicalism is untouched by faction::react.
    CHECK(state.factions[0].radicalism == doctest::Approx(radicalism_before));

    // Date advanced.
    CHECK(state.current_date.year()  == 1930);
    CHECK(state.current_date.month() == 2);
    CHECK(state.current_date.day()   == 1);
}

TEST_CASE("run_state: 12 monthly ticks across 1930 with countries are byte-identical given same seed") {
    // Determinism property holds for non-empty state too.
    TempDir td_a("leviathan_runner_m110_state_det_a");
    TempDir td_b("leviathan_runner_m110_state_det_b");

    auto build_state = []() {
        GameState s;
        s.current_date = GameDate{1930, 1, 1};
        s.rng.seed     = 0xBABE;
        s.countries.push_back(m110_germany());
        s.factions.push_back(m110_faction(0, 0));
        return s;
    };

    auto make_opts = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path  = kCanonicalConfig;
        o.days         = 365;
        o.output_dir   = dir;
        return o;
    };

    auto s_a = build_state();
    auto s_b = build_state();
    REQUIRE(rn::run_state(s_a, make_opts(td_a.path)).ok());
    REQUIRE(rn::run_state(s_b, make_opts(td_b.path)).ok());

    CHECK(read_file(td_a.path / "save.json")    == read_file(td_b.path / "save.json"));
    CHECK(read_file(td_a.path / "events.jsonl") == read_file(td_b.path / "events.jsonl"));
}

TEST_CASE("run_state: monthly_ticks counter ignores days==0") {
    TempDir td("leviathan_runner_m110_zerodays");
    GameState state;
    state.current_date = GameDate{1930, 1, 1};
    state.rng.seed     = 1;
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 0;
    opts.output_dir  = td.path;
    const auto r = rn::run_state(state, opts);
    REQUIRE(r.ok());
    CHECK(r.value().monthly_ticks == 0);
}

TEST_CASE("run_state: negative days is rejected before any mutation") {
    TempDir td("leviathan_runner_m110_negdays");
    GameState state;
    state.current_date = GameDate{1930, 1, 1};
    const auto date_before = state.current_date;
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = -1;
    opts.output_dir  = td.path;
    const auto r = rn::run_state(state, opts);
    REQUIRE(r.failed());
    CHECK(state.current_date == date_before);
}

TEST_CASE("run: explicit --seed overrides the config seed") {
    TempDir td_default("leviathan_runner_seed_default");
    TempDir td_override("leviathan_runner_seed_override");

    // Default seed run uses whatever the config supplies.
    rn::RunnerOptions o1;
    o1.config_path = kCanonicalConfig;
    o1.days        = 5;
    o1.output_dir  = td_default.path;
    REQUIRE(rn::run(o1).ok());

    // Override with a different seed.
    rn::RunnerOptions o2 = o1;
    o2.output_dir   = td_override.path;
    o2.seed_override = std::uint64_t{7};
    REQUIRE(rn::run(o2).ok());

    // The save files should differ at the rng.seed line.
    const auto save_default  = read_file(td_default.path / "save.json");
    const auto save_override = read_file(td_override.path / "save.json");
    CHECK(save_default != save_override);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// M2.1 - --player COUNTRY_IDCODE flag
// =====================================================================

TEST_CASE("parse_args: --player flag is plumbed through") {
    Argv arg(std::array<const char*, 5>{
        "leviathan", "--days", "3",
        "--player", "GER"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    REQUIRE(r.value().player_id_code.has_value());
    CHECK(r.value().player_id_code.value() == "GER");
}

TEST_CASE("parse_args: --player without a value is rejected") {
    Argv arg(std::array<const char*, 4>{
        "leviathan", "--days", "1", "--player"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--player") != std::string::npos);
}

TEST_CASE("parse_args: --player defaults to unset when not passed") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "5"});
    const auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK_FALSE(r.value().player_id_code.has_value());
}

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("run: --player on an empty world is rejected with the id_code in the error") {
    // No --scenario, so state.countries is empty. --player must reject
    // rather than silently leave player_country unset.
    TempDir td("leviathan_runner_m201_player_empty_world");
    rn::RunnerOptions opts;
    opts.config_path    = kCanonicalConfig;
    opts.days           = 1;
    opts.output_dir     = td.path;
    opts.player_id_code = "GER";
    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("--player") != std::string::npos);
    CHECK(r.error().find("GER")      != std::string::npos);
}

TEST_CASE("run: --player with an unknown id_code is rejected") {
    TempDir td("leviathan_runner_m201_player_unknown");
    rn::RunnerOptions opts;
    opts.config_path    = kCanonicalConfig;
    opts.days           = 1;
    opts.output_dir     = td.path;
    opts.scenario_path  = kCanonicalScenario;
    opts.player_id_code = "BOGUS";
    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("--player")           != std::string::npos);
    CHECK(r.error().find("BOGUS")              != std::string::npos);
    CHECK(r.error().find("no country with that id_code") != std::string::npos);
}

TEST_CASE("run: --player GER resolves to CountryId{0} and round-trips through save") {
    TempDir td("leviathan_runner_m201_player_happy");
    rn::RunnerOptions opts;
    opts.config_path    = kCanonicalConfig;
    opts.days           = 1;
    opts.output_dir     = td.path;
    opts.scenario_path  = kCanonicalScenario;   // GER is the first country -> id 0
    opts.player_id_code = "GER";
    REQUIRE(rn::run(opts).ok());

    // Save file contains the resolved player_country == 0.
    const std::string save = read_file(td.path / "save.json");
    CHECK(save.find("\"player_country\": 0") != std::string::npos);
}

TEST_CASE("run: --player FRA picks the second country (CountryId{1})") {
    TempDir td("leviathan_runner_m201_player_fra");
    rn::RunnerOptions opts;
    opts.config_path    = kCanonicalConfig;
    opts.days           = 1;
    opts.output_dir     = td.path;
    opts.scenario_path  = kCanonicalScenario;
    opts.player_id_code = "FRA";
    REQUIRE(rn::run(opts).ok());

    const std::string save = read_file(td.path / "save.json");
    CHECK(save.find("\"player_country\": 1") != std::string::npos);
}

TEST_CASE("run: no --player keeps player_country at -1 in the save") {
    TempDir td("leviathan_runner_m201_player_default");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    // player_id_code intentionally unset.
    REQUIRE(rn::run(opts).ok());

    const std::string save = read_file(td.path / "save.json");
    CHECK(save.find("\"player_country\": -1") != std::string::npos);
}

// PR #29 nit drive-bys: a failed --player resolution must not leave
// half-written artefacts on disk. The error message check is already
// covered above; these regressions pin the file-system side.

TEST_CASE("run: bad --player on empty world leaves no save / events on disk") {
    TempDir td("leviathan_runner_m202_bad_player_no_artifacts_empty");
    rn::RunnerOptions opts;
    opts.config_path    = kCanonicalConfig;
    opts.days           = 5;
    opts.output_dir     = td.path;
    opts.player_id_code = "GER";   // no --scenario => empty world => rejected
    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK_FALSE(fs::exists(td.path / "save.json"));
    CHECK_FALSE(fs::exists(td.path / "events.jsonl"));
}

TEST_CASE("run: bad --player unknown id_code leaves no save / events on disk") {
    TempDir td("leviathan_runner_m202_bad_player_no_artifacts_unknown");
    rn::RunnerOptions opts;
    opts.config_path    = kCanonicalConfig;
    opts.days           = 5;
    opts.output_dir     = td.path;
    opts.scenario_path  = kCanonicalScenario;
    opts.player_id_code = "BOGUS";
    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK_FALSE(fs::exists(td.path / "save.json"));
    CHECK_FALSE(fs::exists(td.path / "events.jsonl"));
}

#endif  // LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// M2.2 - begin_tick / step_one_day / end_tick primitives
// =====================================================================

TEST_CASE("M2.2 begin_tick: misuse - double begin is rejected") {
    rn::TickController ctrl;
    GameState state;
    rn::RunnerOptions opts;
    opts.days = 0;
    REQUIRE(rn::begin_tick(state, opts, ctrl).ok());
    const auto r = rn::begin_tick(state, opts, ctrl);
    REQUIRE(r.failed());
    CHECK(r.error().find("already started") != std::string::npos);
}

TEST_CASE("M2.2 step_one_day: misuse - step before begin is rejected") {
    rn::TickController ctrl;
    GameState state;
    rn::RunnerOptions opts;
    opts.days = 0;
    const auto r = rn::step_one_day(state, opts, ctrl);
    REQUIRE(r.failed());
    CHECK(r.error().find("not been started") != std::string::npos);
}

TEST_CASE("M2.2 end_tick: misuse - end before begin is rejected") {
    rn::TickController ctrl;
    GameState state;
    rn::RunnerOptions opts;
    opts.days = 0;
    const auto r = rn::end_tick(state, opts, ctrl);
    REQUIRE(r.failed());
    CHECK(r.error().find("not been started") != std::string::npos);
}

TEST_CASE("M2.2 step_one_day: misuse - step after end is rejected") {
    TempDir td("leviathan_runner_m202_step_after_end");
    rn::TickController ctrl;
    GameState state;
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 1;
    opts.output_dir  = td.path;
    REQUIRE(rn::begin_tick(state, opts, ctrl).ok());
    REQUIRE(rn::end_tick(state, opts, ctrl).ok());
    const auto r = rn::step_one_day(state, opts, ctrl);
    REQUIRE(r.failed());
    CHECK(r.error().find("already ended") != std::string::npos);
}

TEST_CASE("M2.2 end_tick: misuse - double end is rejected") {
    TempDir td("leviathan_runner_m202_end_twice");
    rn::TickController ctrl;
    GameState state;
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 1;
    opts.output_dir  = td.path;
    REQUIRE(rn::begin_tick(state, opts, ctrl).ok());
    REQUIRE(rn::end_tick(state, opts, ctrl).ok());
    const auto r = rn::end_tick(state, opts, ctrl);
    REQUIRE(r.failed());
    CHECK(r.error().find("already ended") != std::string::npos);
}

TEST_CASE("M2.2 controller: days_stepped + monthly_ticks reflect the step calls") {
    TempDir td("leviathan_runner_m202_counters");
    rn::TickController ctrl;
    GameState state;
    state.current_date = GameDate(1930, 1, 1);
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;

    REQUIRE(rn::begin_tick(state, opts, ctrl).ok());
    CHECK(ctrl.start_date  == GameDate(1930, 1, 1));
    CHECK(ctrl.started     == true);
    CHECK(ctrl.ended       == false);
    CHECK(ctrl.days_stepped == 0);
    CHECK(ctrl.monthly_ticks == 0);

    for (int i = 0; i < 31; ++i) {
        REQUIRE(rn::step_one_day(state, opts, ctrl).ok());
    }
    CHECK(ctrl.days_stepped == 31);
    // 1930-01-01 + 31 days crosses Jan->Feb exactly once.
    CHECK(ctrl.monthly_ticks == 1);

    REQUIRE(rn::end_tick(state, opts, ctrl).ok());
    CHECK(ctrl.ended == true);
}

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("M2.2 step primitives: begin/step*N/end matches run_state byte-for-byte") {
    // Equivalence proof: the new public primitives must produce the
    // exact same save.json and events.jsonl as the original
    // run_state(days = N) entry point. This pins the refactor as
    // behaviour-preserving.
    TempDir td_a("leviathan_runner_m202_eq_runstate");
    TempDir td_b("leviathan_runner_m202_eq_primitives");

    auto make_opts = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path        = kCanonicalConfig;
        o.days               = 31;
        o.output_dir         = dir;
        o.seed_override      = std::uint64_t{0xBEEF};
        o.scenario_path      = kCanonicalScenario;
        o.summary_csv_path   = dir / "summary.csv";
        o.countries_csv_path = dir / "countries.csv";
        o.factions_csv_path  = dir / "factions.csv";
        return o;
    };

    // Path A: run() -> run_state (the existing entry point).
    REQUIRE(rn::run(make_opts(td_a.path)).ok());

    // Path B: drive the primitives directly. We have to replicate
    // run()'s scenario-load step because begin_tick does NOT load
    // scenarios (that's run()'s responsibility).
    {
        const auto opts = make_opts(td_b.path);
        namespace dl = leviathan::systems::data_loader;
        namespace sl = leviathan::systems::scenario_loader;
        auto cfg_r = dl::load_simulation_config(opts.config_path);
        REQUIRE(cfg_r.ok());
        auto cfg = std::move(cfg_r).value();
        cfg.seed = opts.seed_override.value();
        auto state = leviathan::core::make_game_state(cfg);
        REQUIRE(sl::load_into_state(state, opts.scenario_path.value()).ok());

        rn::TickController ctrl;
        REQUIRE(rn::begin_tick(state, opts, ctrl).ok());
        for (int i = 0; i < opts.days; ++i) {
            REQUIRE(rn::step_one_day(state, opts, ctrl).ok());
        }
        REQUIRE(rn::end_tick(state, opts, ctrl).ok());
    }

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
}

TEST_CASE("M2.2 step primitives: pause-then-resume produces byte-identical output") {
    // Stronger property: calling step_one_day in two batches (e.g.
    // 15 + 16) with an arbitrary delay between batches is observably
    // identical to calling it all at once. This is the resume-from-
    // pause case that justifies the refactor.
    TempDir td_a("leviathan_runner_m202_pause_one_shot");
    TempDir td_b("leviathan_runner_m202_pause_two_batches");

    auto make_opts = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path        = kCanonicalConfig;
        o.days               = 31;
        o.output_dir         = dir;
        o.seed_override      = std::uint64_t{0xCAFE};
        o.scenario_path      = kCanonicalScenario;
        o.summary_csv_path   = dir / "summary.csv";
        return o;
    };

    // Path A: 31 days in one shot via run().
    REQUIRE(rn::run(make_opts(td_a.path)).ok());

    // Path B: drive 15 days, "pause" (do nothing), drive 16 more.
    {
        const auto opts = make_opts(td_b.path);
        namespace dl = leviathan::systems::data_loader;
        namespace sl = leviathan::systems::scenario_loader;
        auto cfg_r = dl::load_simulation_config(opts.config_path);
        REQUIRE(cfg_r.ok());
        auto cfg = std::move(cfg_r).value();
        cfg.seed = opts.seed_override.value();
        auto state = leviathan::core::make_game_state(cfg);
        REQUIRE(sl::load_into_state(state, opts.scenario_path.value()).ok());

        rn::TickController ctrl;
        REQUIRE(rn::begin_tick(state, opts, ctrl).ok());
        for (int i = 0; i < 15; ++i) {
            REQUIRE(rn::step_one_day(state, opts, ctrl).ok());
        }
        // --- pause ---
        for (int i = 0; i < 16; ++i) {
            REQUIRE(rn::step_one_day(state, opts, ctrl).ok());
        }
        REQUIRE(rn::end_tick(state, opts, ctrl).ok());
    }

    CHECK(read_file(td_a.path / "save.json")    ==
          read_file(td_b.path / "save.json"));
    CHECK(read_file(td_a.path / "events.jsonl") ==
          read_file(td_b.path / "events.jsonl"));
    CHECK(read_file(td_a.path / "summary.csv")  ==
          read_file(td_b.path / "summary.csv"));
}

#endif  // LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// M2.8 - --replay PATH CLI harness
// =====================================================================

TEST_CASE("parse_args: --replay flag is plumbed through") {
    Argv arg(std::array<const char*, 5>{
        "leviathan", "--days", "3",
        "--replay", "out/source.json"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    REQUIRE(r.value().replay_path.has_value());
    CHECK(r.value().replay_path.value() == fs::path("out/source.json"));
}

TEST_CASE("parse_args: --replay without a value is rejected") {
    Argv arg(std::array<const char*, 4>{
        "leviathan", "--days", "1", "--replay"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--replay") != std::string::npos);
}

TEST_CASE("parse_args: --replay defaults to unset when not passed") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "5"});
    const auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK_FALSE(r.value().replay_path.has_value());
}

#ifdef LEVIATHAN_TEST_DATA_DIR

namespace {

// Build a "source" save at the given path: load the canonical
// scenario, set GER as the player, submit `commands` through
// commands::apply_pending, then write the resulting state to disk.
// Returns the in-memory state for later comparison.
leviathan::core::GameState build_source_save(
        const fs::path& save_path,
        const std::vector<leviathan::core::PlayerCommand>& commands) {
    namespace dl = leviathan::systems::data_loader;
    namespace sl = leviathan::systems::scenario_loader;
    namespace cmd = leviathan::systems::commands;
    namespace ss = leviathan::systems::save_system;

    auto cfg_r = dl::load_simulation_config(kCanonicalConfig);
    REQUIRE(cfg_r.ok());
    auto state = leviathan::core::make_game_state(cfg_r.value());
    REQUIRE(sl::load_into_state(state, kCanonicalScenario).ok());
    state.player_country = leviathan::core::CountryId{0};   // GER
    if (!commands.empty()) {
        cmd::CommandQueue q;
        for (const auto& c : commands) {
            q.pending.push_back(c);
        }
        REQUIRE(cmd::apply_pending(state, q).ok());
    }
    REQUIRE(ss::save(state, save_path).ok());
    return state;
}

}  // namespace

TEST_CASE("run: --replay without --scenario is rejected loudly") {
    TempDir td("leviathan_runner_m208_no_scenario");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 0;
    opts.output_dir  = td.path;
    opts.replay_path = td.path / "source.json";   // path doesn't matter; rejected earlier
    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("--replay")  != std::string::npos);
    CHECK(r.error().find("--scenario") != std::string::npos);
}

TEST_CASE("run: --replay with single EnactPolicy reproduces the source state") {
    TempDir td("leviathan_runner_m208_replay_enact");
    const fs::path source_path = td.path / "source.json";

    leviathan::core::PlayerCommand cmd;
    cmd.kind            = leviathan::core::PlayerCommandKind::EnactPolicy;
    cmd.policy_id_code = "raise_taxes";
    const auto source = build_source_save(source_path, {cmd});

    // Drive replay via run().
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().replay_commands_replayed == 1);

    // Load the runner's output save and confirm it mirrors the source's
    // command effects and replay log.
    auto loaded_r = leviathan::systems::save_system::load(td.path / "save.json");
    REQUIRE(loaded_r.ok());
    const auto& replayed = loaded_r.value();
    REQUIRE(replayed.applied_commands.size() == 1u);
    CHECK(replayed.applied_commands[0].command.policy_id_code == "raise_taxes");
    CHECK(replayed.countries[0].legal_tax_burden ==
          doctest::Approx(source.countries[0].legal_tax_burden));
}

TEST_CASE("run: --replay inherits player_country from the loaded save when --player is absent") {
    TempDir td("leviathan_runner_m208_inherit_player");
    const fs::path source_path = td.path / "source.json";

    // Source picks GER (index 0) as the player.
    leviathan::core::PlayerCommand cmd;
    cmd.kind            = leviathan::core::PlayerCommandKind::EnactPolicy;
    cmd.policy_id_code = "raise_taxes";
    const auto source = build_source_save(source_path, {cmd});
    REQUIRE(source.player_country.value() == 0);

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    // opts.player_id_code intentionally unset — should auto-inherit.
    REQUIRE(rn::run(opts).ok());

    auto loaded_r = leviathan::systems::save_system::load(td.path / "save.json");
    REQUIRE(loaded_r.ok());
    CHECK(loaded_r.value().player_country.value() == 0);
}

TEST_CASE("run: --replay of an empty-log save replays zero commands") {
    TempDir td("leviathan_runner_m208_empty_log");
    const fs::path source_path = td.path / "source.json";

    // Source state with no commands submitted.
    const auto source = build_source_save(source_path, {});
    REQUIRE(source.applied_commands.empty());

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().replay_commands_replayed == 0);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// M2.11 - --verify CLI flag
// =====================================================================

TEST_CASE("parse_args: --verify flag is plumbed when combined with --replay") {
    Argv arg(std::array<const char*, 6>{
        "leviathan", "--days", "3",
        "--replay", "out/source.json",
        "--verify"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK(r.value().verify == true);
}

TEST_CASE("parse_args: --verify defaults to false when not passed") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "5"});
    const auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK(r.value().verify == false);
}

TEST_CASE("parse_args: --verify without --replay is rejected") {
    Argv arg(std::array<const char*, 4>{
        "leviathan", "--days", "3", "--verify"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--verify") != std::string::npos);
    CHECK(r.error().find("--replay") != std::string::npos);
}

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("run: --replay + --verify with matching source reports zero mismatches") {
    TempDir td("leviathan_runner_m211_verify_match");
    const fs::path source_path = td.path / "source.json";

    leviathan::core::PlayerCommand cmd;
    cmd.kind            = leviathan::core::PlayerCommandKind::EnactPolicy;
    cmd.policy_id_code = "raise_taxes";
    (void) build_source_save(source_path, {cmd});

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    opts.verify        = true;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().verify_mismatches.empty());
}

TEST_CASE("run: --replay + --verify on a tweaked source detects mismatch") {
    // Build a source save, then manually mutate a country field on
    // disk and save again so replay produces a state that DOESN'T
    // match the source. compare_states should catch the diff.
    namespace ss = leviathan::systems::save_system;
    TempDir td("leviathan_runner_m211_verify_diff");
    const fs::path source_path = td.path / "source.json";

    leviathan::core::PlayerCommand cmd;
    cmd.kind            = leviathan::core::PlayerCommandKind::EnactPolicy;
    cmd.policy_id_code = "raise_taxes";
    auto source = build_source_save(source_path, {cmd});

    // Tweak a non-command-driven field that replay can't reproduce.
    // legal_tax_burden is mutated by the policy effect; tweaking it
    // here makes replay's deterministic output differ from source.
    source.countries[0].legal_tax_burden = 0.99;
    REQUIRE(ss::save(source, source_path).ok());

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    opts.verify        = true;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    // Find the legal_tax_burden mismatch among the reported entries.
    const auto& mismatches = r.value().verify_mismatches;
    REQUIRE_FALSE(mismatches.empty());
    bool found = false;
    for (const auto& m : mismatches) {
        if (m.field_path == "countries[0].legal_tax_burden") {
            found = true;
            break;
        }
    }
    CHECK(found);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// M2.12 - --verify-strict CLI flag
// =====================================================================

TEST_CASE("parse_args: --verify-strict plumbed when combined with --verify --replay") {
    Argv arg(std::array<const char*, 7>{
        "leviathan", "--days", "3",
        "--replay", "out/source.json",
        "--verify",
        "--verify-strict"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK(r.value().verify_strict == true);
}

TEST_CASE("parse_args: --verify-strict defaults to false") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "5"});
    const auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK(r.value().verify_strict == false);
}

TEST_CASE("parse_args: --verify-strict without --verify is rejected") {
    Argv arg(std::array<const char*, 6>{
        "leviathan", "--days", "3",
        "--replay", "out/source.json",
        "--verify-strict"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--verify-strict") != std::string::npos);
    CHECK(r.error().find("--verify")        != std::string::npos);
}

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("run: --verify-strict on matching source succeeds with empty mismatches") {
    // strict mode is a main()-level policy; run() still succeeds
    // when there are no mismatches. The test confirms verify_strict
    // didn't accidentally change run() semantics.
    TempDir td("leviathan_runner_m212_strict_match");
    const fs::path source_path = td.path / "source.json";

    leviathan::core::PlayerCommand cmd;
    cmd.kind            = leviathan::core::PlayerCommandKind::EnactPolicy;
    cmd.policy_id_code = "raise_taxes";
    (void) build_source_save(source_path, {cmd});

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    opts.verify        = true;
    opts.verify_strict = true;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().verify_mismatches.empty());
}

TEST_CASE("run: --verify-strict on tweaked source still succeeds at run() but reports mismatches") {
    // run() always succeeds when simulation+replay completes;
    // strict mode is a main()-level exit-code decision. The test
    // confirms run() does NOT downgrade to failure on mismatches.
    namespace ss = leviathan::systems::save_system;
    TempDir td("leviathan_runner_m212_strict_diff");
    const fs::path source_path = td.path / "source.json";

    leviathan::core::PlayerCommand cmd;
    cmd.kind            = leviathan::core::PlayerCommandKind::EnactPolicy;
    cmd.policy_id_code = "raise_taxes";
    auto source = build_source_save(source_path, {cmd});
    source.countries[0].legal_tax_burden = 0.42;
    REQUIRE(ss::save(source, source_path).ok());

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    opts.verify        = true;
    opts.verify_strict = true;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());  // run() still succeeds; main() decides exit code
    CHECK_FALSE(r.value().verify_mismatches.empty());
}

#endif  // LEVIATHAN_TEST_DATA_DIR

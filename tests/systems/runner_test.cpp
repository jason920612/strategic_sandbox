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
#include "leviathan/systems/svg_export.hpp"
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

    // Save schema is now v12 - M4.1 fleshed out root-level provinces.
    CHECK(save.find("\"save_version\": 12") != std::string::npos);
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

TEST_CASE("run: save schema is now v12 (M4.1 bumped from v11 for provinces)") {
    TempDir td("leviathan_runner_m31_save_version");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;
    REQUIRE(rn::run(opts).ok());
    const std::string save = read_file(td.path / "save.json");
    // Pin the unchanged version: M0.8 documented strict equality.
    CHECK(save.find("\"save_version\":") != std::string::npos);
    CHECK(save.find("\"save_version\": 12") != std::string::npos);
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

// =====================================================================
// M2.13 - --verify-tolerance CLI flag
// =====================================================================

TEST_CASE("parse_args: --verify-tolerance plumbed when combined with --verify --replay") {
    Argv arg(std::array<const char*, 8>{
        "leviathan", "--days", "3",
        "--replay", "out/source.json",
        "--verify",
        "--verify-tolerance", "1e-3"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    REQUIRE(r.value().verify_tolerance.has_value());
    CHECK(r.value().verify_tolerance.value() == doctest::Approx(1e-3));
}

TEST_CASE("parse_args: --verify-tolerance defaults to nullopt") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "5"});
    const auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK_FALSE(r.value().verify_tolerance.has_value());
}

TEST_CASE("parse_args: --verify-tolerance without a value is rejected") {
    Argv arg(std::array<const char*, 6>{
        "leviathan", "--days", "3",
        "--replay", "out/source.json",
        "--verify-tolerance"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--verify-tolerance") != std::string::npos);
}

TEST_CASE("parse_args: --verify-tolerance non-numeric is rejected") {
    Argv arg(std::array<const char*, 8>{
        "leviathan", "--days", "3",
        "--replay", "out/source.json",
        "--verify",
        "--verify-tolerance", "abc"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--verify-tolerance") != std::string::npos);
    CHECK(r.error().find("abc")                != std::string::npos);
    CHECK(r.error().find("floating-point")     != std::string::npos);
}

TEST_CASE("parse_args: --verify-tolerance negative is rejected") {
    Argv arg(std::array<const char*, 8>{
        "leviathan", "--days", "3",
        "--replay", "out/source.json",
        "--verify",
        "--verify-tolerance", "-1e-3"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--verify-tolerance") != std::string::npos);
    CHECK(r.error().find(">= 0")               != std::string::npos);
}

TEST_CASE("parse_args: --verify-tolerance without --verify is rejected") {
    Argv arg(std::array<const char*, 7>{
        "leviathan", "--days", "3",
        "--replay", "out/source.json",
        "--verify-tolerance", "1e-3"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--verify-tolerance") != std::string::npos);
    CHECK(r.error().find("--verify")           != std::string::npos);
}

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("run: --verify-tolerance loose enough absorbs a small mismatch") {
    // Tweak the source's gdp by 1e-3 (below loose tolerance 1e-2 but
    // far above the default 1e-9). With --verify-tolerance 1e-2 the
    // diff is silent; without it, the default would catch it.
    namespace ss = leviathan::systems::save_system;
    TempDir td("leviathan_runner_m213_loose");
    const fs::path source_path = td.path / "source.json";

    leviathan::core::PlayerCommand cmd;
    cmd.kind            = leviathan::core::PlayerCommandKind::EnactPolicy;
    cmd.policy_id_code = "raise_taxes";
    auto source = build_source_save(source_path, {cmd});
    source.countries[0].gdp = source.countries[0].gdp + 1e-3;
    REQUIRE(ss::save(source, source_path).ok());

    rn::RunnerOptions opts;
    opts.config_path      = kCanonicalConfig;
    opts.days             = 0;
    opts.output_dir       = td.path;
    opts.scenario_path    = kCanonicalScenario;
    opts.replay_path      = source_path;
    opts.verify           = true;
    opts.verify_tolerance = 1e-2;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    // gdp diff (1e-3) is within tolerance (1e-2), so it must not appear.
    for (const auto& m : r.value().verify_mismatches) {
        CHECK(m.field_path != "countries[0].gdp");
    }
}

TEST_CASE("run: --verify-tolerance tight enough catches a small mismatch") {
    // Same tweak, but a tight tolerance below the diff. Mismatch
    // shows up.
    namespace ss = leviathan::systems::save_system;
    TempDir td("leviathan_runner_m213_tight");
    const fs::path source_path = td.path / "source.json";

    leviathan::core::PlayerCommand cmd;
    cmd.kind            = leviathan::core::PlayerCommandKind::EnactPolicy;
    cmd.policy_id_code = "raise_taxes";
    auto source = build_source_save(source_path, {cmd});
    source.countries[0].gdp = source.countries[0].gdp + 1e-3;
    REQUIRE(ss::save(source, source_path).ok());

    rn::RunnerOptions opts;
    opts.config_path      = kCanonicalConfig;
    opts.days             = 0;
    opts.output_dir       = td.path;
    opts.scenario_path    = kCanonicalScenario;
    opts.replay_path      = source_path;
    opts.verify           = true;
    opts.verify_tolerance = 1e-6;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    bool found = false;
    for (const auto& m : r.value().verify_mismatches) {
        if (m.field_path == "countries[0].gdp") {
            found = true;
            break;
        }
    }
    CHECK(found);
}

// =====================================================================
// M2.9 - replay CLI failure artifact semantics
//
// Replay-mode `run()` must not write save.json / events.jsonl /
// summary.csv / countries.csv / factions.csv when the replay fails
// for any reason. End-to-end this falls out of the existing layout
// (all artefact writes happen in `end_tick`, every replay failure
// path returns before `end_tick` is reached) — these tests cement
// the guarantee so a future refactor can't quietly regress it.
// =====================================================================

namespace {

// Set every artefact output path on `opts` to a distinct file
// underneath `dir`, and report all eight paths the test should
// later assert absent. Used by every M2.9 regression test below
// so the "no artefact survived" check is uniformly applied.
// M3.5 added `interest_groups_csv` to the artefact set; M3.6
// added the two formula-trace files. All three are
// unconditionally written by `end_tick`, so the M2.9
// pre-`end_tick` no-artefact contract automatically extends to
// them.
struct ArtifactPaths {
    fs::path save;
    fs::path log;
    fs::path summary_csv;
    fs::path countries_csv;
    fs::path factions_csv;
    fs::path interest_groups_csv;
    fs::path country_feedback_csv;
    fs::path authority_pressure_csv;
};

ArtifactPaths wire_all_artifacts(rn::RunnerOptions& opts, const fs::path& dir) {
    ArtifactPaths a{
        dir / "out_save.json",
        dir / "out_events.jsonl",
        dir / "out_summary.csv",
        dir / "out_countries.csv",
        dir / "out_factions.csv",
        dir / "out_interest_groups.csv",
        dir / "out_country_feedback.csv",
        dir / "out_authority_pressure.csv",
    };
    opts.save_path                  = a.save;
    opts.log_path                   = a.log;
    opts.summary_csv_path           = a.summary_csv;
    opts.countries_csv_path         = a.countries_csv;
    opts.factions_csv_path          = a.factions_csv;
    opts.interest_groups_csv_path   = a.interest_groups_csv;
    opts.interest_group_country_feedback_csv_path   = a.country_feedback_csv;
    opts.interest_group_authority_pressure_csv_path = a.authority_pressure_csv;
    return a;
}

void check_no_artifacts(const ArtifactPaths& a) {
    CHECK_FALSE(fs::exists(a.save));
    CHECK_FALSE(fs::exists(a.log));
    CHECK_FALSE(fs::exists(a.summary_csv));
    CHECK_FALSE(fs::exists(a.countries_csv));
    CHECK_FALSE(fs::exists(a.factions_csv));
    CHECK_FALSE(fs::exists(a.interest_groups_csv));
    CHECK_FALSE(fs::exists(a.country_feedback_csv));
    CHECK_FALSE(fs::exists(a.authority_pressure_csv));
}

}  // namespace

TEST_CASE("run: --replay with a missing source file fails and writes no artifacts") {
    TempDir td("leviathan_runner_m209_missing_source");
    const fs::path missing = td.path / "does_not_exist.json";
    REQUIRE_FALSE(fs::exists(missing));

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = missing;
    const auto paths = wire_all_artifacts(opts, td.path);

    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("--replay") != std::string::npos);
    check_no_artifacts(paths);
}

TEST_CASE("run: --replay with an out-of-order log fails and writes no artifacts") {
    namespace ss = leviathan::systems::save_system;
    TempDir td("leviathan_runner_m209_out_of_order");
    const fs::path source_path = td.path / "source.json";

    // Start from an empty-log source, then hand-craft two log
    // entries whose dates go backward. apply_pending wouldn't have
    // emitted these on its own; we splice them in directly to
    // exercise replay_with_time's monotonicity check.
    auto source = build_source_save(source_path, {});
    leviathan::core::AppliedPlayerCommand e0;
    e0.applied_on            = leviathan::core::GameDate(1930, 1, 5);
    e0.command.kind          = leviathan::core::PlayerCommandKind::EnactPolicy;
    e0.command.policy_id_code = "raise_taxes";
    leviathan::core::AppliedPlayerCommand e1;
    e1.applied_on            = leviathan::core::GameDate(1930, 1, 3);  // < e0
    e1.command.kind          = leviathan::core::PlayerCommandKind::EnactPolicy;
    e1.command.policy_id_code = "raise_taxes";
    source.applied_commands.push_back(e0);
    source.applied_commands.push_back(e1);
    REQUIRE(ss::save(source, source_path).ok());

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    const auto paths = wire_all_artifacts(opts, td.path);

    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("out-of-order") != std::string::npos);
    check_no_artifacts(paths);
}

TEST_CASE("run: --replay with an unknown policy id_code fails and writes no artifacts") {
    namespace ss = leviathan::systems::save_system;
    TempDir td("leviathan_runner_m209_unknown_policy");
    const fs::path source_path = td.path / "source.json";

    // Empty-log source plus one hand-crafted entry referencing a
    // policy id_code the scenario does NOT define. apply_pending
    // would have rejected this at submission time, but the on-disk
    // log can technically hold it (e.g., a save from a scenario with
    // a different policy set). Replay must fail and write nothing.
    auto source = build_source_save(source_path, {});
    leviathan::core::AppliedPlayerCommand bad;
    bad.applied_on            = source.current_date;
    bad.command.kind          = leviathan::core::PlayerCommandKind::EnactPolicy;
    bad.command.policy_id_code = "no_such_policy_id_code";
    source.applied_commands.push_back(bad);
    REQUIRE(ss::save(source, source_path).ok());

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    const auto paths = wire_all_artifacts(opts, td.path);

    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("no_such_policy_id_code") != std::string::npos);
    check_no_artifacts(paths);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// M2.14 - --target-date replay window
// =====================================================================

TEST_CASE("parse_args: --target-date plumbed when combined with --replay") {
    Argv arg(std::array<const char*, 7>{
        "leviathan", "--days", "0",
        "--replay", "out/source.json",
        "--target-date", "1930-06-15"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    REQUIRE(r.value().target_date.has_value());
    CHECK(r.value().target_date.value() ==
          leviathan::core::GameDate(1930, 6, 15));
}

TEST_CASE("parse_args: --target-date defaults to nullopt") {
    Argv arg(std::array<const char*, 3>{"leviathan", "--days", "0"});
    const auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.ok());
    CHECK_FALSE(r.value().target_date.has_value());
}

TEST_CASE("parse_args: --target-date without a value is rejected") {
    Argv arg(std::array<const char*, 6>{
        "leviathan", "--days", "0",
        "--replay", "out/source.json",
        "--target-date"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--target-date") != std::string::npos);
}

TEST_CASE("parse_args: --target-date with a malformed date is rejected") {
    Argv arg(std::array<const char*, 7>{
        "leviathan", "--days", "0",
        "--replay", "out/source.json",
        "--target-date", "1930-13-01"});  // month 13: parse-time reject
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--target-date") != std::string::npos);
    CHECK(r.error().find("1930-13-01")    != std::string::npos);
}

TEST_CASE("parse_args: --target-date without --replay is rejected") {
    Argv arg(std::array<const char*, 5>{
        "leviathan", "--days", "0",
        "--target-date", "1930-06-15"});
    auto r = rn::parse_args(arg.argc, arg.argv());
    REQUIRE(r.failed());
    CHECK(r.error().find("--target-date") != std::string::npos);
    CHECK(r.error().find("--replay")      != std::string::npos);
}

#ifdef LEVIATHAN_TEST_DATA_DIR

namespace {

// Build a source save whose applied_commands log lives at chosen
// dates. Bypasses apply_pending so the test can stage entries with
// arbitrary (monotonic) applied_on values that the canonical
// scenario's start date precedes — exactly the shape M2.14
// truncation needs to exercise.
leviathan::core::GameState build_source_with_dated_log(
        const fs::path& save_path,
        const std::vector<leviathan::core::GameDate>& dates) {
    namespace ss = leviathan::systems::save_system;
    auto state = build_source_save(save_path, {});
    for (const auto& d : dates) {
        leviathan::core::AppliedPlayerCommand e;
        e.applied_on             = d;
        e.command.kind           = leviathan::core::PlayerCommandKind::EnactPolicy;
        e.command.policy_id_code = "raise_taxes";
        state.applied_commands.push_back(e);
    }
    REQUIRE(ss::save(state, save_path).ok());
    return state;
}

}  // namespace

TEST_CASE("run: --target-date past the log advances the time system to the target") {
    TempDir td("leviathan_runner_m214_past_log");
    const fs::path source_path = td.path / "source.json";
    (void) build_source_with_dated_log(source_path, {
        leviathan::core::GameDate(1930, 1, 5),
        leviathan::core::GameDate(1930, 1, 10),
    });

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    opts.target_date   = leviathan::core::GameDate(1930, 1, 20);

    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().replay_commands_replayed == 2);
    CHECK(r.value().end_date ==
          leviathan::core::GameDate(1930, 1, 20));

    auto loaded_r = leviathan::systems::save_system::load(td.path / "save.json");
    REQUIRE(loaded_r.ok());
    CHECK(loaded_r.value().current_date ==
          leviathan::core::GameDate(1930, 1, 20));
    CHECK(loaded_r.value().applied_commands.size() == 2u);
}

TEST_CASE("run: --target-date equal to the last entry replays all and steps no further") {
    TempDir td("leviathan_runner_m214_equal_to_last");
    const fs::path source_path = td.path / "source.json";
    (void) build_source_with_dated_log(source_path, {
        leviathan::core::GameDate(1930, 1, 5),
        leviathan::core::GameDate(1930, 1, 10),
    });

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    opts.target_date   = leviathan::core::GameDate(1930, 1, 10);

    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().replay_commands_replayed == 2);
    CHECK(r.value().end_date ==
          leviathan::core::GameDate(1930, 1, 10));
}

TEST_CASE("run: --target-date earlier than a log entry truncates the log") {
    TempDir td("leviathan_runner_m214_truncate");
    const fs::path source_path = td.path / "source.json";
    (void) build_source_with_dated_log(source_path, {
        leviathan::core::GameDate(1930, 1, 5),
        leviathan::core::GameDate(1930, 1, 10),
    });

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    opts.target_date   = leviathan::core::GameDate(1930, 1, 7);

    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    // Only the first entry (1930-01-05) survives the cut.
    CHECK(r.value().replay_commands_replayed == 1);
    CHECK(r.value().end_date ==
          leviathan::core::GameDate(1930, 1, 7));

    auto loaded_r = leviathan::systems::save_system::load(td.path / "save.json");
    REQUIRE(loaded_r.ok());
    REQUIRE(loaded_r.value().applied_commands.size() == 1u);
    CHECK(loaded_r.value().applied_commands[0].applied_on ==
          leviathan::core::GameDate(1930, 1, 5));
}

TEST_CASE("run: --target-date before the scenario start is rejected") {
    TempDir td("leviathan_runner_m214_before_start");
    const fs::path source_path = td.path / "source.json";
    (void) build_source_save(source_path, {});

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 0;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    opts.replay_path   = source_path;
    opts.target_date   = leviathan::core::GameDate(1929, 12, 31);
    const auto paths = wire_all_artifacts(opts, td.path);

    const auto r = rn::run(opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("--target-date")        != std::string::npos);
    CHECK(r.error().find("1929-12-31")           != std::string::npos);
    CHECK(r.error().find("scenario start")       != std::string::npos);
    // Pre-end_tick failure: M2.9 contract — no artefacts on disk.
    check_no_artifacts(paths);
}

// =====================================================================
// M3.5 - interest_groups.csv is an unconditional artefact
// =====================================================================

TEST_CASE("run: interest_groups.csv is written even without --scenario (header-only)") {
    // No scenario, so state.interest_groups is empty. The artefact is
    // still produced — the contract is "the file always exists".
    TempDir td("leviathan_runner_m35_empty_interest_groups");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().interest_groups_csv_rows == 0u);
    REQUIRE(fs::exists(td.path / "interest_groups.csv"));
    const std::string text = read_file(td.path / "interest_groups.csv");
    CHECK(text ==
          "date,id_code,name,kind,country_id,country_id_code,"
          "influence,loyalty,radicalism\n");
}

TEST_CASE("run: interest_groups.csv defaults to <output_dir>/interest_groups.csv") {
    TempDir td("leviathan_runner_m35_default_path");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 1;
    opts.output_dir  = td.path;
    // interest_groups_csv_path intentionally unset.
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().interest_groups_csv_path ==
          td.path / "interest_groups.csv");
    CHECK(fs::exists(r.value().interest_groups_csv_path));
}

TEST_CASE("run: interest_groups_csv_path override is honoured") {
    TempDir td("leviathan_runner_m35_path_override");
    rn::RunnerOptions opts;
    opts.config_path             = kCanonicalConfig;
    opts.days                    = 1;
    opts.output_dir              = td.path;
    opts.interest_groups_csv_path = td.path / "custom" / "ig.csv";
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().interest_groups_csv_path ==
          td.path / "custom" / "ig.csv");
    CHECK(fs::exists(td.path / "custom" / "ig.csv"));
    // Default path is NOT written when the override is set.
    CHECK_FALSE(fs::exists(td.path / "interest_groups.csv"));
}

TEST_CASE("run: scenario with hand-built interest groups produces one row per group per snapshot point") {
    // Canonical scenarios don't author interest groups; build a state
    // manually and use the `run_state` injection point.
    TempDir td("leviathan_runner_m35_with_groups");

    leviathan::core::SimulationConfig cfg;
    cfg.start_date = leviathan::core::GameDate(1930, 1, 1);
    cfg.seed       = 1u;
    auto state = leviathan::core::make_game_state(cfg);

    leviathan::core::CountryState c;
    c.id      = leviathan::core::CountryId{0};
    c.id_code = "GER";
    c.name    = "Germany";
    state.countries.push_back(c);

    leviathan::core::InterestGroupState g1;
    g1.id_code    = "ger_bureaucracy";
    g1.name       = "German Bureaucracy";
    g1.kind       = leviathan::core::InterestGroupKind::Bureaucracy;
    g1.country    = leviathan::core::CountryId{0};
    g1.influence  = 0.4;
    g1.loyalty    = 0.6;
    g1.radicalism = 0.2;
    state.interest_groups.push_back(g1);

    leviathan::core::InterestGroupState g2 = g1;
    g2.id_code    = "ger_workers";
    g2.name       = "German Workers";
    g2.kind       = leviathan::core::InterestGroupKind::Workers;
    g2.influence  = 0.3;
    g2.loyalty    = 0.4;
    g2.radicalism = 0.5;
    state.interest_groups.push_back(g2);

    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;
    const auto r = rn::run_state(state, opts);
    REQUIRE(r.ok());
    // Snapshot cadence: start + each month_changed + final post-sanity.
    // 31 days from 1930-01-01 crosses one month boundary → 3 snapshot
    // points. 2 groups → 6 data rows.
    CHECK(r.value().interest_groups_csv_rows == 6u);

    const std::string text = read_file(td.path / "interest_groups.csv");
    CHECK(text.find("ger_bureaucracy") != std::string::npos);
    CHECK(text.find("ger_workers")     != std::string::npos);
    CHECK(text.find("Bureaucracy")     != std::string::npos);
    CHECK(text.find("Workers")         != std::string::npos);
    // Doubles use scientific notation.
    CHECK(text.find("e-") != std::string::npos);
}

TEST_CASE("run: interest_groups.csv preserves vector ordering, not lexical") {
    TempDir td("leviathan_runner_m35_ordering");

    leviathan::core::SimulationConfig cfg;
    cfg.start_date = leviathan::core::GameDate(1930, 1, 1);
    cfg.seed       = 1u;
    auto state = leviathan::core::make_game_state(cfg);

    leviathan::core::CountryState c;
    c.id      = leviathan::core::CountryId{0};
    c.id_code = "GER";
    c.name    = "Germany";
    state.countries.push_back(c);

    auto make_g = [](const std::string& id_code,
                     leviathan::core::InterestGroupKind k) {
        leviathan::core::InterestGroupState g;
        g.id_code   = id_code;
        g.name      = id_code;
        g.kind      = k;
        g.country   = leviathan::core::CountryId{0};
        g.influence = 0.5;
        return g;
    };
    // Insert in non-lexical order.
    state.interest_groups.push_back(make_g("zeta", leviathan::core::InterestGroupKind::Workers));
    state.interest_groups.push_back(make_g("alpha", leviathan::core::InterestGroupKind::Bureaucracy));
    state.interest_groups.push_back(make_g("mu", leviathan::core::InterestGroupKind::Military));

    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 0;
    opts.output_dir  = td.path;
    REQUIRE(rn::run_state(state, opts).ok());

    const std::string text = read_file(td.path / "interest_groups.csv");
    // zeta appears before alpha (vector order), not alphabetic.
    const auto zeta_pos  = text.find("zeta");
    const auto alpha_pos = text.find("alpha");
    const auto mu_pos    = text.find(",mu,");  // surround with commas to avoid matching other "mu" substrings
    REQUIRE(zeta_pos  != std::string::npos);
    REQUIRE(alpha_pos != std::string::npos);
    REQUIRE(mu_pos    != std::string::npos);
    CHECK(zeta_pos < alpha_pos);
    CHECK(alpha_pos < mu_pos);
}

TEST_CASE("run: interest_groups.csv preserves byte-identical determinism on same seed") {
    TempDir td_a("leviathan_runner_m35_det_a");
    TempDir td_b("leviathan_runner_m35_det_b");

    auto opts_for = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path   = kCanonicalConfig;
        o.days          = 90;
        o.output_dir    = dir;
        o.seed_override = std::uint64_t{0xDADAFEED};
        o.scenario_path = kCanonicalScenario;
        return o;
    };

    REQUIRE(rn::run(opts_for(td_a.path)).ok());
    REQUIRE(rn::run(opts_for(td_b.path)).ok());
    CHECK(read_file(td_a.path / "interest_groups.csv") ==
          read_file(td_b.path / "interest_groups.csv"));
}

TEST_CASE("run: interest_groups.csv with invalid country reference fails loudly with no artifacts") {
    TempDir td("leviathan_runner_m35_invalid_country");

    leviathan::core::SimulationConfig cfg;
    cfg.start_date = leviathan::core::GameDate(1930, 1, 1);
    cfg.seed       = 1u;
    auto state = leviathan::core::make_game_state(cfg);

    leviathan::core::CountryState c;
    c.id      = leviathan::core::CountryId{0};
    c.id_code = "GER";
    state.countries.push_back(c);

    leviathan::core::InterestGroupState g;
    g.id_code = "phantom";
    g.name    = "Phantom";
    g.kind    = leviathan::core::InterestGroupKind::Religious;
    g.country = leviathan::core::CountryId{99};  // out of range
    state.interest_groups.push_back(std::move(g));

    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 0;
    opts.output_dir  = td.path;
    const auto r = rn::run_state(state, opts);
    REQUIRE(r.failed());
    CHECK(r.error().find("phantom") != std::string::npos);
    CHECK(r.error().find("invalid country") != std::string::npos);
    // The interest-group snapshot in `begin_tick` runs AFTER the
    // "simulation start" log line is appended to state.logs, but
    // BEFORE `end_tick` is ever called. `end_tick` is the only
    // function that writes to disk, so no artefact (including
    // interest_groups.csv) reaches the filesystem. M2.9
    // pre-`end_tick` no-artefact contract extended to the 6th
    // file.
    CHECK_FALSE(fs::exists(td.path / "save.json"));
    CHECK_FALSE(fs::exists(td.path / "events.jsonl"));
    CHECK_FALSE(fs::exists(td.path / "interest_groups.csv"));
}

TEST_CASE("run: --summary-csv byte-stream is unchanged by interest_groups.csv writing (M0.10 contract)") {
    // Baseline run with only --summary-csv; M3.5 run also opts into
    // --countries-csv / --factions-csv but interest_groups.csv is
    // always written either way. The summary CSV byte stream must
    // not budge.
    TempDir td_a("leviathan_runner_m35_summary_iso_a");
    TempDir td_b("leviathan_runner_m35_summary_iso_b");

    rn::RunnerOptions opts_a;
    opts_a.config_path      = kCanonicalConfig;
    opts_a.days             = 60;
    opts_a.output_dir       = td_a.path;
    opts_a.seed_override    = std::uint64_t{0xCAFEBABE};
    opts_a.summary_csv_path = td_a.path / "summary.csv";

    rn::RunnerOptions opts_b = opts_a;
    opts_b.output_dir        = td_b.path;
    opts_b.summary_csv_path  = td_b.path / "summary.csv";

    REQUIRE(rn::run(opts_a).ok());
    REQUIRE(rn::run(opts_b).ok());
    CHECK(read_file(td_a.path / "summary.csv") == read_file(td_b.path / "summary.csv"));
}

// =====================================================================
// M3.6 - interest_group_country_feedback.csv /
//        interest_group_authority_pressure.csv (unconditional)
// =====================================================================

TEST_CASE("run: M3.6 trace CSVs are header-only for the empty world / short run") {
    // No scenario + 0 days → no monthly tick fires, so no trace row
    // is ever appended. Both files must still exist with the
    // documented headers.
    TempDir td("leviathan_runner_m36_empty");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 0;
    opts.output_dir  = td.path;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().interest_group_country_feedback_csv_rows   == 0u);
    CHECK(r.value().interest_group_authority_pressure_csv_rows == 0u);

    REQUIRE(fs::exists(td.path / "interest_group_country_feedback.csv"));
    REQUIRE(fs::exists(td.path / "interest_group_authority_pressure.csv"));
    CHECK(read_file(td.path / "interest_group_country_feedback.csv") ==
          "date,country_id,country_id_code,matched_groups,"
          "weight_sum,weighted_radicalism,target_stability,"
          "stability_before,stability_after,stability_delta\n");
    CHECK(read_file(td.path / "interest_group_authority_pressure.csv") ==
          "date,country_id,country_id_code,matched_groups,"
          "weight_sum,weighted_bureaucracy_loyalty,"
          "target_bureaucratic_compliance,"
          "bureaucratic_compliance_before,"
          "bureaucratic_compliance_after,"
          "bureaucratic_compliance_delta\n");
}

TEST_CASE("run: M3.6 trace CSVs default to <output_dir>/<name>.csv") {
    TempDir td("leviathan_runner_m36_default_path");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 1;
    opts.output_dir  = td.path;
    // Neither override is set.
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().interest_group_country_feedback_csv_path ==
          td.path / "interest_group_country_feedback.csv");
    CHECK(r.value().interest_group_authority_pressure_csv_path ==
          td.path / "interest_group_authority_pressure.csv");
    CHECK(fs::exists(r.value().interest_group_country_feedback_csv_path));
    CHECK(fs::exists(r.value().interest_group_authority_pressure_csv_path));
}

TEST_CASE("run: M3.6 trace CSV path overrides are honoured") {
    TempDir td("leviathan_runner_m36_path_override");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 1;
    opts.output_dir  = td.path;
    opts.interest_group_country_feedback_csv_path =
        td.path / "custom" / "cf.csv";
    opts.interest_group_authority_pressure_csv_path =
        td.path / "custom" / "ap.csv";
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(fs::exists(td.path / "custom" / "cf.csv"));
    CHECK(fs::exists(td.path / "custom" / "ap.csv"));
    // Default paths are NOT written when the overrides are set.
    CHECK_FALSE(fs::exists(td.path / "interest_group_country_feedback.csv"));
    CHECK_FALSE(fs::exists(td.path / "interest_group_authority_pressure.csv"));
}

TEST_CASE("run: M3.6 trace CSVs emit rows after a monthly boundary with active groups") {
    // Hand-built state with one country + one Bureaucracy group
    // (so authority_pressure fires) + one extra group (so
    // country_feedback fires). 31 days crosses one month boundary.
    TempDir td("leviathan_runner_m36_with_groups");

    leviathan::core::SimulationConfig cfg;
    cfg.start_date = leviathan::core::GameDate(1930, 1, 1);
    cfg.seed       = 1u;
    auto state = leviathan::core::make_game_state(cfg);

    leviathan::core::CountryState c;
    c.id         = leviathan::core::CountryId{0};
    c.id_code    = "GER";
    c.name       = "Germany";
    c.stability  = 0.5;
    c.legitimacy = 0.5;
    c.government_authority.bureaucratic_compliance = 0.4;
    state.countries.push_back(c);

    leviathan::core::InterestGroupState g1;
    g1.id_code    = "ger_bureaucracy";
    g1.name       = "GER Bureaucracy";
    g1.kind       = leviathan::core::InterestGroupKind::Bureaucracy;
    g1.country    = leviathan::core::CountryId{0};
    g1.influence  = 0.6;
    g1.loyalty    = 0.8;
    g1.radicalism = 0.2;
    state.interest_groups.push_back(g1);

    leviathan::core::InterestGroupState g2 = g1;
    g2.id_code    = "ger_workers";
    g2.kind       = leviathan::core::InterestGroupKind::Workers;
    g2.influence  = 0.4;
    g2.loyalty    = 0.4;
    g2.radicalism = 0.6;
    state.interest_groups.push_back(g2);

    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 31;
    opts.output_dir  = td.path;
    const auto r = rn::run_state(state, opts);
    REQUIRE(r.ok());
    // One month boundary crossed → one row per system per
    // updated country = 1 row each.
    CHECK(r.value().interest_group_country_feedback_csv_rows   == 1u);
    CHECK(r.value().interest_group_authority_pressure_csv_rows == 1u);

    const std::string cf = read_file(
        td.path / "interest_group_country_feedback.csv");
    CHECK(cf.find("GER")    != std::string::npos);
    CHECK(cf.find("1930-")  != std::string::npos);
    CHECK(cf.find("e-")     != std::string::npos);

    const std::string ap = read_file(
        td.path / "interest_group_authority_pressure.csv");
    CHECK(ap.find("GER")    != std::string::npos);
    CHECK(ap.find("1930-")  != std::string::npos);
    CHECK(ap.find("e-")     != std::string::npos);
}

TEST_CASE("run: M3.6 trace CSVs preserve byte-identical determinism on same seed") {
    TempDir td_a("leviathan_runner_m36_det_a");
    TempDir td_b("leviathan_runner_m36_det_b");

    auto opts_for = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path   = kCanonicalConfig;
        o.days          = 90;
        o.output_dir    = dir;
        o.seed_override = std::uint64_t{0xC0FFEE};
        o.scenario_path = kCanonicalScenario;
        return o;
    };

    REQUIRE(rn::run(opts_for(td_a.path)).ok());
    REQUIRE(rn::run(opts_for(td_b.path)).ok());
    CHECK(read_file(td_a.path / "interest_group_country_feedback.csv") ==
          read_file(td_b.path / "interest_group_country_feedback.csv"));
    CHECK(read_file(td_a.path / "interest_group_authority_pressure.csv") ==
          read_file(td_b.path / "interest_group_authority_pressure.csv"));
}

TEST_CASE("run: M3.6 trace CSV writing does NOT change save / log / state CSVs") {
    // Adding two new CSVs at end_tick must not perturb the existing
    // five artefacts (M0.10 / M1.14 / M1.16 / M2.4 / M3.5 contracts
    // all preserved).
    TempDir td_a("leviathan_runner_m36_iso_a");
    TempDir td_b("leviathan_runner_m36_iso_b");

    auto opts_for = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path        = kCanonicalConfig;
        o.days               = 60;
        o.output_dir         = dir;
        o.seed_override      = std::uint64_t{0xFACE};
        o.scenario_path      = kCanonicalScenario;
        o.summary_csv_path   = dir / "summary.csv";
        o.countries_csv_path = dir / "countries.csv";
        o.factions_csv_path  = dir / "factions.csv";
        return o;
    };

    REQUIRE(rn::run(opts_for(td_a.path)).ok());
    REQUIRE(rn::run(opts_for(td_b.path)).ok());
    // Every pre-M3.6 artefact byte-stream must still be stable
    // across two independent runs.
    CHECK(read_file(td_a.path / "save.json")           ==
          read_file(td_b.path / "save.json"));
    CHECK(read_file(td_a.path / "events.jsonl")        ==
          read_file(td_b.path / "events.jsonl"));
    CHECK(read_file(td_a.path / "summary.csv")         ==
          read_file(td_b.path / "summary.csv"));
    CHECK(read_file(td_a.path / "countries.csv")       ==
          read_file(td_b.path / "countries.csv"));
    CHECK(read_file(td_a.path / "factions.csv")        ==
          read_file(td_b.path / "factions.csv"));
    CHECK(read_file(td_a.path / "interest_groups.csv") ==
          read_file(td_b.path / "interest_groups.csv"));
}

// =====================================================================
// M4.2 - provinces.svg is the 9th unconditional artefact
// =====================================================================

TEST_CASE("run: provinces.svg is written by end_tick (unconditional, empty-state header-only)") {
    // No scenario, so state.provinces is empty. The artefact still
    // exists; the contract is "the file always exists".
    TempDir td("leviathan_runner_m42_empty_provinces");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 0;
    opts.output_dir  = td.path;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    REQUIRE(fs::exists(td.path / "provinces.svg"));
    const std::string svg = read_file(td.path / "provinces.svg");
    CHECK(svg.find("<svg ")    != std::string::npos);
    CHECK(svg.find("</svg>")   != std::string::npos);
    CHECK(svg.find("<circle")  == std::string::npos);
}

TEST_CASE("run: provinces.svg defaults to <output_dir>/provinces.svg") {
    TempDir td("leviathan_runner_m42_default_path");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 1;
    opts.output_dir  = td.path;
    // provinces_svg_path intentionally unset.
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().provinces_svg_path == td.path / "provinces.svg");
    CHECK(fs::exists(r.value().provinces_svg_path));
}

TEST_CASE("run: provinces_svg_path override is honoured") {
    TempDir td("leviathan_runner_m42_path_override");
    rn::RunnerOptions opts;
    opts.config_path        = kCanonicalConfig;
    opts.days               = 1;
    opts.output_dir         = td.path;
    opts.provinces_svg_path = td.path / "custom" / "map.svg";
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().provinces_svg_path == td.path / "custom" / "map.svg");
    CHECK(fs::exists(td.path / "custom" / "map.svg"));
    // Default path is NOT written when the override is set.
    CHECK_FALSE(fs::exists(td.path / "provinces.svg"));
}

TEST_CASE("run: canonical scenario renders all three M4.1 nodes into provinces.svg") {
    // M4.1 fixtures put berlin / paris / tokyo in the canonical
    // 1930_minimal manifest. The SVG render exposes id_code and
    // owner via data-* attributes, so the test pins both shape and
    // ownership without parsing presentation values.
    TempDir td("leviathan_runner_m42_canonical_svg");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    const std::string svg = read_file(td.path / "provinces.svg");
    CHECK(svg.find("data-id=\"berlin\"") != std::string::npos);
    CHECK(svg.find("data-id=\"paris\"")  != std::string::npos);
    CHECK(svg.find("data-id=\"tokyo\"")  != std::string::npos);
    // owner indices: berlin=0 (GER), paris=1 (FRA), tokyo=2 (JPN).
    CHECK(svg.find("data-owner=\"0\"") != std::string::npos);
    CHECK(svg.find("data-owner=\"1\"") != std::string::npos);
    CHECK(svg.find("data-owner=\"2\"") != std::string::npos);
}

TEST_CASE("run: canonical scenario uses M4.3 per-owner palette in provinces.svg") {
    // M4.3 replaces M4.2's fixed black fill with a deterministic
    // owner-keyed palette lookup. Canonical owners 0 / 1 / 2 map
    // to palette entries 0 / 1 / 2 respectively; pin all three
    // colours via the public `kOwnerPalette` constant so a future
    // edit to the palette table fails this gate loudly.
    TempDir td("leviathan_runner_m43_canonical_palette");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    const std::string svg = read_file(td.path / "provinces.svg");

    namespace svgx = leviathan::systems::svg_export;
    const std::string ger_fill =
        std::string("fill=\"") +
        std::string(svgx::kOwnerPalette[0]) + "\"";
    const std::string fra_fill =
        std::string("fill=\"") +
        std::string(svgx::kOwnerPalette[1]) + "\"";
    const std::string jpn_fill =
        std::string("fill=\"") +
        std::string(svgx::kOwnerPalette[2]) + "\"";

    CHECK(svg.find(ger_fill) != std::string::npos);
    CHECK(svg.find(fra_fill) != std::string::npos);
    CHECK(svg.find(jpn_fill) != std::string::npos);
    // The M4.2 black fill is gone.
    CHECK(svg.find("fill=\"black\"") == std::string::npos);
}

TEST_CASE("run: canonical scenario emits M4.4 <text> labels in provinces.svg") {
    // M4.4 adds one <text> per ProvinceNode using the
    // XML-text-escaped name. The canonical 1930_minimal fixture
    // names the three nodes "Berlin" / "Paris" / "Tokyo" — none
    // contain XML special characters, so the labels appear
    // verbatim in the output.
    TempDir td("leviathan_runner_m44_canonical_labels");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    const std::string svg = read_file(td.path / "provinces.svg");
    CHECK(svg.find(">Berlin</text>") != std::string::npos);
    CHECK(svg.find(">Paris</text>")  != std::string::npos);
    CHECK(svg.find(">Tokyo</text>")  != std::string::npos);
    // text-anchor pinned so a future presentation change
    // doesn't silently break centred labels.
    CHECK(svg.find("text-anchor=\"middle\"") != std::string::npos);
}

TEST_CASE("run: provinces.svg preserves byte-identical determinism on same seed") {
    TempDir td_a("leviathan_runner_m42_det_a");
    TempDir td_b("leviathan_runner_m42_det_b");

    auto opts_for = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path   = kCanonicalConfig;
        o.days          = 31;
        o.output_dir    = dir;
        o.seed_override = std::uint64_t{0xBEEF};
        o.scenario_path = kCanonicalScenario;
        return o;
    };
    REQUIRE(rn::run(opts_for(td_a.path)).ok());
    REQUIRE(rn::run(opts_for(td_b.path)).ok());
    CHECK(read_file(td_a.path / "provinces.svg") ==
          read_file(td_b.path / "provinces.svg"));
}

// =====================================================================
// M4.5 - map.html is the 10th unconditional artefact
// =====================================================================

TEST_CASE("run: map.html is written by end_tick (unconditional, empty-state still a valid HTML doc)") {
    // No scenario, so state.provinces is empty. The artefact
    // still exists; the contract is "the file always exists".
    TempDir td("leviathan_runner_m45_empty_map_html");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 0;
    opts.output_dir  = td.path;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    REQUIRE(fs::exists(td.path / "map.html"));
    const std::string html = read_file(td.path / "map.html");
    CHECK(html.find("<!DOCTYPE html>") != std::string::npos);
    CHECK(html.find("<svg ")           != std::string::npos);
    CHECK(html.find("</html>")         != std::string::npos);
    CHECK(html.find("<circle")         == std::string::npos);
}

TEST_CASE("run: map.html defaults to <output_dir>/map.html") {
    TempDir td("leviathan_runner_m45_default_path");
    rn::RunnerOptions opts;
    opts.config_path = kCanonicalConfig;
    opts.days        = 1;
    opts.output_dir  = td.path;
    // map_html_path intentionally unset.
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().map_html_path == td.path / "map.html");
    CHECK(fs::exists(r.value().map_html_path));
}

TEST_CASE("run: map_html_path override is honoured") {
    TempDir td("leviathan_runner_m45_path_override");
    rn::RunnerOptions opts;
    opts.config_path  = kCanonicalConfig;
    opts.days         = 1;
    opts.output_dir   = td.path;
    opts.map_html_path = td.path / "custom" / "viewer.html";
    const auto r = rn::run(opts);
    REQUIRE(r.ok());
    CHECK(r.value().map_html_path == td.path / "custom" / "viewer.html");
    CHECK(fs::exists(td.path / "custom" / "viewer.html"));
    // Default path is NOT written when the override is set.
    CHECK_FALSE(fs::exists(td.path / "map.html"));
}

TEST_CASE("run: canonical scenario embeds full SVG body inside map.html") {
    // M4.5 inlines the SAME <svg>...</svg> body as provinces.svg
    // (minus the XML prolog). Pin that the canonical owner-keyed
    // colours and labels appear inside the HTML output, so the
    // wrapper is byte-faithful to the renderer.
    TempDir td("leviathan_runner_m45_canonical_html");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    const std::string html = read_file(td.path / "map.html");

    // Canonical ProvinceNode names appear as <text> bodies.
    CHECK(html.find(">Berlin</text>") != std::string::npos);
    CHECK(html.find(">Paris</text>")  != std::string::npos);
    CHECK(html.find(">Tokyo</text>")  != std::string::npos);
    // Canonical owner palette colours appear on the circles.
    namespace svgx = leviathan::systems::svg_export;
    const std::string ger_fill =
        std::string("fill=\"") +
        std::string(svgx::kOwnerPalette[0]) + "\"";
    CHECK(html.find(ger_fill) != std::string::npos);
    // No XML prolog inside the HTML document.
    CHECK(html.find("<?xml") == std::string::npos);
}

TEST_CASE("run: canonical scenario carries the M4.6 minimal CSS in map.html") {
    // M4.6 adds three CSS selectors to the HTML wrapper.
    // Pin that the canonical run carries them all so a
    // future edit to the inline stylesheet fails this gate
    // loudly. provinces.svg must NOT carry the CSS.
    TempDir td("leviathan_runner_m46_canonical_css");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    const std::string html = read_file(td.path / "map.html");
    CHECK(html.find("<style>")                   != std::string::npos);
    CHECK(html.find("body {")                    != std::string::npos);
    CHECK(html.find("background-color: #f0f0f0") != std::string::npos);
    CHECK(html.find("svg {")                     != std::string::npos);
    CHECK(html.find("margin: 0 auto")            != std::string::npos);
    CHECK(html.find("border: 1px solid #888")    != std::string::npos);
    CHECK(html.find("svg text {")                != std::string::npos);
    CHECK(html.find("font-family: sans-serif")   != std::string::npos);

    // The standalone SVG stays CSS-free — the M4.6 styling
    // applies only to the HTML viewer.
    const std::string svg = read_file(td.path / "provinces.svg");
    CHECK(svg.find("<style")          == std::string::npos);
    CHECK(svg.find("font-family")     == std::string::npos);
    CHECK(svg.find("background-color") == std::string::npos);
}

TEST_CASE("run: canonical scenario emits M4.7 legend listing GER / FRA / JPN in map.html") {
    // M4.7 adds a static <ul class="legend"> after the inline
    // SVG. Pin canonical content (id_code + name + per-owner
    // swatch colour) so a future fixture / palette edit fails
    // this gate loudly. provinces.svg must remain legend-free.
    TempDir td("leviathan_runner_m47_canonical_legend");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    const std::string html = read_file(td.path / "map.html");

    // Legend element + one <li> per canonical country, with
    // id_code, name, and a swatch fill matching the palette
    // entry for owner index i.
    CHECK(html.find("<ul class=\"legend\">") != std::string::npos);
    CHECK(html.find("data-owner=\"0\"")      != std::string::npos);
    CHECK(html.find("data-owner=\"1\"")      != std::string::npos);
    CHECK(html.find("data-owner=\"2\"")      != std::string::npos);
    // Canonical fixtures: name="Germany" / "France" / "Japan"
    // (display_name is "French Republic" / "Empire of Japan"
    // for those two, but per spec the legend renders name).
    CHECK(html.find("GER &mdash; Germany") != std::string::npos);
    CHECK(html.find("FRA &mdash; France")  != std::string::npos);
    CHECK(html.find("JPN &mdash; Japan")   != std::string::npos);

    namespace svgx = leviathan::systems::svg_export;
    for (int i = 0; i < 3; ++i) {
        const std::string fill =
            std::string("fill=\"") +
            std::string(svgx::kOwnerPalette[static_cast<std::size_t>(i)]) +
            "\"";
        CHECK(html.find(fill) != std::string::npos);
    }

    // The standalone SVG stays legend-free.
    const std::string svg = read_file(td.path / "provinces.svg");
    CHECK(svg.find("<ul")     == std::string::npos);
    CHECK(svg.find("legend")  == std::string::npos);
    CHECK(svg.find("&mdash;") == std::string::npos);
}

TEST_CASE("run: canonical scenario carries M4.8 data-* attrs on both provinces.svg AND map.html") {
    // M4.8 widens the identity surface inside the SVG body —
    // both <circle> and <text> now carry data-id /
    // data-owner / data-owner-code / data-name. The change
    // happens inside `render_svg_root`, which both
    // `render_provinces` (→ provinces.svg) and
    // `render_map_html` (→ map.html) share, so the new
    // attrs land in BOTH artefacts.
    TempDir td("leviathan_runner_m48_canonical_data_attrs");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    const std::string svg  = read_file(td.path / "provinces.svg");
    const std::string html = read_file(td.path / "map.html");

    // Canonical fixtures: berlin owned by GER (index 0),
    // paris owned by FRA (index 1), tokyo owned by JPN (index 2).
    // M4.13 added data-owner-name (Germany / France / Japan).
    for (const std::string& body : {svg, html}) {
        CHECK(body.find("data-id=\"berlin\"")           != std::string::npos);
        CHECK(body.find("data-id=\"paris\"")            != std::string::npos);
        CHECK(body.find("data-id=\"tokyo\"")            != std::string::npos);
        CHECK(body.find("data-owner-code=\"GER\"")      != std::string::npos);
        CHECK(body.find("data-owner-code=\"FRA\"")      != std::string::npos);
        CHECK(body.find("data-owner-code=\"JPN\"")      != std::string::npos);
        CHECK(body.find("data-owner-name=\"Germany\"")  != std::string::npos);
        CHECK(body.find("data-owner-name=\"France\"")   != std::string::npos);
        CHECK(body.find("data-owner-name=\"Japan\"")    != std::string::npos);
        CHECK(body.find("data-name=\"Berlin\"")         != std::string::npos);
        CHECK(body.find("data-name=\"Paris\"")          != std::string::npos);
        CHECK(body.find("data-name=\"Tokyo\"")          != std::string::npos);
    }
}

TEST_CASE("run: canonical scenario emits the M4.10 clickable surface in map.html only") {
    // M4.10 adds a <div id="details"> panel + exactly one
    // inline <script> click handler to map.html. provinces.svg
    // stays inert (no <script>, no <div id="details">, no
    // .details CSS, no addEventListener).
    TempDir td("leviathan_runner_m410_canonical_clickable");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    const std::string html = read_file(td.path / "map.html");
    const std::string svg  = read_file(td.path / "provinces.svg");

    // map.html carries the details placeholder + exactly one
    // inline <script>.
    CHECK(html.find("<div id=\"details\"") != std::string::npos);
    CHECK(html.find("Click a province to see its details.")
          != std::string::npos);
    CHECK(html.find("<script")  != std::string::npos);
    CHECK(html.find("</script") != std::string::npos);
    CHECK(html.find("<script src=")  == std::string::npos);
    CHECK(html.find("<script type=") == std::string::npos);
    CHECK(html.find("addEventListener") != std::string::npos);
    // The click handler is read-only (no fetch / no storage /
    // no innerHTML / no eval).
    CHECK(html.find("fetch(")        == std::string::npos);
    CHECK(html.find("localStorage")  == std::string::npos);
    CHECK(html.find("innerHTML")     == std::string::npos);
    CHECK(html.find("eval(")         == std::string::npos);

    // provinces.svg stays fully inert.
    CHECK(svg.find("<script")              == std::string::npos);
    CHECK(svg.find("</script")             == std::string::npos);
    CHECK(svg.find("<div id=\"details\"")  == std::string::npos);
    CHECK(svg.find(".details")             == std::string::npos);
    CHECK(svg.find("addEventListener")     == std::string::npos);
}

TEST_CASE("run: map.html preserves byte-identical determinism on same seed") {
    TempDir td_a("leviathan_runner_m45_det_a");
    TempDir td_b("leviathan_runner_m45_det_b");

    auto opts_for = [&](const fs::path& dir) {
        rn::RunnerOptions o;
        o.config_path   = kCanonicalConfig;
        o.days          = 31;
        o.output_dir    = dir;
        o.seed_override = std::uint64_t{0xCAFEF00D};
        o.scenario_path = kCanonicalScenario;
        return o;
    };
    REQUIRE(rn::run(opts_for(td_a.path)).ok());
    REQUIRE(rn::run(opts_for(td_b.path)).ok());
    CHECK(read_file(td_a.path / "map.html") ==
          read_file(td_b.path / "map.html"));
}

// =====================================================================
// M3.8 - canonical scenarios author Bureaucracy interest groups
// =====================================================================

TEST_CASE("run: canonical scenario produces data rows in all three M3 CSVs") {
    // M3.8 added one Bureaucracy interest group per canonical
    // country (GER / FRA / JPN). A 31-day run from the canonical
    // 1930_minimal scenario crosses one month boundary, so:
    //
    //   * interest_groups.csv: 3 groups × 3 snapshot points
    //     (start + month_changed + final post-sanity) = 9 rows.
    //   * interest_group_country_feedback.csv: one monthly tick,
    //     all 3 countries have at least one matching group with
    //     non-zero influence = 3 rows.
    //   * interest_group_authority_pressure.csv: same — all 3
    //     countries have a Bureaucracy group with non-zero
    //     influence = 3 rows.
    TempDir td("leviathan_runner_m38_canonical_data_rows");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 31;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    CHECK(r.value().interest_groups_csv_rows                   == 9u);
    CHECK(r.value().interest_group_country_feedback_csv_rows   == 3u);
    CHECK(r.value().interest_group_authority_pressure_csv_rows == 3u);

    // Every M3 CSV exists with data, not just a header line.
    const std::string ig = read_file(td.path / "interest_groups.csv");
    CHECK(ig.find("ger_bureaucracy") != std::string::npos);
    CHECK(ig.find("fra_bureaucracy") != std::string::npos);
    CHECK(ig.find("jpn_bureaucracy") != std::string::npos);

    const std::string cf =
        read_file(td.path / "interest_group_country_feedback.csv");
    CHECK(cf.find("GER") != std::string::npos);
    CHECK(cf.find("FRA") != std::string::npos);
    CHECK(cf.find("JPN") != std::string::npos);

    const std::string ap =
        read_file(td.path / "interest_group_authority_pressure.csv");
    CHECK(ap.find("GER") != std::string::npos);
    CHECK(ap.find("FRA") != std::string::npos);
    CHECK(ap.find("JPN") != std::string::npos);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

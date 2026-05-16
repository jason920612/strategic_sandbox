#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "leviathan/systems/runner.hpp"

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
const char* kCanonicalConfig = LEVIATHAN_TEST_DATA_DIR "/config/simulation.json";
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

// M3 reaction-loop integration checkpoint (M3.7).
//
// This is NOT an M3 exit report. M3 stays in progress; the
// purpose of these tests is to pin the *composition* of the M3
// sub-milestones that have already shipped:
//
//   M3.1   InterestGroupState data layer
//   M3.2   country.stability -> group loyalty / radicalism
//   M3.3   group radicalism -> country.stability
//   M3.4   Bureaucracy loyalty -> bureaucratic_compliance
//   M3.5   interest_groups.csv state surface
//   M3.6   feedback / authority pressure outcome trace CSVs
//
// Unit tests already exercise each system in isolation and pin
// the exact arithmetic of every formula. The integration tests
// here are about the seam between them — that monthly pipeline
// runs all three reaction systems in order on the same state,
// that the runner emits the eight-file artefact set when the
// state actually has interest groups, and that two same-seed
// runs of the same scenario produce byte-identical output
// across every artefact.
//
// Three test cases, all hand-built state (canonical scenarios
// author zero interest groups, so the loop never fires through
// them):
//
//   A. One-month reaction loop. Build a country + Bureaucracy
//      interest group, call `monthly::tick_all_countries`, and
//      assert all three M3 reaction systems mutated the
//      expected fields. Loose inequality checks, not exact
//      arithmetic — the unit tests already pin numbers.
//
//   B. Runner emits all nine artefacts with data rows in the
//      M3 CSVs. Drive a hand-built state through
//      `begin_tick / step_one_day * 31 / end_tick`, cross one
//      month boundary, and assert every artefact lands on disk
//      and the four M3 CSVs are NOT header-only (M3.10's
//      military_pressure CSV joined the set).
//
//   C. Same seed + same options + same hand-built state runs
//      twice produce byte-identical 9-artefact output. Mirrors
//      the M1.17 / M2.22 byte-identical contracts but on a
//      state that exercises the M3 reaction loop.

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
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
#include "leviathan/systems/monthly_pipeline.hpp"
#include "leviathan/systems/runner.hpp"
#include "leviathan/systems/save_system.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionState;
using leviathan::core::FactionId;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::InterestGroupKind;
using leviathan::core::InterestGroupState;
using leviathan::core::SimulationConfig;
namespace mp = leviathan::systems::monthly;
namespace rn = leviathan::systems::runner;

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
#endif

// Build a single-country state with one Bureaucracy-kind
// interest group whose influence / loyalty / radicalism are
// chosen so all three M3 reaction systems produce a visible
// drift on a single monthly tick:
//
//   * M3.2 mutates the group's loyalty + radicalism (every
//     group with a valid country update).
//   * M3.3 mutates country.stability because the group's
//     radicalism is non-trivial and influence > 0.
//   * M3.4 mutates country.government_authority.bureaucratic_compliance
//     because the group's kind is Bureaucracy and influence > 0.
//
// Stability and bureaucratic_compliance start at 0.5 so the
// target / current delta has room to move in either direction.
GameState m37_state_one_bureaucracy_group() {
    SimulationConfig cfg;
    cfg.start_date = GameDate(1930, 1, 1);
    cfg.seed       = std::uint64_t{0xC1A551C};
    GameState s = leviathan::core::make_game_state(cfg);

    CountryState c;
    c.id                              = CountryId{0};
    c.id_code                         = "GER";
    c.name                            = "Germany";
    c.gdp                             = 100.0;
    c.legal_tax_burden                = 0.20;
    c.fiscal_capacity                 = 0.50;
    c.administrative_efficiency       = 0.40;
    c.central_control                 = 0.50;
    c.corruption                      = 0.20;
    c.stability                       = 0.50;
    c.legitimacy                      = 0.55;
    c.military_power                  = 0.45;
    c.threat_perception               = 0.30;
    c.government_authority.bureaucratic_compliance = 0.50;
    c.government_authority.military_loyalty        = 0.50;
    // Budget categories at small non-zero values so economy::tick
    // produces a meaningful growth rate; not the focus of M3.7
    // but the monthly pipeline runs every per-country system, so
    // a populated budget keeps the integration honest.
    c.budget.administration   = 0.10;
    c.budget.military         = 0.20;
    c.budget.education        = 0.10;
    c.budget.welfare          = 0.10;
    c.budget.intelligence     = 0.05;
    c.budget.infrastructure   = 0.10;
    c.budget.industry         = 0.15;
    s.countries.push_back(c);

    // One faction so `faction::react` has something to drift; it
    // doesn't matter for the M3 systems' assertions but the
    // pipeline expects to run faction::react -> stability::tick
    // -> economy::tick per country before the global M3 step.
    FactionState f;
    f.id              = FactionId{0};
    f.country         = CountryId{0};
    f.id_code         = "GER_military";
    f.country_id_code = "GER";
    f.name            = "GER Military";
    f.type            = "military";
    f.support         = 0.50;
    f.influence       = 0.40;
    f.radicalism      = 0.20;
    f.loyalty         = 0.50;
    f.resources       = 1.0;
    s.factions.push_back(f);

    InterestGroupState g;
    g.id_code    = "ger_bureaucracy";
    g.name       = "German Bureaucracy";
    g.kind       = InterestGroupKind::Bureaucracy;
    g.country    = CountryId{0};
    g.influence  = 0.60;
    g.loyalty    = 0.80;     // > country.stability (0.50)
    g.radicalism = 0.30;     // != 1 - country.stability (0.50)
    s.interest_groups.push_back(g);

    // M3.9: also add a Military-kind group so the new fourth global
    // step in tick_all_countries has something to fold. Loyalty 0.75
    // is well above the country's military_loyalty (0.50) and well
    // below 1.0 so the post-clamp value moves visibly without
    // saturating.
    InterestGroupState m;
    m.id_code    = "ger_military_ig";
    m.name       = "German General Staff";
    m.kind       = InterestGroupKind::Military;
    m.country    = CountryId{0};
    m.influence  = 0.50;
    m.loyalty    = 0.75;
    m.radicalism = 0.20;
    s.interest_groups.push_back(m);
    return s;
}

}  // namespace

// =====================================================================
// A. One-month reaction loop runs all four M3 systems on the same state
// =====================================================================
TEST_CASE("M3 integration: one monthly tick runs M3.2 / M3.3 / M3.4 / M3.9 on the same state") {
    GameState s = m37_state_one_bureaucracy_group();

    const double loyalty_before    = s.interest_groups[0].loyalty;
    const double radicalism_before = s.interest_groups[0].radicalism;
    const double stability_before  = s.countries[0].stability;
    const double compliance_before =
        s.countries[0].government_authority.bureaucratic_compliance;
    const double military_loyalty_before =
        s.countries[0].government_authority.military_loyalty;

    const auto r = mp::tick_all_countries(s);
    REQUIRE(r.ok());
    const auto& outcome = r.value();

    // Counters: M3.2 mutates every valid group (two: bureaucracy +
    // military); M3.3 mutates GER because at least one group has
    // positive influence; M3.4 mutates GER because its Bureaucracy
    // group has positive influence; M3.9 mutates GER because its
    // Military group has positive influence.
    CHECK(outcome.interest_groups_updated                    == 2);
    CHECK(outcome.interest_group_countries_updated           == 1);
    CHECK(outcome.interest_group_authority_countries_updated == 1);
    CHECK(outcome.interest_group_military_countries_updated  == 1);

    // Group state changed (M3.2 effect).
    CHECK(s.interest_groups[0].loyalty    != doctest::Approx(loyalty_before).epsilon(1e-18));
    CHECK(s.interest_groups[0].radicalism != doctest::Approx(radicalism_before).epsilon(1e-18));

    // Country stability moved (M3.3 effect). The group's
    // radicalism is 0.30 (pre-M3.2; M3.2 nudges it toward
    // 1 - 0.50 = 0.50, so post-M3.2 radicalism is ~0.31). The
    // target for M3.3 is 1 - weighted_radicalism, which is
    // slightly above stability_before, so stability drifts up.
    CHECK(s.countries[0].stability != doctest::Approx(stability_before).epsilon(1e-18));

    // Bureaucratic compliance moved (M3.4 effect). The
    // Bureaucracy group's loyalty (post-M3.2 ~0.79) is above
    // compliance_before (0.50), so compliance drifts up.
    CHECK(s.countries[0].government_authority.bureaucratic_compliance
          != doctest::Approx(compliance_before).epsilon(1e-18));

    // Military loyalty moved (M3.9 effect). The Military group's
    // loyalty (post-M3.2 ~0.74) is above military_loyalty_before
    // (0.50), so military_loyalty drifts up.
    CHECK(s.countries[0].government_authority.military_loyalty
          != doctest::Approx(military_loyalty_before).epsilon(1e-18));

    // M3.6 trace vectors + M3.9 trace vector arrived through
    // `MonthlyOutcome`.
    REQUIRE(outcome.interest_group_country_feedback_trace_rows.size() == 1u);
    REQUIRE(outcome.interest_group_authority_pressure_trace_rows.size() == 1u);
    REQUIRE(outcome.interest_group_military_pressure_trace_rows.size() == 1u);
    CHECK(outcome.interest_group_country_feedback_trace_rows[0].country_id_code
          == "GER");
    CHECK(outcome.interest_group_authority_pressure_trace_rows[0].country_id_code
          == "GER");
    CHECK(outcome.interest_group_military_pressure_trace_rows[0].country_id_code
          == "GER");
    // Trace `*_after` reflects the just-clamped value.
    CHECK(outcome.interest_group_country_feedback_trace_rows[0].stability_after
          == doctest::Approx(s.countries[0].stability));
    CHECK(outcome.interest_group_authority_pressure_trace_rows[0].bureaucratic_compliance_after
          == doctest::Approx(s.countries[0].government_authority.bureaucratic_compliance));
    CHECK(outcome.interest_group_military_pressure_trace_rows[0].military_loyalty_after
          == doctest::Approx(s.countries[0].government_authority.military_loyalty));
}

#ifdef LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// B. Runner emits all eight artefacts with data rows in the M3 CSVs
// =====================================================================
TEST_CASE("M3 integration: runner emits all 9 artefacts and the four M3 CSVs are not header-only") {
    TempDir td("leviathan_m3_endtoend_artifacts");
    rn::RunnerOptions opts;
    opts.config_path        = kCanonicalConfig;
    opts.days               = 31;  // crosses one month boundary
    opts.output_dir         = td.path;
    opts.summary_csv_path   = td.path / "summary.csv";
    opts.countries_csv_path = td.path / "countries.csv";
    opts.factions_csv_path  = td.path / "factions.csv";

    GameState s = m37_state_one_bureaucracy_group();
    const auto r = rn::run_state(s, opts);
    REQUIRE(r.ok());
    CHECK(r.value().monthly_ticks >= 1);

    // All nine artefacts present.
    const fs::path save_path           = td.path / "save.json";
    const fs::path log_path            = td.path / "events.jsonl";
    const fs::path summary_path        = td.path / "summary.csv";
    const fs::path countries_path      = td.path / "countries.csv";
    const fs::path factions_path       = td.path / "factions.csv";
    const fs::path interest_groups     = td.path / "interest_groups.csv";
    const fs::path country_feedback    = td.path / "interest_group_country_feedback.csv";
    const fs::path authority_pressure  = td.path / "interest_group_authority_pressure.csv";
    const fs::path military_pressure   = td.path / "interest_group_military_pressure.csv";

    REQUIRE(fs::exists(save_path));
    REQUIRE(fs::exists(log_path));
    REQUIRE(fs::exists(summary_path));
    REQUIRE(fs::exists(countries_path));
    REQUIRE(fs::exists(factions_path));
    REQUIRE(fs::exists(interest_groups));
    REQUIRE(fs::exists(country_feedback));
    REQUIRE(fs::exists(authority_pressure));
    REQUIRE(fs::exists(military_pressure));

    // The four M3 CSVs all have data beyond the header.
    const std::string ig_text = read_file(interest_groups);
    const std::string cf_text = read_file(country_feedback);
    const std::string ap_text = read_file(authority_pressure);
    const std::string mp_text = read_file(military_pressure);

    // interest_groups.csv: M3.5 snapshots at start, each month
    // boundary, and end -> 3 snapshot points × 2 groups (the
    // Bureaucracy group + the Military group added for M3.9) = 6
    // data rows.
    CHECK(ig_text.find("ger_bureaucracy")    != std::string::npos);
    CHECK(ig_text.find("ger_military_ig")    != std::string::npos);
    CHECK(r.value().interest_groups_csv_rows == 6u);

    // M3.6 / M3.10 trace CSVs: one mutation per month boundary,
    // and 31 days crosses one boundary -> exactly one data row
    // each. The helper state has one Bureaucracy + one Military
    // group, so both authority-layer systems fire.
    CHECK(cf_text.find("GER") != std::string::npos);
    CHECK(ap_text.find("GER") != std::string::npos);
    CHECK(mp_text.find("GER") != std::string::npos);
    CHECK(r.value().interest_group_country_feedback_csv_rows   == 1u);
    CHECK(r.value().interest_group_authority_pressure_csv_rows == 1u);
    CHECK(r.value().interest_group_military_pressure_csv_rows  == 1u);

    // RunOutcome paths point at the actual files written.
    CHECK(r.value().interest_groups_csv_path == interest_groups);
    CHECK(r.value().interest_group_country_feedback_csv_path == country_feedback);
    CHECK(r.value().interest_group_authority_pressure_csv_path == authority_pressure);
    CHECK(r.value().interest_group_military_pressure_csv_path == military_pressure);
}

// =====================================================================
// C. Same seed + same hand-built state produces byte-identical 8-artefact output
// =====================================================================
TEST_CASE("M3 integration: same seed + same options produces byte-identical 9 artefacts") {
    auto run_once = [](const fs::path& output_dir) {
        rn::RunnerOptions opts;
        opts.config_path        = kCanonicalConfig;
        opts.days               = 31;
        opts.output_dir         = output_dir;
        opts.summary_csv_path   = output_dir / "summary.csv";
        opts.countries_csv_path = output_dir / "countries.csv";
        opts.factions_csv_path  = output_dir / "factions.csv";

        GameState s = m37_state_one_bureaucracy_group();
        REQUIRE(rn::run_state(s, opts).ok());
    };

    TempDir td_a("leviathan_m3_endtoend_det_a");
    TempDir td_b("leviathan_m3_endtoend_det_b");
    run_once(td_a.path);
    run_once(td_b.path);

    // All 9 artefacts byte-identical (M1.17 / M2.22 contract +
    // M3.5 + M3.6 + M3.10 extensions, validated end-to-end on
    // a state that actually exercises the M3 reaction loop).
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
    CHECK(read_file(td_a.path / "interest_groups.csv") ==
          read_file(td_b.path / "interest_groups.csv"));
    CHECK(read_file(td_a.path / "interest_group_country_feedback.csv") ==
          read_file(td_b.path / "interest_group_country_feedback.csv"));
    CHECK(read_file(td_a.path / "interest_group_authority_pressure.csv") ==
          read_file(td_b.path / "interest_group_authority_pressure.csv"));
    CHECK(read_file(td_a.path / "interest_group_military_pressure.csv") ==
          read_file(td_b.path / "interest_group_military_pressure.csv"));
}

// =====================================================================
// M3.11 - M3 close-out integration tests
//
// Three long-running, deliberately-broad tests pinning the M3 surface
// at the seam between M3 and M4+. They mirror the M1.17 / M2.22
// close-out pattern: a real scenario stretched across a year, a soak
// run, and a save round-trip. Unit tests already pin per-system
// arithmetic; M3.11 tests pin *composition over time* — that the
// reaction loop and the 9 artefacts stay coherent across many monthly
// boundaries.
// =====================================================================

namespace {

// Build a multi-country state covering all four M3 reaction systems
// with non-trivial inputs. Three countries (GER / FRA / JPN), each
// with a Bureaucracy + a Military + one other-kind group so M3.2
// drifts every group, M3.3 fires on every country, M3.4 fires
// (Bureaucracy present), and M3.9 fires (Military present).
// Ratios chosen so each system produces a visible delta per month.
GameState m311_full_m3_state() {
    SimulationConfig cfg;
    cfg.start_date = GameDate(1930, 1, 1);
    cfg.seed       = std::uint64_t{0xC10551A6};
    GameState s = leviathan::core::make_game_state(cfg);

    auto add_country = [&](int idx, const std::string& code,
                           double stability, double compliance,
                           double military_loyalty) {
        CountryState c;
        c.id      = CountryId{idx};
        c.id_code = code;
        c.name    = code;
        c.gdp                                     = 100.0;
        c.legal_tax_burden                        = 0.20;
        c.fiscal_capacity                         = 0.50;
        c.administrative_efficiency               = 0.40;
        c.central_control                         = 0.50;
        c.corruption                              = 0.20;
        c.stability                               = stability;
        c.legitimacy                              = 0.55;
        c.military_power                          = 0.45;
        c.threat_perception                       = 0.30;
        c.government_authority.bureaucratic_compliance = compliance;
        c.government_authority.military_loyalty        = military_loyalty;
        c.budget.administration   = 0.10;
        c.budget.military         = 0.20;
        c.budget.education        = 0.10;
        c.budget.welfare          = 0.10;
        c.budget.intelligence     = 0.05;
        c.budget.infrastructure   = 0.10;
        c.budget.industry         = 0.15;
        s.countries.push_back(c);

        FactionState f;
        f.id              = FactionId{idx};
        f.country         = CountryId{idx};
        f.id_code         = code + "_military_faction";
        f.country_id_code = code;
        f.name            = code + " Military Faction";
        f.type            = "military";
        f.support         = 0.50;
        f.influence       = 0.40;
        f.radicalism      = 0.20;
        f.loyalty         = 0.50;
        f.resources       = 1.0;
        s.factions.push_back(f);
    };

    add_country(0, "GER", /*stability=*/0.50,
                /*compliance=*/0.40, /*military_loyalty=*/0.40);
    add_country(1, "FRA", /*stability=*/0.55,
                /*compliance=*/0.50, /*military_loyalty=*/0.55);
    add_country(2, "JPN", /*stability=*/0.45,
                /*compliance=*/0.60, /*military_loyalty=*/0.35);

    auto add_group = [&](const std::string& code, InterestGroupKind kind,
                         int country_idx,
                         double influence, double loyalty, double radicalism) {
        InterestGroupState g;
        g.id_code    = code;
        g.name       = code;
        g.kind       = kind;
        g.country    = CountryId{country_idx};
        g.influence  = influence;
        g.loyalty    = loyalty;
        g.radicalism = radicalism;
        s.interest_groups.push_back(g);
    };

    // GER: bureaucracy / military / workers
    add_group("ger_bureaucracy", InterestGroupKind::Bureaucracy, 0, 0.6, 0.8, 0.2);
    add_group("ger_military",    InterestGroupKind::Military,    0, 0.5, 0.7, 0.3);
    add_group("ger_workers",     InterestGroupKind::Workers,     0, 0.4, 0.4, 0.6);
    // FRA: bureaucracy / military / religious
    add_group("fra_bureaucracy", InterestGroupKind::Bureaucracy, 1, 0.5, 0.6, 0.3);
    add_group("fra_military",    InterestGroupKind::Military,    1, 0.5, 0.6, 0.3);
    add_group("fra_religious",   InterestGroupKind::Religious,   1, 0.3, 0.7, 0.2);
    // JPN: bureaucracy / military / business
    add_group("jpn_bureaucracy", InterestGroupKind::Bureaucracy, 2, 0.7, 0.9, 0.1);
    add_group("jpn_military",    InterestGroupKind::Military,    2, 0.6, 0.5, 0.5);
    add_group("jpn_business",    InterestGroupKind::Business,    2, 0.4, 0.6, 0.3);

    return s;
}

rn::RunnerOptions m311_full_artefact_opts(const fs::path& dir,
                                          int days,
                                          std::uint64_t seed_override) {
    rn::RunnerOptions opts;
    opts.config_path        = kCanonicalConfig;
    opts.days               = days;
    opts.output_dir         = dir;
    opts.seed_override      = seed_override;
    opts.summary_csv_path   = dir / "summary.csv";
    opts.countries_csv_path = dir / "countries.csv";
    opts.factions_csv_path  = dir / "factions.csv";
    return opts;
}

}  // namespace

// ---------------------------------------------------------------------
// 1. 1-year scenario run on the full M3 surface
// ---------------------------------------------------------------------
TEST_CASE("M3 close-out: 365-day run on multi-country / multi-kind state exercises every M3 system") {
    TempDir td("leviathan_m3_closeout_year");

    GameState s = m311_full_m3_state();
    const auto opts = m311_full_artefact_opts(td.path, 365,
                                              std::uint64_t{0xC1051D11});
    const auto r = rn::run_state(s, opts);
    REQUIRE(r.ok());
    const auto& outcome = r.value();

    // 365 days from 1930-01-01 crosses 12 month boundaries.
    CHECK(outcome.monthly_ticks == 12);

    // Every per-system counter ticked: 9 interest groups across 3
    // countries means M3.2 mutates all 9 every month, M3.3 fires on
    // every country every month (3), M3.4 fires on every country
    // (each has a Bureaucracy group), and M3.9 fires on every
    // country (each has a Military group). 12 monthly ticks ×
    // 3 countries = 36 rows in each of the two per-system CSVs
    // (skipped countries would emit 0, but here none are skipped).
    CHECK(outcome.interest_group_country_feedback_csv_rows   == 36u);
    CHECK(outcome.interest_group_authority_pressure_csv_rows == 36u);
    CHECK(outcome.interest_group_military_pressure_csv_rows  == 36u);

    // interest_groups.csv snapshots at start + 12 month boundaries
    // + final post-sanity = 14 points × 9 groups = 126 rows.
    CHECK(outcome.interest_groups_csv_rows == 126u);

    // All 9 artefacts present.
    REQUIRE(fs::exists(td.path / "save.json"));
    REQUIRE(fs::exists(td.path / "events.jsonl"));
    REQUIRE(fs::exists(td.path / "summary.csv"));
    REQUIRE(fs::exists(td.path / "countries.csv"));
    REQUIRE(fs::exists(td.path / "factions.csv"));
    REQUIRE(fs::exists(td.path / "interest_groups.csv"));
    REQUIRE(fs::exists(td.path / "interest_group_country_feedback.csv"));
    REQUIRE(fs::exists(td.path / "interest_group_authority_pressure.csv"));
    REQUIRE(fs::exists(td.path / "interest_group_military_pressure.csv"));

    // sanity_check found nothing — clamps and preflight held across
    // the full year on a non-trivial state.
    CHECK(outcome.sanity_issues_logged == 0u);

    // Every country / group ratio that M3 systems mutate is still
    // inside [0, 1] — the per-system clamps survived the year.
    for (const auto& c : s.countries) {
        CHECK(c.stability >= 0.0);
        CHECK(c.stability <= 1.0);
        CHECK(c.government_authority.bureaucratic_compliance >= 0.0);
        CHECK(c.government_authority.bureaucratic_compliance <= 1.0);
        CHECK(c.government_authority.military_loyalty >= 0.0);
        CHECK(c.government_authority.military_loyalty <= 1.0);
    }
    for (const auto& g : s.interest_groups) {
        CHECK(g.loyalty    >= 0.0);
        CHECK(g.loyalty    <= 1.0);
        CHECK(g.radicalism >= 0.0);
        CHECK(g.radicalism <= 1.0);
        // Influence is structural — M3 never mutates it.
        CHECK(g.influence  >= 0.0);
        CHECK(g.influence  <= 1.0);
    }
}

// ---------------------------------------------------------------------
// 2. 10-year soak run — sanity over a long horizon
// ---------------------------------------------------------------------
TEST_CASE("M3 close-out: 10-year soak run keeps every M3-mutated field inside [0, 1] with no sanity issues") {
    TempDir td("leviathan_m3_closeout_soak");

    GameState s = m311_full_m3_state();
    const auto opts = m311_full_artefact_opts(td.path, 3652,
                                              std::uint64_t{0x50AC500A});

    const auto r = rn::run_state(s, opts);
    REQUIRE(r.ok());
    const auto& outcome = r.value();

    // 3652 days from 1930-01-01 → 1939-12-31 with 9 leap-day-handled
    // years; the runner reports `monthly_ticks` per crossed boundary.
    // 12 boundaries / year × ~10 years = 120 (the off-by-one cases
    // around year boundaries don't matter for this soak — the only
    // assertion is "lots of monthly ticks happened").
    CHECK(outcome.monthly_ticks >= 119);
    CHECK(outcome.monthly_ticks <= 121);

    CHECK(outcome.sanity_issues_logged == 0u);

    // Every M3-mutated field still inside its [0, 1] band after ~10
    // years of drift. This is the soak test's only real claim —
    // unit tests pin per-tick arithmetic; this pins "no slow drift
    // out of bounds over a long horizon".
    for (const auto& c : s.countries) {
        CHECK(std::isfinite(c.stability));
        CHECK(c.stability >= 0.0);
        CHECK(c.stability <= 1.0);
        CHECK(std::isfinite(c.government_authority.bureaucratic_compliance));
        CHECK(c.government_authority.bureaucratic_compliance >= 0.0);
        CHECK(c.government_authority.bureaucratic_compliance <= 1.0);
        CHECK(std::isfinite(c.government_authority.military_loyalty));
        CHECK(c.government_authority.military_loyalty >= 0.0);
        CHECK(c.government_authority.military_loyalty <= 1.0);
    }
    for (const auto& g : s.interest_groups) {
        CHECK(std::isfinite(g.loyalty));
        CHECK(g.loyalty    >= 0.0);
        CHECK(g.loyalty    <= 1.0);
        CHECK(std::isfinite(g.radicalism));
        CHECK(g.radicalism >= 0.0);
        CHECK(g.radicalism <= 1.0);
    }
}

// ---------------------------------------------------------------------
// 3. Save round-trip preserves the M3 surface byte-for-byte
// ---------------------------------------------------------------------
TEST_CASE("M3 close-out: save round-trip preserves interest_groups + government_authority across all 9 artefacts") {
    namespace ss = leviathan::systems::save_system;

    TempDir td("leviathan_m3_closeout_roundtrip");

    GameState src = m311_full_m3_state();
    const auto opts = m311_full_artefact_opts(td.path, 31,
                                              std::uint64_t{0xCAFE5A7E});
    REQUIRE(rn::run_state(src, opts).ok());

    // Load the save back.
    auto loaded_r = ss::load(td.path / "save.json");
    REQUIRE(loaded_r.ok());
    const auto loaded = loaded_r.value();

    // M3.1 interest_groups round-trips entry by entry.
    REQUIRE(loaded.interest_groups.size() == src.interest_groups.size());
    for (std::size_t i = 0; i < src.interest_groups.size(); ++i) {
        const auto& a = src.interest_groups[i];
        const auto& b = loaded.interest_groups[i];
        CHECK(a.id_code        == b.id_code);
        CHECK(a.name           == b.name);
        CHECK(a.kind           == b.kind);
        CHECK(a.country.value() == b.country.value());
        CHECK(a.influence      == doctest::Approx(b.influence));
        CHECK(a.loyalty        == doctest::Approx(b.loyalty));
        CHECK(a.radicalism     == doctest::Approx(b.radicalism));
    }

    // M2.16 government_authority round-trips per country, including
    // the two fields M3.4 / M3.9 actually mutate.
    REQUIRE(loaded.countries.size() == src.countries.size());
    for (std::size_t i = 0; i < src.countries.size(); ++i) {
        const auto& a = src.countries[i].government_authority;
        const auto& b = loaded.countries[i].government_authority;
        CHECK(a.bureaucratic_compliance ==
              doctest::Approx(b.bureaucratic_compliance));
        CHECK(a.military_loyalty ==
              doctest::Approx(b.military_loyalty));
        // The two still-inert sub-fields M3.4 / M3.9 do NOT mutate
        // also round-trip identically.
        CHECK(a.intelligence_capability ==
              doctest::Approx(b.intelligence_capability));
        CHECK(a.media_control ==
              doctest::Approx(b.media_control));
    }

    // Re-running the same seed + same hand-built state into a
    // second temp dir produces byte-identical 9-artefact output.
    TempDir td_b("leviathan_m3_closeout_roundtrip_b");
    GameState src_b = m311_full_m3_state();
    const auto opts_b = m311_full_artefact_opts(td_b.path, 31,
                                                std::uint64_t{0xCAFE5A7E});
    REQUIRE(rn::run_state(src_b, opts_b).ok());

    CHECK(read_file(td.path / "save.json") ==
          read_file(td_b.path / "save.json"));
    CHECK(read_file(td.path / "events.jsonl") ==
          read_file(td_b.path / "events.jsonl"));
    CHECK(read_file(td.path / "summary.csv") ==
          read_file(td_b.path / "summary.csv"));
    CHECK(read_file(td.path / "countries.csv") ==
          read_file(td_b.path / "countries.csv"));
    CHECK(read_file(td.path / "factions.csv") ==
          read_file(td_b.path / "factions.csv"));
    CHECK(read_file(td.path / "interest_groups.csv") ==
          read_file(td_b.path / "interest_groups.csv"));
    CHECK(read_file(td.path / "interest_group_country_feedback.csv") ==
          read_file(td_b.path / "interest_group_country_feedback.csv"));
    CHECK(read_file(td.path / "interest_group_authority_pressure.csv") ==
          read_file(td_b.path / "interest_group_authority_pressure.csv"));
    CHECK(read_file(td.path / "interest_group_military_pressure.csv") ==
          read_file(td_b.path / "interest_group_military_pressure.csv"));
}

#endif  // LEVIATHAN_TEST_DATA_DIR

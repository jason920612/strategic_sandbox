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
//   B. Runner emits all eight artefacts with data rows in the
//      M3 CSVs. Drive a hand-built state through
//      `begin_tick / step_one_day * 31 / end_tick`, cross one
//      month boundary, and assert every artefact lands on disk
//      and the three M3 CSVs are NOT header-only.
//
//   C. Same seed + same options + same hand-built state runs
//      twice produce byte-identical 8-artefact output. Mirrors
//      the M1.17 / M2.22 byte-identical contracts but on a
//      state that exercises the M3 reaction loop.

#include <doctest/doctest.h>

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
TEST_CASE("M3 integration: runner emits all 8 artefacts and the three M3 CSVs are not header-only") {
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

    // All eight artefacts present.
    const fs::path save_path           = td.path / "save.json";
    const fs::path log_path            = td.path / "events.jsonl";
    const fs::path summary_path        = td.path / "summary.csv";
    const fs::path countries_path      = td.path / "countries.csv";
    const fs::path factions_path       = td.path / "factions.csv";
    const fs::path interest_groups     = td.path / "interest_groups.csv";
    const fs::path country_feedback    = td.path / "interest_group_country_feedback.csv";
    const fs::path authority_pressure  = td.path / "interest_group_authority_pressure.csv";

    REQUIRE(fs::exists(save_path));
    REQUIRE(fs::exists(log_path));
    REQUIRE(fs::exists(summary_path));
    REQUIRE(fs::exists(countries_path));
    REQUIRE(fs::exists(factions_path));
    REQUIRE(fs::exists(interest_groups));
    REQUIRE(fs::exists(country_feedback));
    REQUIRE(fs::exists(authority_pressure));

    // The three M3 CSVs all have data beyond the header.
    const std::string ig_text = read_file(interest_groups);
    const std::string cf_text = read_file(country_feedback);
    const std::string ap_text = read_file(authority_pressure);

    // interest_groups.csv: M3.5 snapshots at start, each month
    // boundary, and end -> 3 snapshot points × 2 groups (the
    // Bureaucracy group + the Military group added for M3.9) = 6
    // data rows.
    CHECK(ig_text.find("ger_bureaucracy")    != std::string::npos);
    CHECK(ig_text.find("ger_military_ig")    != std::string::npos);
    CHECK(r.value().interest_groups_csv_rows == 6u);

    // M3.6 trace CSVs: one mutation per month boundary, and 31
    // days crosses one boundary -> exactly one data row each.
    CHECK(cf_text.find("GER") != std::string::npos);
    CHECK(ap_text.find("GER") != std::string::npos);
    CHECK(r.value().interest_group_country_feedback_csv_rows   == 1u);
    CHECK(r.value().interest_group_authority_pressure_csv_rows == 1u);

    // RunOutcome paths point at the actual files written.
    CHECK(r.value().interest_groups_csv_path == interest_groups);
    CHECK(r.value().interest_group_country_feedback_csv_path == country_feedback);
    CHECK(r.value().interest_group_authority_pressure_csv_path == authority_pressure);
}

// =====================================================================
// C. Same seed + same hand-built state produces byte-identical 8-artefact output
// =====================================================================
TEST_CASE("M3 integration: same seed + same options produces byte-identical 8 artefacts") {
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

    // All 8 artefacts byte-identical (M1.17 / M2.22 contract +
    // M3.5 + M3.6 extensions, validated end-to-end on a state
    // that actually exercises the M3 reaction loop).
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
}

#endif  // LEVIATHAN_TEST_DATA_DIR

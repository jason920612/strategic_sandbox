// M3.7 reaction-loop integration checkpoint.
//
// M3 ships a closed loop:
//
//   country.stability
//     -> M3.2 interest_group::react
//     -> group.loyalty / group.radicalism
//
//   group.radicalism + influence
//     -> M3.3 interest_group::country_feedback
//     -> country.stability
//
//   Bureaucracy group loyalty + influence
//     -> M3.4 interest_group::authority_pressure
//     -> country.government_authority.bureaucratic_compliance
//
//   state / outcomes
//     -> M3.5 interest_groups.csv
//     -> M3.6 interest_group_country_feedback.csv
//             interest_group_authority_pressure.csv
//
// Exact arithmetic for each leg is already pinned by
// `tests/systems/interest_group_system_test.cpp`. This
// integration test pins three coarser properties at the seam
// between M3 and any future milestone:
//
//   A. monthly::tick_all_countries actually drives every leg in
//      one call when a country has at least one Bureaucracy
//      interest group with non-zero influence (so all three
//      reverse-direction outcomes increment and the two trace
//      vectors get one row each).
//
//   B. runner::run_state emits the full 8-artefact set with
//      actual M3 data rows when the same hand-built state goes
//      through begin_tick / step_one_day / end_tick. M1.17 /
//      M2's canonical-scenario runs already cover the
//      header-only path; this run covers the data-row path.
//
//   C. Two byte-for-byte identical hand-built states with the
//      same options produce byte-identical 8-artefact output.
//      Mirrors M1.17 / M2.22's determinism contract but with
//      M3 mutation actually happening on the wire.
//
// M3.7 deliberately does NOT add a new system, formula,
// artefact, save schema bump, gameplay branch, or close M3 —
// see docs/milestone-3-checkpoint.md.

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
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/monthly_pipeline.hpp"
#include "leviathan/systems/runner.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
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

// Build a one-country, one-faction, one-Bureaucracy-group state
// whose numeric setup guarantees a non-zero delta in every M3
// leg (M3.2 group drift, M3.3 stability feedback, M3.4 authority
// pressure). Each value sits well clear of its target so a
// single monthly tick produces a clearly-positive change.
//
// Used by both the in-memory loop test (A) and the runner-driven
// artefact tests (B / C); building it via a free helper keeps
// the two paths byte-identical at the input layer.
GameState make_m3_loop_state() {
    SimulationConfig cfg;
    cfg.start_date = GameDate(1930, 1, 1);
    cfg.seed       = 1u;
    GameState state = leviathan::core::make_game_state(cfg);

    CountryState c;
    c.id         = CountryId{0};
    c.id_code    = "GER";
    c.name       = "Germany";
    c.stability  = 0.6;
    c.legitimacy = 0.5;
    // Compliance starts below the M3.4 target (post-M3.2 group
    // loyalty drifts toward stability=0.6, so target ≈ 0.41 in
    // the first month). 0.3 keeps the drift clearly positive.
    c.government_authority.bureaucratic_compliance = 0.3;
    state.countries.push_back(c);

    // One faction so factions.csv has a data row and the M1
    // monthly pipeline has something to mutate. Numeric values
    // are unimportant for the M3 assertions; they only need to
    // keep the M1 pipeline happy.
    FactionState f;
    f.id              = FactionId{0};
    f.country         = CountryId{0};
    f.id_code         = "ger_military";
    f.country_id_code = "GER";
    f.name            = "German Military";
    f.type            = "military";
    f.support         = 0.5;
    f.influence       = 0.5;
    f.radicalism      = 0.5;
    f.loyalty         = 0.5;
    state.factions.push_back(f);

    // One Bureaucracy-kind group with non-zero influence so all
    // three reverse-direction outcomes (M3.2 react, M3.3
    // country_feedback, M3.4 authority_pressure) fire and each
    // emits exactly one trace row.
    InterestGroupState g;
    g.id_code    = "ger_bureaucracy";
    g.name       = "GER Bureaucracy";
    g.kind       = InterestGroupKind::Bureaucracy;
    g.country    = CountryId{0};
    g.influence  = 0.6;
    g.loyalty    = 0.4;
    g.radicalism = 0.3;
    state.interest_groups.push_back(g);

    return state;
}

bool changed(double before, double after) {
    return std::abs(after - before) > 1e-12;
}

}  // namespace

// =====================================================================
// A. one-month reaction loop drives every M3 leg in a single call
// =====================================================================
TEST_CASE("M3 end-to-end: tick_all_countries fires react + country_feedback + authority_pressure") {
    GameState state = make_m3_loop_state();

    const double loyalty_before    = state.interest_groups[0].loyalty;
    const double radicalism_before = state.interest_groups[0].radicalism;
    const double stability_before  = state.countries[0].stability;
    const double compliance_before =
        state.countries[0].government_authority.bureaucratic_compliance;

    const auto r = mp::tick_all_countries(state);
    REQUIRE(r.ok());

    const auto& out = r.value();
    CHECK(out.countries_processed                          == 1);
    CHECK(out.interest_groups_updated                      == 1);
    CHECK(out.interest_group_countries_updated             == 1);
    CHECK(out.interest_group_authority_countries_updated   == 1);

    // M3.2 drifted both ratios on the only group.
    CHECK(changed(loyalty_before,    state.interest_groups[0].loyalty));
    CHECK(changed(radicalism_before, state.interest_groups[0].radicalism));

    // M3.3 drifted the only country's stability.
    CHECK(changed(stability_before, state.countries[0].stability));

    // M3.4 drifted the only country's bureaucratic_compliance.
    CHECK(changed(compliance_before,
                  state.countries[0]
                      .government_authority.bureaucratic_compliance));

    // M3.6 trace surface: one row per system per actually-updated
    // country. One country was updated by each system, so each
    // vector has exactly one row.
    REQUIRE(out.interest_group_country_feedback_trace_rows.size()   == 1u);
    REQUIRE(out.interest_group_authority_pressure_trace_rows.size() == 1u);

    const auto& cf_row = out.interest_group_country_feedback_trace_rows[0];
    const auto& ap_row = out.interest_group_authority_pressure_trace_rows[0];

    CHECK(cf_row.country_id_code == "GER");
    CHECK(ap_row.country_id_code == "GER");

    // The trace row's post-mutation field equals the live state
    // field — M3.3 wrote stability_after, M3.4 wrote
    // bureaucratic_compliance_after, nothing between then and now
    // touches those fields again.
    CHECK(cf_row.stability_after ==
          doctest::Approx(state.countries[0].stability));
    CHECK(ap_row.bureaucratic_compliance_after ==
          doctest::Approx(state.countries[0]
                              .government_authority.bureaucratic_compliance));
}

// =====================================================================
// B. runner emits all 8 artefacts with actual M3 data rows
// =====================================================================
TEST_CASE("M3 end-to-end: run_state emits 8 artefacts and the three M3 files have data rows") {
    TempDir td("leviathan_m3_endtoend_artifacts");

    GameState state = make_m3_loop_state();

    rn::RunnerOptions opts;
    opts.days               = 31;  // crosses exactly one month boundary
    opts.output_dir         = td.path;
    opts.summary_csv_path   = td.path / "summary.csv";
    opts.countries_csv_path = td.path / "countries.csv";
    opts.factions_csv_path  = td.path / "factions.csv";
    // M3.5 / M3.6 artefacts are unconditional — no path override needed.

    const auto r = rn::run_state(state, opts);
    REQUIRE(r.ok());

    const auto& outcome = r.value();
    CHECK(outcome.monthly_ticks == 1);
    // M3.5: snapshot cadence is start + each month_changed + final
    // post-sanity = 3 snapshot points × 1 group = 3 data rows.
    CHECK(outcome.interest_groups_csv_rows == 3u);
    // M3.6: one monthly tick that mutated one country in each
    // system = exactly one row per trace CSV.
    CHECK(outcome.interest_group_country_feedback_csv_rows   == 1u);
    CHECK(outcome.interest_group_authority_pressure_csv_rows == 1u);

    // All eight artefacts on disk.
    CHECK(fs::exists(td.path / "save.json"));
    CHECK(fs::exists(td.path / "events.jsonl"));
    CHECK(fs::exists(td.path / "summary.csv"));
    CHECK(fs::exists(td.path / "countries.csv"));
    CHECK(fs::exists(td.path / "factions.csv"));
    CHECK(fs::exists(td.path / "interest_groups.csv"));
    CHECK(fs::exists(td.path / "interest_group_country_feedback.csv"));
    CHECK(fs::exists(td.path / "interest_group_authority_pressure.csv"));

    // The three M3 files have header + data (not header-only).
    const std::string ig = read_file(td.path / "interest_groups.csv");
    CHECK(ig.find("ger_bureaucracy") != std::string::npos);

    const std::string cf =
        read_file(td.path / "interest_group_country_feedback.csv");
    CHECK(cf.find("GER")   != std::string::npos);
    CHECK(cf.find("1930-") != std::string::npos);

    const std::string ap =
        read_file(td.path / "interest_group_authority_pressure.csv");
    CHECK(ap.find("GER")   != std::string::npos);
    CHECK(ap.find("1930-") != std::string::npos);
}

// =====================================================================
// C. same hand-built state + same options -> byte-identical 8 artefacts
// =====================================================================
TEST_CASE("M3 end-to-end: same hand-built state produces byte-identical 8 artefacts") {
    auto run_once = [](const fs::path& output_dir) {
        GameState state = make_m3_loop_state();

        rn::RunnerOptions opts;
        opts.days               = 31;
        opts.output_dir         = output_dir;
        opts.summary_csv_path   = output_dir / "summary.csv";
        opts.countries_csv_path = output_dir / "countries.csv";
        opts.factions_csv_path  = output_dir / "factions.csv";

        REQUIRE(rn::run_state(state, opts).ok());
    };

    TempDir td_a("leviathan_m3_endtoend_det_a");
    TempDir td_b("leviathan_m3_endtoend_det_b");
    run_once(td_a.path);
    run_once(td_b.path);

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

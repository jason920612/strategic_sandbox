// RCR-1: RFC compliance recovery integration test.
//
// RCR is a recovery-track identifier, NOT an M-milestone number.
// This integration test verifies the data + selection items
// cleared by RCR-1 against the new
// `data/scenarios/1930_rfc_compliance.json` scenario:
//
//   - RFC-090 §3.2 / §3.3 / RFC-010 §5 country floor: 20
//     countries load through scenario_loader without error.
//   - RFC-010 §5 policy floor: 20 policies load.
//   - RFC-090 §5.10 / RFC-010 §5 event floor: 10 event
//     definitions load (2 from canonical events file +
//     8 from the extended events file).
//   - RFC-010 §5 faction / actor floor: 6+ interest groups
//     spread across multiple countries (not all GER).
//   - RFC-090 §3.5 / RFC-010 §2.2 AI auto-policy selection
//     (selection-only): ai_policy::select_policies returns
//     20 selections (no player country).
//   - RFC-090 §3.10 partial — short-period run reaches
//     end without crash and produces zero sanity issues.
//
// What this test deliberately does NOT cover (deferred to a
// future RCR PR per docs/rfc-090-010-compliance-audit.md):
//
//   - RFC-090 §3.6 relationship-value system (no save bump
//     in RCR-1)
//   - RFC-090 §3.7 / §3.8 threat / military computation
//     systems (the fields exist on CountryState but no
//     system drives them yet)
//   - RFC-090 §3.9 / RFC-010 annual world statistics CSV
//     artefact
//   - RFC-090 §5.3 / §5.4 / §5.6 / §5.7 / §5.8 event
//     weights / options / weighted selection
//   - RFC-090 §5.9 per-fire events.jsonl emission
//   - RFC-090 §5.11 10-year event stress test
//   - RFC-090 §5.12 followup-event-chain model
//   - AI-policy *apply* path (RCR-1 ships selection-only)
//
// A future RCR PR that ships any of those items also adds the
// corresponding assertions here.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/ai_policy.hpp"
#include "leviathan/systems/runner.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::GameState;
namespace ai = leviathan::systems::ai_policy;
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

}  // namespace

#ifdef LEVIATHAN_TEST_DATA_DIR
namespace {

const fs::path kDataDir            = LEVIATHAN_TEST_DATA_DIR;
const fs::path kCanonicalConfig    = kDataDir / "config" / "simulation.json";
const fs::path kComplianceScenario =
    kDataDir / "scenarios" / "1930_rfc_compliance.json";

}  // namespace

// =====================================================================
// RCR-1 §1: 20-country / 20-policy / 10-event / 6+-IG floors load
// =====================================================================

TEST_CASE("RCR-1 integration: compliance scenario loads with 20 countries / 20 policies / 10 events / 6+ IGs across countries") {
    TempDir td("leviathan_rcr1_compliance_load");

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.scenario_path = kComplianceScenario;
    opts.days          = 0;            // zero-day run: load + end_tick
    opts.output_dir    = td.path;

    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    const std::string save = read_file(td.path / "save.json");

    // Save must reflect the loaded compliance scenario data.
    // 20 countries: every country id_code appears in the save.
    static const char* kCountryIds[] = {
        "GER", "FRA", "JPN", "ITA", "GBR", "USA", "SOV", "POL",
        "CHN", "ESP", "TUR", "BRA", "ARG", "MEX", "CAN", "IND",
        "NLD", "BEL", "CZE", "SWE",
    };
    for (const auto* id : kCountryIds) {
        CAPTURE(id);
        // Each country id_code appears at least once as a JSON
        // string in the save (in countries[i].id_code).
        const std::string needle = std::string("\"") + id + "\"";
        CHECK(save.find(needle) != std::string::npos);
    }

    // 20 policies: each new RCR-1 policy id_code appears in the save.
    static const char* kNewPolicyIds[] = {
        "industrial_subsidy",
        "infrastructure_investment",
        "agricultural_subsidy",
        "labor_protections",
        "corruption_crackdown",
        "centralize_authority",
        "propaganda_campaign",
        "fiscal_consolidation",
        "security_tightening",
        "decentralize_governance",
    };
    for (const auto* id : kNewPolicyIds) {
        CAPTURE(id);
        const std::string needle = std::string("\"") + id + "\"";
        CHECK(save.find(needle) != std::string::npos);
    }

    // 10 events: 2 canonical + 8 extended.
    static const char* kExtendedEventIds[] = {
        "bureaucratic_strain",
        "legitimacy_crisis",
        "budget_shortfall_warning",
        "corruption_scandal",
        "weak_intelligence_warning",
        "media_control_backlash",
        "military_loyalty_concern",
        "economic_slowdown_warning",
    };
    for (const auto* id : kExtendedEventIds) {
        CAPTURE(id);
        const std::string needle = std::string("\"") + id + "\"";
        CHECK(save.find(needle) != std::string::npos);
    }
    CHECK(save.find("\"low_stability_unrest\"")           != std::string::npos);
    CHECK(save.find("\"radical_interest_group_warning\"") != std::string::npos);

    // 6+ interest groups across countries.
    static const char* kInterestGroupIds[] = {
        "ger_bureaucracy",
        "fra_bureaucracy",
        "jpn_bureaucracy",
        "gbr_workers",
        "usa_business",
        "sov_military",
        "chn_farmers",
        "ita_media",
        "ind_religious",
        "swe_technocrats",
    };
    for (const auto* id : kInterestGroupIds) {
        CAPTURE(id);
        const std::string needle = std::string("\"") + id + "\"";
        CHECK(save.find(needle) != std::string::npos);
    }

    // Save format stays at v16 — RCR-1 does NOT bump.
    CHECK(save.find("\"save_version\": 16") != std::string::npos);

    // The 10 canonical artefacts still appear (RCR-1 does NOT
    // change the 10-artefact contract).
    CHECK(fs::exists(td.path / "save.json"));
    CHECK(fs::exists(td.path / "events.jsonl"));
    CHECK(fs::exists(td.path / "interest_groups.csv"));
    CHECK(fs::exists(td.path / "interest_group_country_feedback.csv"));
    CHECK(fs::exists(td.path / "interest_group_authority_pressure.csv"));
    CHECK(fs::exists(td.path / "provinces.svg"));
    CHECK(fs::exists(td.path / "map.html"));
    // Annual world stats CSV is explicitly DEFERRED to a future
    // RCR PR (see docs/rfc-090-010-compliance-audit.md). It must
    // not be present yet.
    CHECK_FALSE(fs::exists(td.path / "annual_world_stats.csv"));
}

// =====================================================================
// RCR-1 §2: short-period run on the compliance scenario survives
// without crash and produces zero sanity issues (RFC-090 §3.10 partial)
// =====================================================================

TEST_CASE("RCR-1 integration: compliance scenario survives a 365-day run with zero sanity issues") {
    TempDir td("leviathan_rcr1_compliance_365d");

    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.scenario_path = kComplianceScenario;
    opts.days          = 365;         // ~12 month boundaries; covers RFC-090 §M3 multi-country pipeline
    opts.output_dir    = td.path;

    const auto r = rn::run(opts);
    REQUIRE(r.ok());

    // sanity_issues_logged == 0 — the runner's M0.10 diagnostic
    // surface would surface duplicate / invalid / NaN states.
    CHECK(r.value().sanity_issues_logged == 0u);

    // events.jsonl exists; events do NOT fire on a thresholdsafe
    // RCR-1 compliance scenario (extended events keep canonical
    // thresholds intentionally low) so canonical-only event id_codes
    // are absent from the lifecycle log (M0.6 semantics unchanged).
    const std::string ev_log = read_file(td.path / "events.jsonl");
    CHECK(ev_log.find("low_stability_unrest")           == std::string::npos);
    CHECK(ev_log.find("radical_interest_group_warning") == std::string::npos);

    // Save reflects the post-run state and is still v16.
    const std::string save = read_file(td.path / "save.json");
    CHECK(save.find("\"save_version\": 16") != std::string::npos);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// RCR-1 §3: ai_policy::select_policies returns one Selection per
// country on a non-trivial 20-country compliance scenario
// =====================================================================

TEST_CASE("RCR-1 integration: ai_policy::select_policies produces 20 selections on a hand-built 20-country state") {
    // Build a hand-built state mirroring the compliance scenario
    // structure (countries + policies) without going through the
    // scenario loader. Avoids LEVIATHAN_TEST_DATA_DIR coupling
    // and keeps the test fast.
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});

    static const char* kCountryIds[] = {
        "GER", "FRA", "JPN", "ITA", "GBR", "USA", "SOV", "POL",
        "CHN", "ESP", "TUR", "BRA", "ARG", "MEX", "CAN", "IND",
        "NLD", "BEL", "CZE", "SWE",
    };
    for (const auto* id : kCountryIds) {
        leviathan::core::CountryState c;
        c.id_code = id;
        c.name    = id;
        state.countries.push_back(c);
    }

    leviathan::core::PolicyData p;
    p.id_code = "first_policy";
    p.name    = "First Policy";
    state.policies.push_back(p);

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() == 20u);

    // Selections in vector order, all pointing to policies[0].
    for (std::size_t i = 0; i < 20u; ++i) {
        CAPTURE(i);
        CHECK(r.value()[i].country ==
              CountryId{static_cast<CountryId::underlying_type>(i)});
        CHECK(r.value()[i].policy_id_code == "first_policy");
    }
}

TEST_CASE("RCR-1 integration: ai_policy::select_policies skips player country in a 20-country state") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    for (int i = 0; i < 20; ++i) {
        leviathan::core::CountryState c;
        c.id_code = "C" + std::to_string(i);
        c.name    = c.id_code;
        state.countries.push_back(c);
    }
    leviathan::core::PolicyData p;
    p.id_code = "any_policy";
    state.policies.push_back(p);
    state.player_country = CountryId{7};   // designate one as the player

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() == 19u);

    const auto player_count = std::count_if(
        r.value().begin(), r.value().end(),
        [](const ai::Selection& s) { return s.country == CountryId{7}; });
    CHECK(player_count == 0);
}

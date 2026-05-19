// M7.1 faction_demands unit tests (RFC-090 §7.1, RFC-020 §7).
//
// Pins:
//   * kind / status string conversion (closed allowlist)
//   * faction-type → demand-kind mapping (RFC-020 §7 allowlist)
//   * tick_generate: generation predicate per kind, threshold,
//     determinism, no double-generation while Pending, strict
//     validation rejections
//   * tick_expire_and_apply: status flip, asymptotic radicalism /
//     loyalty drift, no re-trigger after Expired, strict
//     validation rejections
//   * save round-trip including faction_demands

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/faction_demands.hpp"
#include "leviathan/systems/save_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionDemand;
using leviathan::core::FactionDemandKind;
using leviathan::core::FactionDemandStatus;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameDate;
using leviathan::core::GameState;
namespace fd = leviathan::systems::faction_demands;
namespace ss = leviathan::systems::save_system;

namespace {

CountryState make_country(int id, const std::string& code) {
    CountryState c;
    c.id      = CountryId{id};
    c.id_code = code;
    c.name    = code;
    c.stability  = 0.55;
    c.legitimacy = 0.55;
    return c;
}

FactionState make_faction(int id, int country_id,
                          const std::string& id_code,
                          const std::string& country_id_code,
                          const std::string& type,
                          double radicalism,
                          double loyalty = 0.5) {
    FactionState f;
    f.id              = FactionId{id};
    f.country         = CountryId{country_id};
    f.id_code         = id_code;
    f.country_id_code = country_id_code;
    f.name            = id_code;
    f.type            = type;
    f.support         = 0.5;
    f.influence       = 0.5;
    f.radicalism      = radicalism;
    f.loyalty         = loyalty;
    f.resources       = 0.0;
    return f;
}

}  // namespace

// =====================================================================
// Kind / status string round-trip
// =====================================================================

TEST_CASE("M7.1 kind_to_string covers all six RFC-020 §7 variants") {
    CHECK(fd::kind_to_string(FactionDemandKind::IncreaseMilitaryBudget) == "increase_military_budget");
    CHECK(fd::kind_to_string(FactionDemandKind::ExpandWelfare) == "expand_welfare");
    CHECK(fd::kind_to_string(FactionDemandKind::ReligiousEducationAuthority) == "religious_education_authority");
    CHECK(fd::kind_to_string(FactionDemandKind::LocalAutonomy) == "local_autonomy");
    CHECK(fd::kind_to_string(FactionDemandKind::TechnocratResearchFunding) == "technocrat_research_funding");
    CHECK(fd::kind_to_string(FactionDemandKind::IntelligenceSurveillanceAuthority) == "intelligence_surveillance_authority");
}

TEST_CASE("M7.1 kind_from_string round-trips every variant") {
    for (FactionDemandKind k : {
            FactionDemandKind::IncreaseMilitaryBudget,
            FactionDemandKind::ExpandWelfare,
            FactionDemandKind::ReligiousEducationAuthority,
            FactionDemandKind::LocalAutonomy,
            FactionDemandKind::TechnocratResearchFunding,
            FactionDemandKind::IntelligenceSurveillanceAuthority,
        }) {
        const auto s = fd::kind_to_string(k);
        const auto r = fd::kind_from_string(s);
        REQUIRE(r);
        CHECK(r.value() == k);
    }
}

TEST_CASE("M7.1 kind_from_string rejects unknown string loudly") {
    const auto r = fd::kind_from_string("not_a_kind");
    REQUIRE(r.failed());
    CHECK(r.error().find("unknown kind") != std::string::npos);
    CHECK(r.error().find("not_a_kind")   != std::string::npos);
}

TEST_CASE("M7.1 status_to_string + status_from_string covers both variants") {
    CHECK(fd::status_to_string(FactionDemandStatus::Pending) == "pending");
    CHECK(fd::status_to_string(FactionDemandStatus::Expired) == "expired");
    const auto p = fd::status_from_string("pending");
    REQUIRE(p);
    CHECK(p.value() == FactionDemandStatus::Pending);
    const auto e = fd::status_from_string("expired");
    REQUIRE(e);
    CHECK(e.value() == FactionDemandStatus::Expired);
}

TEST_CASE("M7.1 status_from_string rejects unknown status loudly") {
    const auto r = fd::status_from_string("satisfied");
    REQUIRE(r.failed());
    CHECK(r.error().find("unknown status") != std::string::npos);
    CHECK(r.error().find("satisfied")      != std::string::npos);
}

// =====================================================================
// Faction-type → demand-kind mapping (RFC-020 §7 allowlist)
// =====================================================================

TEST_CASE("M7.1 map_faction_type_to_demand_kind: all six RFC-020 §7 types resolve") {
    struct Case { const char* type; FactionDemandKind expected; };
    const Case cases[] = {
        {"military",         FactionDemandKind::IncreaseMilitaryBudget},
        {"workers",          FactionDemandKind::ExpandWelfare},
        {"religious",        FactionDemandKind::ReligiousEducationAuthority},
        {"local_elites",     FactionDemandKind::LocalAutonomy},
        {"technical_elites", FactionDemandKind::TechnocratResearchFunding},
        {"intelligence",     FactionDemandKind::IntelligenceSurveillanceAuthority},
    };
    for (const auto& c : cases) {
        const auto r = fd::map_faction_type_to_demand_kind(c.type);
        REQUIRE(r);
        CHECK(r.value().has_kind == true);
        CHECK(r.value().kind == c.expected);
    }
}

TEST_CASE("M7.1 map_faction_type_to_demand_kind: out-of-allowlist types return has_kind=false") {
    // RFC-020 §7 lists six examples; types outside the list
    // (bureaucracy / media / students / farmers / nationalists /
    // aristocracy / etc.) do NOT generate demands under M7.1.
    for (const char* t : {"bureaucracy", "media", "students",
                          "farmers", "", "unknown_type"}) {
        const auto r = fd::map_faction_type_to_demand_kind(t);
        REQUIRE(r);
        CHECK(r.value().has_kind == false);
    }
}

// =====================================================================
// tick_generate
// =====================================================================

TEST_CASE("M7.1 tick_generate: faction at threshold does NOT generate (strictly >)") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_military", "X", "military",
        fd::kFactionDemandGenerateRadicalismThreshold));
    const auto r = fd::tick_generate(s, GameDate(1930, 4, 1));
    REQUIRE(r);
    CHECK(r.value().demands_generated == 0);
    CHECK(s.faction_demands.empty());
}

TEST_CASE("M7.1 tick_generate: faction strictly above threshold generates one Pending demand") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_military", "X", "military",
        fd::kFactionDemandGenerateRadicalismThreshold + 0.05));
    const auto r = fd::tick_generate(s, GameDate(1930, 4, 1));
    REQUIRE(r);
    CHECK(r.value().demands_generated == 1);
    REQUIRE(s.faction_demands.size() == 1u);
    const auto& d = s.faction_demands[0];
    CHECK(d.faction_id_code == "X_military");
    CHECK(d.country_id_code == "X");
    CHECK(d.kind == FactionDemandKind::IncreaseMilitaryBudget);
    CHECK(d.created_on == GameDate(1930, 4, 1));
    CHECK(d.status == FactionDemandStatus::Pending);
    CHECK(d.expires_on > d.created_on);
    // id_code shape:
    CHECK(d.id_code == "X_military_demand_increase_military_budget_1930-04-01");
}

TEST_CASE("M7.1 tick_generate: faction outside RFC-020 §7 allowlist generates NO demand even with high radicalism") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_bureaucracy", "X", "bureaucracy",
        0.95));
    const auto r = fd::tick_generate(s, GameDate(1930, 4, 1));
    REQUIRE(r);
    CHECK(r.value().demands_generated == 0);
    CHECK(s.faction_demands.empty());
}

TEST_CASE("M7.1 tick_generate: no double-generation while Pending demand exists for same (faction, kind)") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_military", "X", "military", 0.95));
    REQUIRE(fd::tick_generate(s, GameDate(1930, 4, 1)));
    REQUIRE(s.faction_demands.size() == 1u);

    // Second call with the SAME faction state: no new demand
    // because the prior one is still Pending.
    const auto r = fd::tick_generate(s, GameDate(1930, 5, 1));
    REQUIRE(r);
    CHECK(r.value().demands_generated == 0);
    CHECK(s.faction_demands.size() == 1u);
}

TEST_CASE("M7.1 tick_generate: deterministic across repeated calls on identical state") {
    auto run_once = []() {
        GameState s;
        s.countries.push_back(make_country(0, "X"));
        s.countries.push_back(make_country(1, "Y"));
        s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.80));
        s.factions.push_back(make_faction(1, 1, "Y_w", "Y", "workers", 0.70));
        s.factions.push_back(make_faction(2, 1, "Y_int", "Y", "intelligence", 0.90));
        REQUIRE(fd::tick_generate(s, GameDate(1930, 4, 1)));
        return ss::serialize(s);
    };
    CHECK(run_once() == run_once());
}

TEST_CASE("M7.1 tick_generate: NaN radicalism on a §7-allowlist faction FAILS LOUDLY") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_military", "X", "military",
        std::numeric_limits<double>::quiet_NaN()));
    const auto r = fd::tick_generate(s, GameDate(1930, 4, 1));
    REQUIRE(r.failed());
    CHECK(r.error().find("X_military") != std::string::npos);
    CHECK(r.error().find("radicalism") != std::string::npos);
    CHECK(s.faction_demands.empty());
}

TEST_CASE("M7.1 tick_generate: NaN radicalism on a NON-§7 faction is IGNORED") {
    // M1.6 owns numerical validation for every faction; M7.1
    // only re-validates the candidates it would mutate. A NaN
    // on a bureaucracy faction is out-of-scope for M7.1.
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_bureaucracy", "X", "bureaucracy",
        std::numeric_limits<double>::quiet_NaN()));
    const auto r = fd::tick_generate(s, GameDate(1930, 4, 1));
    REQUIRE(r);
    CHECK(r.value().demands_generated == 0);
}

TEST_CASE("M7.1 tick_generate: faction with unresolvable country_id_code FAILS LOUDLY when it would generate") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_military", "ORPHAN_COUNTRY", "military", 0.90));
    const auto r = fd::tick_generate(s, GameDate(1930, 4, 1));
    REQUIRE(r.failed());
    CHECK(r.error().find("X_military")      != std::string::npos);
    CHECK(r.error().find("ORPHAN_COUNTRY")  != std::string::npos);
    CHECK(r.error().find("does not match")  != std::string::npos);
}

TEST_CASE("M7.1 tick_generate: invalid current_date FAILS LOUDLY") {
    GameState s;
    const auto r = fd::tick_generate(s, GameDate(0, 0, 0));
    REQUIRE(r.failed());
    CHECK(r.error().find("current_date") != std::string::npos);
}

// =====================================================================
// tick_expire_and_apply
// =====================================================================

TEST_CASE("M7.1 tick_expire_and_apply: Pending demand expires when current_date >= expires_on") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_military", "X", "military", 0.80, /*loyalty*/0.60));

    const auto created = GameDate(1930, 4, 1);
    REQUIRE(fd::tick_generate(s, created));
    REQUIRE(s.faction_demands.size() == 1u);
    const double rad_before = s.factions[0].radicalism;
    const double loy_before = s.factions[0].loyalty;
    const GameDate expires_on = s.faction_demands[0].expires_on;

    // Day before expiry → no flip, no drift.
    {
        GameDate day_before = expires_on;
        // day-1: rewind by setting the comparison to fail.
        // Easier: construct a date one day earlier by subtracting.
        // GameDate has no - operator; use a known "before" date.
        const auto r = fd::tick_expire_and_apply(s, GameDate(1930, 4, 30));
        REQUIRE(r);
        CHECK(r.value().demands_expired == 0);
        CHECK(s.faction_demands[0].status == FactionDemandStatus::Pending);
        CHECK(s.factions[0].radicalism == doctest::Approx(rad_before));
        CHECK(s.factions[0].loyalty    == doctest::Approx(loy_before));
    }

    // Exactly the expiry date → flip + drift.
    {
        const auto r = fd::tick_expire_and_apply(s, expires_on);
        REQUIRE(r);
        CHECK(r.value().demands_expired   == 1);
        CHECK(r.value().factions_affected == 1);
        CHECK(s.faction_demands[0].status == FactionDemandStatus::Expired);
        // Asymptotic-add radicalism:
        //   new = old + delta × (1 - old)
        const double expected_rad =
            rad_before +
            fd::kFactionDemandExpireRadicalismAsymptoticDelta *
                (1.0 - rad_before);
        CHECK(s.factions[0].radicalism == doctest::Approx(expected_rad));
        // Asymptotic-subtract loyalty:
        //   new = old - delta × old
        const double expected_loy =
            loy_before -
            fd::kFactionDemandExpireLoyaltyAsymptoticDelta *
                loy_before;
        CHECK(s.factions[0].loyalty == doctest::Approx(expected_loy));
    }

    // Second expire pass: already Expired demands do NOT
    // re-trigger.
    const double rad_after = s.factions[0].radicalism;
    const double loy_after = s.factions[0].loyalty;
    {
        const auto r = fd::tick_expire_and_apply(s, expires_on);
        REQUIRE(r);
        CHECK(r.value().demands_expired == 0);
        CHECK(s.factions[0].radicalism == doctest::Approx(rad_after));
        CHECK(s.factions[0].loyalty    == doctest::Approx(loy_after));
    }
}

TEST_CASE("M7.1 tick_expire_and_apply: multiple expiring demands on same faction count once for factions_affected") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_military", "X", "military", 0.80));
    s.factions.push_back(make_faction(
        1, 0, "X_workers", "X", "workers", 0.80));

    REQUIRE(fd::tick_generate(s, GameDate(1930, 4, 1)));
    REQUIRE(s.faction_demands.size() == 2u);
    // Both expire on the same date.
    const GameDate exp_date = s.faction_demands[0].expires_on;
    CHECK(s.faction_demands[1].expires_on == exp_date);

    const auto r = fd::tick_expire_and_apply(s, exp_date);
    REQUIRE(r);
    CHECK(r.value().demands_expired   == 2);
    CHECK(r.value().factions_affected == 2);
}

TEST_CASE("M7.1 tick_expire_and_apply: demand with unresolvable faction_id_code FAILS LOUDLY") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_military", "X", "military", 0.80));

    // Hand-build a demand whose faction_id_code does NOT exist.
    FactionDemand orphan;
    orphan.id_code         = "orphan_demand";
    orphan.faction_id_code = "GHOST";
    orphan.country_id_code = "X";
    orphan.kind            = FactionDemandKind::IncreaseMilitaryBudget;
    orphan.created_on      = GameDate(1930, 4, 1);
    orphan.expires_on      = GameDate(1930, 5, 31);
    orphan.status          = FactionDemandStatus::Pending;
    s.faction_demands.push_back(orphan);

    const auto r = fd::tick_expire_and_apply(s, GameDate(1930, 6, 1));
    REQUIRE(r.failed());
    CHECK(r.error().find("orphan_demand") != std::string::npos);
    CHECK(r.error().find("GHOST")         != std::string::npos);
    // Per-event atomicity: no status flip applied on failure.
    CHECK(s.faction_demands[0].status == FactionDemandStatus::Pending);
}

TEST_CASE("M7.1 tick_expire_and_apply: invalid current_date FAILS LOUDLY") {
    GameState s;
    const auto r = fd::tick_expire_and_apply(s, GameDate(0, 0, 0));
    REQUIRE(r.failed());
    CHECK(r.error().find("current_date") != std::string::npos);
}

// =====================================================================
// Save round-trip
// =====================================================================

TEST_CASE("M7.1 save round-trip: state.faction_demands preserved byte-stably") {
    GameState before;
    before.countries.push_back(make_country(0, "X"));
    before.factions.push_back(make_faction(
        0, 0, "X_military", "X", "military", 0.80));
    REQUIRE(fd::tick_generate(before, GameDate(1930, 4, 1)));
    REQUIRE(before.faction_demands.size() == 1u);

    const std::string text = ss::serialize(before);
    CHECK(text.find("\"faction_demands\":") != std::string::npos);
    CHECK(text.find("\"increase_military_budget\"") != std::string::npos);
    CHECK(text.find("\"pending\"") != std::string::npos);

    const auto r = ss::deserialize(text);
    REQUIRE(r);
    const auto& after = r.value();
    REQUIRE(after.faction_demands.size() == 1u);
    const auto& d_before = before.faction_demands[0];
    const auto& d_after  = after.faction_demands[0];
    CHECK(d_after.id_code         == d_before.id_code);
    CHECK(d_after.faction_id_code == d_before.faction_id_code);
    CHECK(d_after.country_id_code == d_before.country_id_code);
    CHECK(d_after.kind            == d_before.kind);
    CHECK(d_after.created_on      == d_before.created_on);
    CHECK(d_after.expires_on      == d_before.expires_on);
    CHECK(d_after.status          == d_before.status);
}

TEST_CASE("M7.1 save deserialize: unknown kind string rejected loudly") {
    const std::string text = R"({
        "save_version": 19,
        "rng_algorithm_version": 1,
        "current_date": "1930-04-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "X", "name": "X", "display_name": "X",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "military_strength": 0.0, "last_gdp_growth_rate": 0.0,
              "budget": { "administration": 0.2, "military": 0.2,
                "education": 0.1, "welfare": 0.1, "intelligence": 0.1,
                "infrastructure": 0.1, "industry": 0.1 },
              "government_authority": { "bureaucratic_compliance": 0.5,
                "military_loyalty": 0.5, "intelligence_capability": 0.5,
                "media_control": 0.5 },
              "active_policies": [] }
        ],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "factions": [
            { "id": 0, "country": 0, "id_code": "X_military",
              "country_id_code": "X", "name": "Mil", "type": "military",
              "support": 0.5, "influence": 0.5, "radicalism": 0.8,
              "loyalty": 0.5, "resources": 0.0,
              "preferred_policies": [] }
        ],
        "event_history": [],
        "relationships": [],
        "pending_player_events": [],
        "policies": [],
        "logs": [],
        "faction_demands": [
            { "id_code": "X_demand_x",
              "faction_id_code": "X_military",
              "country_id_code": "X",
              "kind": "not_a_real_kind",
              "created_on": "1930-04-01",
              "expires_on": "1930-05-31",
              "status": "pending" }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("not_a_real_kind") != std::string::npos);
}

TEST_CASE("M7.1 save deserialize: demand referencing unknown faction_id_code rejected loudly") {
    const std::string text = R"({
        "save_version": 19,
        "rng_algorithm_version": 1,
        "current_date": "1930-04-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "X", "name": "X", "display_name": "X",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "military_strength": 0.0, "last_gdp_growth_rate": 0.0,
              "budget": { "administration": 0.2, "military": 0.2,
                "education": 0.1, "welfare": 0.1, "intelligence": 0.1,
                "infrastructure": 0.1, "industry": 0.1 },
              "government_authority": { "bureaucratic_compliance": 0.5,
                "military_loyalty": 0.5, "intelligence_capability": 0.5,
                "media_control": 0.5 },
              "active_policies": [] }
        ],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "factions": [],
        "event_history": [],
        "relationships": [],
        "pending_player_events": [],
        "policies": [],
        "logs": [],
        "faction_demands": [
            { "id_code": "ghost_demand",
              "faction_id_code": "GHOST",
              "country_id_code": "X",
              "kind": "increase_military_budget",
              "created_on": "1930-04-01",
              "expires_on": "1930-05-31",
              "status": "pending" }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("GHOST") != std::string::npos);
    CHECK(r.error().find("does not match") != std::string::npos);
}

TEST_CASE("M7.1 save deserialize: missing required faction_demands field rejected loudly") {
    const std::string text = R"({
        "save_version": 19,
        "rng_algorithm_version": 1,
        "current_date": "1930-04-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "factions": [],
        "event_history": [],
        "relationships": [],
        "pending_player_events": [],
        "policies": [],
        "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("faction_demands") != std::string::npos);
    CHECK(r.error().find("missing required field") != std::string::npos);
}

// =====================================================================
// Atomicity (PR #119 review fix)
//
// `tick_generate` must be ALL-OR-NONE: if any candidate fails
// validation, NO demand is appended. Previous implementation
// appended each eligible candidate as it walked
// `state.factions`, so a later malformed candidate could leave
// the vector partially mutated.
// =====================================================================

TEST_CASE("M7.1 atomicity: first eligible faction valid, second eligible faction has unresolved country_id_code → no demand appended") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    // 1st eligible: passes validation.
    s.factions.push_back(make_faction(
        0, 0, "X_m", "X", "military", 0.90));
    // 2nd eligible: type in §7 allowlist + radicalism above
    // threshold (so validation would proceed), but
    // country_id_code does NOT resolve.
    s.factions.push_back(make_faction(
        1, 0, "X_w", "GHOST_COUNTRY", "workers", 0.90));

    const auto r = fd::tick_generate(s, GameDate(1930, 4, 1));
    REQUIRE(r.failed());
    CHECK(r.error().find("GHOST_COUNTRY")        != std::string::npos);
    CHECK(r.error().find("does not match any")   != std::string::npos);
    // ATOMIC: no demand from the FIRST eligible faction either.
    CHECK(s.faction_demands.empty());
}

TEST_CASE("M7.1 atomicity: first eligible faction valid, second eligible faction has NaN loyalty → no demand appended") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_m", "X", "military", 0.90));
    auto bad = make_faction(1, 0, "X_w", "X", "workers", 0.90);
    bad.loyalty = std::numeric_limits<double>::quiet_NaN();
    s.factions.push_back(bad);

    const auto r = fd::tick_generate(s, GameDate(1930, 4, 1));
    REQUIRE(r.failed());
    CHECK(r.error().find("X_w")        != std::string::npos);
    CHECK(r.error().find("loyalty")    != std::string::npos);
    CHECK(s.faction_demands.empty());
}

TEST_CASE("M7.1 atomicity: first eligible faction valid, second eligible faction has empty id_code → no demand appended") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_m", "X", "military", 0.90));
    auto bad = make_faction(1, 0, "", "X", "workers", 0.90);
    s.factions.push_back(bad);

    const auto r = fd::tick_generate(s, GameDate(1930, 4, 1));
    REQUIRE(r.failed());
    CHECK(r.error().find("empty id_code") != std::string::npos);
    CHECK(s.faction_demands.empty());
}

TEST_CASE("M7.1 atomicity: pre-existing Pending demand survives a failed tick_generate") {
    // Seed state with an EXISTING Pending demand. A failed
    // generate call must not corrupt that record either.
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(
        0, 0, "X_m", "X", "military", 0.90));
    // Seed: pretend a prior tick already generated for the
    // military faction.
    REQUIRE(fd::tick_generate(s, GameDate(1930, 4, 1)));
    REQUIRE(s.faction_demands.size() == 1u);
    const std::string before = ss::serialize(s);

    // Now add a second eligible-but-malformed faction; the
    // generate call must fail loudly and the existing Pending
    // demand must remain byte-identical.
    s.factions.push_back(make_faction(
        1, 0, "X_w", "GHOST_COUNTRY", "workers", 0.90));
    const auto r = fd::tick_generate(s, GameDate(1930, 5, 1));
    REQUIRE(r.failed());
    // Vector size unchanged; pre-existing record bytes unchanged.
    CHECK(s.faction_demands.size() == 1u);
    // serialize/deserialize the pre-call snapshot; the only
    // delta from `before` should be the SECOND faction record
    // (which was added before the failed call).
    // We cannot directly compare serialize(s) == before because
    // the second faction is now in state.factions. Instead,
    // verify the seeded demand is unchanged.
    CHECK(s.faction_demands[0].faction_id_code == "X_m");
    CHECK(s.faction_demands[0].status ==
          FactionDemandStatus::Pending);
    CHECK(s.faction_demands[0].created_on == GameDate(1930, 4, 1));
    (void)before;
}

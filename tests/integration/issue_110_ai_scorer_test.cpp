// Issue #110: integration tests that prove the AI policy scorer
// actually drives selection in ordinary simulation flow rather than
// returning the first policy in vector order.
//
// Each test constructs a country / state with a specific RFC-named
// pressure (low stability, budget deficit, hostile-neighbour threat,
// military-strength gap, radical interest groups, already-active
// policy), then calls `ai::select_policies(state)` and asserts the
// chosen policy id_code reflects that pressure.
//
// These tests use hand-built PolicyData with real effects, NOT
// `make_policy(id)`-style empty shells (which would score 0 across
// the board and reduce to vector-order tie-break — the previous RCR-1
// behaviour issue #110 §1 explicitly rejected).

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/ai_policy.hpp"
#include "leviathan/systems/effect_desire.hpp"
#include "leviathan/systems/policy_system.hpp"

using leviathan::core::ActivePolicy;
using leviathan::core::CountryId;
using leviathan::core::CountryRelation;
using leviathan::core::CountryState;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::InterestGroupKind;
using leviathan::core::InterestGroupState;
using leviathan::core::PolicyData;
using leviathan::core::PolicyEffect;
namespace ai = leviathan::systems::ai_policy;
namespace desire = leviathan::systems::effect_desire;

namespace {

PolicyData make_policy_with_effects(
        std::string                id_code,
        std::string                category,
        std::vector<PolicyEffect>  effects) {
    PolicyData p;
    p.id_code  = std::move(id_code);
    p.name     = p.id_code;
    p.category = std::move(category);
    p.effects  = std::move(effects);
    return p;
}

PolicyEffect eff(std::string target, std::string op, double value) {
    PolicyEffect e;
    e.target = std::move(target);
    e.op     = std::move(op);
    e.value  = value;
    return e;
}

// Country baseline modelled after a typical authored 1930s state
// (compare data/countries/germany.json). Setting fiscal_capacity /
// administrative_efficiency / central_control to realistic mid-range
// values keeps the scorer's "low capacity → wants up" terms from
// dominating when the test really wants to exercise a different
// pressure axis.
CountryState make_country_with_id(std::size_t index, std::string id_code) {
    CountryState c;
    c.id      = CountryId{static_cast<CountryId::underlying_type>(index)};
    c.id_code = std::move(id_code);
    c.name    = c.id_code;
    c.fiscal_capacity            = 0.70;
    c.administrative_efficiency  = 0.65;
    c.central_control            = 0.65;
    c.legitimacy                 = 0.60;
    c.stability                  = 0.55;
    return c;
}

// Common policy set for scorer cases. Each entry mirrors a real
// data/policies/*.json file shape (id_code + category + effects).
std::vector<PolicyData> realistic_policy_set() {
    std::vector<PolicyData> ps;
    // index 0: tax / revenue.
    ps.push_back(make_policy_with_effects(
        "raise_taxes", "tax", {
            eff("country.legal_tax_burden", "add",  0.05),
            eff("faction:workers.support",  "add", -0.05),
        }));
    // index 1: welfare / stabilising.
    ps.push_back(make_policy_with_effects(
        "expand_welfare", "welfare", {
            eff("country.stability",      "add",  0.04),
            eff("faction:workers.support","add",  0.08),
            eff("country.budget_balance", "add", -0.07),
        }));
    // index 2: military.
    ps.push_back(make_policy_with_effects(
        "increase_military_budget", "budget", {
            eff("country.military_power",   "add",  0.03),
            eff("faction:military.support", "add",  0.08),
        }));
    // index 3: fiscal consolidation (revenue-improving without raising taxes).
    ps.push_back(make_policy_with_effects(
        "fiscal_consolidation", "tax", {
            eff("country.fiscal_capacity",  "add",  0.03),
            eff("country.budget_balance",   "add",  0.02),
        }));
    return ps;
}

GameState make_state_with(std::vector<CountryState> countries,
                          std::vector<PolicyData>   policies) {
    GameState s;
    s.countries = std::move(countries);
    s.policies  = std::move(policies);
    return s;
}

}  // namespace

// =====================================================================
// Issue #110 §1: scorer-driven selection (not state.policies.front())
// =====================================================================

TEST_CASE("Issue #110: low-stability country picks a stabilising policy") {
    auto c = make_country_with_id(0, "GER");
    c.stability  = 0.10;   // very low — wants stabilising help
    c.legitimacy = 0.60;   // OK
    c.gdp        = 1000.0;
    c.budget_balance = 0.0;   // no budget pressure
    c.legal_tax_burden = 0.20;

    auto state = make_state_with({c}, realistic_policy_set());

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() >= 1u);
    // expand_welfare boosts country.stability; with stability=0.1 the
    // (1 - stability) = 0.9 desire term wins over the tax / budget /
    // military alternatives that score near zero under these inputs.
    CHECK(r.value()[0].policy_id_code == "expand_welfare");
}

TEST_CASE("Issue #110: budget-crisis country picks a revenue policy") {
    auto c = make_country_with_id(0, "FRA");
    c.stability      = 0.50;   // mid
    c.gdp            = 1000.0;
    c.budget_balance = -200.0; // severe deficit → budget pressure clamped to 1.0
    c.legal_tax_burden = 0.10; // headroom to raise taxes

    auto state = make_state_with({c}, realistic_policy_set());

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() >= 1u);
    // raise_taxes scores 0.05 × (1.0 - 0.5×0.1) = 0.0475;
    // expand_welfare scores 0.04 × 0.5 + (-0.07) × 1.0 = -0.05;
    // increase_military_budget = 0.0 (no threat); fiscal_consolidation
    // ~0.05 — close but raise_taxes wins by vector-order tie-break OR
    // by a hair on the legal_tax_burden term. We assert one of the
    // two revenue policies — never welfare/military.
    const auto& chosen = r.value()[0].policy_id_code;
    CHECK((chosen == "raise_taxes" || chosen == "fiscal_consolidation"));
}

TEST_CASE("Issue #110: high-incoming-threat country picks a military policy"
          " (proves state.relationships is read by AI)") {
    auto target = make_country_with_id(0, "POL");
    target.stability       = 0.50;
    target.legitimacy      = 0.60;
    target.gdp             = 1000.0;
    target.budget_balance  = 0.0;
    target.legal_tax_burden = 0.20;
    target.threat_perception = 0.0;  // no internal threat signal
    target.military_strength = 50.0;

    auto neighbour = make_country_with_id(1, "GER");
    neighbour.military_strength = 50.0;

    // Hostile neighbour → relationship records inbound threat = 0.9.
    CountryRelation rel;
    rel.from         = CountryId{1};  // GER
    rel.to           = CountryId{0};  // POL
    rel.relationship = -0.80;
    rel.threat       = 0.90;

    auto state = make_state_with(
        {std::move(target), std::move(neighbour)}, realistic_policy_set());
    state.relationships.push_back(rel);
    state.player_country = CountryId{1};   // GER is player; POL the AI

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() >= 1u);
    CHECK(r.value()[0].country == CountryId{0});  // POL only
    // increase_military_budget effect on country.military_power scores
    // 0.03 × (effective_threat 0.9 + 0.5×military_gap_norm 0.0) = 0.027.
    // No other policy in the set produces a comparable positive score
    // under these inputs.
    CHECK(r.value()[0].policy_id_code == "increase_military_budget");
}

TEST_CASE("Issue #110: military_strength disparity also drives military pick"
          " (proves military_strength is read by AI, with zero relationship threat)") {
    auto target = make_country_with_id(0, "POL");
    // Internal pressures intentionally MID-HIGH so the disparity is
    // the dominant pressure axis: stability + legitimacy high enough
    // that the stabilising-policy desire term sits below the
    // military-gap desire term, but not SO high that the country
    // dips below the issue #112 pressure threshold (0.80).
    target.stability         = 0.75;
    target.legitimacy        = 0.75;
    target.gdp               = 1000.0;
    target.budget_balance    = 0.0;
    target.legal_tax_burden  = 0.20;
    target.threat_perception = 0.0;
    target.military_strength = 10.0;   // weak

    auto neighbour = make_country_with_id(1, "SOV");
    neighbour.military_strength = 90.0;  // strong neighbour

    // Relationship with zero threat — the disparity itself must
    // produce the desire to militarise.
    CountryRelation rel;
    rel.from         = CountryId{1};
    rel.to           = CountryId{0};
    rel.relationship = -0.20;
    rel.threat       = 0.0;

    auto state = make_state_with(
        {std::move(target), std::move(neighbour)}, realistic_policy_set());
    state.relationships.push_back(rel);
    state.player_country = CountryId{1};

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() >= 1u);
    // military_gap = 90 - 10 = 80, gap_norm = 0.8 → desire = 0.5 × 0.8 = 0.4.
    // increase_military_budget score 0.03 × 0.4 = 0.012, dominating
    // the near-zero alternatives.
    CHECK(r.value()[0].policy_id_code == "increase_military_budget");
}

TEST_CASE("Issue #112 residual: stronger friendly neighbour does NOT create military-gap desire") {
    auto target = make_country_with_id(0, "POL");
    target.stability         = 0.775;
    target.legitimacy        = 0.775;
    target.gdp               = 1000.0;
    target.budget_balance    = 0.0;
    target.legal_tax_burden  = 0.20;
    target.threat_perception = 0.0;
    target.military_strength = 10.0;

    auto ally = make_country_with_id(1, "FRA");
    ally.military_strength = 90.0;

    CountryRelation friendly;
    friendly.from         = CountryId{1};
    friendly.to           = CountryId{0};
    friendly.relationship = 0.80;
    friendly.threat       = 0.0;

    auto state = make_state_with(
        {target, std::move(ally)}, realistic_policy_set());
    state.relationships.push_back(friendly);
    state.player_country = CountryId{1};

    CHECK(desire::for_country(state.countries[0],
                              "country.military_power",
                              state) == doctest::Approx(0.0));

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    CHECK(r.value().empty());

    friendly.relationship = -0.20;
    state.relationships[0] = friendly;
    CHECK(desire::for_country(state.countries[0],
                              "country.military_power",
                              state) == doctest::Approx(0.4));
    const auto hostile = ai::select_policies(state);
    REQUIRE(hostile);
    REQUIRE(hostile.value().size() >= 1u);
    CHECK(hostile.value()[0].policy_id_code == "increase_military_budget");
}

TEST_CASE("Issue #110: high-radicalism interest groups sway AI toward welfare") {
    auto c = make_country_with_id(0, "ITA");
    c.stability         = 0.70;   // OK
    c.legitimacy        = 0.70;
    c.gdp               = 1000.0;
    c.budget_balance    = 0.0;
    c.legal_tax_burden  = 0.20;
    c.corruption        = 0.10;

    // Two radical IGs with high influence pull toward concession.
    InterestGroupState ig1;
    ig1.id_code    = "ITA_workers";
    ig1.kind       = InterestGroupKind::Workers;
    ig1.country    = CountryId{0};
    ig1.influence  = 0.70;
    ig1.loyalty    = 0.30;
    ig1.radicalism = 0.90;
    InterestGroupState ig2;
    ig2.id_code    = "ITA_students";
    ig2.kind       = InterestGroupKind::Students;
    ig2.country    = CountryId{0};
    ig2.influence  = 0.70;
    ig2.loyalty    = 0.30;
    ig2.radicalism = 0.90;

    auto state = make_state_with({c}, realistic_policy_set());
    state.interest_groups.push_back(std::move(ig1));
    state.interest_groups.push_back(std::move(ig2));

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() >= 1u);
    // expand_welfare (welfare category) gets +0.7 × 0.9 = +0.63 from the
    // IG-radicalism pressure term — overwhelming the otherwise-modest
    // stability term and any near-zero alternatives.
    CHECK(r.value()[0].policy_id_code == "expand_welfare");
}

TEST_CASE("Issue #110: no-stacking skips already-active policy and picks next-best") {
    auto c = make_country_with_id(0, "FRA");
    c.stability        = 0.50;
    c.gdp              = 1000.0;
    c.budget_balance   = -200.0;   // budget crisis — raise_taxes would normally win
    c.legal_tax_burden = 0.10;

    // raise_taxes is already active and unexpired.
    ActivePolicy ap;
    ap.policy_id_code = "raise_taxes";
    ap.expires_on     = GameDate(1930, 3, 1);
    c.active_policies.push_back(ap);

    GameState state;
    state.current_date = GameDate(1930, 1, 15);   // before expiry
    state.countries.push_back(std::move(c));
    state.policies = realistic_policy_set();

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() >= 1u);
    // raise_taxes is stacked → excluded. fiscal_consolidation is the
    // remaining revenue-improving policy: 0.03 × (1 - fiscal_capacity 0)
    // + 0.02 × 1.0 = 0.05 under budget crisis. It must NOT be raise_taxes.
    CHECK(r.value()[0].policy_id_code != "raise_taxes");
    CHECK((r.value()[0].policy_id_code == "fiscal_consolidation" ||
           r.value()[0].policy_id_code == "expand_welfare" ||
           r.value()[0].policy_id_code == "increase_military_budget"));
    // Specifically: fiscal_consolidation is the best of the remaining
    // three for a deficit-stricken country (welfare worsens budget,
    // military gets zero threat signal).
    CHECK(r.value()[0].policy_id_code == "fiscal_consolidation");
}

TEST_CASE("Issue #110: expired policy is re-eligible for selection") {
    auto c = make_country_with_id(0, "FRA");
    c.stability        = 0.50;
    c.gdp              = 1000.0;
    c.budget_balance   = -200.0;
    c.legal_tax_burden = 0.10;

    // raise_taxes was applied but has already expired.
    ActivePolicy ap;
    ap.policy_id_code = "raise_taxes";
    ap.expires_on     = GameDate(1930, 1, 10);
    c.active_policies.push_back(ap);

    GameState state;
    state.current_date = GameDate(1930, 1, 15);   // after expiry
    state.countries.push_back(std::move(c));
    state.policies = realistic_policy_set();

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() >= 1u);
    // raise_taxes is expired → eligible again, and best for a deficit
    // country with low tax burden.
    CHECK(r.value()[0].policy_id_code == "raise_taxes");
}

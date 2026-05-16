#include <doctest/doctest.h>

#include <limits>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/policy_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameState;
using leviathan::core::PolicyData;
using leviathan::core::PolicyEffect;
namespace ps = leviathan::systems::policy;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------

namespace {

CountryState germany_baseline() {
    CountryState g;
    g.id           = CountryId{0};
    g.id_code      = "GER";
    g.name         = "Germany";
    g.gdp                       = 100.0;
    g.tax_revenue               = 18.5;
    g.budget_balance            = -3.2;
    g.legal_tax_burden          = 0.20;
    g.fiscal_capacity           = 0.50;
    g.administrative_efficiency = 0.55;
    g.central_control           = 0.60;
    g.corruption                = 0.25;
    g.stability                 = 0.55;
    g.legitimacy                = 0.55;
    g.military_power            = 0.50;
    g.threat_perception         = 0.30;
    g.budget.administration     = 0.25;
    g.budget.military           = 0.35;
    g.budget.education          = 0.10;
    g.budget.welfare            = 0.10;
    g.budget.intelligence       = 0.05;
    g.budget.infrastructure     = 0.10;
    g.budget.industry           = 0.05;
    return g;
}

FactionState faction(int id, CountryId country, std::string type,
                     double support = 0.50,
                     double influence = 0.50,
                     double radicalism = 0.30,
                     double loyalty = 0.50,
                     double resources = 1.0) {
    FactionState f;
    f.id              = FactionId{id};
    f.country         = country;
    f.id_code         = type + "_test";
    f.country_id_code = "GER";
    f.name            = type;
    f.type            = std::move(type);
    f.support         = support;
    f.influence       = influence;
    f.radicalism      = radicalism;
    f.loyalty         = loyalty;
    f.resources       = resources;
    return f;
}

}  // namespace

// =====================================================================
// Happy paths
// =====================================================================

TEST_CASE("apply: country.military_power add increases the field") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.military_power", "add", 0.03});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.ok());
    CHECK(r.value().effects_applied == 1);
    CHECK(state.countries[0].military_power == doctest::Approx(0.53));
}

TEST_CASE("apply: country.stability set replaces the field") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.stability", "set", 0.42});

    REQUIRE(ps::apply_policy_effects(state, CountryId{0}, p).ok());
    CHECK(state.countries[0].stability == doctest::Approx(0.42));
}

TEST_CASE("apply: country.gdp add accepts non-ratio absolute field") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.gdp", "add", 25.0});

    REQUIRE(ps::apply_policy_effects(state, CountryId{0}, p).ok());
    CHECK(state.countries[0].gdp == doctest::Approx(125.0));
}

TEST_CASE("apply: country.budget.military add hits the budget sub-object") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.budget.military", "add", 0.05});

    REQUIRE(ps::apply_policy_effects(state, CountryId{0}, p).ok());
    CHECK(state.countries[0].budget.military == doctest::Approx(0.40));
}

// =====================================================================
// Clamping
// =====================================================================

TEST_CASE("apply: ratio field clamps to 1.0 on overshoot") {
    GameState state;
    auto g = germany_baseline();
    g.stability = 0.90;
    state.countries.push_back(g);

    PolicyData p;
    p.effects.push_back({"country.stability", "add", 0.50});

    REQUIRE(ps::apply_policy_effects(state, CountryId{0}, p).ok());
    CHECK(state.countries[0].stability == doctest::Approx(1.0));
}

TEST_CASE("apply: ratio field clamps to 0.0 on undershoot") {
    GameState state;
    auto g = germany_baseline();
    g.corruption = 0.10;
    state.countries.push_back(g);

    PolicyData p;
    p.effects.push_back({"country.corruption", "add", -0.50});

    REQUIRE(ps::apply_policy_effects(state, CountryId{0}, p).ok());
    CHECK(state.countries[0].corruption == doctest::Approx(0.0));
}

TEST_CASE("apply: absolute fields are NOT clamped") {
    GameState state;
    auto g = germany_baseline();
    g.budget_balance = 1.0;
    state.countries.push_back(g);

    PolicyData p;
    p.effects.push_back({"country.budget_balance", "add", -5.0});

    REQUIRE(ps::apply_policy_effects(state, CountryId{0}, p).ok());
    CHECK(state.countries[0].budget_balance == doctest::Approx(-4.0));
}

TEST_CASE("apply: set on a ratio field clamps if value out of range") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.stability", "set", 1.7});

    REQUIRE(ps::apply_policy_effects(state, CountryId{0}, p).ok());
    CHECK(state.countries[0].stability == doctest::Approx(1.0));
}

// =====================================================================
// Faction broadcast
// =====================================================================

TEST_CASE("apply: faction broadcast updates every matching faction") {
    GameState state;
    state.countries.push_back(germany_baseline());
    state.factions.push_back(faction(0, CountryId{0}, "military", 0.40));
    state.factions.push_back(faction(1, CountryId{0}, "workers",  0.50));
    state.factions.push_back(faction(2, CountryId{0}, "military", 0.30));

    PolicyData p;
    p.effects.push_back({"faction:military.support", "add", 0.10});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.ok());
    CHECK(r.value().faction_targets_updated == 2);
    CHECK(state.factions[0].support == doctest::Approx(0.50));  // military
    CHECK(state.factions[1].support == doctest::Approx(0.50));  // workers (unchanged)
    CHECK(state.factions[2].support == doctest::Approx(0.40));  // military
}

TEST_CASE("apply: faction broadcast skips factions in other countries") {
    GameState state;
    state.countries.push_back(germany_baseline());
    auto fra = germany_baseline();
    fra.id      = CountryId{1};
    fra.id_code = "FRA";
    state.countries.push_back(fra);

    state.factions.push_back(faction(0, CountryId{0}, "military", 0.40));
    state.factions.push_back(faction(1, CountryId{1}, "military", 0.50));

    PolicyData p;
    p.effects.push_back({"faction:military.support", "add", 0.10});

    REQUIRE(ps::apply_policy_effects(state, CountryId{0}, p).ok());
    CHECK(state.factions[0].support == doctest::Approx(0.50));  // GER updated
    CHECK(state.factions[1].support == doctest::Approx(0.50));  // FRA untouched
}

TEST_CASE("apply: faction broadcast with zero matches is a silent no-op") {
    GameState state;
    state.countries.push_back(germany_baseline());
    state.factions.push_back(faction(0, CountryId{0}, "workers", 0.50));

    PolicyData p;
    p.effects.push_back({"faction:military.support", "add", 0.10});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.ok());
    CHECK(r.value().effects_applied == 1);
    CHECK(r.value().faction_targets_updated == 0);
    CHECK(state.factions[0].support == doctest::Approx(0.50));
}

TEST_CASE("apply: faction.resources is an absolute field (no clamp)") {
    GameState state;
    state.countries.push_back(germany_baseline());
    state.factions.push_back(faction(0, CountryId{0}, "intelligence",
                                     0.5, 0.5, 0.2, 0.5, /*resources=*/0.8));

    PolicyData p;
    p.effects.push_back({"faction:intelligence.resources", "add", 0.5});

    REQUIRE(ps::apply_policy_effects(state, CountryId{0}, p).ok());
    CHECK(state.factions[0].resources == doctest::Approx(1.3));
}

// =====================================================================
// Multi-effect policy
// =====================================================================

TEST_CASE("apply: a policy with multiple effects applies them in order") {
    GameState state;
    state.countries.push_back(germany_baseline());
    state.factions.push_back(faction(0, CountryId{0}, "military", 0.40));
    state.factions.push_back(faction(1, CountryId{0}, "workers",  0.50));

    PolicyData p;
    p.effects.push_back({"country.military_power",   "add",  0.03});
    p.effects.push_back({"faction:military.support", "add",  0.08});
    p.effects.push_back({"faction:workers.support",  "add", -0.03});
    p.effects.push_back({"country.budget_balance",   "add", -0.10});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.ok());
    CHECK(r.value().effects_applied == 4);
    CHECK(r.value().faction_targets_updated == 2);
    CHECK(state.countries[0].military_power == doctest::Approx(0.53));
    CHECK(state.factions[0].support         == doctest::Approx(0.48));
    CHECK(state.factions[1].support         == doctest::Approx(0.47));
    CHECK(state.countries[0].budget_balance == doctest::Approx(-3.3));
}

TEST_CASE("apply: empty effects array applies zero effects successfully") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.ok());
    CHECK(r.value().effects_applied == 0);
}

// =====================================================================
// Error paths
// =====================================================================

TEST_CASE("apply: invalid actor CountryId is rejected") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.stability", "add", 0.01});

    const auto r = ps::apply_policy_effects(state, CountryId{5}, p);
    REQUIRE(r.failed());
    CHECK(r.error().find("CountryId 5") != std::string::npos);
}

TEST_CASE("apply: default-constructed CountryId (invalid) is rejected") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.stability", "add", 0.01});

    const auto r = ps::apply_policy_effects(state, CountryId{}, p);
    REQUIRE(r.failed());
}

TEST_CASE("apply: unknown country field is rejected") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.does_not_exist", "add", 0.01});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.failed());
    CHECK(r.error().find("country") != std::string::npos);
    CHECK(r.error().find("does_not_exist") != std::string::npos);
}

TEST_CASE("apply: unknown budget category is rejected") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.budget.research", "add", 0.05});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.failed());
    CHECK(r.error().find("budget") != std::string::npos);
    CHECK(r.error().find("research") != std::string::npos);
}

TEST_CASE("apply: unknown faction field is rejected") {
    GameState state;
    state.countries.push_back(germany_baseline());
    state.factions.push_back(faction(0, CountryId{0}, "military"));

    PolicyData p;
    p.effects.push_back({"faction:military.morale", "add", 0.1});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.failed());
    CHECK(r.error().find("faction") != std::string::npos);
    CHECK(r.error().find("morale")  != std::string::npos);
}

TEST_CASE("apply: unknown faction field is caught even when no factions match") {
    // Critical pre-flight property: a typo in the field name fails
    // even if no faction of that type belongs to the actor.
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"faction:military.morale", "add", 0.1});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.failed());
    CHECK(r.error().find("morale") != std::string::npos);
}

TEST_CASE("apply: unrecognised target syntax is rejected") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"province.berlin.population", "add", 100.0});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.failed());
    CHECK(r.error().find("unrecognised target syntax") != std::string::npos);
}

TEST_CASE("apply: malformed faction target (missing field) rejected") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"faction:military", "add", 0.1});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.failed());
}

TEST_CASE("apply: unrecognised op is rejected") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.stability", "multiply", 1.1});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.failed());
    CHECK(r.error().find("multiply") != std::string::npos);
    CHECK(r.error().find("add")      != std::string::npos);
}

// =====================================================================
// Atomicity
// =====================================================================

TEST_CASE("apply: non-finite effect value is rejected at pre-flight") {
    // PR #16 review: a manually constructed PolicyData carrying NaN /
    // Inf must not slip past the DataLoader and corrupt state. The
    // DataLoader already rejects non-finite via require_number; this
    // test pins the same guarantee at the PolicySystem entry point.
    GameState state;
    state.countries.push_back(germany_baseline());
    const auto before_stability = state.countries[0].stability;

    PolicyData p;
    p.effects.push_back({"country.stability", "add",
                         std::numeric_limits<double>::quiet_NaN()});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.failed());
    CHECK(r.error().find("not finite") != std::string::npos);
    CHECK(state.countries[0].stability == doctest::Approx(before_stability));
}

TEST_CASE("apply: positive infinity effect value is rejected at pre-flight") {
    GameState state;
    state.countries.push_back(germany_baseline());

    PolicyData p;
    p.effects.push_back({"country.gdp", "add",
                         std::numeric_limits<double>::infinity()});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.failed());
    CHECK(r.error().find("not finite") != std::string::npos);
}

TEST_CASE("apply: a failure in any effect leaves state unchanged") {
    GameState state;
    state.countries.push_back(germany_baseline());
    state.factions.push_back(faction(0, CountryId{0}, "military", 0.40));

    const auto before_stability   = state.countries[0].stability;
    const auto before_military    = state.countries[0].military_power;
    const auto before_corruption  = state.countries[0].corruption;
    const auto before_fac_support = state.factions[0].support;

    PolicyData p;
    // Effect 0 would succeed if we didn't pre-flight.
    p.effects.push_back({"country.stability",       "add",  0.10});
    // Effect 1 would succeed.
    p.effects.push_back({"country.military_power",  "set",  0.95});
    // Effect 2 would succeed.
    p.effects.push_back({"faction:military.support","add",  0.10});
    // Effect 3 fails pre-flight. Nothing should apply.
    p.effects.push_back({"country.does_not_exist",  "add",  0.05});
    // Effect 4 also bad, but resolution stops at the first failure.
    p.effects.push_back({"country.corruption",      "ohno", 0.0});

    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.failed());
    CHECK(state.countries[0].stability      == doctest::Approx(before_stability));
    CHECK(state.countries[0].military_power == doctest::Approx(before_military));
    CHECK(state.countries[0].corruption     == doctest::Approx(before_corruption));
    CHECK(state.factions[0].support         == doctest::Approx(before_fac_support));
}

#include <doctest/doctest.h>

#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/stability_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameState;
namespace st_sys = leviathan::systems::stability;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------

namespace {

CountryState country_with(int id,
                          double stability,
                          double legitimacy,
                          double corruption) {
    CountryState c;
    c.id          = CountryId{id};
    c.id_code     = "GER";
    c.name        = "Germany";
    c.stability   = stability;
    c.legitimacy  = legitimacy;
    c.corruption  = corruption;
    // Other fields default to 0; tick() doesn't read them.
    return c;
}

FactionState faction_with(int id, CountryId country,
                          double support, double radicalism) {
    FactionState f;
    f.id              = FactionId{id};
    f.country         = country;
    f.id_code         = "f_test";
    f.country_id_code = "GER";
    f.name            = "test";
    f.type            = "military";
    f.support         = support;
    f.influence       = 0.5;
    f.radicalism      = radicalism;
    f.loyalty         = 0.5;
    f.resources       = 1.0;
    return f;
}

}  // namespace

// =====================================================================
// One-step exact arithmetic
// =====================================================================

TEST_CASE("tick: one-step delta matches the documented formula") {
    GameState state;
    state.countries.push_back(country_with(/*id=*/0,
                                           /*stability=*/0.50,
                                           /*legitimacy=*/0.60,
                                           /*corruption=*/0.20));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*support=*/0.60,
                                          /*radicalism=*/0.20));

    // raw_target = 0.5*0.60 + 0.5*0.60 - 0.3*0.20 - 0.2*0.20
    //            = 0.30   + 0.30    - 0.06     - 0.04   = 0.50
    // target = clamp(0.50, 0, 1) = 0.50
    // delta  = (0.50 - 0.50) * 0.10 = 0.0
    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().previous_stability == doctest::Approx(0.50));
    CHECK(r.value().target_stability   == doctest::Approx(0.50));
    CHECK(r.value().new_stability      == doctest::Approx(0.50));
    CHECK(state.countries[0].stability == doctest::Approx(0.50));
}

TEST_CASE("tick: pulls stability up toward a higher target") {
    GameState state;
    state.countries.push_back(country_with(0,
                                           /*stability=*/0.30,
                                           /*legitimacy=*/0.80,
                                           /*corruption=*/0.10));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*support=*/0.80,
                                          /*radicalism=*/0.10));

    // raw_target = 0.5*0.80 + 0.5*0.80 - 0.3*0.10 - 0.2*0.10
    //            = 0.40 + 0.40 - 0.03 - 0.02 = 0.75
    // target = clamp(0.75, 0, 1) = 0.75
    // delta  = (0.75 - 0.30) * 0.10 = 0.045
    // new    = 0.30 + 0.045 = 0.345
    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().target_stability   == doctest::Approx(0.75));
    CHECK(r.value().new_stability      == doctest::Approx(0.345));
    CHECK(state.countries[0].stability == doctest::Approx(0.345));
}

TEST_CASE("tick: pulls stability down toward a lower target") {
    GameState state;
    state.countries.push_back(country_with(0,
                                           /*stability=*/0.80,
                                           /*legitimacy=*/0.20,
                                           /*corruption=*/0.50));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*support=*/0.20,
                                          /*radicalism=*/0.50));

    // raw_target = 0.5*0.20 + 0.5*0.20 - 0.3*0.50 - 0.2*0.50
    //            = 0.10 + 0.10 - 0.15 - 0.10 = -0.05
    // target = clamp(-0.05, 0, 1) = 0.0
    // delta  = (0.0 - 0.80) * 0.10 = -0.08
    // new    = 0.80 - 0.08 = 0.72
    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().target_stability   == doctest::Approx(0.0));
    CHECK(r.value().new_stability      == doctest::Approx(0.72));
    CHECK(state.countries[0].stability == doctest::Approx(0.72));
}

// =====================================================================
// Coefficients
// =====================================================================

TEST_CASE("tick: rate / weight constants match the spec") {
    CHECK(st_sys::kSupportWeight       == doctest::Approx(0.5));
    CHECK(st_sys::kLegitimacyWeight    == doctest::Approx(0.5));
    CHECK(st_sys::kCorruptionWeight    == doctest::Approx(0.3));
    CHECK(st_sys::kRadicalismWeight    == doctest::Approx(0.2));
    CHECK(st_sys::kStabilityDriftRate  == doctest::Approx(0.10));
    CHECK(st_sys::kNoFactionsSupportDefault    == doctest::Approx(0.5));
    CHECK(st_sys::kNoFactionsRadicalismDefault == doctest::Approx(0.5));
}

// =====================================================================
// Clamping
// =====================================================================

TEST_CASE("tick: stability clamps at the upper bound") {
    GameState state;
    // Perfect inputs: support=1, legitimacy=1, corruption=0, radicalism=0
    // raw_target = 0.5*1 + 0.5*1 - 0 - 0 = 1.0
    // stability already at 0.99, delta = (1.0 - 0.99)*0.10 = 0.001
    // new = 0.991, still in [0, 1] - test the explicit clamp by
    // pushing stability beyond 1.0 in the input.
    state.countries.push_back(country_with(0,
                                           /*stability=*/1.2,  // pathological
                                           /*legitimacy=*/1.0,
                                           /*corruption=*/0.0));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*support=*/1.0,
                                          /*radicalism=*/0.0));

    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    // target = 1.0, delta = (1.0 - 1.2)*0.10 = -0.02, raw new = 1.18
    // Post-clamp = 1.0.
    CHECK(state.countries[0].stability == doctest::Approx(1.0));
}

TEST_CASE("tick: stability clamps at the lower bound") {
    GameState state;
    state.countries.push_back(country_with(0,
                                           /*stability=*/-0.5,  // pathological
                                           /*legitimacy=*/0.0,
                                           /*corruption=*/1.0));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*support=*/0.0,
                                          /*radicalism=*/1.0));

    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    // raw_target = 0 + 0 - 0.3 - 0.2 = -0.5 -> clamp 0.
    // delta = (0 - -0.5)*0.10 = 0.05, raw new = -0.45
    // Post-clamp = 0.0.
    CHECK(state.countries[0].stability == doctest::Approx(0.0));
}

// =====================================================================
// Multi-step convergence
// =====================================================================

TEST_CASE("tick: 50 steps converges stability close to target") {
    GameState state;
    state.countries.push_back(country_with(0,
                                           /*stability=*/0.20,
                                           /*legitimacy=*/0.60,
                                           /*corruption=*/0.20));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*support=*/0.60,
                                          /*radicalism=*/0.20));

    // raw_target = 0.5*0.60 + 0.5*0.60 - 0.3*0.20 - 0.2*0.20 = 0.50
    for (int i = 0; i < 50; ++i) {
        REQUIRE(st_sys::tick(state, CountryId{0}).ok());
    }
    // Geometric decay at rate 0.10 -> after 50 steps remaining error
    // ~= 0.9^50 * |0.50 - 0.20| ~= 0.005 * 0.30 = 0.0015.
    CHECK(state.countries[0].stability == doctest::Approx(0.50).epsilon(0.01));
}

// =====================================================================
// No factions / defaults
// =====================================================================

TEST_CASE("tick: country with no factions uses 0.5 / 0.5 defaults") {
    GameState state;
    state.countries.push_back(country_with(0,
                                           /*stability=*/0.50,
                                           /*legitimacy=*/0.50,
                                           /*corruption=*/0.20));

    // With no factions, avg_support = avg_radicalism = 0.5
    // raw_target = 0.5*0.5 + 0.5*0.5 - 0.3*0.2 - 0.2*0.5
    //            = 0.25 + 0.25 - 0.06 - 0.10 = 0.34
    // delta = (0.34 - 0.50) * 0.10 = -0.016
    // new = 0.50 - 0.016 = 0.484
    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().target_stability   == doctest::Approx(0.34));
    CHECK(state.countries[0].stability == doctest::Approx(0.484));
}

TEST_CASE("tick: country with only foreign-country factions uses defaults") {
    GameState state;
    state.countries.push_back(country_with(0, 0.5, 0.5, 0.2));
    state.countries.push_back(country_with(1, 0.5, 0.5, 0.2));
    // The faction belongs to country 1, not 0.
    state.factions.push_back(faction_with(0, CountryId{1}, 1.0, 0.0));

    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    // Same formula as the no-factions test above.
    CHECK(r.value().target_stability == doctest::Approx(0.34));
}

// =====================================================================
// Country filter
// =====================================================================

TEST_CASE("tick: only target country's stability changes") {
    GameState state;
    state.countries.push_back(country_with(0, /*stab=*/0.50, 0.60, 0.20));
    state.countries.push_back(country_with(1, /*stab=*/0.50, 0.60, 0.20));
    state.factions.push_back(faction_with(0, CountryId{0}, 0.60, 0.20));
    state.factions.push_back(faction_with(1, CountryId{1}, 0.60, 0.20));

    const auto before_country1_stability = state.countries[1].stability;
    REQUIRE(st_sys::tick(state, CountryId{0}).ok());
    CHECK(state.countries[1].stability == doctest::Approx(before_country1_stability));
}

TEST_CASE("tick: averages only factions in the target country") {
    // Two factions in country 0 with support {0.4, 0.6} -> avg 0.5
    // Plus a faction in country 1 with support 1.0 - must NOT be in
    // the average for country 0.
    GameState state;
    state.countries.push_back(country_with(0, /*stab=*/0.50,
                                           /*legitimacy=*/0.40,
                                           /*corruption=*/0.20));
    state.countries.push_back(country_with(1, 0.5, 0.5, 0.2));
    state.factions.push_back(faction_with(0, CountryId{0}, /*support=*/0.4, /*radicalism=*/0.4));
    state.factions.push_back(faction_with(1, CountryId{0}, /*support=*/0.6, /*radicalism=*/0.2));
    state.factions.push_back(faction_with(2, CountryId{1}, /*support=*/1.0, /*radicalism=*/0.0));

    // For country 0: avg_support = 0.5, avg_radicalism = 0.3
    // raw_target = 0.5*0.5 + 0.5*0.40 - 0.3*0.20 - 0.2*0.30
    //            = 0.25 + 0.20 - 0.06 - 0.06 = 0.33
    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().target_stability == doctest::Approx(0.33));
}

// =====================================================================
// Faction state untouched
// =====================================================================

TEST_CASE("tick: does NOT modify any faction state") {
    GameState state;
    state.countries.push_back(country_with(0, 0.5, 0.5, 0.3));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*support=*/0.42, /*radicalism=*/0.17));
    state.factions[0].influence = 0.65;
    state.factions[0].loyalty   = 0.71;
    state.factions[0].resources = 2.7;

    REQUIRE(st_sys::tick(state, CountryId{0}).ok());
    CHECK(state.factions[0].support    == doctest::Approx(0.42));
    CHECK(state.factions[0].radicalism == doctest::Approx(0.17));
    CHECK(state.factions[0].influence  == doctest::Approx(0.65));
    CHECK(state.factions[0].loyalty    == doctest::Approx(0.71));
    CHECK(state.factions[0].resources  == doctest::Approx(2.7));
}

// =====================================================================
// Error paths
// =====================================================================

TEST_CASE("tick: invalid CountryId rejected, state unchanged") {
    GameState state;
    state.countries.push_back(country_with(0, 0.5, 0.5, 0.2));
    const auto before = state.countries[0].stability;

    const auto r = st_sys::tick(state, CountryId{99});
    REQUIRE(r.failed());
    CHECK(r.error().find("CountryId 99") != std::string::npos);
    CHECK(state.countries[0].stability == doctest::Approx(before));
}

TEST_CASE("tick: default-constructed (invalid) CountryId rejected") {
    GameState state;
    state.countries.push_back(country_with(0, 0.5, 0.5, 0.2));
    const auto r = st_sys::tick(state, CountryId{});
    REQUIRE(r.failed());
}

TEST_CASE("tick: empty state (no countries) -> invalid id rejected") {
    GameState state;
    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.failed());
}

// =====================================================================
// Outcome struct
// =====================================================================

TEST_CASE("tick: outcome struct carries previous / new / target") {
    GameState state;
    state.countries.push_back(country_with(0, /*stab=*/0.40, 0.60, 0.20));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*support=*/0.60, /*radicalism=*/0.20));

    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().previous_stability == doctest::Approx(0.40));
    CHECK(r.value().target_stability   == doctest::Approx(0.50));
    CHECK(r.value().new_stability      == doctest::Approx(0.41));
    // new_stability == state.countries[0].stability after the call
    CHECK(r.value().new_stability == doctest::Approx(state.countries[0].stability));
}

// =====================================================================
// M1.12: EconomicGrowth term
// =====================================================================

TEST_CASE("tick: kEconomicGrowthWeight constant matches the spec") {
    CHECK(st_sys::kEconomicGrowthWeight == doctest::Approx(2.0));
}

TEST_CASE("tick: positive last_gdp_growth_rate raises target stability") {
    // Baseline (growth = 0): target = 0.5*0.5 + 0.5*0.5 - 0 - 0 = 0.50.
    // With growth = 0.0035: target = 0.50 + 2.0 * 0.0035 = 0.50 + 0.007 = 0.507.
    GameState state;
    auto c = country_with(0, /*stab=*/0.50, /*legitimacy=*/0.50,
                          /*corruption=*/0.0);
    c.last_gdp_growth_rate = 0.0035;  // canonical GER 1930 monthly growth
    state.countries.push_back(c);
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*support=*/0.50, /*radicalism=*/0.0));

    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().target_stability == doctest::Approx(0.507));
}

TEST_CASE("tick: negative last_gdp_growth_rate lowers target stability") {
    // raw_target = 0.50 + 2.0 * -0.010 = 0.50 - 0.020 = 0.480.
    GameState state;
    auto c = country_with(0, 0.50, 0.50, 0.0);
    c.last_gdp_growth_rate = -0.010;  // recession case
    state.countries.push_back(c);
    state.factions.push_back(faction_with(0, CountryId{0}, 0.50, 0.0));

    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().target_stability == doctest::Approx(0.480));
}

TEST_CASE("tick: zero last_gdp_growth_rate is identical to pre-M1.12 behaviour") {
    // Regression guard: with the field at its default 0.0, every
    // existing test that pre-dates M1.12 must continue to compute
    // the same target. We re-derive the M1.7 formula here.
    GameState state;
    auto c = country_with(0, 0.50, 0.50, 0.0);
    // last_gdp_growth_rate defaults to 0.0 - left untouched.
    state.countries.push_back(c);
    state.factions.push_back(faction_with(0, CountryId{0}, 0.50, 0.0));

    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    // Pre-M1.12: target = 0.5*0.5 + 0.5*0.5 - 0 - 0 = 0.50.
    CHECK(r.value().target_stability == doctest::Approx(0.50));
}

TEST_CASE("tick: pathological last_gdp_growth_rate is still clamped to [0,1]") {
    // A wildly out-of-range growth value (e.g. caused by future
    // economy bugs) should not break stability::tick's contract.
    // target = clamp(0.5 + 2.0 * 1.0, 0, 1) = clamp(2.5, 0, 1) = 1.0.
    GameState state;
    auto c = country_with(0, 0.50, 0.50, 0.0);
    c.last_gdp_growth_rate = 1.0;
    state.countries.push_back(c);
    state.factions.push_back(faction_with(0, CountryId{0}, 0.50, 0.0));

    const auto r = st_sys::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().target_stability == doctest::Approx(1.0));

    // And on the negative end: target = clamp(0.5 + 2.0 * -1.0, 0, 1) = 0.0.
    GameState state_neg;
    auto c2 = country_with(0, 0.50, 0.50, 0.0);
    c2.last_gdp_growth_rate = -1.0;
    state_neg.countries.push_back(c2);
    state_neg.factions.push_back(faction_with(0, CountryId{0}, 0.50, 0.0));

    const auto r2 = st_sys::tick(state_neg, CountryId{0});
    REQUIRE(r2.ok());
    CHECK(r2.value().target_stability == doctest::Approx(0.0));
}

TEST_CASE("tick: stability::tick does NOT modify last_gdp_growth_rate") {
    GameState state;
    auto c = country_with(0, 0.50, 0.50, 0.0);
    c.last_gdp_growth_rate = 0.0035;
    state.countries.push_back(c);
    state.factions.push_back(faction_with(0, CountryId{0}, 0.50, 0.0));

    REQUIRE(st_sys::tick(state, CountryId{0}).ok());
    // The field is only WRITTEN by economy::tick. Stability reads it.
    CHECK(state.countries[0].last_gdp_growth_rate == doctest::Approx(0.0035));
}

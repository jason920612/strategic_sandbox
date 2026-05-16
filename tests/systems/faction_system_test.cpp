#include <doctest/doctest.h>

#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/faction_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameState;
namespace fs_sys = leviathan::systems::faction;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------

namespace {

CountryState country_with(double stability, double legitimacy,
                          int id = 0) {
    CountryState c;
    c.id           = CountryId{id};
    c.id_code      = "GER";
    c.name         = "Germany";
    c.stability    = stability;
    c.legitimacy   = legitimacy;
    // Other fields default to 0; M1.6 reactions don't read them.
    return c;
}

FactionState faction_with(int id, CountryId country,
                          double loyalty, double support,
                          std::string type = "military") {
    FactionState f;
    f.id              = FactionId{id};
    f.country         = country;
    f.id_code         = type + "_test";
    f.country_id_code = "GER";
    f.name            = type;
    f.type            = std::move(type);
    f.support         = support;
    f.influence       = 0.50;
    f.radicalism      = 0.30;
    f.loyalty         = loyalty;
    f.resources       = 1.0;
    return f;
}

}  // namespace

// =====================================================================
// Happy path - one step
// =====================================================================

TEST_CASE("react: loyalty drifts toward country.stability by 10%") {
    GameState state;
    state.countries.push_back(country_with(/*stability=*/1.00, /*legitimacy=*/0.5));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*loyalty=*/0.50,
                                          /*support=*/0.50));

    REQUIRE(fs_sys::react(state, CountryId{0}).ok());
    // delta = (1.00 - 0.50) * 0.10 = 0.05
    CHECK(state.factions[0].loyalty == doctest::Approx(0.55));
}

TEST_CASE("react: support drifts toward country.legitimacy by 5%") {
    GameState state;
    state.countries.push_back(country_with(/*stability=*/0.50, /*legitimacy=*/1.00));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*loyalty=*/0.50,
                                          /*support=*/0.40));

    REQUIRE(fs_sys::react(state, CountryId{0}).ok());
    // delta = (1.00 - 0.40) * 0.05 = 0.03
    CHECK(state.factions[0].support == doctest::Approx(0.43));
}

TEST_CASE("react: rate constants match the spec") {
    CHECK(fs_sys::kLoyaltyDriftRate == doctest::Approx(0.10));
    CHECK(fs_sys::kSupportDriftRate == doctest::Approx(0.05));
}

// =====================================================================
// Equilibrium and clamping
// =====================================================================

TEST_CASE("react: equilibrium (faction == country target) is a no-op") {
    GameState state;
    state.countries.push_back(country_with(0.50, 0.50));
    state.factions.push_back(faction_with(0, CountryId{0}, 0.50, 0.50));

    REQUIRE(fs_sys::react(state, CountryId{0}).ok());
    CHECK(state.factions[0].loyalty == doctest::Approx(0.50));
    CHECK(state.factions[0].support == doctest::Approx(0.50));
}

TEST_CASE("react: loyalty above target moves DOWN toward it") {
    GameState state;
    state.countries.push_back(country_with(/*stability=*/0.20, 0.5));
    state.factions.push_back(faction_with(0, CountryId{0},
                                          /*loyalty=*/0.80, 0.50));

    REQUIRE(fs_sys::react(state, CountryId{0}).ok());
    // delta = (0.20 - 0.80) * 0.10 = -0.06
    CHECK(state.factions[0].loyalty == doctest::Approx(0.74));
}

TEST_CASE("react: clamps loyalty at the upper bound") {
    GameState state;
    state.countries.push_back(country_with(/*stability=*/10.0, 0.5));   // pathological
    state.factions.push_back(faction_with(0, CountryId{0}, 0.99, 0.50));

    REQUIRE(fs_sys::react(state, CountryId{0}).ok());
    // delta = (10.0 - 0.99) * 0.10 = 0.901 -> 0.99 + 0.901 = 1.891 -> clamp to 1.0
    CHECK(state.factions[0].loyalty == doctest::Approx(1.0));
}

TEST_CASE("react: clamps support at the lower bound") {
    GameState state;
    state.countries.push_back(country_with(0.5, /*legitimacy=*/-2.0));  // pathological
    state.factions.push_back(faction_with(0, CountryId{0}, 0.50, 0.01));

    REQUIRE(fs_sys::react(state, CountryId{0}).ok());
    // delta = (-2.0 - 0.01) * 0.05 = -0.1005 -> 0.01 - 0.1005 = -0.0905 -> clamp to 0.0
    CHECK(state.factions[0].support == doctest::Approx(0.0));
}

// =====================================================================
// Multi-step convergence
// =====================================================================

TEST_CASE("react: 50 steps converges loyalty to within epsilon of stability") {
    GameState state;
    state.countries.push_back(country_with(/*stability=*/0.80, 0.5));
    state.factions.push_back(faction_with(0, CountryId{0}, 0.20, 0.5));

    for (int i = 0; i < 50; ++i) {
        REQUIRE(fs_sys::react(state, CountryId{0}).ok());
    }
    // Geometric convergence at rate 0.10: after 50 steps the
    // remaining error is (1 - 0.10)^50 * |0.80 - 0.20| ~= 0.6 * 0.005 ~= 0.003.
    CHECK(state.factions[0].loyalty == doctest::Approx(0.80).epsilon(0.01));
}

// =====================================================================
// Country filter
// =====================================================================

TEST_CASE("react: only factions in the requested country are touched") {
    GameState state;
    state.countries.push_back(country_with(/*stability=*/1.0,  0.5, /*id=*/0));
    state.countries.push_back(country_with(/*stability=*/0.5,  0.5, /*id=*/1));

    state.factions.push_back(faction_with(0, CountryId{0}, 0.50, 0.50));
    state.factions.push_back(faction_with(1, CountryId{1}, 0.50, 0.50));
    state.factions.push_back(faction_with(2, CountryId{0}, 0.50, 0.50));

    const auto r = fs_sys::react(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().factions_updated == 2);
    CHECK(state.factions[0].loyalty == doctest::Approx(0.55));  // GER
    CHECK(state.factions[1].loyalty == doctest::Approx(0.50));  // FRA - untouched
    CHECK(state.factions[2].loyalty == doctest::Approx(0.55));  // GER
}

TEST_CASE("react: country with no factions returns success with 0 updated") {
    GameState state;
    state.countries.push_back(country_with(0.5, 0.5));
    // Add a faction belonging to a different country.
    state.countries.push_back(country_with(0.5, 0.5, /*id=*/1));
    state.factions.push_back(faction_with(0, CountryId{1}, 0.5, 0.5));

    const auto r = fs_sys::react(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().factions_updated == 0);
}

// =====================================================================
// Untouched fields
// =====================================================================

TEST_CASE("react: influence / radicalism / resources unchanged") {
    GameState state;
    state.countries.push_back(country_with(1.0, 1.0));
    state.factions.push_back(faction_with(0, CountryId{0}, 0.30, 0.30));
    state.factions[0].influence  = 0.42;
    state.factions[0].radicalism = 0.17;
    state.factions[0].resources  = 2.7;

    REQUIRE(fs_sys::react(state, CountryId{0}).ok());
    CHECK(state.factions[0].influence  == doctest::Approx(0.42));
    CHECK(state.factions[0].radicalism == doctest::Approx(0.17));
    CHECK(state.factions[0].resources  == doctest::Approx(2.7));
}

TEST_CASE("react: identity fields (id, country, type, ...) unchanged") {
    GameState state;
    state.countries.push_back(country_with(0.7, 0.7));
    state.factions.push_back(faction_with(0, CountryId{0}, 0.3, 0.3, "workers"));
    state.factions[0].preferred_policies = {"expand_welfare"};

    REQUIRE(fs_sys::react(state, CountryId{0}).ok());
    CHECK(state.factions[0].id.value() == 0);
    CHECK(state.factions[0].country == CountryId{0});
    CHECK(state.factions[0].type == "workers");
    CHECK(state.factions[0].country_id_code == "GER");
    REQUIRE(state.factions[0].preferred_policies.size() == 1);
    CHECK(state.factions[0].preferred_policies[0] == "expand_welfare");
}

// =====================================================================
// Error paths
// =====================================================================

TEST_CASE("react: invalid CountryId is rejected, state unchanged") {
    GameState state;
    state.countries.push_back(country_with(0.5, 0.5));
    state.factions.push_back(faction_with(0, CountryId{0}, 0.3, 0.3));

    const auto before_loyalty = state.factions[0].loyalty;
    const auto r = fs_sys::react(state, CountryId{99});
    REQUIRE(r.failed());
    CHECK(r.error().find("CountryId 99") != std::string::npos);
    CHECK(state.factions[0].loyalty == doctest::Approx(before_loyalty));
}

TEST_CASE("react: default-constructed (invalid) CountryId is rejected") {
    GameState state;
    state.countries.push_back(country_with(0.5, 0.5));
    state.factions.push_back(faction_with(0, CountryId{0}, 0.3, 0.3));

    const auto before = state.factions[0].loyalty;
    const auto r = fs_sys::react(state, CountryId{});
    REQUIRE(r.failed());
    CHECK(state.factions[0].loyalty == doctest::Approx(before));
}

TEST_CASE("react: no countries at all -> invalid id rejected") {
    GameState state;
    const auto r = fs_sys::react(state, CountryId{0});
    REQUIRE(r.failed());
}

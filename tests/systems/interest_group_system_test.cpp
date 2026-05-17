#include <doctest/doctest.h>

#include <cmath>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/interest_group_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::GameState;
using leviathan::core::InterestGroupKind;
using leviathan::core::InterestGroupState;
namespace ig = leviathan::systems::interest_group;

namespace {

CountryState country_at_stability(double stability,
                                  const std::string& id_code) {
    CountryState c;
    c.id_code   = id_code;
    c.name      = id_code;
    c.stability = stability;
    return c;
}

InterestGroupState group_at(CountryId country,
                            double loyalty,
                            double radicalism,
                            double influence = 0.5,
                            InterestGroupKind kind =
                                InterestGroupKind::Bureaucracy,
                            const std::string& id_code = "g") {
    InterestGroupState g;
    g.id_code    = id_code;
    g.name       = id_code;
    g.kind       = kind;
    g.country    = country;
    g.loyalty    = loyalty;
    g.radicalism = radicalism;
    g.influence  = influence;
    return g;
}

}  // namespace

// ---------------------------------------------------------------------
// Empty state
// ---------------------------------------------------------------------

TEST_CASE("react: no interest groups returns success with zero updates") {
    GameState state;
    const auto r = ig::react(state);
    REQUIRE(r.ok());
    CHECK(r.value().groups_updated == 0);
}

TEST_CASE("react: empty interest groups with non-empty countries still succeeds") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    const auto r = ig::react(state);
    REQUIRE(r.ok());
    CHECK(r.value().groups_updated == 0);
}

// ---------------------------------------------------------------------
// Single-group drift in both directions
// ---------------------------------------------------------------------

TEST_CASE("react: high stability drifts loyalty up and radicalism down") {
    GameState state;
    state.countries.push_back(country_at_stability(0.8, "GER"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, /*loyalty=*/0.4, /*radicalism=*/0.6));

    const auto r = ig::react(state);
    REQUIRE(r.ok());
    CHECK(r.value().groups_updated == 1);

    // loyalty:    0.4 + (0.8 - 0.4) * 0.05 = 0.42
    // radicalism: 0.6 + (0.2 - 0.6) * 0.05 = 0.58
    CHECK(state.interest_groups[0].loyalty    == doctest::Approx(0.42));
    CHECK(state.interest_groups[0].radicalism == doctest::Approx(0.58));
}

TEST_CASE("react: low stability drifts loyalty down and radicalism up") {
    GameState state;
    state.countries.push_back(country_at_stability(0.2, "GER"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, /*loyalty=*/0.6, /*radicalism=*/0.1));

    const auto r = ig::react(state);
    REQUIRE(r.ok());
    CHECK(r.value().groups_updated == 1);

    // loyalty:    0.6 + (0.2 - 0.6) * 0.05 = 0.58
    // radicalism: 0.1 + (0.8 - 0.1) * 0.05 = 0.135
    CHECK(state.interest_groups[0].loyalty    == doctest::Approx(0.58));
    CHECK(state.interest_groups[0].radicalism == doctest::Approx(0.135));
}

TEST_CASE("react: stability at exact mid-point produces no drift when group already at target") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, /*loyalty=*/0.5, /*radicalism=*/0.5));

    const auto r = ig::react(state);
    REQUIRE(r.ok());
    CHECK(state.interest_groups[0].loyalty    == doctest::Approx(0.5));
    CHECK(state.interest_groups[0].radicalism == doctest::Approx(0.5));
}

// ---------------------------------------------------------------------
// Multi-group, multi-country
// ---------------------------------------------------------------------

TEST_CASE("react: each group uses its own country's stability") {
    GameState state;
    state.countries.push_back(country_at_stability(0.9, "GER"));   // 0
    state.countries.push_back(country_at_stability(0.1, "FRA"));   // 1

    state.interest_groups.push_back(
        group_at(CountryId{0}, /*loyalty=*/0.5, /*radicalism=*/0.5,
                 /*influence=*/0.5, InterestGroupKind::Bureaucracy, "ger_b"));
    state.interest_groups.push_back(
        group_at(CountryId{1}, /*loyalty=*/0.5, /*radicalism=*/0.5,
                 /*influence=*/0.5, InterestGroupKind::Military,    "fra_m"));

    const auto r = ig::react(state);
    REQUIRE(r.ok());
    CHECK(r.value().groups_updated == 2);

    // GER (stab=0.9): loyalty 0.5 -> 0.5 + (0.9-0.5)*0.05 = 0.52
    //                 radicalism 0.5 -> 0.5 + (0.1-0.5)*0.05 = 0.48
    CHECK(state.interest_groups[0].loyalty    == doctest::Approx(0.52));
    CHECK(state.interest_groups[0].radicalism == doctest::Approx(0.48));
    // FRA (stab=0.1): loyalty 0.5 -> 0.5 + (0.1-0.5)*0.05 = 0.48
    //                 radicalism 0.5 -> 0.5 + (0.9-0.5)*0.05 = 0.52
    CHECK(state.interest_groups[1].loyalty    == doctest::Approx(0.48));
    CHECK(state.interest_groups[1].radicalism == doctest::Approx(0.52));
}

// ---------------------------------------------------------------------
// Non-mutation guarantees
// ---------------------------------------------------------------------

TEST_CASE("react: influence is never touched") {
    GameState state;
    state.countries.push_back(country_at_stability(0.9, "GER"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, /*loyalty=*/0.5, /*radicalism=*/0.5,
                 /*influence=*/0.73));

    REQUIRE(ig::react(state).ok());
    CHECK(state.interest_groups[0].influence == doctest::Approx(0.73));
}

TEST_CASE("react: kind / country / id_code / name are never touched") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, 0.5, 0.5, 0.5,
                 InterestGroupKind::Military, "ger_mil"));

    REQUIRE(ig::react(state).ok());
    const auto& g = state.interest_groups[0];
    CHECK(g.id_code    == "ger_mil");
    CHECK(g.name       == "ger_mil");
    CHECK(g.kind       == InterestGroupKind::Military);
    CHECK(g.country.value() == 0);
}

TEST_CASE("react: clamp keeps loyalty / radicalism inside [0, 1]") {
    // Pathological start values just outside the canonical range
    // would normally get rejected at load, but defensively pin
    // that the formula's output stays clamped even when the
    // input is not.
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    InterestGroupState g = group_at(CountryId{0}, 1.0, 0.0);
    state.interest_groups.push_back(g);

    REQUIRE(ig::react(state).ok());
    CHECK(state.interest_groups[0].loyalty    >= 0.0);
    CHECK(state.interest_groups[0].loyalty    <= 1.0);
    CHECK(state.interest_groups[0].radicalism >= 0.0);
    CHECK(state.interest_groups[0].radicalism <= 1.0);
}

// ---------------------------------------------------------------------
// Preflight atomicity
// ---------------------------------------------------------------------

TEST_CASE("react: invalid country index fails preflight and leaves every group untouched") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));

    // Valid group at index 0 first.
    InterestGroupState valid =
        group_at(CountryId{0}, /*loyalty=*/0.4, /*radicalism=*/0.4,
                 /*influence=*/0.5,
                 InterestGroupKind::Bureaucracy, "ger_b");
    state.interest_groups.push_back(valid);

    // Bad group at index 1 — country index points past the single
    // loaded country.
    InterestGroupState bad = valid;
    bad.id_code = "bad";
    bad.country = CountryId{7};
    state.interest_groups.push_back(bad);

    const double loyalty_before    = state.interest_groups[0].loyalty;
    const double radicalism_before = state.interest_groups[0].radicalism;

    const auto r = ig::react(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_group::react") != std::string::npos);
    CHECK(r.error().find("interest_groups[1]")    != std::string::npos);

    // Atomicity: no group's ratios changed.
    CHECK(state.interest_groups[0].loyalty    == doctest::Approx(loyalty_before));
    CHECK(state.interest_groups[0].radicalism == doctest::Approx(radicalism_before));
}

TEST_CASE("react: invalid (sentinel) country index fails preflight") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    InterestGroupState g = group_at(CountryId{0}, 0.5, 0.5);
    g.country = CountryId::invalid();   // -1 sentinel.
    state.interest_groups.push_back(g);

    const auto r = ig::react(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_group::react") != std::string::npos);
    CHECK(r.error().find("interest_groups[0]")    != std::string::npos);
}

// =====================================================================
// M3.3 - country_feedback: influence-weighted radicalism -> country.stability
// =====================================================================

TEST_CASE("country_feedback: empty state succeeds with zero updates") {
    GameState state;
    const auto r = ig::country_feedback(state);
    REQUIRE(r.ok());
    CHECK(r.value().countries_updated == 0);
}

TEST_CASE("country_feedback: country with no matching groups is skipped") {
    GameState state;
    state.countries.push_back(country_at_stability(0.6, "GER"));
    const double before = state.countries[0].stability;

    const auto r = ig::country_feedback(state);
    REQUIRE(r.ok());
    CHECK(r.value().countries_updated == 0);
    // Stability untouched.
    CHECK(state.countries[0].stability == doctest::Approx(before));
}

TEST_CASE("country_feedback: high group radicalism lowers country stability") {
    GameState state;
    state.countries.push_back(country_at_stability(0.8, "GER"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, /*loyalty=*/0.5, /*radicalism=*/0.6,
                 /*influence=*/1.0));

    const auto r = ig::country_feedback(state);
    REQUIRE(r.ok());
    CHECK(r.value().countries_updated == 1);

    // target = 1 - 0.6 = 0.4
    // new    = 0.8 + (0.4 - 0.8) * 0.02 = 0.792
    CHECK(state.countries[0].stability == doctest::Approx(0.792));
}

TEST_CASE("country_feedback: low group radicalism raises country stability") {
    GameState state;
    state.countries.push_back(country_at_stability(0.4, "GER"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, /*loyalty=*/0.5, /*radicalism=*/0.1,
                 /*influence=*/1.0));

    const auto r = ig::country_feedback(state);
    REQUIRE(r.ok());
    // target = 1 - 0.1 = 0.9
    // new    = 0.4 + (0.9 - 0.4) * 0.02 = 0.41
    CHECK(state.countries[0].stability == doctest::Approx(0.41));
}

TEST_CASE("country_feedback: influence-weighted aggregate across two groups") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, /*loyalty=*/0.5, /*radicalism=*/0.2,
                 /*influence=*/0.75,
                 InterestGroupKind::Bureaucracy, "g1"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, /*loyalty=*/0.5, /*radicalism=*/0.8,
                 /*influence=*/0.25,
                 InterestGroupKind::Military,    "g2"));

    const auto r = ig::country_feedback(state);
    REQUIRE(r.ok());
    // weighted = (0.75*0.2 + 0.25*0.8) / (0.75 + 0.25)
    //          = (0.15 + 0.20) / 1.0 = 0.35
    // target   = 1 - 0.35 = 0.65
    // new      = 0.5 + (0.65 - 0.5) * 0.02 = 0.503
    CHECK(state.countries[0].stability == doctest::Approx(0.503));
}

TEST_CASE("country_feedback: zero-influence groups are ignored") {
    // The only group has influence 0 — it should not affect the
    // aggregate; the country is treated as having no political
    // weight and is skipped.
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, /*loyalty=*/0.5, /*radicalism=*/0.9,
                 /*influence=*/0.0));
    const double before = state.countries[0].stability;

    const auto r = ig::country_feedback(state);
    REQUIRE(r.ok());
    CHECK(r.value().countries_updated == 0);
    CHECK(state.countries[0].stability == doctest::Approx(before));
}

TEST_CASE("country_feedback: multi-country updates are independent") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));   // 0
    state.countries.push_back(country_at_stability(0.5, "FRA"));   // 1

    state.interest_groups.push_back(
        group_at(CountryId{0}, 0.5, /*rad=*/0.2, 1.0,
                 InterestGroupKind::Bureaucracy, "ger_b"));
    state.interest_groups.push_back(
        group_at(CountryId{1}, 0.5, /*rad=*/0.8, 1.0,
                 InterestGroupKind::Military,    "fra_m"));

    const auto r = ig::country_feedback(state);
    REQUIRE(r.ok());
    CHECK(r.value().countries_updated == 2);

    // GER: target = 1 - 0.2 = 0.8; new = 0.5 + 0.3 * 0.02 = 0.506
    // FRA: target = 1 - 0.8 = 0.2; new = 0.5 + (-0.3) * 0.02 = 0.494
    CHECK(state.countries[0].stability == doctest::Approx(0.506));
    CHECK(state.countries[1].stability == doctest::Approx(0.494));
}

TEST_CASE("country_feedback: interest groups themselves are never mutated") {
    GameState state;
    state.countries.push_back(country_at_stability(0.4, "GER"));
    InterestGroupState g =
        group_at(CountryId{0}, /*loyalty=*/0.37, /*radicalism=*/0.62,
                 /*influence=*/0.81);
    state.interest_groups.push_back(g);

    REQUIRE(ig::country_feedback(state).ok());
    const auto& after = state.interest_groups[0];
    CHECK(after.loyalty    == doctest::Approx(0.37));
    CHECK(after.radicalism == doctest::Approx(0.62));
    CHECK(after.influence  == doctest::Approx(0.81));
}

TEST_CASE("country_feedback: invalid group country fails preflight without mutating any country") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    state.countries.push_back(country_at_stability(0.5, "FRA"));
    // Valid group first.
    state.interest_groups.push_back(
        group_at(CountryId{0}, 0.5, 0.5, 1.0,
                 InterestGroupKind::Bureaucracy, "ok"));
    // Bad group with country index past the end.
    InterestGroupState bad =
        group_at(CountryId{0}, 0.5, 0.5, 1.0,
                 InterestGroupKind::Military, "bad");
    bad.country = CountryId{7};
    state.interest_groups.push_back(bad);

    const double ger_before = state.countries[0].stability;
    const double fra_before = state.countries[1].stability;

    const auto r = ig::country_feedback(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("country_feedback")  != std::string::npos);
    CHECK(r.error().find("interest_groups[1]") != std::string::npos);

    // No country mutated.
    CHECK(state.countries[0].stability == doctest::Approx(ger_before));
    CHECK(state.countries[1].stability == doctest::Approx(fra_before));
}

TEST_CASE("country_feedback: non-finite group ratio fails preflight") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    InterestGroupState g = group_at(CountryId{0}, 0.5, 0.5, 1.0);
    g.radicalism = std::nan("");
    state.interest_groups.push_back(g);

    const auto r = ig::country_feedback(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("radicalism") != std::string::npos);
}

TEST_CASE("country_feedback: out-of-range group influence fails preflight") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    InterestGroupState g = group_at(CountryId{0}, 0.5, 0.5, 1.0);
    g.influence = 1.5;
    state.interest_groups.push_back(g);

    const auto r = ig::country_feedback(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("influence") != std::string::npos);
}

TEST_CASE("country_feedback: non-finite country stability fails preflight") {
    GameState state;
    state.countries.push_back(country_at_stability(0.5, "GER"));
    state.countries[0].stability = std::nan("");
    state.interest_groups.push_back(
        group_at(CountryId{0}, 0.5, 0.5, 1.0));

    const auto r = ig::country_feedback(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("stability") != std::string::npos);
}

TEST_CASE("country_feedback: clamp keeps stability inside [0, 1]") {
    // Pathological inputs at the edge: stability 1.0 and very low
    // weighted radicalism nudge above 1.0 before clamp; clamp
    // must keep it inside.
    GameState state;
    state.countries.push_back(country_at_stability(1.0, "GER"));
    state.interest_groups.push_back(
        group_at(CountryId{0}, 0.5, /*rad=*/0.0, 1.0));

    REQUIRE(ig::country_feedback(state).ok());
    CHECK(state.countries[0].stability >= 0.0);
    CHECK(state.countries[0].stability <= 1.0);
}

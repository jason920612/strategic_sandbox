// Trajectory observation tests for the asymptotic ratio-`add` formula
// introduced by the post-M6.7 hardening sweep.
//
// Why this file exists:
//   The hardening sweep replaced linear `old + delta` on ratio fields
//   with an asymptotic update:
//     positive delta: new = old + delta * (1 - old)
//     negative delta: new = old + delta * old
//   Per `feedback_trajectory_observation_tests`, the realism check is
//   "observe the trajectory over time and confirm its SHAPE matches
//   the underlying research-theory expectation", not "pin a specific
//   numeric value at one step". The per-step value tests are the
//   mechanical-recompute checks already in policy_system_test; this
//   file pins the SHAPE properties.
//
// Honest research citation discipline (see
// docs/hardening-strict-numeric-validation.md §4):
//   The literature supports (a) bounded institutional indicator scales
//   and (b) slow-moving capacity stocks with diminishing returns near
//   the bounds — i.e. an asymptotic-shape update is a literature-
//   aligned MODELLING CHOICE. The literature does NOT prove the exact
//   functional form `delta * (1 - old)`; that is a project game-model
//   assumption. The tests below assert the qualitative shape (bounded,
//   monotonic, marginal-change shrinks near the bound), not specific
//   coefficients.
//
// References for the bounded / diminishing-returns shape:
//   - Marshall & Jaggers 2002, Polity IV Project: Dataset Users'
//     Manual — bounded composite indicator design.
//   - Coppedge et al. 2011, V-Dem: A New Way to Measure Democracy —
//     disaggregated democracy indices, all bounded.
//   - Besley & Persson 2009 / 2010 — state capacity as a slow-moving
//     stock with diminishing returns near saturation.

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/policy_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::GameState;
using leviathan::core::PolicyData;
namespace ps = leviathan::systems::policy;

namespace {

CountryState baseline(const std::string& code) {
    CountryState c;
    c.id      = CountryId{0};
    c.id_code = code;
    c.name    = code;
    c.gdp                       = 100.0;
    c.tax_revenue               = 18.0;
    c.budget_balance            = 0.0;
    c.legal_tax_burden          = 0.20;
    c.fiscal_capacity           = 0.50;
    c.administrative_efficiency = 0.50;
    c.central_control           = 0.50;
    c.corruption                = 0.50;
    c.stability                 = 0.50;
    c.legitimacy                = 0.50;
    c.military_power            = 0.50;
    c.threat_perception         = 0.30;
    c.budget.administration     = 0.20;
    c.budget.military           = 0.20;
    c.budget.education          = 0.15;
    c.budget.welfare            = 0.15;
    c.budget.intelligence       = 0.10;
    c.budget.infrastructure     = 0.10;
    c.budget.industry           = 0.10;
    return c;
}

// Helper: apply one ratio-target `add` effect and return the new
// value of that field. Uses the production `apply_policy_effects`
// entry point so the test exercises the same code path that AI
// and event flows use.
double step_add(GameState& state, const std::string& target, double delta) {
    PolicyData p;
    p.effects.push_back({target, "add", delta});
    const auto r = ps::apply_policy_effects(state, CountryId{0}, p);
    REQUIRE(r.ok());
    if (target == "country.stability") return state.countries[0].stability;
    if (target == "country.corruption") return state.countries[0].corruption;
    if (target == "country.legitimacy") return state.countries[0].legitimacy;
    return std::numeric_limits<double>::quiet_NaN();
}

}  // namespace

// =====================================================================
// 1. Bounded-from-above approach
//
// Research-grounded shape claim:
//   Bounded institutional indicators (Polity IV / V-Dem) cannot exceed
//   their scale maximum, and repeated reinforcement shocks produce
//   diminishing marginal change near the upper bound (consistent with
//   Besley & Persson 2009/2010 state-capacity-stock dynamics).
//
// The test pins these qualitative properties, NOT specific values.
// =====================================================================
TEST_CASE("Trajectory: repeated positive `add` approaches but never crosses 1.0") {
    GameState state;
    state.countries.push_back(baseline("UP"));
    state.countries[0].stability = 0.20;

    const double delta = 0.10;

    double prev_value  = state.countries[0].stability;
    double prev_margin = std::numeric_limits<double>::infinity();
    for (int step = 0; step < 50; ++step) {
        const double next = step_add(state, "country.stability", delta);
        const double margin = next - prev_value;

        // (a) Monotonic increase toward the upper bound.
        CHECK(next > prev_value);
        // (b) Never crosses 1.0.
        CHECK(next <= 1.0);
        // (c) Diminishing returns: each step's marginal change is
        //     strictly smaller than the previous step's. This is
        //     the qualitative "diminishing-returns near the bound"
        //     property that matches the Besley & Persson capacity-
        //     stock literature.
        CHECK(margin < prev_margin);

        prev_value  = next;
        prev_margin = margin;
    }

    // After 50 steps with delta = 0.10 the value should be very close
    // to the upper bound. The exact closeness depends on the formula
    // (and is checked mechanically by policy_system_test); here we
    // just pin the qualitative "converges near the bound" property.
    CHECK(state.countries[0].stability > 0.99);
    CHECK(state.countries[0].stability <= 1.0);
}

// =====================================================================
// 2. Bounded-from-below approach (symmetric)
//
// Mirror property of test #1 — `new = old + delta * old` for delta < 0
// converges toward 0.0 from above with diminishing marginal magnitude.
// =====================================================================
TEST_CASE("Trajectory: repeated negative `add` approaches but never crosses 0.0") {
    GameState state;
    state.countries.push_back(baseline("DN"));
    state.countries[0].corruption = 0.80;

    const double delta = -0.10;

    double prev_value      = state.countries[0].corruption;
    double prev_abs_margin = std::numeric_limits<double>::infinity();
    for (int step = 0; step < 50; ++step) {
        const double next = step_add(state, "country.corruption", delta);
        const double abs_margin = prev_value - next;

        CHECK(next < prev_value);
        CHECK(next >= 0.0);
        CHECK(abs_margin < prev_abs_margin);

        prev_value      = next;
        prev_abs_margin = abs_margin;
    }

    CHECK(state.countries[0].corruption < 0.01);
    CHECK(state.countries[0].corruption >= 0.0);
}

// =====================================================================
// 3. No-overshoot fuzz (mechanical invariant)
//
// Mechanical invariant test — for any (old, delta) pair with old in
// [0, 1] and delta in [-1, 1], the asymptotic-add candidate stays
// inside [0, 1]. This is an algebraic property of the formula, not
// a behavioural claim; synthetic random inputs are appropriate here.
// =====================================================================
TEST_CASE("No-overshoot fuzz: 1000 random (old, delta) inputs stay in [0, 1]") {
    GameState state;
    state.countries.push_back(baseline("FZ"));

    // Deterministic seed so the test is reproducible across runs.
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_real_distribution<double> old_dist(0.0, 1.0);
    std::uniform_real_distribution<double> delta_dist(-1.0, 1.0);

    for (int i = 0; i < 1000; ++i) {
        const double old_v  = old_dist(rng);
        const double delta  = delta_dist(rng);

        state.countries[0].legitimacy = old_v;
        PolicyData p;
        p.effects.push_back({"country.legitimacy", "add", delta});
        const auto r = ps::apply_policy_effects(state, CountryId{0}, p);

        REQUIRE(r.ok());
        const double next = state.countries[0].legitimacy;
        CHECK(next >= 0.0);
        CHECK(next <= 1.0);
        CHECK(std::isfinite(next));
    }
}

// =====================================================================
// 4. Symmetry around midpoint
//
// Mechanical invariant: a positive and a negative `add` of the same
// magnitude at the symmetric points (old, 1-old) produce symmetric
// candidates around the midpoint. This is a closed-form algebraic
// property of `new = old + delta * (1 - old)` / `old + delta * old`.
// =====================================================================
TEST_CASE("Symmetry: positive and negative `add` are symmetric around 0.5") {
    GameState state_a;
    state_a.countries.push_back(baseline("SA"));
    state_a.countries[0].legitimacy = 0.30;

    GameState state_b;
    state_b.countries.push_back(baseline("SB"));
    state_b.countries[0].legitimacy = 0.70;

    const double delta = 0.20;

    const double a_next = step_add(state_a, "country.legitimacy",  delta);
    const double b_next = step_add(state_b, "country.legitimacy", -delta);

    // a_next: 0.30 + 0.20 * (1 - 0.30) = 0.44
    // b_next: 0.70 + (-0.20) * 0.70    = 0.56
    // distance from 0.5 is identical: |0.44 - 0.5| == |0.56 - 0.5| = 0.06
    CHECK(std::abs(a_next - 0.5) == doctest::Approx(std::abs(b_next - 0.5)));
}

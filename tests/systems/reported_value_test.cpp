#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/information_accuracy.hpp"
#include "leviathan/systems/reported_value.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::GameState;
namespace ia = leviathan::systems::information_accuracy;
namespace rv = leviathan::systems::reported_value;

// =====================================================================
// M6.4 happy path: accuracy=1.0 returns true_value verbatim;
// accuracy=0.0 returns 0; midpoint interpolates linearly.
// =====================================================================

TEST_CASE("M6.4 from_true_value: accuracy=1.0 returns true_value verbatim (M6.3 ceiling)") {
    const auto r = rv::from_true_value(0.30, 1.0);
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(0.30));
}

TEST_CASE("M6.4 from_true_value: accuracy=0.0 returns 0 regardless of true_value") {
    const auto r_pos = rv::from_true_value(0.55, 0.0);
    const auto r_neg = rv::from_true_value(-0.02, 0.0);
    const auto r_big = rv::from_true_value(1.0e6, 0.0);
    REQUIRE(r_pos);
    REQUIRE(r_neg);
    REQUIRE(r_big);
    CHECK(r_pos.value() == doctest::Approx(0.0));
    CHECK(r_neg.value() == doctest::Approx(0.0));
    CHECK(r_big.value() == doctest::Approx(0.0));
}

TEST_CASE("M6.4 from_true_value: midpoint accuracy=0.5 returns half of true_value") {
    const auto r_pos = rv::from_true_value(10.0, 0.5);
    const auto r_neg = rv::from_true_value(-10.0, 0.5);
    const auto r_small = rv::from_true_value(0.30, 0.5);
    REQUIRE(r_pos);
    REQUIRE(r_neg);
    REQUIRE(r_small);
    CHECK(r_pos.value()   == doctest::Approx(5.0));
    CHECK(r_neg.value()   == doctest::Approx(-5.0));
    CHECK(r_small.value() == doctest::Approx(0.15));
}

TEST_CASE("M6.4 from_true_value: true_value=0 returns 0 for any valid accuracy") {
    for (double a : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        CAPTURE(a);
        const auto r = rv::from_true_value(0.0, a);
        REQUIRE(r);
        CHECK(r.value() == doctest::Approx(0.0));
    }
}

TEST_CASE("M6.4 from_true_value: result is monotonically scaled by accuracy") {
    // For a fixed positive true_value, higher accuracy ->
    // higher reported_value.
    const auto r_lo  = rv::from_true_value(1.0, 0.25);
    const auto r_mid = rv::from_true_value(1.0, 0.50);
    const auto r_hi  = rv::from_true_value(1.0, 0.75);
    REQUIRE(r_lo);
    REQUIRE(r_mid);
    REQUIRE(r_hi);
    CHECK(r_lo.value() < r_mid.value());
    CHECK(r_mid.value() < r_hi.value());
}

// =====================================================================
// M6.4 negative true_value: skeleton supports effect-magnitude shape
// =====================================================================

TEST_CASE("M6.4 from_true_value: negative true_value (effect magnitude) round-trips at accuracy=1.0") {
    // Canonical M5.1 low_stability_unrest effect has value -0.02.
    // At full accuracy the player should see the exact effect.
    const auto r = rv::from_true_value(-0.02, 1.0);
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(-0.02));
}

TEST_CASE("M6.4 from_true_value: negative true_value damps toward 0 at low accuracy") {
    // At accuracy=0.10 the player sees a much smaller-magnitude
    // effect than the truth.
    const auto r = rv::from_true_value(-0.02, 0.10);
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(-0.002));
}

// =====================================================================
// M6.4 validation: invalid inputs rejected
// =====================================================================

TEST_CASE("M6.4 from_true_value: NaN true_value rejected") {
    const auto r = rv::from_true_value(std::nan(""), 1.0);
    REQUIRE(r.failed());
    CHECK(r.error().find("true_value") != std::string::npos);
    CHECK(r.error().find("not finite") != std::string::npos);
}

TEST_CASE("M6.4 from_true_value: infinity true_value rejected") {
    const auto r = rv::from_true_value(
        std::numeric_limits<double>::infinity(), 0.5);
    REQUIRE(r.failed());
    CHECK(r.error().find("true_value") != std::string::npos);
    CHECK(r.error().find("not finite") != std::string::npos);
}

TEST_CASE("M6.4 from_true_value: NaN accuracy rejected") {
    const auto r = rv::from_true_value(1.0, std::nan(""));
    REQUIRE(r.failed());
    CHECK(r.error().find("accuracy") != std::string::npos);
    CHECK(r.error().find("not finite") != std::string::npos);
}

TEST_CASE("M6.4 from_true_value: infinity accuracy rejected") {
    const auto r = rv::from_true_value(1.0,
        std::numeric_limits<double>::infinity());
    REQUIRE(r.failed());
    CHECK(r.error().find("accuracy") != std::string::npos);
    CHECK(r.error().find("not finite") != std::string::npos);
}

TEST_CASE("M6.4 from_true_value: accuracy below 0 rejected") {
    const auto r = rv::from_true_value(1.0, -0.01);
    REQUIRE(r.failed());
    CHECK(r.error().find("accuracy") != std::string::npos);
    CHECK(r.error().find("[0, 1]")   != std::string::npos);
}

TEST_CASE("M6.4 from_true_value: accuracy above 1 rejected") {
    const auto r = rv::from_true_value(1.0, 1.01);
    REQUIRE(r.failed());
    CHECK(r.error().find("accuracy") != std::string::npos);
    CHECK(r.error().find("[0, 1]")   != std::string::npos);
}

// =====================================================================
// M6.4 determinism: repeated identical calls return identical values
// =====================================================================

TEST_CASE("M6.4 from_true_value: deterministic — repeated calls return the same value") {
    const auto r1 = rv::from_true_value(0.42, 0.7);
    const auto r2 = rv::from_true_value(0.42, 0.7);
    const auto r3 = rv::from_true_value(0.42, 0.7);
    REQUIRE(r1);
    REQUIRE(r2);
    REQUIRE(r3);
    CHECK(r1.value() == doctest::Approx(r2.value()));
    CHECK(r2.value() == doctest::Approx(r3.value()));
}

// =====================================================================
// M6.4 composition with M6.6 information_accuracy
// =====================================================================

TEST_CASE("M6.4 + M6.6 composition: with maxed intelligence accuracy=1.0 reported equals true_value") {
    // Pin the M6 pipeline composition: pull the M6.6 accuracy
    // for a country with full intelligence (capability=1.0,
    // budget.intelligence=1.0), feed it into M6.4 along with a
    // true_value, get the true_value back verbatim. Under M6.6
    // the helper only returns 1.0 when both intelligence inputs
    // are at 1.0 — the M6.3 "any country returns 1.0" placeholder
    // is gone.
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "GER";
    ger.government_authority.intelligence_capability = 1.0;
    ger.budget.intelligence                          = 1.0;
    s.countries.push_back(ger);

    const auto acc = ia::compute_for_country(s, CountryId{0});
    REQUIRE(acc);
    CHECK(acc.value() == doctest::Approx(1.0));

    // Trigger threshold from canonical low_stability_unrest.
    const auto rep = rv::from_true_value(0.30, acc.value());
    REQUIRE(rep);
    CHECK(rep.value() == doctest::Approx(0.30));
}

TEST_CASE("M6.4 + M6.6 composition: degraded intelligence damps the reported_value") {
    // The M6.6 damping case the previous test's comment promised:
    // a country with zero intelligence_capability AND zero
    // budget.intelligence sits at the M6.6 accuracy floor
    // (kMinInformationAccuracy = 0.4); M6.4 multiplies the
    // true_value by that floor verbatim.
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "GER";
    ger.government_authority.intelligence_capability = 0.0;
    ger.budget.intelligence                          = 0.0;
    s.countries.push_back(ger);

    const auto acc = ia::compute_for_country(s, CountryId{0});
    REQUIRE(acc);
    CHECK(acc.value() == doctest::Approx(ia::kMinInformationAccuracy));
    CHECK(acc.value() == doctest::Approx(0.4));

    const auto rep = rv::from_true_value(0.30, acc.value());
    REQUIRE(rep);
    CHECK(rep.value() == doctest::Approx(0.30 * 0.4));
}

TEST_CASE("M6.4 + M6.3 composition: an invalid M6.3 lookup propagates through the caller") {
    // The caller that wires M6.3 -> M6.4 must check the M6.3
    // Result first; on failure, M6.4 should not be called.
    // Pin the shape of that error pathway via the M6.3 helper's
    // existing failure mode (empty state.countries).
    GameState s;
    const auto acc = ia::compute_for_country(s, CountryId{0});
    REQUIRE(acc.failed());
    // The caller (a future M6.x consumer) is responsible for
    // not feeding a failed accuracy into M6.4. M6.4 itself,
    // when called with valid inputs (accuracy=0.0 here), still
    // returns success. This test pins the shape of the
    // composition: M6.3 failure path is checkable BEFORE
    // calling M6.4.
    const auto rep = rv::from_true_value(0.30, 0.0);
    REQUIRE(rep);
    CHECK(rep.value() == doctest::Approx(0.0));
}

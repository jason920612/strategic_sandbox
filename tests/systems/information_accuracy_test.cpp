#include <doctest/doctest.h>

#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/information_accuracy.hpp"
#include "leviathan/systems/save_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::GameState;
namespace ia = leviathan::systems::information_accuracy;
namespace ss = leviathan::systems::save_system;

namespace {

// M6.6 test helper: explicitly authors the two inputs the new
// formula reads (intelligence_capability + budget.intelligence)
// plus a couple of unrelated fields the previous M6.3 tests
// asserted the formula does NOT depend on (M6.7's corruption
// term will land later; M6.6 must NOT consume it).
CountryState make_country(int id, const std::string& code,
                          double intelligence_capability = 0.5,
                          double budget_intelligence     = 0.0,
                          double corruption              = 0.25,
                          double stability               = 0.55) {
    CountryState c;
    c.id          = CountryId{id};
    c.id_code     = code;
    c.name        = code;
    c.corruption  = corruption;
    c.stability   = stability;
    c.legitimacy  = 0.55;
    c.government_authority.intelligence_capability =
        intelligence_capability;
    c.budget.intelligence = budget_intelligence;
    return c;
}

// Reproduce the M6.6 formula in the test file so the assertions
// pin a specific numeric value, not just an inequality. If the
// header constants are rebalanced in a future M-driver the
// expected value tracks them automatically.
double expected_accuracy(double cap, double bud) {
    const double intel_score =
        ia::kInformationAccuracyCapabilityWeight * cap +
        ia::kInformationAccuracyBudgetWeight     * bud;
    return ia::kMinInformationAccuracy +
           (1.0 - ia::kMinInformationAccuracy) * intel_score;
}

}  // namespace

// =====================================================================
// M6.6 happy path: formula returns the documented affine combination
// =====================================================================

TEST_CASE("M6.6 compute_for_country: maxed intelligence returns kPlaceholderInformationAccuracy (= 1.0)") {
    // M6.6 contract: the public constant kPlaceholderInformationAccuracy
    // stays as the "no-distortion ceiling" — but it is now returned
    // only when both intelligence inputs are at 1.0 (not for any
    // valid country, which was the M6.3 placeholder semantic).
    GameState s;
    s.countries.push_back(make_country(0, "GER",
                                       /*intelligence_capability*/1.0,
                                       /*budget_intelligence*/   1.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(ia::kPlaceholderInformationAccuracy));
    CHECK(ia::kPlaceholderInformationAccuracy == doctest::Approx(1.0));
}

TEST_CASE("M6.6 compute_for_country: zero intelligence returns kMinInformationAccuracy (= 0.4)") {
    // The M6.6 floor: a country with literally no intelligence
    // capability AND no intelligence budget still gets the floor
    // accuracy, not zero.
    GameState s;
    s.countries.push_back(make_country(0, "GER",
                                       /*intelligence_capability*/0.0,
                                       /*budget_intelligence*/   0.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(ia::kMinInformationAccuracy));
    CHECK(ia::kMinInformationAccuracy == doctest::Approx(0.4));
}

TEST_CASE("M6.6 compute_for_country: result is in [kMinInformationAccuracy, 1.0]") {
    // Across a swept grid of inputs the result must stay inside
    // the closed [0.4, 1.0] range.
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    for (double cap = 0.0; cap <= 1.0; cap += 0.1) {
        for (double bud = 0.0; bud <= 1.0; bud += 0.1) {
            s.countries[0].government_authority.intelligence_capability = cap;
            s.countries[0].budget.intelligence = bud;
            const auto r = ia::compute_for_country(s, CountryId{0});
            REQUIRE(r);
            CHECK(r.value() >= ia::kMinInformationAccuracy);
            CHECK(r.value() <= 1.0);
        }
    }
}

TEST_CASE("M6.6 compute_for_country: formula matches documented expression") {
    // Pin a few concrete (cap, bud) pairs against the documented
    // affine combination. Any rebalance of the header constants
    // is picked up via the helper `expected_accuracy`.
    GameState s;
    s.countries.push_back(make_country(0, "GER"));

    struct Sample { double cap; double bud; };
    const Sample samples[] = {
        {0.0, 0.0},
        {1.0, 1.0},
        {0.5, 0.0},   // default-ish state
        {0.5, 0.5},
        {0.7, 0.3},
        {0.1, 0.9},
        {0.9, 0.1},
    };
    for (const auto& sample : samples) {
        s.countries[0].government_authority.intelligence_capability = sample.cap;
        s.countries[0].budget.intelligence = sample.bud;
        const auto r = ia::compute_for_country(s, CountryId{0});
        REQUIRE(r);
        CHECK(r.value() == doctest::Approx(expected_accuracy(sample.cap, sample.bud)));
    }
}

TEST_CASE("M6.6 compute_for_country: DOES consult intelligence_capability") {
    // Construct two states that differ ONLY in
    // government_authority.intelligence_capability. M6.6 must
    // produce strictly different accuracies — high-intel > low-intel.
    GameState s_low;
    s_low.countries.push_back(make_country(0, "GER",
                                           /*intelligence_capability*/0.1,
                                           /*budget_intelligence*/   0.2));
    GameState s_high;
    s_high.countries.push_back(make_country(0, "GER",
                                            /*intelligence_capability*/0.9,
                                            /*budget_intelligence*/   0.2));

    const auto r_low  = ia::compute_for_country(s_low,  CountryId{0});
    const auto r_high = ia::compute_for_country(s_high, CountryId{0});
    REQUIRE(r_low);
    REQUIRE(r_high);
    CHECK(r_high.value() > r_low.value());
}

TEST_CASE("M6.6 compute_for_country: DOES consult budget.intelligence") {
    // Symmetric to the previous test: two states differing ONLY in
    // budget.intelligence. The funded country must get a strictly
    // higher accuracy.
    GameState s_low;
    s_low.countries.push_back(make_country(0, "GER",
                                           /*intelligence_capability*/0.4,
                                           /*budget_intelligence*/   0.0));
    GameState s_high;
    s_high.countries.push_back(make_country(0, "GER",
                                            /*intelligence_capability*/0.4,
                                            /*budget_intelligence*/   1.0));

    const auto r_low  = ia::compute_for_country(s_low,  CountryId{0});
    const auto r_high = ia::compute_for_country(s_high, CountryId{0});
    REQUIRE(r_low);
    REQUIRE(r_high);
    CHECK(r_high.value() > r_low.value());
}

TEST_CASE("M6.6 compute_for_country: capability weight dominates budget weight") {
    // The header pins kInformationAccuracyCapabilityWeight = 0.7 and
    // kInformationAccuracyBudgetWeight = 0.3 — a 1-unit shift in
    // capability must outweigh the same shift in budget. Pin the
    // ordering so a future weight swap can't go unnoticed.
    GameState s_high_cap;
    s_high_cap.countries.push_back(make_country(0, "GER",
                                                /*intelligence_capability*/1.0,
                                                /*budget_intelligence*/   0.0));
    GameState s_high_bud;
    s_high_bud.countries.push_back(make_country(0, "GER",
                                                /*intelligence_capability*/0.0,
                                                /*budget_intelligence*/   1.0));

    const auto r_cap = ia::compute_for_country(s_high_cap, CountryId{0});
    const auto r_bud = ia::compute_for_country(s_high_bud, CountryId{0});
    REQUIRE(r_cap);
    REQUIRE(r_bud);
    CHECK(r_cap.value() > r_bud.value());
    CHECK(ia::kInformationAccuracyCapabilityWeight >
          ia::kInformationAccuracyBudgetWeight);
    CHECK(ia::kInformationAccuracyCapabilityWeight +
          ia::kInformationAccuracyBudgetWeight == doctest::Approx(1.0));
}

TEST_CASE("M6.6 compute_for_country: monotonic in both inputs") {
    // Stepping either input upward (holding the other) must produce
    // a non-decreasing accuracy.
    GameState s;
    s.countries.push_back(make_country(0, "GER"));

    double prev = -1.0;
    s.countries[0].budget.intelligence = 0.5;
    for (double cap = 0.0; cap <= 1.001; cap += 0.1) {
        s.countries[0].government_authority.intelligence_capability = cap;
        const auto r = ia::compute_for_country(s, CountryId{0});
        REQUIRE(r);
        CHECK(r.value() >= prev);
        prev = r.value();
    }

    prev = -1.0;
    s.countries[0].government_authority.intelligence_capability = 0.5;
    for (double bud = 0.0; bud <= 1.001; bud += 0.1) {
        s.countries[0].budget.intelligence = bud;
        const auto r = ia::compute_for_country(s, CountryId{0});
        REQUIRE(r);
        CHECK(r.value() >= prev);
        prev = r.value();
    }
}

TEST_CASE("M6.6 compute_for_country: out-of-range inputs clamped defensively") {
    // The data layer pins ratios in [0, 1] but the helper clamps
    // defensively so a hand-built test fixture or a future schema
    // change can't push the result outside [0.4, 1.0].
    GameState s_under;
    s_under.countries.push_back(make_country(0, "GER",
                                             /*intelligence_capability*/-0.5,
                                             /*budget_intelligence*/   -0.2));
    GameState s_over;
    s_over.countries.push_back(make_country(0, "GER",
                                            /*intelligence_capability*/1.5,
                                            /*budget_intelligence*/   2.0));

    const auto r_under = ia::compute_for_country(s_under, CountryId{0});
    const auto r_over  = ia::compute_for_country(s_over,  CountryId{0});
    REQUIRE(r_under);
    REQUIRE(r_over);
    CHECK(r_under.value() == doctest::Approx(ia::kMinInformationAccuracy));
    CHECK(r_over.value()  == doctest::Approx(1.0));
}

TEST_CASE("M6.6 compute_for_country: does NOT consume corruption (M6.7 scope)") {
    // M6.6 strictly forbids reading corruption — that's M6.7's
    // territory. Two states differing only in corruption must
    // produce identical accuracies.
    GameState s_clean;
    s_clean.countries.push_back(make_country(0, "GER",
                                             /*intelligence_capability*/0.6,
                                             /*budget_intelligence*/   0.4,
                                             /*corruption*/            0.05));
    GameState s_dirty;
    s_dirty.countries.push_back(make_country(0, "GER",
                                             /*intelligence_capability*/0.6,
                                             /*budget_intelligence*/   0.4,
                                             /*corruption*/            0.95));

    const auto r_clean = ia::compute_for_country(s_clean, CountryId{0});
    const auto r_dirty = ia::compute_for_country(s_dirty, CountryId{0});
    REQUIRE(r_clean);
    REQUIRE(r_dirty);
    CHECK(r_clean.value() == doctest::Approx(r_dirty.value()));
}

// =====================================================================
// M6.3 validation (carried into M6.6): invalid country handle rejected
// =====================================================================

TEST_CASE("M6.6 compute_for_country: invalid CountryId (out of range) rejected") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    const auto r = ia::compute_for_country(s, CountryId{99});
    REQUIRE(r.failed());
    CHECK(r.error().find("99")     != std::string::npos);
    CHECK(r.error().find("is not a valid index")
          != std::string::npos);
}

TEST_CASE("M6.6 compute_for_country: invalid CountryId::invalid() rejected") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    const auto r = ia::compute_for_country(s, CountryId::invalid());
    REQUIRE(r.failed());
    CHECK(r.error().find("is not a valid index")
          != std::string::npos);
}

TEST_CASE("M6.6 compute_for_country: empty state.countries -> CountryId{0} rejected") {
    GameState s;
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
}

// =====================================================================
// M6.6 purity: helper is read-only / deterministic
// =====================================================================

TEST_CASE("M6.6 compute_for_country: does NOT mutate GameState") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.5, 0.3));
    s.countries.push_back(make_country(1, "FRA", 0.7, 0.2));

    const std::string before = ss::serialize(s);
    (void)ia::compute_for_country(s, CountryId{0});
    (void)ia::compute_for_country(s, CountryId{1});
    (void)ia::compute_for_country(s, CountryId{99});   // failed call too
    const std::string after = ss::serialize(s);
    CHECK(before == after);
}

TEST_CASE("M6.6 compute_for_country: deterministic — repeated calls return the same value") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.4, 0.6));
    const auto r1 = ia::compute_for_country(s, CountryId{0});
    const auto r2 = ia::compute_for_country(s, CountryId{0});
    const auto r3 = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r1);
    REQUIRE(r2);
    REQUIRE(r3);
    CHECK(r1.value() == doctest::Approx(r2.value()));
    CHECK(r2.value() == doctest::Approx(r3.value()));
}

// =====================================================================
// M6.6 module surface: constants are stable
// =====================================================================

TEST_CASE("M6.6 kPlaceholderInformationAccuracy: stable public constant") {
    // The constant's semantic graduated from "always-returned" (M6.3)
    // to "no-distortion ceiling" (M6.6). The numeric value 1.0 itself
    // stays put across M6.6 / M6.7.
    CHECK(ia::kPlaceholderInformationAccuracy == doctest::Approx(1.0));
}

TEST_CASE("M6.6 kMinInformationAccuracy: stable public constant") {
    // The accuracy floor introduced in M6.6. Future M6.7 may subtract
    // a corruption term and push the EFFECTIVE accuracy below this
    // floor, but the M6.6 contribution alone never goes below 0.4.
    CHECK(ia::kMinInformationAccuracy == doctest::Approx(0.4));
    CHECK(ia::kMinInformationAccuracy < ia::kPlaceholderInformationAccuracy);
}

TEST_CASE("M6.6 intelligence weights: sum to 1") {
    CHECK(ia::kInformationAccuracyCapabilityWeight +
          ia::kInformationAccuracyBudgetWeight == doctest::Approx(1.0));
    CHECK(ia::kInformationAccuracyCapabilityWeight > 0.0);
    CHECK(ia::kInformationAccuracyBudgetWeight     > 0.0);
}

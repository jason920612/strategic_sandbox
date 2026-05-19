#include <doctest/doctest.h>

#include <cmath>
#include <limits>
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

// M6.7 test helper: authors the three formula inputs explicitly
// (intelligence_capability + budget.intelligence + corruption).
// All three default to values that keep the result at the M6.7
// formula's documented boundaries when their counterparts are
// also at their defaults — see `expected_accuracy` for the exact
// formula reproduction. `corruption` defaults to 0 so a test that
// doesn't care about the M6.7 term still pins the M6.6 baseline.
CountryState make_country(int id, const std::string& code,
                          double intelligence_capability = 0.5,
                          double budget_intelligence     = 0.0,
                          double corruption              = 0.0,
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

// Reproduce the M6.7 formula in the test file so assertions pin
// concrete numeric values rather than inequalities. If the header
// constants are rebalanced in a future M-driver the expected
// value tracks them automatically.
double expected_accuracy(double cap, double bud,
                         double corruption = 0.0) {
    const double intel_score =
        ia::kInformationAccuracyCapabilityWeight * cap +
        ia::kInformationAccuracyBudgetWeight     * bud;
    const double m6_6_base =
        ia::kMinInformationAccuracy +
        (1.0 - ia::kMinInformationAccuracy) * intel_score;
    return m6_6_base -
           ia::kInformationAccuracyCorruptionWeight * corruption;
}

}  // namespace

// =====================================================================
// M6.6 baseline (carried into M6.7 with corruption = 0)
// =====================================================================

TEST_CASE("M6.7 compute_for_country: maxed intel + zero corruption returns kPlaceholderInformationAccuracy (= 1.0)") {
    // Per the M6.7 header doc-comment: the "no-distortion ceiling"
    // is reached only when intelligence is maxed AND corruption is
    // zero. Numeric value 1.0 is unchanged from M6.6.
    GameState s;
    s.countries.push_back(make_country(0, "GER",
                                       /*intelligence_capability*/1.0,
                                       /*budget_intelligence*/   1.0,
                                       /*corruption*/            0.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(ia::kPlaceholderInformationAccuracy));
    CHECK(ia::kPlaceholderInformationAccuracy == doctest::Approx(1.0));
}

TEST_CASE("M6.7 compute_for_country: zero intel + zero corruption returns kMinInformationAccuracy (= 0.4)") {
    // The M6.6 contribution floor — preserved in M6.7 when
    // corruption is zero. M6.7's corruption subtraction can push
    // the total below this floor; the constant itself documents
    // the M6.6 baseline term alone.
    GameState s;
    s.countries.push_back(make_country(0, "GER",
                                       /*intelligence_capability*/0.0,
                                       /*budget_intelligence*/   0.0,
                                       /*corruption*/            0.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(ia::kMinInformationAccuracy));
    CHECK(ia::kMinInformationAccuracy == doctest::Approx(0.4));
}

TEST_CASE("M6.7 compute_for_country: result is in [0.0, 1.0] across the full input cube") {
    // Sweep the cube [0,1]^3 over (cap, bud, corruption). The
    // M6.7 range is [0.0, 1.0] — wider than M6.6's [0.4, 1.0]
    // because the corruption subtraction can drive accuracy
    // toward zero.
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    for (double cap = 0.0; cap <= 1.0; cap += 0.25) {
        for (double bud = 0.0; bud <= 1.0; bud += 0.25) {
            for (double corr = 0.0; corr <= 1.0; corr += 0.25) {
                s.countries[0].government_authority
                    .intelligence_capability = cap;
                s.countries[0].budget.intelligence = bud;
                s.countries[0].corruption          = corr;
                const auto r = ia::compute_for_country(s, CountryId{0});
                REQUIRE(r);
                CHECK(r.value() >= 0.0);
                CHECK(r.value() <= 1.0);
            }
        }
    }
}

TEST_CASE("M6.6 baseline: formula matches documented expression at corruption = 0") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));

    struct Sample { double cap; double bud; };
    const Sample samples[] = {
        {0.0, 0.0}, {1.0, 1.0}, {0.5, 0.0}, {0.5, 0.5},
        {0.7, 0.3}, {0.1, 0.9}, {0.9, 0.1},
    };
    for (const auto& sample : samples) {
        s.countries[0].government_authority.intelligence_capability = sample.cap;
        s.countries[0].budget.intelligence = sample.bud;
        s.countries[0].corruption          = 0.0;
        const auto r = ia::compute_for_country(s, CountryId{0});
        REQUIRE(r);
        CHECK(r.value() == doctest::Approx(
            expected_accuracy(sample.cap, sample.bud, 0.0)));
    }
}

TEST_CASE("M6.6 baseline: DOES consult intelligence_capability") {
    GameState s_low;
    s_low.countries.push_back(make_country(0, "GER",
                                           /*intelligence_capability*/0.1,
                                           /*budget_intelligence*/   0.2,
                                           /*corruption*/            0.0));
    GameState s_high;
    s_high.countries.push_back(make_country(0, "GER",
                                            /*intelligence_capability*/0.9,
                                            /*budget_intelligence*/   0.2,
                                            /*corruption*/            0.0));
    const auto r_low  = ia::compute_for_country(s_low,  CountryId{0});
    const auto r_high = ia::compute_for_country(s_high, CountryId{0});
    REQUIRE(r_low);
    REQUIRE(r_high);
    CHECK(r_high.value() > r_low.value());
}

TEST_CASE("M6.6 baseline: DOES consult budget.intelligence") {
    GameState s_low;
    s_low.countries.push_back(make_country(0, "GER",
                                           /*intelligence_capability*/0.4,
                                           /*budget_intelligence*/   0.0,
                                           /*corruption*/            0.0));
    GameState s_high;
    s_high.countries.push_back(make_country(0, "GER",
                                            /*intelligence_capability*/0.4,
                                            /*budget_intelligence*/   1.0,
                                            /*corruption*/            0.0));
    const auto r_low  = ia::compute_for_country(s_low,  CountryId{0});
    const auto r_high = ia::compute_for_country(s_high, CountryId{0});
    REQUIRE(r_low);
    REQUIRE(r_high);
    CHECK(r_high.value() > r_low.value());
}

TEST_CASE("M6.6 baseline: capability weight dominates budget weight") {
    GameState s_high_cap;
    s_high_cap.countries.push_back(make_country(0, "GER",
                                                /*intelligence_capability*/1.0,
                                                /*budget_intelligence*/   0.0,
                                                /*corruption*/            0.0));
    GameState s_high_bud;
    s_high_bud.countries.push_back(make_country(0, "GER",
                                                /*intelligence_capability*/0.0,
                                                /*budget_intelligence*/   1.0,
                                                /*corruption*/            0.0));
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

TEST_CASE("M6.6 baseline: monotonic in both intelligence inputs (at corruption = 0)") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    s.countries[0].corruption = 0.0;

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

// =====================================================================
// M6.7 corruption term (RFC-090 §6.7, RFC-080 §8 `-Corruption`)
// =====================================================================

TEST_CASE("M6.7 corruption: zero corruption preserves M6.6 baseline") {
    // Pin that the M6.7 body reduces to the M6.6 formula exactly
    // when corruption = 0. This is what makes M6.7 a strict
    // extension of M6.6 rather than a replacement.
    GameState s;
    s.countries.push_back(make_country(0, "GER",
                                       /*intelligence_capability*/0.6,
                                       /*budget_intelligence*/   0.4,
                                       /*corruption*/            0.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(expected_accuracy(0.6, 0.4, 0.0)));
}

TEST_CASE("M6.7 corruption: max corruption + zero intel = 0.0 (full blackout)") {
    // The chosen weight `kInformationAccuracyCorruptionWeight =
    // 0.4` is symmetric to `kMinInformationAccuracy = 0.4`. At
    // max corruption + zero intelligence, the M6.6 baseline (0.4)
    // is fully cancelled.
    GameState s;
    s.countries.push_back(make_country(0, "GER",
                                       /*intelligence_capability*/0.0,
                                       /*budget_intelligence*/   0.0,
                                       /*corruption*/            1.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(0.0));
}

TEST_CASE("M6.7 corruption: max corruption + max intel = 1.0 - kCorruptionWeight (= 0.6)") {
    // The corruption subtraction is independent of the M6.6
    // baseline magnitude — even a maxed-intelligence country
    // loses kInformationAccuracyCorruptionWeight to corruption.
    GameState s;
    s.countries.push_back(make_country(0, "GER",
                                       /*intelligence_capability*/1.0,
                                       /*budget_intelligence*/   1.0,
                                       /*corruption*/            1.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(
        1.0 - ia::kInformationAccuracyCorruptionWeight));
    CHECK(r.value() == doctest::Approx(0.6));
}

TEST_CASE("M6.7 corruption: formula matches documented expression across (cap, bud, corruption) samples") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));

    struct Sample { double cap; double bud; double corr; };
    const Sample samples[] = {
        {0.0, 0.0, 0.5},   // half blackout, baseline 0.4 → 0.2
        {1.0, 1.0, 0.5},   // ceiling, baseline 1.0 → 0.8
        {0.5, 0.0, 0.25},  // M6.6 default-ish + light corruption
        {0.7, 0.3, 0.10},
        {0.1, 0.9, 0.40},
        {0.5, 0.5, 1.00},
        {0.3, 0.3, 0.30},
    };
    for (const auto& sample : samples) {
        s.countries[0].government_authority.intelligence_capability = sample.cap;
        s.countries[0].budget.intelligence = sample.bud;
        s.countries[0].corruption          = sample.corr;
        const auto r = ia::compute_for_country(s, CountryId{0});
        REQUIRE(r);
        CHECK(r.value() == doctest::Approx(
            expected_accuracy(sample.cap, sample.bud, sample.corr)));
    }
}

TEST_CASE("M6.7 corruption: monotonically NON-increasing in corruption") {
    // Holding intelligence fixed, raising corruption must
    // produce a non-increasing accuracy.
    GameState s;
    s.countries.push_back(make_country(0, "GER",
                                       /*intelligence_capability*/0.6,
                                       /*budget_intelligence*/   0.4));

    double prev = std::numeric_limits<double>::infinity();
    for (double corr = 0.0; corr <= 1.001; corr += 0.1) {
        s.countries[0].corruption = corr;
        const auto r = ia::compute_for_country(s, CountryId{0});
        REQUIRE(r);
        CHECK(r.value() <= prev);
        prev = r.value();
    }
}

TEST_CASE("M6.7 corruption: DOES consult corruption (strictly different accuracy for low vs high)") {
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
    CHECK(r_clean.value() > r_dirty.value());
    // The difference is exactly the weighted corruption delta.
    CHECK((r_clean.value() - r_dirty.value()) == doctest::Approx(
        ia::kInformationAccuracyCorruptionWeight * (0.95 - 0.05)));
}

TEST_CASE("M6.7 corruption: can push effective accuracy below kMinInformationAccuracy") {
    // With non-zero corruption, the M6.6 floor is NOT a lower
    // bound on the total. Pin that explicitly so a future edit
    // that tries to re-add a defensive `std::max(_, kMin)` would
    // be caught.
    GameState s;
    s.countries.push_back(make_country(0, "GER",
                                       /*intelligence_capability*/0.0,
                                       /*budget_intelligence*/   0.0,
                                       /*corruption*/            0.5));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() < ia::kMinInformationAccuracy);
    CHECK(r.value() == doctest::Approx(
        ia::kMinInformationAccuracy -
        ia::kInformationAccuracyCorruptionWeight * 0.5));
}

// =====================================================================
// M6.7 strict input validation
// (per feedback_no_silent_degradation — no clamping, fail loudly)
// =====================================================================

TEST_CASE("M6.7 validation: finite out-of-range intelligence_capability rejected") {
    GameState s_under;
    s_under.countries.push_back(make_country(0, "GER",
                                             /*intelligence_capability*/-0.5,
                                             /*budget_intelligence*/   0.5,
                                             /*corruption*/            0.0));
    const auto r_under = ia::compute_for_country(s_under, CountryId{0});
    REQUIRE(r_under.failed());
    CHECK(r_under.error().find("intelligence_capability")
          != std::string::npos);
    CHECK(r_under.error().find("finite ratio in [0, 1]")
          != std::string::npos);

    GameState s_over;
    s_over.countries.push_back(make_country(0, "GER",
                                            /*intelligence_capability*/1.5,
                                            /*budget_intelligence*/   0.5,
                                            /*corruption*/            0.0));
    const auto r_over = ia::compute_for_country(s_over, CountryId{0});
    REQUIRE(r_over.failed());
    CHECK(r_over.error().find("intelligence_capability")
          != std::string::npos);
}

TEST_CASE("M6.7 validation: finite out-of-range budget.intelligence rejected") {
    GameState s_under;
    s_under.countries.push_back(make_country(0, "GER",
                                             /*intelligence_capability*/0.5,
                                             /*budget_intelligence*/   -0.2,
                                             /*corruption*/            0.0));
    const auto r_under = ia::compute_for_country(s_under, CountryId{0});
    REQUIRE(r_under.failed());
    CHECK(r_under.error().find("budget.intelligence")
          != std::string::npos);

    GameState s_over;
    s_over.countries.push_back(make_country(0, "GER",
                                            /*intelligence_capability*/0.5,
                                            /*budget_intelligence*/   2.0,
                                            /*corruption*/            0.0));
    const auto r_over = ia::compute_for_country(s_over, CountryId{0});
    REQUIRE(r_over.failed());
    CHECK(r_over.error().find("budget.intelligence")
          != std::string::npos);
}

TEST_CASE("M6.7 validation: finite out-of-range corruption rejected") {
    GameState s_under;
    s_under.countries.push_back(make_country(0, "GER",
                                             /*intelligence_capability*/0.5,
                                             /*budget_intelligence*/   0.5,
                                             /*corruption*/            -0.1));
    const auto r_under = ia::compute_for_country(s_under, CountryId{0});
    REQUIRE(r_under.failed());
    CHECK(r_under.error().find("corruption") != std::string::npos);
    CHECK(r_under.error().find("finite ratio in [0, 1]")
          != std::string::npos);

    GameState s_over;
    s_over.countries.push_back(make_country(0, "GER",
                                            /*intelligence_capability*/0.5,
                                            /*budget_intelligence*/   0.5,
                                            /*corruption*/            1.5));
    const auto r_over = ia::compute_for_country(s_over, CountryId{0});
    REQUIRE(r_over.failed());
    CHECK(r_over.error().find("corruption") != std::string::npos);
}

TEST_CASE("M6.7 validation: NaN intelligence_capability rejected") {
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/std::numeric_limits<double>::quiet_NaN(),
        /*budget_intelligence*/   0.3,
        /*corruption*/            0.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("GER") != std::string::npos);
    CHECK(r.error().find("intelligence_capability")
          != std::string::npos);
    CHECK(r.error().find("finite ratio in [0, 1]")
          != std::string::npos);
}

TEST_CASE("M6.7 validation: +Inf intelligence_capability rejected") {
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/std::numeric_limits<double>::infinity(),
        /*budget_intelligence*/   0.3,
        /*corruption*/            0.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("intelligence_capability")
          != std::string::npos);
}

TEST_CASE("M6.7 validation: -Inf intelligence_capability rejected") {
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/-std::numeric_limits<double>::infinity(),
        /*budget_intelligence*/   0.3,
        /*corruption*/            0.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("intelligence_capability")
          != std::string::npos);
}

TEST_CASE("M6.7 validation: NaN budget.intelligence rejected") {
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/0.5,
        /*budget_intelligence*/   std::numeric_limits<double>::quiet_NaN(),
        /*corruption*/            0.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("budget.intelligence")
          != std::string::npos);
}

TEST_CASE("M6.7 validation: +Inf budget.intelligence rejected") {
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/0.5,
        /*budget_intelligence*/   std::numeric_limits<double>::infinity(),
        /*corruption*/            0.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("budget.intelligence")
          != std::string::npos);
}

TEST_CASE("M6.7 validation: -Inf budget.intelligence rejected") {
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/0.5,
        /*budget_intelligence*/   -std::numeric_limits<double>::infinity(),
        /*corruption*/            0.0));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("budget.intelligence")
          != std::string::npos);
}

TEST_CASE("M6.7 validation: NaN corruption rejected") {
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/0.5,
        /*budget_intelligence*/   0.3,
        /*corruption*/            std::numeric_limits<double>::quiet_NaN()));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("GER") != std::string::npos);
    CHECK(r.error().find("corruption") != std::string::npos);
    CHECK(r.error().find("finite ratio in [0, 1]")
          != std::string::npos);
}

TEST_CASE("M6.7 validation: +Inf corruption rejected") {
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/0.5,
        /*budget_intelligence*/   0.3,
        /*corruption*/            std::numeric_limits<double>::infinity()));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("corruption") != std::string::npos);
}

TEST_CASE("M6.7 validation: -Inf corruption rejected") {
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/0.5,
        /*budget_intelligence*/   0.3,
        /*corruption*/            -std::numeric_limits<double>::infinity()));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("corruption") != std::string::npos);
}

TEST_CASE("M6.7 validation: capability checked before budget before corruption (deterministic short-circuit)") {
    // When ALL THREE inputs are bad, the helper short-circuits on
    // the first (capability), then budget, then corruption — so
    // the diagnostic message is deterministic.
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/std::numeric_limits<double>::quiet_NaN(),
        /*budget_intelligence*/   std::numeric_limits<double>::quiet_NaN(),
        /*corruption*/            std::numeric_limits<double>::quiet_NaN()));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("intelligence_capability")
          != std::string::npos);
    CHECK(r.error().find("budget.intelligence")
          == std::string::npos);
    CHECK(r.error().find("corruption") == std::string::npos);
}

TEST_CASE("M6.7 validation: budget checked before corruption when capability is OK") {
    GameState s;
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/0.5,
        /*budget_intelligence*/   std::numeric_limits<double>::quiet_NaN(),
        /*corruption*/            std::numeric_limits<double>::quiet_NaN()));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("budget.intelligence")
          != std::string::npos);
    CHECK(r.error().find("corruption") == std::string::npos);
}

TEST_CASE("M6.7 validation: failure does not mutate state") {
    GameState s;
    const double bad_corr = std::numeric_limits<double>::quiet_NaN();
    s.countries.push_back(make_country(
        0, "GER",
        /*intelligence_capability*/0.5,
        /*budget_intelligence*/   0.3,
        /*corruption*/            bad_corr));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(std::isnan(s.countries[0].corruption));
    CHECK(s.countries[0].government_authority
            .intelligence_capability == doctest::Approx(0.5));
    CHECK(s.countries[0].budget.intelligence == doctest::Approx(0.3));
    CHECK(s.countries[0].id_code == "GER");
}

// =====================================================================
// M6.3 validation (carried into M6.7): invalid country handle rejected
// =====================================================================

TEST_CASE("M6.7 compute_for_country: invalid CountryId (out of range) rejected") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    const auto r = ia::compute_for_country(s, CountryId{99});
    REQUIRE(r.failed());
    CHECK(r.error().find("99")     != std::string::npos);
    CHECK(r.error().find("is not a valid index")
          != std::string::npos);
}

TEST_CASE("M6.7 compute_for_country: invalid CountryId::invalid() rejected") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    const auto r = ia::compute_for_country(s, CountryId::invalid());
    REQUIRE(r.failed());
    CHECK(r.error().find("is not a valid index")
          != std::string::npos);
}

TEST_CASE("M6.7 compute_for_country: empty state.countries -> CountryId{0} rejected") {
    GameState s;
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
}

// =====================================================================
// M6.7 purity: helper is read-only / deterministic
// =====================================================================

TEST_CASE("M6.7 compute_for_country: does NOT mutate GameState") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.5, 0.3, 0.10));
    s.countries.push_back(make_country(1, "FRA", 0.7, 0.2, 0.20));

    const std::string before = ss::serialize(s);
    (void)ia::compute_for_country(s, CountryId{0});
    (void)ia::compute_for_country(s, CountryId{1});
    (void)ia::compute_for_country(s, CountryId{99});   // failed call too
    const std::string after = ss::serialize(s);
    CHECK(before == after);
}

TEST_CASE("M6.7 compute_for_country: deterministic — repeated calls return the same value") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.4, 0.6, 0.25));
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
// M6.7 module surface: constants are stable
// =====================================================================

TEST_CASE("M6.7 kPlaceholderInformationAccuracy: stable public constant") {
    CHECK(ia::kPlaceholderInformationAccuracy == doctest::Approx(1.0));
}

TEST_CASE("M6.7 kMinInformationAccuracy: stable public constant") {
    // Still 0.4 — the floor of the M6.6 contribution alone. M6.7's
    // corruption subtraction can push the EFFECTIVE accuracy below
    // this; the constant itself documents only the M6.6 baseline.
    CHECK(ia::kMinInformationAccuracy == doctest::Approx(0.4));
    CHECK(ia::kMinInformationAccuracy < ia::kPlaceholderInformationAccuracy);
}

TEST_CASE("M6.7 intelligence weights: sum to 1") {
    CHECK(ia::kInformationAccuracyCapabilityWeight +
          ia::kInformationAccuracyBudgetWeight == doctest::Approx(1.0));
    CHECK(ia::kInformationAccuracyCapabilityWeight > 0.0);
    CHECK(ia::kInformationAccuracyBudgetWeight     > 0.0);
}

TEST_CASE("M6.7 kInformationAccuracyCorruptionWeight: stable public constant") {
    // Symmetric to kMinInformationAccuracy so the zero-intel +
    // max-corruption corner produces exactly 0.0.
    CHECK(ia::kInformationAccuracyCorruptionWeight == doctest::Approx(0.4));
    CHECK(ia::kInformationAccuracyCorruptionWeight ==
          doctest::Approx(ia::kMinInformationAccuracy));
}

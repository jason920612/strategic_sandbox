// M6 closeout-audit PropagandaBias unit tests.
//
// Sibling to information_accuracy_test.cpp — both helpers read
// `government_authority.media_control` (the only existing
// CountryState field that maps rigorously onto an RFC-080 §8
// term per the audit-doc classification matrix). The
// PropagandaBias helper is the Bias-side surface; the
// MediaFreedomSignal addition in information_accuracy is the
// accuracy-side surface.

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/propaganda_bias.hpp"
#include "leviathan/systems/save_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::GameState;
namespace pb = leviathan::systems::propaganda_bias;
namespace ss = leviathan::systems::save_system;

namespace {

CountryState make_country(int id, const std::string& code,
                          double media_control = 0.0) {
    CountryState c;
    c.id      = CountryId{id};
    c.id_code = code;
    c.name    = code;
    c.government_authority.media_control = media_control;
    return c;
}

}  // namespace

// =====================================================================
// Formula
// =====================================================================

TEST_CASE("propaganda_bias compute_for_country: zero media_control returns 0") {
    GameState s;
    s.countries.push_back(make_country(0, "FRE", /*media_control*/0.0));
    const auto r = pb::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(0.0));
}

TEST_CASE("propaganda_bias compute_for_country: max media_control returns kPropagandaBiasMaxMagnitude") {
    GameState s;
    s.countries.push_back(make_country(0, "CTL", /*media_control*/1.0));
    const auto r = pb::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(pb::kPropagandaBiasMaxMagnitude));
    CHECK(pb::kPropagandaBiasMaxMagnitude == doctest::Approx(0.3));
}

TEST_CASE("propaganda_bias compute_for_country: monotonically NON-decreasing in media_control") {
    GameState s;
    s.countries.push_back(make_country(0, "MID"));
    double prev = -1.0;
    for (double mc = 0.0; mc <= 1.001; mc += 0.1) {
        s.countries[0].government_authority.media_control = mc;
        const auto r = pb::compute_for_country(s, CountryId{0});
        REQUIRE(r);
        CHECK(r.value() >= prev);
        prev = r.value();
    }
}

TEST_CASE("propaganda_bias compute_for_country: formula matches documented expression") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    const double samples[] = {0.0, 0.1, 0.25, 0.5, 0.75, 1.0};
    for (double mc : samples) {
        s.countries[0].government_authority.media_control = mc;
        const auto r = pb::compute_for_country(s, CountryId{0});
        REQUIRE(r);
        CHECK(r.value() == doctest::Approx(
            pb::kPropagandaBiasMaxMagnitude * mc));
    }
}

TEST_CASE("propaganda_bias compute_for_country: result is in [0, kPropagandaBiasMaxMagnitude]") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    for (double mc = 0.0; mc <= 1.0; mc += 0.05) {
        s.countries[0].government_authority.media_control = mc;
        const auto r = pb::compute_for_country(s, CountryId{0});
        REQUIRE(r);
        CHECK(r.value() >= 0.0);
        CHECK(r.value() <= pb::kPropagandaBiasMaxMagnitude);
    }
}

// =====================================================================
// Strict validation (feedback_no_silent_degradation +
// feedback_api_signature_expresses_failure)
// =====================================================================

TEST_CASE("propaganda_bias compute_for_country: invalid CountryId (out of range) rejected") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    const auto r = pb::compute_for_country(s, CountryId{99});
    REQUIRE(r.failed());
    CHECK(r.error().find("99")             != std::string::npos);
    CHECK(r.error().find("is not a valid index") != std::string::npos);
}

TEST_CASE("propaganda_bias compute_for_country: CountryId::invalid() rejected") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    const auto r = pb::compute_for_country(s, CountryId::invalid());
    REQUIRE(r.failed());
    CHECK(r.error().find("is not a valid index") != std::string::npos);
}

TEST_CASE("propaganda_bias compute_for_country: empty state.countries -> CountryId{0} rejected") {
    GameState s;
    const auto r = pb::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
}

TEST_CASE("propaganda_bias compute_for_country: NaN media_control rejected") {
    GameState s;
    auto c = make_country(0, "BAD");
    c.government_authority.media_control =
        std::numeric_limits<double>::quiet_NaN();
    s.countries.push_back(c);
    const auto r = pb::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("media_control") != std::string::npos);
    CHECK(r.error().find("finite ratio in [0, 1]") != std::string::npos);
}

TEST_CASE("propaganda_bias compute_for_country: +Inf media_control rejected") {
    GameState s;
    auto c = make_country(0, "BAD");
    c.government_authority.media_control =
        std::numeric_limits<double>::infinity();
    s.countries.push_back(c);
    const auto r = pb::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("media_control") != std::string::npos);
}

TEST_CASE("propaganda_bias compute_for_country: -Inf media_control rejected") {
    GameState s;
    auto c = make_country(0, "BAD");
    c.government_authority.media_control =
        -std::numeric_limits<double>::infinity();
    s.countries.push_back(c);
    const auto r = pb::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("media_control") != std::string::npos);
}

TEST_CASE("propaganda_bias compute_for_country: out-of-range media_control rejected (negative)") {
    GameState s;
    auto c = make_country(0, "BAD");
    c.government_authority.media_control = -0.1;
    s.countries.push_back(c);
    const auto r = pb::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("media_control") != std::string::npos);
}

TEST_CASE("propaganda_bias compute_for_country: out-of-range media_control rejected (> 1)") {
    GameState s;
    auto c = make_country(0, "BAD");
    c.government_authority.media_control = 1.5;
    s.countries.push_back(c);
    const auto r = pb::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    CHECK(r.error().find("media_control") != std::string::npos);
}

TEST_CASE("propaganda_bias compute_for_country: failure does not mutate state") {
    GameState s;
    auto c = make_country(0, "BAD");
    c.government_authority.media_control =
        std::numeric_limits<double>::quiet_NaN();
    s.countries.push_back(c);

    const std::string before = ss::serialize(s);
    const auto r = pb::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
    const std::string after = ss::serialize(s);
    CHECK(before == after);
}

// =====================================================================
// Purity: helper is read-only / deterministic
// =====================================================================

TEST_CASE("propaganda_bias compute_for_country: does NOT mutate GameState") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*media_control*/0.3));
    s.countries.push_back(make_country(1, "FRA", /*media_control*/0.7));

    const std::string before = ss::serialize(s);
    (void)pb::compute_for_country(s, CountryId{0});
    (void)pb::compute_for_country(s, CountryId{1});
    (void)pb::compute_for_country(s, CountryId{99});   // failed call too
    const std::string after = ss::serialize(s);
    CHECK(before == after);
}

TEST_CASE("propaganda_bias compute_for_country: deterministic — repeated calls return the same value") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*media_control*/0.42));
    const auto r1 = pb::compute_for_country(s, CountryId{0});
    const auto r2 = pb::compute_for_country(s, CountryId{0});
    const auto r3 = pb::compute_for_country(s, CountryId{0});
    REQUIRE(r1);
    REQUIRE(r2);
    REQUIRE(r3);
    CHECK(r1.value() == doctest::Approx(r2.value()));
    CHECK(r2.value() == doctest::Approx(r3.value()));
}

// =====================================================================
// Module surface
// =====================================================================

TEST_CASE("propaganda_bias kPropagandaBiasMaxMagnitude: stable public constant in (0, 1)") {
    CHECK(pb::kPropagandaBiasMaxMagnitude > 0.0);
    CHECK(pb::kPropagandaBiasMaxMagnitude < 1.0);
}

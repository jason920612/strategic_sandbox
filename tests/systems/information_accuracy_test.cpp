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

CountryState make_country(int id, const std::string& code,
                          double corruption = 0.25,
                          double stability  = 0.55) {
    CountryState c;
    c.id          = CountryId{id};
    c.id_code     = code;
    c.name        = code;
    c.corruption  = corruption;
    c.stability   = stability;
    c.legitimacy  = 0.55;
    return c;
}

}  // namespace

// =====================================================================
// M6.3 happy path: placeholder returns the public constant
// =====================================================================

TEST_CASE("M6.3 compute_for_country: valid country returns kPlaceholderInformationAccuracy (= 1.0)") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(ia::kPlaceholderInformationAccuracy));
    CHECK(ia::kPlaceholderInformationAccuracy == doctest::Approx(1.0));
}

TEST_CASE("M6.3 compute_for_country: result is in [0, 1] for a valid country") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r);
    CHECK(r.value() >= 0.0);
    CHECK(r.value() <= 1.0);
}

TEST_CASE("M6.3 compute_for_country: placeholder body does NOT consult country fields yet") {
    // Construct two states whose only difference is the country's
    // corruption value. M6.3 placeholder returns 1.0 regardless;
    // M6.6 / M6.7 will replace this body with a formula that does
    // read corruption / intelligence-budget fields and produce
    // different values per country.
    GameState s_a;
    s_a.countries.push_back(make_country(0, "GER", /*corruption*/0.05));
    GameState s_b;
    s_b.countries.push_back(make_country(0, "GER", /*corruption*/0.95));

    const auto r_a = ia::compute_for_country(s_a, CountryId{0});
    const auto r_b = ia::compute_for_country(s_b, CountryId{0});
    REQUIRE(r_a);
    REQUIRE(r_b);
    CHECK(r_a.value() == doctest::Approx(r_b.value()));
    CHECK(r_a.value() == doctest::Approx(1.0));
    // When M6.7 lands, this test should be REWRITTEN to pin the
    // expected difference. Until then it pins that M6.3 is a
    // strict placeholder.
}

// =====================================================================
// M6.3 validation: invalid country handle is rejected
// =====================================================================

TEST_CASE("M6.3 compute_for_country: invalid CountryId (out of range) rejected") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    const auto r = ia::compute_for_country(s, CountryId{99});
    REQUIRE(r.failed());
    CHECK(r.error().find("99")     != std::string::npos);
    CHECK(r.error().find("is not a valid index")
          != std::string::npos);
}

TEST_CASE("M6.3 compute_for_country: invalid CountryId::invalid() rejected") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    const auto r = ia::compute_for_country(s, CountryId::invalid());
    REQUIRE(r.failed());
    // The exact integer of "invalid" is -1; pin the substring.
    CHECK(r.error().find("is not a valid index")
          != std::string::npos);
}

TEST_CASE("M6.3 compute_for_country: empty state.countries -> CountryId{0} rejected") {
    GameState s;
    // No countries.
    const auto r = ia::compute_for_country(s, CountryId{0});
    REQUIRE(r.failed());
}

// =====================================================================
// M6.3 purity: helper is read-only / deterministic
// =====================================================================

TEST_CASE("M6.3 compute_for_country: does NOT mutate GameState") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.25, 0.55));
    s.countries.push_back(make_country(1, "FRA", 0.30, 0.60));

    // Snapshot the entire state via the save layer's
    // deterministic serializer; before/after must match.
    const std::string before = ss::serialize(s);
    (void)ia::compute_for_country(s, CountryId{0});
    (void)ia::compute_for_country(s, CountryId{1});
    (void)ia::compute_for_country(s, CountryId{99});   // failed call too
    const std::string after = ss::serialize(s);
    CHECK(before == after);
}

TEST_CASE("M6.3 compute_for_country: deterministic — repeated calls return the same value") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
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
// M6.3 module surface: constant + signature are stable
// =====================================================================

TEST_CASE("M6.3 kPlaceholderInformationAccuracy: stable public constant") {
    // Greppable / stable. Future M6.6 / M6.7 body changes
    // should leave this constant alone — it remains the
    // "no-distortion ceiling".
    CHECK(ia::kPlaceholderInformationAccuracy == doctest::Approx(1.0));
}

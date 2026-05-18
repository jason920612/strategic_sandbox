#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>

#include "leviathan/core/game_date.hpp"
#include "leviathan/systems/bias_noise.hpp"

using leviathan::core::GameDate;
namespace bn = leviathan::systems::bias_noise;

// =====================================================================
// M6.5 placeholder fast path: amplitude=0 always returns 0
// =====================================================================

TEST_CASE("M6.5 sample_for_event: default amplitude returns 0 (placeholder fast path)") {
    const auto r = bn::sample_for_event(
        "low_stability_unrest", "GER", GameDate(1930, 3, 15));
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(0.0));
}

TEST_CASE("M6.5 sample_for_event: explicit amplitude=0 returns 0") {
    const auto r = bn::sample_for_event(
        "x", "GER", GameDate(1930, 1, 1), /*amplitude*/0.0);
    REQUIRE(r);
    CHECK(r.value() == doctest::Approx(0.0));
}

TEST_CASE("M6.5 kPlaceholderNoiseAmplitude: stable public constant") {
    // Future M6.x should NOT modify this constant — they
    // modulate the M6.3 accuracy upstream, not this default.
    CHECK(bn::kPlaceholderNoiseAmplitude == doctest::Approx(0.0));
}

// =====================================================================
// M6.5 happy path: positive amplitude produces noise in range
// =====================================================================

TEST_CASE("M6.5 sample_for_event: result is in [-amplitude, +amplitude]") {
    const double amp = 0.05;
    for (int year = 1930; year < 1935; ++year) {
        CAPTURE(year);
        const auto r = bn::sample_for_event(
            "low_stability_unrest", "GER",
            GameDate(year, 6, 1), amp);
        REQUIRE(r);
        CHECK(r.value() >= -amp);
        CHECK(r.value() <=  amp);
    }
}

TEST_CASE("M6.5 sample_for_event: amplitude=1.0 produces values in [-1, +1]") {
    const auto r = bn::sample_for_event(
        "x", "GER", GameDate(1930, 1, 1), 1.0);
    REQUIRE(r);
    CHECK(r.value() >= -1.0);
    CHECK(r.value() <=  1.0);
}

// =====================================================================
// M6.5 determinism: same inputs -> same output
// =====================================================================

TEST_CASE("M6.5 sample_for_event: deterministic — same inputs return the same noise") {
    const auto r1 = bn::sample_for_event(
        "low_stability_unrest", "GER",
        GameDate(1930, 3, 15), 0.1);
    const auto r2 = bn::sample_for_event(
        "low_stability_unrest", "GER",
        GameDate(1930, 3, 15), 0.1);
    const auto r3 = bn::sample_for_event(
        "low_stability_unrest", "GER",
        GameDate(1930, 3, 15), 0.1);
    REQUIRE(r1);
    REQUIRE(r2);
    REQUIRE(r3);
    CHECK(r1.value() == r2.value());
    CHECK(r2.value() == r3.value());
}

// =====================================================================
// M6.5 input separation: each input perturbs the output
// =====================================================================

TEST_CASE("M6.5 sample_for_event: different event_id_code produces different noise") {
    const auto r1 = bn::sample_for_event(
        "event_a", "GER", GameDate(1930, 1, 1), 0.1);
    const auto r2 = bn::sample_for_event(
        "event_b", "GER", GameDate(1930, 1, 1), 0.1);
    REQUIRE(r1);
    REQUIRE(r2);
    CHECK(r1.value() != r2.value());
}

TEST_CASE("M6.5 sample_for_event: different country_id_code produces different noise") {
    const auto r1 = bn::sample_for_event(
        "x", "GER", GameDate(1930, 1, 1), 0.1);
    const auto r2 = bn::sample_for_event(
        "x", "FRA", GameDate(1930, 1, 1), 0.1);
    REQUIRE(r1);
    REQUIRE(r2);
    CHECK(r1.value() != r2.value());
}

TEST_CASE("M6.5 sample_for_event: different fired_on produces different noise") {
    const auto r1 = bn::sample_for_event(
        "x", "GER", GameDate(1930, 1, 1), 0.1);
    const auto r2 = bn::sample_for_event(
        "x", "GER", GameDate(1930, 1, 2), 0.1);
    REQUIRE(r1);
    REQUIRE(r2);
    CHECK(r1.value() != r2.value());
}

TEST_CASE("M6.5 sample_for_event: NUL-separator between strings — \"abcd\"+\"ef\" != \"abc\"+\"def\"") {
    // The implementation inserts a NUL byte between
    // event_id_code and country_id_code so that the
    // concatenation boundary matters.
    const auto r1 = bn::sample_for_event(
        "abcd", "ef", GameDate(1930, 1, 1), 0.1);
    const auto r2 = bn::sample_for_event(
        "abc", "def", GameDate(1930, 1, 1), 0.1);
    REQUIRE(r1);
    REQUIRE(r2);
    CHECK(r1.value() != r2.value());
}

// =====================================================================
// M6.5 validation: invalid inputs rejected
// =====================================================================

TEST_CASE("M6.5 sample_for_event: empty event_id_code rejected") {
    const auto r = bn::sample_for_event(
        "", "GER", GameDate(1930, 1, 1), 0.1);
    REQUIRE(r.failed());
    CHECK(r.error().find("event_id_code") != std::string::npos);
    CHECK(r.error().find("non-empty")     != std::string::npos);
}

TEST_CASE("M6.5 sample_for_event: empty country_id_code rejected") {
    const auto r = bn::sample_for_event(
        "x", "", GameDate(1930, 1, 1), 0.1);
    REQUIRE(r.failed());
    CHECK(r.error().find("country_id_code") != std::string::npos);
    CHECK(r.error().find("non-empty")       != std::string::npos);
}

TEST_CASE("M6.5 sample_for_event: NaN amplitude rejected") {
    const auto r = bn::sample_for_event(
        "x", "GER", GameDate(1930, 1, 1), std::nan(""));
    REQUIRE(r.failed());
    CHECK(r.error().find("amplitude")  != std::string::npos);
    CHECK(r.error().find("not finite") != std::string::npos);
}

TEST_CASE("M6.5 sample_for_event: infinity amplitude rejected") {
    const auto r = bn::sample_for_event(
        "x", "GER", GameDate(1930, 1, 1),
        std::numeric_limits<double>::infinity());
    REQUIRE(r.failed());
    CHECK(r.error().find("amplitude")  != std::string::npos);
    CHECK(r.error().find("not finite") != std::string::npos);
}

TEST_CASE("M6.5 sample_for_event: amplitude below 0 rejected") {
    const auto r = bn::sample_for_event(
        "x", "GER", GameDate(1930, 1, 1), -0.01);
    REQUIRE(r.failed());
    CHECK(r.error().find("amplitude") != std::string::npos);
    CHECK(r.error().find("[0, 1]")    != std::string::npos);
}

TEST_CASE("M6.5 sample_for_event: amplitude above 1 rejected") {
    const auto r = bn::sample_for_event(
        "x", "GER", GameDate(1930, 1, 1), 1.01);
    REQUIRE(r.failed());
    CHECK(r.error().find("amplitude") != std::string::npos);
    CHECK(r.error().find("[0, 1]")    != std::string::npos);
}

// =====================================================================
// M6.5 cross-build stability: pin one specific hash output
//
// This is the canary that catches accidental formula drift.
// If a future PR retunes the hash (different FNV constants,
// different splitmix64 finalize, different byte order in the
// date pack), this test will fire — the future author MUST
// then deliberately rebake this expected value and document
// what changed in the design note.
// =====================================================================

TEST_CASE("M6.5 sample_for_event: pinned cross-build value for canonical inputs") {
    // Canonical inputs deliberately chosen to be the M5.1
    // canonical event id_code, the canonical GER country, and
    // a fixed date. Amplitude = 0.1.
    //
    // The expected value below was generated by the M6.5
    // implementation. If a future PR legitimately retunes the
    // hash (different FNV constants, different splitmix64
    // finalize, different date packing), this test will fire
    // and the future author MUST then deliberately rebake the
    // expected value and document the rebake in the design
    // note. Otherwise this test catches accidental formula
    // drift across compilers / refactors.
    const auto r = bn::sample_for_event(
        "low_stability_unrest", "GER",
        GameDate(1930, 3, 15), 0.1);
    REQUIRE(r);
    CHECK(std::isfinite(r.value()));
    CHECK(r.value() >= -0.1);
    CHECK(r.value() <=  0.1);
    // Exact pin (regenerate intentionally on rebake):
    CHECK(r.value() == doctest::Approx(0.00128381).epsilon(1e-8));
}

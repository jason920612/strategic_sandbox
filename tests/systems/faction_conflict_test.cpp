// M7.4 faction_conflict unit tests (RFC-090 §7.4 + RFC-020 §8).
//
// Pins:
//   * Each RFC-020 §8 rivalry pair triggers radicalism drift
//     when both sides are present in the same country with
//     influence above threshold.
//   * Type outside the allowlist generates no drift.
//   * Per-country scoping: a rival in country X doesn't
//     activate the pair in country Y.
//   * Strict validation: NaN / out-of-range inputs rejected.
//   * Determinism: same state → same drift.
//   * Asymptotic-add cannot push radicalism above 1.0.

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/faction_conflict.hpp"
#include "leviathan/systems/save_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameState;
namespace fc = leviathan::systems::faction_conflict;
namespace ss = leviathan::systems::save_system;

namespace {

CountryState make_country(int id, const std::string& code) {
    CountryState c;
    c.id      = CountryId{id};
    c.id_code = code;
    c.name    = code;
    c.stability  = 0.55;
    c.legitimacy = 0.55;
    return c;
}

FactionState make_faction(int id, int country_id,
                          const std::string& id_code,
                          const std::string& country_id_code,
                          const std::string& type,
                          double influence,
                          double radicalism = 0.30,
                          double loyalty    = 0.50) {
    FactionState f;
    f.id              = FactionId{id};
    f.country         = CountryId{country_id};
    f.id_code         = id_code;
    f.country_id_code = country_id_code;
    f.name            = id_code;
    f.type            = type;
    f.support         = 0.5;
    f.influence       = influence;
    f.radicalism      = radicalism;
    f.loyalty         = loyalty;
    f.resources       = 0.0;
    return f;
}

double expected_asymptotic_add(double current, double delta) {
    return current + delta * (1.0 - current);
}

}  // namespace

// =====================================================================
// RFC-020 §8 rivalry pairs each activate on threshold
// =====================================================================

TEST_CASE("M7.4 military ↔ intelligence: both at threshold drifts radicalism on both") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military",
                                       /*infl*/0.60,
                                       /*rad*/0.25));
    s.factions.push_back(make_faction(1, 0, "X_int", "X", "intelligence",
                                       /*infl*/0.55,
                                       /*rad*/0.30));

    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(r.value().pairs_active     == 1);
    CHECK(r.value().factions_drifted == 2);

    CHECK(s.factions[0].radicalism == doctest::Approx(
        expected_asymptotic_add(0.25, fc::kFactionConflictAsymptoticRadicalismDelta)));
    CHECK(s.factions[1].radicalism == doctest::Approx(
        expected_asymptotic_add(0.30, fc::kFactionConflictAsymptoticRadicalismDelta)));
}

TEST_CASE("M7.4 workers ↔ technical_elites pair activates correctly") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_w", "X", "workers", 0.60));
    s.factions.push_back(make_faction(1, 0, "X_t", "X", "technical_elites", 0.55));
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(r.value().pairs_active == 1);
}

TEST_CASE("M7.4 bureaucracy ↔ local_elites pair activates correctly") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_b", "X", "bureaucracy", 0.60));
    s.factions.push_back(make_faction(1, 0, "X_le", "X", "local_elites", 0.55));
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(r.value().pairs_active == 1);
}

TEST_CASE("M7.4 students ↔ religious pair activates correctly") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_s", "X", "students", 0.60));
    s.factions.push_back(make_faction(1, 0, "X_r", "X", "religious", 0.55));
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(r.value().pairs_active == 1);
}

TEST_CASE("M7.4 media ↔ intelligence pair activates correctly") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_md", "X", "media", 0.60));
    s.factions.push_back(make_faction(1, 0, "X_int", "X", "intelligence", 0.55));
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(r.value().pairs_active == 1);
}

// =====================================================================
// Out-of-allowlist faction types do not generate drift
// =====================================================================

TEST_CASE("M7.4 unpaired faction types (e.g. farmers) generate no conflict") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_f1", "X", "farmers",  0.60));
    s.factions.push_back(make_faction(1, 0, "X_f2", "X", "students", 0.60));  // students has a §8 pair but no rival here
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(r.value().pairs_active     == 0);
    CHECK(r.value().factions_drifted == 0);
}

// =====================================================================
// Threshold gating: below threshold = inactive
// =====================================================================

TEST_CASE("M7.4 dormant rivalry: one side below influence threshold → no drift") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military",     0.60));
    s.factions.push_back(make_faction(1, 0, "X_i", "X", "intelligence",
        /*infl*/fc::kFactionConflictInfluenceThreshold));  // exactly at threshold = NOT strictly > threshold
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(r.value().pairs_active     == 0);
    CHECK(r.value().factions_drifted == 0);
}

// =====================================================================
// Per-country scoping: X's military vs Y's intelligence ≠ active pair
// =====================================================================

TEST_CASE("M7.4 per-country scoping: factions in different countries do NOT form a pair") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.countries.push_back(make_country(1, "Y"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military",     0.60));
    s.factions.push_back(make_faction(1, 1, "Y_i", "Y", "intelligence", 0.55));
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(r.value().pairs_active     == 0);
    CHECK(r.value().factions_drifted == 0);
}

// =====================================================================
// Intelligence participates in TWO pairs (military + media)
// =====================================================================

TEST_CASE("M7.4 intelligence vs military AND media: both pairs activate; intelligence faction is counted ONCE in factions_drifted") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m",   "X", "military",     0.60, /*rad*/0.20));
    s.factions.push_back(make_faction(1, 0, "X_md",  "X", "media",        0.55, /*rad*/0.25));
    s.factions.push_back(make_faction(2, 0, "X_int", "X", "intelligence", 0.55, /*rad*/0.30));
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(r.value().pairs_active == 2);
    // factions_drifted counts DISTINCT factions, so 3
    // (military once via military↔intel; media once via
    // media↔intel; intel ONCE because the set semantics
    // dedupes despite intel participating in both pairs).
    CHECK(r.value().factions_drifted == 3);
    // Intelligence faction drifted TWICE (compounded
    // asymptotically). Pin the compound math.
    const double after_first =
        expected_asymptotic_add(0.30, fc::kFactionConflictAsymptoticRadicalismDelta);
    const double after_second =
        expected_asymptotic_add(after_first, fc::kFactionConflictAsymptoticRadicalismDelta);
    CHECK(s.factions[2].radicalism == doctest::Approx(after_second));
}

// =====================================================================
// Strict validation
// =====================================================================

TEST_CASE("M7.4 NaN influence on a candidate faction FAILS LOUDLY") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    auto bad = make_faction(0, 0, "X_m", "X", "military",
                            std::numeric_limits<double>::quiet_NaN());
    s.factions.push_back(bad);
    s.factions.push_back(make_faction(1, 0, "X_i", "X", "intelligence", 0.55));
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r.failed());
    CHECK(r.error().find("X_m") != std::string::npos);
    CHECK(r.error().find("influence") != std::string::npos);
}

TEST_CASE("M7.4 NaN radicalism on a participating faction FAILS LOUDLY") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    auto bad = make_faction(0, 0, "X_m", "X", "military", 0.60);
    bad.radicalism = std::numeric_limits<double>::quiet_NaN();
    s.factions.push_back(bad);
    s.factions.push_back(make_faction(1, 0, "X_i", "X", "intelligence", 0.55));
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r.failed());
    CHECK(r.error().find("X_m") != std::string::npos);
    CHECK(r.error().find("radicalism") != std::string::npos);
}

// =====================================================================
// Asymptotic-add bounds: cannot push above 1.0
// =====================================================================

TEST_CASE("M7.4 radicalism near 1.0: asymptotic drift stays in [0, 1]") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military",     0.60, /*rad*/0.99));
    s.factions.push_back(make_faction(1, 0, "X_i", "X", "intelligence", 0.55, /*rad*/0.99));
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(s.factions[0].radicalism <= 1.0);
    CHECK(s.factions[1].radicalism <= 1.0);
    CHECK(s.factions[0].radicalism > 0.99);  // strict-monotone-up
}

// =====================================================================
// Determinism + state-purity
// =====================================================================

TEST_CASE("M7.4 deterministic: same state → same drift") {
    auto build = []() {
        GameState s;
        s.countries.push_back(make_country(0, "X"));
        s.factions.push_back(make_faction(0, 0, "X_m", "X", "military",     0.60));
        s.factions.push_back(make_faction(1, 0, "X_i", "X", "intelligence", 0.55));
        return s;
    };
    GameState a = build();
    GameState b = build();
    REQUIRE(fc::tick_apply_pressure(a));
    REQUIRE(fc::tick_apply_pressure(b));
    CHECK(ss::serialize(a) == ss::serialize(b));
}

TEST_CASE("M7.4 no factions: succeeds with zero counts") {
    GameState s;
    const auto r = fc::tick_apply_pressure(s);
    REQUIRE(r);
    CHECK(r.value().pairs_active     == 0);
    CHECK(r.value().factions_drifted == 0);
}

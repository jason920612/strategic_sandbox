#include <doctest/doctest.h>

#include <algorithm>
#include <string>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/ai_policy.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::GameState;
using leviathan::core::PolicyData;
namespace ai = leviathan::systems::ai_policy;

namespace {

CountryState make_country(std::string id_code) {
    CountryState c;
    c.id_code = std::move(id_code);
    c.name    = c.id_code;
    return c;
}

PolicyData make_policy(std::string id_code) {
    PolicyData p;
    p.id_code = std::move(id_code);
    p.name    = p.id_code;
    return p;
}

}  // namespace

// =====================================================================
// RCR-1 ai_policy::select_policies — emptiness / boundary
// =====================================================================

TEST_CASE("RCR-1 ai_policy: empty state produces empty selections") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    const auto r = ai::select_policies(state);
    REQUIRE(r);
    CHECK(r.value().empty());
}

TEST_CASE("RCR-1 ai_policy: countries but no policies produces empty selections") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.countries.push_back(make_country("GER"));
    state.countries.push_back(make_country("FRA"));

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    CHECK(r.value().empty());
}

TEST_CASE("RCR-1 ai_policy: policies but no countries produces empty selections") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.policies.push_back(make_policy("raise_taxes"));

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    CHECK(r.value().empty());
}

// =====================================================================
// RCR-1 ai_policy::select_policies — happy path
// =====================================================================

TEST_CASE("ai_policy: 3 countries no player, synthetic high-pressure state "
          "emits capacity-bounded picks per country "
          "(empty PolicyData scores tie at 0; vector-order tie-break)") {
    // Default-constructed CountryState has stability=0 + legitimacy=0
    // which sums to pressure ≥ 2.0 — well above kPressureThreshold —
    // so each country emits selections. Default
    // `government_authority.bureaucratic_compliance = 0.5` plus
    // `administrative_efficiency = 0.0` and budget_balance = 0
    // yields capacity = 0.5×0 + 0.3×0.5 + 0.2×1.0 = 0.35 → 2 picks
    // per country (kCapacityLowMax=0.30, kCapacityMediumMax=0.60).
    // Synthetic empty PolicyData all score 0 → tie → vector-order
    // tie-break selects raise_taxes then expand_welfare for each
    // country.
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.countries.push_back(make_country("GER"));
    state.countries.push_back(make_country("FRA"));
    state.countries.push_back(make_country("JPN"));
    state.policies.push_back(make_policy("raise_taxes"));
    state.policies.push_back(make_policy("expand_welfare"));

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() == 6u);  // 3 countries × 2 picks each

    for (std::size_t ci = 0; ci < 3u; ++ci) {
        CAPTURE(ci);
        const auto& a = r.value()[ci * 2 + 0];
        const auto& b = r.value()[ci * 2 + 1];
        CHECK(a.country
              == CountryId{static_cast<CountryId::underlying_type>(ci)});
        CHECK(b.country
              == CountryId{static_cast<CountryId::underlying_type>(ci)});
        CHECK(a.policy_id_code == "raise_taxes");
        CHECK(b.policy_id_code == "expand_welfare");
    }
}

TEST_CASE("RCR-1 ai_policy: player country is skipped") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.countries.push_back(make_country("GER"));
    state.countries.push_back(make_country("FRA"));
    state.countries.push_back(make_country("JPN"));
    state.policies.push_back(make_policy("raise_taxes"));
    state.player_country = CountryId{1};  // FRA is the player

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() == 2);
    CHECK(r.value()[0].country == CountryId{0});
    CHECK(r.value()[1].country == CountryId{2});

    const auto fra_count = std::count_if(
        r.value().begin(), r.value().end(),
        [](const ai::Selection& s) { return s.country == CountryId{1}; });
    CHECK(fra_count == 0);
}

TEST_CASE("RCR-1 ai_policy: player country invalid -> no skip") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.countries.push_back(make_country("GER"));
    state.countries.push_back(make_country("FRA"));
    state.policies.push_back(make_policy("raise_taxes"));
    // state.player_country defaults to invalid()
    REQUIRE(!state.player_country.valid());

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    CHECK(r.value().size() == 2);
}

TEST_CASE("RCR-1 ai_policy: player country out of range -> no skip") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.countries.push_back(make_country("GER"));
    state.policies.push_back(make_policy("raise_taxes"));
    state.player_country = CountryId{42};  // out of range

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    CHECK(r.value().size() == 1);
    CHECK(r.value()[0].country == CountryId{0});
}

// =====================================================================
// RCR-1 ai_policy::select_policies — determinism + non-mutation
// =====================================================================

TEST_CASE("RCR-1 ai_policy: deterministic — repeated calls return identical selections") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.countries.push_back(make_country("GER"));
    state.countries.push_back(make_country("FRA"));
    state.countries.push_back(make_country("JPN"));
    state.policies.push_back(make_policy("raise_taxes"));
    state.policies.push_back(make_policy("expand_welfare"));

    const auto r1 = ai::select_policies(state);
    const auto r2 = ai::select_policies(state);
    const auto r3 = ai::select_policies(state);
    REQUIRE(r1);
    REQUIRE(r2);
    REQUIRE(r3);
    REQUIRE(r1.value().size() == r2.value().size());
    REQUIRE(r1.value().size() == r3.value().size());

    for (std::size_t i = 0; i < r1.value().size(); ++i) {
        CHECK(r1.value()[i].country        == r2.value()[i].country);
        CHECK(r1.value()[i].country        == r3.value()[i].country);
        CHECK(r1.value()[i].policy_id_code == r2.value()[i].policy_id_code);
        CHECK(r1.value()[i].policy_id_code == r3.value()[i].policy_id_code);
    }
}

TEST_CASE("RCR-1 ai_policy: select_policies does NOT mutate state") {
    GameState before = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    before.countries.push_back(make_country("GER"));
    before.countries.push_back(make_country("FRA"));
    before.policies.push_back(make_policy("raise_taxes"));
    GameState after = before;

    const auto r = ai::select_policies(after);
    REQUIRE(r);

    // The visible fields the caller might touch should all match the
    // pre-call snapshot — selection is pure.
    CHECK(after.countries.size() == before.countries.size());
    CHECK(after.policies.size()  == before.policies.size());
    CHECK(after.event_history.size() == before.event_history.size());
    CHECK(after.rng.counter == before.rng.counter);
    CHECK(after.current_date.year()  == before.current_date.year());
    CHECK(after.current_date.month() == before.current_date.month());
    CHECK(after.current_date.day()   == before.current_date.day());
}

TEST_CASE("RCR-1 ai_policy: selection vector follows state.countries vector order") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.countries.push_back(make_country("Z_first_loaded"));
    state.countries.push_back(make_country("A_second_loaded"));
    state.countries.push_back(make_country("M_third_loaded"));
    state.policies.push_back(make_policy("first_policy"));

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() == 3);
    // Insertion order, NOT alphabetical sort.
    CHECK(r.value()[0].country == CountryId{0});
    CHECK(r.value()[1].country == CountryId{1});
    CHECK(r.value()[2].country == CountryId{2});
}

// =====================================================================
// RCR-1 ai_policy::apply_selected_policies — actually mutate state
// =====================================================================

TEST_CASE("RCR-1 ai_policy: apply_selected_policies applies to non-player countries via existing policy path") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.current_date = leviathan::core::GameDate(1930, 1, 1);

    // Three countries.
    auto a = make_country("AAA"); a.id = CountryId{0}; a.stability = 0.5;
    auto b = make_country("BBB"); b.id = CountryId{1}; b.stability = 0.5;
    auto c = make_country("CCC"); c.id = CountryId{2}; c.stability = 0.5;
    state.countries.push_back(a);
    state.countries.push_back(b);
    state.countries.push_back(c);

    // One policy that raises stability.
    PolicyData p = make_policy("test_stability_up");
    p.duration_days = 30;
    p.effects.push_back({"country.stability", "add", 0.1});
    state.policies.push_back(p);

    const auto r = ai::apply_selected_policies(state);
    REQUIRE(r);
    CHECK(r.value().considered == 3);
    CHECK(r.value().applied    == 3);
    CHECK(r.value().skipped    == 0);
    CHECK(r.value().failed_countries.empty());

    CHECK(state.countries[0].stability == doctest::Approx(0.6));
    CHECK(state.countries[1].stability == doctest::Approx(0.6));
    CHECK(state.countries[2].stability == doctest::Approx(0.6));

    // M1.15 active_policies appended through the existing policy
    // path (the AI applicator reuses policy::apply_policy_effects).
    CHECK(state.countries[0].active_policies.size() == 1u);
    CHECK(state.countries[1].active_policies.size() == 1u);
    CHECK(state.countries[2].active_policies.size() == 1u);
}

TEST_CASE("RCR-1 ai_policy: apply_selected_policies skips the player country") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.current_date = leviathan::core::GameDate(1930, 1, 1);

    auto a = make_country("AAA"); a.id = CountryId{0}; a.stability = 0.5;
    auto b = make_country("BBB"); b.id = CountryId{1}; b.stability = 0.5;
    state.countries.push_back(a);
    state.countries.push_back(b);
    state.player_country = CountryId{0};

    PolicyData p = make_policy("test_stab_up");
    p.duration_days = 30;
    p.effects.push_back({"country.stability", "add", 0.1});
    state.policies.push_back(p);

    const auto r = ai::apply_selected_policies(state);
    REQUIRE(r);
    CHECK(r.value().considered == 1);
    CHECK(r.value().applied    == 1);
    // Player country unchanged.
    CHECK(state.countries[0].stability == doctest::Approx(0.5));
    CHECK(state.countries[1].stability == doctest::Approx(0.6));
    CHECK(state.countries[0].active_policies.size() == 0u);
    CHECK(state.countries[1].active_policies.size() == 1u);
}

TEST_CASE("Hardening: apply_selected_policies fail-FAST on per-country failure") {
    // Post-M6.7 strict numeric validation + signature honesty:
    // the previous fail-continue (accumulate into
    // ApplyOutcome.failed_countries) is gone. A per-country apply
    // failure now surfaces as a hard Result::failure so the
    // monthly pipeline propagates it to the runner.
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.current_date = leviathan::core::GameDate(1930, 1, 1);

    auto a = make_country("AAA"); a.id = CountryId{0};
    auto b = make_country("BBB"); b.id = CountryId{1};
    state.countries.push_back(a);
    state.countries.push_back(b);

    PolicyData p = make_policy("broken_policy");
    p.duration_days = 30;
    p.effects.push_back({"country.this_field_does_not_exist", "add", 0.1});
    state.policies.push_back(p);

    const auto r = ai::apply_selected_policies(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("broken_policy") != std::string::npos);
    CHECK(r.error().find("unknown country field") != std::string::npos);
}

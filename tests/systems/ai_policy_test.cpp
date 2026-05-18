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

TEST_CASE("RCR-1 ai_policy: 3 countries no player gets one selection each, all to policies[0]") {
    GameState state = leviathan::core::make_game_state(
        leviathan::core::SimulationConfig{});
    state.countries.push_back(make_country("GER"));
    state.countries.push_back(make_country("FRA"));
    state.countries.push_back(make_country("JPN"));
    state.policies.push_back(make_policy("raise_taxes"));
    state.policies.push_back(make_policy("expand_welfare"));

    const auto r = ai::select_policies(state);
    REQUIRE(r);
    REQUIRE(r.value().size() == 3);

    CHECK(r.value()[0].country == CountryId{0});
    CHECK(r.value()[1].country == CountryId{1});
    CHECK(r.value()[2].country == CountryId{2});
    CHECK(r.value()[0].policy_id_code == "raise_taxes");
    CHECK(r.value()[1].policy_id_code == "raise_taxes");
    CHECK(r.value()[2].policy_id_code == "raise_taxes");
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

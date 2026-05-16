#include <doctest/doctest.h>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/core/simulation_config.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventId;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::LogEntry;
using leviathan::core::make_game_state;
using leviathan::core::PolicyData;
using leviathan::core::PolicyId;
using leviathan::core::ProvinceId;
using leviathan::core::ProvinceState;
using leviathan::core::SimulationConfig;

TEST_CASE("Default GameState has the documented baseline") {
    GameState state;
    CHECK(state.current_date == GameDate(1930, 1, 1));
    CHECK(state.rng.seed     == 0u);
    CHECK(state.rng.counter  == 0u);
    CHECK(state.countries.empty());
    CHECK(state.provinces.empty());
    CHECK(state.factions.empty());
    CHECK(state.policies.empty());
    CHECK(state.events.empty());
    CHECK(state.logs.empty());
}

TEST_CASE("GameState fields are directly mutable (no setters needed)") {
    GameState state;

    state.current_date = GameDate(1945, 5, 8);
    state.rng.seed     = 19450508u;
    state.rng.counter  = 17u;

    CHECK(state.current_date == GameDate(1945, 5, 8));
    CHECK(state.rng.seed     == 19450508u);
    CHECK(state.rng.counter  == 17u);
}

TEST_CASE("GameState containers accept their entity types") {
    GameState state;

    CountryState germany;
    germany.id   = CountryId{1};
    germany.name = "Germany";
    state.countries.push_back(std::move(germany));
    state.provinces.push_back(ProvinceState{ProvinceId{10}, CountryId{1}});
    state.factions.push_back(FactionState{FactionId{100}, CountryId{1}});
    state.policies.push_back(PolicyData{PolicyId{1}, "increase_military_budget"});
    state.events.push_back(EventDefinition{EventId{1}, "labor_strike"});
    LogEntry init_entry;
    init_entry.date    = GameDate(1930, 1, 1);
    init_entry.message = "init";
    state.logs.push_back(std::move(init_entry));

    CHECK(state.countries.size() == 1);
    CHECK(state.provinces.size() == 1);
    CHECK(state.factions.size()  == 1);
    CHECK(state.policies.size()  == 1);
    CHECK(state.events.size()    == 1);
    CHECK(state.logs.size()      == 1);

    CHECK(state.countries.front().id   == CountryId{1});
    CHECK(state.countries.front().name == "Germany");
    CHECK(state.provinces.front().owner == CountryId{1});
}

TEST_CASE("make_game_state propagates config.start_date") {
    SimulationConfig config;
    config.start_date = GameDate(1936, 7, 17);
    config.seed       = 19360717u;

    GameState state = make_game_state(config);

    CHECK(state.current_date == GameDate(1936, 7, 17));
    CHECK(state.rng.seed     == 19360717u);
}

TEST_CASE("make_game_state resets rng.counter to 0") {
    SimulationConfig config;
    config.seed = 12345u;

    GameState state = make_game_state(config);

    // Counter MUST start at 0 so deterministic replays from a given
    // seed reproduce the same draws regardless of any prior state.
    CHECK(state.rng.counter == 0u);
}

TEST_CASE("make_game_state leaves all entity containers empty") {
    SimulationConfig config;
    GameState state = make_game_state(config);

    CHECK(state.countries.empty());
    CHECK(state.provinces.empty());
    CHECK(state.factions.empty());
    CHECK(state.policies.empty());
    CHECK(state.events.empty());
    CHECK(state.logs.empty());
}

TEST_CASE("make_game_state honours a custom start_date independent of default") {
    SimulationConfig config;
    config.start_date = GameDate(2000, 1, 1);

    GameState state = make_game_state(config);

    CHECK(state.current_date == GameDate(2000, 1, 1));
    // Default GameState's date should not bleed through.
    CHECK_FALSE(state.current_date == GameDate(1930, 1, 1));
}

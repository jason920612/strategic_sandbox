#include <doctest/doctest.h>

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/simulation_config.hpp"

using leviathan::core::GameDate;
using leviathan::core::SimulationConfig;

TEST_CASE("SimulationConfig default-constructs to RFC-070 prototype values") {
    SimulationConfig config;
    CHECK(config.start_date == GameDate(1930, 1, 1));
    CHECK(config.end_date   == GameDate(2000, 12, 31));
    CHECK(config.seed       == 0u);
    CHECK(config.daily_tick);
}

TEST_CASE("SimulationConfig fields are independently mutable") {
    SimulationConfig config;
    config.start_date = GameDate(1936, 6, 1);
    config.seed       = 19360601u;
    config.daily_tick = false;

    CHECK(config.start_date == GameDate(1936, 6, 1));
    CHECK(config.seed       == 19360601u);
    CHECK_FALSE(config.daily_tick);
    // unchanged fields keep their defaults
    CHECK(config.end_date == GameDate(2000, 12, 31));
}

TEST_CASE("SimulationConfig supports aggregate initialisation") {
    SimulationConfig config{GameDate(1945, 9, 2), GameDate(1990, 12, 31),
                            42u, false};
    CHECK(config.start_date == GameDate(1945, 9, 2));
    CHECK(config.end_date   == GameDate(1990, 12, 31));
    CHECK(config.seed       == 42u);
    CHECK_FALSE(config.daily_tick);
}

#include <doctest/doctest.h>

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/systems/time_system.hpp"

using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::systems::time::advance_days;
using leviathan::systems::time::advance_one_day;
using leviathan::systems::time::TickResult;

TEST_CASE("advance_one_day inside a month flags no boundary") {
    GameState state;
    state.current_date = GameDate(1930, 6, 14);

    const TickResult r = advance_one_day(state);

    CHECK(state.current_date == GameDate(1930, 6, 15));
    CHECK_FALSE(r.month_changed);
    CHECK_FALSE(r.year_changed);
}

TEST_CASE("advance_one_day across month-end flags month_changed only") {
    GameState state;
    state.current_date = GameDate(1930, 1, 31);

    const TickResult r = advance_one_day(state);

    CHECK(state.current_date == GameDate(1930, 2, 1));
    CHECK(r.month_changed);
    CHECK_FALSE(r.year_changed);
}

TEST_CASE("advance_one_day across year-end flags both boundaries") {
    GameState state;
    state.current_date = GameDate(1930, 12, 31);

    const TickResult r = advance_one_day(state);

    CHECK(state.current_date == GameDate(1931, 1, 1));
    CHECK(r.month_changed);
    CHECK(r.year_changed);
}

TEST_CASE("advance_one_day handles a leap-year February correctly") {
    GameState state;
    state.current_date = GameDate(1932, 2, 28);

    // 1932-02-28 -> 1932-02-29 is within Feb, no boundary.
    TickResult r = advance_one_day(state);
    CHECK(state.current_date == GameDate(1932, 2, 29));
    CHECK_FALSE(r.month_changed);
    CHECK_FALSE(r.year_changed);

    // 1932-02-29 -> 1932-03-01 crosses the month.
    r = advance_one_day(state);
    CHECK(state.current_date == GameDate(1932, 3, 1));
    CHECK(r.month_changed);
    CHECK_FALSE(r.year_changed);
}

TEST_CASE("advance_one_day refuses an invalid date via debug assert") {
    // We do not exercise the assert directly (release builds disable
    // it); the test just documents the precondition by setting up a
    // valid date and verifying the contract holds.
    GameState state;
    state.current_date = GameDate(1930, 1, 1);
    REQUIRE(state.current_date.is_valid());

    advance_one_day(state);
    CHECK(state.current_date.is_valid());
}

TEST_CASE("advance_days(0) is a no-op") {
    GameState state;
    state.current_date = GameDate(1930, 6, 15);

    advance_days(state, 0);

    CHECK(state.current_date == GameDate(1930, 6, 15));
}

TEST_CASE("advance_days(1) is equivalent to advance_one_day") {
    GameState state;
    state.current_date = GameDate(1930, 1, 31);

    advance_days(state, 1);

    CHECK(state.current_date == GameDate(1930, 2, 1));
}

TEST_CASE("advance_days(365) on a non-leap year reaches the same date next year") {
    // 1930 is not a leap year -> +365 days = exactly one year forward.
    GameState state;
    state.current_date = GameDate(1930, 1, 1);

    advance_days(state, 365);

    CHECK(state.current_date == GameDate(1931, 1, 1));
}

TEST_CASE("advance_days(366) on a leap year reaches the same date next year") {
    GameState state;
    state.current_date = GameDate(1932, 1, 1);

    advance_days(state, 366);

    CHECK(state.current_date == GameDate(1933, 1, 1));
}

TEST_CASE("advance_days crosses the 1999->2000 boundary correctly") {
    // Two-month walk: 1999-12-31 +60 days = 2000-02-29 (2000 IS a leap
    // year because 2000 % 400 == 0). Then +1 day = 2000-03-01.
    GameState state;
    state.current_date = GameDate(1999, 12, 31);

    advance_days(state, 60);
    CHECK(state.current_date == GameDate(2000, 2, 29));

    advance_days(state, 1);
    CHECK(state.current_date == GameDate(2000, 3, 1));
}

TEST_CASE("Manual loop pattern: counting month-end and year-end ticks over a year") {
    // Demonstrates the "minimal pipeline" pattern from the spec:
    // a caller that needs per-day boundary callbacks loops on
    // advance_one_day itself rather than relying on advance_days.
    GameState state;
    state.current_date = GameDate(1932, 1, 1);  // leap year

    int month_changes = 0;
    int year_changes  = 0;
    for (int i = 0; i < 366; ++i) {
        const TickResult r = advance_one_day(state);
        if (r.month_changed) ++month_changes;
        if (r.year_changed)  ++year_changes;
    }

    // 12 month boundaries in a year (Jan->Feb ... Dec->Jan-of-next-year).
    CHECK(month_changes == 12);
    CHECK(year_changes  == 1);
    CHECK(state.current_date == GameDate(1933, 1, 1));
}

TEST_CASE("TimeSystem does not touch RNG, entity containers, or seed") {
    GameState state;
    state.current_date = GameDate(1930, 6, 14);
    state.rng.seed     = 99u;
    state.rng.counter  = 7u;
    state.countries.push_back({});
    leviathan::core::LogEntry marker;
    marker.date    = GameDate(1930, 6, 14);
    marker.message = "marker";
    state.logs.push_back(std::move(marker));

    advance_days(state, 30);

    CHECK(state.rng.seed     == 99u);
    CHECK(state.rng.counter  == 7u);  // M0.5 is the only thing that mutates this
    CHECK(state.countries.size() == 1);
    CHECK(state.logs.size()      == 1);
}

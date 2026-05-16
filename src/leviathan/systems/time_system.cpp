#include "leviathan/systems/time_system.hpp"

#include <cassert>

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"

namespace leviathan::systems::time {

TickResult advance_one_day(core::GameState& state) {
    assert(state.current_date.is_valid() &&
           "advance_one_day called with an invalid current_date");

    const core::GameDate before = state.current_date;
    state.current_date.advance_one_day();

    // Aggregate-init relies on TickResult field order: {month_changed, year_changed}.
    return TickResult{
        before.month() != state.current_date.month(),
        before.year()  != state.current_date.year(),
    };
}

void advance_days(core::GameState& state, int days) {
    assert(days >= 0 &&
           "advance_days does not accept negative deltas");
    assert(state.current_date.is_valid() &&
           "advance_days called with an invalid current_date");

    for (int i = 0; i < days; ++i) {
        advance_one_day(state);  // boundary flags intentionally discarded
    }
}

}  // namespace leviathan::systems::time

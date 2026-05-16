// TimeSystem - the first "system" in the project.
//
// Per RFC-060 §2 ("資料集中、系統分離"), systems are not member functions
// on GameState. They are free functions that take GameState& (or
// const GameState&) and return either void or a small result struct
// describing what just happened.
//
// M0.4 advances simulation time at one-day resolution. It does not
// run economy, factions, events, or any other simulation logic; those
// land in later milestones and will be invoked by the system pipeline
// after the time tick.

#ifndef LEVIATHAN_SYSTEMS_TIME_SYSTEM_HPP
#define LEVIATHAN_SYSTEMS_TIME_SYSTEM_HPP

#include "leviathan/core/game_state.hpp"

namespace leviathan::systems::time {

// Describes what calendar boundaries were crossed by a single-day tick.
// `month_changed` implies the date is now in a different month than
// before the tick; `year_changed` implies the same for year. A simple
// day-to-day tick within the same month produces {false, false}.
struct TickResult {
    bool month_changed = false;
    bool year_changed  = false;
};

// Advances state.current_date by exactly one day.
//
// Returns the boundary flags so a caller can act on month-end or
// year-end events without re-computing the previous date.
//
// Precondition: state.current_date.is_valid().
TickResult advance_one_day(core::GameState& state);

// Advances state.current_date by `days` days. Equivalent to calling
// advance_one_day() in a loop. Boundary flags are not returned in
// aggregate form - if a caller cares about per-day boundaries they
// should loop and call advance_one_day themselves.
//
// Preconditions: state.current_date.is_valid() and days >= 0.
// Negative deltas are unsupported in M0.4: simulation time only
// moves forward.
void advance_days(core::GameState& state, int days);

}  // namespace leviathan::systems::time

#endif  // LEVIATHAN_SYSTEMS_TIME_SYSTEM_HPP

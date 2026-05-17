// InterestGroupSystem - one step of interest-group reaction (M3.2).
//
// First M3 system to mutate the M3.1 `GameState::interest_groups`
// data layer. Deliberately the smallest deterministic shape that
// counts as a "reaction layer": each interest group's `loyalty`
// drifts linearly toward its country's `stability`, and its
// `radicalism` drifts linearly toward `1.0 - stability`. Both are
// clamped to `[0, 1]` after the step. Nothing else moves.
//
// Reaction rules (M3.2):
//
//   loyalty    += (country.stability         - loyalty)    * 0.05
//   radicalism += ((1.0 - country.stability) - radicalism) * 0.05
//
// `influence`, `kind`, `country`, `id_code`, and `name` are all
// UNCHANGED. M3.2 only updates the two ratios that future
// reaction-driven gameplay (events, command resistance, faction
// reactions, eventually coup / strike / civil-war risk) is most
// likely to consume.
//
// What this system DOES (M3.2):
//   * Preflight-validates every `group.country` against
//     `state.countries` BEFORE mutating any group. If any group
//     has an out-of-range or invalid country index, `react`
//     returns failure and no group's `loyalty` / `radicalism`
//     has been touched. Atomicity across the list is documented
//     and tested.
//   * Iterates `state.interest_groups` in vector order and
//     applies the two-line formula above.
//   * Returns a `ReactionOutcome` whose `groups_updated` matches
//     `state.interest_groups.size()` on success.
//   * Pure read of every input field; no logs, no RNG, no time
//     advancement, no event emission.
//
// What this system does NOT do (deliberate non-goals for M3.2):
//   * No `influence` drift. Influence is structural-political
//     weight; M3.2 doesn't model how it moves.
//   * No `corruption` / `government_authority` / `legitimacy`
//     input — only `country.stability` drives the drift.
//   * No per-`InterestGroupKind` rate. The 0.05 constant is the
//     same for every kind.
//   * No weighted multi-input formula.
//   * No country aggregate effect — interest groups react to the
//     country but do NOT push back on `country.stability` etc.
//     The reverse direction is reserved for M3.3+.
//   * No event triggers, no `state.logs` entry, no AI, no UI,
//     no command-system integration, no new
//     `PlayerCommandKind`, no scenario hook.
//   * No save schema change. The M3.1 v11 shape is sufficient.
//   * No coup, strike, protest, civil war, cross-border
//     behaviour.

#ifndef LEVIATHAN_SYSTEMS_INTEREST_GROUP_SYSTEM_HPP
#define LEVIATHAN_SYSTEMS_INTEREST_GROUP_SYSTEM_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::interest_group {

// Drift rate applied per monthly step. All `InterestGroupKind`
// variants share the same rate in M3.2; per-kind tuning is
// deferred until gameplay evidence asks for it.
inline constexpr double kInterestGroupReactionRate = 0.05;

struct ReactionOutcome {
    // Count of interest groups whose `loyalty` / `radicalism`
    // were updated. On a successful `react` call, equal to
    // `state.interest_groups.size()` (every group gets the
    // formula applied unconditionally; deltas may be zero when
    // already at the target). Zero when `state.interest_groups`
    // is empty.
    int groups_updated = 0;
};

// Apply one step of M3.2 interest-group reaction to every group
// in `state.interest_groups`.
//
// Preconditions:
//   * Every `group.country` in `state.interest_groups` must be
//     a valid index into `state.countries`. The function pre-
//     flight-validates the whole list BEFORE mutating any
//     group, so a single bad entry leaves every other group
//     untouched.
//
// On success the function:
//   * Drifts each group's `loyalty` toward
//     `state.countries[group.country].stability` at
//     `kInterestGroupReactionRate`.
//   * Drifts each group's `radicalism` toward
//     `1.0 - state.countries[group.country].stability` at
//     the same rate.
//   * Clamps both fields to `[0, 1]` after the step.
//   * Returns a `ReactionOutcome` with
//     `groups_updated == state.interest_groups.size()`.
//
// `react` is a pure read of every input field except the two
// it mutates. It does NOT touch `influence`, `state.logs`,
// `state.rng`, `state.current_date`, country state, faction
// state, policy state, or `state.applied_commands`.
core::Result<ReactionOutcome> react(core::GameState& state);

}  // namespace leviathan::systems::interest_group

#endif  // LEVIATHAN_SYSTEMS_INTEREST_GROUP_SYSTEM_HPP

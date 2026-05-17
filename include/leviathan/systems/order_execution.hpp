// OrderExecutionSystem skeleton (M2.17).
//
// First M2 system that reads the M2.16 `government_authority` block.
// Deliberately minimal: this PR ships the shape of a future order-
// execution evaluator without changing any existing behaviour. No
// caller wires the function in yet; `commands::apply_pending`
// continues to act exactly as in M2.5.
//
// What this skeleton DOES:
//   * Snapshots the actor country's `government_authority` ratios
//     into an `OrderExecutionInputs` POD so future formulas can be
//     written against a single, stable input bundle.
//   * Returns an `OrderExecutionOutcome` with `status == Accepted`.
//   * Fails loudly when `state.player_country` is unset or out of
//     range (mirrors the M2.3 `apply_pending` precondition shape).
//   * Is a pure read: never mutates `state`, never logs, never
//     touches the RNG.
//
// What this skeleton does NOT do (deliberate non-goals for M2.17):
//   * No `resistance` score. Adding a `0.0`-pinned field now would
//     pretend the formula shape is decided; it isn't. M2.18+
//     introduces both the formula and the field together.
//   * No `Rejected` / `Delayed` / `Distorted` status variants. The
//     enum is shipped with a single `Accepted` value so callers
//     can already `switch` on it and add cases additively.
//   * No effect on `commands::apply_pending`. The player command
//     queue path is byte-identical with M2.5 / M2.16.
//   * No new `PlayerCommandKind`. The function reads `command`
//     for future API stability but only uses the player-country
//     precondition for now.
//   * No AI, no events, no UI, no scheduler.
//
// Stable seam:
//   * `OrderExecutionInputs` only grows additively. New authority
//     ratios added to `GovernmentAuthorityState` should be mirrored
//     into the inputs in the same PR.
//   * `ExecutionStatus` only grows by adding enum variants.
//     Existing variants must not be renumbered or removed.
//   * `evaluate()` must stay a pure read of `state` and `command`.
//     A future PR may extend the outcome but should not introduce
//     state mutation or RNG dependence inside this function.

#ifndef LEVIATHAN_SYSTEMS_ORDER_EXECUTION_HPP
#define LEVIATHAN_SYSTEMS_ORDER_EXECUTION_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/player_commands.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::order_execution {

// Snapshot of the M2.16 `GovernmentAuthorityState` fields read out
// of the actor country. Captured into the outcome so callers can
// diagnose how a future formula would have read its inputs, even
// when the skeleton itself does nothing with them.
//
// Defaults match `GovernmentAuthorityState` (0.5 across the
// board) so a default-constructed outcome is a neutral one.
struct OrderExecutionInputs {
    double bureaucratic_compliance  = 0.5;
    double military_loyalty         = 0.5;
    double intelligence_capability  = 0.5;
    double media_control            = 0.5;
};

// Coarse-grained execution outcome class. M2.17 only produces
// `Accepted`; the other variants are reserved names for M2.18+:
//   * `Rejected` — execution failed outright; no state mutation.
//   * `Delayed`  — execution accepted but spread across multiple
//                  days; future scheduler decides exact timing.
//   * `Distorted` — execution accepted but with modified effects
//                   (e.g. partial application).
//
// Enum names match RFC-020 §2 categories. Adding a variant is
// always backwards compatible; renaming or removing one is a
// behaviour change a future PR must explicitly call out.
enum class ExecutionStatus {
    Accepted,
};

// Result of evaluating how a command would execute. M2.17 is a
// skeleton: every successful evaluation returns
// `status == Accepted` and `inputs` snapshotted from the actor
// country. Future PRs will replace the always-Accepted branch
// with a formula over `inputs` plus the command kind.
struct OrderExecutionOutcome {
    ExecutionStatus      status = ExecutionStatus::Accepted;
    OrderExecutionInputs inputs = {};
};

// Evaluate how `state` would execute `command`.
//
// Preconditions (mirror `commands::apply_pending`):
//   * `state.player_country` must be valid (>= 0).
//   * `state.player_country` must index into `state.countries`.
// Either precondition violation returns `Result::failure` whose
// error names `evaluate` and the offending state shape.
//
// On success the function:
//   * Snapshots the actor country's `government_authority` into
//     the returned `OrderExecutionInputs`.
//   * Returns `ExecutionStatus::Accepted`.
//   * Leaves `state` and `command` byte-identical.
//
// The function inspects `command` only to honour the future API
// shape — M2.17 produces the same outcome for every kind. M2.18+
// will branch on `command.kind` and the actor's command-specific
// resistance terms.
core::Result<OrderExecutionOutcome> evaluate(
    const core::GameState& state,
    const core::PlayerCommand& command);

}  // namespace leviathan::systems::order_execution

#endif  // LEVIATHAN_SYSTEMS_ORDER_EXECUTION_HPP

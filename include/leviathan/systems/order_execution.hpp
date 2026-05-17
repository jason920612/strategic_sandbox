// OrderExecutionSystem (M2.17 skeleton + M2.18 EnactPolicy gate).
//
// First M2 system that reads the M2.16 `government_authority` block.
// M2.17 shipped the API surface (types + a pure-read `evaluate()`
// that always returned `Accepted`); M2.18 turns the surface into
// a real gate for `PlayerCommandKind::EnactPolicy`:
//
//   * The first deterministic formula lives here. `EnactPolicy`
//     command evaluation now compares the actor country's
//     `bureaucratic_compliance` against
//     `kEnactPolicyComplianceThreshold` (0.3). Strictly below ⇒
//     `Rejected`; equal-or-above ⇒ `Accepted`.
//   * `OrderExecutionOutcome` gains a `resistance` field
//     (`1.0 - bureaucratic_compliance` for `EnactPolicy`). It is
//     informational; no system other than the gate itself reads it.
//     `AdjustBudget` and other kinds leave `resistance` at the
//     default 0.0 because no gate is evaluated for them yet.
//   * `ExecutionStatus` gains a `Rejected` variant.
//   * `commands::apply_pending` calls `evaluate` BEFORE the M2.3
//     policy lookup for `EnactPolicy` and returns `Result::failure`
//     when the outcome is `Rejected`. The failed command stays at
//     the head of the queue and is NOT appended to
//     `state.applied_commands` — mirrors M2.3 mid-list failure
//     atomicity.
//
// What this system DOES (M2.17 + M2.18):
//   * Snapshots the actor country's `government_authority` ratios
//     into `OrderExecutionInputs`.
//   * Computes a per-command-kind resistance + status. M2.18 only
//     evaluates `EnactPolicy`; every other kind is unconditionally
//     `Accepted` with `resistance = 0.0`.
//   * Fails loudly when `state.player_country` is unset or out of
//     range (mirrors the M2.3 `apply_pending` precondition shape).
//   * Is a pure read: never mutates `state`, never logs, never
//     touches the RNG.
//
// What this system does NOT do (deliberate non-goals for M2.18):
//   * No `Delayed` / `Distorted` status variants. M2.18 ships only
//     `Accepted` and `Rejected`; the other RFC-020 §2 cases stay
//     reserved by name in the enum comment.
//   * No `AdjustBudget` gate. `evaluate` returns `Accepted` for
//     `AdjustBudget` regardless of `bureaucratic_compliance`.
//   * No multi-input or weighted formula. The composite shape
//     waits for actual gameplay evidence about which terms matter.
//   * No probabilistic / RNG-based gate. The threshold check is
//     fully deterministic.
//   * No scheduler / pending-execution queue.
//   * No automatic logging of rejected commands. `state.logs` is
//     untouched.
//   * No AI, no events, no UI.
//
// Stable seam (still applies):
//   * `OrderExecutionInputs` grows additively.
//   * `ExecutionStatus` grows by adding variants only.
//   * `evaluate()` stays a pure read of `state` and `command`.
//     A future PR that adds RNG / mutation should rename, not
//     overload this function.

#ifndef LEVIATHAN_SYSTEMS_ORDER_EXECUTION_HPP
#define LEVIATHAN_SYSTEMS_ORDER_EXECUTION_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/player_commands.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::order_execution {

// M2.18 threshold for the `EnactPolicy` gate. A country's
// `bureaucratic_compliance` strictly below this value rejects the
// command; equal-or-above accepts. Picked at 0.3 so canonical
// default-0.5 countries always accept (no behaviour change for
// existing scenarios) while leaving room to author low-compliance
// countries that visibly stall.
inline constexpr double kEnactPolicyComplianceThreshold = 0.3;

// Snapshot of the M2.16 `GovernmentAuthorityState` fields read out
// of the actor country. Captured into the outcome so callers can
// diagnose how the formula read its inputs and so future per-kind
// gates can reuse the same bundle.
//
// Defaults match `GovernmentAuthorityState` (0.5 across the
// board) so a default-constructed outcome is a neutral one.
struct OrderExecutionInputs {
    double bureaucratic_compliance  = 0.5;
    double military_loyalty         = 0.5;
    double intelligence_capability  = 0.5;
    double media_control            = 0.5;
};

// Coarse-grained execution outcome class. M2.18 produces
// `Accepted` or `Rejected`. The other RFC-020 §2 categories stay
// reserved by name so a future PR can introduce them as additive
// enum variants without churn:
//   * `Delayed`  — execution accepted but spread across multiple
//                  days; future scheduler decides exact timing.
//   * `Distorted` — execution accepted but with modified effects
//                   (e.g. partial application).
//
// Adding a variant is always backwards compatible; renaming or
// removing one is a behaviour change a future PR must explicitly
// call out.
enum class ExecutionStatus {
    Accepted,
    Rejected,
    // Future: Delayed, Distorted
};

// Result of evaluating how a command would execute. M2.18:
//   * `status` is `Rejected` only for `EnactPolicy` with
//     `inputs.bureaucratic_compliance < kEnactPolicyComplianceThreshold`.
//     Every other (command kind, inputs) pair returns `Accepted`.
//   * `resistance` is `1.0 - bureaucratic_compliance` for
//     `EnactPolicy` (informational; equals the distance from
//     full compliance). For `AdjustBudget` and any future kind
//     that doesn't yet have a gate, `resistance` stays at the
//     default `0.0` — "no gate was evaluated".
//   * `inputs` is always populated with the actor country's
//     authority snapshot regardless of kind.
//
// `resistance` is informational. M2.18 doesn't consume it
// anywhere downstream; future diagnostics / logging PRs may
// surface it but should not change its computation rule.
struct OrderExecutionOutcome {
    ExecutionStatus      status     = ExecutionStatus::Accepted;
    OrderExecutionInputs inputs     = {};
    double               resistance = 0.0;
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
//     `OrderExecutionInputs`.
//   * For `PlayerCommandKind::EnactPolicy`:
//       resistance = 1.0 - bureaucratic_compliance
//       status     = (bureaucratic_compliance >=
//                     kEnactPolicyComplianceThreshold)
//                    ? Accepted : Rejected
//   * For every other kind:
//       resistance = 0.0
//       status     = Accepted
//   * Leaves `state` and `command` byte-identical.
//
// `commands::apply_pending` calls this function before the M2.3
// policy lookup for every `EnactPolicy` command. A `Rejected`
// outcome short-circuits the call: no state mutation, no queue
// pop, no `applied_commands` entry — same atomicity rule M2.3
// applies to mid-list policy-system failures.
core::Result<OrderExecutionOutcome> evaluate(
    const core::GameState& state,
    const core::PlayerCommand& command);

}  // namespace leviathan::systems::order_execution

#endif  // LEVIATHAN_SYSTEMS_ORDER_EXECUTION_HPP

// OrderExecutionSystem (M2.17 skeleton + M2.18 EnactPolicy gate +
//                       M2.19 AdjustBudget gate).
//
// First M2 system that reads the M2.16 `government_authority` block.
// M2.17 shipped the API surface; M2.18 turned the `EnactPolicy`
// branch into a real gate; M2.19 extends the gate to `AdjustBudget`
// with a category-aware single-input formula.
//
//   * M2.18 — `EnactPolicy` arm: compares the actor country's
//     `bureaucratic_compliance` against
//     `kEnactPolicyComplianceThreshold` (0.3). Strictly below ⇒
//     `Rejected`; equal-or-above ⇒ `Accepted`. `resistance` is
//     `1.0 - bureaucratic_compliance`.
//   * M2.19 — `AdjustBudget` arm: selects ONE authority input
//     based on `command.budget_category`:
//       - `"military"` ⇒ `military_loyalty`
//       - every other category ⇒ `bureaucratic_compliance`
//     Then compares the selected value against
//     `kAdjustBudgetComplianceThreshold` (0.3) with the same
//     `>= threshold` rule. `resistance` is `1.0 - selected`.
//     The military branch captures the gameplay intuition that
//     the armed forces resist a low-loyalty regime trying to
//     reshape the military budget, while non-military categories
//     still flow through normal bureaucracy.
//
// Across both arms `commands::apply_pending` calls `evaluate`
// BEFORE the existing per-kind validation (policy lookup for
// `EnactPolicy`, category whitelist + delta finiteness for
// `AdjustBudget`). A `Rejected` outcome returns `Result::failure`
// from `apply_pending`; the rejected command stays at the head
// of the queue and is NOT appended to `state.applied_commands`,
// matching the M2.3 / M2.4 mid-list-failure atomicity rule.
//
// What this system DOES (M2.17 + M2.18 + M2.19):
//   * Snapshots the actor country's `government_authority` ratios
//     into `OrderExecutionInputs`.
//   * Computes a per-command-kind status + resistance:
//       - `EnactPolicy`: bureaucratic gate (M2.18).
//       - `AdjustBudget`: category-aware gate (M2.19).
//       - Any future kind without an explicit arm: keeps the
//         M2.17 defaults (`Accepted`, `resistance = 0.0`).
//   * Fails loudly when `state.player_country` is unset or out of
//     range (mirrors the M2.3 `apply_pending` precondition shape).
//   * Is a pure read: never mutates `state`, never logs, never
//     touches the RNG.
//
// What this system does NOT do (deliberate non-goals for M2.19):
//   * No `Delayed` / `Distorted` status variants. M2.19 still
//     ships only `Accepted` and `Rejected`; the other RFC-020 §2
//     cases stay reserved by name in the enum comment.
//   * No multi-input or weighted formula. M2.19 keeps the per-
//     kind / per-category formula to a single input; weighted
//     composites wait for actual gameplay evidence.
//   * No probabilistic / RNG-based gate.
//   * No scheduler / pending-execution queue.
//   * No category-specific selection beyond
//     `"military"` ⇒ `military_loyalty`. The other six budget
//     categories (administration / education / welfare /
//     intelligence / infrastructure / industry) all share the
//     `bureaucratic_compliance` path — adding bespoke per-
//     category inputs (e.g. `intelligence` ⇒
//     `intelligence_capability`) waits for its own PR.
//   * No automatic logging of rejected commands. `state.logs`
//     remains untouched.
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

// M2.19 threshold for the `AdjustBudget` gate. Compared against
// the category-selected input (`military_loyalty` for the
// `"military"` budget category, `bureaucratic_compliance` for
// every other category). Same `0.3` floor as the `EnactPolicy`
// gate so canonical default-0.5 authority countries continue to
// accept budget changes unchanged.
inline constexpr double kAdjustBudgetComplianceThreshold = 0.3;

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

// Result of evaluating how a command would execute. M2.19:
//   * `status` is `Rejected` when:
//       - `EnactPolicy` with
//         `bureaucratic_compliance < kEnactPolicyComplianceThreshold`,
//         OR
//       - `AdjustBudget` with the category-selected input below
//         `kAdjustBudgetComplianceThreshold` — the selection is
//         `military_loyalty` for `budget_category == "military"`
//         and `bureaucratic_compliance` for every other category.
//     Every other (command kind, inputs) pair returns `Accepted`.
//   * `resistance` is `1.0 - selected_input`:
//       - For `EnactPolicy`: selected = `bureaucratic_compliance`.
//       - For `AdjustBudget` military: selected = `military_loyalty`.
//       - For `AdjustBudget` non-military: selected =
//         `bureaucratic_compliance`.
//     For any future kind that doesn't yet have a gate,
//     `resistance` stays at the default `0.0` — "no gate was
//     evaluated".
//   * `inputs` is always populated with the actor country's
//     authority snapshot regardless of kind.
//
// `resistance` is informational. M2.19 doesn't consume it
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
//   * For `PlayerCommandKind::EnactPolicy` (M2.18):
//       resistance = 1.0 - bureaucratic_compliance
//       status     = (bureaucratic_compliance >=
//                     kEnactPolicyComplianceThreshold)
//                    ? Accepted : Rejected
//   * For `PlayerCommandKind::AdjustBudget` (M2.19):
//       selected   = (command.budget_category == "military")
//                    ? military_loyalty
//                    : bureaucratic_compliance
//       resistance = 1.0 - selected
//       status     = (selected >= kAdjustBudgetComplianceThreshold)
//                    ? Accepted : Rejected
//   * For every other (future) kind:
//       resistance = 0.0
//       status     = Accepted
//   * Leaves `state` and `command` byte-identical.
//
// `commands::apply_pending` calls this function before the M2.3
// per-kind validation for every `EnactPolicy` and `AdjustBudget`
// command. A `Rejected` outcome short-circuits the call: no
// state mutation, no queue pop, no `applied_commands` entry —
// same atomicity rule M2.3 applies to mid-list per-kind failures.
core::Result<OrderExecutionOutcome> evaluate(
    const core::GameState& state,
    const core::PlayerCommand& command);

}  // namespace leviathan::systems::order_execution

#endif  // LEVIATHAN_SYSTEMS_ORDER_EXECUTION_HPP

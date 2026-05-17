// Player command queue + dispatch (M2.3).
//
// The M2 player-operation prototype submits commands from an outer
// driver (CLI script today, eventually an interactive UI). M2.3
// ships the smallest useful surface: a runtime `CommandQueue`, an
// `ApplyOutcome` struct, and a free function that drains the queue
// by dispatching each entry through the appropriate M1 system.
//
// Design rules:
//   * `CommandQueue` is RUNTIME-ONLY. It lives outside `GameState`
//     and `TickController`; the driver owns the queue. M2.3 does NOT
//     bump the save format -- M2.4 will introduce a separate
//     persistent command log if replay is needed.
//   * Atomicity is per-command, NOT per-queue. On the first command
//     that fails, `apply_pending` stops; previously-applied commands
//     stay applied, and the failed command (plus any remaining)
//     stays at the head of the queue. This matches M1.13's documented
//     mid-list-failure semantics.
//   * `apply_pending` requires `state.player_country` to be valid AND
//     to index into `state.countries`. Without a player selection
//     there is no actor; the function rejects loudly.

#ifndef LEVIATHAN_SYSTEMS_COMMANDS_HPP
#define LEVIATHAN_SYSTEMS_COMMANDS_HPP

#include <optional>
#include <string>
#include <vector>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/player_commands.hpp"
#include "leviathan/core/result.hpp"
#include "leviathan/systems/runner.hpp"

namespace leviathan::systems::commands {

// Runtime container holding player commands waiting to be applied.
// Owned by an outer driver; not part of GameState and not part of
// `runner::TickController`. Plain data: append with
// `q.pending.push_back(...)`, inspect with `q.pending.size()`, etc.
struct CommandQueue {
    std::vector<core::PlayerCommand> pending;
};

struct ApplyOutcome {
    // Count of commands that were applied and popped from `pending`.
    int commands_applied = 0;
};

// Drain `q.pending` by dispatching each command through the
// appropriate M1 system. Commands are processed in insertion order:
//
//   - `EnactPolicy`: looks up `policy_id_code` in `state.policies`
//     and calls `policy::apply_policy_effects(state, player_country,
//     policy)`. The M1.5 atomicity rule applies per-command (a
//     rejected policy leaves state unchanged for THAT command).
//
// Atomicity is per-command, not per-queue. The first failure stops
// processing; previously-applied commands stay applied, the failed
// command remains at the head of the queue, and any subsequent
// commands stay queued. Callers can fix state and retry.
//
// Precondition: `state.player_country` must be valid AND index into
// `state.countries`. Otherwise the function rejects without touching
// the queue.
core::Result<ApplyOutcome> apply_pending(core::GameState& state,
                                         CommandQueue& q);

// ===========================================================================
// M2.20 - structured rejection reporting.
//
// `apply_pending` returns `Result::failure` when an order-execution
// gate rejects a command (M2.18 EnactPolicy, M2.19 AdjustBudget).
// That keeps the M2.3 / M2.4 mid-list-failure semantics intact but
// only delivers the rejection details as a free-form error string.
// `try_apply_pending` is the narrow alternative for callers that
// want to inspect a rejection programmatically:
//
//   * Successful drain ⇒ `Result::success` with
//     `rejection == std::nullopt`, identical to the existing
//     `apply_pending` happy path.
//   * Order-execution rejection ⇒ `Result::success` with
//     `rejection` populated. `state`, the queue, and
//     `applied_commands` follow the same atomicity rule as
//     `apply_pending`: rejected command stays at the queue head,
//     no mutation, no log entry.
//   * Non-execution failure (precondition violation, NaN
//     `budget_delta`, unknown policy id_code, unknown
//     budget_category) ⇒ `Result::failure`, same shape as
//     `apply_pending` so this helper never silently swallows
//     real validation errors.
//
// `apply_pending` is untouched: existing callers (including
// `commands::replay_with_time` and every M2.18 / M2.19 test) keep
// seeing `Result::failure` on rejection. Future PRs that want
// per-command audit can adopt `try_apply_pending` opt-in.
// ===========================================================================

// Structured snapshot of an order-execution gate rejection.
// Mirrors the inputs the gate read so callers can format their
// own message, compare against expectations in tests, or surface
// the gate decision in a UI / log without parsing the error
// string `apply_pending` produces.
struct RejectionRecord {
    // Kind of the rejected player command (EnactPolicy /
    // AdjustBudget). Identifies which of the two id-bearing
    // fields below is meaningful.
    core::PlayerCommandKind kind{};

    // For `EnactPolicy` rejections: the policy_id_code of the
    // rejected command. Empty otherwise.
    std::string policy_id_code;

    // For `AdjustBudget` rejections: the budget_category of the
    // rejected command. Empty otherwise.
    std::string budget_category;

    // The authority input the gate evaluated. M2.18 EnactPolicy
    // gates always read `bureaucratic_compliance`; M2.19
    // AdjustBudget reads `military_loyalty` for the `"military"`
    // category and `bureaucratic_compliance` for every other
    // category. This field carries the selected value as observed
    // at evaluation time.
    double compliance = 0.0;

    // The threshold the gate compared `compliance` against. For
    // M2.18 / M2.19 this is `kEnactPolicyComplianceThreshold` or
    // `kAdjustBudgetComplianceThreshold` respectively; both
    // currently 0.3 but the record records whatever was active.
    double threshold = 0.0;

    // `1.0 - compliance`. Surfaced for diagnostics so a UI / log
    // can present the rejection in terms of resistance instead of
    // compliance without recomputing.
    double resistance = 0.0;
};

// Outcome shape for `try_apply_pending`. Wraps the existing
// `ApplyOutcome` (so `commands_applied` retains the same meaning
// as in `apply_pending`) and adds an optional `RejectionRecord`
// describing where the drain stopped if it stopped at a gate.
//
// Invariants on a successful return:
//   * `rejection == std::nullopt` ⇒ the queue drained fully,
//     `apply.commands_applied == initial pending.size()`.
//   * `rejection.has_value()` ⇒ the drain stopped at the head
//     command; `apply.commands_applied` counts the strictly
//     prior successful commands. The queue still contains the
//     rejected command at the head plus any commands behind it.
struct ApplyWithReportOutcome {
    ApplyOutcome                   apply;
    std::optional<RejectionRecord> rejection;
};

// Drain `q.pending` like `apply_pending` does — same per-command
// atomicity, same precondition shape, same effect on `state` and
// `state.applied_commands` — but surface order-execution
// rejections as a `Result::success` carrying an
// `ApplyWithReportOutcome` whose `rejection` field is populated.
//
// Failure shape:
//   * Precondition (`state.player_country` invalid / out of
//     range) violated.
//   * Non-execution per-command failure (`AdjustBudget` with
//     non-finite delta or unknown category; `EnactPolicy` with
//     unknown id_code or a policy effect that fails to resolve).
//
// In every other case the helper returns `Result::success`,
// possibly with a rejection.
core::Result<ApplyWithReportOutcome> try_apply_pending(
    core::GameState& state, CommandQueue& q);

// ===========================================================================
// M2.6: replay an applied-command log into a target state.
// ===========================================================================

struct ReplayOutcome {
    // Count of log entries that were re-applied and re-logged in the
    // target state. On success, equals `log.size()`.
    int commands_replayed = 0;
};

// Re-apply a recorded command log into `target_state`. For each entry:
//   1. Set `target_state.current_date = entry.applied_on` so the
//      command's recorded date is used (not the wall-clock state).
//   2. Build a 1-element `CommandQueue` containing `entry.command`.
//   3. Call `apply_pending` (so M2.3 dispatch + M2.4 log append +
//      M1.5/M1.15 effects all run unchanged).
//
// On success, `target_state.applied_commands` mirrors `log` byte-for-
// byte: each replayed command appends one log entry with the same
// `applied_on` (forced) and the same payload (passed through).
//
// Preconditions:
//   - `target_state.player_country` must be valid AND index into
//     `target_state.countries` (same as `apply_pending`).
//   - `target_state.applied_commands` must be empty on entry. Replay
//     would otherwise mix new entries with prior ones, defeating the
//     "log mirrors source" guarantee. Callers that load a save and
//     want to replay should build a FRESH state (e.g. from the
//     scenario loader), not reload the save.
//
// Failure cases:
//   - Either precondition violated.
//   - Any per-entry `apply_pending` call fails (unknown policy /
//     bad budget category / etc.). The entry's index is included
//     in the error. Entries before the failed one stay applied and
//     logged in `target_state`; the failed entry's mutation is
//     skipped per M2.3 per-command atomicity; entries after it are
//     NOT replayed.
//
// LIMITATIONS (M2.6 is a prototype):
//   - Time-system advancement between commands is NOT performed.
//     `target_state.current_date` is forced to each entry's
//     `applied_on` and ends at the LAST entry's date — it does not
//     advance to match the source state's `current_date` past that
//     point. The simulation systems (monthly pipeline, faction
//     react, stability tick, economy tick) are NOT ticked during
//     replay.
//   - Scenario state (countries, factions, policies) must be loaded
//     into `target_state` by the caller BEFORE replay. Replay does
//     not reload scenarios.
//   - The function does not verify that the replayed final state
//     matches any reference state. Caller compares fields directly.
core::Result<ReplayOutcome> replay(
    core::GameState& target_state,
    const std::vector<core::AppliedPlayerCommand>& log);

// ===========================================================================
// M2.7: replay with time-system advancement.
//
// Like M2.6 `replay`, but advances the simulation day-by-day between
// commands using M2.2 `step_one_day`. The M1.10 monthly pipeline
// therefore runs naturally on every month boundary that lies between
// two consecutive log entries. After this function returns,
// `target_state.current_date == log.back().applied_on` (or
// unchanged for an empty log).
//
// Caller pattern:
//
//   runner::TickController ctrl;
//   runner::begin_tick(state, opts, ctrl);
//   auto r = commands::replay_with_time(state, opts, ctrl, log);
//   if (!r) { ... }
//   // (optional: caller can call step_one_day further to advance past
//   // the last log entry, then end_tick for save + CSV writes.)
//   runner::end_tick(state, opts, ctrl);
//
// Preconditions (rejected loudly with state untouched on first three):
//   - target_state.player_country valid + indexes into countries.
//   - target_state.applied_commands empty (same as M2.6 `replay`).
//   - ctrl.started && !ctrl.ended (caller must have begun the tick).
//   - For each i > 0: log[i].applied_on >= log[i-1].applied_on
//     (monotonic non-decreasing dates; addresses PR #34 nit). The
//     check is per-entry against current state.current_date, so the
//     first out-of-order entry fails with its index in the error.
//
// Failure cases:
//   - Any precondition violated above.
//   - Any step_one_day failure (M1.10 monthly pipeline can fail).
//     Prior commands in the log are already applied + logged; the
//     advance toward the next command is interrupted; the failed
//     entry is NOT applied.
//   - Any apply_pending failure (same shape as M2.6: entry's index
//     in the error, prior entries stay applied + logged).
//
// LIMITATIONS (still a prototype):
//   - After return, target_state.current_date == log.back().applied_on,
//     NOT necessarily the source state's actual final current_date.
//     If you need to advance further, call step_one_day in a loop
//     before end_tick.
//   - The function does not verify the replayed final state matches
//     any reference. Caller compares fields directly.
core::Result<ReplayOutcome> replay_with_time(
    core::GameState& state,
    const runner::RunnerOptions& opts,
    runner::TickController& ctrl,
    const std::vector<core::AppliedPlayerCommand>& log);

}  // namespace leviathan::systems::commands

#endif  // LEVIATHAN_SYSTEMS_COMMANDS_HPP

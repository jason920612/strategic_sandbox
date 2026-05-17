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
// M2.21 - scripted-driver helper.
//
// `apply_command_script` is a thin wrapper over
// `try_apply_pending` for callers that already have a one-shot
// list of commands (a "script") rather than a long-lived queue.
// Internally it copies `script` into a local `CommandQueue` and
// dispatches through the same per-command pipeline; semantics
// inherit directly from `try_apply_pending`:
//
//   * Empty `script` -> Result::success with
//     `apply.commands_applied == 0` and
//     `rejection == std::nullopt`.
//   * Full drain -> Result::success with
//     `apply.commands_applied == script.size()` and
//     `rejection == std::nullopt`.
//   * Order-execution gate rejection -> Result::success with
//     `apply.commands_applied` counting the strictly prior
//     successes and `rejection` populated for the first
//     rejected command. The rejected command and any commands
//     behind it are NOT surfaced through the return value —
//     M2.21 deliberately keeps the API minimal. Callers that
//     need access to the trailing tail should build a
//     `CommandQueue` directly and call `try_apply_pending`.
//   * Non-execution failure (precondition / NaN delta /
//     unknown policy id_code / unknown budget_category /
//     policy-effect resolution failure) -> Result::failure
//     with the same message shape `try_apply_pending` produces.
//
// The input `script` vector is NOT mutated: the helper copies
// elements into the local queue and drains the queue, not the
// caller's vector. `state` and `state.applied_commands` follow
// the same atomicity rule as `apply_pending` / `try_apply_pending`:
// only successful commands mutate state and append to the log.
core::Result<ApplyWithReportOutcome> apply_command_script(
    core::GameState& state,
    const std::vector<core::PlayerCommand>& script);

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

// ===========================================================================
// M4.1 - Command gate diagnostics surface.
//
// Read-only helpers that explain how the existing M2.18 / M2.19
// command-execution gates would evaluate on a country in its
// CURRENT `government_authority` state. They do NOT mutate state,
// do NOT enqueue or dispatch commands, do NOT change the gate
// formula. They exist so future work (UI / command feedback /
// AI / structured logs) can read the gate seam without going
// through `apply_pending` and without reverse-engineering the
// formula.
//
// The diagnostic is purely about the authority gate. It deliberately
// does NOT:
//   * verify that the policy exists in `state.policies`
//     (`diagnose_enact_policy_gate` ignores the policy lookup —
//     that's `apply_pending`'s downstream concern);
//   * verify that the budget category is one of the seven valid
//     ones (`diagnose_adjust_budget_gate` only branches on the
//     `"military"` keyword; everything else uses bureaucratic
//     compliance, matching `order_execution::evaluate`);
//   * branch on `state.player_country` (the diagnostic takes a
//     CountryId argument directly so a caller can ask "what
//     would the gate decide for country X right now?" without
//     temporarily flipping the player selection).
//
// The threshold values + the field-selection rule come from
// `order_execution::evaluate` exactly. Tests pin "diagnostic
// agrees with `apply_pending`" on representative cases so the
// helper can't silently drift from real gate behaviour.
// ===========================================================================

// Identifies which gate produced the diagnostic. Mirrors
// `core::PlayerCommandKind` for the two kinds that currently have
// a gate (M2.18 / M2.19); kept as a separate enum so M4.X work can
// add gate-only diagnostics for surfaces that have no matching
// `PlayerCommandKind` (e.g. a hypothetical
// "diagnose this hypothetical command kind" helper) without
// dragging the `PlayerCommandKind` enum around.
enum class CommandGateKind {
    EnactPolicy,
    AdjustBudget,
};

// Structured snapshot of a command gate's decision on a country.
// Carries every input the gate read so a caller can format its own
// message / surface the decision in UI / compare against
// expectations in a test without recomputing.
struct CommandGateDiagnostic {
    CommandGateKind gate{};

    // The country the gate was evaluated for.
    core::CountryId country{};

    // The country's `id_code`. Denormalised so callers don't have
    // to re-index `state.countries`.
    std::string country_id_code;

    // Human-readable descriptor of what the command would touch.
    // For `EnactPolicy`: `"policy:<policy_id_code>"`. For
    // `AdjustBudget`: `"budget:<category>"`. The diagnostic does
    // NOT validate that the policy / category exists; this string
    // is for display.
    std::string target;

    // Name of the authority sub-field this gate read. One of:
    //   * `"bureaucratic_compliance"`
    //   * `"military_loyalty"`
    // Selected per `order_execution::evaluate`: `EnactPolicy`
    // always reads bureaucratic_compliance; `AdjustBudget` reads
    // military_loyalty iff `budget_category == "military"`,
    // otherwise bureaucratic_compliance.
    std::string authority_field;

    // The authority value observed at evaluation time. Range
    // `[0, 1]` if upstream invariants hold; the diagnostic does
    // not re-validate.
    double authority_value = 0.0;

    // The threshold the gate compares `authority_value` against.
    // Currently `0.3` from
    // `order_execution::kEnactPolicyComplianceThreshold` /
    // `kAdjustBudgetComplianceThreshold`. The two thresholds
    // happen to share a value today but are sourced separately;
    // a future PR that diverges them will flow through naturally.
    double threshold = 0.0;

    // `authority_value >= threshold` per the M2.18 / M2.19
    // accept rule. The diagnostic does not branch on `>` vs
    // `>=` — it matches the existing gate.
    bool allowed = false;
};

// Explain how the `EnactPolicy` gate would evaluate on `country`
// in its current authority state. `policy_id_code` is echoed back
// into `target` but is NOT looked up in `state.policies` — the
// diagnostic is gate-only.
//
// Failure cases:
//   * `country` is not a valid index into `state.countries`. The
//     diagnostic does NOT silently fall back to a default — a
//     bad CountryId is a caller error.
//
// On success the diagnostic carries the full gate decision; the
// caller decides what to do with it (display, log, compare,
// branch downstream work).
core::Result<CommandGateDiagnostic> diagnose_enact_policy_gate(
    const core::GameState& state,
    core::CountryId country,
    const std::string& policy_id_code);

// Explain how the `AdjustBudget` gate would evaluate on `country`
// for `budget_category`. Field selection mirrors
// `order_execution::evaluate` exactly: `"military"` selects
// `military_loyalty`, every other string selects
// `bureaucratic_compliance`. The diagnostic does NOT validate
// that `budget_category` is one of the seven canonical
// `BudgetState` fields — that's `apply_pending`'s downstream
// whitelist check, not the gate's.
//
// Failure cases:
//   * `country` is not a valid index into `state.countries`.
core::Result<CommandGateDiagnostic> diagnose_adjust_budget_gate(
    const core::GameState& state,
    core::CountryId country,
    const std::string& budget_category);

}  // namespace leviathan::systems::commands

#endif  // LEVIATHAN_SYSTEMS_COMMANDS_HPP

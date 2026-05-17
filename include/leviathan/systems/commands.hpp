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

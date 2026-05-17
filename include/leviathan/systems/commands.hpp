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

}  // namespace leviathan::systems::commands

#endif  // LEVIATHAN_SYSTEMS_COMMANDS_HPP

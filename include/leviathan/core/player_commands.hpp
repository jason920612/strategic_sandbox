// Player command type for the M2 player-operation prototype.
//
// M2.3 introduces a single-kind command (`EnactPolicy`) that an
// outer driver can submit between `runner::step_one_day` calls.
// The runtime queue, the dispatch dispatch, and the integration with
// the existing M1 systems live in `leviathan::systems::commands`
// (see `systems/commands.hpp`).
//
// This header carries only the data type. Pure POD; no behaviour.
// Other M2.x sub-milestones will grow `PlayerCommandKind` with
// additional variants (adjust budget, change tax burden, etc.).

#ifndef LEVIATHAN_CORE_PLAYER_COMMANDS_HPP
#define LEVIATHAN_CORE_PLAYER_COMMANDS_HPP

#include <string>

namespace leviathan::core {

enum class PlayerCommandKind {
    // Enact a policy on the currently-selected player country.
    // The policy must already exist in `state.policies` (loaded by
    // the scenario loader or hand-built in tests). Effects are
    // applied through `systems::policy::apply_policy_effects`, so
    // M1.5 atomicity, M1.15 active_policies tracking, and the
    // M1.15 `kMaxTrackedPolicyDurationDays` cap all apply.
    EnactPolicy,
};

struct PlayerCommand {
    PlayerCommandKind kind = PlayerCommandKind::EnactPolicy;

    // EnactPolicy payload: the policy's `id_code`. Unused for other
    // kinds; future variants may carry additional fields.
    std::string policy_id_code;
};

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_PLAYER_COMMANDS_HPP

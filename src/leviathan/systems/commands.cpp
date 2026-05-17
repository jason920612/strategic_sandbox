#include "leviathan/systems/commands.hpp"

#include <cstddef>
#include <string>
#include <utility>

#include "leviathan/core/entities.hpp"
#include "leviathan/systems/policy_system.hpp"

namespace leviathan::systems::commands {

namespace {

// Locate a policy by its on-disk identifier. Returns nullptr if no
// loaded policy matches. id_code is stable across loads; numeric
// PolicyId is not (assigned by scenario-loader index), so we look up
// by string here.
const core::PolicyData* find_policy_by_id_code(
        const core::GameState& state, const std::string& id_code) {
    for (const auto& p : state.policies) {
        if (p.id_code == id_code) return &p;
    }
    return nullptr;
}

}  // namespace

core::Result<ApplyOutcome> apply_pending(core::GameState& state,
                                         CommandQueue& q) {
    namespace pol = leviathan::systems::policy;

    // ---- Precondition: a valid player_country is mandatory. -----
    if (!state.player_country.valid() ||
        state.player_country.value() < 0 ||
        static_cast<std::size_t>(state.player_country.value()) >=
            state.countries.size()) {
        return core::Result<ApplyOutcome>::failure(
            "commands::apply_pending: state.player_country is not a"
            " valid index into state.countries (call run() with"
            " --player COUNTRY_IDCODE before submitting commands)");
    }

    ApplyOutcome outcome;

    // ---- Drain the queue. Stop at first failure. -----------------
    while (!q.pending.empty()) {
        const core::PlayerCommand cmd = q.pending.front();
        const std::string ctx =
            "commands::apply_pending[" +
            std::to_string(outcome.commands_applied) + "]";

        switch (cmd.kind) {
            case core::PlayerCommandKind::EnactPolicy: {
                const core::PolicyData* p =
                    find_policy_by_id_code(state, cmd.policy_id_code);
                if (p == nullptr) {
                    return core::Result<ApplyOutcome>::failure(
                        ctx + ": EnactPolicy unknown policy id_code '" +
                        cmd.policy_id_code + "'");
                }
                auto r = pol::apply_policy_effects(state,
                                                   state.player_country,
                                                   *p);
                if (!r) {
                    return core::Result<ApplyOutcome>::failure(
                        ctx + ": EnactPolicy '" + cmd.policy_id_code +
                        "': " + std::move(r.error()));
                }
                break;
            }
        }

        q.pending.erase(q.pending.begin());
        ++outcome.commands_applied;
    }

    return core::Result<ApplyOutcome>::success(std::move(outcome));
}

}  // namespace leviathan::systems::commands

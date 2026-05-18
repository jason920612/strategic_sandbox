#include "leviathan/systems/ai_policy.hpp"

#include <cstddef>

#include "leviathan/systems/policy_system.hpp"

namespace leviathan::systems::ai_policy {

core::Result<std::vector<Selection>>
select_policies(const core::GameState& state) {
    std::vector<Selection> out;

    if (state.countries.empty() || state.policies.empty()) {
        return core::Result<std::vector<Selection>>::success(std::move(out));
    }

    const std::string& chosen_policy_id_code = state.policies.front().id_code;
    const core::CountryId player              = state.player_country;
    const bool player_valid = player.valid()
        && static_cast<std::size_t>(player.value()) < state.countries.size();

    out.reserve(state.countries.size());
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const core::CountryId cid{static_cast<core::CountryId::underlying_type>(i)};
        if (player_valid && cid == player) {
            continue;
        }
        out.push_back(Selection{cid, chosen_policy_id_code});
    }

    return core::Result<std::vector<Selection>>::success(std::move(out));
}

core::Result<ApplyOutcome>
apply_selected_policies(core::GameState& state) {
    ApplyOutcome outcome;

    auto sel_r = select_policies(state);
    if (!sel_r) {
        return core::Result<ApplyOutcome>::failure(
            std::move(sel_r.error()));
    }
    const auto& selections = sel_r.value();
    outcome.considered = selections.size();
    if (selections.empty()) {
        return core::Result<ApplyOutcome>::success(std::move(outcome));
    }

    // Build an id_code -> policy index map for O(N+M) instead of
    // O(N*M) lookup across selections. Stable since state.policies
    // is the same object across the call.
    for (const auto& sel : selections) {
        const core::PolicyData* policy = nullptr;
        for (const auto& p : state.policies) {
            if (p.id_code == sel.policy_id_code) {
                policy = &p;
                break;
            }
        }
        if (policy == nullptr) {
            outcome.skipped += 1;
            continue;
        }
        auto apply_r = policy::apply_policy_effects(
            state, sel.country, *policy);
        if (!apply_r) {
            // Fail-continue per the M5.6 neighbour invariant —
            // record but don't abort the remaining apply calls.
            outcome.failed_countries.push_back(sel.country);
            continue;
        }
        outcome.applied += 1;
    }

    return core::Result<ApplyOutcome>::success(std::move(outcome));
}

}  // namespace leviathan::systems::ai_policy

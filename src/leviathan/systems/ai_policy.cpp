#include "leviathan/systems/ai_policy.hpp"

#include <cstddef>

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

}  // namespace leviathan::systems::ai_policy

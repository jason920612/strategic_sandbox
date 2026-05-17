#include "leviathan/systems/interest_group_system.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace leviathan::systems::interest_group {

core::Result<ReactionOutcome> react(core::GameState& state) {
    // ---- Preflight: every group.country must index into state.countries.
    // M3.2 documents atomicity across the list: if any single
    // group has a bad country, NO group's loyalty / radicalism
    // is mutated. The preflight pass is cheap (size N walk) and
    // makes the test surface obvious.
    for (std::size_t i = 0; i < state.interest_groups.size(); ++i) {
        const auto& g = state.interest_groups[i];
        if (!g.country.valid() ||
            g.country.value() < 0 ||
            static_cast<std::size_t>(g.country.value()) >=
                state.countries.size()) {
            return core::Result<ReactionOutcome>::failure(
                "interest_group::react: interest_groups[" +
                std::to_string(i) +
                "] country is not a valid index into state.countries");
        }
    }

    // ---- Apply drift -----------------------------------------------------
    ReactionOutcome outcome;
    for (auto& g : state.interest_groups) {
        const auto& country = state.countries[
            static_cast<std::size_t>(g.country.value())];
        const double target_loyalty    = country.stability;
        const double target_radicalism = 1.0 - country.stability;

        g.loyalty    += (target_loyalty    - g.loyalty)
                        * kInterestGroupReactionRate;
        g.radicalism += (target_radicalism - g.radicalism)
                        * kInterestGroupReactionRate;

        g.loyalty    = std::clamp(g.loyalty,    0.0, 1.0);
        g.radicalism = std::clamp(g.radicalism, 0.0, 1.0);

        ++outcome.groups_updated;
    }

    return core::Result<ReactionOutcome>::success(outcome);
}

}  // namespace leviathan::systems::interest_group

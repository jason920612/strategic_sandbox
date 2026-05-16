#include "leviathan/systems/faction_system.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace leviathan::systems::faction {

core::Result<ReactionOutcome> react(core::GameState& state,
                                    core::CountryId  country) {
    // Precondition: country resolves to a valid index. We refuse to
    // mutate any faction if the actor is bogus.
    if (!country.valid() ||
        country.value() < 0 ||
        static_cast<std::size_t>(country.value()) >= state.countries.size()) {
        return core::Result<ReactionOutcome>::failure(
            "faction::react: country CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }

    const auto& c = state.countries[static_cast<std::size_t>(country.value())];

    ReactionOutcome outcome;
    for (auto& f : state.factions) {
        if (f.country != country) continue;

        // Loyalty drifts toward country.stability.
        f.loyalty += (c.stability - f.loyalty) * kLoyaltyDriftRate;
        f.loyalty  = std::clamp(f.loyalty, 0.0, 1.0);

        // Support drifts toward country.legitimacy.
        f.support += (c.legitimacy - f.support) * kSupportDriftRate;
        f.support  = std::clamp(f.support, 0.0, 1.0);

        // influence, radicalism, resources, country_id_code, type,
        // preferred_policies all unchanged for M1.6.

        ++outcome.factions_updated;
    }

    return core::Result<ReactionOutcome>::success(outcome);
}

}  // namespace leviathan::systems::faction

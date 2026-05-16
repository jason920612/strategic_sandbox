#include "leviathan/systems/stability_system.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace leviathan::systems::stability {

core::Result<StabilityOutcome> tick(core::GameState& state,
                                    core::CountryId  country) {
    if (!country.valid() ||
        country.value() < 0 ||
        static_cast<std::size_t>(country.value()) >= state.countries.size()) {
        return core::Result<StabilityOutcome>::failure(
            "stability::tick: country CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }

    auto& c = state.countries[static_cast<std::size_t>(country.value())];

    // ---- Compute averages over factions in this country -----------
    // Iteration order: vector order, deterministic. Doubles only,
    // so no integer-overflow concerns at any sane faction count.
    double sum_support    = 0.0;
    double sum_radicalism = 0.0;
    int matching          = 0;
    for (const auto& f : state.factions) {
        if (f.country != country) continue;
        sum_support    += f.support;
        sum_radicalism += f.radicalism;
        ++matching;
    }
    const double avg_support    = (matching > 0)
                                    ? sum_support    / static_cast<double>(matching)
                                    : kNoFactionsSupportDefault;
    const double avg_radicalism = (matching > 0)
                                    ? sum_radicalism / static_cast<double>(matching)
                                    : kNoFactionsRadicalismDefault;

    // ---- Target stability -----------------------------------------
    const double raw_target =
          kSupportWeight    * avg_support
        + kLegitimacyWeight * c.legitimacy
        - kCorruptionWeight * c.corruption
        - kRadicalismWeight * avg_radicalism;
    const double target = std::clamp(raw_target, 0.0, 1.0);

    // ---- Drift ----------------------------------------------------
    StabilityOutcome out;
    out.previous_stability = c.stability;
    out.target_stability   = target;

    c.stability += (target - c.stability) * kStabilityDriftRate;
    c.stability  = std::clamp(c.stability, 0.0, 1.0);

    out.new_stability = c.stability;
    return core::Result<StabilityOutcome>::success(out);
}

}  // namespace leviathan::systems::stability

#include "leviathan/systems/stability_system.hpp"

#include <cstddef>
#include <string>

#include "internal/numeric_guards.hpp"

namespace leviathan::systems::stability {

namespace ng = leviathan::systems::detail;

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

    // Validate country-side inputs the formula reads.
    if (auto err = ng::require_unit_ratio<StabilityOutcome>(
            "stability::tick", "country", c.id_code,
            "country.stability", c.stability)) {
        return *err;
    }
    if (auto err = ng::require_unit_ratio<StabilityOutcome>(
            "stability::tick", "country", c.id_code,
            "country.legitimacy", c.legitimacy)) {
        return *err;
    }
    if (auto err = ng::require_unit_ratio<StabilityOutcome>(
            "stability::tick", "country", c.id_code,
            "country.corruption", c.corruption)) {
        return *err;
    }
    // last_gdp_growth_rate is signed (recession produces a negative
    // value), so a unit-ratio check would be wrong; require only that
    // it be a finite double. Per the post-M6.7 hardening, a non-finite
    // growth rate (e.g. NaN propagated from a buggy economy::tick)
    // surfaces here instead of silently clamping the target.
    if (auto err = ng::require_finite_double<StabilityOutcome>(
            "stability::tick", "country", c.id_code,
            "country.last_gdp_growth_rate", c.last_gdp_growth_rate)) {
        return *err;
    }

    // ---- Compute averages over factions in this country -----------
    // Iteration order: vector order, deterministic. Validate each
    // faction's support / radicalism before reading them into the sum.
    double sum_support    = 0.0;
    double sum_radicalism = 0.0;
    int matching          = 0;
    for (const auto& f : state.factions) {
        if (f.country != country) continue;
        if (auto err = ng::require_unit_ratio<StabilityOutcome>(
                "stability::tick", "faction", f.id_code,
                "faction.support", f.support)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<StabilityOutcome>(
                "stability::tick", "faction", f.id_code,
                "faction.radicalism", f.radicalism)) {
            return *err;
        }
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
    // M1.12: + kEconomicGrowthWeight * last_gdp_growth_rate is the
    // RFC-080 §5 EconomicGrowth term. The value was written at the
    // END of the previous monthly tick's economy::tick; the canonical
    // monthly pipeline order (faction -> stability -> economy) is
    // unchanged, so this read sees last month's growth.
    const double target_candidate =
          kSupportWeight        * avg_support
        + kLegitimacyWeight     * c.legitimacy
        - kCorruptionWeight     * c.corruption
        - kRadicalismWeight     * avg_radicalism
        + kEconomicGrowthWeight * c.last_gdp_growth_rate;

    // Post-M6.7 hardening: the previous `std::clamp(raw_target, 0.0,
    // 1.0)` is gone. Reject if the formula produces a value outside
    // `[0, 1]` — that signals either a pathological growth rate or an
    // upstream coefficient drift the silent clamp was hiding.
    if (auto err = ng::require_unit_ratio<StabilityOutcome>(
            "stability::tick", "country", c.id_code,
            "stability.target (formula candidate)", target_candidate)) {
        return *err;
    }
    const double target = target_candidate;

    // ---- Drift ----------------------------------------------------
    const double new_stability =
        c.stability + (target - c.stability) * kStabilityDriftRate;

    // Post-M6.7 hardening: the previous post-drift clamp is gone.
    // With every input validated in `[0, 1]` and the drift rate in
    // `(0, 1)`, the candidate is provably in `[0, 1]` by construction
    // — the explicit check below catches any future drift-rate change
    // and any upstream bug that slipped past the input validation.
    if (auto err = ng::require_unit_ratio<StabilityOutcome>(
            "stability::tick", "country", c.id_code,
            "country.stability (post-drift candidate)", new_stability)) {
        return *err;
    }

    StabilityOutcome out;
    out.previous_stability = c.stability;
    out.target_stability   = target;
    c.stability            = new_stability;
    out.new_stability      = c.stability;
    return core::Result<StabilityOutcome>::success(out);
}

}  // namespace leviathan::systems::stability

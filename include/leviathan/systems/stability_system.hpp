// StabilitySystem - one step of country stability adjustment.
//
// M1.7 introduces the FIRST country-side dynamic. Scope is
// deliberately tiny per the M1.6 review: an explicit-call free
// function that computes a target stability from inputs the state
// already has, drifts current stability toward it, and clamps.
// No monthly tick, no runner integration, no AI, no event
// integration.
//
// The formula consumes inputs that exist in the current GameState:
//
//   target =  kSupportWeight        * avg_faction_support
//          +  kLegitimacyWeight     * country.legitimacy
//          -  kCorruptionWeight     * country.corruption
//          -  kRadicalismWeight     * avg_faction_radicalism
//          +  kEconomicGrowthWeight * country.last_gdp_growth_rate  (M1.12)
//
//   stability += (clamp(target, 0, 1) - stability) * kStabilityDriftRate
//   stability  = clamp(stability, 0, 1)
//
// The EconomicGrowth term is RFC-080 §5's `EconomicGrowth` input
// (M1.12). It reads CountryState::last_gdp_growth_rate, which
// economy::tick wrote at the END of the PREVIOUS monthly tick. The
// monthly pipeline order (faction -> stability -> economy) is
// unchanged from M1.9, so stability sees last month's growth, not
// this month's. The one-month lag is deliberate; see
// docs/m1-12-economy-stability-coupling.md.
//
// RFC-080 §5's full formula also includes WelfareSatisfaction,
// InequalityProxy, WarWeariness, BudgetCrisis. None of those have
// inputs in the current GameState; they land in later sub-milestones
// as welfare / war systems come online.
//
// When a country has zero factions belonging to it, avg_support and
// avg_radicalism each default to 0.5 (the neutral midpoint), so the
// formula remains well-defined for empty-faction-list countries.

#ifndef LEVIATHAN_SYSTEMS_STABILITY_SYSTEM_HPP
#define LEVIATHAN_SYSTEMS_STABILITY_SYSTEM_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::stability {

struct StabilityOutcome {
    double previous_stability = 0.0;
    double new_stability      = 0.0;
    double target_stability   = 0.0;   // the value `new_stability` is drifting toward
};

// Coefficients exported so tests reference named constants rather
// than literal magic numbers, and so a balance-tuning PR can locate
// every knob in one place.
inline constexpr double kSupportWeight       = 0.5;
inline constexpr double kLegitimacyWeight    = 0.5;
inline constexpr double kCorruptionWeight    = 0.3;
inline constexpr double kRadicalismWeight    = 0.2;
inline constexpr double kStabilityDriftRate  = 0.10;

// M1.12: EconomicGrowth term coefficient (RFC-080 §5). Multiplies
// CountryState::last_gdp_growth_rate, which is a fractional monthly
// value (e.g. 0.0035 = +0.35% monthly). At 2.0 the term contributes
// roughly 0.007 to the target for a typical 0.35%/month economy
// (well below the legitimacy / support / corruption / radicalism
// terms), so it shifts behaviour at the margin rather than dominating.
inline constexpr double kEconomicGrowthWeight = 2.0;

// Defaults used when the actor has no factions in state.factions.
// 0.5 is the neutral midpoint of a ratio - it means "no political
// pressure either way" rather than "no support at all".
inline constexpr double kNoFactionsSupportDefault    = 0.5;
inline constexpr double kNoFactionsRadicalismDefault = 0.5;

// Apply one step of stability adjustment to `country` in `state`.
// See the formula at the top of this header.
//
// Failure cases:
//   - `country` is not a valid index into `state.countries`
// On failure, no state is modified.
//
// Only state.countries[country].stability is modified. Faction state
// is read but never written.
core::Result<StabilityOutcome> tick(core::GameState& state,
                                    core::CountryId  country);

}  // namespace leviathan::systems::stability

#endif  // LEVIATHAN_SYSTEMS_STABILITY_SYSTEM_HPP

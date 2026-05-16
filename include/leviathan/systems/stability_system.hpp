// StabilitySystem - one step of country stability adjustment.
//
// M1.7 introduces the FIRST country-side dynamic. Scope is
// deliberately tiny per the M1.6 review: an explicit-call free
// function that computes a target stability from inputs the state
// already has, drifts current stability toward it, and clamps.
// No monthly tick, no runner integration, no AI, no event
// integration.
//
// The formula is a stripped-down RFC-080 §5 stability formula. It
// only consumes inputs that exist in the current GameState:
//
//   target =  kSupportWeight    * avg_faction_support
//          +  kLegitimacyWeight * country.legitimacy
//          -  kCorruptionWeight * country.corruption
//          -  kRadicalismWeight * avg_faction_radicalism
//
//   stability += (clamp(target, 0, 1) - stability) * kStabilityDriftRate
//   stability  = clamp(stability, 0, 1)
//
// RFC-080 §5's full formula also includes WelfareSatisfaction,
// EconomicGrowth, InequalityProxy, WarWeariness, BudgetCrisis. None
// of those have inputs in M1.7's GameState; they land in M1.8+ as
// economy / war systems come online.
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

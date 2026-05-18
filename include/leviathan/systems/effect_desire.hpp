// effect_desire — "how much does country C want target T to move up?"
//
// Shared scoring primitive used by both `ai_policy::score_policy` and
// `event_effects::select_best_option_for_country`. Returns a positive
// number when the country benefits from RAISING the named target, a
// negative number when the country benefits from LOWERING it, and zero
// when the target is neutral / unknown.
//
// Score-for-effect convention (both consumers use this):
//
//     score(effect) = effect.value × effect_desire::for_country(c, effect.target, state)
//
// A `+0.05 country.stability` effect on a country with low stability
// (1 − 0.10 desire = 0.90) scores `0.05 × 0.90 = 0.045`. A `−0.04
// country.corruption` effect on a country with high corruption
// (−0.50 desire) scores `−0.04 × −0.50 = 0.02`. Negative-axis (bad-
// axis) targets return negative desire so a negative-value effect on
// them lands positively for the country.
//
// Reads:
//   - CountryState fields named on the LHS of every `country.*` branch
//     (stability / legitimacy / administrative_efficiency / fiscal_capacity
//     / central_control / corruption / budget_balance / gdp /
//     legal_tax_burden / threat_perception / military_strength).
//   - GameState::relationships for the `country.military_power` term
//     (inbound threat + the strongest neighbour's military_strength).
//
// Determinism:
//   - Pure read; never mutates state.
//   - RNG-free.
//   - No I/O.
//   - Same inputs → identical output bytes.
//
// Unknown country target (e.g. `country.gdp`, `country.tax_revenue`):
// returns 0.0. Faction-side targets (`faction:*`) and any non-
// `country.*` target also return 0.0 — the desire helper deliberately
// scores only the modern `country.*` surface; faction-support effects
// are handled by the caller if needed (issue #110 / #111 chose to
// ignore faction-side effects in policy / option scoring, and this
// helper keeps that decision).

#ifndef LEVIATHAN_SYSTEMS_EFFECT_DESIRE_HPP
#define LEVIATHAN_SYSTEMS_EFFECT_DESIRE_HPP

#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"

namespace leviathan::systems::effect_desire {

double for_country(const core::CountryState& c,
                   const std::string&        target,
                   const core::GameState&    state);

}  // namespace leviathan::systems::effect_desire

#endif  // LEVIATHAN_SYSTEMS_EFFECT_DESIRE_HPP

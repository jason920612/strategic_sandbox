#ifndef LEVIATHAN_SYSTEMS_FACTION_INTEREST_BIAS_HPP
#define LEVIATHAN_SYSTEMS_FACTION_INTEREST_BIAS_HPP

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::faction_interest_bias {

inline constexpr double kFactionInterestBiasMaxMagnitude = 0.25;

core::Result<double> compute_for_event_country(
    const core::GameState&       state,
    const core::EventDefinition& definition,
    core::CountryId              country);

}  // namespace leviathan::systems::faction_interest_bias

#endif  // LEVIATHAN_SYSTEMS_FACTION_INTEREST_BIAS_HPP

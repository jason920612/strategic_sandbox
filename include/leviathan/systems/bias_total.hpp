#ifndef LEVIATHAN_SYSTEMS_BIAS_TOTAL_HPP
#define LEVIATHAN_SYSTEMS_BIAS_TOTAL_HPP

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::bias_total {

struct BiasBreakdown {
    double faction_interest_bias = 0.0;
    double bureaucratic_self_protection = 0.0;
    double propaganda_bias = 0.0;
    double total = 0.0;
};

core::Result<BiasBreakdown> compute_for_event_country(
    const core::GameState&       state,
    const core::EventDefinition& definition,
    core::CountryId              country);

}  // namespace leviathan::systems::bias_total

#endif  // LEVIATHAN_SYSTEMS_BIAS_TOTAL_HPP

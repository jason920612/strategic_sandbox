#ifndef LEVIATHAN_SYSTEMS_BUREAUCRATIC_SELF_PROTECTION_HPP
#define LEVIATHAN_SYSTEMS_BUREAUCRATIC_SELF_PROTECTION_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::bureaucratic_self_protection {

inline constexpr double kBureaucraticSelfProtectionMaxMagnitude = 0.20;

core::Result<double> compute_for_country(
    const core::GameState& state,
    core::CountryId        country);

}  // namespace leviathan::systems::bureaucratic_self_protection

#endif  // LEVIATHAN_SYSTEMS_BUREAUCRATIC_SELF_PROTECTION_HPP

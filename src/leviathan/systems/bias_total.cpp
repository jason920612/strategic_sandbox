#include "leviathan/systems/bias_total.hpp"

#include "leviathan/systems/bureaucratic_self_protection.hpp"
#include "leviathan/systems/faction_interest_bias.hpp"
#include "leviathan/systems/propaganda_bias.hpp"

namespace leviathan::systems::bias_total {

core::Result<BiasBreakdown> compute_for_event_country(
        const core::GameState&       state,
        const core::EventDefinition& definition,
        core::CountryId              country) {
    BiasBreakdown out;

    auto faction_r =
        faction_interest_bias::compute_for_event_country(
            state, definition, country);
    if (!faction_r) {
        return core::Result<BiasBreakdown>::failure(std::move(faction_r.error()));
    }
    out.faction_interest_bias = faction_r.value();

    auto bureaucracy_r =
        bureaucratic_self_protection::compute_for_country(state, country);
    if (!bureaucracy_r) {
        return core::Result<BiasBreakdown>::failure(
            std::move(bureaucracy_r.error()));
    }
    out.bureaucratic_self_protection = bureaucracy_r.value();

    auto propaganda_r = propaganda_bias::compute_for_country(state, country);
    if (!propaganda_r) {
        return core::Result<BiasBreakdown>::failure(
            std::move(propaganda_r.error()));
    }
    out.propaganda_bias = propaganda_r.value();

    out.total = out.faction_interest_bias +
                out.bureaucratic_self_protection +
                out.propaganda_bias;
    return core::Result<BiasBreakdown>::success(out);
}

}  // namespace leviathan::systems::bias_total

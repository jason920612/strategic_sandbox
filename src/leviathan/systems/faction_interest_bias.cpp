#include "leviathan/systems/faction_interest_bias.hpp"

#include <cmath>
#include <cstddef>
#include <string>

#include "leviathan/core/interest_group_kind.hpp"

namespace leviathan::systems::faction_interest_bias {

namespace {

bool is_unit_ratio(double v) {
    return std::isfinite(v) && v >= 0.0 && v <= 1.0;
}

bool is_alignment(double v) {
    return std::isfinite(v) && v >= -1.0 && v <= 1.0;
}

}  // namespace

core::Result<double> compute_for_event_country(
        const core::GameState&       state,
        const core::EventDefinition& definition,
        core::CountryId              country) {
    if (!country.valid() ||
        country.value() < 0 ||
        static_cast<std::size_t>(country.value()) >= state.countries.size()) {
        return core::Result<double>::failure(
            "faction_interest_bias::compute_for_event_country: actor CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }
    const auto& c = state.countries[
        static_cast<std::size_t>(country.value())];

    double weighted_sum = 0.0;
    double influence_sum = 0.0;

    for (std::size_t i = 0; i < definition.faction_interest_bias.size(); ++i) {
        const auto& row = definition.faction_interest_bias[i];
        if (row.interest_group_kind.empty()) {
            return core::Result<double>::failure(
                "faction_interest_bias::compute_for_event_country: event '" +
                definition.id_code + "' faction_interest_bias[" +
                std::to_string(i) + "].interest_group_kind is empty");
        }
        auto kind_r =
            core::interest_group_kind_from_string(row.interest_group_kind);
        if (!kind_r) {
            return core::Result<double>::failure(
                "faction_interest_bias::compute_for_event_country: event '" +
                definition.id_code + "' faction_interest_bias[" +
                std::to_string(i) + "]: " + std::move(kind_r.error()));
        }
        if (!is_alignment(row.alignment)) {
            return core::Result<double>::failure(
                "faction_interest_bias::compute_for_event_country: event '" +
                definition.id_code + "' faction_interest_bias[" +
                std::to_string(i) + "].alignment = " +
                std::to_string(row.alignment) +
                " is not finite in [-1, 1]");
        }

        for (const auto& g : state.interest_groups) {
            if (g.country.value() != country.value() ||
                g.kind != kind_r.value()) {
                continue;
            }
            if (!is_unit_ratio(g.influence)) {
                return core::Result<double>::failure(
                    "faction_interest_bias::compute_for_event_country: "
                    "interest group '" + g.id_code + "' influence = " +
                    std::to_string(g.influence) +
                    " is not a finite ratio in [0, 1]");
            }
            weighted_sum += g.influence * row.alignment;
            influence_sum += g.influence;
        }
    }

    if (influence_sum == 0.0) {
        return core::Result<double>::success(0.0);
    }

    return core::Result<double>::success(
        kFactionInterestBiasMaxMagnitude * (weighted_sum / influence_sum));
}

}  // namespace leviathan::systems::faction_interest_bias

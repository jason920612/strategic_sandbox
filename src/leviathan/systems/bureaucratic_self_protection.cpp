#include "leviathan/systems/bureaucratic_self_protection.hpp"

#include <cmath>
#include <cstddef>
#include <string>

namespace leviathan::systems::bureaucratic_self_protection {

namespace {

bool is_unit_ratio(double v) {
    return std::isfinite(v) && v >= 0.0 && v <= 1.0;
}

}  // namespace

core::Result<double> compute_for_country(
        const core::GameState& state,
        core::CountryId        country) {
    if (!country.valid() ||
        country.value() < 0 ||
        static_cast<std::size_t>(country.value()) >= state.countries.size()) {
        return core::Result<double>::failure(
            "bureaucratic_self_protection::compute_for_country: actor CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }

    const auto& c = state.countries[
        static_cast<std::size_t>(country.value())];
    const double compliance =
        c.government_authority.bureaucratic_compliance;
    if (!is_unit_ratio(compliance)) {
        return core::Result<double>::failure(
            "bureaucratic_self_protection::compute_for_country: country '" +
            c.id_code +
            "' government_authority.bureaucratic_compliance = " +
            std::to_string(compliance) +
            " is not a finite ratio in [0, 1]");
    }

    return core::Result<double>::success(
        kBureaucraticSelfProtectionMaxMagnitude * (1.0 - compliance));
}

}  // namespace leviathan::systems::bureaucratic_self_protection

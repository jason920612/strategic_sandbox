// PropagandaBias implementation — RFC-080 §8 Bias term
// `+ PropagandaBias`.
//
// See include/leviathan/systems/propaganda_bias.hpp for the
// public contract (formula, polarity, magnitude grounding,
// validation rules, deliberate non-goals).
//
// The body is intentionally small: validate the country index,
// validate `media_control` as a finite `[0, 1]` ratio, return
// `kPropagandaBiasMaxMagnitude × media_control`. The validation
// mirrors `information_accuracy::require_unit_ratio` shape so
// the diagnostic format stays consistent across the M6 helpers.

#include "leviathan/systems/propaganda_bias.hpp"

#include <cmath>
#include <cstddef>
#include <string>

namespace leviathan::systems::propaganda_bias {

namespace {

// Strict ratio validation. Per `feedback_no_silent_degradation`,
// out-of-range / non-finite inputs are REJECTED loudly. The
// helper here mirrors the message format used by
// `information_accuracy::compute_for_country` so a future grep
// across the M6 helpers shows one consistent diagnostic shape.
bool is_unit_ratio(double v) {
    return std::isfinite(v) && v >= 0.0 && v <= 1.0;
}

}  // namespace

core::Result<double> compute_for_country(
        const core::GameState& state,
        core::CountryId        country) {
    if (!country.valid() ||
        country.value() < 0 ||
        static_cast<std::size_t>(country.value())
            >= state.countries.size()) {
        return core::Result<double>::failure(
            "propaganda_bias::compute_for_country: "
            "actor CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }

    const auto& c = state.countries[
        static_cast<std::size_t>(country.value())];
    const double media_control = c.government_authority.media_control;
    if (!is_unit_ratio(media_control)) {
        return core::Result<double>::failure(
            "propaganda_bias::compute_for_country: "
            "country '" + c.id_code +
            "' government_authority.media_control = " +
            std::to_string(media_control) +
            " is not a finite ratio in [0, 1]");
    }

    return core::Result<double>::success(
        kPropagandaBiasMaxMagnitude * media_control);
}

}  // namespace leviathan::systems::propaganda_bias

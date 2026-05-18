// M6.3: InformationAccuracy implementation.
//
// See include/leviathan/systems/information_accuracy.hpp for
// the public contract (placeholder formula, range, validation,
// deliberate non-goals). M6.3 ships the helper SHAPE only —
// the body is a placeholder that returns 1.0 unconditionally
// for any valid country. M6.6 / M6.7 will replace this body
// with the actual distortion formula.

#include "leviathan/systems/information_accuracy.hpp"

#include <cstddef>
#include <string>

namespace leviathan::systems::information_accuracy {

core::Result<double> compute_for_country(
        const core::GameState& state,
        core::CountryId        country) {
    // Validate the country index. Mirrors the M1.5 /
    // M5.6-extracted `policy::apply_effects_to_actor` validation
    // pattern — same shape so a future M6.6 / M6.7 refactor that
    // pulls in a shared "country lookup" helper can move both
    // call sites at once.
    if (!country.valid() ||
        country.value() < 0 ||
        static_cast<std::size_t>(country.value())
            >= state.countries.size()) {
        return core::Result<double>::failure(
            "information_accuracy::compute_for_country: "
            "actor CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }

    // M6.3 placeholder: always return the
    // no-distortion ceiling. M6.6 will start reducing this
    // below 1.0 based on the intelligence budget; M6.7 will
    // further reduce it based on corruption. The body of this
    // function is the ONLY thing that needs to change to bring
    // those formulas online — the signature, the validation,
    // the return-type contract, and the public constant all
    // stay verbatim.
    return core::Result<double>::success(
        kPlaceholderInformationAccuracy);
}

}  // namespace leviathan::systems::information_accuracy

// M6.6: InformationAccuracy formula body — intelligence-budget
// influence on the M6.3 placeholder ceiling.
//
// M6.3 shipped the function shape with a placeholder body that
// always returned `kPlaceholderInformationAccuracy = 1.0`
// (no-distortion ceiling). M6.6 replaces the body with a real
// formula that reads two existing CountryState fields:
//
//   1. country.government_authority.intelligence_capability
//        [0, 1] — "how capable is this country's intelligence
//        apparatus at producing accurate reports". Direct signal.
//   2. country.budget.intelligence
//        [0, 1] — "share of the budget allocated to intelligence".
//        Per RFC-030 §4 the intelligence budget raises information
//        accuracy directly; M6.6 reads it as a second input
//        alongside intelligence_capability.
//
// Composition (weighted sum, both inputs in [0, 1]):
//
//   intel_score = 0.7 × intelligence_capability
//               + 0.3 × budget.intelligence
//               ∈ [0, 1]
//
// Map to accuracy via an affine floor + ceiling:
//
//   accuracy = kMinAccuracy + (1 - kMinAccuracy) × intel_score
//            ∈ [kMinAccuracy, 1.0]
//
// where `kMinAccuracy = 0.4` (a country with literally zero
// intelligence is still not flat-blind — the player gets a
// degraded report, not nothing).
//
// M6.6 is a stripped-down subset of RFC-080 §8 (full formula:
// `BaseAccuracy + IntelligenceCapacity + MediaFreedomSignal +
// BureaucraticProfessionalism + AuditCapacity - Corruption -
// FactionCapture - LeaderIsolation - LocalAutonomyOpacity`).
// M6.6 ships only the BaseAccuracy + IntelligenceCapacity terms
// (with capability split into `intelligence_capability` weight
// and `budget.intelligence` weight). The remaining RFC-080 §8
// terms are deferred to future RFC-090 sub-milestones — RFC-090
// §6.7 covers the corruption term; the other terms have no
// RFC-090 task assigned yet.
//
// What M6.6 deliberately does NOT do:
//
//   no save schema bump — both fields already exist on CountryState
//   no new state field
//   no caller wiring (compute_for_country has no consumer yet)
//   no corruption term (RFC-090 §6.7 scope)
//   no debug-mode bypass (RFC-090 §6.8 scope)
//   no event_evaluator / event_firer / event_effects / event_engine
//     / monthly_pipeline / runner change
//   no RNG consumption (kept deterministic — same state +
//     same country → same result; canonical 1930_minimal byte-
//     identical baselines stay green because nothing calls this)
//   no change to kPlaceholderInformationAccuracy (the public
//     constant remains the "no-distortion ceiling" semantic)
//
// Canonical-baseline impact: zero. `compute_for_country` has no
// production caller, so the M1.17 / M2 / M3 / M4 / M5 byte-
// identical determinism baselines are unaffected by the new
// formula body. The only behaviour change is observable to
// tests that call the helper directly.

#include "leviathan/systems/information_accuracy.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace leviathan::systems::information_accuracy {

core::Result<double> compute_for_country(
        const core::GameState& state,
        core::CountryId        country) {
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

    const auto& c = state.countries[
        static_cast<std::size_t>(country.value())];

    // M6.6: reject non-finite intelligence inputs before clamping.
    // `std::clamp(NaN, 0, 1)` returns NaN — a silent clamp would
    // leak a NaN accuracy past the documented closed range and
    // mask a state-corruption bug. The load layer already requires
    // finite ratios; reaching this branch means the state was
    // mutated to a bad value at runtime, which the caller needs to
    // see surfaced as a failure.
    const double raw_cap =
        c.government_authority.intelligence_capability;
    const double raw_bud = c.budget.intelligence;
    if (!std::isfinite(raw_cap)) {
        return core::Result<double>::failure(
            "information_accuracy::compute_for_country: "
            "country '" + c.id_code +
            "' government_authority.intelligence_capability "
            "is not finite");
    }
    if (!std::isfinite(raw_bud)) {
        return core::Result<double>::failure(
            "information_accuracy::compute_for_country: "
            "country '" + c.id_code +
            "' budget.intelligence is not finite");
    }

    // M6.6: weighted combination of the two intelligence inputs.
    // Clamp each input to [0, 1] defensively — the load layer
    // already pins ratios in range, but a hand-built test fixture
    // or a future schema change shouldn't be able to push the
    // result outside [kMinInformationAccuracy, 1.0]. Non-finite
    // inputs were already rejected above, so clamp here only
    // narrows finite out-of-range values.
    const double cap = std::clamp(raw_cap, 0.0, 1.0);
    const double bud = std::clamp(raw_bud, 0.0, 1.0);
    const double intel_score =
        kInformationAccuracyCapabilityWeight * cap +
        kInformationAccuracyBudgetWeight     * bud;

    const double accuracy =
        kMinInformationAccuracy +
        (1.0 - kMinInformationAccuracy) * intel_score;

    // Range is closed [kMinInformationAccuracy, 1.0] by
    // construction; clamp once defensively in case the weights
    // get rebalanced incorrectly in a future edit.
    return core::Result<double>::success(
        std::clamp(accuracy, kMinInformationAccuracy, 1.0));
}

}  // namespace leviathan::systems::information_accuracy

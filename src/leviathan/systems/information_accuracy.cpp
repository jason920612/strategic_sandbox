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
//        Funding side; would feed capability over time once a
//        future M-driver wires it. Currently read directly because
//        no such driver exists yet.
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
// degraded report, not nothing). M6.7 will further reduce
// accuracy via the `-corruption` term and may push the effective
// floor below 0.4 when corruption is high; M6.9 will be the
// first downstream caller (consuming accuracy through
// `reported_value::from_true_value` + `bias_noise::sample_for_event`
// in the non-debug hiding path).
//
// What M6.6 deliberately does NOT do (per the M6 invariant chain
// the user pinned):
//
//   no save schema bump — both fields already exist on CountryState
//   no new state field
//   no caller wiring (no system reads compute_for_country yet;
//     M6.9 is the first caller)
//   no corruption term (M6.7)
//   no debug-mode bypass (M6.8)
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

    // M6.6: weighted combination of the two intelligence inputs.
    // Clamp each input to [0, 1] defensively — the load layer
    // already pins ratios in range, but a hand-built test fixture
    // or a future schema change shouldn't be able to push the
    // result outside [kMinInformationAccuracy, 1.0].
    const double cap = std::clamp(
        c.government_authority.intelligence_capability, 0.0, 1.0);
    const double bud = std::clamp(c.budget.intelligence, 0.0, 1.0);
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

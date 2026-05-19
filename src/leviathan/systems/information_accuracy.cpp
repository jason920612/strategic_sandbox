// InformationAccuracy formula body — M6.6 intelligence-budget
// baseline + M6.7 corruption-influence subtraction.
//
// The helper computes a single accuracy value for one country by
// reading three CountryState ratios (each must already be a
// finite value in `[0, 1]` — the load layer pins them, and the
// helper re-validates per `feedback_no_silent_degradation`):
//
//   1. country.government_authority.intelligence_capability
//        [0, 1] — how capable the country's intelligence apparatus
//        is at producing accurate reports. Direct signal. (M6.6)
//   2. country.budget.intelligence
//        [0, 1] — share of the budget allocated to intelligence.
//        Per RFC-030 §4 the intelligence budget raises information
//        accuracy directly. (M6.6)
//   3. country.corruption
//        [0, 1] — the negative term per RFC-080 §8. Corruption
//        erodes accuracy regardless of how much capability or
//        budget the country has. (M6.7, RFC-090 §6.7
//        `加入腐敗影響`.)
//
// Composition:
//
//   intel_score = kCapabilityWeight × intelligence_capability
//               + kBudgetWeight     × budget.intelligence
//               ∈ [0, 1]
//
//   m6_6_baseline = kMinInformationAccuracy
//                 + (1 - kMinInformationAccuracy) × intel_score
//                 ∈ [kMinInformationAccuracy, 1.0]
//
//   accuracy      = m6_6_baseline
//                 - kCorruptionWeight × corruption
//                 ∈ [0.0, 1.0]
//
// `kMinInformationAccuracy = 0.4` is the M6.6 contribution floor
// (a country with literally zero intelligence is still not
// flat-blind on the M6.6 axis alone). `kCorruptionWeight = 0.4`
// is the M6.7 maximum subtraction — symmetric to the M6.6 floor,
// so a fully-corrupt country with zero intelligence reaches a
// total accuracy of 0 (full blackout). A fully-corrupt country
// with maxed intelligence still keeps accuracy = 0.6.
//
// The helper now implements a stripped-down subset of RFC-080
// §8's full accuracy formula:
//
//   BaseAccuracy
//   + IntelligenceCapacity                        ← M6.6 (split into
//                                                   capability + budget)
//   + MediaFreedomSignal                          ← deferred
//   + BureaucraticProfessionalism                 ← deferred
//   + AuditCapacity                               ← deferred
//   - Corruption                                  ← M6.7
//   - FactionCapture                              ← deferred
//   - LeaderIsolation                             ← deferred
//   - LocalAutonomyOpacity                        ← deferred
//
// Deferred terms have no RFC-090 task assigned yet.
//
// What M6.7 deliberately does NOT do:
//
//   no save schema bump — `corruption` already exists on
//     CountryState since M1.1
//   no new state field
//   no caller wiring (compute_for_country still has no consumer)
//   no debug-mode bypass (RFC-090 §6.8 scope)
//   no event_evaluator / event_firer / event_effects / event_engine
//     / monthly_pipeline / runner change
//   no RNG consumption (kept deterministic)
//   no change to kPlaceholderInformationAccuracy (the public
//     constant remains the "no-distortion ceiling" semantic —
//     `accuracy = 1.0` is now reached when intelligence is maxed
//     AND corruption is zero)
//
// Canonical-baseline impact: zero. `compute_for_country` still
// has no production caller, so the M1.17 / M2 / M3 / M4 / M5
// byte-identical determinism baselines are unaffected. The
// only behaviour changes are observable to tests that call the
// helper directly.

#include "leviathan/systems/information_accuracy.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>

namespace leviathan::systems::information_accuracy {

namespace {

// Per `feedback_no_silent_degradation`: numeric inputs with a
// documented `[0, 1]` ratio range must be REJECTED loudly when
// out-of-range or non-finite — never silently clamped. The load
// layer is the first line of defence; this runtime check is the
// second. Both should fail loudly, because clamping a NaN, Inf,
// or out-of-range value turns a detectable state-corruption bug
// into a silent one.
//
// Returns std::nullopt on success; otherwise returns a populated
// failure Result the caller can hand straight back.
std::optional<core::Result<double>> require_unit_ratio(
        const std::string& country_id_code,
        const char*        field_name,
        double             value) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        return core::Result<double>::failure(
            "information_accuracy::compute_for_country: "
            "country '" + country_id_code + "' " + field_name +
            " = " + std::to_string(value) +
            " is not a finite ratio in [0, 1]");
    }
    return std::nullopt;
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
            "information_accuracy::compute_for_country: "
            "actor CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }

    const auto& c = state.countries[
        static_cast<std::size_t>(country.value())];

    if (auto err = require_unit_ratio(
            c.id_code,
            "government_authority.intelligence_capability",
            c.government_authority.intelligence_capability)) {
        return *err;
    }
    if (auto err = require_unit_ratio(
            c.id_code, "budget.intelligence",
            c.budget.intelligence)) {
        return *err;
    }
    if (auto err = require_unit_ratio(
            c.id_code, "corruption", c.corruption)) {
        return *err;
    }

    // M6.6 baseline: weighted combination of the two intelligence
    // inputs mapped to an affine `[kMinInformationAccuracy, 1.0]`
    // range. Inputs are validated above, so no defensive clamping
    // is needed here.
    const double intel_score =
        kInformationAccuracyCapabilityWeight *
            c.government_authority.intelligence_capability +
        kInformationAccuracyBudgetWeight *
            c.budget.intelligence;
    const double m6_6_baseline =
        kMinInformationAccuracy +
        (1.0 - kMinInformationAccuracy) * intel_score;

    // M6.7 corruption subtraction (RFC-090 §6.7 `加入腐敗影響`):
    // subtract a corruption-weighted penalty from the M6.6
    // baseline. With `kInformationAccuracyCorruptionWeight = 0.4`
    // and inputs validated to `[0, 1]`, the math is provably in
    // `[0.0, 1.0]`:
    //   - intel maxed + zero corruption  →  baseline=1.0 → 1.0
    //   - intel maxed + max  corruption  →  baseline=1.0 → 0.6
    //   - zero  intel  + zero corruption →  baseline=0.4 → 0.4
    //   - zero  intel  + max  corruption →  baseline=0.4 → 0.0
    const double accuracy =
        m6_6_baseline -
        kInformationAccuracyCorruptionWeight * c.corruption;

    // The math above lands in `[0.0, 1.0]` by construction. The
    // final clamp is a single-ULP floating-point safety net (e.g.,
    // rounding may produce 1.0000000000000002 for some weight
    // combinations) — NOT an input-validation fallback. Input
    // validation lives in `require_unit_ratio` above.
    return core::Result<double>::success(
        std::clamp(accuracy, 0.0, 1.0));
}

}  // namespace leviathan::systems::information_accuracy

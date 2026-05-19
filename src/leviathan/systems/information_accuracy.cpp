// InformationAccuracy formula body — M6.6 intelligence-budget
// baseline + M6.7 corruption-influence subtraction + M6 closeout-
// audit MediaFreedomSignal positive term.
//
// The helper computes a single accuracy value for one country by
// reading four CountryState ratios (each must already be a finite
// value in `[0, 1]` — the load layer pins them, and the helper
// re-validates per `feedback_no_silent_degradation`):
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
//   4. country.government_authority.media_control
//        [0, 1] — state control of the media. The complement
//        `1 - media_control` is the MediaFreedomSignal positive
//        contributor in RFC-080 §8. Freer media exposes the
//        leader to information the state would otherwise suppress
//        (V-Dem methodology; Egorov, Guriev & Sonin (QJP 2009)).
//        Shipped here as one of two representative residuals in
//        the M6 closeout-audit PR. The same field also feeds
//        `propaganda_bias::compute_for_country` (RFC-080 §8
//        `Bias = ... + PropagandaBias`) on the bias-term side.
//
// Composition:
//
//   intel_score   = kCapabilityWeight × intelligence_capability
//                 + kBudgetWeight     × budget.intelligence
//                 ∈ [0, 1]
//
//   media_freedom = 1 - media_control
//                 ∈ [0, 1]
//
//   positive_axis = (1 - kMediaFreedomWeight) × intel_score
//                 +       kMediaFreedomWeight × media_freedom
//                 ∈ [0, 1]
//
//   base          = kMinInformationAccuracy
//                 + (1 - kMinInformationAccuracy) × positive_axis
//                 ∈ [kMinInformationAccuracy, 1.0]
//
//   accuracy      = base
//                 - kCorruptionWeight × corruption
//                 ∈ [0.0, 1.0]
//
// `kMinInformationAccuracy = 0.4` is the positive-axis floor
// (a country with zero intelligence and total media capture is
// still not flat-blind on the positive axis alone).
// `kCorruptionWeight = 0.4` is the M6.7 maximum subtraction —
// symmetric to the positive-axis floor, so a fully-corrupt
// country with zero intelligence and total media capture reaches
// a total accuracy of 0 (full blackout). A fully-corrupt country
// with maxed intelligence and free media still keeps accuracy
// = 0.6.
//
// The helper now implements a stripped-down subset of RFC-080
// §8's full accuracy formula:
//
//   BaseAccuracy
//   + IntelligenceCapacity                        ← M6.6 (split into
//                                                   capability + budget)
//   + MediaFreedomSignal                          ← M6 closeout audit
//                                                   (1 - media_control)
//   + BureaucraticProfessionalism                 ← deferred (no field;
//                                                   see audit doc §9)
//   + AuditCapacity                               ← deferred (no field)
//   - Corruption                                  ← M6.7
//   - FactionCapture                              ← deferred
//   - LeaderIsolation                             ← deferred
//   - LocalAutonomyOpacity                        ← deferred
//
// Deferred terms remain RFC-080 §8 closure blockers documented in
// `docs/m6-closeout-audit.md`; no RFC-090 task numbers exist for
// them, and the M6 milestone REMAINS OPEN.
//
// What this PR deliberately does NOT do:
//
//   no save schema bump — `media_control` already exists on
//     CountryState since M2.16
//   no new state field
//   no per-event TrueValue source change (still 1.0 in the
//     event_firer consumer — flagged in audit doc)
//   no separate EventReport artefact (also flagged in audit doc)
//   no debug-mode change (RFC-090 §6.8 ships unchanged)
//   no RNG consumption (kept deterministic)
//   no change to kPlaceholderInformationAccuracy (the public
//     constant remains the "no-distortion ceiling" semantic —
//     `accuracy = 1.0` is now reached when intelligence is maxed
//     AND corruption is zero AND media_control is zero)
//
// Canonical-baseline impact: byte-identical for the canonical
// `1930_minimal` scenario because its events are tuned not to
// fire. The compliance `1930_rfc_compliance` scenario fires
// events; its `events.jsonl` numeric metadata values change but
// the integration test only checks key PRESENCE, not byte values.

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
    if (auto err = require_unit_ratio(
            c.id_code,
            "government_authority.media_control",
            c.government_authority.media_control)) {
        return *err;
    }

    // M6.6 intel pair: weighted combination of the two
    // intelligence inputs, in [0, 1]. The pair sums to 1 by
    // contract (kInformationAccuracyCapabilityWeight +
    // kInformationAccuracyBudgetWeight == 1.0).
    const double intel_score =
        kInformationAccuracyCapabilityWeight *
            c.government_authority.intelligence_capability +
        kInformationAccuracyBudgetWeight *
            c.budget.intelligence;

    // M6 closeout-audit MediaFreedomSignal: the complement of
    // government_authority.media_control. High state control of
    // the media yields low MediaFreedomSignal; free press yields
    // signal = 1. Applied as an OUTER weighted blend with the
    // intel pair so the inner pair-sum invariant is preserved.
    const double media_freedom =
        1.0 - c.government_authority.media_control;
    const double positive_axis =
        (1.0 - kInformationAccuracyMediaFreedomWeight) * intel_score +
        kInformationAccuracyMediaFreedomWeight         * media_freedom;
    const double base =
        kMinInformationAccuracy +
        (1.0 - kMinInformationAccuracy) * positive_axis;

    // M6.7 corruption subtraction (RFC-090 §6.7 `加入腐敗影響`):
    // subtract a corruption-weighted penalty from the positive-
    // axis baseline. With `kInformationAccuracyCorruptionWeight =
    // 0.4` and inputs validated to `[0, 1]`, the math is provably
    // in `[0.0, 1.0]`:
    //   - intel maxed + free media + zero corruption → 1.0
    //   - intel maxed + free media + max  corruption → 0.6
    //   - zero  intel + max  media-control + zero corruption → 0.4
    //   - zero  intel + max  media-control + max  corruption → 0.0
    const double accuracy =
        base -
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

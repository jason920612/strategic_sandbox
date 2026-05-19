// InformationAccuracy - deterministic read-only helper for
// computing how accurate the player's view of an event is in a
// given country.
//
// The body implements a stripped-down subset of RFC-080 §8's
// full accuracy formula:
//
//   M6.3 (`information_accuracy`)  — function shape; placeholder
//                                    body returned 1.0.
//   M6.6 (RFC-090 §6.6             — replaced the placeholder
//   `加入情報預算影響`)              with a weighted sum of
//                                    intelligence_capability +
//                                    budget.intelligence mapped
//                                    to `[0.4, 1.0]`.
//   M6.7 (RFC-090 §6.7             — subtract a corruption-
//   `加入腐敗影響`)                  weighted penalty from the
//                                    M6.6 baseline; range widens
//                                    to `[0.0, 1.0]`.
//
// Already-shipped M6 sub-milestones the helper composes with:
//
//   M6.1 (`true_cause`)         — author-written truth narrative
//                                 on EventDefinition.
//   M6.2 (`visible_report`)     — author-written public report
//                                 string on EventDefinition.
//   M6.4 (`reported_value::`    — pure double × double helper.
//   `from_true_value`)            Consumes accuracy to damp a
//                                 true numeric value toward 0.
//   M6.5 (`bias_noise::`        — pure deterministic-hash noise
//   `sample_for_event`)           primitive over event id /
//                                 country id / fire date /
//                                 amplitude. Does NOT consume
//                                 accuracy.
//
// Still-deferred RFC-090 §M6 sub-milestones:
//
//   §6.8 (`debug 模式顯示真相`) — debug bypass.
//   §6.9 (`非 debug 模式隱藏    — first downstream caller of
//   真相`)                        compute_for_country in normal
//                                 simulation; will compose M6.4
//                                 + M6.5 to hide visible_report
//                                 toward true_cause.
//
// M6.7 intentionally does NOT bump the save schema. The
// `country.corruption` field has been on CountryState since
// M1.1; M6.6's intelligence inputs since M2.16 / M1.3. No new
// persistent field ships.
//
// Semantics (after M6.7):
//
//   * Pure read.  `compute_for_country` never mutates state.
//   * Deterministic.  Same state + same country → same result.
//   * Range.  Result is in `[0.0, 1.0]`:
//       1.0 = player sees truth verbatim (no distortion).
//             Reached when intelligence is maxed AND corruption
//             is zero.
//       0.0 = full blackout. Reached when intelligence is zero
//             AND corruption is maxed.
//     `kMinInformationAccuracy = 0.4` is the floor of the M6.6
//     contribution alone (when corruption = 0); the M6.7
//     corruption subtraction can push the total below it.
//     The exact value is
//       accuracy = 0.4
//                + 0.6 × (0.7 × intelligence_capability
//                       + 0.3 × budget.intelligence)
//                - 0.4 × corruption
//   * Validation.  `country` must be a valid index into
//     `state.countries`. Each of the three ratio inputs
//     (`government_authority.intelligence_capability`,
//     `budget.intelligence`, `corruption`) must be a finite
//     value in `[0, 1]`. Any failure returns `Result::failure`
//     naming the offending CountryId or country `id_code` +
//     field. Per `feedback_no_silent_degradation`, out-of-range
//     and non-finite values are REJECTED rather than silently
//     clamped — clamping would convert a state-corruption bug
//     into an invisible degradation.
//
// M6.7 deliberate non-goals:
//
//   no save schema bump (still v18; M6.7 reuses existing fields)
//   no new state field
//   no debug mode (RFC-090 §6.8 scope)
//   no non-debug hiding consumer (RFC-090 §6.9 scope)
//   no EventReport type / artefact
//   no events.jsonl semantic change
//   no UI surface
//   no consumer in any current system —
//     `event_evaluator` / `event_firer` / `event_effects` /
//     `event_engine` / `monthly_pipeline` / `runner` all
//     unchanged; the helper is callable but no one calls it.
//   no per-event variant — `compute_for_event` is not part of
//     M6.7 scope.
//   no RNG consumption — the helper is RNG-free.
//   no allowlist drift — M6.7 reads exactly three CountryState
//     fields (intelligence_capability + budget.intelligence +
//     corruption).

#ifndef LEVIATHAN_SYSTEMS_INFORMATION_ACCURACY_HPP
#define LEVIATHAN_SYSTEMS_INFORMATION_ACCURACY_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::information_accuracy {

// "No-distortion ceiling" — the value `compute_for_country`
// returns when intelligence is maxed AND corruption is zero
// (M6.7). The semantic is stable across M6.3 / M6.6 / M6.7:
// 1.0 = the player sees the truth verbatim. M6.3 returned this
// unconditionally; M6.6 returned it only when both intelligence
// inputs were 1.0; M6.7 narrows further: corruption must also
// be 0.
inline constexpr double kPlaceholderInformationAccuracy = 1.0;

// M6.6 contribution floor — the value the M6.6 baseline term
// (`BaseAccuracy + IntelligenceCapacity` portion of RFC-080 §8)
// returns when both intelligence inputs are zero. M6.7's
// corruption subtraction can push the total accuracy below this
// floor; this constant is the floor of the M6.6 contribution
// alone (corresponds to RFC-080 §8's `BaseAccuracy` slot).
inline constexpr double kMinInformationAccuracy = 0.4;

// M6.6 weights for the two intelligence inputs. Sum = 1.0; a
// future M-driver may rebalance once additional RFC-080 §8
// positive terms (MediaFreedomSignal / BureaucraticProfessionalism
// / AuditCapacity) land.
inline constexpr double kInformationAccuracyCapabilityWeight = 0.7;
inline constexpr double kInformationAccuracyBudgetWeight     = 0.3;

// M6.7 corruption weight (RFC-080 §8 `-Corruption` term). The
// maximum corruption subtraction is symmetric to
// `kMinInformationAccuracy = 0.4` so a fully-corrupt country
// with zero intelligence reaches accuracy = 0 (full blackout).
// Exposed publicly so future RFC-080 §8 sub-milestones (each
// landing one more negative term: -FactionCapture /
// -LeaderIsolation / -LocalAutonomyOpacity) can be rebalanced
// against the M6.7 baseline.
inline constexpr double kInformationAccuracyCorruptionWeight = 0.4;

// Compute the player's information accuracy for events in the
// given country.
//
// Formula (M6.7, RFC-080 §8 subset):
//   intel_score = kInformationAccuracyCapabilityWeight
//                 × country.government_authority.intelligence_capability
//               + kInformationAccuracyBudgetWeight
//                 × country.budget.intelligence
//   m6_6_base   = kMinInformationAccuracy
//               + (1 - kMinInformationAccuracy) × intel_score
//   accuracy    = m6_6_base
//               - kInformationAccuracyCorruptionWeight
//                 × country.corruption
//
// Returns:
//   - `Result::success` with the computed accuracy in
//     `[0.0, 1.0]` for any valid country index whose three
//     ratio inputs (`intelligence_capability`,
//     `budget.intelligence`, `corruption`) are each finite
//     values in `[0, 1]`.
//   - `Result::failure` when `country` is not a valid index
//     into `state.countries` (error message includes the
//     offending `CountryId.value()`), or when any of the three
//     ratio inputs is non-finite or out of `[0, 1]` (error
//     message names the offending country `id_code`, the
//     offending field, and the offending numeric value).
//
// Per `feedback_no_silent_degradation`, out-of-range and
// non-finite inputs are rejected — never silently clamped.
//
// Pure / read-only: no GameState mutation, no logs, no time /
// RNG side effects. Deterministic: same inputs → same result.
core::Result<double> compute_for_country(
    const core::GameState& state,
    core::CountryId        country);

}  // namespace leviathan::systems::information_accuracy

#endif  // LEVIATHAN_SYSTEMS_INFORMATION_ACCURACY_HPP

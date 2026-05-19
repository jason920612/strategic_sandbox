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
//   M6 closeout audit              — add MediaFreedomSignal as
//   (RFC-080 §8 `+ MediaFreedom-      a positive contributor.
//   Signal`)                         Existing intel weights still
//                                    sum to 1; MediaFreedomSignal
//                                    is an outer weighted blend
//                                    (w_media on (1 - media_
//                                    control)) so the overall
//                                    accuracy ceiling stays at
//                                    1.0 and the floor stays at
//                                    0.0. M6 remains OPEN — this
//                                    is one of two representative
//                                    RFC-080 §8 residuals shipped
//                                    by the closeout-audit PR. See
//                                    docs/m6-closeout-audit.md.
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
//                                 Engine path consumes it via
//                                 event_firer (M6.9).
//   M6.5 (`bias_noise::`        — pure deterministic-hash noise
//   `sample_for_event`)           primitive over event id /
//                                 country id / fire date /
//                                 amplitude. Does NOT consume
//                                 accuracy.
//   M6.8 / M6.9                 — event_firer composes accuracy
//                                 with M6.4 / M6.5 to emit
//                                 publicText + distortion
//                                 metadata; debug-mode bypass
//                                 lives in `logging::export_jsonl`.
//
// M6 closeout audit adds MediaFreedomSignal to this helper and
// `propaganda_bias::compute_for_country` as a sibling helper
// (RFC-080 §8 `Bias = ... + PropagandaBias`). Both representative
// residuals share the same underlying field
// (`government_authority.media_control`); the helper here applies
// it as a positive accuracy contributor, and `propaganda_bias`
// applies the same field as a separate positive Bias term emitted
// by event_firer alongside the existing distortion fields.
//
// Still-deferred RFC-080 §8 terms (M6 closeout audit remaining
// blockers — none of these have RFC-090 task numbers, and M6
// REMAINS OPEN until they ship):
//
//   + MediaFreedomSignal          ← shipped here (closeout audit)
//   + BureaucraticProfessionalism ← deferred (no state field; see
//                                   docs/m6-closeout-audit.md
//                                   §5 / §9 for design)
//   + AuditCapacity               ← deferred (no state field)
//   - FactionCapture              ← deferred (needs FactionState
//                                   ↔ government link signal;
//                                   design in audit doc)
//   - LeaderIsolation             ← deferred (no state field)
//   - LocalAutonomyOpacity        ← deferred (inverse of
//                                   central_control under one
//                                   reading; flagged for design)
//
// M6 closeout audit reuses existing CountryState surface only:
// `corruption` / `government_authority.intelligence_capability`
// / `budget.intelligence` / `government_authority.media_control`.
// No new persistent field ships in this PR.
//
// Semantics (after the M6 closeout-audit MediaFreedomSignal
// extension):
//
//   * Pure read.  `compute_for_country` never mutates state.
//   * Deterministic.  Same state + same country → same result.
//   * Range.  Result is in `[0.0, 1.0]`:
//       1.0 = player sees truth verbatim (no distortion).
//             Reached when intelligence is maxed AND
//             corruption is zero AND media_control is zero
//             (fully free press).
//       0.0 = full blackout. Reached when intelligence is zero
//             AND corruption is maxed AND media_control is one
//             (no information leaks past state control).
//     `kMinInformationAccuracy = 0.4` is the floor of the
//     positive-axis contribution alone (when corruption = 0);
//     the corruption subtraction can push the total below it.
//     The exact value is
//       intel_score   = 0.7 × intelligence_capability
//                     + 0.3 × budget.intelligence
//       media_freedom = 1 - government_authority.media_control
//       positive_axis = (1 - 0.2) × intel_score
//                     + 0.2       × media_freedom
//       accuracy      = 0.4 + 0.6 × positive_axis
//                     - 0.4 × corruption
//   * Validation.  `country` must be a valid index into
//     `state.countries`. Each of the four ratio inputs
//     (`government_authority.intelligence_capability`,
//     `budget.intelligence`, `corruption`,
//     `government_authority.media_control`) must be a finite
//     value in `[0, 1]`. Any failure returns `Result::failure`
//     naming the offending CountryId or country `id_code` +
//     field. Per `feedback_no_silent_degradation`, out-of-range
//     and non-finite values are REJECTED rather than silently
//     clamped — clamping would convert a state-corruption bug
//     into an invisible degradation.
//
// M6 closeout-audit deliberate non-goals (MediaFreedomSignal
// extension):
//
//   no save schema bump (still v18; reuses existing
//     government_authority.media_control field, on CountryState
//     since M2.16)
//   no new state field
//   no per-event TrueValue source (still pinned at 1.0 in the
//     event_firer consumer — `docs/m6-closeout-audit.md` lists
//     this as an explicit remaining M6 closure blocker)
//   no separate EventReport artefact (also a remaining blocker
//     per the audit doc; designed but not implemented in this PR)
//   no debug-mode change (RFC-090 §6.8 ships unchanged)
//   no non-debug hiding consumer change beyond M6.9's metadata
//     surface (the engine consumer in event_firer continues to
//     emit `information_accuracy` / `reported_intensity` /
//     `noise_sample`; closeout-audit adds a sibling
//     `propaganda_bias_sample` key)
//   no RNG consumption — the helper stays RNG-free.
//   no rebalancing of intelligence-pair weights — the two
//     intelligence weights still sum to 1.0; MediaFreedomSignal
//     is composed as an OUTER blend on top of the intel score,
//     not as a third weight on the inner blend.

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

// M6 closeout-audit MediaFreedomSignal weight (RFC-080 §8
// `+ MediaFreedomSignal` positive term). Applied as an OUTER
// blend on top of the existing intel_score so the inner
// kInformationAccuracyCapabilityWeight +
// kInformationAccuracyBudgetWeight = 1.0 invariant is preserved.
//
// Game-model assumption (RFC-080 §1, §11). Grounding:
//   * V-Dem methodology decomposes regime quality along media
//     freedom + media bias axes; press freedom is treated as a
//     first-class component of state-information quality.
//   * Egorov, Guriev & Sonin (QJP 2009) "Why Resource-Poor
//     Dictators Allow Freer Media": rulers tolerate freer media
//     specifically as an information-aggregation channel —
//     constrained media is shown to materially degrade the
//     ruler's signal quality.
// Neither source pins the numeric weight; 0.20 is chosen so
// MediaFreedomSignal is a meaningful but secondary contributor
// (intel_score still dominates the positive axis at 0.80).
// Symmetric exposure: a future RFC-080 §8 residual that needs
// to renormalise can rebalance this constant against the intel
// pair.
inline constexpr double kInformationAccuracyMediaFreedomWeight = 0.20;

// Compute the player's information accuracy for events in the
// given country.
//
// Formula (M6 closeout-audit, RFC-080 §8 subset):
//   intel_score   = kInformationAccuracyCapabilityWeight
//                   × country.government_authority.intelligence_capability
//                 + kInformationAccuracyBudgetWeight
//                   × country.budget.intelligence
//   media_freedom = 1 - country.government_authority.media_control
//   positive_axis = (1 - kInformationAccuracyMediaFreedomWeight)
//                       × intel_score
//                 +       kInformationAccuracyMediaFreedomWeight
//                       × media_freedom
//   base          = kMinInformationAccuracy
//                 + (1 - kMinInformationAccuracy) × positive_axis
//   accuracy      = base
//                 - kInformationAccuracyCorruptionWeight
//                   × country.corruption
//
// Returns:
//   - `Result::success` with the computed accuracy in
//     `[0.0, 1.0]` for any valid country index whose four ratio
//     inputs (`intelligence_capability`, `budget.intelligence`,
//     `corruption`, `media_control`) are each finite values in
//     `[0, 1]`.
//   - `Result::failure` when `country` is not a valid index
//     into `state.countries` (error message includes the
//     offending `CountryId.value()`), or when any of the four
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

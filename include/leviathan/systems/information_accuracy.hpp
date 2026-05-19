// InformationAccuracy - deterministic read-only helper for
// computing how accurate the player's view of an event is in a
// given country.
//
// M6.3 shipped the function shape with a placeholder body that
// always returned `kPlaceholderInformationAccuracy = 1.0`
// (no-distortion ceiling). M6.6 replaces the body with the
// intelligence-budget formula RFC-090 §6.6 (`加入情報預算影響`)
// describes — a stripped-down subset of RFC-080 §8's full
// information-accuracy formula.
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
//   §6.7 (`加入腐敗影響`)        — corruption term on accuracy.
//   §6.8 (`debug 模式顯示真相`) — debug bypass.
//   §6.9 (`非 debug 模式隱藏    — first downstream caller of
//   真相`)                        compute_for_country in normal
//                                 simulation; will compose M6.4
//                                 + M6.5 to hide visible_report
//                                 toward true_cause.
//
// M6.6 intentionally does NOT bump the save schema. Both
// intelligence inputs (intelligence_capability + budget.intelligence)
// already exist on CountryState since M2.16 / M1.3. No new
// persistent field ships.
//
// Semantics (after M6.6):
//
//   * Pure read.  `compute_for_country` never mutates state.
//   * Deterministic.  Same state + same country → same result.
//   * Range.  Result is in `[kMinInformationAccuracy, 1.0]` =
//     `[0.4, 1.0]`:
//       1.0 = player sees truth verbatim (no distortion).
//       0.4 = zero intelligence floor — degraded but not blank.
//     The exact value is
//       accuracy = 0.4 + 0.6 × (0.7 × intelligence_capability
//                              + 0.3 × budget.intelligence)
//     Both inputs are clamped to [0, 1] defensively. M6.7 will
//     subtract a corruption term and may push effective accuracy
//     below 0.4 when corruption is high; M6.6 itself never goes
//     below kMinInformationAccuracy.
//   * Validation.  `country` must be a valid index into
//     `state.countries`; otherwise the helper returns
//     `Result::failure` with the offending CountryId in the
//     message. Finite out-of-range intelligence inputs are
//     clamped to [0, 1] defensively. Non-finite inputs
//     (NaN / ±Inf) are rejected with `Result::failure` so a
//     corrupted state surfaces instead of leaking a NaN
//     accuracy past the closed range.
//
// M6.6 deliberate non-goals:
//
//   no save schema bump (still v18; M6.6 reuses existing fields)
//   no new state field
//   no corruption formula (RFC-090 §6.7 scope)
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
//     M6.6 scope.
//   no RNG consumption — the helper is RNG-free.
//   no allowlist drift — M6.6 reads exactly two CountryState
//     fields (intelligence_capability + budget.intelligence).

#ifndef LEVIATHAN_SYSTEMS_INFORMATION_ACCURACY_HPP
#define LEVIATHAN_SYSTEMS_INFORMATION_ACCURACY_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::information_accuracy {

// "No-distortion ceiling" — the value `compute_for_country`
// returns for a country with maxed intelligence. The semantic
// is stable across M6.6 / M6.7: 1.0 = the player sees the truth
// verbatim. M6.3 returned this unconditionally; M6.6 returns
// it only when both intelligence inputs are at 1.0.
inline constexpr double kPlaceholderInformationAccuracy = 1.0;

// M6.6 accuracy floor — the value `compute_for_country` returns
// for a country with zero intelligence_capability AND zero
// budget.intelligence. A country with no intelligence apparatus
// still gets a degraded report rather than a complete blackout
// (M6.5's bias / noise primitive layers on top; collapsing this
// floor to 0 would over-attenuate the noise band's signal).
//
// Exposed publicly so tests + future M6.7 / M6.9 callers can pin
// the boundary without re-deriving it. M6.7's corruption term
// will subtract from accuracy AFTER this floor, so the effective
// final accuracy under high corruption may sit below the M6.6
// floor; this constant is the floor of the M6.6 contribution
// alone.
inline constexpr double kMinInformationAccuracy = 0.4;

// M6.6 weights for the two intelligence inputs in the
// `compute_for_country` formula. `intelligence_capability` is
// the more direct signal (current ability to produce accurate
// reports); `budget.intelligence` is the slower-moving funding
// input. Sum = 1.0; a future M-driver may rebalance.
inline constexpr double kInformationAccuracyCapabilityWeight = 0.7;
inline constexpr double kInformationAccuracyBudgetWeight     = 0.3;

// Compute the player's information accuracy for events in the
// given country, in `[kMinInformationAccuracy, 1.0]` =
// `[0.4, 1.0]` (M6.6).
//
// Formula:
//   intel_score = kInformationAccuracyCapabilityWeight
//                 × country.government_authority.intelligence_capability
//               + kInformationAccuracyBudgetWeight
//                 × country.budget.intelligence
//   accuracy   = kMinInformationAccuracy
//              + (1 - kMinInformationAccuracy) × intel_score
//
// Returns:
//   - `Result::success` with the computed accuracy for any
//     valid country index whose intelligence inputs are finite.
//     The value is in `[kMinInformationAccuracy, 1.0]`
//     (= [0.4, 1.0]).
//   - `Result::failure` when `country` is not a valid index
//     into `state.countries` (error message includes the
//     offending `CountryId.value()`), or when
//     `government_authority.intelligence_capability` or
//     `budget.intelligence` is non-finite (NaN / ±Inf; error
//     message names the offending country `id_code` and the
//     offending field).
//
// Pure / read-only: no GameState mutation, no logs, no time /
// RNG side effects. Deterministic: same inputs → same result.
core::Result<double> compute_for_country(
    const core::GameState& state,
    core::CountryId        country);

}  // namespace leviathan::systems::information_accuracy

#endif  // LEVIATHAN_SYSTEMS_INFORMATION_ACCURACY_HPP

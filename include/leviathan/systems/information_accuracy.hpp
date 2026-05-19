// InformationAccuracy - deterministic read-only helper for
// computing how accurate the player's view of an event is in a
// given country.
//
// M6.3 shipped the function shape with a placeholder body that
// always returned `kPlaceholderInformationAccuracy = 1.0`
// (no-distortion ceiling). M6.6 replaces the body with the
// intelligence-budget formula RFC-090 §6.6 (`加入情報預算影響`)
// describes. Subsequent M6 sub-milestones continue the chain:
//
//   M6.4 reported value         — uses accuracy to derive the
//                                 numeric reported value of an
//                                 event's trigger / effect.
//   M6.5 bias / noise           — adds deterministic-hash noise
//                                 on top of the accuracy gate.
//   M6.6 intelligence budget    — SHIPPED. body reads
//                                 government_authority
//                                 .intelligence_capability and
//                                 budget.intelligence; lowers
//                                 accuracy toward
//                                 `kMinInformationAccuracy = 0.4`
//                                 when both are zero.
//   M6.7 corruption             — will add a -corruption term on
//                                 top of the M6.6 baseline.
//   M6.8 debug mode             — will bypass accuracy entirely.
//   M6.9 non-debug mode         — first downstream caller: will
//                                 consume accuracy through
//                                 reported_value::from_true_value
//                                 + bias_noise::sample_for_event
//                                 to hide / distort the
//                                 visible_report (M6.2) toward
//                                 the true_cause (M6.1).
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
//     message.
//
// M6.6 deliberate non-goals (forward-stable):
//
//   no save schema bump (still v18; M6.6 reuses existing fields)
//   no new state field
//   no corruption formula (M6.7)
//   no debug mode (M6.8)
//   no non-debug hiding consumer (M6.9 will wire it)
//   no EventReport type / artefact
//   no events.jsonl semantic change
//   no UI surface
//   no consumer in any current system —
//     `event_evaluator` / `event_firer` / `event_effects` /
//     `event_engine` / `monthly_pipeline` / `runner` all
//     unchanged; the helper is callable but no one calls it
//     yet (M6.9 will be the first caller).
//   no per-event variant — `compute_for_event` is still
//     deferred (M6.9 will resolve the actor's country from
//     `instance.actors.front().country_id_code` and delegate
//     to `compute_for_country`).
//   no RNG consumption — the helper is RNG-free; bias / noise
//     belongs to M6.5 which is layered on top of accuracy, not
//     baked into it.
//   no allowlist drift — M6.6 reads exactly two CountryState
//     fields (intelligence_capability + budget.intelligence).
//     M6.7 will add one more read (corruption); the helper
//     body never reads beyond what the active sub-milestone
//     formula needs.

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
//     valid country index. The value is in
//     `[kMinInformationAccuracy, 1.0]` (= [0.4, 1.0]).
//   - `Result::failure` when `country` is not a valid index
//     into `state.countries`. Error message includes the
//     offending `CountryId.value()`.
//
// Pure / read-only: no GameState mutation, no logs, no time /
// RNG side effects. Deterministic: same inputs → same result.
core::Result<double> compute_for_country(
    const core::GameState& state,
    core::CountryId        country);

}  // namespace leviathan::systems::information_accuracy

#endif  // LEVIATHAN_SYSTEMS_INFORMATION_ACCURACY_HPP

// InformationAccuracy - deterministic read-only helper for
// computing how accurate the player's view of an event is in a
// given country.
//
// M6.3 ships the SKELETON of RFC-090 §6.3 (`6.3 實作
// information_accuracy`): a pure free-function helper that
// returns a `[0, 1]` accuracy value. M6.3 itself is a
// **placeholder formula** — it always returns 1.0
// (`kPlaceholderInformationAccuracy` = player sees the truth
// verbatim, no distortion). Later M6 sub-milestones add the
// actual distortion model:
//
//   M6.4 reported value         — uses this accuracy to derive
//                                 the numeric reported value of
//                                 an event's trigger / effect.
//   M6.5 bias / noise           — adds randomised distortion on
//                                 top of the accuracy gate.
//   M6.6 intelligence budget    — weights accuracy by the
//                                 country's intelligence
//                                 budget (lower budget ->
//                                 lower accuracy).
//   M6.7 corruption             — weights accuracy by the
//                                 country's corruption (higher
//                                 corruption -> lower accuracy).
//   M6.8 debug mode             — bypasses accuracy entirely
//                                 (always show the truth).
//   M6.9 non-debug mode         — uses the computed accuracy
//                                 to hide / distort the
//                                 visible_report (M6.2) toward
//                                 the true_cause (M6.1).
//
// M6.3 intentionally does NOT bump the save schema. Per the PR
// #101 reviewer's note (preserved here so future-me doesn't
// drift): *"M6.3 要做 information_accuracy 時，建議不要再 bump
// save schema，除非真的新增 persistent field"*. The helper
// reads existing GameState; no new field on GameState /
// CountryState / EventDefinition / EventInstance ships. M6.6
// will introduce an intelligence_budget field (or reuse an
// existing one) and may bump the save format at that time;
// M6.7 similarly for corruption coupling if needed.
//
// Semantics:
//
//   * Pure read.  `compute_for_country` never mutates state.
//   * Deterministic.  Same state + same country → same result.
//   * Range.  Result is in `[0, 1]`:
//       1.0 = player sees truth verbatim (no distortion).
//       0.0 = player sees no useful info (full distortion).
//     M6.3 always returns 1.0; later sub-milestones bring the
//     value below 1.0.
//   * Validation.  `country` must be a valid index into
//     `state.countries`; otherwise the helper returns
//     `Result::failure` with the offending CountryId in the
//     message. The country's actual field values are not
//     consulted in the M6.3 placeholder; future formulas
//     (M6.6 / M6.7) will read `state.countries[country].*`
//     fields.
//
// M6.3 deliberate non-goals:
//
//   no save schema bump (still v16)
//   no new state field
//   no reported value (M6.4)
//   no bias / noise (M6.5)
//   no intelligence-budget formula (M6.6)
//   no corruption formula (M6.7)
//   no debug mode (M6.8)
//   no non-debug hiding (M6.9)
//   no EventReport type / artefact
//   no events.jsonl semantic change
//   no UI surface
//   no consumer in any current system —
//     `event_evaluator` / `event_firer` / `event_effects` /
//     `event_engine` / `monthly_pipeline` / `runner` all
//     unchanged; the helper is callable but no one calls it
//     yet (M6.4+ will).
//   no per-event variant — `compute_for_event` is M6.4 scope
//     (it'll resolve the country from
//     `instance.actors.front().country_id_code` and delegate
//     to `compute_for_country`).
//   no allowlist of country fields the future formulas can
//     read — that's a design decision for M6.6 / M6.7.

#ifndef LEVIATHAN_SYSTEMS_INFORMATION_ACCURACY_HPP
#define LEVIATHAN_SYSTEMS_INFORMATION_ACCURACY_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::information_accuracy {

// M6.3 placeholder return value. Exposed so tests and any
// future consumer can grep for the constant rather than the
// magic number; M6.6 / M6.7 will replace the body of
// `compute_for_country` so this constant becomes the
// "no-distortion ceiling" rather than the always-returned
// value.
inline constexpr double kPlaceholderInformationAccuracy = 1.0;

// Compute the player's information accuracy for events in the
// given country, in `[0, 1]`.
//
// Returns:
//   - `Result::success` with `kPlaceholderInformationAccuracy`
//     (= 1.0) for any valid country index. M6.6 / M6.7 will
//     replace this body with the actual formula.
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

// ReportedValue - deterministic read-only helper for computing
// what numeric value the player sees for an event's trigger /
// effect, given a true value and an information accuracy.
//
// M6.4 ships the SKELETON of RFC-090 §6.4 (`6.4 實作 reported
// value`): a pure free function that converts
// `(true_value, accuracy) -> reported_value`. M6.4 itself is a
// **placeholder formula**:
//
//     reported_value = true_value * accuracy
//
// At `accuracy = 1.0` (the M6.3 `kPlaceholderInformationAccuracy`
// ceiling), reported equals true_value verbatim — the player
// sees the truth. At `accuracy = 0.0`, reported equals 0 — the
// player sees nothing useful. Intermediate values linearly
// interpolate between truth and nothing.
//
// Composition with the M6 pipeline (per RFC-090 §M6):
//
//   M6.3 information_accuracy::compute_for_country
//        -> double accuracy in [0, 1]
//
//   M6.4 (this module) reported_value::from_true_value(
//          true_value, accuracy)
//        -> double reported_value
//
//   M6.5 bias / noise   — adds randomised distortion on top of
//                         this skeleton output. The formula
//                         body of M6.4 stays the linear path;
//                         M6.5 introduces RNG and wraps this
//                         function (it does NOT change this
//                         function's body).
//   M6.6 intelligence budget — reduces M6.3's accuracy below
//                         1.0; M6.4's body is unaffected.
//   M6.7 corruption     — reduces M6.3's accuracy below 1.0;
//                         M6.4's body is unaffected.
//   M6.8 debug mode     — bypasses M6.4 entirely (player sees
//                         true_cause / true_value verbatim).
//   M6.9 non-debug mode — uses the M6.4 result to hide
//                         visible_report toward true_cause.
//
// The reviewer for PR #102 (M6.3) noted:
// *"README 說「M6.6 will replace this body with intelligence
//   budget; M6.7 corruption」，但 RFC-090 的 6.4 reported value
//   會先 consume helper。這不是問題，因為 PR body/README 也有說
//   M6.4 會 consume resulting accuracy；只是 M6.4 應避免改
//   formula body."*
//
// M6.4 honours that: it CONSUMES `information_accuracy::
// compute_for_country` in tests + design but does NOT modify
// its body. The M6.3 helper's placeholder body (return 1.0)
// is untouched.
//
// Semantics:
//
//   * Pure free function.  No GameState parameter; the
//     function operates on two doubles and returns one.
//     Future M6.x consumers will wrap this with the country-
//     specific accuracy from M6.3.
//   * Deterministic.  Same `(true_value, accuracy)` -> same
//     result.
//   * Range.  `accuracy` must be a finite double in `[0, 1]`;
//     `true_value` must be a finite double; otherwise the
//     helper returns `Result::failure`. Result range is
//     `[min(true_value, 0), max(true_value, 0)]` (truth ⇄ 0
//     linear interpolation).
//   * No RNG.  M6.4 is deterministic; M6.5 introduces the RNG
//     path on top.
//   * No clamping of `true_value`.  Negative true_values
//     (e.g. an effect's `-0.02`) flow through correctly and
//     report `-0.01` at accuracy 0.5 — exactly what an
//     effect-magnitude consumer expects.
//
// M6.4 deliberate non-goals:
//
//   no save schema bump (still v16)
//   no new state field
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
//     yet (M6.5+ / M6.9 will).
//   no per-event variant — `from_true_value` is the
//     primitive; a future `from_event` that resolves the
//     country + the trigger / effect value automatically
//     belongs alongside the M6.9 hiding pipeline.
//   no change to M6.3 `information_accuracy::compute_for_country`
//     body (the M6.3 placeholder stays at 1.0; only the
//     M6.4 helper is added).
//   no allowlist of what kinds of `true_value` are passed —
//     trigger thresholds, effect magnitudes, raw stat
//     percentages all share the same numeric shape.

#ifndef LEVIATHAN_SYSTEMS_REPORTED_VALUE_HPP
#define LEVIATHAN_SYSTEMS_REPORTED_VALUE_HPP

#include "leviathan/core/result.hpp"

namespace leviathan::systems::reported_value {

// Convert a `(true_value, accuracy)` pair into the value the
// player sees. See header doc for the placeholder formula
// (linear interpolation toward 0 by the accuracy weight), the
// composition with M6.3 / M6.5+ / M6.9, and the validation
// rules.
//
// Returns:
//   - `Result::success(true_value * accuracy)` on valid input.
//   - `Result::failure` when:
//       * `true_value` is not finite (NaN / +-Inf).
//       * `accuracy` is not finite, or is outside `[0, 1]`.
//     Error messages include the offending value.
//
// Pure / read-only: no side effects.
core::Result<double> from_true_value(double true_value,
                                     double accuracy);

}  // namespace leviathan::systems::reported_value

#endif  // LEVIATHAN_SYSTEMS_REPORTED_VALUE_HPP

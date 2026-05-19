// PropagandaBias - deterministic read-only helper for the
// RFC-080 §8 Bias term `+ PropagandaBias`.
//
// One of two representative RFC-080 §8 residuals shipped by the
// M6 closeout-audit PR (`docs/m6-closeout-audit.md`). The other
// is the MediaFreedomSignal accuracy contributor in
// `information_accuracy::compute_for_country`. Both read the same
// CountryState field (`government_authority.media_control`) — the
// closeout audit deliberately picked the most rigorously
// supportable residuals first, where "rigorously" means the
// formula reads a CountryState field that already exists, has
// been validated as a `[0, 1]` ratio at load, and maps to a
// documented research direction without inventing defaults.
//
// RFC-080 §8 strict semantics:
//
//   ReportedValue = TrueValue + Bias + Noise
//   Bias          = FactionInterestBias
//                 + BureaucraticSelfProtection
//                 + PropagandaBias              ← this module
//   Noise         = RandomNormal(0, 1 - InformationAccuracy)
//
// The M6.9 event_firer composes the M6.4 placeholder
// `reported_value::from_true_value(TrueValue, accuracy)` (a
// multiplicative damp toward 0) with the M6.5 `bias_noise`
// sample and emits both. The M6 closeout audit adds
// `propaganda_bias::compute_for_country` so the firer can ALSO
// emit a `propaganda_bias_sample` metadata key alongside the
// existing distortion fields. The full RFC-080 §8 additive
// composition (TrueValue + Bias + Noise) is not yet wired
// through the firer — see the audit doc for the
// `perceived_intensity` / EventReport-artefact follow-up design.
//
// Formula:
//
//   propaganda_bias = kPropagandaBiasMaxMagnitude
//                   × country.government_authority.media_control
//                   ∈ [0.0, kPropagandaBiasMaxMagnitude]
//
// Sign: positive. Propaganda systematically inflates the
// state-favoured narrative — under high media control the
// player's reported intensity skews UPWARD toward the regime's
// preferred framing. The polarity matches the conventional
// "good news is loud, bad news is missing" propaganda
// asymmetry in the cited literature (Egorov, Guriev & Sonin
// QJP 2009; King, Pan & Roberts APSR 2013; V-Dem propaganda
// indicators).
//
// Magnitude (`kPropagandaBiasMaxMagnitude = 0.3`): a
// game-model assumption per RFC-080 §1 / §11. The literature
// quantifies media bias and propaganda effects in many forms
// (e.g. DellaVigna & Kaplan 2007 estimated Fox News' effect
// on US Republican vote share at +0.4 to +0.7 pp), but none
// of the cited papers pin a numeric Bias coefficient for a
// game model. 0.3 keeps PropagandaBias comparable in
// magnitude to the M6.5 noise envelope (which ranges over
// `[-1.0, +1.0]` at minimum accuracy), so propaganda is a
// meaningful but secondary distortion source — not a runaway
// override.
//
// Validation:
//
//   * `country` must be a valid index into `state.countries`.
//   * `country.government_authority.media_control` must be a
//     finite value in `[0, 1]`. Per
//     `feedback_no_silent_degradation`, out-of-range / non-
//     finite inputs are rejected; never silently clamped.
//
// Purity:
//
//   * Pure read.  No GameState mutation.
//   * Deterministic.  Same state + same country → same result.
//   * No RNG consumption.
//   * No new persistent field; reuses M2.16
//     GovernmentAuthorityState.media_control.

#ifndef LEVIATHAN_SYSTEMS_PROPAGANDA_BIAS_HPP
#define LEVIATHAN_SYSTEMS_PROPAGANDA_BIAS_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::propaganda_bias {

// Maximum PropagandaBias magnitude. Reached when
// `government_authority.media_control == 1.0` (full state
// suppression of independent media). At media_control = 0
// (free press) the bias is 0.
//
// Game-model assumption (RFC-080 §1, §11). Not derived from a
// specific paper coefficient; see header comment for cited
// directional grounding (Egorov-Guriev-Sonin QJP 2009; King-
// Pan-Roberts APSR 2013; V-Dem propaganda indicators).
inline constexpr double kPropagandaBiasMaxMagnitude = 0.3;

// Compute the PropagandaBias contribution for events in the
// given country. See header comment for the formula and the
// citation chain.
//
// Returns:
//   - `Result::success` with the computed bias in
//     `[0.0, kPropagandaBiasMaxMagnitude]` for any valid country
//     index whose `media_control` ratio is a finite value in
//     `[0, 1]`.
//   - `Result::failure` when `country` is not a valid index
//     into `state.countries` (error message includes the
//     offending `CountryId.value()`), or when `media_control`
//     is non-finite or out of `[0, 1]` (error message names
//     the offending country `id_code`, the field, and the
//     offending numeric value).
//
// Per `feedback_no_silent_degradation`, out-of-range and
// non-finite inputs are rejected — never silently clamped.
//
// Pure / read-only: no GameState mutation, no logs, no time /
// RNG side effects. Deterministic: same inputs → same result.
core::Result<double> compute_for_country(
    const core::GameState& state,
    core::CountryId        country);

}  // namespace leviathan::systems::propaganda_bias

#endif  // LEVIATHAN_SYSTEMS_PROPAGANDA_BIAS_HPP

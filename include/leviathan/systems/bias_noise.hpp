// BiasNoise - deterministic hash-based noise helper for the M6
// reported-value pipeline.
//
// M6.5 ships the SKELETON of RFC-090 §6.5 (`6.5 實作 bias/noise`):
// a pure free function that produces a deterministic noise value
// for a fired event, given a stable identifier triple (event
// id_code + country id_code + fire date) and a noise amplitude.
//
// Design choice (per PR #103 reviewer call): **deterministic
// hash-based noise, NOT `state.rng` consumption.** The reviewer's
// note:
//
//   *"我會偏好 M6.5 先做 deterministic hash noise helper，不要
//   消耗 state.rng，除非你明確想讓資訊失真進入 simulation RNG
//   stream."*
//
// Reasoning (preserved for future-me):
//
//   * `state.rng` is the only mutable RNG state. If M6.5
//     consumed it, every event fire would advance
//     `state.rng.counter`, shifting all downstream RNG draws.
//     That perturbs M5 / M1.17 / M2 / M3 / M4 byte-identical
//     determinism baselines.
//   * Deterministic hash on stable inputs (event id, country
//     id, fire date) is purely a function of inputs known at
//     fire time. Same inputs → same noise; no global state.
//     Two consecutive months with identical fire conditions
//     produce identical noise — which is the right semantics
//     for "the player's report would have looked the same way
//     last month".
//
// M6.5 still ships **placeholder amplitude = 0.0**, which means
// the helper returns 0 by default. Callers that opt into noise
// pass a positive amplitude explicitly. The default keeps
// `event_history`-touching tests + canonical-no-fire byte-stable
// even after M6.5 lands; M6.9 (non-debug hiding) will be the
// first caller to pass a non-zero amplitude.
//
// Composition with the M6 pipeline (per RFC-090 §M6):
//
//   M6.3 information_accuracy::compute_for_country
//        -> accuracy in [0, 1]
//
//   M6.4 reported_value::from_true_value(true_value, accuracy)
//        -> linear-interpolated reported
//
//   M6.5 bias_noise::sample_for_event(event_id, country_id,
//                                     fired_on, amplitude)
//        -> noise in [-amplitude, +amplitude]
//
//   final_visible = M6.4 reported + M6.5 noise        (M6.9)
//
//   M6.5 itself never combines with M6.4; the caller (M6.9)
//   does the sum. M6.5 is the pure noise primitive.
//
// M6.5 deliberate non-goals (carried forward from the M6 line):
//
//   no save schema bump (still v16)
//   no new state field
//   no `state.rng` consumption (the load-bearing decision)
//   no consumer in any current system —
//     `event_evaluator` / `event_firer` / `event_effects` /
//     `event_engine` / `monthly_pipeline` / `runner` all
//     unchanged; the helper is callable but no one calls it
//     yet (M6.9 will).
//   no event-aware variant (`sample_for_event_instance` that
//     resolves country_id_code from the instance's first
//     actor) — that's M6.9 scope.
//   no automatic composition with M6.4 reported value — M6.9
//     owns the `reported + noise` sum.
//   no intelligence-budget weighting (M6.6) — M6.6 modulates
//     the upstream M6.3 accuracy, not M6.5 amplitude.
//   no corruption weighting (M6.7) — same as M6.6.
//   no debug mode (M6.8)
//   no non-debug hiding (M6.9)
//   no events.jsonl semantic change
//   no UI surface
//   no rebake of M1.17 / M2 / M3 / M4 / M5 byte-identical
//     determinism baselines (no consumer wired; canonical
//     runs still emit `event_history: []`).

#ifndef LEVIATHAN_SYSTEMS_BIAS_NOISE_HPP
#define LEVIATHAN_SYSTEMS_BIAS_NOISE_HPP

#include <string>

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::bias_noise {

// Public amplitude knob. M6.5 ships placeholder = 0.0 (no
// noise applied). Callers that want noise pass a positive
// amplitude in `[0, 1]` explicitly. Future M6.x sub-milestones
// will NOT modify this constant — they modulate the M6.3
// accuracy upstream, not this amplitude.
inline constexpr double kPlaceholderNoiseAmplitude = 0.0;

// Sample a deterministic noise value for a fired event, in
// `[-amplitude, +amplitude]`.
//
// Same `(event_id_code, country_id_code, fired_on, amplitude)`
// always produces the same noise. `state.rng` is NEVER
// consumed.
//
// Returns:
//   - `Result::success(0.0)` when `amplitude == 0.0` (the
//     placeholder default).
//   - `Result::success(noise)` with `noise` in
//     `[-amplitude, +amplitude]` for valid inputs and
//     `amplitude > 0`.
//   - `Result::failure` when:
//       * `event_id_code` or `country_id_code` is empty.
//       * `amplitude` is not finite (NaN / ±∞).
//       * `amplitude` is outside `[0, 1]`.
//     Error messages include the offending value.
//
// Pure / read-only: no GameState parameter, no state.rng
// consumption, no logs, no time-system side effects.
// Deterministic across builds: the underlying hash is
// FNV-1a + splitmix64 finalize, both standardised
// integer-arithmetic primitives (no dependency on std::hash
// which is compiler-implementation-defined).
core::Result<double> sample_for_event(
    const std::string&     event_id_code,
    const std::string&     country_id_code,
    const core::GameDate&  fired_on,
    double                 amplitude = kPlaceholderNoiseAmplitude);

}  // namespace leviathan::systems::bias_noise

#endif  // LEVIATHAN_SYSTEMS_BIAS_NOISE_HPP

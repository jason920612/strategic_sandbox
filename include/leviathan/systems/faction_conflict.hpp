// FactionConflict — M7.4 (RFC-090 §7.4 `加入派系衝突`,
// RFC-020 §8 `派系鬥爭`).
//
// Implements the first observable inter-faction dynamic for
// `state.factions`: opposing-type faction pairs that exist in
// the SAME country with non-trivial influence apply mutual
// radicalism pressure on each other. The pressure is a small
// asymptotic-add per monthly tick so the simulation does not
// runaway.
//
// RFC-020 §8 enumerates five concrete rivalry pairs (the
// allowlist baked into this module):
//
//   軍方 ↔ 情報部門           (military ↔ intelligence)
//   工會 ↔ 技術菁英            (workers ↔ technical_elites)
//   中央官僚 ↔ 地方勢力        (bureaucracy ↔ local_elites)
//   學生 ↔ 宗教勢力            (students ↔ religious)
//   媒體 ↔ 情報部門            (media ↔ intelligence)
//
// Note that intelligence appears in TWO pairs (versus
// military, and versus media) — RFC-020 §8's literal
// enumeration is preserved here without simplification.
//
// Mechanics (one monthly tick):
//
//   for each country in state.countries:
//     for each rivalry pair (type_a, type_b) in the §8 allowlist:
//       collect all factions of type_a in the country whose
//         influence > kFactionConflictInfluenceThreshold
//       collect all factions of type_b in the country whose
//         influence > kFactionConflictInfluenceThreshold
//       if both collections are non-empty:
//         for every faction in either collection:
//           radicalism += (1 - radicalism) ×
//                         kFactionConflictAsymptoticRadicalismDelta
//
// The asymptotic-`add` shape matches the post-PR #115
// hardening convention for ratio fields — radicalism cannot
// be pushed above 1.0 by repeated application.
//
// Game-model coefficients (RFC-080 §1, §11):
//
//   * `kFactionConflictInfluenceThreshold = 0.30`
//     Direction: RFC-020 §8 says conflicts BETWEEN factions,
//     not unilateral grievance; a faction needs non-trivial
//     political weight to participate in an active rivalry.
//     0.30 is a game-model floor below which the rivalry is
//     dormant.
//   * `kFactionConflictAsymptoticRadicalismDelta = 0.01`
//     Direction: Collier-Hoeffler / Alesina-Perotti establish
//     that elite conflict raises grievance and destabilises
//     the regime. 0.01 per monthly tick is a deliberately
//     small pressure that compounds over many months but does
//     not produce a single-month explosion. Coefficient
//     magnitude: pure game-model.
//
// What M7.4 deliberately DOES NOT do:
//
//   * No persistent FactionConflict struct on GameState. The
//     rivalry pairs are enumerated in code (this header);
//     state has only the existing FactionState surface. A
//     future sub-milestone may add a persistent rivalry-state
//     record (per-pair intensity, history, etc.) but RFC-020
//     §8 itself does not call for that.
//   * No save schema bump.
//   * No `state.rng` consumption.
//   * No new player-facing command.
//   * No new artefact.
//   * No effect on faction LOYALTY or SUPPORT — RFC-020 §8 is
//     strictly about rivalry → radicalism pressure. A future
//     sub-milestone can extend the mechanics if RFC anchors
//     authorise it.

#ifndef LEVIATHAN_SYSTEMS_FACTION_CONFLICT_HPP
#define LEVIATHAN_SYSTEMS_FACTION_CONFLICT_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::faction_conflict {

// Game-model coefficients (RFC-080 §1, §11). See header
// comment for direction grounding + assumption disclosure.
inline constexpr double kFactionConflictInfluenceThreshold        = 0.30;
inline constexpr double kFactionConflictAsymptoticRadicalismDelta = 0.01;

// Outcome of one `tick_apply_pressure` call.
//
//   pairs_active     — number of (country, rivalry-pair)
//                      combinations where both sides crossed
//                      the influence threshold.
//   factions_drifted — count of faction radicalism drifts
//                      applied (one faction may participate in
//                      multiple rivalry pairs in a single tick
//                      and is counted once PER pair it
//                      participates in).
struct PressureOutcome {
    int pairs_active     = 0;
    int factions_drifted = 0;
};

// Walk state.factions per country; for each RFC-020 §8
// rivalry pair where BOTH sides have at least one faction in
// the country with influence > kFactionConflictInfluence-
// Threshold, apply asymptotic radicalism+ on every
// participating faction.
//
// Strict validation (per `feedback_no_silent_degradation` +
// `feedback_api_signature_expresses_failure`):
//
//   * For every faction that participates (i.e., whose `type`
//     matches one side of a §8 rivalry pair AND whose
//     `country_id_code` resolves to a country with a rival on
//     the other side), `radicalism` and `loyalty` are
//     re-validated as finite ratios in `[0, 1]`. Non-finite
//     or out-of-range inputs are rejected loudly with the
//     faction's `id_code` named.
//   * The post-drift radicalism candidate is verified in
//     `[0, 1]` (asymptotic-add form guarantees this when
//     inputs are valid; the check guards against silent
//     corruption).
//   * Factions outside the §8 allowlist (and factions inside
//     the allowlist whose country has no opposing-type
//     faction at threshold) are SKIPPED with no validation —
//     M1.6 `faction::react` owns the per-tick numerical
//     validation of every faction's state; M7.4 only adds an
//     additional check on the factions it will mutate.
//
// Atomicity:
//
//   * Walks state.countries in vector order; for each
//     country, walks the rivalry-pair allowlist in source
//     order; within each pair, walks state.factions in
//     vector order. Deterministic across runs.
//   * Candidate-validate-commit: every candidate is computed
//     and validated first; on any failure the function
//     returns `Result::failure` BEFORE any mutation.
//   * No `state.rng` consumption.
core::Result<PressureOutcome>
tick_apply_pressure(core::GameState& state);

}  // namespace leviathan::systems::faction_conflict

#endif  // LEVIATHAN_SYSTEMS_FACTION_CONFLICT_HPP

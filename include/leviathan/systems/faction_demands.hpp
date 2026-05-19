// FactionDemands — M7.1 (RFC-090 §7.1 `加入派系要求`, RFC-020 §7
// `派系主動要求`).
//
// Implements the FIRST observable lifecycle for `state.faction_demands`:
//
//   1. `tick_generate(state, current_date)`
//      Per RFC-020 §7, a faction "actively makes a demand". M7.1
//      reads this as: when a faction's `radicalism` rises above
//      `kFactionDemandGenerateRadicalismThreshold` AND the faction
//      type matches one of the six RFC-020 §7 examples AND no
//      Pending demand of that kind currently exists for the
//      faction, append a new `FactionDemand{kind, status=Pending,
//      created_on=current_date,
//      expires_on=current_date + kFactionDemandLifetimeDays}` to
//      `state.faction_demands`. The generator is deterministic,
//      RNG-free, and produces a stable `id_code` of shape
//      `{faction_id_code}_demand_{kind_snake}_{created_on}`.
//
//   2. `tick_expire_and_apply(state, current_date)`
//      For every Pending demand whose `expires_on <= current_date`:
//      flip status to Expired AND apply an asymptotic-`add` drift
//      to the issuing faction's `radicalism` (positive delta
//      `kFactionDemandExpireRadicalismAsymptoticDelta`) and a
//      symmetric asymptotic-subtract on `loyalty`
//      (`-kFactionDemandExpireLoyaltyAsymptoticDelta`). The drift
//      reflects RFC-020 §7's premise that an unaddressed demand
//      compounds factional discontent; the asymptotic form
//      matches the post-PR #115 hardening rule for ratio fields.
//      Expired demands stay in the vector as an audit trail; they
//      do NOT regenerate effects on subsequent ticks (the
//      `Expired` flip is the gate).
//
// Wiring (M7.1):
//
//   `monthly::tick_all_countries` runs `tick_generate` BEFORE
//   `tick_expire_and_apply`, both placed AFTER the M3.4 authority
//   pressure step and BEFORE the M5.8 event tick step. The
//   ordering rationale:
//
//     * Generation runs first so a freshly radicalised faction
//       (post-M1.6 `faction::react` + post-M3.x interest-group
//       loop) gets its demand recorded in the SAME tick the
//       radicalism crossed the threshold.
//
//     * Expiration runs immediately after generation so the
//       audit-trail walk doesn't skip a tick. With M7.1's 60-day
//       lifetime, no demand created this tick can also expire
//       this tick (`expires_on > current_date` by construction).
//
//     * Event tick reads the updated radicalism so an event
//       triggered by `interest_group.radicalism > X` (or future
//       faction-radicalism triggers) sees the expiration drift.
//
// Game-model coefficients (RFC-080 §1, §11):
//
//   * `kFactionDemandGenerateRadicalismThreshold = 0.50`
//     Direction grounded in Collier & Hoeffler "Greed and
//     Grievance in Civil War" (grievance-opportunity model)
//     and Alesina & Perotti "Income Distribution, Political
//     Instability, and Investment" (factions with elevated
//     discontent become politically active). Neither source
//     pins a numeric activation threshold; 0.5 is a game-model
//     midpoint chosen so a moderate faction (radicalism in the
//     canonical 0.10 – 0.35 range) does NOT generate demands,
//     and a faction whose radicalism has drifted to the upper
//     half DOES.
//
//   * `kFactionDemandLifetimeDays = 60`
//     RFC-020 §7 specifies neither a timeline nor a satisfaction
//     window. 60 days = approximately two monthly ticks, long
//     enough that a player has time to enact a policy response
//     (when a future sub-milestone adds the response surface)
//     but short enough that expired demands surface visible
//     radicalism drift on a multi-year sim. Game-model.
//
//   * `kFactionDemandExpireRadicalismAsymptoticDelta = 0.05`
//     Positive asymptotic-add (Alesina-Perotti direction:
//     unaddressed grievance → instability). The asymptotic form
//     matches the post-PR #115 hardening convention so a
//     long-running sim cannot push radicalism past 1.0.
//     Coefficient is game-model.
//
//   * `kFactionDemandExpireLoyaltyAsymptoticDelta = 0.03`
//     Positive value applied as an asymptotic-SUBTRACT on
//     `faction.loyalty` (an unaddressed demand erodes loyalty
//     to the regime). Smaller magnitude than the radicalism
//     delta so a single expired demand is not catastrophic.
//     Coefficient is game-model.
//
// Faction-type ↔ demand-kind allowlist (strict RFC-020 §7
// reading):
//
//     faction.type == "military"          → IncreaseMilitaryBudget
//     faction.type == "workers"           → ExpandWelfare
//     faction.type == "religious"         → ReligiousEducationAuthority
//     faction.type == "local_elites"      → LocalAutonomy
//     faction.type == "technical_elites"  → TechnocratResearchFunding
//     faction.type == "intelligence"      → IntelligenceSurveillanceAuthority
//
// Factions whose `type` is outside this allowlist (e.g.
// "bureaucracy", "media", "students", "farmers") generate NO
// demand under M7.1 — RFC-020 §7 does not list them. A future
// sub-milestone that needs additional kinds widens both the
// `FactionDemandKind` enum and the predicate, in lock-step with
// an additive save bump.
//
// What M7.1 deliberately DOES NOT do:
//
//   * No `Satisfied` lifecycle state. RFC-020 §7 enumerates the
//     "actively make a demand" half of the dynamic; the
//     reply / satisfy surface (player command? AI?) is not
//     authorised by §7 itself and lives in a later sub-milestone.
//     M7.1's only terminal status is Expired.
//   * No `state.rng` consumption. Deterministic-by-construction
//     so the M1.17 / M2 / M3 / M4 / M5 / M6 byte-identical
//     determinism baselines remain re-bakeable from
//     reproducible inputs.
//   * No new player-facing command. No CLI flag. No new artefact.
//     `state.faction_demands` round-trips through `save.json`
//     under save v19; expiration effects surface through
//     `interest_groups.csv` / `summary.csv` indirectly via the
//     faction radicalism / loyalty fields.
//   * No new RFC-090 milestone feature beyond §7.1.

#ifndef LEVIATHAN_SYSTEMS_FACTION_DEMANDS_HPP
#define LEVIATHAN_SYSTEMS_FACTION_DEMANDS_HPP

#include <cstddef>
#include <string>
#include <string_view>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::faction_demands {

// Game-model coefficients (RFC-080 §1, §11). See header comment
// for direction grounding + assumption disclosure.
inline constexpr double kFactionDemandGenerateRadicalismThreshold        = 0.50;
inline constexpr int    kFactionDemandLifetimeDays                       = 60;
inline constexpr double kFactionDemandExpireRadicalismAsymptoticDelta    = 0.05;
inline constexpr double kFactionDemandExpireLoyaltyAsymptoticDelta       = 0.03;

// Closed-allowlist conversions between FactionDemandKind and its
// canonical snake_case string. Both directions reject unknown
// input loudly per `feedback_no_silent_degradation`.
std::string kind_to_string(core::FactionDemandKind kind);
core::Result<core::FactionDemandKind> kind_from_string(std::string_view s);

// Closed-allowlist conversions for FactionDemandStatus.
std::string status_to_string(core::FactionDemandStatus status);
core::Result<core::FactionDemandStatus> status_from_string(std::string_view s);

// RFC-020 §7 faction-type → demand-kind mapping. Returns
// std::nullopt-equivalent via empty optional success (modelled
// here as a Result for parity with the rest of the M6 / M7 helper
// surface): the call SUCCEEDS with an empty payload string when
// `faction_type` is outside the §7 list, and fails ONLY on
// internal-invariant violations (currently none — kept as
// Result-bearing so a future allowlist tightening can fail
// loudly). For each in-list type, the success payload is the
// canonical kind snake_case identifier (so callers can avoid
// re-deriving it).
//
// `has_kind` carries the boolean: true = in §7 list, false =
// not in §7 list. Callers use `has_kind` to gate generation;
// the canonical string is returned only when has_kind == true.
struct FactionTypeMappingResult {
    bool                    has_kind = false;
    core::FactionDemandKind kind     = core::FactionDemandKind::IncreaseMilitaryBudget;
};
core::Result<FactionTypeMappingResult> map_faction_type_to_demand_kind(
    std::string_view faction_type);

// Outcome of `tick_generate`. `demands_generated` counts newly
// appended Pending demands; `factions_considered` counts every
// faction scanned (matches `state.factions.size()`).
struct GenerateOutcome {
    int factions_considered = 0;
    int demands_generated   = 0;
};

// Walk `state.factions`; for each faction whose `type` maps to a
// RFC-020 §7 demand kind AND whose `radicalism` strictly exceeds
// `kFactionDemandGenerateRadicalismThreshold` AND which does not
// currently hold a Pending demand of the matching kind in
// `state.faction_demands`, append a new
// `FactionDemand{kind, status=Pending,
//                created_on=current_date,
//                expires_on=current_date + kFactionDemandLifetimeDays}`.
//
// Strict validation (per `feedback_no_silent_degradation` +
// `feedback_api_signature_expresses_failure`):
//
//   Pre-flight validates ONLY factions that would actually
//   generate a demand (type matches RFC-020 §7 allowlist AND
//   radicalism > threshold). Factions outside the allowlist are
//   none of M7.1's concern (M1.6 `faction::react` owns the
//   per-tick numerical validation of every faction's
//   `support` / `loyalty` / `radicalism` / `influence`).
//
//   For a candidate faction the helper rejects loudly if:
//     * `radicalism` is non-finite or out of `[0, 1]` (checked
//       BEFORE the threshold comparison so a NaN does not
//       silently fall through the `>` test).
//     * `id_code` is empty (used to construct the demand's
//       deterministic id_code).
//     * `country_id_code` is empty.
//     * `country_id_code` does not resolve to an entry in
//       `state.countries`.
//     * `loyalty` is non-finite or out of `[0, 1]` (the
//       expiration step would later mutate it; reject up-front).
//
// Determinism / atomicity:
//
//   * Deterministic over `state.factions` insertion order.
//   * Pure read except for the `state.faction_demands` append.
//   * No `state.rng` consumption.
//   * On any validation failure the function returns
//     `Result::failure` BEFORE any append (candidate-validate-
//     commit). Either all eligible demands are appended or none.
core::Result<GenerateOutcome> tick_generate(
    core::GameState&        state,
    const core::GameDate&   current_date);

// Outcome of `tick_expire_and_apply`. `demands_expired` counts
// status transitions Pending → Expired; `factions_affected` is
// the distinct number of factions whose radicalism / loyalty
// fields were drifted (some factions may carry multiple
// expiring demands per tick, but each faction is counted once).
struct ExpireOutcome {
    int demands_expired   = 0;
    int factions_affected = 0;
};

// Walk `state.faction_demands`; for each demand whose
// `status == Pending` AND `expires_on <= current_date`:
//
//   1. Flip status to Expired (status mutation is sticky; the
//      next call will not re-trigger).
//   2. Locate the issuing faction by `faction_id_code` and
//      apply:
//        radicalism += (1 - radicalism) × kExpireRadicalism…
//        loyalty    -= loyalty × kExpireLoyalty…
//      The asymptotic shape matches the post-PR #115 hardening
//      rule for ratio fields; both targets stay in `[0, 1]` by
//      construction.
//
// Strict validation:
//
//   * Every demand's `faction_id_code` must resolve to an entry
//     in `state.factions`. Loader cross-check is the first
//     line of defence; this is the runtime second line.
//   * Each affected faction's `radicalism` / `loyalty` must
//     already be finite ratios in `[0, 1]` before the drift,
//     and the post-drift candidates must also be in `[0, 1]`
//     (asymptotic form guarantees this when inputs are valid;
//     the check guards against silent corruption).
//
// Determinism / atomicity:
//
//   * Deterministic over `state.faction_demands` insertion order
//     (which is also the generator's append order).
//   * Pure read except for the demand status mutation and the
//     issuing faction's radicalism / loyalty drift.
//   * No `state.rng` consumption.
//   * On any validation failure the function returns
//     `Result::failure` BEFORE any status flip or radicalism /
//     loyalty mutation.
core::Result<ExpireOutcome> tick_expire_and_apply(
    core::GameState&        state,
    const core::GameDate&   current_date);

}  // namespace leviathan::systems::faction_demands

#endif  // LEVIATHAN_SYSTEMS_FACTION_DEMANDS_HPP

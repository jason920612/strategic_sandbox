// PolicySystem - apply a PolicyData's effects to GameState.
//
// First M1 sub-milestone that produces a real gameplay effect.
// Everything before M1.5 was plumbing: loading data, round-tripping
// through saves, validating shape. This is where numbers actually
// change.
//
// Scope per the M1.4 review (unchanged through M1.15):
//   * Targets: country.<field>, country.budget.<category>,
//              faction:<type>.<field>
//   * Ops:     "add", "set"
//   * Effects are instantaneous (NO time-spread application).
//   * NO AI / automatic enactment
//   * NO event integration
//   * NO faction.preferred_policies evaluation
//
// M1.15 adds duration TRACKING (not scheduling): each successful
// apply records an `ActivePolicy{policy.id_code, current_date +
// duration_days}` in `state.countries[actor].active_policies`. The
// list is append-only in M1.15 - no scheduler removes expired
// entries and nothing reverts the effects. The list is what later
// systems (UI, AI, expiration sweep) will read.
//
// Atomicity:
//   apply_policy_effects pre-flight-checks every effect (target
//   resolution, op recognition, candidate-value validation)
//   BEFORE mutating any state. If any effect fails any check,
//   the function returns Result::failure and state is untouched
//   (NO active_policies entry is added). Otherwise every effect
//   applies in order and exactly one ActivePolicy entry is
//   appended.
//
// Strict numeric validation (post-M6.7 hardening sweep):
//   The post-op `std::clamp` that previously saturated ratio
//   fields silently has been removed. Per
//   `feedback_no_silent_degradation`, an effect that would push
//   a ratio target outside `[0, 1]` (either by `set value` or by
//   `add` overshoot) is REJECTED with `Result::failure` naming
//   the target, the candidate value, and the effect index. Non-
//   ratio targets (gdp, tax_revenue, budget_balance, resources)
//   must produce a finite candidate; non-finite candidates are
//   rejected on the same path.

#ifndef LEVIATHAN_SYSTEMS_POLICY_SYSTEM_HPP
#define LEVIATHAN_SYSTEMS_POLICY_SYSTEM_HPP

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::policy {

// Runtime cap on `PolicyData::duration_days`. M1.4 stored it as a raw
// integer; M1.15 made it enter the runtime path
// (`GameDate::advance_days`, a per-day loop), so a hand-rolled
// `INT_MAX` duration would stall an apply call indefinitely. We reject
// anything beyond ~100 years at apply time; the DataLoader does NOT
// enforce this cap because data_loader -> policy_system would invert
// the existing module layering. PolicySystem is the last line of
// defense and is exhaustively unit-tested for both bounds.
inline constexpr int kMaxTrackedPolicyDurationDays = 36500;  // ~100 years

struct ApplyOutcome {
    // Count of effects that were resolved AND applied. Equal to
    // policy.effects.size() on success.
    int effects_applied = 0;

    // Faction broadcast effects record how many distinct faction
    // instances they updated, summed across all faction:* effects
    // in this policy. A faction:* effect that matched no factions
    // contributes 0 (silent no-op).
    int faction_targets_updated = 0;
};

// Apply every effect of `policy` to the country `actor` in `state`.
//
// Target paths recognised:
//   "country.<field>"             - direct CountryState field
//   "country.budget.<category>"   - one BudgetState category
//   "faction:<type>.<field>"      - all factions in `actor` whose
//                                   `type` matches; 0 matches is a
//                                   silent no-op
//
// Ops recognised: "add", "set".
//
// Strict candidate validation (post-M6.7 hardening sweep):
//   Ratio targets (anything documented as `[0, 1]`) require the
//   computed candidate (old + delta for `add`, value for `set`)
//   to be a finite value in `[0, 1]`. Non-ratio targets (gdp,
//   tax_revenue, budget_balance, resources) require the candidate
//   to be finite. Any candidate that fails its check rejects the
//   whole call with `Result::failure`; no state mutation happens.
//
// Failure cases:
//   - actor is not a valid index into state.countries
//   - policy.duration_days is negative or exceeds
//     kMaxTrackedPolicyDurationDays (M1.15 runtime cap, see above)
//   - any effect has an unrecognised op or target path
//   - any effect target's <field> name is unknown
//   - any effect's computed candidate fails its range / finite
//     check (see "Strict candidate validation" above)
// On failure, state is unchanged.
core::Result<ApplyOutcome> apply_policy_effects(
    core::GameState&        state,
    core::CountryId         actor,
    const core::PolicyData& policy);

// M5.6: lower-level effect-application primitive extracted from
// apply_policy_effects so the event-effects applicator
// (`leviathan::systems::event_effects`) can reuse the exact
// target/op resolution + pre-flight atomicity without dragging
// in the M1.15 active_policies bookkeeping (events aren't
// policies; they should not appear in a country's active_policies
// list).
//
// Same target / op / failure semantics as apply_policy_effects:
//   - validates actor is a valid index into state.countries
//   - rejects non-finite effect values at pre-flight
//   - rejects unknown op (only "add" / "set" recognised)
//   - rejects unknown target / field
//   - rejects when a candidate value escapes the target's range
//     (ratio targets require `[0, 1]`; non-ratio require finite)
//   - pre-flight atomicity: any failure leaves state untouched
//   - faction:<type>.<field> with zero matches is a silent no-op
//
// Does NOT:
//   - record an ActivePolicy entry (that's policy-specific;
//     apply_policy_effects appends after calling this helper)
//   - apply a duration cap (no duration concept here)
//
// Used by:
//   - apply_policy_effects (above, this module)
//   - event_effects::apply_event_effects (M5.6 new module)
core::Result<ApplyOutcome> apply_effects_to_actor(
    core::GameState&                       state,
    core::CountryId                        actor,
    const std::vector<core::PolicyEffect>& effects);

}  // namespace leviathan::systems::policy

#endif  // LEVIATHAN_SYSTEMS_POLICY_SYSTEM_HPP

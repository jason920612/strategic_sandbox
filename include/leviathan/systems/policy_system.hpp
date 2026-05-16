// PolicySystem - apply a PolicyData's effects to GameState.
//
// First M1 sub-milestone that produces a real gameplay effect.
// Everything before M1.5 was plumbing: loading data, round-tripping
// through saves, validating shape. This is where numbers actually
// change.
//
// Scope per the M1.4 review:
//   * Targets: country.<field>, country.budget.<category>,
//              faction:<type>.<field>
//   * Ops:     "add", "set"
//   * NO duration queue (effects are instantaneous)
//   * NO AI / automatic enactment
//   * NO event integration
//   * NO faction.preferred_policies evaluation
//
// Atomicity:
//   apply_policy_effects pre-flight-checks every effect (target
//   resolution, op recognition) BEFORE mutating any state. If any
//   effect fails to resolve, the function returns Result::failure
//   and state is untouched. Otherwise every effect applies in order.

#ifndef LEVIATHAN_SYSTEMS_POLICY_SYSTEM_HPP
#define LEVIATHAN_SYSTEMS_POLICY_SYSTEM_HPP

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::policy {

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
// Post-op clamping: ratio fields (anything documented as `[0, 1]`)
// are clamped to `[0, 1]` after the op. Absolute fields (gdp,
// tax_revenue, budget_balance, resources) are not clamped.
//
// Failure cases:
//   - actor is not a valid index into state.countries
//   - any effect has an unrecognised op or target path
//   - any effect target's <field> name is unknown
// On failure, state is unchanged.
core::Result<ApplyOutcome> apply_policy_effects(
    core::GameState&        state,
    core::CountryId         actor,
    const core::PolicyData& policy);

}  // namespace leviathan::systems::policy

#endif  // LEVIATHAN_SYSTEMS_POLICY_SYSTEM_HPP

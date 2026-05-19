// Issue #110: AI policy selection driven by country / interest-group /
// relationship state — not the previous RCR-1 first-policy stub.
//
// Wired into `monthly::tick_all_countries` (see monthly_pipeline.hpp).
//
// Module surface:
//
//   select_policies(state) -> Result<vector<Selection>>
//     Read-only / mutation-free. Returns one Selection per non-player
//     country that has at least one eligible (non-stacked) policy.
//     The selection rule is the deterministic scorer defined in
//     ai_policy.cpp: for each candidate policy, sum
//
//        score = Σ_effect (effect.value × target_desire(country, effect.target))
//              + IG-aggregate term (radicalism / loyalty pressure)
//              + category baseline (intelligence/media/anti-corruption)
//
//     The highest-scoring policy wins; on a tie, the lower vector index
//     wins (stable tie-break). Policies whose `id_code` is already
//     present in `country.active_policies` with an unexpired
//     `expires_on` are excluded (no-stacking rule). If every policy is
//     stacked, the country emits no Selection that month (counted via
//     ApplyOutcome.skipped).
//
//   apply_selected_policies(state) -> Result<ApplyOutcome>
//     Calls select_policies, then dispatches every Selection through
//     `policy::apply_policy_effects(state, country, policy)`. Apply
//     inherits M1.5 pre-flight atomicity (a failing effect leaves
//     THAT country untouched) and M1.15 active_policies bookkeeping
//     (a successful apply appends one ActivePolicy entry).
//     Post-M6.7 hardening (`feedback_no_silent_degradation` +
//     `feedback_api_signature_expresses_failure`): a per-country
//     apply failure now surfaces as a hard `Result::failure` rather
//     than being recorded into `failed_countries`. The latter field
//     is retained for API compat but is always empty under the
//     strict validation regime.
//
// Inputs the scorer reads (RFC-090 §3.5 / §3.6 / §3.7 / §3.8;
// RFC-040 §4):
//   - CountryState: stability / legitimacy / corruption /
//     administrative_efficiency / fiscal_capacity / central_control /
//     budget_balance / gdp / legal_tax_burden / threat_perception /
//     military_strength
//   - GameState::relationships (CountryRelation): inbound threat to
//     this country and source country's military_strength for the
//     military-gap term
//   - GameState::interest_groups (InterestGroupState):
//     influence-weighted radicalism + mean loyalty over IGs whose
//     `country` is this country
//   - PolicyData: category + per-effect target / op / value
//
// Determinism guarantees (both functions):
//   - RNG-free (state.rng untouched)
//   - vector order preserved (state.countries[i] -> Selection in
//     traversal order; equal-score policies break by lower vector index)
//   - apply atomicity (per-country, via M1.5 pre-flight)
//   - no time-based input beyond `state.current_date` for the
//     unexpired-policy filter
//
// What this module does NOT do:
//   - apply to the player country (skipped via state.player_country
//     check)
//   - stack a policy already active+unexpired on the country
//   - emit logs / events.jsonl entries (consistent with the M3.4 /
//     M5.6 applicator "no log emission" convention)
//   - emit CSV rows
//   - consume state.rng

#ifndef LEVIATHAN_SYSTEMS_AI_POLICY_HPP
#define LEVIATHAN_SYSTEMS_AI_POLICY_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::ai_policy {

// One AI-driven selection: which policy the AI would enact for
// `country`. Mirrors the (country, policy_id_code) shape of scenario
// manifest `starting_policies` entries (M1.13) so the apply path
// reuses the same plumbing.
struct Selection {
    core::CountryId country;
    std::string     policy_id_code;
};

// Compute one Selection per non-player country that has an eligible
// (non-stacked) candidate. Countries whose every policy is currently
// active+unexpired emit no Selection (handled in apply_selected_policies
// via ApplyOutcome.skipped).
//
// Preconditions: none. Empty `state.countries` and empty `state.policies`
// both produce an empty selection vector (Result::success with an empty
// vector — not a failure).
//
// Guarantees:
//   - deterministic (same input -> identical output bytes)
//   - RNG-free (state.rng untouched)
//   - mutation-free (state unchanged)
//   - skips state.player_country when valid
core::Result<std::vector<Selection>>
select_policies(const core::GameState& state);

// AI policy apply path. Calls `select_policies(state)` and then
// applies each Selection via the existing
// `policy::apply_policy_effects(state, country, policy)` path so
// every AI selection inherits M1.5 pre-flight atomicity + M1.15
// active_policies bookkeeping.
//
// Returns a per-call summary: how many AI countries were considered,
// how many had a successful apply, how many were skipped (no
// eligible candidate; every policy currently active+unexpired).
struct ApplyOutcome {
    std::size_t considered      = 0;  // non-player countries scanned
    std::size_t applied         = 0;  // successful apply calls
    std::size_t skipped         = 0;  // no eligible candidate (all stacked)
    // Issue #112: countries whose `compute_total_pressure` was
    // below `kPressureThreshold` and therefore emitted 0
    // selections. These countries ARE counted in `considered`
    // but NOT in `skipped` (skipped means "tried, all
    // candidates stacked"). Sum:
    //   considered = applied_count_per_country + skipped + pressure_below_threshold_skipped
    std::size_t pressure_below_threshold_skipped = 0;
    // Vestigial under the post-M6.7 strict validation regime — a
    // per-country apply failure now surfaces as a hard
    // Result::failure from apply_selected_policies, so this vector
    // is always empty. Retained for API compat with M3.5-era callers.
    std::vector<core::CountryId> failed_countries;
};

core::Result<ApplyOutcome>
apply_selected_policies(core::GameState& state);

}  // namespace leviathan::systems::ai_policy

#endif  // LEVIATHAN_SYSTEMS_AI_POLICY_HPP

// RCR-1: AI policy selection + apply.
//
// Clears RFC-090 §3.5 ("AI policy selection") AND
// RFC-010 §2.2 ("AI countries can auto-select policy") with
// both a deterministic selection helper and a per-country
// apply path that reuses the existing M1.5 policy machinery.
//
// RCR-1 is a one-time corrective PR (NOT a long-running
// recovery track and NOT an M-milestone number); see
// docs/rfc-090-010-compliance-audit.md §5.1 for the framing
// rule. M6.6 resumes per RFC-090 §6.6 after this PR lands.
//
// Module surface:
//
//   select_policies(state) -> Result<vector<Selection>>
//     Read-only / mutation-free. One Selection per
//     non-player country in state.countries vector order
//     (player skipped when state.player_country is a valid
//     index). When state.policies is empty, returns an empty
//     vector. Selection rule for RCR-1 is "first policy in
//     state.policies (vector order)" — deliberately minimal,
//     deterministic, RNG-free. A future refinement can swap
//     in a fit-scoring routine without changing the API.
//
//   apply_selected_policies(state) -> Result<ApplyOutcome>
//     Calls select_policies, then dispatches every Selection
//     through policy::apply_policy_effects(state, country,
//     policy). Apply inherits M1.5 pre-flight atomicity
//     (a failing effect leaves THAT country untouched) and
//     M1.15 active_policies bookkeeping (a successful apply
//     appends one ActivePolicy entry to country.active_policies
//     via the existing policy path). Fail-continue across
//     countries: a failure on country i records into
//     ApplyOutcome.failed_countries but does NOT abort the
//     remaining apply calls — mirrors the M5.6 applicator
//     "broken event for country X doesn't block country Y"
//     convention.
//
// Determinism guarantees (both functions):
//
//   - RNG-free          (state.rng untouched)
//   - vector order      (state.countries[i] -> Selection[i]
//                        when neither i is the player nor
//                        state.policies is empty)
//   - apply atomicity   (per-country, via M1.5 pre-flight)
//   - no time-based input
//
// What this module does NOT do:
//
//   - schedule policies for later application (no scheduler;
//     each apply records ONE ActivePolicy entry via the
//     existing M1.15 path)
//   - consume state.rng (RNG-free)
//   - emit logs / events.jsonl / CSV (consistent with the
//     M3.4 / M5.6 applicator "no log emission" convention)
//   - consult RFC-010 §3.6 relationships / §3.7 threat /
//     §3.8 military_strength (those fields exist on state but
//     are inputs for a future smarter selection rule, not
//     the current first-policy rule)
//   - run as part of monthly::tick_all_countries (standalone
//     caller-driven helpers, mirroring M5.7's pre-M5.8
//     contract; a future runner-policy may auto-call
//     apply_selected_policies once per month)
//   - apply to the player country (deliberately skipped)

#ifndef LEVIATHAN_SYSTEMS_AI_POLICY_HPP
#define LEVIATHAN_SYSTEMS_AI_POLICY_HPP

#include <string>
#include <vector>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::ai_policy {

// One AI-driven selection: which policy the AI would enact
// for `country`. Mirrors the (country, policy_id_code) shape
// of scenario manifest `starting_policies` entries (M1.13)
// so a future apply path can reuse the same plumbing.
struct Selection {
    core::CountryId country;
    std::string     policy_id_code;
};

// Compute one Selection per non-player country.
//
// Preconditions: none. Empty `state.countries` and empty
// `state.policies` both produce an empty selection vector
// (Result::success with an empty vector — not a failure).
//
// Guarantees:
//   - deterministic (same input -> identical output bytes)
//   - RNG-free (state.rng untouched)
//   - mutation-free (state unchanged)
//   - vector order preserved (state.countries[i] -> result[i]
//     when neither i is the player nor state.policies is empty)
core::Result<std::vector<Selection>>
select_policies(const core::GameState& state);

// RCR-1: AI policy apply path. Calls `select_policies(state)` and
// then applies each `Selection` via the existing
// `policy::apply_policy_effects(state, country, policy)` path so
// every AI selection inherits M1.5 pre-flight atomicity + M1.15
// `active_policies` bookkeeping.
//
// Returns a per-call summary: how many AI countries were
// considered, how many had a successful apply, how many were
// skipped (no matching policy in `state.policies`, or
// `policy::apply_policy_effects` returned a failure).
//
// Atomicity:
//   - Each per-country apply call is atomic in its own M1.5
//     pre-flight sense (a failing effect leaves THAT country
//     untouched).
//   - Across countries, this function is FAIL-CONTINUE rather
//     than FAIL-FAST: a failing apply on country `i` records
//     the failure in the outcome but does not abort the
//     remaining `[i+1..end)` apply calls. This mirrors the
//     M5.6 `apply_event_effects` policy of "broken event for
//     country X doesn't block country Y" and keeps the AI
//     applicator from leaving the world half-AI'd.
//   - The function never throws. Failures are reported through
//     `ApplyOutcome.failed_countries`.
//
// Determinism: identical to `select_policies` — RNG-free, vector-
// order, no time-based input. Same input state produces an
// identical mutation sequence.
//
// What this function does NOT do:
//   - apply to the player country (skipped via the same
//     player-detection used in `select_policies`)
//   - select more than one policy per country in this call
//   - schedule policies for later application (no scheduler;
//     reuses M1.15 active_policies expiry-tracking that the
//     existing policy path already records)
//   - emit logs / events.jsonl entries (consistent with the
//     M3.4 / M5.6 "no log emission" invariant for this
//     applicator's neighbours)
//   - emit CSV rows
//   - consult relationships / threat / military values (those
//     fields exist on state but are inputs for a future smarter
//     selection rule, not the current first-policy rule)
struct ApplyOutcome {
    std::size_t considered      = 0;  // non-player countries scanned
    std::size_t applied         = 0;  // successful apply calls
    std::size_t skipped         = 0;  // no policies / no selection
    std::vector<core::CountryId> failed_countries;  // apply returned failure
};

core::Result<ApplyOutcome>
apply_selected_policies(core::GameState& state);

}  // namespace leviathan::systems::ai_policy

#endif  // LEVIATHAN_SYSTEMS_AI_POLICY_HPP

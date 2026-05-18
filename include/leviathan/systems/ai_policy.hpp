// RCR-1: AI policy selection skeleton.
//
// Clears RFC-090 §3.5 ("AI policy selection") and partially
// clears RFC-010 §2.2 ("AI countries can auto-select policy")
// at the *selection* layer only.
//
// RCR is a recovery-track identifier, NOT an M-milestone
// number. RCR-1 ships a deterministic, RNG-free, mutation-free
// selection primitive. The corresponding *apply* path is
// deliberately out of scope — it requires careful integration
// with M1.5 pre-flight atomicity + M1.15 active_policies
// bookkeeping and is tracked in
// docs/rfc-090-010-compliance-audit.md for a later RCR PR.
//
// Selection rule for RCR-1:
//
//   For each country `c` in `state.countries`, in vector order:
//     - If `state.player_country == c`, skip — the player picks
//       their own policies.
//     - Otherwise, select the FIRST policy in `state.policies`
//       (vector order). When `state.policies` is empty, skip.
//
// That rule is deliberately minimal:
//
//   - deterministic         — no RNG, no time-based input
//   - vector-order tie-break  — same canonical-load-order
//                               convention used by the M5
//                               evaluator
//   - per-country emission   — one Selection per applicable
//                              country; no scoring across
//                              countries
//   - no side effects        — returns a vector, does NOT
//                              call apply_policy_effects
//
// A future RCR PR can replace "first policy" with a
// fit-scoring routine that ranks policies against per-country
// state (e.g. the country with the lowest stability prefers
// a stability-raising policy). That refinement is data-only
// and would not change this API.
//
// What this module does NOT do:
//
//   - apply selected policies (no state mutation)
//   - schedule policies for later application
//   - consume state.rng (RNG-free, like the M5 evaluator)
//   - emit logs (no state.logs append)
//   - emit CSV / events.jsonl entries
//   - consult RFC-010 §3.6 relationships / §3.7 threat (those
//     fields exist on CountryState but no RCR-1 caller needs
//     them; future RCR PRs that ship the relationships
//     matrix can extend the selection rule)
//   - run as part of monthly::tick_all_countries (this is
//     a standalone caller-driven helper, mirroring M5.7's
//     pre-M5.8 contract)

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

}  // namespace leviathan::systems::ai_policy

#endif  // LEVIATHAN_SYSTEMS_AI_POLICY_HPP

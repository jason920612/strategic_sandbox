# M1.15 - Policy duration tracking

Companion notes for `feature/m1-15-policy-duration-tracking`. M1.15
gives `PolicyData::duration_days` its first runtime meaning: every
successful `policy::apply_policy_effects` call now records an
`ActivePolicy{policy_id_code, expires_on}` entry on the actor country.
The list is **append-only** in this milestone — nothing removes
expired entries, nothing reverts the policy's effects when
`expires_on` arrives. M1.15 is deliberately tracking-only;
expiration / revert is a future milestone's concern.

## 1. Scope

M1.4 introduced the `duration_days` field on `PolicyData`. M1.5
applied policy effects atomically but never looked at `duration_days`.
M1.13 used `apply_policy_effects` to enact day-0 policies from a
scenario manifest, again without a duration concept.

M1.15 adds the minimum runtime state that lets a future system reason
about *which policies are currently in effect on which country* and
*when each one is scheduled to end*:

- a new core type `ActivePolicy { std::string policy_id_code; GameDate expires_on; }`
- a new `CountryState::active_policies` vector (default empty)
- a single new side effect inside `policy::apply_policy_effects`
- a save format bump (v6 → v7) so the new state survives a
  save / load round trip

That's the whole PR. No scheduler. No AI. No event integration. No
expiration sweep. No revert. No dedup. No new flags. No new logs.

## 2. Public API

`include/leviathan/core/entities.hpp`:

```cpp
struct ActivePolicy {
    std::string policy_id_code;   // matches a PolicyData::id_code
    GameDate    expires_on{};     // current_date + duration_days at apply time
};

struct CountryState {
    // ... M1.1-M1.12 fields ...
    BudgetState                 budget;
    std::vector<ActivePolicy>   active_policies;   // M1.15, append-only
};
```

We deliberately store `policy_id_code` (the on-disk-stable string)
rather than a `PolicyId` handle. The string identifier survives
across loads even when the numeric `PolicyId` would be reassigned by
vector index; a future caller that wants the numeric id can re-resolve
from `state.policies` cheaply.

## 3. Behaviour change in `policy::apply_policy_effects`

Pre-flight semantics from M1.5 are unchanged: any unresolvable target
or unrecognised op fails the whole call and leaves state untouched.
What changes is the tail of a **successful** apply:

```cpp
// After the apply loop completes:
GameDate expires_on = state.current_date;
expires_on.advance_days(policy.duration_days);
state.countries[actor].active_policies.push_back(
    ActivePolicy{policy.id_code, expires_on});
```

Properties:

- Exactly one `ActivePolicy` entry is appended per successful call.
- Pre-flight failure appends nothing. Atomicity covers the new side
  effect, not just the numeric mutations.
- `duration_days == 0` is allowed (the DataLoader admits it). The
  entry is still recorded; `expires_on` equals `current_date`. We
  prefer "tracked with same-day expiry" over "silently dropped"
  because diagnostics may still want to surface that the policy was
  enacted.
- Only the `actor` country's list grows. Cross-country side effects
  are forbidden.
- A faction-broadcast effect that matches zero factions remains a
  silent no-op for the *effect*, but the policy itself is still
  considered enacted and is recorded.

## 4. Save format v6 → v7

History: v5 added the policies array, v6 added
`CountryState.last_gdp_growth_rate`. v7 adds
`CountryState.active_policies`.

`SaveSystem` serialises each country's `active_policies` as a JSON
array of two-key objects, written after the `budget` block:

```json
{
  "id": 0, "id_code": "GER", "name": "Germany", "display_name": "Germany",
  "gdp": 100.0, ...,
  "last_gdp_growth_rate": 0.0035,
  "budget": { ... },
  "active_policies": [
    { "policy_id_code": "raise_taxes",              "expires_on": "1930-03-02" },
    { "policy_id_code": "increase_military_budget", "expires_on": "1930-01-31" }
  ]
}
```

Loader rules:

- `save_version != 7` is rejected loudly (same gate as v1–v5 / v6).
- `active_policies` missing on any country object is a hard failure
  (no silent default to `[]`). A v6 save tolerated as v7 would drop
  every day-0 enactment a scenario loaded, which would be silently
  wrong.
- Each entry is a JSON object. Missing `policy_id_code` or `expires_on`
  is rejected with the array index in the error message
  (e.g. `active_policies[0]`).
- `expires_on` is parsed by the same proleptic Gregorian helper used
  elsewhere; an invalid date like `"1930-02-30"` is rejected loudly.

Insertion order matches the order in which `apply_policy_effects`
appended entries, so a save + load round trip preserves the apply
sequence.

## 5. Tests

12 new doctest cases (M1.14 was 399 → M1.15 is 411):

`tests/systems/policy_system_test.cpp` (6 cases under the
"M1.15: duration tracking" section):

- successful enact appends one entry with `expires_on = current_date + duration_days`
- pre-flight failure does NOT append (atomicity covers the side effect)
- `duration_days == 0` still records the policy with same-day expiry
- multiple enacts append in call order
- faction-zero-match enactment still records the policy
- only the actor's `active_policies` grows; sibling countries unaffected

`tests/systems/save_system_test.cpp` (6 cases):

- rejects an old v6 save loudly (mirrors the v1..v5 rejection pattern)
- `serialize` always emits `"active_policies": []` (so freshly built
  states round-trip cleanly under v7's strict-required rule)
- `save + load` round-trips a country whose `active_policies` is
  populated (two entries on GER, one on FRA, all dates preserved)
- v7 country missing `active_policies` is rejected with
  `countries[N]` in the error
- entry missing `policy_id_code` is rejected with `active_policies[N]`
  in the error
- entry with malformed `expires_on` is rejected with the bad date
  string in the error

`tests/systems/scenario_loader_test.cpp` (extended the existing M1.13
"day-0 enactment" case): after the scenario loader applies
`raise_taxes` on GER at day 0, GER's `active_policies` has exactly
one entry whose `expires_on` equals `1930-03-02` (1930-01-01 + 60
days).

The existing M0.10 `--summary-csv` byte-identical determinism contract
is unaffected. The M1.10 / M1.11 runner determinism tests (same seed
→ byte-identical `save.json`) still pass against the new schema.

## 6. What's NOT in scope

Deliberate non-goals, listed so future contributors do not silently
unwind the M1.15 contract:

- **No expiration sweep.** Nothing removes entries whose `expires_on`
  has passed. The list grows forever for the duration of M1.15. A
  future sub-milestone will add an explicit sweep, almost certainly
  inside the monthly pipeline.
- **No effect revert.** When an entry expires, the policy's
  earlier-applied effects on numeric fields (`legal_tax_burden`,
  budget categories, etc.) stay applied. M1.4 said effects are
  instantaneous; M1.15 does not retroactively change that.
- **No dedup.** Re-enacting the same `policy_id_code` on the same
  country appends a second entry. The loop in `apply_policy_effects`
  is intentionally simple; "is this policy already active" is a
  question for a future system.
- **No scheduler.** No automatic enactment loop, no AI decision, no
  event-triggered enactment, no caller other than the existing
  ones (`policy::apply_policy_effects` direct call, and the M1.13
  scenario loader day-0 path).
- **No new logs.** Enactment does not emit a log entry. The monthly
  pipeline is untouched.
- **No JSON-config / DataLoader change.** Country / faction / policy
  JSON shapes are unchanged. Only the save format bumps.

## 7. Cross-links

- M1.4 (`m1-4-policy-data.md`) introduced `duration_days` and the
  policy fixtures.
- M1.5 (`m1-5-policy-system.md`) introduced
  `apply_policy_effects` with its pre-flight atomicity rule, which
  M1.15 extends to cover the new side effect.
- M1.12 (`m1-12-economy-stability-coupling.md`) was the previous
  save-format bump (v5 → v6) and is the cleanest precedent for the
  loud-rejection pattern that M1.15 follows.
- M1.13 (`m1-13-scenario-starting-policies.md`) was the first caller
  outside hand-rolled tests; its day-0 enactment now also populates
  `active_policies`.

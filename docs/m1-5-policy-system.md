# M1.5 - PolicySystem apply effects

**The first M1 sub-milestone that produces a real gameplay effect.**
Everything before M1.5 was plumbing — loading data, round-tripping
through saves, validating shape. M1.5 is where numbers actually
change.

Companion notes for `feature/m1-05-policy-system-apply`.

## 1. Scope (deliberately minimal per the M1.4 review)

- **Targets**: `country.<field>`, `country.budget.<category>`,
  `faction:<type>.<field>`
- **Ops**: `add`, `set`
- **NO** duration queue — effects are instantaneous
- **NO** AI / automatic enactment
- **NO** event integration
- **NO** `FactionState.preferred_policies` evaluation
- **NO** runner CLI integration

If a policy effect has a richer target syntax (province paths, global
effects, conditional triggers, ...), this PR fails it at pre-flight.
Future PRs can extend the target grammar.

## 2. API

```cpp
namespace leviathan::systems::policy {

struct ApplyOutcome {
    int effects_applied        = 0;  // == policy.effects.size() on success
    int faction_targets_updated = 0; // sum across faction:* effects
};

Result<ApplyOutcome> apply_policy_effects(
    GameState&        state,
    CountryId         actor,
    const PolicyData& policy);

}
```

Free function over `GameState&` — same shape as every other system
in the project. `GameState` itself remains methodless.

## 3. The atomicity guarantee

**Pre-flight check first**: walk every effect, validate target /
op resolves, only then apply. If any effect fails to resolve, the
function returns `Result::failure` with the effect index in the
message and **state is untouched**.

Why this matters: a half-applied policy leaves state in a confusing
mixed condition that's hard to debug. The atomicity guarantee makes
`apply_policy_effects` an all-or-nothing operation from the caller's
perspective, matching the M0.8 / SaveSystem failure semantics.

Pinned by `"apply: a failure in any effect leaves state unchanged"`
in `tests/systems/policy_system_test.cpp` — a policy where effects
0–2 are valid but effect 3 has a typo'd target verifies that none
of 0–2 take effect.

### Pre-flight subtlety: unknown faction field when zero match

```json
{"target": "faction:military.morale", "op": "add", "value": 0.1}
```

If the actor country has no `military` factions, the naive
implementation would silently no-op (target syntax is valid, just no
recipients). That would miss the typo. The resolver detects this
case by probing a default-constructed `FactionState` for the field
name — if the field is unknown even on the canonical shape, the
pre-flight fails. If the field is known but no factions match, it's
a legitimate broadcast-with-zero-recipients no-op.

## 4. Target path grammar

| Pattern | What it touches | Range |
|---|---|---|
| `country.gdp` | `actor.gdp` | absolute |
| `country.tax_revenue` | `actor.tax_revenue` | absolute |
| `country.budget_balance` | `actor.budget_balance` | absolute (can be negative) |
| `country.legal_tax_burden` | ratio | `[0, 1]` |
| `country.fiscal_capacity` | ratio | `[0, 1]` |
| `country.administrative_efficiency` | ratio | `[0, 1]` |
| `country.central_control` | ratio | `[0, 1]` |
| `country.corruption` | ratio | `[0, 1]` |
| `country.stability` | ratio | `[0, 1]` |
| `country.legitimacy` | ratio | `[0, 1]` |
| `country.military_power` | ratio | `[0, 1]` |
| `country.threat_perception` | ratio | `[0, 1]` |
| `country.budget.administration` | `actor.budget.administration` | `[0, 1]` |
| `country.budget.military` | ratio | `[0, 1]` |
| `country.budget.education` | ratio | `[0, 1]` |
| `country.budget.welfare` | ratio | `[0, 1]` |
| `country.budget.intelligence` | ratio | `[0, 1]` |
| `country.budget.infrastructure` | ratio | `[0, 1]` |
| `country.budget.industry` | ratio | `[0, 1]` |
| `faction:<type>.support` | every faction in actor with matching type | `[0, 1]` |
| `faction:<type>.influence` | ratio | `[0, 1]` |
| `faction:<type>.radicalism` | ratio | `[0, 1]` |
| `faction:<type>.loyalty` | ratio | `[0, 1]` |
| `faction:<type>.resources` | absolute | `>= 0` per data loader, but **not clamped here** |

Anything else fails pre-flight with `"unrecognised target syntax ..."`.

## 5. Op semantics

- `add`: `field += value`. Negative values supported.
- `set`: `field = value`.
- Both ops apply post-clamp to `[0, 1]` for ratio fields. Absolute
  fields are never clamped.

`set` on a ratio field with an out-of-range value clamps rather than
errors:

```json
{"target": "country.stability", "op": "set", "value": 1.7}
```

→ `stability` becomes 1.0. Rationale: the policy author may have
written 1.7 intending "maxed out"; silently clamping is friendlier
than refusing. If a future PR needs strict-set-must-be-in-range
behaviour, we can add a `set_strict` op.

Anything other than `add` / `set` fails pre-flight.

## 6. Faction broadcast details

`faction:<type>.<field>` matches **all** factions in the actor
country whose `type == <type>`. Counts:

| Match count | Behaviour |
|---|---|
| 0 | Silent no-op. `faction_targets_updated += 0`. |
| 1 | Single faction updated. `faction_targets_updated += 1`. |
| N | All N updated in vector order. `faction_targets_updated += N`. |

Factions belonging to other countries (different
`FactionState::country`) are **always skipped**. A
`faction:military.support` effect from Germany's policy doesn't
touch France's military.

## 7. What M1.5 deliberately does NOT do

- **No `Diagnostics::sanity_check` rule for policy targets.** Could
  add "every PolicyData.effects[].target resolves against the
  current GameState" once we have a runner consumer.
- **No `--enact` runner flag.** Policies still aren't enacted from
  the CLI. A future PR can wire the runner to load policies, choose
  one (interactive? scripted?), and call `apply_policy_effects`.
- **No FactionState.preferred_policies consumption.** That list
  still doesn't drive anything.
- **No duration tracking.** Effects are instantaneous. A future
  "PolicyInstance" abstraction (separate from PolicyData templates)
  will track in-flight policies with remaining duration.
- **No save format change.** Applying a policy mutates the existing
  `CountryState` / `FactionState` fields, which are already in v5
  saves. The save format stays at v5.

## 8. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/systems/policy_system.hpp` | **new** — public API |
| `src/leviathan/systems/policy_system.cpp` | **new** — implementation |
| `src/leviathan/systems/CMakeLists.txt` | adds `policy_system.cpp` |
| `tests/systems/policy_system_test.cpp` | **new** — 24 cases |
| `tests/CMakeLists.txt` | adds the test source |
| `docs/m1-5-policy-system.md` | this file |
| `docs/README.md` | index entry |
| `README.md`, `rfc/README.md` | status / progress notes |

Drive-by from PR #15 review:

| File | Δ |
|---|---|
| `src/leviathan/systems/data_loader.cpp` | `2147483647` → `std::numeric_limits<int>::max()`; `<limits>` include |
| `src/leviathan/systems/save_system.cpp` | `2147483647` → `std::numeric_limits<int>::max()` (`<limits>` already included) |

`src/leviathan/systems/runner.cpp` still has its own hardcoded
`2147483647` for `--days`; left alone in this PR to keep scope tight
(can be a separate one-line follow-up).

Total tests: **231 → 255** (+24).

## 9. Risks / things to watch

- **Field-name lookup is a hand-rolled if/else chain.** Easy to
  read, easy to forget when a new field lands. Future PRs that add
  fields to `CountryState` / `BudgetState` / `FactionState` must
  remember to extend `country_field_ptr` / `country_budget_field_ptr`
  / `faction_field_ptr`. A unit test should be added for the new
  field. The pre-flight check will fail loudly if a policy
  references the new field before the lookup is updated — which is
  the failure mode we want.
- **`country.budget.<cat>` overlaps `country.<field>` in the prefix
  match.** The implementation checks the longer prefix first, so
  `country.budget.military` resolves to the budget category, not
  the `country.military_power` field. Tests cover the budget path
  explicitly so a regression would be caught.
- **Faction broadcast iterates `state.factions` linearly.** Fine for
  M1 scale (≤ a few hundred factions per game). If a future
  scenario has many thousands of factions and many faction-targeting
  policies per turn, we'll want an index by `(country, type)`. Not
  yet.
- **`set` on a ratio field clamps silently.** Documented above;
  reviewers should reject a future PR that introduces strict-set
  semantics without also adding a `set_strict` op (or similar).

## 10. Next sub-milestone

The user's M1 plan deferred subsequent sub-milestones beyond M1.5.
Likely candidates (per RFC-090 §M1):

- **M1.6 — faction reaction logic**: support / influence /
  radicalism evolve in response to policies, ticking on month
  boundaries. Probably wired into the runner + a
  `faction_system::tick(state)` free function.
- **M1.7 — stability tick** per RFC-080 §5.
- **M1.8 — month-end economy tick** (GDP × tax_burden × fiscal_capacity
  × ..., per RFC-080 §3).

PolicySystem is the foundation those later milestones will build
on: policies enact effects → faction support shifts → faction
support changes drive stability → economy ticks reflect stability.
M1.5 just provides the "policy → numbers change" primitive.

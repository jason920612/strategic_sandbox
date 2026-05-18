# M5.2 - Trigger evaluator skeleton

Companion notes for
`feature/rfc090-m5-02-trigger-evaluator-skeleton`.

**This is RFC-090 M5** (event engine), not a post-M4 governance
follow-up. M5.2 ships only the **trigger evaluator** — a small
read-only API that reads M5.1's `state.events` and reports which
event definitions' triggers all match the current state. It does
NOT fire events, NOT append history, NOT apply effects, NOT
integrate with the runner or monthly pipeline, and NOT change
`events.jsonl` semantics. The next M5 sub-milestone composes
evaluation with firing / history / effects-application; M5.2
chooses what evaluation looks like.

The boundary is intentional: every M5 sub-milestone so far has
been one decoupled surface (M5.1 = schema, M5.2 = evaluation).
Composing them into "evaluate → fire → apply" is a separate PR.

## 1. Scope

What ships:

```text
leviathan::systems::event_evaluator                (new module)
    TriggerMatch { event_index, event_id_code }     (new struct)
    bool trigger_matches(state, EventTrigger)       (new free function)
    bool evaluate(state, EventDefinition)           (new free function)
    std::vector<TriggerMatch> match_events(state)   (new free function)
include/leviathan/systems/event_evaluator.hpp       (new header)
src/leviathan/systems/event_evaluator.cpp           (new impl)
tests/systems/event_evaluator_test.cpp              (28 new doctest cases)
CMakeLists updates for the new sources
```

What does NOT change:

```text
no save format bump (still v13)
no new state field
no new artefact (still 10)
no new runner CLI flag
no events.jsonl semantic change
no new PlayerCommandKind
no change to M5.1 schema (EventDefinition / EventTrigger /
   PolicyEffect reuse)
no change to M1/M2/M3/M4 systems
no change to scenario_loader / save_system / diagnostics
   (state.events is M5.1's read; M5.2 only reads it)
no change to canonical fixtures
no change to milestone-5-checkpoint.md (it still doesn't
   exist — deferred until M5 ships multiple surfaces of
   non-trivial cross-cutting concern; M5.2 is one more
   single-surface PR)
```

## 2. API shape

```cpp
namespace leviathan::systems::event_evaluator {

struct TriggerMatch {
    std::size_t event_index = 0;
    std::string event_id_code;   // mirror of state.events[i].id_code
};

bool trigger_matches(const core::GameState&    state,
                     const core::EventTrigger& trig);

bool evaluate(const core::GameState&       state,
              const core::EventDefinition& def);

std::vector<TriggerMatch> match_events(const core::GameState& state);

}  // namespace leviathan::systems::event_evaluator
```

Three reasons the API is three free functions and not one:

- `match_events` is the high-level call the future
  evaluator-firer composition will use. It walks
  `state.events` once.
- `evaluate` is the per-event single-shot version, useful for a
  hand-built / synthesised `EventDefinition` (tests use it; a
  future "evaluate this one specific event when the user clicks
  it" surface might use it too).
- `trigger_matches` is the per-trigger predicate, exposed so
  tests can pin per-target / per-op semantics without going
  through full event evaluation, and so a future "why didn't
  this event fire?" debug surface can introspect.

All three are pure: read GameState, return a value. No mutation,
no logging, no event firing, no time / RNG side effects.

## 3. Evaluation semantics

### 3.1 Per-trigger (`trigger_matches`)

A trigger is `{ target, op, value }`. The evaluator dispatches
on `target` into a state-slice, applies `op` against `value`,
and returns true iff at least one entity satisfies the
comparison ("ANY satisfies" semantics).

```text
country.stability                                          -> state.countries
country.legitimacy                                         -> state.countries
country.government_authority.bureaucratic_compliance       -> state.countries
interest_group.radicalism                                  -> state.interest_groups
interest_group.loyalty                                     -> state.interest_groups
```

Op semantics on a single (lhs = state field, rhs = trigger
value) pair:

```text
lt    lhs <  rhs
lte   lhs <= rhs
gt    lhs >  rhs
gte   lhs >= rhs
```

### 3.2 Aggregation across entities ("ANY")

For each country-scoped trigger, the predicate is `exists
country c in state.countries such that op(c.<field>,
trig.value)`. For each interest-group-scoped trigger, same with
`state.interest_groups`. The empty-list case therefore
evaluates to FALSE (existential quantifier over an empty set is
false).

"ANY satisfies" is the conservative choice for an event-engine
trigger gate: if author writes `country.stability lt 0.30`, the
author almost certainly means "fire when SOMEONE has low
stability". "ALL satisfy" would require every country to be
unstable, which is a much rarer condition; "FIRST country
satisfies" would be both order-dependent and rarely what an
author wanted. A future M5.x may add per-trigger qualifiers
(`all_of` / `first_of` / `for_country:GER`); M5.2 ships only
ANY.

### 3.3 Combination within an event (`evaluate`)

`evaluate(state, def)` returns true iff EVERY trigger in
`def.triggers` matches the state ("ALL must match" — AND
semantics). The empty-triggers case is **vacuously TRUE**
(mathematical convention; the M5.1 loader rejects empty
triggers, so this case is unreachable through canonical load
paths but the API still pins it for hand-built / synthesised
defs in tests and for defensive readers).

OR semantics are easily expressible by splitting one event into
multiple events with the same `effects` block. NOT semantics
would need a new op or a `negate: true` field — neither ships
in M5.2.

### 3.4 Walk order (`match_events`)

`match_events` walks `state.events` in vector order and emits a
`TriggerMatch` for every matching event. The result vector is
in the same order as `state.events`. Save format guarantees
`state.events` is canonically ordered (insertion order from
scenario load), so the evaluator's output is deterministic
across re-runs.

### 3.5 Non-finite handling

- Non-finite trigger `value` (NaN / ±∞) → trigger never matches.
- Non-finite state value (e.g. a future system writes NaN into
  `country.stability`) → that entity is skipped; if all
  entities have non-finite values the trigger evaluates to
  false. NaN never matches any comparison in this evaluator,
  even though IEEE-754 makes `NaN < x` return false (which
  would be "doesn't match" anyway, so the explicit guard is
  belt-and-braces but pins the contract).

### 3.6 Unknown target / op

Both fall through the dispatch and return false. The M5.1
loader is the gate — it rejects unknown targets / ops at load
time. The evaluator does NOT duplicate the allowlist messaging:
no exception, no `Result<bool, string>`, no log. The
defensive-false behaviour is pinned by tests so a refactor
can't silently change to "throws" or "asserts".

## 4. What's deferred (the M5.3+ horizon)

```text
event FIRING                  (turn a match into a log + history append)
effects APPLICATION           (call policy::apply_policy_effects)
per-actor selection           (which country / which IG triggered)
runner / monthly integration  (auto-evaluate on month boundary)
events.jsonl fire records     (per-fire log entries)
runner CLI flag               (e.g. --events-csv)
cooldown / weight / exclusivity / chained events / choices /
   RNG-driven outcome branches / historical-once gating
broader trigger ops           (eq / ne / between / in)
broader trigger targets       (factions, budget categories,
                               active_policies, current_date, RNG)
trigger logical operators     (all_of / any_of / not /
                               for_country:GER per trigger)
save schema change            (still v13)
UI surface                    (events still absent from
                               map.html / provinces.svg / any CSV)
balance pass                  (canonical events still chosen to
                               not fire on canonical scenario)
docs/milestone-5-checkpoint.md (still not written; deferred
                                until M5 ships multiple
                                cross-cutting surfaces)
```

## 5. Tests added (28)

The per-op / per-target / aggregation / no-mutate matrix:

- **Per-op on country.stability (4)**: `lt` matches at least
  one country; `lt` false when none match; `lte` includes
  equality; `gt` + `gte` semantics.
- **Per country-target (2)**: `country.legitimacy`,
  `country.government_authority.bureaucratic_compliance`.
- **Per interest-group target (3)**: `interest_group.radicalism`
  gt-match; gt-no-match; `interest_group.loyalty`.
- **Empty-entity cases (2)**: country.* on empty countries =
  false; interest_group.* on empty IGs = false.
- **Unknown / non-finite (4)**: unknown target = false; unknown
  op = false; non-finite trigger value = false; non-finite
  state value never matches (NaN handling).
- **AND across triggers (4)**: two-trigger AND both pass; AND
  fails when one fails; cross-scope AND (country + IG);
  empty-triggers vacuously true.
- **match_events ordering (4)**: empty `state.events`;
  no-match; single match has correct index + id_code; multi-
  match preserves canonical order.
- **No-mutate regression (1)**: every call into `match_events`
  / `evaluate` / `trigger_matches` leaves countries / IGs /
  events / logs / applied_commands unchanged.
- **Canonical-shape semantics (2)**: canonical event shape +
  canonical state shape = zero matches (pins the M5.1 fixture
  no-fire property at the evaluator level); drifted state past
  thresholds = both events match.
- **Evaluator is trigger-only (2)**: effects vector contents
  not consulted at evaluation; `match_events` never appends to
  `state.logs` / `state.applied_commands`.

Test counts: 949 doctest cases / 61924 assertions total
(was 921 / 61862 at M5.1 close).

## 6. What M5.2 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no event firing
no log entry on match
no events.jsonl change
no history append
no effects application
no per-actor selection (no "which country/IG triggered" record)
no runner integration / monthly integration
no auto-evaluation cadence
no save format bump (still v13)
no new artefact (still 10)
no new RunnerOptions field / new CLI flag
no new PlayerCommandKind
no new state field
no broader trigger ops (eq / ne / between / in)
no broader trigger targets
no trigger logical operators (all_of / any_of / not / for_country:GER)
no event author tooling
no UI surface (no map.html / provinces.svg / CSV change)
no balance pass
no changes to M5.1 schema
no changes to M1/M2/M3/M4 systems
no changes to scenario_loader / save_system / diagnostics
   (they still own state.events; M5.2 only reads it)
no docs/milestone-5-checkpoint.md (still deferred —
   premature framing at M5.2)
```

M5 remains in progress. M5.2 ships the evaluator; the next
sub-milestone is unspec'd and waits for the reviewer.

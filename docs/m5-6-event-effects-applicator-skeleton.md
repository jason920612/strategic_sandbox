# M5.6 - Event effects applicator skeleton

Companion notes for
`feature/rfc090-m5-06-event-effects-applicator-skeleton`.

**This is RFC-090 M5** (event engine). M5.6 closes the inner
loop of the M5 pipeline: it takes a fired `EventInstance` (M5.4
data; M5.5 firer) plus the matching `EventDefinition` (M5.1
schema) and applies the definition's effects through the
existing M1.5 policy machinery. M5.6 still does NOT integrate
with the runner or monthly pipeline, NOT change `events.jsonl`
semantics, NOT add any new artefact, NOT bump the save format.

The boundary keeps M5 sub-milestone pacing intact:

```text
M5.1 = EventDefinition schema
M5.2 = trigger evaluator (predicate)
M5.3 = actor binding on the evaluator's return shape
M5.4 = event_history data layer + save round-trip
M5.5 = event firer (EventMatch -> EventInstance bridge)
M5.6 = event-effects applicator (this PR)
M5.7+ = runner integration (glue all of the above into the
        monthly pipeline; events.jsonl emission; CLI surface)
```

Each PR is one decoupled surface so a reviewer can read it
end-to-end in one sitting.

## 1. Scope

What ships:

```text
leviathan::systems::event_effects                   (new module)
    ApplyOutcome { effects_applied,
                   faction_targets_updated }         (new struct)
    apply_event_effects(state, instance, def)        (new free function)
include/leviathan/systems/event_effects.hpp          (new header)
src/leviathan/systems/event_effects.cpp              (new impl)
tests/systems/event_effects_test.cpp                 (20 new doctest cases)
policy::apply_effects_to_actor                       (EXTRACTED from
                                                      apply_policy_effects;
                                                      now public)
include/leviathan/systems/policy_system.hpp          (updated decl)
src/leviathan/systems/policy_system.cpp              (refactored to delegate)
CMakeLists updates for the new sources
```

What does NOT change:

```text
no save format bump (still v14)
no new state field
no new artefact (still 10)
no new runner CLI flag
no events.jsonl semantic change
no new PlayerCommandKind
no scenario_loader change
no event_evaluator change (M5.3 evaluator stays read-only)
no event_firer change (M5.5 firer stays as the bridge)
no canonical fixtures change (applicator is opt-in by caller;
   no system invokes it yet)
no M1.5 apply_policy_effects external behavior change
   (the refactor is purely a delegation; same Results, same
    state mutations, same active_policies append — pinned by
    two regression tests)
no docs/milestone-5-checkpoint.md (still deferred — reads
   better alongside the runner-integration PR where the M5
   composition becomes load-bearing for canonical runs)
```

## 2. Reuse vs extract — the design call

The user spec said *"先復用/抽出既有 policy effect application
path"* — reuse OR extract. Two paths to consider:

- **Reuse path (path A)**: have the applicator synthesise a
  `PolicyData{ id_code = event_id_code, effects = def.effects,
  duration_days = 0 }`, call `apply_policy_effects`, then pop
  the entry that landed in `state.countries[actor].active_policies`.
  *Pros*: zero refactor to `policy_system`. *Cons*: clutters
  every event-fire callsite with a synthesised PolicyData and a
  cleanup pass; and if M1.15's `ActivePolicy` semantics evolve
  (e.g. an expiration sweep), events would silently inherit
  policy-shaped lifecycle behaviour they shouldn't have.
- **Extract path (path B)**: factor the actor-validate +
  pre-flight + apply core into a new public free function
  `policy::apply_effects_to_actor(state, actor, effects)`. Have
  both `apply_policy_effects` (existing, M1.5) and
  `event_effects::apply_event_effects` (new, M5.6) call it.
  *Pros*: clean separation — policy-specific bookkeeping stays
  with `apply_policy_effects`; event-specific applicators can
  reuse the effect grammar without inheriting unrelated policy
  state. *Cons*: touches `policy_system` code; needs a M1.5
  regression test pin.

M5.6 picks **path B**. The refactor is mechanical (the existing
function body becomes two functions, glued by a delegation
call); two regression tests pin the M1.5 contract; M1
end-to-end suite + 996 M5.0-era tests all stay green.

### New public helper signature

```cpp
namespace leviathan::systems::policy {

core::Result<ApplyOutcome> apply_effects_to_actor(
    core::GameState&                       state,
    core::CountryId                        actor,
    const std::vector<core::PolicyEffect>& effects);

}
```

Same semantics as the apply-loop inside `apply_policy_effects`:
validates actor, pre-flights every effect (target / op /
finite value), applies in order, ratio-clamps post-op,
faction-broadcast no-op-on-zero-matches. **Does NOT** append
an `ActivePolicy` (that's the policy-specific bookkeeping
`apply_policy_effects` does AFTER calling this helper).

### `apply_policy_effects` post-refactor

```cpp
Result<ApplyOutcome> apply_policy_effects(state, actor, policy) {
    // 1. Validate duration_days (M1.15 cap).
    // 2. Delegate to apply_effects_to_actor(state, actor, policy.effects).
    // 3. On success, append ActivePolicy{policy.id_code, current_date + duration_days}.
}
```

Behaviour identical to pre-refactor; the only observable change
is that errors that previously said `apply_policy_effects:` for
actor-validation now come from `apply_effects_to_actor:`. None
of the M1.5 tests checked that specific prefix.

## 3. M5.6 API

```cpp
namespace leviathan::systems::event_effects {

struct ApplyOutcome {
    int effects_applied         = 0;
    int faction_targets_updated = 0;
};

core::Result<ApplyOutcome> apply_event_effects(
    core::GameState&             state,
    const core::EventInstance&   instance,
    const core::EventDefinition& definition);

}
```

Pre/post-conditions are documented in the header. Summary:

- The caller must ensure `instance.event_id_code ==
  definition.id_code` (M5.6 does not cross-check; the future
  runner integration is the only legitimate caller and it
  always pairs them).
- The applicator picks the first actor in `instance.actors` and
  resolves its `country_id_code` to a `CountryId` via linear
  scan of `state.countries`. ALL effects in
  `definition.effects` go to that one country.
- Empty `instance.actors` → success with `effects_applied = 0`
  (vacuous case; mirrors M5.1 empty-triggers vacuous true →
  M5.5 fire-with-empty-actors → M5.6 no-op).
- Unresolvable `country_id_code` → `Result::failure` with the
  offending id_code in the message. State unchanged (M1.5
  pre-flight atomicity).
- Bad effects (non-finite / unknown target / unknown op) →
  `Result::failure` from the M1.5 pre-flight path. State
  unchanged.

## 4. Actor-selection policy ("first-actor wins")

M5.6 directs **all** effects to **one** country — the country
resolved from `instance.actors.front().country_id_code`. This
is the simplest meaningful policy for a skeleton; it matches
how an author intuitively reads an event ("event X fires for
country Y") and how the M5.5 firer captures the first
satisfying entity per trigger.

A test explicitly pins this against a two-actor instance: GER
first, FRA second, single `country.stability add -0.10`
effect → only GER's stability changes; FRA stays unchanged.

The alternatives (cross-product on multiple actors / per-effect
actor scoping / weighted / random) all belong in a dedicated
selection-policy sub-milestone per the PR #92 review note
(*"do NOT sneak selection-policy variants into the firing /
effects PR"*). M5.6 ships "first wins" only.

## 5. Pre-flight atomicity inherits from M1.5

`apply_effects_to_actor` runs the M1.5 pre-flight pass before
mutating any state. If any effect has a bad target / op / value,
the whole call fails and state is byte-identical with the
pre-call state. M5.6 tests pin this for non-finite value
(infinity), unknown target (`country.no_such_field`), and
unknown op (`mul`). The first valid effect in a list with a
later failing effect is also NOT applied (M1.5 contract).

## 6. What `apply_event_effects` does NOT do

- **`state.event_history` is NOT touched.** Recording was the
  M5.5 firer's job; applying effects doesn't need to re-record
  the fire. A test pins `event_history.size()` stays constant
  across an `apply_event_effects` call.
- **`state.logs` is NOT touched.** No per-effect / per-fire log
  line is emitted to `events.jsonl`. That's the
  runner-integration sub-milestone's call to make (likely via
  a dedicated `event_log` system that consumes
  `state.event_history`).
- **`state.applied_commands` is NOT touched.** Events are not
  player commands.
- **`country.active_policies` is NOT touched.** Pinned by a
  dedicated regression test — the whole point of extracting
  the helper was to keep policy-specific bookkeeping out of
  the event path.

## 7. Tests added (20)

- **Happy path (5)**: country.* effect lands on first actor's
  country; IG-actor's owning country gets the effect;
  multiple effects in def order; budget category route
  (`country.budget.*` grammar); ratio clamping post-op.
- **Pre-flight atomicity (3)**: non-finite effect value
  rejected, state untouched; unknown target rejected, state
  untouched; unknown op rejected, state untouched.
- **Edge cases (4)**: empty actors → no-op success; first-
  actor `country_id_code` doesn't resolve → failure with
  offending id_code in message; empty `country_id_code` on
  first actor rejected; empty effects → success with 0
  applied.
- **First-actor wins (1)**: two-actor instance, only the
  first actor's country gets the effects; second actor's
  country untouched.
- **No side effects (2)**: `event_history` / `logs` /
  `applied_commands` unchanged; `country.active_policies`
  NOT appended (pins the whole reason the helper was
  extracted).
- **End-to-end composition (1)**: full M5.2 → M5.5 → M5.6
  pipeline on a state where two canonical-shape events
  match and apply.
- **M1.5 regression (2)**: existing `apply_policy_effects`
  still produces an `ActivePolicy` entry; existing
  `apply_policy_effects` still rejects bad `duration_days`
  without touching state.
- **`policy::apply_effects_to_actor` directly (2)**: does
  NOT append `ActivePolicy`; invalid actor index rejected
  with state untouched.

Test counts: 1016 doctest cases / 62209 assertions (was
996 / 62148 at M5.5 close).

## 8. What's deferred (the M5.7+ horizon)

```text
runner / monthly integration             (the tick_events composition:
                                           match_events -> record_matches ->
                                           apply_event_effects, glued into
                                           the M1.10 monthly pipeline)
events.jsonl fire records                 (per-fire LogEntry emission)
runner CLI flag                           (e.g. --events-csv,
                                           --skip-event-effects)
event_history-driven gating               (cooldown / historical-once;
                                           caller policy that consults
                                           state.event_history before
                                           record_matches or apply)
chained events / choices /
   RNG-driven outcome branches
selection-policy variants                 (all-actors / weighted / random /
                                           per-effect actor scoping — its
                                           own dedicated PR per PR #92)
broader trigger ops / targets / actor kinds
save schema change beyond v14
UI surface                                (events still absent from
                                           map.html / provinces.svg /
                                           any CSV)
balance pass                              (canonical events still chosen
                                           to not fire on canonical
                                           scenario)
docs/milestone-5-checkpoint.md            (still not written; reads
                                           better alongside runner-
                                           integration where the M5
                                           composition becomes load-
                                           bearing for canonical runs)
```

## 9. What M5.6 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no runner / monthly integration
no events.jsonl change
no log entry on apply (no state.logs append)
no state.applied_commands append
no state.event_history mutation
no country.active_policies mutation from the event path
no auto-fire cadence
no new artefact (still 10)
no save format bump (still v14)
no new RunnerOptions field / CLI flag
no new PlayerCommandKind
no new state field
no per-effect actor selection
   (all effects go to the first actor's country)
no multi-actor cross-product / weighted / random selection
no event_history-driven gating
   (caller policy that consults state.event_history)
no chained events / choices / RNG outcomes
no broader trigger ops / targets / actor kinds
no balance pass
no event author tooling
no UI surface
no changes to event_evaluator / event_firer / scenario_loader
no changes to canonical fixtures
no changes to M1/M2/M3/M4 systems' external behavior
   (the M1.5 refactor preserves the apply_policy_effects
    contract; pinned by regression tests)
no docs/milestone-5-checkpoint.md
```

M5 remains in progress. M5.6 ships the effects applicator
brick; the next sub-milestone is unspec'd and waits for the
reviewer.

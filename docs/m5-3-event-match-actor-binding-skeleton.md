# M5.3 - EventMatch actor-binding skeleton

Companion notes for
`feature/rfc090-m5-03-event-match-actor-binding-skeleton`.

**This is RFC-090 M5** (event engine). M5.3 extends M5.2's
read-only evaluator return shape to record **which entity**
(country or interest group) satisfied each trigger. The next
M5 sub-milestone (firing / history / effects-application) will
consume this binding to know "apply this event's effect to
whom". M5.3 itself still does NOT fire events, NOT append
history, NOT apply effects, NOT integrate with the runner or
monthly pipeline, and NOT change `events.jsonl` semantics.

The boundary is intentional and matches the M5 pacing so far:
M5.1 = schema, M5.2 = predicate, M5.3 = actor binding,
M5.4+ = firer + effects-applicator. Each PR is one decoupled
surface so a reviewer can read it end-to-end in one sitting.

## 1. Scope

What ships:

```text
event_evaluator::TriggerActorKind                 (new enum)
event_evaluator::TriggerActor                     (new struct)
event_evaluator::TriggerEvaluation                (new struct)
event_evaluator::EventMatch                       (replaces TriggerMatch)
event_evaluator::trigger_actor                    (new free function)
event_evaluator::evaluate_match                   (new free function)
event_evaluator::match_events                     (return type widened
                                                   from TriggerMatch
                                                   to EventMatch)
16 new doctest cases in tests/systems/event_evaluator_test.cpp
```

What does NOT change:

```text
no save format bump (still v13)
no new state field
no new artefact (still 10)
no new runner CLI flag
no events.jsonl semantic change
no new PlayerCommandKind
no change to the M5.1 schema (EventDefinition / EventTrigger /
   PolicyEffect reuse)
no change to M5.2 gate semantics — whether a trigger matches
   is unchanged; M5.3 only enriches the return shape
no change to M1/M2/M3/M4 systems
no change to scenario_loader / save_system / diagnostics
no change to canonical fixtures
no change to docs/milestone-5-checkpoint.md (it still doesn't
   exist — checkpoint deferred until M5 has shipped multiple
   cross-cutting surfaces; M5.3 is one more single-surface PR
   on the same event-evaluator module)
```

## 2. API shape

```cpp
namespace leviathan::systems::event_evaluator {

enum class TriggerActorKind { Country, InterestGroup };

struct TriggerActor {
    TriggerActorKind kind    = TriggerActorKind::Country;
    std::string      id_code;
    core::CountryId  country = core::CountryId::invalid();
    std::size_t      index   = 0;
};

struct TriggerEvaluation {
    std::size_t  trigger_index = 0;
    TriggerActor actor;
};

struct EventMatch {
    std::size_t                    event_index = 0;
    std::string                    event_id_code;
    std::vector<TriggerEvaluation> triggers;     // one per def.triggers when matched
};

bool                          trigger_matches(state, EventTrigger);
std::optional<TriggerActor>   trigger_actor  (state, EventTrigger);

bool                          evaluate      (state, EventDefinition);
std::optional<EventMatch>     evaluate_match(state, EventDefinition);

std::vector<EventMatch>       match_events  (state);

}  // namespace
```

The bool predicates (`trigger_matches`, `evaluate`) are kept
verbatim from M5.2 as cheap "did this match at all?" probes.
The new `trigger_actor` / `evaluate_match` are the M5.3
enrichment path: they return the same yes/no answer plus the
actor binding when matched.

`match_events` keeps its name but its return type widens from
`std::vector<TriggerMatch>` to `std::vector<EventMatch>`. The
preserved field names (`event_index`, `event_id_code`) mean
M5.2-style read sites continue to compile; the new `triggers`
vector field is additive.

### Why an enum + a struct, not std::variant

`TriggerActorKind` is a small closed enum (two values today; a
future M5.x might add `Faction` if trigger targets grow into
faction-scoped state). A `std::variant<CountryActor,
InterestGroupActor>` would push the kind tag into the
typesystem at the cost of a heavier visitor / `std::get`
pattern at every read site. The enum-plus-struct shape keeps
read sites simple (`if (a.kind == InterestGroup) { ... }`) and
avoids a separate type per kind that would otherwise differ
only in `kind` and never grow distinct fields. The trade-off
is a small possibility for misuse (reading `id_code` while
ignoring `kind`); the test surface pins that callers always
co-check `kind`.

### Why a single `country` field on both actor kinds

A `TriggerActor` of kind `Country` records the country itself.
A `TriggerActor` of kind `InterestGroup` records the IG **plus
the country it belongs to** (an IG always has an owning
country in M3.1+). Carrying the owning country on the IG actor
makes the future "apply this effect to the owning country" path
trivial without a re-lookup. For kind `Country` the `country`
field is the same as the actor's own id.

## 3. Aggregation semantics (unchanged from M5.2)

M5.3 does not change WHICH entities satisfy a trigger. The
M5.2 contract holds:

- A `country.*` trigger matches when **at least one** country
  in `state.countries` satisfies the comparison.
- An `interest_group.*` trigger matches when **at least one**
  interest group in `state.interest_groups` satisfies it.
- Empty entity list → no match (existential over empty).
- AND across `def.triggers` — every trigger must match.
- Empty triggers vector is vacuously true.
- Unknown target / op, non-finite trigger value, non-finite
  state value all → no match (defensive).

## 4. Actor selection: "first in vector order"

When multiple entities satisfy a trigger, M5.3 records the
**first** one in vector order. "First" is deterministic
because:

- `state.countries` is in canonical scenario-load insertion
  order.
- `state.interest_groups` is in canonical scenario-load
  insertion order.
- The save layer preserves both orders byte-stably.

Collecting **every** satisfying entity per trigger (rather
than just the first) is deferred to a future M5.x. The "first"
choice mirrors how M5.2's bool predicate short-circuits on the
first match — there is no semantic change at the gate; the
M5.3 actor binding just exposes WHICH first-match entity the
M5.2 predicate already implicitly chose.

A future firer / effects-applicator that needs a different
selection policy (random / weighted / all-of) can either
re-query with its own loop or get a richer "all matches" API
added in a dedicated sub-milestone.

## 5. What `nullopt` means

```text
trigger_actor   nullopt iff no entity satisfies
                          (same condition as trigger_matches == false)
                or trigger value is non-finite
                or target is not in the M5.1 allowlist
                or op    is not in the M5.1 allowlist

evaluate_match  nullopt iff any def.triggers fails to match
                          (same condition as evaluate == false)
                — empty triggers vacuously true is still a
                  populated EventMatch with an empty
                  triggers vector
```

`evaluate_match` always populates `event_id_code` on success;
`event_index` is set to 0 by `evaluate_match` (the caller
doesn't know the index of a hand-built def) and overwritten by
`match_events` with the canonical index into `state.events`.

## 6. What's deferred (the M5.4+ horizon)

```text
event FIRING                    (turn a match into a log + history append)
effects APPLICATION             (call policy::apply_policy_effects(state, actor, effect))
"all matches per trigger"       (M5.3 records the first; future may
                                 record every satisfying entity)
selection policy                (random / weighted / first / all);
                                 M5.3 hard-codes "first"
per-trigger actor scoping       (e.g. `for_country: GER` in the trigger
                                 to bind to a specific country)
trigger logical operators       (and / or / not / xor)
runner / monthly integration    (auto-evaluate on month boundary)
events.jsonl fire records       (per-fire log entries)
runner CLI flag                 (e.g. --events-csv)
cooldown / weight / exclusivity / chained events / choices /
   RNG-driven outcome branches / historical-once gating
broader trigger ops             (eq / ne / between / in)
broader trigger targets         (factions, budget categories,
                                 active_policies, current_date, RNG)
save schema change              (still v13)
UI surface                      (events still absent from
                                 map.html / provinces.svg / any CSV)
balance pass                    (canonical events still chosen to
                                 not fire on canonical scenario)
docs/milestone-5-checkpoint.md  (still not written; deferred until
                                 M5 ships multiple cross-cutting
                                 surfaces)
```

## 7. Tests added (16)

`tests/systems/event_evaluator_test.cpp` already carries the
M5.2 28 cases; M5.3 adds 16:

- **trigger_actor per-target binding (2)**: country.* returns
  first satisfying country with correct id_code / country / index;
  interest_group.* returns first satisfying IG with correct
  id_code / country / index.
- **trigger_actor nullopt (3)**: no entity satisfies; unknown
  target / op / non-finite; empty entity list.
- **trigger_actor with duplicate id_code (1)**: pins that the
  evaluator picks by predicate, not by id_code — index is the
  canonical handle.
- **evaluate_match (4)**: nullopt when any trigger fails;
  populated EventMatch when all match (country.* + country.*);
  cross-scope binding (country.* + interest_group.* pick different
  actors); empty triggers vacuously true with empty actors.
- **evaluate_match shared-actor (1)**: three country.* triggers
  on the same country all bind to that country, indices preserved
  in def order.
- **match_events EventMatch binding (1)**: each EventMatch carries
  triggers vector with actor binding; ordering preserved.
- **canonical no-fire regression (1)**: M5.3 doesn't change the
  gate; canonical-shape state still matches zero events.
- **no-mutate regression (1)**: every call into the new
  enrichment API leaves countries / IGs / events / logs /
  applied_commands bit-identical.
- **M5.3 still doesn't touch effects (2)**: evaluate_match
  doesn't consult effects vector; match_events never appends to
  state.logs / state.applied_commands.

Test counts: 965 doctest cases / 62006 assertions total
(was 949 / 61924 at M5.2 close).

## 8. What M5.3 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no event firing
no log entry on match
no events.jsonl change
no history append
no effects application
no per-trigger "all satisfying entities" capture
   (M5.3 records only the first; future M5.x may add)
no selection-policy options (M5.3 hard-codes "first";
   no random / weighted / for_country:GER yet)
no runner integration / monthly integration
no auto-evaluation cadence
no save format bump (still v13)
no new artefact (still 10)
no new RunnerOptions field / new CLI flag
no new PlayerCommandKind
no new state field
no broader trigger ops (eq / ne / between / in)
no broader trigger targets
no trigger logical operators (all_of / any_of / not /
   for_country:GER)
no event author tooling
no UI surface (no map.html / provinces.svg / CSV change)
no balance pass
no changes to M5.1 schema or M5.2 gate semantics
no changes to M1/M2/M3/M4 systems
no changes to scenario_loader / save_system / diagnostics
no docs/milestone-5-checkpoint.md (still deferred —
   premature framing at M5.3)
```

M5 remains in progress. M5.3 ships the actor binding; the
next sub-milestone is unspec'd and waits for the reviewer.

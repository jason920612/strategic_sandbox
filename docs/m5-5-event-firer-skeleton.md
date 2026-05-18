# M5.5 - Event firer skeleton

Companion notes for
`feature/rfc090-m5-05-event-firer-skeleton`.

**This is RFC-090 M5** (event engine). M5.5 ships the **firer
skeleton** — the bridge that turns a M5.3
`event_evaluator::EventMatch` (with per-trigger actor binding)
into an M5.4 `core::EventInstance` and appends it to
`state.event_history`. It is the missing piece that ties the
M5.1 schema, M5.2 evaluator, M5.3 actor binding, and M5.4 data
layer into a usable pipeline — minus the runner integration.

M5.5 still does NOT apply effects, NOT integrate with the
runner or monthly pipeline, NOT change `events.jsonl`
semantics, NOT touch `state.logs` / `state.applied_commands`,
NOT add any new artefact, and NOT bump the save format.

The boundary keeps the M5 sub-milestone pacing intact:

```text
M5.1 = EventDefinition schema
M5.2 = trigger evaluator (predicate)
M5.3 = actor binding on the evaluator's return shape
M5.4 = event_history data layer + save round-trip
M5.5 = event firer (EventMatch -> EventInstance bridge)
M5.6+ = effects application + runner integration
```

Each PR is one decoupled surface so a reviewer can read it
end-to-end in one sitting.

## 1. Scope

What ships:

```text
leviathan::systems::event_firer                  (new module)
    FireOutcome { recorded }                      (new struct)
    record_match(state, EventMatch, fired_on)     (new free function)
    record_matches(state, matches, fired_on)      (new free function)
include/leviathan/systems/event_firer.hpp         (new header)
src/leviathan/systems/event_firer.cpp             (new impl)
tests/systems/event_firer_test.cpp                (13 new doctest cases)
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
no canonical fixtures change (firer is opt-in by caller; no
   system invokes it yet)
no effects application
no log entry on fire
no state.applied_commands append
no M1/M2/M3/M4 system change
no docs/milestone-5-checkpoint.md (still deferred; even after
   five M5 sub-milestones, every one of them has been a
   single decoupled surface, so a multi-surface checkpoint
   would still mostly say "here are five surfaces, they
   compose like X" — that page is more useful written
   alongside the runner-integration PR)
```

## 2. API shape

```cpp
namespace leviathan::systems::event_firer {

struct FireOutcome {
    std::size_t recorded = 0;  // entries appended to event_history
};

void record_match(core::GameState&                                state,
                  const event_evaluator::EventMatch&              match,
                  const core::GameDate&                           fired_on);

FireOutcome record_matches(core::GameState&                                 state,
                           const std::vector<event_evaluator::EventMatch>&  matches,
                           const core::GameDate&                            fired_on);

}  // namespace
```

Three API design choices:

- **Two functions, not one.** `record_match` is the single-
  match form (hand-built matches in tests use it;
  point-in-time "fire this one specific event" surfaces will
  use it too). `record_matches` is the batch form the future
  runner integration will call with the output of
  `event_evaluator::match_events`. Splitting them avoids a
  silly `std::vector<EventMatch>{match}` wrapper at every
  single-shot callsite, and lets the batch function own the
  `FireOutcome` count without duplicating that bookkeeping
  in the single form.
- **`fired_on` is caller-supplied, not read from
  `state.current_date`.** Firing cadence is a *runner*-policy
  decision (does the runner fire on month boundaries, on
  every day, only on explicit player triggers?). Hard-coding
  `state.current_date` here would make those decisions for
  the runner. The caller passes whatever date is meaningful
  to its policy. A test pins that the firer doesn't consult
  `state.current_date` even when it would be the obvious
  default.
- **No `Result<T, E>` return.** The firer is documented as
  *always succeeds*: even when actor-binding lookup fails
  (state internally inconsistent), it writes an empty
  `country_id_code` and the M5.4 save-layer validation
  catches it on the next round-trip. This keeps the firer
  callsite quiet at the firing site — the save layer is the
  load-bearing validation point, not the firer.

## 3. Conversion semantics

For each `event_evaluator::EventMatch` passed in, the firer
produces one `core::EventInstance`:

```text
EventInstance.event_id_code = match.event_id_code
EventInstance.fired_on      = fired_on  (caller-supplied)
EventInstance.actors        = [to_actor(state, te) for te in match.triggers]
```

The per-`TriggerEvaluation` actor conversion is:

```text
Country kind:
  EventInstanceActor.kind            = "country"
  EventInstanceActor.id_code         = te.actor.id_code
  EventInstanceActor.country_id_code = te.actor.id_code   (a country IS its own owning country)
  EventInstanceActor.index           = te.actor.index

InterestGroup kind:
  EventInstanceActor.kind            = "interest_group"
  EventInstanceActor.id_code         = te.actor.id_code
  EventInstanceActor.country_id_code = state.countries[CountryId matches te.actor.country].id_code
  EventInstanceActor.index           = te.actor.index
```

The Country-kind path avoids the
`state.countries[idx].id_code` lookup since `te.actor.id_code`
already carries the same string. The IG-kind path needs the
lookup because the IG's `country` field is a `CountryId`
runtime handle — the save format wants the stable id_code
string. The lookup is a linear scan of `state.countries`
(canonically small; not on a per-tick hot path).

### When the lookup fails

If `te.actor.country` does NOT resolve to any country in
`state.countries` (which shouldn't happen with a well-formed
evaluator output, but might if a caller hand-builds an
`EventMatch` or if `state.countries` shrinks between
evaluation and firing), the firer writes an EMPTY
`country_id_code`. The M5.4 save-layer validation then rejects
the save on the next round-trip with a `country_id_code`
error. This is **deliberate** — surfacing the bug loudly on
the next save beats silently corrupting the history with a
default-country reference.

A test pins this contract so a future refactor can't quietly
change it to "skip the actor" or "throw".

## 4. What the firer does NOT do

- **Effects application.** The matched `EventDefinition`'s
  `effects` vector is **not** consulted. A test (`M5.5
  record_match: effects vector on the EventDefinition is NOT
  consulted`) hand-builds a definition with deliberately
  bogus effects, fires it, and pins that the firer still
  records it normally and the country is not mutated. Effects
  application is a separate M5.x sub-milestone that composes
  with this firer.
- **Log entry on fire.** The firer does not append to
  `state.logs` (which would emit `events.jsonl` entries via
  the M0.6 logging system) nor to `state.applied_commands`
  (which is the M2.4 player-command log). Both vectors are
  pinned untouched by tests. A future M5.x firer-with-logging
  sub-milestone can add per-fire `LogEntry`s if useful, but
  M5.5's minimal surface stays out of the way.
- **Dedup / historical-once gating.** Calling
  `record_matches` twice with the same input appends twice.
  Cooldown / exclusivity / "fire-at-most-once-per-day"
  semantics are caller-policy concerns that gate BEFORE
  calling the firer. M5.5 deliberately doesn't bake any of
  those policies in.
- **Runner / monthly integration.** No `RunnerOptions` field,
  no CLI flag, no auto-fire cadence. The runner-integration
  sub-milestone composes:
  ```cpp
  const auto matches = event_evaluator::match_events(state);
  event_firer::record_matches(state, matches, state.current_date);
  ```
  somewhere appropriate (likely the M1.10 monthly pipeline)
  in its own dedicated PR.
- **Selection-policy variants.** M5.3's "first satisfying
  entity" is still the actor-selection rule. Adding
  all-matches / weighted / `for_country:GER` selection
  variants belongs in a dedicated selection-policy
  sub-milestone — per the PR #92 review note.

## 5. Tests added (13)

- **Per-kind conversion (3)**: country actor; interest_group
  actor (resolves owning country's id_code from state);
  cross-scope match records both actors in def order.
- **`fired_on` policy (1)**: caller-supplied date is recorded
  verbatim; `state.current_date` is NOT consulted and NOT
  modified.
- **Empty-triggers vacuous case (1)**: hand-built match with
  empty triggers becomes EventInstance with empty actors.
- **Broken IG handle (1)**: hand-built IG actor referencing
  a non-existent `CountryId` produces empty `country_id_code`
  and the save layer rejects it on round-trip.
- **`record_matches` batch (3)**: empty input is a no-op;
  preserves input order; multiple calls accumulate (no
  dedup).
- **End-to-end composition (1)**: `match_events` →
  `record_matches` on a state where two canonical-shape
  events match.
- **No side effects (1)**: countries / IGs / events / logs /
  applied_commands / current_date all untouched after
  `record_matches`.
- **Effects vector irrelevant (1)**: matched definition's
  bogus `effects` doesn't break firing and doesn't mutate
  state.
- **Save round-trip (1)**: a recorded entry survives a save
  v14 serialize / deserialize cycle byte-stably.

Test counts: 996 doctest cases / 62148 assertions
(was 983 / 62064 at M5.4 close).

## 6. What's deferred (the M5.6+ horizon)

```text
effects APPLICATION                      (call policy::apply_policy_effects
                                          using EventInstance.actors)
runner / monthly integration             (match_events + record_matches
                                          glued into the M1.10 pipeline)
events.jsonl fire records                (per-fire LogEntry emission)
runner CLI flag                          (e.g. --events-csv)
cooldown / weight / exclusivity /
   historical-once gating                (caller policies that consult
                                          state.event_history)
chained events / choices /
   RNG-driven outcome branches
selection-policy variants                (all-matches / weighted /
                                          for_country:GER — own dedicated
                                          PR per PR #92 review)
broader trigger ops / targets / logical operators
broader actor kinds                      (faction etc.; would grow the
                                          M5.4 save-layer kind allowlist)
save schema change beyond v14
UI surface                               (events still absent from
                                          map.html / provinces.svg /
                                          any CSV)
balance pass                             (canonical events still chosen
                                          to not fire on canonical
                                          scenario)
docs/milestone-5-checkpoint.md           (still not written; reads
                                          better alongside runner-
                                          integration where the
                                          composition becomes load-
                                          bearing for canonical runs)
```

## 7. What M5.5 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no effects application
no log entry on fire (no state.logs append)
no state.applied_commands append
no events.jsonl change
no runner / monthly integration
no auto-fire cadence
no new artefact (still 10)
no new RunnerOptions field / new CLI flag
no new PlayerCommandKind
no new state field
no save format bump (still v14)
no dedup / cooldown / historical-once gating
no chained events / choices / RNG outcomes
no selection-policy variants
no broader trigger ops / targets / actor kinds
no balance pass
no event author tooling
no UI surface
no changes to event_evaluator (M5.3 evaluator stays read-only)
no changes to scenario_loader
no changes to canonical fixtures
   (firer is opt-in by caller; no system invokes it yet)
no changes to M1/M2/M3/M4 systems
no docs/milestone-5-checkpoint.md
```

M5 remains in progress. M5.5 ships the firer brick; the next
sub-milestone is unspec'd and waits for the reviewer.

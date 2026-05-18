# M5.7 - Event runner integration skeleton

Companion notes for
`feature/rfc090-m5-07-event-runner-integration-skeleton`.

**This is RFC-090 M5** (event engine). M5.7 ships the **runner-
integration skeleton**: a single free function
`event_engine::tick_events(state)` that composes the M5.2
evaluator, M5.5 firer, and M5.6 effects applicator into one
"evaluate → record → apply" round. The caller decides WHEN to
invoke it.

The boundary keeps M5 sub-milestone pacing intact:

```text
M5.1 = EventDefinition schema
M5.2 = trigger evaluator (predicate)
M5.3 = actor binding on the evaluator's return shape
M5.4 = event_history data layer + save round-trip
M5.5 = event firer (EventMatch -> EventInstance bridge)
M5.6 = event-effects applicator (definition.effects -> state mutation)
M5.7 = composition helper tick_events(state)  (this PR)
M5.8+ = wire tick_events into the runner / monthly pipeline;
        events.jsonl emission; CLI surface; cooldown /
        historical-once gating
```

Each PR is one decoupled surface so a reviewer can read it
end-to-end in one sitting.

## 1. Why standalone helper, not auto-wired

The user spec for M5.7 said: *"接進月 tick 或一個明確的
`tick_events(state)` helper"* — wire into the monthly tick **or**
ship a standalone helper. M5.7 picks the standalone helper. Two
reasons:

1. **M5 pacing**: every M5 sub-milestone so far has been one
   decoupled surface (schema, evaluator, actor binding, data
   layer, firer, applicator). Auto-wiring into the monthly
   pipeline in the same PR that introduces the composition
   helper would mix two surfaces. M5.7 ships the brick; M5.8
   wires it.

2. **Baseline-rebake risk**: `monthly_pipeline::tick_all_countries`
   is on the canonical 365-day soak path
   (`tests/integration/m1_end_to_end_test.cpp` checks
   byte-identical determinism across save.json + events.jsonl
   + 3 CSVs). The instant `tick_events` runs inside the
   monthly tick, every test that runs a long simulation may
   produce a different save (event_history could grow, or
   stability/legitimacy could shift), and every byte-identical
   pin needs a rebake. Splitting that rebake into its own PR
   keeps M5.7's diff focused on the composition itself.

Result: M5.7 adds a new module + new tests + new doc, and
**zero existing tests change**. The 1016 M5.6-era tests all
pass byte-identically.

## 2. Scope

What ships:

```text
leviathan::systems::event_engine                    (new module)
    TickOutcome { events_matched, events_recorded,
                   events_applied, total_effects_applied }
    tick_events(state) -> Result<TickOutcome>
include/leviathan/systems/event_engine.hpp           (new header)
src/leviathan/systems/event_engine.cpp               (new impl)
tests/systems/event_engine_test.cpp                  (13 new doctest cases)
CMakeLists updates for the new sources
```

What does NOT change:

```text
no auto-wire into runner / monthly pipeline
no save format bump (still v14)
no new state field
no new artefact (still 10)
no new runner CLI flag
no events.jsonl semantic change
no new PlayerCommandKind
no scenario_loader change
no event_evaluator change (M5.3 evaluator stays read-only)
no event_firer change (M5.5 firer signature preserved)
no event_effects change (M5.6 applicator signature preserved)
no policy_system change (M5.6 extraction stays intact)
no canonical fixtures change (tick_events is opt-in by caller)
no M1.5 / M1.10 / M1.17 / M2 / M3 / M4 test changes (no
   auto-wiring means no behaviour drift)
no docs/milestone-5-checkpoint.md (still deferred — reads
   better alongside the M5.8 runner-wiring PR where the
   composition becomes load-bearing for canonical runs)
```

## 3. API shape

```cpp
namespace leviathan::systems::event_engine {

struct TickOutcome {
    int events_matched        = 0;
    int events_recorded       = 0;
    int events_applied        = 0;
    int total_effects_applied = 0;
};

core::Result<TickOutcome> tick_events(core::GameState& state);

}
```

A successful call leaves all four counts populated with the
final tally. A failed call's counts reflect the (matched,
recorded, applied) progress at the time of failure.

## 4. Per-round semantics

```text
1.  matches = event_evaluator::match_events(state)
      - read-only; never mutates state
      - returns vector<EventMatch> with M5.3 actor binding
      - TickOutcome.events_matched = matches.size()

2.  for each match m in matches (canonical order):
      a. def = state.events[m.event_index]
      b. event_firer::record_match(state, m, state.current_date)
           - appends one EventInstance to state.event_history
           - fired_on = state.current_date
           - TickOutcome.events_recorded += 1
      c. instance = state.event_history.back()
      d. event_effects::apply_event_effects(state, instance, def)
           - applies def.effects via M5.6 / M1.5 shared helper
           - on failure: tick_events bails out with Result::failure
           - on success: TickOutcome.events_applied += 1;
                          TickOutcome.total_effects_applied += r.effects_applied
```

### Pinned semantics

- **`fired_on` = `state.current_date`.** This is the ONLY
  place `tick_events` reads `state.current_date`. The firer
  itself remains date-neutral per M5.5; the engine layer
  introduces the convention.
- **Snapshot evaluation.** The evaluator runs **once** at the
  top of `tick_events`. Subsequent apply passes that mutate
  state do NOT re-trigger evaluation in the same round. A
  test pins this: an event that drops a value past another
  event's threshold does not cause the second event to fire
  in the same tick.
- **Canonical order preserved.** Matches fire in vector order
  of `state.events`. The M5.2 evaluator already returns
  matches in canonical order; the engine never reshuffles.
- **No dedup.** Two consecutive `tick_events` calls on the
  same state fire the same events twice. Cooldown /
  historical-once gating belongs to the future M5.x runner-
  integration PR.

## 5. Failure mode

If `apply_event_effects` fails on match index `i`, the engine:

- Returns `Result::failure` with the inner error prefixed by
  `"tick_events: "`.
- Has already appended `i+1` entries to `state.event_history`
  (record happens before apply each round).
- Has applied effects from matches `[0..i-1]` (M5.6 atomicity
  guarantees match `i`'s effects didn't partially land).
- Has NOT applied effects from matches `[i+1..]`.

The caller (a future M5.x runner-integration PR or a test) is
responsible for deciding whether to roll forward or back.

A regression test exercises this path via an interest-group
whose owning country doesn't exist in `state.countries`: the
firer writes an empty `country_id_code`, the applicator
rejects it, `tick_events` returns failure with the matched
event recorded but its effect not applied.

## 6. What `tick_events` does NOT do

- **No state.logs append.** No `events.jsonl` emission. A
  future M5.x event-log sub-milestone may add per-fire
  `LogEntry`s; M5.7 stays silent.
- **No state.applied_commands append.** Events are not player
  commands.
- **No country.active_policies append from the event path.**
  M5.6 ensured this; M5.7 inherits it (tick_events delegates
  to apply_event_effects).
- **No selection-policy change.** M5.6's "first actor wins"
  is still the contract — M5.7 doesn't extend the actor
  selection to all-of / weighted / per-effect.
- **No auto-firing-on-tick wiring.** The runner and monthly
  pipeline don't call `tick_events` yet. That's M5.8's job.
- **No new RunnerOptions field / CLI flag.** A future M5.x
  may add `--events-csv` or `--skip-events`; M5.7 doesn't.

## 7. Tests added (13)

- **Empty cases (3)**: empty state succeeds with all zero
  counts; state with countries but no `state.events` succeeds
  with zero counts; state where no events match emits no
  history entry.
- **Happy path (2)**: one matching event records + applies
  effects; `fired_on` for every recorded instance equals
  `state.current_date`.
- **Multiple matches (1)**: two matches are recorded + applied
  in vector order.
- **Idempotency (1)**: calling twice fires twice (no dedup).
- **Snapshot ordering (1)**: an event that drops a value past
  another event's threshold does NOT cause the second event
  to fire in the same tick (evaluator runs once at the top).
- **Failure path (1)**: applicator failure bubbles up; partial
  state pinned (event recorded; effect not applied).
- **Save round-trip (1)**: after-tick state survives a save
  v14 round-trip byte-stably.
- **No M5.6/M5.7 side effects (3)**: no append to
  `country.active_policies`; no append to `state.logs` or
  `state.applied_commands`; with empty `state.events` the
  whole state is byte-identical before and after `tick_events`
  (a literal `serialize(s) == serialize(s)` pin).

Test counts: 1029 doctest cases / 62274 assertions
(was 1016 / 62209 at M5.6 close).

## 8. What's deferred (the M5.8+ horizon)

```text
runner / monthly integration              (wire tick_events into
                                           tick_all_countries; this
                                           is the byte-identical
                                           determinism rebake PR)
events.jsonl fire records                  (per-fire LogEntry emission
                                           into state.logs)
runner CLI flag                            (e.g. --events-csv,
                                           --skip-events)
event_history-driven gating                (cooldown / historical-once;
                                           caller policy that consults
                                           state.event_history before
                                           record_matches / apply)
chained events / choices /
   RNG-driven outcome branches
selection-policy variants                  (all-actors / weighted /
                                           random / per-effect actor
                                           scoping — own dedicated
                                           PR per PR #92)
broader trigger ops / targets / actor kinds
save schema change beyond v14
UI surface                                 (events still absent from
                                           map.html / provinces.svg /
                                           any CSV)
balance pass                               (canonical events still
                                           chosen to not fire on
                                           canonical scenario)
docs/milestone-5-checkpoint.md             (still not written; reads
                                           better alongside the M5.8
                                           wiring PR where the M5
                                           composition becomes load-
                                           bearing for canonical runs)
```

## 9. What M5.7 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no auto-wire into runner / monthly pipeline
no events.jsonl change
no log entry on tick (no state.logs append)
no state.applied_commands append
no country.active_policies append from the event path
no auto-fire cadence
no new artefact (still 10)
no save format bump (still v14)
no new RunnerOptions field / new CLI flag
no new PlayerCommandKind
no new state field
no dedup / cooldown / historical-once gating
no selection-policy variants (M5.6 first-actor-wins stays)
no chained events / choices / RNG outcomes
no broader trigger ops / targets / actor kinds
no balance pass
no event author tooling
no UI surface
no changes to event_evaluator / event_firer / event_effects
   module APIs
no changes to policy_system module APIs
no changes to scenario_loader
no changes to canonical fixtures
no changes to M1/M2/M3/M4 systems' external behavior
no changes to M1.17 / M2 / M3 / M4 byte-identical determinism
   baselines (no auto-wiring means no determinism drift)
no docs/milestone-5-checkpoint.md
```

M5 remains in progress. M5.7 ships the composition helper;
the next sub-milestone is unspec'd and waits for the reviewer.

# M5.8 - Monthly event tick wiring

Companion notes for
`feature/rfc090-m5-08-monthly-event-tick-wiring`.

**This is RFC-090 M5** (event engine). M5.8 wires the M5.7
composition helper `event_engine::tick_events(state)` into the
M1.9 monthly pipeline at an explicit position: as **step 7**,
the final global step after M3.4 `authority_pressure`. Every
month boundary now evaluates triggers, fires matching events,
and applies their effects — through the same `tick_events`
brick M5.7 shipped.

The wiring is deliberately the only behavioural change. **No
`events.jsonl` change, no CLI flag, no cooldown / historical-
once gating, no selection-policy variants, no new artefact,
no save bump.** All of M5.7's M5.7 deliberate non-goals still
hold; only the *invocation site* moved from "any caller that
wants it" to "the monthly pipeline, every month".

The boundary keeps M5 sub-milestone pacing intact:

```text
M5.1 = EventDefinition schema
M5.2 = trigger evaluator (predicate)
M5.3 = actor binding on the evaluator's return shape
M5.4 = event_history data layer + save round-trip
M5.5 = event firer (EventMatch -> EventInstance bridge)
M5.6 = event-effects applicator (definition.effects -> state mutation)
M5.7 = composition helper tick_events(state)
M5.8 = wire tick_events into monthly_pipeline (this PR)
M5.9+ = events.jsonl emission, CLI surface, cooldown /
        historical-once gating, selection-policy variants, etc.
```

## 1. Scope

What ships:

```text
monthly::MonthlyOutcome::event_tick                  (new field)
monthly::tick_all_countries calls tick_events as     (wiring)
   step 7 after M3.4 authority_pressure
include/leviathan/systems/monthly_pipeline.hpp       (doc update +
                                                      struct field
                                                      + new include)
src/leviathan/systems/monthly_pipeline.cpp           (delegation call)
tests/systems/monthly_pipeline_test.cpp              (5 new doctest cases)
```

What does NOT change:

```text
no save format bump (still v14)
no new state field beyond MonthlyOutcome::event_tick
no new artefact (still 10)
no new runner CLI flag
no events.jsonl semantic change
no new PlayerCommandKind
no scenario_loader change
no event_evaluator / event_firer / event_effects / event_engine
   module API change
no policy_system change
no canonical fixtures change
no M1.5 / M1.10 / M1.17 / M2 / M3 / M4 behavioural change
   (M1.17's 365-day soak and other byte-identical determinism
    baselines all still pass — the canonical 1930 fixtures'
    events deliberately do NOT fire on the canonical scenario,
    so adding tick_events to every monthly tick produces zero
    EventInstance records in canonical runs)
no docs/milestone-5-checkpoint.md (still deferred until M5
   reaches an actual close-out PR; the wiring becoming
   load-bearing for canonical runs would justify the
   checkpoint, but since canonical events still don't fire,
   the load-bearing claim is conditional on author intent,
   not on the wiring itself)
```

## 2. Pipeline ordering

The full monthly pipeline at M5.8:

```text
For every country c in state.countries (in vector order):
  1.  faction::react(state, c)               (M1.6)
  2.  stability::tick(state, c)              (M1.7)
  3.  economy::tick(state, c)                (M1.8)

State-wide steps after the per-country loop:

  4.  interest_group::react(state)           (M3.2)
  5.  interest_group::country_feedback(state) (M3.3)
  6.  interest_group::authority_pressure(state) (M3.4)
  7.  event_engine::tick_events(state)        (M5.8 — this PR)
```

Step 7 sees the freshest values every other monthly system just
produced. Events about "low stability" or "high radicalism"
evaluate against month-end values, not pre-month values. This
matches typical sim patterns and is pinned by a dedicated test
(see §4).

### Why step 7, not earlier

Earlier positions were considered and rejected:

- **Step 0 (before per-country)**: events would evaluate against
  the previous month's final state. The M3.2-M3.4 closed-loop
  feedback wouldn't be visible to the current month's events.
- **Step 3.5 (between per-country and IG steps)**: stability has
  drifted but IG mood / country_feedback / authority_pressure
  haven't run yet. Events that check IG radicalism would see
  stale values.
- **Per-country interleaving**: events would fire mid-loop with
  partial monthly state. Country B's events would see country A's
  post-tick stability but country C's pre-tick stability. The
  resulting fire order would couple to the iteration order in
  a non-obvious way.

Step 7 is the only position where every per-country and every
state-wide system has finished writing.

## 3. Failure propagation

`tick_events` returns `Result::failure` when any matched event's
`apply_event_effects` call fails (M5.7 contract). The wiring
forwards that failure with a monthly-pipeline prefix:

```text
monthly::tick_all_countries: event_engine::tick_events failed: <inner error>
```

A test pins this end-to-end (matched event with a deliberately
bad effect target → M1.5 pre-flight reject → tick_events
failure → tick_all_countries failure with the right prefix).
State at failure inherits M5.7's "partial state" contract:
matches `[0..i]` are recorded in `event_history`; only matches
`[0..i-1]` had effects applied.

## 4. Tests added (5)

- **No `state.events`**: `event_tick.*` counters all zero;
  `event_history` empty.
- **Unreachable trigger**: event whose threshold is below the
  post-tick stability does not fire; `event_history` empty.
- **Matched event fires**: event with threshold above stability
  matches, records, applies; counters reflect 1 / 1 / 1 / 1;
  `event_history` grows by 1; effect lands on the legitimate
  country.
- **Post-M3.4 snapshot ordering**: dry-run the monthly tick to
  observe the post-step-6 stability, then build an event whose
  threshold sits between the pre-month and post-monthly values.
  When the real tick runs, the event fires (proving the
  evaluator saw the post-monthly value, not the pre-month
  snapshot).
- **Failure bubble-up**: matched event with a bad effect target
  → tick_events fails → monthly pipeline fails with the
  `event_engine` / `tick_events failed` prefix; the match was
  RECORDED before apply failed (M5.7 record-then-apply
  contract).

## 5. Why no determinism rebake actually shipped

The M5.7 design note flagged that wiring `tick_events` would
likely rebake M1.17 365-day soak baselines and other
byte-identical pins. In practice, **zero rebake was needed**:

- **Canonical events are deliberately tuned to NOT fire**.
  `low_stability_unrest` triggers at `country.stability < 0.30`;
  canonical GER stability stays above 0.30 across the M1.17
  soak. `radical_interest_group_warning` triggers at
  `interest_group.radicalism > 0.75`; canonical IG radicalism
  stays below 0.75.
- **Tests that build state by hand** (M1.x / M2.x / M3.x /
  M4.x unit tests) don't populate `state.events`, so step 7
  finds nothing to do.
- **`tick_events` on a state with no matches is a strict
  no-op** — no `event_history` mutation, no country field
  drift, no logs / applied_commands / active_policies side
  effects. The result is byte-identical to the pre-wiring
  monthly tick.

Result: all 1029 M5.7-era tests passed unchanged. M5.8 added
5 new tests covering the wiring itself, bringing the total to
**1034 / 62303**.

The implication: when a future author adds an event with values
that *do* fire on the canonical scenario (or when M5.x adds an
events.jsonl emission step, or when balance tuning shifts
canonical fields past existing thresholds), THAT PR will be
the one that rebakes the M1.17 baselines. M5.8 ships the
wiring with zero behavioural drift, and the baseline rebake
becomes "the day the canonical fixtures start firing", not
"the day tick_events landed in the pipeline".

## 6. What's deferred (the M5.9+ horizon)

```text
events.jsonl fire records                  (per-fire LogEntry emission
                                           into state.logs)
runner CLI flag                            (e.g. --events-csv,
                                           --skip-events)
event_history-driven gating                (cooldown / historical-once;
                                           caller policy that consults
                                           state.event_history before
                                           the fire)
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
docs/milestone-5-checkpoint.md             (still not written; M5.8
                                           kept the canonical-runs
                                           "no fire" property so the
                                           composition is wired but
                                           not yet load-bearing for
                                           canonical content)
```

## 7. What M5.8 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no events.jsonl change
no log entry on tick (no state.logs append)
no state.applied_commands append
no country.active_policies append from the event path
no new artefact (still 10)
no save format bump (still v14)
no new RunnerOptions field / CLI flag
no new PlayerCommandKind
no new state field beyond MonthlyOutcome::event_tick
no dedup / cooldown / historical-once gating
no selection-policy variants (M5.6 first-actor-wins stays)
no chained events / choices / RNG outcomes
no broader trigger ops / targets / actor kinds
no balance pass
no canonical fixture change
no event author tooling
no UI surface
no changes to event_evaluator / event_firer / event_effects /
   event_engine module APIs
no changes to policy_system module APIs
no changes to scenario_loader
no changes to M1/M2/M3/M4 systems' external behaviour
no changes to M1.17 / M2 / M3 / M4 byte-identical determinism
   baselines (canonical events deliberately tuned to not fire)
no docs/milestone-5-checkpoint.md
```

M5 remains in progress. M5.8 wires the composition into the
monthly pipeline; the next sub-milestone is unspec'd and waits
for the reviewer.

# M6.1 - EventDefinition true_cause schema

Companion notes for
`feature/rfc090-m6-01-event-definition-true-cause-schema`.

**This is RFC-090 M6** (hidden truth / information distortion),
opening sub-milestone. M6.1 implements **only** RFC-090 §6.1:

> 6.1 為事件加入 true_cause

A single new field on `core::EventDefinition`:

```cpp
struct EventDefinition {
    std::string id_code;
    std::string name;
    std::string description;
    std::string true_cause;     // M6.1 (RFC-090 §6.1)
    std::vector<EventTrigger> triggers;
    std::vector<PolicyEffect> effects;
};
```

`true_cause` is the **author-written truth narrative** — the
description of *what actually caused this event* that future
M6 sub-milestones will hide / distort / leak through the
information-budget / bias-noise pipeline (RFC-090 §§6.2–6.9).
M6.1 itself is **schema-only**: the field is stored,
round-tripped through the save layer, and walked by
diagnostics, but **no system consumes it**. Later M6
sub-milestones (6.2 visible_report, 6.3 information_accuracy,
6.4 reported value, 6.5 bias/noise, 6.6 intelligence-budget
influence, 6.7 corruption influence, 6.8 debug-mode display,
6.9 non-debug hiding) will read it.

## 1. Scope

What ships:

```text
core::EventDefinition.true_cause                  (new required field)
SaveSystem v14 -> v15                              (true_cause required at save layer)
scenario_loader::parse_event_file                  (validates required non-empty true_cause)
ScenarioLoader ManifestEvent.true_cause            (carries the parsed value)
diagnostics::compare_states                        (walks events[N].true_cause)
data/events/1930_core_events.json                  (both canonical events get true_cause)
~13 new doctest cases
   (6 save_system + 4 scenario_loader + 2 diagnostics
    + 1 runtime regression)
```

What does NOT change:

```text
no visible_report (M6.2)
no information_accuracy (M6.3)
no reported value (M6.4)
no bias / noise (M6.5)
no intelligence-budget formula (M6.6)
no corruption formula (M6.7)
no debug mode (M6.8)
no non-debug hiding logic (M6.9)
no EventReport type / artefact
no events.jsonl semantic change
no new state field on EventInstance (instance-level dynamic
   true_cause is a future M6.x concern; M6.1 is definition-
   level only per the spec)
no event_evaluator / event_firer / event_effects /
   event_engine code change
no monthly_pipeline code change
no runner / RunnerOptions / CLI flag change
no new artefact (still 10)
no new PlayerCommandKind
no new EventTrigger / EventInstance / EventInstanceActor field
no UI surface
no balance pass (canonical events still don't fire on canonical
   scenario; the new true_cause text is purely narrative)
no M6.2+ work (this PR opens M6 with only 6.1)
```

## 2. Why definition-level (not instance-level)

The spec asked for `true_cause` on `EventDefinition`, not
`EventInstance`. Two reasons preserved here for the next
reviewer:

1. **Author-written narrative belongs with the definition.**
   The author writes one truth string when they author the
   event; firing the same event in two different countries
   doesn't change the author's intent. The actor binding
   (which country/IG triggered) is already captured on
   `EventInstance.actors` (M5.3 + M5.4); future M6 sub-
   milestones can compose the actor + the definition's
   `true_cause` to produce per-fire narrative without
   duplicating the string.
2. **Instance-level dynamic `true_cause` is M6.x scope.** If
   a future sub-milestone wants per-fire computed narrative
   ("GER's stability fell because of military mutiny" vs
   "GER's stability fell because of an austerity backlash"),
   that's a richer feature that the spec explicitly defers.
   M6.1 ships the static, definition-level baseline.

## 3. Why required non-empty

The spec offered two options:

- *true_cause required string, may be empty*
- *true_cause required non-empty string (preferred)*

The spec explicitly preferred non-empty: *"RFC-090 M6.1 的目
的就是讓事件有真相描述；空字串沒有價值"*. M6.1 implements
that: missing / wrong-type / empty are all rejected at:

- **scenario loader** (`parse_event_file`): rejected with the
  existing `need_string_nonempty` helper (same shape used for
  `id` and `name`).
- **save system** (`deserialize`): rejected explicitly with
  the messages
  `<source>: events[N]: 'true_cause' missing or not a string`
  and
  `<source>: events[N]: 'true_cause' must be non-empty`.

The rejection messages match the existing M5.1 style so a
future error-message audit can be done with one grep.

## 4. Save schema bump v14 -> v15

A v14 save's `events[]` entries lacked `true_cause`. Silently
defaulting to an empty string on reload would erase author
intent (and the M5.0 deferred-content snapshot would diverge
from the M6.1 author intent the moment a v14 save was
re-loaded under M6.1 code). We bump strictly under the M0.8
rule.

At the save-file level, `true_cause` is **REQUIRED non-empty**
for every event entry. Missing / wrong-type / empty all
rejected loudly. `event_history` schema is **unchanged** in
M6.1 — the field lives on `EventDefinition`, not
`EventInstance`. v14 saves are rejected with `supports 15`
in the message.

## 5. Canonical fixture

`data/events/1930_core_events.json` gains a `true_cause`
field on each of the two existing canonical events:

| event                                | true_cause                                                                                  |
| ------------------------------------ | ------------------------------------------------------------------------------------------- |
| `low_stability_unrest`               | "The country's actual stability has fallen below the unrest threshold."                     |
| `radical_interest_group_warning`     | "An interest group's actual radicalism has crossed the warning threshold."                  |

Thresholds, triggers, and effects are **unchanged**. No new
event definitions ship. The new strings are factual
descriptions of the underlying numeric state — they don't
imply any new game behaviour, just narrative.

The canonical fixture's "deliberately does NOT fire on the
canonical scenario" property from M5.1 holds verbatim:
canonical GER stability stays above 0.30, canonical IG
radicalism stays below 0.75. M6.1 adds zero behavioural
drift; `M1.17 / M2 / M3 / M4 / M5` byte-identical determinism
baselines all still pass.

## 6. Event engine behaviour (unchanged)

This is the load-bearing M6.1 contract. **No event runtime
code changes.** Specifically:

- `event_evaluator::match_events` / `evaluate` /
  `trigger_matches` / `trigger_actor` / `evaluate_match`
  evaluate triggers exactly the same way; `true_cause` is
  not in any trigger predicate.
- `event_firer::record_match` / `record_matches` produce
  `EventInstance` records carrying only the existing M5.3
  actor-binding fields. `true_cause` is not copied to the
  instance (it's looked up from the definition by id_code at
  any future read site).
- `event_effects::apply_event_effects` applies
  `definition.effects` exactly as before; `true_cause` is
  read-but-not-consulted by the applicator.
- `event_engine::tick_events` composes the above in the same
  order with the same snapshot-evaluation semantics.
- `monthly::tick_all_countries` runs `tick_events` as step 7,
  unchanged.

A dedicated runtime regression test (`event_engine_test.cpp`
M6.1 case) pins this: two states differing **only** in
`true_cause` produce identical `TickOutcome` counters,
identical post-tick country state, and identical
`event_history` content.

## 7. Diagnostics

`diagnostics::compare_states` walks `events[N].true_cause`
after `events[N].description`:

```text
events.size()
events[N].id_code
events[N].name
events[N].description
events[N].true_cause                          <- M6.1
events[N].triggers.size()
events[N].triggers[M].target
events[N].triggers[M].op
events[N].triggers[M].value
events[N].effects.size()
events[N].effects[M].target
events[N].effects[M].op
events[N].effects[M].value
```

A test pins that a `true_cause` mismatch produces exactly
one mismatch at the path `events[0].true_cause` (and no
other paths drift).

## 8. Tests added (~13)

- **Save (6)**: serialize emits `"true_cause":`; canonical
  round-trip preserves `true_cause`; v14 save rejected with
  `supports 15`; v15 missing `true_cause` rejected; v15
  non-string `true_cause` rejected; v15 empty `true_cause`
  rejected.
- **Scenario loader (4)**: canonical-shape events carry
  authored `true_cause`; missing rejected; wrong-type
  rejected; empty rejected.
- **Diagnostics (2)**: `true_cause` mismatch reported at
  `events[0].true_cause`; identical events with identical
  `true_cause` produce no mismatch.
- **Runtime regression (1)**: `tick_events` produces
  identical outcome + identical post-tick state when only
  `true_cause` differs between two otherwise-equal states.

All 1039 M5.10-era tests still pass after fixture migration
(every hand-built EventDefinition now sets `true_cause`;
every hand-built save JSON now includes `"save_version": 15`
+ `"true_cause"`).

Total: **1052 doctest cases / 62400 assertions** (was 1039 /
62364 at M5.10 close).

## 9. What's deferred (the M6.2+ horizon, per RFC-090)

```text
M6.2  visible_report
M6.3  information_accuracy
M6.4  reported value
M6.5  bias / noise
M6.6  intelligence-budget influence
M6.7  corruption influence
M6.8  debug mode displays truth
M6.9  non-debug mode hides truth
```

Each is its own dedicated sub-milestone PR. M6.1 ships the
schema foundation that all of the above will consume.

## 10. What M6.1 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no visible_report (M6.2)
no information_accuracy (M6.3)
no reported value (M6.4)
no bias / noise (M6.5)
no intelligence-budget formula (M6.6)
no corruption formula (M6.7)
no debug mode (M6.8)
no non-debug hiding (M6.9)
no EventReport / EventReportData type
no event report artefact
no events.jsonl semantic change
no UI / map integration
no new event definitions
no new EventTrigger / EventInstance / EventInstanceActor field
no trigger / effect behaviour change
no event_evaluator / event_firer / event_effects /
   event_engine / monthly_pipeline code change
no new artefact (still 10)
no new RunnerOptions field / CLI flag
no new PlayerCommandKind / new state field beyond the
   EventDefinition.true_cause field
no RNG draws from the event pipeline (M5-era RNG-free
   property preserved)
no rebake of M1.17 / M2 / M3 / M4 / M5 byte-identical
   determinism baselines (canonical events still don't fire;
   true_cause is narrative metadata, not behaviour)
no M6.2 work in this PR
no docs/milestone-6-checkpoint.md
no docs/milestone-6-result.md
no "M6 closed" wording — M6 just opened
```

M6 is in progress. M6.1 opens it; the next sub-milestone is
M6.2 (`visible_report`) per RFC-090 §6.2, but M6.2 is
**unspec'd here** and waits for an explicit `做M6.2`
go-ahead.

# M6.2 - EventDefinition visible_report schema

Companion notes for
`feature/rfc090-m6-02-event-definition-visible-report-schema`.

**This is RFC-090 M6** (hidden truth / information distortion),
second sub-milestone. M6.2 implements **only** RFC-090 §6.2:

> 6.2 加入 visible_report

A single new field on `core::EventDefinition`:

```cpp
struct EventDefinition {
    std::string id_code;
    std::string name;
    std::string description;
    std::string visible_report;   // M6.2 (RFC-090 §6.2)
    std::string true_cause;       // M6.1 (RFC-090 §6.1)
    std::vector<EventTrigger> triggers;
    std::vector<PolicyEffect> effects;
};
```

`visible_report` is the **author-written public-facing
fired-report description** — what the player would see when
the event fires. M6.3 (`information_accuracy`), M6.4
(reported value), M6.5 (bias/noise), M6.6 (intelligence
budget), M6.7 (corruption), M6.8 (debug mode), M6.9
(non-debug hiding) are the M6 sub-milestones that will
*distort, blur, hide, or selectively leak* this string
(and the M6.4 numeric reported value) toward the truth in
`true_cause`. M6.2 itself is **schema-only**: the field is
stored, round-tripped, and walked by diagnostics, but
**no system consumes it**.

## 1. Scope

What ships:

```text
core::EventDefinition.visible_report               (new required field)
SaveSystem v15 -> v16                               (visible_report required at save layer)
scenario_loader::parse_event_file                   (validates required non-empty visible_report)
ScenarioLoader ManifestEvent.visible_report         (carries the parsed value)
diagnostics::compare_states                         (walks events[N].visible_report)
data/events/1930_core_events.json                   (both canonical events get visible_report)
~13 new doctest cases
   (6 save_system + 4 scenario_loader + 2 diagnostics
    + 1 runtime regression)
```

What does NOT change:

```text
no information_accuracy (M6.3)
no reported value (M6.4)
no bias / noise (M6.5)
no intelligence-budget formula (M6.6)
no corruption formula (M6.7)
no debug mode (M6.8)
no non-debug hiding logic (M6.9)
no EventReport type / artefact
no events.jsonl semantic change
no new state field on EventInstance (instance-level
   dynamic visible_report — e.g. a per-fire computed
   distorted report — is M6.3+ scope; M6.2 is the
   definition-level baseline only)
no event_evaluator / event_firer / event_effects /
   event_engine code change
no monthly_pipeline code change
no runner / RunnerOptions / CLI flag change
no new artefact (still 10)
no new PlayerCommandKind
no new EventTrigger / EventInstance / EventInstanceActor field
no UI surface
no balance pass (canonical events still don't fire on canonical
   scenario; the new visible_report text is purely narrative)
no M6.3+ work (one sub-milestone per PR)
```

## 2. Field ordering: public → fired-report → truth → mechanics

The M6 narrative fields sit in a deliberate ordering:

```text
description       public, always-visible, since M5.1
visible_report    public, fired-report, M6.2 (this PR)
true_cause        author truth, M6.1
triggers/effects  mechanics, M5.1
```

This reads top-to-bottom as the *information chain* a future
M6.3+ pipeline will model:

- `description` is what the player can read about the event
  *type* even when no instance has fired (e.g. a tooltip).
- `visible_report` is what the player gets shown when an
  instance fires — possibly **after** M6.3
  `information_accuracy` filters / M6.5 bias/noise applies.
- `true_cause` is what *actually* caused the fire — what
  the M6.8 debug mode displays and the M6.9 non-debug mode
  hides.

M6.2 ships only the `visible_report` storage. The
distortion / hiding pipeline is M6.3–M6.9.

## 3. Why required non-empty

Inherits the M6.1 rule (preferred by the spec + reaffirmed
by the PR #100 reviewer):

> 大量 test fixtures 從 v14 遷到 v15 是正常的 schema bump
> 成本。之後 M6.2 加 `visible_report` 時，也會再次 bump
> schema；請保持同樣規則，不要把 `visible_report` default
> 成空字串，因為那同樣會抹掉 author intent。

M6.2 implements that: missing / wrong-type / empty are all
rejected at:

- **scenario loader** (`parse_event_file`): rejected with the
  existing `need_string_nonempty` helper (same shape used for
  `id`, `name`, and `true_cause`).
- **save system** (`deserialize`): rejected explicitly with
  the messages
  `<source>: events[N]: 'visible_report' missing or not a string`
  and
  `<source>: events[N]: 'visible_report' must be non-empty`.

The rejection messages mirror M6.1's `true_cause` style for
a future error-message audit.

## 4. Save schema bump v15 -> v16

A v15 save's `events[]` entries lacked `visible_report`.
Silently defaulting to an empty string on reload would erase
author intent (the public-facing fired-report description).
We bump strictly under the M0.8 rule.

At the save-file level, `visible_report` is **REQUIRED
non-empty** for every event entry. Missing / wrong-type /
empty all rejected loudly. `event_history` schema is
**unchanged** in M6.2 — same property as M6.1; the new field
lives on `EventDefinition`, not on the fired-instance record.
v15 saves are rejected with `supports 16` in the message.

Field order in the serialised JSON mirrors the in-memory
struct order: `id_code` → `name` → `description` →
`visible_report` → `true_cause` → `triggers` → `effects`.

## 5. Canonical fixture

`data/events/1930_core_events.json` gains a `visible_report`
field on each of the two existing canonical events:

| event                                | visible_report                                                                                  |
| ------------------------------------ | ----------------------------------------------------------------------------------------------- |
| `low_stability_unrest`               | "Reports indicate growing unrest among the population; stability appears to be slipping."        |
| `radical_interest_group_warning`     | "Intelligence reports a sharp uptick in radical rhetoric from one of the country's interest groups." |

Thresholds, triggers, and effects are **unchanged**. No new
event definitions ship. The new strings are believable
fired-report text — what an intelligence brief might say —
not numeric or programmatic content.

The canonical fixture's "deliberately does NOT fire on the
canonical scenario" property from M5.1 holds verbatim:
canonical GER stability stays above 0.30, canonical IG
radicalism stays below 0.75. M6.2 adds zero behavioural
drift; M1.17 / M2 / M3 / M4 / M5 byte-identical determinism
baselines all still pass.

## 6. Event engine behaviour (unchanged)

This is the load-bearing M6.2 contract. **No event runtime
code changes.** Same as M6.1: every event module pretends
the new field doesn't exist. A dedicated runtime regression
test (`event_engine_test.cpp` M6.2 case, mirroring the M6.1
test) pins this: two states differing **only** in
`visible_report` produce identical `TickOutcome` counters,
identical post-tick country state, and identical
`event_history` content.

## 7. Diagnostics

`diagnostics::compare_states` walks `events[N].visible_report`
between `events[N].description` and `events[N].true_cause`
(reflecting the in-memory field order):

```text
events.size()
events[N].id_code
events[N].name
events[N].description
events[N].visible_report                       <- M6.2
events[N].true_cause                            <- M6.1
events[N].triggers.size()
events[N].triggers[M].target
events[N].triggers[M].op
events[N].triggers[M].value
events[N].effects.size()
events[N].effects[M].target
events[N].effects[M].op
events[N].effects[M].value
```

A test pins that a `visible_report` mismatch produces
exactly one mismatch at the path `events[0].visible_report`.

## 8. Tests added (~13)

- **Save (6)**: serialize emits `"visible_report":`; canonical
  round-trip preserves `visible_report`; v15 save rejected
  with `supports 16`; v16 missing `visible_report` rejected;
  v16 non-string `visible_report` rejected; v16 empty
  `visible_report` rejected.
- **Scenario loader (4)**: canonical-shape events carry
  authored `visible_report`; missing rejected; wrong-type
  rejected; empty rejected.
- **Diagnostics (2)**: `visible_report` mismatch reported
  at `events[0].visible_report`; identical events with
  identical `visible_report` produce no mismatch.
- **Runtime regression (1)**: `tick_events` produces
  identical outcome + identical post-tick state when only
  `visible_report` differs between two otherwise-equal
  states.

All 1052 M6.1-era tests still pass after fixture migration
(every hand-built EventDefinition now sets `visible_report`;
every hand-built save JSON now includes `"save_version": 16`
+ `"visible_report"`; M6.1 negative tests that fed a bad
`true_cause` now also include a valid `visible_report` so
the loader reaches the true_cause check).

Total: **1065 doctest cases / 62436 assertions** (was 1052 /
62400 at M6.1 close).

## 9. What's deferred (the M6.3+ horizon, per RFC-090)

```text
M6.3  information_accuracy
M6.4  reported value
M6.5  bias / noise
M6.6  intelligence-budget influence
M6.7  corruption influence
M6.8  debug mode displays truth
M6.9  non-debug mode hides truth
```

Each is its own dedicated sub-milestone PR. M6.2 ships the
public-facing string that M6.3+ will distort / hide.

## 10. What M6.2 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
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
   EventDefinition.visible_report field
no RNG draws were added by this M6.2 schema-only PR (later
   PR #111 intentionally added event-engine RNG draws for
   RFC-090 §5.7 weighted selection)
no rebake of M1.17 / M2 / M3 / M4 / M5 byte-identical
   determinism baselines (canonical events still don't fire;
   visible_report is narrative metadata, not behaviour)
no M6.3 work in this PR
no docs/milestone-6-checkpoint.md
no docs/milestone-6-result.md
no "M6 closed" wording
```

M6 remains in progress. M6.2 ships the public-facing
fired-report string; the next sub-milestone is M6.3
(`information_accuracy`) per RFC-090 §6.3, but M6.3 is
**unspec'd here** and waits for an explicit `做M6.3`
go-ahead.

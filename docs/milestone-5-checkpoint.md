# Milestone 5 Checkpoint

> **Historical checkpoint.** M5 closed at **M5.10**. The
> canonical M5 record is now
> [`milestone-5-result.md`](milestone-5-result.md) — read
> that for the authoritative M5 ledger / dataflow /
> 10-artefact contract / save-format v14 floor /
> architectural invariants / deferred items / next-
> milestone candidates. This checkpoint file is kept
> verbatim below for archaeology: it captures the M5
> in-progress snapshot as it stood at M5.9, just before
> the M5.10 close-out PR. Future readers wanting "what
> does the M5 contract look like right now?" should read
> `milestone-5-result.md`, not this file. Future readers
> wanting "how did the M5 contract evolve sub-milestone by
> sub-milestone?" can read the per-PR design notes
> `docs/m5-NN-*.md` alongside this snapshot.

Status: **historical (M5 closed at M5.10)**

Originally written at M5.9 — companion notes for
`feature/rfc090-m5-09-event-observability-checkpoint`.

M5.9 was the **M5 status snapshot** — a single-page page reading
"what does the event engine look like right now?" without
piecing together nine per-sub-milestone notes. M5.9 was **NOT**
the M5 exit report; M5 was still in progress at the time. The
pattern mirrored M3.7 / M4.9 / M4.14 / M4.18 / M4.22: docs +
integration tests, no new gameplay. M5.10 then took over as the
M5 close-out (publishing `milestone-5-result.md` + flipping
READMEs + annotating this file as historical).

The companion exit report `docs/milestone-5-result.md` is
**deliberately not written yet**. The close-out lands when the
reviewer decides M5 is done, not at any checkpoint.

Each refresh (this initial one at M5.9, and any future M5.x
checkpoint refreshes) is a **checkpoint, not an exit report**.
No new system, formula, artefact, save-schema bump, gameplay
branch, runner CLI flag, RunnerOptions field, or new
PlayerCommandKind ships in a checkpoint refresh.

## 1. M5 sub-milestones shipped

```text
M5.1   EventDefinition trigger/effect schema foundation
       (save v12 -> v13; loader + validator + store only;
        canonical fixture data/events/1930_core_events.json
        wired into both 1930 manifests)
M5.2   trigger evaluator skeleton
       (event_evaluator::match_events: AND across triggers;
        ANY-entity-satisfies for country.* / interest_group.*;
        lt / lte / gt / gte ops; defensive false on unknown
        target/op/non-finite)
M5.3   EventMatch actor-binding skeleton
       (TriggerActorKind / TriggerActor / TriggerEvaluation;
        renamed TriggerMatch -> EventMatch with triggers vector;
        new trigger_actor / evaluate_match free fns;
        match_events widened to vector<EventMatch>;
        "first in vector order" actor selection)
M5.4   EventInstance / event_history data skeleton
       (save v13 -> v14; event_history vector on GameState;
        per-actor strings stable across save round-trip; no
        cross-check of event_id_code against state.events)
M5.5   event firer skeleton
       (record_match / record_matches bridge converting M5.3
        EventMatch -> M5.4 EventInstance; fired_on caller-
        supplied; broken actor -> empty country_id_code ->
        save layer rejects; no dedup)
M5.6   event effects applicator skeleton
       (apply_event_effects via shared M1.5/M5.6-extracted
        policy::apply_effects_to_actor helper; first-actor-
        wins; M1.5 pre-flight atomicity inherited; does NOT
        append country.active_policies)
M5.7   event runner integration skeleton (standalone helper)
       (event_engine::tick_events composition; snapshot
        evaluation -- cascade events don't fire in same tick;
        no dedup; failure path with partial state)
M5.8   monthly event tick wiring
       (event_engine::tick_events as step 7 of
        monthly::tick_all_countries, after M3.4
        authority_pressure; MonthlyOutcome gains event_tick
        field; failure propagates with "tick_events failed"
        prefix; zero canonical determinism rebake needed)
M5.9   event observability checkpoint (this PR — docs + 5
       end-to-end integration tests; zero gameplay change)
```

## 2. Current M5 dataflow

```text
scenario manifest "events": [ "events/<file>.json" ]
  -> scenario_loader::parse_event_file               (M5.1)
       (per-file event-array parser; target/op allowlist;
        finite value; non-empty triggers; effects validation
        mirrors data_loader::parse_policy)
  -> scenario_loader::load_into_state
       (cross-file id_code uniqueness; appends to state.events)

state.events
  -> save_system::serialize       (root-level "events" array, save v14)
  -> save_system::deserialize     (allowlist re-checked at load)
  -> diagnostics::compare_states  (events.size + per-field walk)

Monthly tick (every state.current_date.month_changed):

  per-country (in vector order):
    1. faction::react              (M1.6)
    2. stability::tick             (M1.7)
    3. economy::tick               (M1.8)

  state-wide:
    4. interest_group::react              (M3.2)
    5. interest_group::country_feedback   (M3.3)
    6. interest_group::authority_pressure (M3.4)
    7. event_engine::tick_events          (M5.8 - this is the wiring)

Inside event_engine::tick_events:

  matches = event_evaluator::match_events(state)       (M5.2)
    -> vector<EventMatch> with per-trigger actor binding (M5.3)

  for each match m in canonical order:
    a. def = state.events[m.event_index]
    b. event_firer::record_match(state, m, state.current_date)  (M5.5)
       -> appends EventInstance to state.event_history (M5.4)
    c. instance = state.event_history.back()
    d. event_effects::apply_event_effects(state, instance, def)  (M5.6)
       -> policy::apply_effects_to_actor (M5.6-extracted from M1.5)
       -> mutates the country resolved from
          instance.actors.front().country_id_code

state.event_history
  -> save_system::serialize         (root-level "event_history" array)
  -> save_system::deserialize       (per-entry kind allowlist)
  -> diagnostics::compare_states    (event_history.size + per-entry walk)
```

## 3. M5 schema (frozen at M5.4)

```cpp
struct EventTrigger {
    std::string target;       // M5.1 allowlist (see §4)
    std::string op;           // M5.1 allowlist (see §4)
    double      value = 0.0;
};

struct EventDefinition {
    std::string               id_code;
    std::string               name;
    std::string               description;
    std::vector<EventTrigger> triggers;       // non-empty at load
    std::vector<PolicyEffect> effects;        // may be empty
};

struct EventInstanceActor {
    std::string kind;             // "country" | "interest_group"
    std::string id_code;
    std::string country_id_code;  // owning country (self for kind=country)
    std::size_t index = 0;        // runtime hint, not authoritative
};

struct EventInstance {
    std::string                     event_id_code;
    GameDate                        fired_on;
    std::vector<EventInstanceActor> actors;
};

// In GameState:
std::vector<EventDefinition> events;          // schema (M5.1)
std::vector<EventInstance>   event_history;   // history (M5.4)
```

The runtime evaluator surface (M5.2/M5.3) and runtime firer
surface (M5.5) layer on top of these data types; they are not
themselves part of the schema and would change shape without
touching the save format.

## 4. M5 allowlists (frozen at M5.1)

**Trigger `target`** (5 entries):

```text
country.stability
country.legitimacy
country.government_authority.bureaucratic_compliance
interest_group.radicalism
interest_group.loyalty
```

**Trigger `op`** (4 entries):

```text
lt   (strict less than)
lte  (less than or equal)
gt   (strict greater than)
gte  (greater than or equal)
```

**Trigger `value`**: required finite double (rejected on NaN /
±∞ via `std::isfinite`).

**Effects**: target/op required non-empty strings + finite
value at load. **No allowlist at load** — mirrors
`data_loader::parse_policy`. The target/op allowlist for
effects lives in `policy::apply_effects_to_actor` (M1.5/M5.6)
and the event applicator inherits it for free.

**Actor kind** (save-layer allowlist, 2 entries — M5.4):

```text
country
interest_group
```

## 5. Current artefacts (unchanged from M4)

End-of-run produces ten files; M5 added zero artefacts.

```text
save.json                                  (M0.8,  required)  v14 since M5.4
events.jsonl                               (M0.6,  required)  M5 did NOT change semantics
interest_groups.csv                        (M3.5,  unconditional)
interest_group_country_feedback.csv        (M3.6,  unconditional)
interest_group_authority_pressure.csv      (M3.6,  unconditional)
provinces.svg                              (M4.2,  unconditional, 9th)
map.html                                   (M4.5,  unconditional, 10th)
summary.csv                                (M0.10, opt-in)
countries.csv                              (M1.14, opt-in)
factions.csv                               (M1.16, opt-in)
```

`events.jsonl` is the M0.6 lifecycle log. M5 emits zero
event-fire records into it; the M5.9 integration tests pin
that the canonical event id_codes never appear in
`events.jsonl` even when their definitions are loaded.

## 6. Current invariants

Pinned at M5.9 close. Each property is enforced either by a
test, a code path, or the lack of an opposing code path.

```text
save format is v14
v13 saves are rejected loudly (supports 14)
state.event_history is the only writeable surface a firer
  produces; no system reads it to drive simulation yet
events.jsonl semantics unchanged from M0.6
no event id_code appears in events.jsonl
no event fire produces a state.applied_commands entry
  (events aren't player commands)
no event fire produces a country.active_policies entry
  (events aren't policies)
runner CLI has no event-related flag (no --events-csv,
  no --skip-events, no --events-jsonl)
no new RunnerOptions field for events
artefact set is still 10 (no event_history.csv /
  events.csv / event_log.csv etc.)
no new PlayerCommandKind for events
no new RNG draws from the event pipeline
   (M5.1-M5.9 are RNG-free)
M5.6 first-actor-wins selection policy is the contract
   (no all-of / weighted / random / for_country:X variants)
no cooldown / historical-once gating ships
no chained events / event-chains / choice options
no event author tooling (UI, validation CLI, etc.)
canonical 1930 events do NOT fire on canonical state by
  construction (low_stability_unrest threshold 0.30 vs
  canonical GER stability 0.55+; radical_interest_group_
  warning threshold 0.75 vs canonical IG radicalism 0.10)
M1.17 / M2 / M3 / M4 byte-identical determinism baselines
  remain byte-identical with M5.7-era output
event tick is the LAST step of monthly::tick_all_countries
  (after M1.6/M1.7/M1.8 per-country + M3.2/M3.3/M3.4
  state-wide); reads post-step-6 snapshot
event_engine::tick_events is the ONLY caller of
  event_evaluator::match_events + event_firer::record_match
  + event_effects::apply_event_effects together
state.event_history grows ONLY through event_firer
  (M5.5 record_match / record_matches — no direct
  manipulation by any other system)
PolicyEffect shape is unchanged from M1.4
EventId still defined in core/ids.hpp (reserved for future
  use; not on any current code path)
M1/M2/M3/M4 systems are byte-identical with M4 close-out
  for their external behaviour
```

## 7. Deferred items (M5.10+ horizon)

Pulled forward from the per-sub-milestone notes for reader
convenience. None of these are committed; each would be its
own dedicated sub-milestone PR.

```text
events.jsonl fire records                  (per-fire LogEntry
                                           emission into
                                           state.logs)
runner CLI flag                            (e.g. --events-csv,
                                           --skip-events,
                                           --events-jsonl)
event_history-driven gating                (cooldown /
                                           historical-once /
                                           fire-at-most-N-times-
                                           per-period; caller
                                           policy that consults
                                           state.event_history
                                           before tick_events)
selection-policy variants                  (all-actors /
                                           weighted / random /
                                           for_country:GER /
                                           per-effect actor
                                           scoping; own
                                           dedicated PR per PR
                                           #92 review)
chained events / choices /
   RNG-driven outcome branches             (parent/child event
                                           id; choice options
                                           on EventInstance;
                                           RNG for outcome
                                           selection)
broader trigger ops                        (eq / ne / between /
                                           in / not-eq)
broader trigger targets                    (factions, budget
                                           categories,
                                           active_policies,
                                           current_date, RNG
                                           state)
trigger logical operators                  (all-of / any-of /
                                           not / xor per
                                           event-level
                                           combination)
broader actor kinds                        (faction etc.; would
                                           require growing the
                                           M5.4 kind allowlist)
event author tooling                       (validation CLI,
                                           schema doc generator,
                                           "what would fire?"
                                           checker)
UI surface                                 (events in map.html
                                           / provinces.svg /
                                           dedicated CSV /
                                           player notifications)
balance pass                               (canonical events
                                           still deliberately
                                           don't fire — first
                                           PR to ship a firing
                                           canonical event
                                           also rebakes M1.17
                                           baselines)
save schema change beyond v14
M5 exit report                             (milestone-5-result.md
                                           — only when the
                                           reviewer decides M5
                                           is done)
```

## 8. What does NOT change in any checkpoint refresh

A checkpoint refresh is documentation + integration tests
only. It is NOT a behaviour change. The following are **never**
modified by a future M5.x checkpoint refresh PR:

```text
no save schema bump
no new runner CLI flag
no new RunnerOptions field
no new artefact
no new state field
no new EventTrigger / EventDefinition / EventInstance /
   EventInstanceActor field
no new allowlist entry (target / op / actor kind)
no new doctest CASE behaviour
   (only narrative-fix CHECK lines)
no behavioural change to M1/M2/M3/M4 systems
no behavioural change to any event module
no event firing on the canonical scenario where there
   wasn't one before
no events.jsonl change
no canonical fixture change
```

If a sub-milestone PR needs to change any of the above, it is
**NOT** a checkpoint refresh — it is a regular sub-milestone
PR and ships its own design note `docs/m5-NN-*.md` alongside
the inline checkpoint refresh.

## 9. Close-out readiness assessment

This section answers "is M5 ready to close out?" at the M5.9
moment. The verdict here is informational only — the reviewer
decides actual close-out timing.

### 9.1 What M5 has shipped

The event engine pipeline is complete from schema to monthly-
tick wiring:

- **Schema** (M5.1): typed `EventDefinition` + `EventTrigger`
  + canonical fixture; save v14 carries `events` array.
- **Evaluator** (M5.2): pure-read `match_events` with snapshot
  semantics.
- **Actor binding** (M5.3): every match knows which country /
  IG satisfied it.
- **History data layer** (M5.4): `state.event_history` +
  `EventInstance`; save round-trip preserves byte-stably.
- **Firer** (M5.5): `EventMatch → EventInstance` bridge with
  caller-supplied fire date.
- **Effects applicator** (M5.6): reuses M1.5 policy effect
  path via `policy::apply_effects_to_actor` extract; first-
  actor-wins.
- **Composition helper** (M5.7): `event_engine::tick_events`
  one-round wrapper with snapshot evaluation pinned.
- **Monthly wiring** (M5.8): tick_events runs as step 7 of
  every monthly tick; `MonthlyOutcome` carries the result.
- **Observability** (M5.9 this PR): docs + integration tests
  that pin the M5 contract end-to-end.

### 9.2 Still-deferred items (categorised)

**Category A — defer-to-M6+ gameplay-domain.** These would
make the event engine player-facing or fundamentally change
its scope:

```text
events.jsonl fire records / UI surface
event author tooling
selection-policy variants
chained events / choices / RNG-driven outcomes
broader trigger ops / targets / actor kinds
balance pass that makes canonical events fire
```

**Category B — post-M5 follow-up polish.** These are small
extensions that don't change the architecture:

```text
runner CLI flag for events
event_history-driven cooldown / historical-once gating
trigger logical operators (and / or / not)
events.jsonl semantic extension
```

**Category C — not-needed-for-close nice-to-haves.** These
are deferred indefinitely until a specific need surfaces:

```text
save schema migration shim for v13 -> v14
typed EventId on the gameplay path
event categories / tags
per-fire severity / log template
```

### 9.3 Verdict

M5 is **structurally complete** for an event-engine skeleton:
every legitimate `EventDefinition` author can use the schema,
load it through the canonical manifest pathway, have it
evaluate against monthly state, fire when satisfied, and
mutate state in a deterministic and save-round-trippable way.
What M5 does NOT yet do — surface events through `events.jsonl`
or a CSV / UI, gate them by cooldown, or branch on choices —
are clearly enumerated in Category A / B and each is a
candidate for a dedicated future sub-milestone or its own
milestone.

The reviewer's next decision (per the M3 / M4 close-out
pattern) is one of:

1. **M5 close-out PR**: publish `docs/milestone-5-result.md`
   + flip READMEs to "M5 closed" — mirrors M1.17 / M2.22 /
   M3.9 / M4.23.
2. **One more polish PR from Category B**: e.g. an
   `events.jsonl` emission step (M5.x), a `--events-csv`
   flag (M5.x), or a cooldown helper.
3. **Stop M5 and move to M6** (whatever the reviewer picks
   as the next milestone — RFC-090 doesn't formally name it).

M5.9 does NOT pick. It documents the surface, pins the
contract end-to-end with integration tests, and waits for
explicit direction.

## 10. Notes for the next M5 (or post-M5) reviewer

When the next M5.x or M6.x lands, the reviewer should expect:

- this checkpoint file refreshed **inline** in the same PR
  (per `feedback_checkpoint_drift`) **if** the PR touches a
  surface this checkpoint describes (schema / evaluator /
  firer / applicator / wiring / artefact list);
- a new `docs/m5-NN-*.md` design note (per
  `feedback_readme_rule`);
- the 3 READMEs (root, `docs/`, `rfc/`) updated to reflect
  whatever new behaviour ships;
- this file's §1 ledger grown by one row, §2 dataflow
  extended if new code paths land, §3-§4 schema/allowlist
  tables only changed if the schema legitimately grew (and
  bumped the save format), §5 artefact list grown if a new
  artefact ships, §6 invariants list refreshed (some entries
  flip from "true" to "no longer true"), §7 deferred list
  shortened by whatever shipped, §8 unchanged, §9 verdict
  re-assessed.

When the reviewer decides to close out M5, the close-out PR
follows the M3.9 / M4.23 pattern:

- publish `docs/milestone-5-result.md` (the canonical M5
  record going forward — read that instead of this checkpoint);
- annotate THIS file with a top-of-file "historical
  checkpoint" note pointing at the exit report;
- flip 3 READMEs to "M5 closed";
- no code change in the close-out PR itself.

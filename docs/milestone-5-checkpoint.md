# Milestone 5 Checkpoint

Status: **in progress (at M5.1)**

Companion notes for
`feature/rfc090-m5-01-event-definition-schema-foundation`.

M5.1 is the **opening** sub-milestone of M5 — RFC-090 §M5
"event engine". This checkpoint exists from PR #1 of M5
onward per the `feedback_checkpoint_drift` rule: every M5
sub-milestone refreshes this file **inline** so the next
sub-milestone has one canonical snapshot to read for "what
does the M5 contract look like right now?" without piecing
together per-sub-milestone notes.

A separate `docs/milestone-5-result.md` will NOT be written
until the reviewer decides M5 is done. The close-out PR lands
in its own deliberate sub-milestone (pattern: M1.17 / M2.22 /
M3.9 / M4.23).

Each refresh is a **checkpoint, not an exit report**. No new
system, formula, artefact, save-schema bump, runner CLI flag,
gameplay branch, or new event behaviour ships in a checkpoint
refresh — only the snapshot text + (when needed) the
section-9-style close-out readiness assessment changes.

The companion exit report (`docs/milestone-5-result.md`) is
deliberately not written yet. The close-out lands when the
reviewer decides M5 is done, not at any checkpoint.

This file mirrors `docs/milestone-3-checkpoint.md`'s and
`docs/milestone-4-checkpoint.md`'s role: a single-page
snapshot of M5 state.

## 1. M5 sub-milestones shipped

```text
M5.1   EventDefinition trigger/effect schema foundation
       (save v12 -> v13; loader + validator + store only;
        no firing, no evaluator, no effects application;
        canonical fixture data/events/1930_core_events.json
        wired into both 1930 manifests)
```

## 2. Current M5 dataflow

```text
data/events/<file>.json
  -> scenario_loader::parse_event_file
       (per-file event-array parser;
        target/op allowlist; finite value;
        non-empty triggers; effects validation
        mirrors data_loader::parse_policy)
  -> scenario_loader::load_into_state
       (cross-file id_code uniqueness;
        appends to state.events)

state.events
  -> save_system::serialize          (root-level "events" array)
  -> save_system::deserialize        (allowlist re-checked at load)
  -> diagnostics::compare_states     (events.size + per-field walk)

[NO READERS OUTSIDE SAVE / LOAD / DIAGNOSTICS YET]
state.events is observation-only; no system mutates it, no
system reads its triggers/effects to drive simulation, no
runner CLI flag surfaces it. M5.2+ will add the evaluator.
```

## 3. Current M5 schema (frozen at M5.1)

```cpp
struct EventTrigger {
    std::string target;     // M5.1 allowlist (see §4)
    std::string op;         // M5.1 allowlist (see §4)
    double      value = 0.0;
};

struct EventDefinition {
    std::string               id_code;       // stable identity
    std::string               name;
    std::string               description;
    std::vector<EventTrigger> triggers;      // non-empty at load
    std::vector<PolicyEffect> effects;       // may be empty
};
```

`PolicyEffect` is reused verbatim (M1.4 type). The intent is
that a future M5.x evaluator dispatches event effects through
`policy::apply_policy_effects(state, actor, synthesised_policy)`
without inventing a parallel effect path.

## 4. Current M5 allowlists (frozen at M5.1)

Trigger `target`:

```text
country.stability
country.legitimacy
country.government_authority.bureaucratic_compliance
interest_group.radicalism
interest_group.loyalty
```

Trigger `op`:

```text
lt   (strict less than)
lte  (less than or equal)
gt   (strict greater than)
gte  (greater than or equal)
```

Trigger `value`: required finite double (rejected on
NaN / ±∞ via `std::isfinite`).

Effects: target/op are required non-empty strings; value is a
required finite double. **No allowlist at load** — mirrors
`data_loader::parse_policy`'s effect validation; the target/op
allowlist lives in `policy::apply_policy_effects` (M1.5) and a
future M5.x evaluator inherits it for free.

## 5. Current artefacts (unchanged from M4)

End-of-run produces ten files; M5.1 adds no artefact.

```text
save.json                                  (M0.8,  required)  - now v13
events.jsonl                               (M0.6,  required)
interest_groups.csv                        (M3.5,  unconditional)
interest_group_country_feedback.csv        (M3.6,  unconditional)
interest_group_authority_pressure.csv      (M3.6,  unconditional)
provinces.svg                              (M4.2,  unconditional, 9th)
map.html                                   (M4.5,  unconditional, 10th)
summary.csv                                (M0.10, opt-in)
countries.csv                              (M1.14, opt-in)
factions.csv                               (M1.16, opt-in)
```

`events.jsonl` semantics remain the M0.6 lifecycle log
(begin/end + month rollover + sanity). It does NOT carry event
fires in M5.1 — no event fires in M5.1 by construction.

## 6. Current invariants

These properties hold at the M5.1 checkpoint:

```text
save format is v13
v12 saves are rejected loudly (supports 13)
state.events is loaded + validated + stored only
no system reads state.events to drive simulation
no event fires in M5.1
events.jsonl semantics unchanged from M0.6
runner CLI has no event-related flag
artefact set is still 10
no new PlayerCommandKind
no new RNG draws (M5.1 is RNG-free)
no balance pass — canonical events do not fire on the
  canonical scenario (GER stability 0.55 > 0.30 threshold;
  canonical interest-group radicalism 0.10 < 0.75 threshold)
PolicyEffect shape is unchanged
EventId still defined in core/ids.hpp (reserved for future use)
M1/M2/M3/M4 systems are byte-identical with M4 close-out
```

## 7. Deferred items (will be addressed by future M5.x)

Pulled forward from the M5.1 design note §10 for reader
convenience. These are the candidate scope for M5.2+:

```text
trigger evaluator                  (which events satisfy now?)
event firing                       (the dispatch)
effects application                (call into policy::apply_policy_effects)
monthly integration                (where in the M1.10 pipeline)
events.jsonl fire records          (per-fire log entries)
runner CLI flag                    (e.g. --events-csv)
cooldown / weight / exclusivity
chained events / choices / RNG outcome branches
historical-once gating             (fire-at-most-once semantics)
broader trigger ops                (eq / ne / between / in)
broader trigger targets            (factions, budget categories,
                                    active_policies, current_date)
trigger logical operators          (all-of / any-of / not)
per-effect actor / duration
event categories / tags
event ordering rules
save schema migration shim         (M5.1 chose loud rejection)
UI surface                         (events absent from map.html)
balance pass                       (M5.1 fixtures chosen to not fire)
```

## 8. What does NOT change in any checkpoint refresh

A checkpoint refresh is documentation only. It is not a
behaviour change. The following are **never** modified by a
checkpoint refresh PR:

```text
no save schema bump
no new runner CLI flag
no new artefact
no new state field
no new EventTrigger / EventDefinition field
no new allowlist entry
no new doctest CASE behaviour (only narrative-fix CHECK lines)
no behavioural change to M1/M2/M3/M4 systems
no event firing
```

If a sub-milestone PR needs to change any of the above, it is
**not** a checkpoint refresh — it is a regular sub-milestone PR
and ships its own design note `docs/m5-NN-*.md` alongside the
inline checkpoint refresh.

## 9. Notes for the next M5 sub-milestone reviewer

When the next M5.x lands, the reviewer should expect:

- this checkpoint file refreshed **inline** in the same PR
  (per `feedback_checkpoint_drift`),
- a new `docs/m5-NN-*.md` design note,
- the 3 READMEs (root, `docs/`, `rfc/`) updated to reflect
  whatever new behaviour ships,
- this file's §1 ledger grown by one row, §2 dataflow extended
  if new code paths land, §3-§4 schema/allowlist tables only
  changed if the schema legitimately grew (and bumped the save
  format), §5 artefact list grown if a new artefact ships, §6
  invariants list refreshed (some entries flip from "true" to
  "no longer true"), §7 deferred list shortened by whatever
  shipped, §8 unchanged.

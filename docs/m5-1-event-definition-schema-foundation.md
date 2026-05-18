# M5.1 - EventDefinition trigger/effect schema foundation

Companion notes for
`feature/rfc090-m5-01-event-definition-schema-foundation`.

**This is RFC-090 M5** (event engine), not a post-M4 governance
follow-up. M5.1 ships only the **schema foundation** required by
a future event evaluator / firer / effect applicator — no
trigger evaluator, no firing, no effects application, no monthly
integration, no runner CLI flag, no new artefact, no
`events.jsonl` semantic change, no new `PlayerCommandKind`. The
next M5 sub-milestones decide how the schema is wired into the
simulation; M5.1 decides what the schema looks like.

The data layer is intentionally a strict subset of the
RFC-070 §3 event-record shape: `id_code` + `name` + `description`
+ `triggers[]` + `effects[]` only. Cool-down, weight, exclusivity,
chained events, choices, RNG-driven outcomes, log emission,
historical-once gating — all explicitly deferred.

## 1. Scope

What ships:

```text
core::EventTrigger { target, op, value }     (new POD)
core::EventDefinition                         (upgraded from M0 stub)
    id_code / name / description / triggers / effects
scenario_loader optional events[]             (paths to per-file event files)
scenario_loader::parse_event_file             (new per-file parser)
SaveSystem v12 -> v13                         (events required at save layer)
data/events/1930_core_events.json             (2 canonical events)
canonical scenarios get an events[]           (1930_minimal + 1930_with_start_policies)
diagnostics::compare_states walks events
~25 new doctest cases
    (8 save_system + 13 scenario_loader + 4 diagnostics
     + 2 runner regression)
```

The M0 `EventDefinition { EventId id; std::string name; }` stub
never had a single reader and is now gone — the typed
`EventDefinition` replaces it with the shape every future event
system actually needs. `EventId` stays defined in
`core/ids.hpp` for now (it is still a sensible future handle),
but `EventDefinition` does not carry one; the string `id_code`
is the stable identity, mirroring `ProvinceNode::id_code` and
`InterestGroupState::id_code` from M3/M4.

`PolicyEffect` is reused for `EventDefinition::effects` so the
event evaluator can dispatch through the existing
`policy::apply_policy_effects` machinery in a future
sub-milestone without inventing a parallel effect type. This is
a deliberate **type reuse**, not a TODO.

## 2. Data model

```cpp
struct EventTrigger {
    std::string target;
    std::string op;
    double      value = 0.0;
};

struct EventDefinition {
    std::string               id_code;
    std::string               name;
    std::string               description;
    std::vector<EventTrigger> triggers;   // non-empty at load
    std::vector<PolicyEffect> effects;    // may be empty
};
```

Five reasons the type is this small in M5.1:

- A future evaluator only needs an identity, a human label, a
  condition list, and an effect list. Anything more is gameplay
  scope that hasn't been decided yet.
- `triggers` is a *list*, not a single trigger — the future
  evaluator can pick `all` / `any` semantics; M5.1 stores them
  and stops.
- `effects` may be **empty**: this models the "warning-only" or
  "narration-only" event class that fires a log line without
  mutating state. The canonical fixture exercises only
  effect-bearing events, but the loader and tests pin the
  empty-effects path so it's available to authors.
- `PolicyEffect` reuse means there is exactly one effect-record
  shape in the codebase; M5 and M1 can share the apply path.
- Deliberately deferred fields — *all listed for the reader so
  it's obvious what M5.1 chose not to ship*: cooldown, weight,
  exclusivity group, chained / parent / child event id,
  player-choice options, RNG-driven outcome branches,
  one-shot/historical-once flags, log severity / log template,
  trigger logical operator (and/or), trigger negation,
  per-effect actor selector, per-effect duration.

## 3. Trigger allowlist (load + save)

Trigger `target` is allowlisted at load time and re-checked at
save time so a malformed save can never reach the evaluator:

```text
country.stability
country.legitimacy
country.government_authority.bureaucratic_compliance
interest_group.radicalism
interest_group.loyalty
```

Trigger `op` allowlist:

```text
lt   (strict less than)
lte  (less than or equal)
gt   (strict greater than)
gte  (greater than or equal)
```

`eq` / `ne` are deliberately omitted — floating-point equality
against an authored `value` is hostile to authors (any drift
silently changes whether the trigger fires) and the four
relational ops cover every condition expressible as a threshold.
A future M5.x sub-milestone may add interval / enum / membership
ops; M5.1 ships only the four.

`value` is required and must be a finite double (rejected on
NaN / ±∞ via `std::isfinite`, matching the M1.4 / M1.5 policy
effect rule).

## 4. Effect validation (mirrors policy effects)

Each `effects[]` entry is validated at load time the same way
M1.4 `data_loader::parse_policy` validates a policy effect:

```text
target  required, non-empty string  (NO allowlist at load)
op      required, non-empty string  (NO allowlist at load)
value   required, finite double
```

The target / op allowlists for effects live inside
`policy::apply_policy_effects` (the existing M1.5 dispatcher).
Events therefore inherit the policy-effect target grammar
(`country.<field>`, `country.budget.<cat>`, `faction:<type>.<field>`)
for free — they just don't *apply* anything in M5.1.

## 5. Scenario loader

The manifest gains an **optional** root-level `events` array of
file paths, mirroring how `provinces` is spelled at M4.1:

```json
"events": [
  "events/1930_core_events.json"
]
```

Each referenced file is a JSON object with an `events` array of
inline records:

```json
{
  "events": [
    {
      "id":          "low_stability_unrest",
      "name":        "Low Stability Unrest",
      "description": "Low stability creates unrest pressure.",
      "triggers": [
        { "target": "country.stability", "op": "lt", "value": 0.30 }
      ],
      "effects": [
        { "target": "country.stability", "op": "add", "value": -0.02 }
      ]
    }
  ]
}
```

The author-facing key is `id` to match the
`provinces/*.json` author convention from M4.1; the in-memory
field is `id_code` (matching ProvinceNode / InterestGroupState).

Cross-file uniqueness of `id_code` is enforced at the
scenario-loader level (the save layer enforces it on reload
too). Manifests authored before M5.1 keep working: missing
`events` parses as an empty vector.

`ScenarioLoadOutcome` gains `events_loaded` so tests and the
runner can pin how many entries actually landed.

**Loading is observation-only**: no event fires during load, no
country / interest_group / authority field is mutated. M5.1
tests pin this no-mutate property explicitly.

## 6. Save schema bump v12 -> v13

The M0 `events` block was always present in the save (the stub
serialised as an empty array). A v12 save's `events` entries
(if any) carry only the M0 stub fields `id` (integer) + `name`;
silently defaulting `description` / `triggers` / `effects` on
reload would fabricate events with empty trigger lists (an
invalid invariant at the schema level). We bump strictly under
the M0.8 rule.

At the save-file level the `events` array is **REQUIRED** (empty
array allowed) and every entry is validated:

```text
id_code      non-empty string
name         non-empty string
description  required string (may be empty)
triggers     required, non-empty array
  .target    string, in the M5.1 target allowlist
  .op        string, in the M5.1 op allowlist
  .value     finite double
effects      required array (may be empty)
  .target    non-empty string
  .op        non-empty string
  .value     finite double
duplicate id_code (across the whole array) rejected
```

The save-layer allowlist mirrors the loader's allowlist
verbatim — there is one source of truth in code (two `static
const std::vector<std::string>` pairs, intentionally
co-located).

v12 saves are rejected loudly with `supports 13` in the message,
matching the M0.8 strict-equality contract.

## 7. Canonical fixture

`data/events/1930_core_events.json` ships two events:

| id                                | trigger                                                  | effect                                              |
| --------------------------------- | -------------------------------------------------------- | --------------------------------------------------- |
| `low_stability_unrest`            | `country.stability` `lt` `0.30`                          | `country.stability` `add` `-0.02`                   |
| `radical_interest_group_warning`  | `interest_group.radicalism` `gt` `0.75`                  | `country.legitimacy` `add` `-0.01`                  |

Two is the minimum that exercises both trigger-target families
(country-scoped + interest-group-scoped) and both effect-target
families. Neither event fires during M5.1's canonical run
(canonical GER stability is 0.55 > 0.30; canonical
interest-group radicalism is 0.10 < 0.75) — the values are
chosen so a future evaluator can be tested *without* having to
edit the canonical fixture.

The filename is event-centric (`1930_core_events`), not
mechanic-centric — future M5.x sub-milestones may add
`1930_economic_events.json` / `1930_diplomatic_events.json`
alongside without renaming the core file.

Both canonical manifests (`1930_minimal.json` and
`1930_with_start_policies.json`) reference the file. The
`scenario_loader_test` canonical-load case pins the two-row
shape end-to-end so a future manifest edit can't silently
drift the fixtures.

## 8. Diagnostics

`diagnostics::compare_states` now walks `state.events` after
`state.provinces` with the field paths:

```text
events.size()
events[N].id_code
events[N].name
events[N].description
events[N].triggers.size()
events[N].triggers[M].target
events[N].triggers[M].op
events[N].triggers[M].value
events[N].effects.size()
events[N].effects[M].target
events[N].effects[M].op
events[N].effects[M].value
```

A size mismatch at the `triggers` or `effects` level skips the
per-element walk (matches the M3.1 `interest_groups[N]` pattern:
size first, then per-element). Tolerance for `value` reuses the
existing `CompareOptions::double_tolerance` (default `1e-9`).

## 9. Tests added (~25)

- **Save (8)**: serialize emits empty `events: []`; round-trip
  preserves canonical events; v13 missing-`events` rejected;
  wrong-type rejected; trigger target not allowlisted rejected;
  trigger op not allowlisted rejected; empty triggers rejected;
  duplicate id_code rejected; plus the new v12 rejection mirror
  of the existing v11 rejection.
- **Scenario loader (13)**: parse_manifest missing key allowed;
  manifest events wrong type / non-string element rejected;
  canonical 2-event load; event file missing `events` array;
  event entry missing `id`; empty `triggers`; trigger target /
  op not allowlisted; trigger value wrong-type; effect missing
  `op`; empty `effects` allowed (warning-only events); duplicate
  event id across files; no-mutate regression on countries +
  interest_groups.
- **Diagnostics (4)**: identical events no mismatch; events.size
  mismatch reported; per-field mismatches reported on the
  expected paths; `triggers.size` mismatch skips per-trigger walk.
- **Runner regression (2)**: canonical scenario still produces
  10 artefacts and save carries the 2 canonical events; canonical
  scenario does NOT pre-fire events during load (GER stability
  stays at 0.55).

Existing test fixtures with hand-written `save_version: 12` are
all migrated to `13` and gain the new `events` array; this is a
mechanical migration, not a behavioural one.

## 10. What M5.1 explicitly does NOT do

```text
no trigger evaluator
no event firing
no effects application
no monthly integration
no events.jsonl semantic change
no runner CLI flag
no new artefact (still 10)
no new PlayerCommandKind
no AI / event-author tooling
no cooldown / weight / exclusivity
no chained events / choices / RNG-driven outcome branches
no historical-once gating
no log emission on fire
no broader trigger ops (eq / ne / between / in)
no broader trigger targets (factions, budget categories,
   active_policies, current_date, RNG state)
no per-effect actor / duration
no event categories / tags
no event ordering rules
no save schema migration shim
no UI surface (events absent from map.html / provinces.svg /
   any CSV)
no balance pass — the two canonical events' values are chosen
  to not fire on the canonical scenario by construction
no changes to M1/M2/M3/M4 systems
no changes to PolicyEffect shape
no removal of EventId (still defined in core/ids.hpp)
```

M5 remains in progress. M5.1 ships the schema foundation; the
next sub-milestone is unspec'd and waits for the reviewer.

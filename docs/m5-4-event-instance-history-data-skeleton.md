# M5.4 - EventInstance / event history data skeleton

Companion notes for
`feature/rfc090-m5-04-event-instance-history-data-skeleton`.

**This is RFC-090 M5** (event engine). M5.4 ships the **data
layer** for fired-event records: a typed `EventInstance` plus
storage on `GameState`, a save-format bump (v13 → v14) so
hand-built / future-firer-produced history round-trips
byte-stably, and a `diagnostics::compare_states` walk over the
new vector. **No system creates EventInstance records yet** —
no auto-fire, no effects application, no runner / monthly
integration, no `events.jsonl` semantic change. The future
M5.x firer will be the first writer; M5.4 stands the data
layer up so that firer doesn't also have to ship a save bump
in the same PR.

The boundary follows the M5 pacing so far:
M5.1 = EventDefinition schema, M5.2 = trigger evaluator,
M5.3 = actor binding on the evaluator's return shape,
M5.4 = data layer for fired-event records (this PR),
M5.5+ = the actual firer + effects-applicator + runner
integration. Each PR is one decoupled surface.

## 1. Scope

What ships:

```text
core::EventInstanceActor                   (new POD struct)
core::EventInstance                        (new struct)
GameState::event_history                   (new append-only vector field)
SaveSystem v13 -> v14                      (event_history required at save layer)
diagnostics::compare_states                (walks event_history)
~18 new doctest cases
   (12 save_system + 4 diagnostics + 1 runner regression
    + 1 retuned m4 contract)
```

What does NOT change:

```text
no system creates EventInstance records (no auto-fire)
no effects application
no runner / monthly integration
no new artefact (still 10)
no events.jsonl semantic change
no new RunnerOptions field / new CLI flag
no new PlayerCommandKind
no change to scenario_loader (history is runtime accumulation,
   not scenario input — manifests do not author it)
no change to event_evaluator (M5.3 evaluator stays read-only;
   M5.4 doesn't auto-wire it to a firer)
no change to canonical fixtures (no auto-fire = no entries
   produced)
no change to M1/M2/M3/M4 systems
no change to docs/milestone-5-checkpoint.md (it still doesn't
   exist — checkpoint deferred until M5 has shipped multiple
   cross-cutting surfaces; M5.4 is one more single-surface PR
   on the event-engine data layer)
```

## 2. Data model

```cpp
struct EventInstanceActor {
    std::string kind;             // "country" | "interest_group"
    std::string id_code;
    std::string country_id_code;  // owning country for IG (self for kind=country)
    std::size_t index = 0;        // pos in state.countries / state.interest_groups
};

struct EventInstance {
    std::string                     event_id_code;
    GameDate                        fired_on;
    std::vector<EventInstanceActor> actors;
};

// In GameState:
std::vector<EventInstance> event_history;
```

Five reasons the type is this small in M5.4:

- **Strings, not numeric handles, for stable serialisation.**
  `EventInstanceActor` carries strings (`kind`, `id_code`,
  `country_id_code`) plus a transient `index` runtime hint.
  Stable across save/reload regardless of `CountryId` values
  (which are session-local indices into `state.countries` and
  can shift if scenarios add/remove countries). M5.3's runtime
  `event_evaluator::TriggerActor` uses `CountryId`; M5.4's
  saved `EventInstanceActor` translates to id_code strings at
  save time and back at load time (or, in M5.4-era tests,
  hand-built directly with id_code strings).
- **`country_id_code` carried on every actor**, even for
  `kind="country"` (where it equals `id_code`). Makes the
  future "apply this IG-trigger's effect to the IG's owning
  country" code path a single field read, not a country
  lookup; for `Country` kind it's a tiny redundancy in the
  save file, traded for callsite simplicity.
- **`index` is a transient hint, not a stable identity.**
  Documented so a future reader doesn't accidentally rely on
  it across reloads. The authoritative identifier is
  `id_code`.
- **`event_id_code` is a string reference, not a cross-check.**
  The save layer does not verify it resolves to a
  `state.events` entry. This is **deliberate**: a legitimate
  workflow is loading a save authored under scenario A into
  scenario B (e.g. for testing, scenario diffing, or
  retro-fixing balance) — cross-checking would break that
  workflow. A future M5.x consumer that needs the definition
  (e.g. to look up effects) handles the resolve-or-skip
  decision at its own callsite.
- **No EventInstance constructor from EventMatch.** M5.4 ships
  only data. The conversion from `event_evaluator::EventMatch`
  → `core::EventInstance` is a one-liner the future M5.x firer
  will own (alongside the `fired_on` date capture, which the
  evaluator doesn't know about). Adding the converter in M5.4
  without a firer to call it would be an unused abstraction.

Deliberately deferred fields — listed so it's obvious what
M5.4 chose not to ship: cause / source (player command vs
auto-tick), severity, tags, suppression-reason, dedup hash,
referenced policy id, applied-effect log, rng draw counter at
fire time, "would have fired but skipped" entries, parent /
child event chains, choice-made record for branching events.

## 3. Save schema bump v13 -> v14

The v13 save had no concept of fired events (M5.1 schema +
M5.2/M5.3 evaluator were all gate; no firing). Silently
defaulting to "no history" on reload would drop any
hand-authored M5.4 history fixture or future M5.x firer
output. We bump strictly under the M0.8 rule.

At the save-file level the `event_history` array is
**REQUIRED** (empty array allowed) and every entry is
validated:

```text
event_id_code            non-empty string
fired_on                 string parseable by GameDate::parse
                         (YYYY-MM-DD, is_valid())
actors                   required array (may be empty)
  .kind                  string from allowlist
                         {"country", "interest_group"}
  .id_code               non-empty string
  .country_id_code       non-empty string
  .index                 non-negative integer (parsed via require_u64)
```

The save-layer kind allowlist (`{"country",
"interest_group"}`) lives in
`src/leviathan/systems/save_system.cpp` as a small `static
const std::vector<std::string>`. If M5.x extends actor scopes
(e.g. `"faction"` when faction-scoped triggers ship), both
that allowlist AND the new kind's evaluator path need to grow
together.

v13 saves are rejected loudly with `supports 14` in the
message, matching the M0.8 strict-equality contract.

## 4. Diagnostics

`diagnostics::compare_states` now walks `state.event_history`
after `state.events` with the field paths:

```text
event_history.size()
event_history[N].event_id_code
event_history[N].fired_on
event_history[N].actors.size()
event_history[N].actors[M].kind
event_history[N].actors[M].id_code
event_history[N].actors[M].country_id_code
event_history[N].actors[M].index
```

A size mismatch at the `event_history` or `actors` level
skips the per-element walk (matches the M3.1 / M4.1 / M5.1
pattern: size first, then per-element).

## 5. Tests added (~18)

- **Save serialize (1)**: empty `state.event_history` emits
  `"event_history": []`.
- **Save round-trip (2)**: canonical single-entry round-trip;
  cross-scope round-trip (country actor + interest_group
  actor in one entry).
- **Save load rejections (9)**: v13 rejected loudly with
  `supports 14`; v14 missing `event_history` rejected; wrong
  type rejected; entry missing `event_id_code` rejected; bad
  `fired_on` date rejected; actor kind not in allowlist
  rejected; actor missing `country_id_code` rejected; actors
  missing/wrong-type rejected; actor empty `id_code` rejected.
- **Cross-check contract (1)**: `event_id_code` referencing a
  non-existent event still loads (no cross-check — pinned so
  a future PR can't silently add one).
- **Diagnostics (4)**: identical history no mismatch; size
  mismatch reported; per-field mismatches reported on
  expected paths (event_id_code / fired_on / actors[0].kind /
  .id_code / .country_id_code / .index); `actors.size`
  mismatch skips per-actor walk.
- **Runner regression (1)**: canonical scenario at M5.4 still
  produces 10 artefacts, save carries v14 + empty
  event_history (no auto-fire), `events.jsonl` semantics
  unchanged.
- **m4 dom contract (1 retuned)**: existing M4 contract test
  checks `"save_version": 14` now (was 13).

Existing v13 fixtures auto-migrate via two bulk replaces:
`"save_version": 13` → `"save_version": 14` and `supports 13`
→ `supports 14`. The hand-built positive-path test that
deserializes a full save (the INT_MAX country test) gains
`"event_history": []` to satisfy the new required field.

Total: 983 doctest cases / 62064 assertions
(was 965 / 62006 at M5.3 close).

## 6. What's deferred (the M5.5+ horizon)

```text
event firer                       (the writer that produces
                                   EventInstance records)
effects APPLICATION               (call policy::apply_policy_effects)
EventMatch -> EventInstance       (the conversion utility,
   conversion utility              owned by the firer)
runner / monthly integration      (auto-fire on month boundary)
events.jsonl fire records         (per-fire log entries)
runner CLI flag                   (e.g. --events-csv)
cooldown / weight / exclusivity   (event firing policy)
chained events / choices /
   RNG-driven outcome branches
historical-once gating            (fire-at-most-once semantics
                                   using event_history)
broader trigger ops               (eq / ne / between / in)
broader trigger targets           (factions, budget categories,
                                   active_policies, current_date,
                                   RNG)
broader actor kinds               (faction; would need to grow
                                   the kind allowlist)
selection-policy variants         (random / weighted / all-of —
                                   their own dedicated PR per
                                   PR #92 review)
save schema change beyond v14
UI surface                        (events still absent from
                                   map.html / provinces.svg /
                                   any CSV)
balance pass                      (canonical events still chosen
                                   to not fire on canonical
                                   scenario)
docs/milestone-5-checkpoint.md    (still not written; deferred
                                   until M5 ships multiple
                                   cross-cutting surfaces)
```

## 7. What M5.4 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no system creates EventInstance records (no auto-fire)
no firing of canonical events at scenario load
no effects application
no log entry on fire / append to state.logs
no events.jsonl change
no runner / monthly integration / auto-fire cadence
no new artefact (still 10)
no new RunnerOptions field / new CLI flag
no new PlayerCommandKind
no scenario_loader change
   (history is runtime accumulation, not scenario input)
no event_evaluator change
   (M5.3 evaluator stays read-only; M5.4 doesn't auto-wire
    it to a firer)
no EventMatch -> EventInstance converter
   (one-liner the future firer owns alongside fired_on
    capture)
no cross-check that event_id_code resolves in state.events
   (preserves "load save into different scenario" workflow)
no broader trigger ops / targets / logical operators
no broader actor kinds (faction etc.)
no selection-policy variants
no UI surface
no balance pass
no changes to M1/M2/M3/M4 systems
no docs/milestone-5-checkpoint.md
   (still deferred — premature framing at M5.4)
```

M5 remains in progress. M5.4 ships the event-history data
layer; the next sub-milestone is unspec'd and waits for the
reviewer.

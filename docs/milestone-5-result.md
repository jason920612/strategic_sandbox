# Milestone 5 Result

**Status: closed (as an implementation milestone).**

> **Governance note.** This document is the implementation
> close-out for the shipped M5 event-engine skeleton
> milestone. It is not a claim that the original RFC-090 §M5
> *event-content / `EventOption` / followup-chain* acceptance
> scope is fully complete. Original RFC-090 §M5 lists weights
> + `EventOption` + event options / choices + 10 event
> templates + a 10-year event stress test + event-chain
> followups; the closed M5 ships an event-engine *skeleton*
> with 2 canonical events deliberately tuned not to fire on
> the canonical scenario, no weighted selection, no event
> options, no followup chains, and no per-fire `events.jsonl`
> emission. Deferred original RFC-090 §M5 items are tracked
> in [`rfc-090-010-compliance-audit.md`](rfc-090-010-compliance-audit.md)
> §6.2. Companion to issue #105.

M5 set out to give Project Leviathan its first event-engine
surface: a typed `EventDefinition` schema with trigger / effect
authoring, a read-only evaluator, an actor-binding layer, a
fired-event history data layer with save round-trip, a firer
that turns evaluator matches into history records, an effects
applicator that reuses the M1.5 policy machinery, a single
composition helper, runner-level wiring into the monthly tick,
an integration-tested checkpoint — all shipped as an additive
sequence of decoupled-surface sub-milestones, with the
canonical 1930 fixtures deliberately tuned so the engine adds
zero behavioural drift to existing M1–M4 outputs. Ten
sub-milestones delivered that.

## 1. What M5 shipped

| Sub-milestone | Title | Highlights |
|---------------|-------|------------|
| **M5.1** | EventDefinition trigger/effect schema foundation | Typed `core::EventDefinition { id_code, name, description, triggers, effects }` (replacing the M0 stub) + new `core::EventTrigger { target, op, value }`. `EventDefinition::effects` reuses `core::PolicyEffect` (M1.4) for free. Trigger op allowlist `lt`/`lte`/`gt`/`gte`; trigger target allowlist 5 entries (country.stability, country.legitimacy, country.government_authority.bureaucratic_compliance, interest_group.radicalism, interest_group.loyalty); finite value enforced. New `scenario_loader::parse_event_file` per-file parser + optional manifest `events[]` (mirrors M4.1 `provinces[]`). **Save format v12 → v13** with `events` required at the save layer. `diagnostics::compare_states` walks `state.events`. Canonical fixture `data/events/1930_core_events.json` with 2 events wired into both 1930 manifests. Loader + validator + store only; no firing / evaluator / effects application. |
| **M5.2** | Trigger evaluator skeleton | New read-only `leviathan::systems::event_evaluator` module: `trigger_matches(state, EventTrigger) → bool`, `evaluate(state, EventDefinition) → bool`, `match_events(state) → vector<TriggerMatch>`. Per-op numeric compare on the M5.1 allowlist; per-target dispatch with **ANY-entity-satisfies** aggregation (country.* against `state.countries`; interest_group.* against `state.interest_groups`); **AND across `def.triggers`**; vacuous-true for empty triggers; defensive false on unknown target/op/non-finite. All pure reads — no GameState mutation. |
| **M5.3** | EventMatch actor-binding skeleton | Extended the evaluator return shape to record **which entity** satisfied each trigger. New `enum class TriggerActorKind { Country, InterestGroup }`, `struct TriggerActor { kind, id_code, country, index }`, `struct TriggerEvaluation { trigger_index, actor }`. Renamed `TriggerMatch` → `EventMatch` with new `triggers` vector (`event_index`/`event_id_code` field names preserved so M5.2-shaped read sites kept compiling). New `trigger_actor`/`evaluate_match` free functions. **Actor selection policy "first in vector order"** — deterministic via canonical scenario-load order; per-effect actor scoping / weighted / random deferred to a future selection-policy sub-milestone. |
| **M5.4** | EventInstance / event_history data skeleton | New `core::EventInstanceActor { kind, id_code, country_id_code, index }` (strings, not numeric handles — stable across save round-trip regardless of session-local `CountryId` values; `country_id_code` is owning country for IG kind) + new `core::EventInstance { event_id_code, fired_on, actors[] }` + new `GameState::event_history` append-only vector. **Save format v13 → v14** with `event_history` required at the save layer; per-entry validation (non-empty `event_id_code`, parseable `fired_on`, actor `kind` in allowlist `{"country", "interest_group"}`, non-empty `id_code` + `country_id_code`, non-negative `index`). **Deliberately no cross-check** that `event_id_code` resolves to a `state.events` entry — preserves "load save into a different scenario" workflow (pinned by a test). `diagnostics::compare_states` walks `event_history`. |
| **M5.5** | Event firer skeleton | New `leviathan::systems::event_firer` module with `record_match(state, EventMatch, fired_on)` + `record_matches(state, vector<EventMatch>, fired_on) → FireOutcome` that converts M5.3 `EventMatch` (with per-trigger actor binding) into M5.4 `EventInstance` and appends to `state.event_history`. **`fired_on` is caller-supplied** (NOT read from `state.current_date` — firing cadence is a runner-policy decision; firer itself stays date-neutral). **No `Result<T,E>` — always succeeds**; broken actors leave `country_id_code` empty and the save layer rejects on next round-trip (loud-on-next-save beats silent corruption; pinned by a test). **No dedup** (caller policy). |
| **M5.6** | Event effects applicator skeleton | New `leviathan::systems::event_effects` module with `apply_event_effects(state, instance, definition)` that applies `definition.effects` to the country resolved from `instance.actors.front().country_id_code`. **Reuse-via-extract refactor**: pulled the actor-validate + pre-flight + apply core out of `policy::apply_policy_effects` into a new public `policy::apply_effects_to_actor(state, actor, vector<PolicyEffect>)` helper. `apply_policy_effects` (existing, M1.5) delegates to the helper then appends `ActivePolicy`. `apply_event_effects` calls the helper directly — events inherit the M1.5 target/op grammar + pre-flight atomicity WITHOUT inheriting the M1.15 `ActivePolicy` lifecycle (events aren't policies). **First-actor-wins** selection policy; multi-actor / weighted / random / per-effect-actor-scope deferred. Pinned by two M1.5 regression tests. |
| **M5.7** | Event runner integration skeleton (standalone helper) | New `leviathan::systems::event_engine` module with `tick_events(state) → Result<TickOutcome>` that composes M5.2 evaluator + M5.5 firer + M5.6 applicator into one "evaluate → record → apply" round. `fired_on = state.current_date` for every recorded instance. **Snapshot evaluation**: evaluator runs ONCE at the top; subsequent apply passes that mutate state do NOT re-trigger evaluation in the same round (pinned by a test where an event drops a value past another event's threshold but the second event does NOT fire in the same tick). No dedup. Failure on apply bubbles up; partial state pinned. **Standalone helper — not auto-wired**: deliberate decoupled-surface pacing; M5.8 owns the wiring. |
| **M5.8** | Monthly event tick wiring | Wired `event_engine::tick_events(state)` into `monthly::tick_all_countries` as **step 7**, the final global step after M3.4 `authority_pressure`. Step 7 is the only position where every per-country (M1.6/M1.7/M1.8) and every state-wide (M3.2/M3.3/M3.4) system has finished writing — events evaluate against month-end values, not pre-month or partially-drifted snapshots (pinned via dry-run-then-build-trigger test). `MonthlyOutcome` gained `event_tick` field mirroring `TickOutcome`. Failure propagates with `event_engine: tick_events failed:` prefix. **Zero canonical determinism rebake needed** because M5.1 canonical events deliberately don't fire on canonical state (GER stability 0.55+ vs `< 0.30` threshold; canonical IG radicalism 0.10 vs `> 0.75` threshold) and hand-built test fixtures don't populate `state.events`. |
| **M5.9** | Event observability checkpoint | Mid-milestone checkpoint sub-milestone (mirrors M3.7 / M4.9 / M4.14 / M4.18 / M4.22). Published `docs/milestone-5-checkpoint.md` single-page status snapshot covering M5.1–M5.8 ledger + dataflow + monthly wiring + schema + allowlists + artefact set + invariants + deferred items + close-out readiness assessment with Category A/B/C breakdown. Added `tests/integration/m5_event_pipeline_test.cpp` with 5 cross-leg seam tests (canonical no-fire / firing event effect lands / 10-artefact set preserved / determinism with events firing / failure path → M2.9 pre-end_tick contract). Zero new gameplay; zero existing tests changed. |
| **M5.10** | M5 exit / close-out | This sub-milestone. New `docs/milestone-5-result.md` (this file). READMEs flipped to "M5 closed". `docs/milestone-5-checkpoint.md` annotated as historical. No code, no formula, no fixture, no test change. **M5 closes here.** |

## 2. Final M5 dataflow

```text
scenario manifest "events": [ "events/<file>.json" ]
   -> scenario_loader::parse_event_file               (M5.1)
        (target/op allowlist; finite value;
         non-empty triggers; effects validation
         mirrors data_loader::parse_policy)
   -> scenario_loader::load_into_state
        (cross-file id_code uniqueness;
         appends to state.events)

state.events
   -> save_system::serialize                     (root "events" array, v14)
   -> save_system::deserialize                   (allowlist re-checked at load)
   -> diagnostics::compare_states                (events.size + per-field walk)

Monthly tick (every state.current_date.month_changed):

   per-country (vector order):
     1. faction::react              (M1.6)
     2. stability::tick             (M1.7)
     3. economy::tick               (M1.8)

   state-wide (after per-country loop):
     4. interest_group::react              (M3.2)
     5. interest_group::country_feedback   (M3.3)
     6. interest_group::authority_pressure (M3.4)
     7. event_engine::tick_events          (M5.8 - the wiring)

Inside event_engine::tick_events (M5.7):

   matches = event_evaluator::match_events(state)     (M5.2)
                -> vector<EventMatch> with M5.3
                   per-trigger actor binding

   for each match m in canonical order:
     a. def = state.events[m.event_index]
     b. event_firer::record_match(state, m,
                                  state.current_date)  (M5.5)
          -> appends EventInstance to event_history    (M5.4)
     c. instance = state.event_history.back()
     d. event_effects::apply_event_effects(state,
                                           instance,
                                           def)        (M5.6)
          -> policy::apply_effects_to_actor (M1.5/M5.6
             extract) mutates the country resolved from
             instance.actors.front().country_id_code

state.event_history
   -> save_system::serialize                     (root "event_history" array)
   -> save_system::deserialize                   (per-entry kind allowlist)
   -> diagnostics::compare_states                (per-entry / per-actor walk)
```

## 3. Final artefact contract

M5 closes with the same ten runner artefacts that M4 closed
with — M5 added zero artefacts:

```text
save.json                                  (M0.8,  required)  v14 since M5.4
events.jsonl                               (M0.6,  required)  M5 did NOT change semantics
summary.csv                                (M0.10, opt-in)
countries.csv                              (M1.14, opt-in)
factions.csv                               (M1.16, opt-in)
interest_groups.csv                        (M3.5,  unconditional)
interest_group_country_feedback.csv        (M3.6,  unconditional)
interest_group_authority_pressure.csv      (M3.6,  unconditional)
provinces.svg                              (M4.2,  unconditional)
map.html                                   (M4.5,  unconditional)
```

`events.jsonl` is the M0.6 lifecycle log. **M5 emits zero
event-fire records into it** — the M5.9 integration tests pin
that canonical event id_codes never appear in `events.jsonl`
even when their definitions are loaded. A future M5.x or M6.x
sub-milestone may add per-fire `LogEntry` emission; M5 closes
without it.

`end_tick` writes the ten files **sequentially, not
transactionally** — same caveat as M4 close. Atomic-end_tick
remains a deferred item across milestones.

## 4. Save schema

```text
save format closes M5 at v14
```

M5 bumped the schema exactly twice:

- **v12 → v13 at M5.1** to make the `events` block a required
  root-level array carrying the typed `EventDefinition` shape
  (id_code, name, description, triggers[], effects[]).
- **v13 → v14 at M5.4** to add the required root-level
  `event_history` array carrying the typed `EventInstance`
  shape (event_id_code, fired_on, actors[]).

M5.2 / M5.3 / M5.5 / M5.6 / M5.7 / M5.8 / M5.9 / M5.10 were all
deliberately schema-neutral. v14 is the new floor; the next
persistent-state addition will bump it under the M0.8
strict-equality + version-history rule.

## 5. Architectural invariants every future milestone must preserve

These are the rules M5 added on top of the M0 / M1 / M2 / M3 /
M4 invariants. They are pinned by either unit tests, the M5.9
cross-leg integration tests, or the absence of an opposing code
path.

### 5.1 Schema invariants

- **`EventDefinition.triggers` is non-empty at load time.**
  Both scenario_loader and save_system reject an empty
  `triggers` array.
- **Trigger ops are strictly the 4-entry allowlist**
  `{lt, lte, gt, gte}`. `eq` / `ne` are deliberately omitted
  (floating-point equality against authored values is hostile
  to authors).
- **Trigger targets are strictly the 5-entry allowlist** (3
  country-scoped, 2 interest-group-scoped).
- **Trigger value is finite.** NaN / ±∞ rejected at load.
- **Effect target/op required non-empty strings + finite value
  at load; no allowlist at load.** Allowlist lives in
  `policy::apply_effects_to_actor` (M1.5 / M5.6) and the event
  applicator inherits it.
- **`EventInstanceActor.kind` is strictly the 2-entry
  allowlist** `{"country", "interest_group"}` at save layer.
- **`EventInstance.event_id_code` is NOT cross-checked against
  `state.events`** on load. Preserves the "reload a save into
  a different scenario manifest" workflow. Pinned by a test.

### 5.2 Evaluator invariants

- **Pure read.** `match_events` / `evaluate` / `trigger_matches`
  / `trigger_actor` / `evaluate_match` never mutate GameState.
- **AND across `def.triggers`.** Every trigger must match for
  the event to match.
- **ANY-entity-satisfies aggregation.** A country-scoped
  trigger matches when at least one country satisfies it;
  empty entity list → false (existential over empty).
- **First-in-vector-order actor selection.** When multiple
  entities satisfy a trigger, the evaluator binds the first
  one; canonical scenario-load order makes this deterministic.
- **Defensive false on unknown target / op / non-finite.** The
  M5.1 loader is the gate; the evaluator does not duplicate
  the allowlist messaging.

### 5.3 Firer invariants

- **Always succeeds.** Broken state surfaces loudly on the
  next save round-trip, not at fire time.
- **`fired_on` is caller-supplied.** Firing cadence is a
  caller-policy decision; the firer is date-neutral.
- **No dedup.** Two calls fire twice. Cooldown / historical-
  once is caller-policy.

### 5.4 Effects applicator invariants

- **Reuses `policy::apply_effects_to_actor` (M1.5/M5.6
  extract).** Events inherit the M1.5 target/op grammar +
  pre-flight atomicity.
- **Does NOT append `country.active_policies`.** Events aren't
  policies. Pinned by a dedicated regression test.
- **First-actor-wins selection policy.** All effects in
  `definition.effects` apply to ONE country — the one
  resolved from `instance.actors.front().country_id_code`.
- **Pre-flight atomicity inherited from M1.5.** Any failing
  effect leaves state untouched.

### 5.5 Composition (`tick_events`) invariants

- **Snapshot evaluation.** The evaluator runs ONCE at the top
  of a round; subsequent apply passes do NOT re-trigger
  evaluation in the same round. Cascade events wait for the
  next monthly tick. Pinned by a test.
- **Failure mode: record happens before apply each round.**
  On apply failure for match `i`, `event_history` contains
  matches `[0..i]` but only `[0..i-1]` had effects applied.
- **No `state.logs` append, no `state.applied_commands`
  append, no `country.active_policies` append.** All pinned
  by tests.

### 5.6 Monthly wiring (`step 7`) invariants

- **Step 7 runs LAST in `monthly::tick_all_countries`.** After
  every per-country tick (steps 1–3) and every state-wide
  reaction step (steps 4–6). Events evaluate against the
  freshest month-end snapshot.
- **`MonthlyOutcome::event_tick` mirrors `TickOutcome`.**
  Carries `events_matched`, `events_recorded`,
  `events_applied`, `total_effects_applied`.
- **Failure propagates** with the prefix
  `"monthly::tick_all_countries: event_engine::tick_events
  failed: " + inner_error`.

### 5.7 Canonical-content invariants (the "no-drift" property)

- **M5.1 canonical events are deliberately tuned to NOT fire
  on the canonical 1930 scenario.**
  - `low_stability_unrest` triggers at `country.stability <
    0.30`; canonical GER stability stays above 0.30 (initial
    0.55; M1.17 365-day soak doesn't drop it below 0.30).
  - `radical_interest_group_warning` triggers at
    `interest_group.radicalism > 0.75`; canonical IG
    radicalism stays below 0.75 (initial 0.10; reactions
    don't push it past 0.75).
- **M1.17 / M2 / M3 / M4 byte-identical determinism baselines
  remain byte-identical with their M4 close-out output.** No
  M5 sub-milestone rebakes them.
- **The day a future PR shifts canonical event thresholds or
  canonical dynamics so events DO fire on the canonical
  scenario, THAT PR rebakes the M1.17 / M2 / M3 / M4
  baselines — not any M5 PR.**

### 5.8 No new artefact / no new player-facing surface

- **Artefact set is still 10.** No `event_history.csv` /
  `event_history.json` / `events.csv` / `event_log.csv` /
  per-fire `LogEntry`s.
- **No new `RunnerOptions` field.** No `--events-csv` /
  `--skip-events` / `--events-jsonl` CLI flag.
- **No new `PlayerCommandKind`.** Events aren't player
  commands.
- **No UI surface.** Events absent from `map.html` /
  `provinces.svg` / any CSV.

### 5.9 RNG-free event pipeline

- **The entire M5 pipeline is RNG-free.** Evaluator, firer,
  applicator, engine, monthly wiring — none of them consult
  `state.rng`. Pinned indirectly by the M5.9 determinism test
  (two byte-identical hand-built states produce byte-
  identical 10 artefacts WITH events firing).

## 6. Deferred items

Items M5 deliberately did NOT ship, categorised by the M4
close-out convention (A / B / C):

### Category A — defer-to-M6+ gameplay-domain

These would make the event engine player-facing or
fundamentally change its scope. Each is a candidate for a
dedicated future milestone (or a sub-milestone in whatever
milestone the reviewer opens next):

- **`events.jsonl` fire records** (per-fire `LogEntry`
  emission into `state.logs`). Would surface events in the
  M0.6 lifecycle log.
- **UI surface for events.** Highlighting fired-event source
  countries / IGs in `map.html`, dedicated event CSV /
  histogram, player notifications, etc.
- **Event author tooling.** Validation CLI, schema doc
  generator, "what would fire?" checker, balance dashboard.
- **Selection-policy variants** (all-actors / weighted /
  random / `for_country:GER` / per-effect actor scoping). Per
  the PR #92 review, this belongs in its own dedicated
  selection-policy sub-milestone — NOT bundled with firing /
  effects / runner-integration PRs.
- **Chained events / choices / RNG-driven outcomes.**
  Parent/child event ids; player-choice options on
  `EventInstance`; RNG draws for outcome selection.
- **Broader trigger ops** (`eq` / `ne` / `between` / `in` /
  negation).
- **Broader trigger targets.** Factions, budget categories,
  `active_policies`, `current_date`, RNG state.
- **Broader actor kinds.** Faction etc.; would require
  growing the M5.4 kind allowlist.
- **Balance pass.** The day canonical events fire on the
  canonical scenario, that PR also rebakes M1.17 / M2 / M3 /
  M4 byte-identical determinism baselines (see §5.7).

### Category B — post-M5 follow-up polish

These are small extensions that don't change the architecture
and could be sub-milestones in a "post-M5 polish" or "M6
events polish" mini-milestone if the reviewer wants to land
them before moving on:

- **Runner CLI flag for events** (e.g. `--events-csv`,
  `--skip-events`, `--events-jsonl`).
- **`event_history`-driven gating** (cooldown / historical-
  once / fire-at-most-N-times-per-period). Caller policy that
  consults `state.event_history` before `tick_events`.
- **Trigger logical operators** (`and` / `or` / `not` / `xor`
  per event-level combination).
- **`events.jsonl` semantic extension** (per-fire log entries
  alongside the M0.6 lifecycle log).
- **Typed `EventId` on the gameplay path.** `EventId` is
  still defined in `core/ids.hpp` but not on any code path;
  if a future system wants a strong handle, it can switch.

### Category C — not-needed-for-close nice-to-haves

These are deferred indefinitely until a specific need
surfaces:

- **Save schema migration shim** for v13 → v14 (M5 chose
  loud rejection; that's the M0.8 contract).
- **Event categories / tags** (severity / domain / source).
- **Per-fire severity / log template** (presupposes
  `events.jsonl` emission).
- **`EventDefinition` validation CLI** (presupposes event
  author tooling).
- **Atomic `end_tick`** (carried over from M4 close — not
  M5-specific; touches save / events.jsonl / CSV / SVG / HTML
  writes uniformly).

## 7. Recommended next milestone candidates

Next milestone direction should be chosen explicitly by the
reviewer.

Candidates:

- **Stop M5 cleanly and move to a new milestone.** The PR #98
  reviewer recommended this path (*"直接 close M5，不要在 M5
  繼續加 events.jsonl / cooldown / CLI，避免把事件系統 scope
  拉大"*) and M5.10 honours that recommendation. The next
  milestone is unspec'd here; RFC-090 candidates include
  M6 (whatever the reviewer numbers next under RFC-090) for
  the next gameplay-domain layer (e.g. multi-country
  interaction, war / diplomacy, AI, replay-driven verify).
- **Post-M5 follow-up polish** *if* the reviewer wants to
  land one or more Category B items before moving on.
  `events.jsonl` fire emission, `event_history`-driven
  cooldown, and a `--events-csv` flag are the three most
  likely candidates. Could be one PR each, or batched if
  scoped carefully — but each would extend M5 retroactively,
  which is exactly what M5.10 declined to do.
- **Pick up Category A gameplay-domain items.** Each would
  more naturally fit in a dedicated future milestone (event
  UI surface alongside any future viewer-polish milestone;
  chained events / choices / RNG outcomes as their own
  event-engine-expansion milestone; selection-policy
  variants in their own selection-policy sub-milestone per
  PR #92).

M5.10 deliberately does **not** open or claim any of the
above. No "M6 in progress" wording lands in this PR; the
next milestone starts when the reviewer says so. The
2026-05-17 force-reset lesson (don't invent milestone
numbers; don't pre-open the next milestone in a close-out
PR) holds: whatever the reviewer picks next starts in its
own deliberate first sub-milestone PR.

**M5 closes here.**

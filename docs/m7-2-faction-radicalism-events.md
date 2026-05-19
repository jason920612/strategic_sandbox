# M7.2 — 加入派系激進度事件 (faction-radicalism events)

- Status: shipped (PR pending)
- RFC anchors: **RFC-090 §7.2**, RFC-020 §6 / §7, RFC-050 §3
- Save format: **unchanged** (no persistent-field addition; the
  M5.4 event_history actor-kind allowlist gains the
  `"faction"` value but the v18 → v19 schema-version bump is
  M7.1's responsibility, not this PR's)
- Artefact contract: 11 (unchanged)
- Branch: `feature/m7-02-faction-radicalism-events`

## 1. Scope

RFC-090 §7.2 reads simply `加入派系激進度事件` (add
faction-radicalism events). RFC-020 §6 lists faction fields
(support, influence, radicalism, ...); RFC-020 §7 establishes
that factions are political actors with their own dynamic
reactions to state events. M5 already shipped the event
engine with `interest_group.radicalism` as one trigger target;
M7.2's strict reading adds `faction.radicalism` — events
keyed off `FactionState.radicalism` directly.

What this PR ships:

1. `EventTrigger` target allowlist extended with
   `faction.radicalism` (scenario_loader + save_system layer).
2. `event_evaluator::TriggerActorKind` gains a third variant,
   `Faction`. New scope predicate `target_is_faction_scope`,
   value projector `pick_faction_value`, and "first satisfying"
   helpers `first_faction_index_satisfying` (global) and
   `faction_index_satisfying_for` (per-country).
3. `event_evaluator::trigger_matches` / `trigger_actor` /
   `evaluate_match_for_country` route `faction.*` triggers
   through the new helpers and bind a `Faction`-kind
   `TriggerActor` with the owning country's `CountryId`.
4. `event_evaluator::rank_weighted_events` accepts
   `faction.radicalism` in WeightModifier targets.
5. `event_firer::to_actor` maps `TriggerActorKind::Faction`
   to the string `"faction"` for the EventInstanceActor.kind
   field; the owning-country id_code is looked up from
   `state.countries` by `FactionState.country` handle (same
   shape as the InterestGroup actor case).
6. Save-layer `event_history` actor-kind allowlist gains
   `"faction"` so saves carrying faction actors round-trip
   without rejection.
7. One canonical faction-radicalism event in
   `data/events/1930_rfc_extended_events.json`:
   `faction_radicalism_crisis`, firing on
   `faction.radicalism > 0.85`.

## 2. Strict RFC reading (what was NOT done)

- Only `faction.radicalism` is added. `faction.loyalty`,
  `faction.support`, and `faction.influence` are NOT in
  the allowlist after M7.2 — RFC-090 §7.2 says
  *radicalism*; a future sub-milestone widens the
  allowlist when an RFC anchor authorises it.
- No `faction.country` / `faction.type` / non-numeric
  faction-side trigger targets.
- No save schema version bump (the allowlist extension is
  backward-compatible: pre-M7.2 saves carry no `"faction"`
  actors, and the version-mismatch gate already exists for
  truly incompatible saves).
- No new player-facing command, no new CLI flag, no new
  artefact (still 11), no `state.rng` consumption beyond
  what `event_engine::tick_events` already does (M5.7).
- No claim that M6 is closed. M6 closure decision
  remains with Jason per `docs/m6-closeout-audit.md` §1.
- No follow-on M7.x work in this PR.

## 3. Trigger evaluation (M7.2 addition)

Aggregation: an `faction.*` trigger matches when at least
one faction in `state.factions` satisfies it. Empty
`state.factions` evaluates the trigger as FALSE — same
ANY-satisfies shape as country / interest_group scopes.

Per-country scoping (issue #112 contract preserved): when
`evaluate_match_for_country(state, country_id, def)` evaluates
a `faction.*` trigger, only factions whose
`FactionState.country == country_id` are scanned. A
high-radicalism faction in country X does NOT make country Y's
event pool match.

First-satisfying actor binding (M5.3 contract preserved): the
TriggerActor records the FIRST faction in vector order that
satisfies the trigger. Stable because `state.factions` is
canonically ordered (scenario-load insertion order, or
save-load deserialisation order — both deterministic).

## 4. Event-firer actor surface (M7.2 addition)

`event_firer::to_actor` maps `TriggerActorKind::Faction` to
the EventInstanceActor:

```cpp
EventInstanceActor {
    kind            = "faction",
    id_code         = faction.id_code,
    country_id_code = country_id_code_for(state, faction.country),
    index           = position in state.factions
}
```

The owning-country lookup uses the existing
`country_id_code_for` helper (linear scan of
`state.countries` by CountryId; matches the InterestGroup
path). If the lookup fails (internal state inconsistency),
the resulting empty `country_id_code` is rejected by the
M6.9 actor-binding preflight in `record_match` /
`record_followup` (defense in depth — every distortion-
field emission requires a non-empty country anchor).

The downstream M6.8 / M6.9 / M6 closeout-audit metadata
keys (`true_cause`, `publicText`, `information_accuracy`,
`propaganda_bias_sample`, `reported_intensity`,
`noise_sample`) all continue to apply to faction-kind
fires — the actor kind is only used as a metadata label.

## 5. Save-layer extension (backward-compatible)

The `event_history` actor-kind allowlist in the loader
(see `save_system.cpp` `kEventHistoryActorKindsSave`):

```cpp
// Pre-M7.2:  {"country", "interest_group"}
// Post-M7.2: {"country", "interest_group", "faction"}
```

The save-version constant is UNCHANGED by this PR. The
extension is backward-compatible:

- Pre-M7.2 saves carry no `"faction"` actors, so they
  parse identically under the new allowlist.
- Post-M7.2 saves may carry `"faction"` actors, which
  fail to parse under a pre-M7.2 binary's older allowlist
  — but that direction is already gated by the
  `save_version` constant mismatch (a pre-M7.2 binary
  rejects any save whose version it does not recognise).

No EventDefinition schema field is added or removed; the
trigger-target allowlist is loader-layer logic, not a
persistent shape.

## 6. Canonical faction-radicalism event

`data/events/1930_rfc_extended_events.json` gains a new
entry:

```json
{
  "id": "faction_radicalism_crisis",
  "name": "Faction Radicalism Crisis",
  "description": "A faction's radicalism has climbed to a politically dangerous level.",
  "visible_report": "Reports describe an organised faction agitating openly against the current direction of policy.",
  "true_cause": "A FactionState.radicalism has crossed the faction-radicalism-crisis threshold (M7.2, RFC-090 §7.2).",
  "category": "political",
  "triggers": [
    { "target": "faction.radicalism", "op": "gt", "value": 0.85 }
  ],
  "effects": [
    { "target": "country.stability", "op": "add", "value": -0.01 }
  ]
}
```

Threshold `0.85` is a game-model assumption per RFC-080 §11
(direction grounding cited in
`docs/m7-1-faction-demands.md` §6: Alesina-Perotti
instability-via-discontent loop). The `country.stability`
effect at `-0.01` matches the asymptotic-add convention
established in PR #115 hardening.

The compliance `1930_rfc_compliance` scenario carries only
three GER factions whose authored radicalism (0.10 – 0.30)
plus the M3.2 drift toward `1 - stability` keeps them under
0.85 for the full 1930 → 2000 sweep. The
`faction_radicalism_crisis` event is therefore NOT
exercised by the canonical / compliance sweeps; it is
exercised by the new
`tests/systems/faction_radicalism_events_test.cpp` unit
tests against hand-built above-threshold states.

## 7. Tests + verification

- `cmake --build build --config Debug` succeeds.
- `build/bin/Debug/leviathan_tests.exe` reports
  **1311 cases / 96 264 assertions / 0 failed**, verified
  via direct binary run per
  `feedback_ctest_masks_doctest`.
- Delta from M6 closeout audit (1301 / 96 228): +10 unit
  tests in `tests/systems/faction_radicalism_events_test.cpp`
  (trigger_matches at / above threshold, empty-state
  false, first-faction-in-vector-order actor binding,
  per-country scoping does not leak across countries,
  no-qualifying-faction-in-country returns empty,
  `rank_weighted_events` accepts `faction.radicalism`
  modifier, `record_match` emits `kind="faction"`,
  save round-trip preserves `"kind": "faction"`).
- 3 stale-error-message tests updated for the new error
  strings (event_engine_test, rcr_1_event_extensions_test,
  scenario_loader_test).
- 1 M5.4 allowlist-rejection test updated: the original
  used `"faction"` as the negative case; M7.2 promotes
  it to valid, so the negative example becomes
  `"media_org"` (never in the allowlist).
- Canonical `1930_minimal` 365-day sweep: `Sanity issues
  : 0`. The canonical scenario does not load
  `1930_rfc_extended_events.json`, so the new event
  definition is irrelevant to canonical determinism.
- Compliance `1930_rfc_compliance` 25 567-day sweep:
  `Sanity issues : 0`. The new event definition loads
  but never fires (no GER faction reaches radicalism >
  0.85 in 70 years).

## 8. What this PR deliberately DOES NOT do

- No save schema version bump.
- No new player-facing command.
- No new CLI flag.
- No new artefact.
- No `state.rng` consumption beyond the existing M5.7
  per-country / per-category weighted-random draw.
- No `faction.loyalty` / `faction.support` /
  `faction.influence` trigger targets — those are out
  of RFC-090 §7.2's strict scope.
- No claim that M6 is closed.
- No M7.3+ work started.

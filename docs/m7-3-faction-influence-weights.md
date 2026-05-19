# M7.3 — 加入派系影響力權重 (faction-influence weights)

- Status: shipped (PR pending)
- RFC anchors: **RFC-090 §7.3**, RFC-020 §6
- Save format: **unchanged** (allowlist extension only; loader-
  layer logic, not a persistent shape)
- Artefact contract: 11 (unchanged)
- Branch: `feature/m7-03-faction-influence-weights`
- Depends on: PR #120 (M7.2 faction-radicalism events) for the
  `TriggerActorKind::Faction` infrastructure

## 1. Scope

RFC-090 §7.3 reads `加入派系影響力權重` ("add
faction-influence weights"). The compound `影響力權重` =
"influence-as-weight". RFC-020 §6 lists `influence` as a
first-class faction field that the simulation already
stores on `FactionState` since M1.2; M7.3 makes it
observable to the event engine so:

1. Author-supplied **WeightModifiers** can weight an event's
   firing weight by faction influence (the primary §7.3
   intent).
2. Events can also **trigger** on `faction.influence` directly
   (the trigger / weight-modifier allowlist share the same
   predicate `target_is_faction_scope`).

What this PR ships:

- `EventTrigger` target allowlist extended with
  `faction.influence` (scenario_loader + save_system).
- `event_evaluator::pick_faction_value` projects
  `faction.influence` to `FactionState.influence`.
- `event_evaluator::target_is_faction_scope` accepts the new
  string; downstream `trigger_matches` / `trigger_actor` /
  `evaluate_match_for_country` / `rank_weighted_events`
  inherit the extension automatically.
- M7.2's `faction_radicalism_crisis` event gains a
  `WeightModifier{ target: faction.influence, op: gt,
  value: 0.60, weight_delta: 0.5 }` so the crisis is
  weighted UP when a high-influence faction exists.
- New canonical event `influential_faction_pressure` keyed
  off `faction.influence > 0.80` with effect
  `country.legitimacy -0.01` (asymptotic-add convention).

## 2. Strict RFC reading (what was NOT done)

- Only `faction.influence` is added. `faction.loyalty` and
  `faction.support` are NOT in the allowlist — RFC-090 §7.3
  says *influence*; a future sub-milestone widens the
  allowlist when an RFC anchor authorises it.
- No new save schema field.
- No new player command, no new CLI flag, no new artefact.
- No `state.rng` consumption beyond what
  `event_engine::tick_events` already does.
- No claim that M6 is closed.

## 3. Tests + verification

- `cmake --build build --config Debug` succeeds.
- `build/bin/Debug/leviathan_tests.exe` reports
  **1318 cases / 96 285 assertions / 0 failed**.
- Delta from M7.2 (1311 / 96 264): +7 new unit tests in
  `tests/systems/faction_influence_weights_test.cpp`
  (trigger_matches at / below threshold;
  faction.influence → Faction-kind TriggerActor; per-country
  scoping does not leak; rank_weighted_events raises weight
  when faction is influential; rank_weighted_events leaves
  weight at base when not; save round-trip preserves the
  trigger + weight_modifier target strings).
- Canonical `1930_minimal` 365-day sweep:
  `Sanity issues : 0`.
- Compliance `1930_rfc_compliance` 25 567-day sweep:
  `Sanity issues : 0`. The new event definitions load but
  rarely fire (canonical GER factions start at influence
  0.60 – 0.70 — the M7.3 `influential_faction_pressure`
  event keyed on `faction.influence > 0.80` does not fire
  on canonical state; the M7.2 crisis's new weight
  modifier never activates because canonical radicalism
  also stays below the M7.2 threshold).

## 4. What this PR deliberately DOES NOT do

- No save schema version bump.
- No new player-facing command.
- No `faction.loyalty` / `faction.support` trigger targets.
- No new RFC-090 milestone feature beyond §7.3.
- No M7.4+ work started.

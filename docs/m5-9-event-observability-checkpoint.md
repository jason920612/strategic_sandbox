# M5.9 - Event observability checkpoint

Companion notes for
`feature/rfc090-m5-09-event-observability-checkpoint`.

**This is RFC-090 M5** (event engine). M5.9 is the M5 status
**checkpoint** — a docs + integration-tests PR mirroring M3.7
(reaction-loop integration checkpoint), M4.9 (DOM contract
checkpoint), M4.14 (DOM contract refresh), M4.18 (accessibility
checkpoint refresh), and M4.22 (close-out readiness
checkpoint). M5.9 ships **zero new gameplay**: no new system,
formula, artefact, save schema bump, RunnerOptions field, CLI
flag, canonical-fixture change, or close-out.

The purpose: capture the M5.1–M5.8 contract end-to-end in one
single-page snapshot doc + a runner-driven integration test
file so the next reviewer (or the next AI assistant resuming
M5) has one canonical reading source for "what does the event
engine look like right now?" — instead of piecing together
nine per-sub-milestone notes.

## 1. Scope

What ships:

```text
docs/milestone-5-checkpoint.md                       (new — M5 status snapshot)
docs/m5-9-event-observability-checkpoint.md          (new — this design note)
tests/integration/m5_event_pipeline_test.cpp         (new — 5 doctest cases)
tests/CMakeLists.txt                                 (registers the new test)
README.md / docs/README.md / rfc/README.md           (flipped to "Latest = M5.9")
```

What does NOT change:

```text
no new system / module
no new formula
no new artefact (still 10)
no save format bump (still v14)
no new state field
no new event_* module API change (M5.1-M5.8 surfaces frozen)
no policy_system change
no scenario_loader change
no monthly_pipeline behaviour change
no canonical fixture change
no new RunnerOptions field
no new CLI flag
no new PlayerCommandKind
no events.jsonl semantic change
no UI surface
no M1/M2/M3/M4 system behaviour change
no rebake of M1.17 / M2 / M3 / M4 byte-identical
   determinism baselines
no M5 close-out (no docs/milestone-5-result.md;
   no "M5 closed" wording)
```

## 2. Why a checkpoint at M5.9, not earlier

The pattern M3 and M4 established was: ship a checkpoint at
the *mid-milestone moment when the surface is structurally
complete and the next step is either close-out or one more
polish*. M3.7 fit because M3.1–M3.6 had wired a closed reaction
loop. M4.9 fit because M4.2–M4.8 had built the DOM identity
surface.

For M5, that moment is M5.8: the inner-loop pipeline
(M5.1→M5.6) plus the composition helper (M5.7) plus the
monthly-tick wiring (M5.8) together form a structurally
complete event engine that just happens to deliberately not
fire on the canonical scenario. Anything M5 might do beyond
that is either a polish item (events.jsonl emission, CLI flag,
cooldown gating) or a scope extension (broader trigger ops,
selection-policy variants, chained events) — both readable as
"what should follow the checkpoint", not as "what should land
before the checkpoint".

M5.9 captures that moment so M5 can either close out next or
pick up one polish item without losing the contract narrative.

## 3. What's in the checkpoint doc

`docs/milestone-5-checkpoint.md` covers:

- **§1 sub-milestones shipped** — full M5.1–M5.8 ledger.
- **§2 current M5 dataflow** — scenario → schema → save +
  monthly tick → evaluator → firer → applicator chain; the
  inner loop of `event_engine::tick_events`; how
  `state.event_history` round-trips through the save layer.
- **§3 M5 schema** (frozen at M5.4): `EventTrigger` /
  `EventDefinition` / `EventInstanceActor` / `EventInstance`
  + the two GameState vectors.
- **§4 M5 allowlists** (frozen at M5.1 + M5.4): 5 trigger
  targets, 4 trigger ops, 2 actor kinds.
- **§5 artefact set** — still 10; M5 added zero.
- **§6 current invariants** — ~25 invariants spanning save
  schema, no-events.jsonl-change, no-applied_commands /
  no-active_policies pollution, monthly-step-7 ordering,
  determinism, and canonical-non-fire semantics.
- **§7 deferred items** — categorised list of what's NOT in
  M5: events.jsonl emission, CLI flag, gating, selection-
  policy variants, chained events, broader ops, balance
  pass, etc.
- **§8 what does NOT change in a checkpoint refresh** — the
  hard "this PR is docs + tests only" rules.
- **§9 close-out readiness assessment** — Category A / B / C
  breakdown of remaining deferred items + verdict ("M5 is
  structurally complete; reviewer's next call is close-out
  vs one polish PR vs move to M6").
- **§10 notes for the next reviewer** — what a future M5.x
  PR should refresh in this doc + the close-out shape when
  it comes.

## 4. What's in the integration test

`tests/integration/m5_event_pipeline_test.cpp` adds 5 doctest
cases pinning the M5 contract end-to-end through the
runner:

- **A. canonical scenario at M5.9** (365-day run): pins the
  most load-bearing M5 invariant — `event_history` stays empty
  because canonical events deliberately don't fire on
  canonical state. Also pins `save_version: 14`, the 10-file
  artefact set, no canonical event id_code in `events.jsonl`,
  and no new event-related artefact on disk.
- **B. firing event lands its effect**: hand-built state with
  an event whose trigger initially matches → 31-day runner
  run → `event_history` populated, effect applied (legitimacy
  drops by 0.05 from 0.50 → 0.45), save round-trip preserves
  both.
- **C. no new artefact when events fire**: a firing run STILL
  produces exactly the same 10-file set; no
  `event_history.csv` / `event_history.json` / `events.csv`
  appear on disk; `events.jsonl` semantics unchanged; no
  applied_commands / active_policies pollution from the event
  path.
- **D. firing run is deterministic**: two byte-identical
  hand-built states with the same options produce
  byte-identical 10 artefacts WITH events firing. The M5
  pipeline is deterministic.
- **E. failure path through runner**: an event with a bad
  effect target causes monthly tick failure → M1.5 pre-flight
  reject → `event_engine::tick_events` failure → `run_state`
  failure → M2.9 contract (no output artefacts written before
  `end_tick` is reached).

Each test exercises the FULL pipeline (scenario load /
hand-built state → monthly tick wiring → tick_events
composition → evaluator → firer → applicator → save
serialisation → reload). Each leg's unit tests already pin
the local behaviour; this file pins the cross-leg seam.

Test counts: 1039 doctest cases / 62364 assertions (was
1034 / 62303 at M5.8 close; +5 cases / +61 assertions).

## 5. Why no existing tests change

Three reasons:

1. **No code change** beyond test registration in
   `tests/CMakeLists.txt`. The event modules
   (event_evaluator / event_firer / event_effects /
   event_engine), policy_system, monthly_pipeline,
   scenario_loader, save_system, and runner all stay
   byte-identical with M5.8 close.
2. **Canonical events deliberately don't fire**. The 365-day
   canonical soak in test A produces an empty `event_history`
   — same as M5.8 close. No M1.17 / M2 / M3 / M4 byte-identical
   determinism baseline shifts.
3. **The new integration tests are pure additions**. They
   use independent temp dirs and hand-built states (test B/C/D)
   or the canonical scenario via the runner (test A). Nothing
   they do depends on or perturbs other tests.

## 6. Future-rebake warning (preserved from PR #97 review)

The PR #97 (M5.8) reviewer flagged that the day a future PR
shifts canonical event thresholds OR canonical dynamics so
events DO fire on the canonical 1930 scenario, THAT PR must
rebake the M1.17 / M2 / M3 / M4 byte-identical determinism
baselines. M5.9 ships zero rebake. The §6 invariants list in
`milestone-5-checkpoint.md` now pins this explicitly so future
authors / models reading the checkpoint know the threshold
tuning is load-bearing for the canonical-non-fire property.

## 7. What M5.9 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no new system / formula / artefact
no save format bump (still v14)
no new state field
no new RunnerOptions field / CLI flag
no new PlayerCommandKind
no new event-module / policy_system / monthly_pipeline
   API change
no canonical fixture change
no scenario_loader change
no behavioural change anywhere
no events.jsonl semantic change
no rebake of M1.17 / M2 / M3 / M4 determinism baselines
no UI surface
no balance pass
no M5 close-out (this is a checkpoint, not an exit report;
   no docs/milestone-5-result.md, no "M5 closed" wording)
no opinion on whether M5 should close next or take one more
   polish PR — §9 of the checkpoint doc enumerates the
   options; M5.9 does not pick
```

M5 remains in progress. M5.9 captures the contract; the next
sub-milestone is unspec'd and waits for the reviewer.

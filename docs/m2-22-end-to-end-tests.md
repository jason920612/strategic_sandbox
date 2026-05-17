# M2.22 - M2 exit / end-to-end integration tests

Backfilled per-sub-milestone design note for M2.22. The
canonical M2 exit ledger lives in
[`milestone-2-result.md`](milestone-2-result.md); this file
covers the M2.22-specific deliverable in isolation so the
per-sub-milestone naming convention is unbroken across M2.

## 1. Scope

M2.22 is the **exit / integration sub-milestone** for M2.
Mirrors M1.17's role: no new gameplay system, no new
formula, no new artefact — only the end-to-end test surface
that pins M2 as a closed milestone.

What shipped:

```text
tests/integration/m2_end_to_end_test.cpp    3 new doctest cases
docs/milestone-2-result.md                  M2 exit report
README.md / docs/README.md / rfc/README.md  flipped to "M2 closed"
```

The three integration cases:

1. **Command script → replay → `compare_states` equivalence.**
   Drives `commands::apply_command_script` on a fresh source
   state, then `commands::replay_with_time` on an independent
   target state, then `diagnostics::compare_states` to confirm
   the two converge across every gameplay-relevant field.
   Pins the M2.4 / M2.7 / M2.20 / M2.21 contract: a successful
   script round-trips through the replay log without drift.

2. **Order-execution gate atomicity across command kinds.**
   Drives a mixed script (`AdjustBudget("military")` +
   `EnactPolicy` + `AdjustBudget("welfare")`) through
   `apply_command_script` against a country with low
   `bureaucratic_compliance` but high `military_loyalty`.
   Pins that the military adjustment lands (gate inputs
   selected by category per M2.19), the `EnactPolicy` is
   rejected by the M2.18 gate, and the trailing welfare
   adjustment is left untouched (M2.3 mid-list-failure
   atomicity inherited through M2.18 / M2.19 / M2.20 / M2.21).

3. **5-artefact byte-identical determinism with M2 commands
   applied.** Composes `begin_tick` + `apply_command_script`
   + `step_one_day × 31` + `end_tick` twice into two
   independent temp dirs and asserts `save.json` /
   `events.jsonl` / `summary.csv` / `countries.csv` /
   `factions.csv` all match byte-for-byte. M1.17's
   determinism contract carried through M2's command + gate
   path.

## 2. Why this is its own sub-milestone

The script + replay + verify trio is what makes "M2 closed"
meaningful. Without it the M2 sub-milestones each pinned
their own slice (queue, log, replay primitives, verify CLI,
authority block, execution gates, rejection reporting,
script driver) but nothing exercised the full M2 frontier
end-to-end. M2.22 fills that gap and writes
`milestone-2-result.md` to record what M2 shipped, what
was deliberately deferred, and the architectural invariants
every M3+ milestone must preserve.

## 3. What M2.22 does NOT do

```text
no new system
no new formula
no new state field
no new artefact
no save schema bump
no new CLI flag
no new PlayerCommandKind
no events
no AI
no UI / REPL
no M3 work
```

Anything that would change the binary's behaviour is by
definition M3-or-later scope.

## 4. Cross-references

- [`milestone-2-result.md`](milestone-2-result.md) — canonical
  M2 exit report (every M2.x sub-milestone summarised in two
  phases, M3+ invariants, deferred items including `Delayed` /
  `Distorted` outcomes / scheduler / RNG resistance / attempted-
  command log, recommendations for M3+).
- [`m2-21-command-script-driver.md`](m2-21-command-script-driver.md)
  — the `apply_command_script` helper the first and third
  cases exercise.
- [`m2-7-replay-with-time.md`](m2-7-replay-with-time.md) — the
  replay primitive whose equivalence the first case pins
  against the directly-driven script.
- [`m2-10-state-comparison.md`](m2-10-state-comparison.md) —
  the `compare_states` API the first case uses to assert
  equivalence.
- [`m2-18-enact-policy-execution-gate.md`](m2-18-enact-policy-execution-gate.md)
  and [`m2-19-adjust-budget-execution-gate.md`](m2-19-adjust-budget-execution-gate.md)
  — the gates the second case exercises with deliberately
  low compliance and high military loyalty.

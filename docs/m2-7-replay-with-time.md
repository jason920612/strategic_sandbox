# M2.7 - Replay with time-system advancement

Companion notes for `feature/m2-07-replay-with-time`. M2.7 lifts the
M2.6 prototype's "no time advance" limit by interleaving day-by-day
simulation advancement with command application. The M1.10 monthly
pipeline therefore runs naturally between commands; the killer
equivalence test pins that the resulting state matches the original
simulation byte-for-byte.

## 1. Scope

M2.6 shipped `commands::replay(state, log)`: snap each command at its
recorded date, no time-system involvement. M2.7 ships
`commands::replay_with_time(state, opts, ctrl, log)`: same semantics,
but the simulation advances day-by-day between commands using the
M2.2 `step_one_day` primitive. The reviewer's M2.7 gate was explicit:

> "只讓 replay 按日期逐日 advance 並在日期抵達時套用 command，
> 不要做 UI、AI、事件或完整 divergence report"

So this PR ships one new free function. M2.6 `replay` stays unchanged
— both primitives coexist (M2.6 for date-stripped replay, M2.7 for
full timeline replay).

## 2. Public API

`include/leviathan/systems/commands.hpp`:

```cpp
namespace leviathan::systems::commands {

core::Result<ReplayOutcome> replay_with_time(
    core::GameState& state,
    const runner::RunnerOptions& opts,
    runner::TickController& ctrl,
    const std::vector<core::AppliedPlayerCommand>& log);

}  // namespace leviathan::systems::commands
```

Reuses M2.6's `ReplayOutcome` struct (`int commands_replayed`).
Caller pattern:

```cpp
runner::TickController ctrl;
runner::begin_tick(state, opts, ctrl);             // M2.2
commands::replay_with_time(state, opts, ctrl, log);
// state.current_date == log.back().applied_on now.
// caller can step further or call end_tick.
runner::end_tick(state, opts, ctrl);               // M2.2
```

`commands.hpp` now includes `runner.hpp` (for `RunnerOptions` +
`TickController`). The dependency direction is still acyclic:
`runner` does not include `commands`.

## 3. Algorithm

```
for entry in log:
    if entry.applied_on < state.current_date:
        return failure("replay_with_time[i]: out-of-order")
    while state.current_date < entry.applied_on:
        step_one_day(state, opts, ctrl)   // M2.2
    // state.current_date == entry.applied_on
    CommandQueue q; q.pending.push_back(entry.command);
    apply_pending(state, q)               // M2.3+ dispatch
```

Two things to note:

1. **`step_one_day` runs the monthly pipeline on month boundaries.**
   So if the log advances across Feb 1, M1.10's `monthly::tick_all_countries`
   fires; `ctrl.monthly_ticks` increments; faction / stability /
   economy state evolves.
2. **The monotonicity check is against `state.current_date`,
   not against the previous log entry.** This handles the case
   where the caller stepped the state past entry[0]'s `applied_on`
   before calling replay — that's an unsupported scenario but the
   error message will name it correctly.

## 4. Preconditions

Rejected loudly with state untouched on the first three; the fourth
is checked entry-by-entry during the loop:

- **`state.player_country` valid** + indexes into `state.countries`.
- **`state.applied_commands` empty.** Replay would otherwise mix new
  entries with prior ones.
- **`ctrl.started && !ctrl.ended`.** Caller must have invoked
  `runner::begin_tick` before this function.
- **Monotonic dates.** For each `log[i]`, `log[i].applied_on >=
  state.current_date` (the entry can be on the current date or
  later, never earlier).

## 5. Atomicity

Same shape as M2.3 / M2.6:

- On a `step_one_day` failure (M1.10 monthly pipeline can fail),
  return `Result::failure` with the offending entry's index and
  the underlying error.
- On an `apply_pending` failure (unknown policy / bad budget
  category / non-finite delta), same.
- Prior entries [0, i) stay applied + logged.
- Entry i is **not** applied (per-command atomicity inside
  `apply_pending` kicks in if it reached dispatch; `step_one_day`
  failure means the entry never reached dispatch).
- Entries (i, end) are **not** replayed.

A subtle wrinkle inherited from M2.6: `state.current_date` may have
moved partway toward entry `i`'s `applied_on` before the failure
fired. The PR #34 reviewer flagged this for the prototype; M2.7
preserves the same behaviour. Full transactional replay would need
a save/restore mechanism that's out of scope.

## 6. Why both `replay` and `replay_with_time` coexist

- `replay`: instant replay, no time advance. Useful when the test
  only cares about command effects on numeric fields and doesn't
  want monthly pipeline noise. Cheaper to run (no day loop).
- `replay_with_time`: full timeline replay. Required when the test
  cares about effects the simulation systems produce between
  commands (GDP growth, stability drift, faction react), or when
  comparing replayed state to a real simulation.

Both share the same `ReplayOutcome` + the same precondition shape
(except `replay_with_time` adds the controller-state requirements).

## 7. Tests

10 new doctest cases (M2.6 was 503 → M2.7 is 513), all in
`tests/systems/commands_test.cpp` under an "M2.7: replay_with_time"
section:

- **Empty log** is a no-op success; `days_stepped == 0`;
  `current_date` unchanged.
- **Command at `start_date`** applies with no advance
  (`days_stepped == 0`).
- **Command 5 days later** advances exactly 5 days, applies on
  day 6.
- **Command past a month boundary** (Jan 1 → Feb 15) advances 45
  days, runs the monthly pipeline once (`monthly_ticks == 1`),
  applies on Feb 15.
- **Multiple commands at different dates** each block advances +
  applies; cumulative counters add up.
- **Out-of-order log** rejected with `replay_with_time[N]:
  out-of-order` and the offending dates in the message; prior
  entries already applied + logged.
- **`TickController` not started** rejected with `not been
  started` in the error; `applied_commands` empty.
- **No `player_country` selected** rejected with `player_country`
  in the error.
- **Non-empty `applied_commands`** rejected with
  `applied_commands must be ...`.
- **Full equivalence with original simulation** (the killer test):
  drive a source state through `begin_tick → step×5 → apply
  EnactPolicy → step×40 (crosses month boundary) → apply
  AdjustBudget`; capture `source.applied_commands`. Build a fresh
  target, replay the log via `replay_with_time`. Verify that
  every observable matches: `current_date`,
  `ctrl.days_stepped`, `ctrl.monthly_ticks`,
  `applied_commands.size()`, each log entry's `applied_on` +
  `kind`, the command-effect fields (`legal_tax_burden`,
  `budget.military`), AND the monthly-pipeline-mutated fields
  (`gdp`, `stability`, `last_gdp_growth_rate`). The last group
  is the load-bearing one — it proves the time-system fires
  identically during replay.

## 8. What's NOT in scope

Deliberate non-goals (per the M2.7 reviewer gate):

- **No UI.** Replay remains a library primitive.
- **No AI.** No automated decision-making during replay.
- **No event integration.** RFC-050 events are still future work.
- **No divergence report.** The function returns ok / failure;
  the caller does the actual state comparison.
- **No save format change.** Schema stays v9.
- **No new CLI flag** like `--replay PATH`. Adding one is a
  natural follow-up but distinct from this primitive.
- **No transactional rollback on mid-list failure.** Prior commands
  stay applied; state.current_date may have moved partway. The
  PR #34 reviewer flagged this for the prototype; M2.7 preserves
  the documented behaviour.
- **No `current_date` extension past the last log entry.** The
  caller can call `step_one_day` further if needed.
- **No new `state.logs` lifecycle entry** like "replay started" /
  "replay finished". The replay log is its own surface; M0.6
  state.logs stays untouched.

## 9. Cross-links

- RFC-050 §8 — "玩家命令需記錄" — recording was M2.4; M2.7 is the
  consumer-side primitive (full version after the M2.6 prototype).
- M2.2 (`m2-2-pause-resume.md`) — `step_one_day` is the per-day
  primitive M2.7 leans on.
- M2.3 (`m2-3-command-queue.md`) — the dispatch loop
  `apply_pending`, reused 1-element-per-entry.
- M2.4 (`m2-4-command-log.md`) — the log shape M2.7 consumes.
- M2.5 (`m2-5-adjust-budget.md`) — second command kind. M2.7
  supports it automatically because it goes through
  `apply_pending`.
- M2.6 (`m2-6-replay-prototype.md`) — the time-stripped sibling.
- M1.10 (`m1-10-runner-monthly-wiring.md`) — the monthly pipeline
  M2.7 wires into via `step_one_day`.

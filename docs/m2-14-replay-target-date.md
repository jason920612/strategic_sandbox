# M2.14 - Replay target-date CLI

Companion notes for `feature/m2-14-replay-target-date`. M2.14 adds
a single CLI knob — `--target-date YYYY-MM-DD` — that scopes the
M2.8 replay flow to a specific calendar day. Two effects layered
into one flag: the recorded log is truncated to entries with
`applied_on <= target_date`, and the time system is advanced
post-replay until `state.current_date == target_date` so the
written save reflects exactly that day.

Requires `--replay`. No save schema change.

## 1. Scope

Until M2.14, `--replay` always walked the entire recorded log and
stopped at `log.back().applied_on`. Two common forensic / regression
use cases were awkward:

1. "Replay the first N days only" — easy enough by hand-trimming
   the log on disk, but the runner couldn't do it.
2. "Replay everything, then advance to a later inspection date" —
   easy via M0.9 `--days` on a non-replay run, but `--days` is
   ignored in replay mode.

M2.14 covers both with one flag. The truncation half is just a
`std::partition`-style scan over `loaded.applied_commands`; the
extension half is a `while (current_date < target) step_one_day`
loop after `replay_with_time` returns.

The flag is `--replay`-only by design. `--target-date` without
`--replay` would just be a clumsy alias for "`--days` adjusted to
land on a specific date", and we don't want two surfaces doing the
same job. Parse rejects the orphan with both flag names in the
error.

## 2. Public API

`include/leviathan/systems/runner.hpp`:

```cpp
struct RunnerOptions {
    // ...
    std::optional<core::GameDate> target_date;   // M2.14
};
```

`std::nullopt` keeps the M2.8 behaviour (replay the whole log,
stop at `log.back().applied_on`). Any valid Gregorian date set via
`--target-date YYYY-MM-DD` engages the truncation + extension
loop.

`parse_args` validations:

- Value must parse via `core::GameDate::parse` (catches month/day
  out of range, malformed format).
- Requires `--replay` (rejected at parse time otherwise).

The scenario-start precondition is checked in `run()` rather than
`parse_args` because it depends on the loaded `simulation.json`
seed date.

## 3. Implementation

### Log truncation

After `begin_tick` succeeds:

```cpp
const auto* log_ptr = &loaded.applied_commands;
std::vector<core::AppliedPlayerCommand> truncated;
if (opts.target_date.has_value()) {
    const auto td = opts.target_date.value();
    for (const auto& e : loaded.applied_commands) {
        if (e.applied_on > td) break;
        truncated.push_back(e);
    }
    log_ptr = &truncated;
}
auto replay_r = cmd::replay_with_time(state, opts, ctrl, *log_ptr);
```

The break-on-first-past-target is sound because `replay_with_time`
documents and enforces monotonic non-decreasing dates (M2.7).
Once one entry is past the target, every subsequent entry is too.

### Post-replay extension loop

```cpp
if (opts.target_date.has_value()) {
    const auto td = opts.target_date.value();
    while (state.current_date < td) {
        auto step_r = step_one_day(state, opts, ctrl);
        if (!step_r) {
            return failure("--target-date " + td.to_string() +
                           ": step_one_day failed ...");
        }
    }
}
```

`step_one_day` is the same M2.2 primitive `replay_with_time` uses
internally. The M1.10 monthly pipeline therefore fires on every
month boundary the extension crosses, exactly as during a normal
run. CSV snapshot rows on those boundaries are also recorded
because the controller's buffers stay live until `end_tick`.

### Scenario-start precondition

`run()`'s replay branch validates `target_date >= state.current_date`
before any tick happens. The check sits in the M2.9 pre-`end_tick`
window, so a bad target writes no output artefacts (regression test
included).

### Contract update

M2.9's "M2.9 contract" block on `runner::run` gains two bullets:

- The `--target-date` precondition itself.
- The post-replay `step_one_day` loop that advances toward target.

Both are pre-`end_tick` failures and therefore covered by the same
no-artefact guarantee.

## 4. CLI examples

```bash
# Replay only the first week of the source, save at 1930-01-07
leviathan --scenario data/scenarios/1930_minimal.json \
          --replay   src.json \
          --days     0 \
          --target-date 1930-01-07

# Replay every command, then advance until 1931-06-01 so post-run
# inspection sees half a year of dynamics with the log applied
leviathan --scenario data/scenarios/1930_minimal.json \
          --replay   src.json \
          --days     0 \
          --target-date 1931-06-01
```

## 5. Tests

9 new doctest cases (M2.9 closed at 553 → M2.14 lands at 562):

`parse_args`:

- **plumbed** (with `--replay`): `opts.target_date == 1930-06-15`.
- **defaults nullopt** when absent.
- **without a value rejected** with the flag name in the error.
- **malformed date rejected** (`"1930-13-01"`) with the flag name
  and the bad value in the error.
- **without `--replay` rejected** with both flag names in the error.

`run()` end-to-end:

- **`--target-date past the log advances`**: log at 01-05 / 01-10,
  target 01-20 → replays both, then steps from 01-10 to 01-20.
  `end_date == 1930-01-20`; reloaded save's `current_date` matches.
- **`--target-date equal to last entry`**: log at 01-05 / 01-10,
  target 01-10 → replays both, no extra step.
- **`--target-date truncates the log`**: log at 01-05 / 01-10,
  target 01-07 → replays only the 01-05 entry, then steps from
  01-05 to 01-07. Reloaded save's `applied_commands.size() == 1`.
- **`--target-date before scenario start rejected`**: target
  1929-12-31 with the canonical 1930-01-01 scenario fails before
  any tick. The M2.9 no-artefact contract is asserted via
  `check_no_artifacts(paths)`.

The dated-log helper (`build_source_with_dated_log`) hand-splices
`AppliedPlayerCommand` entries into a save built from an empty
`apply_pending` run, so the test can stage entries at arbitrary
monotonic dates that `apply_pending` itself would never produce
without running `replay_with_time`.

## 6. What's NOT in scope

Deliberate non-goals:

- **No `--target-date` outside `--replay`**. A future PR may
  generalise the flag to work as a `--days`-by-end-date variant
  on non-replay runs; M2.14 doesn't.
- **No interaction with `--days` in replay mode**. `--days` is
  still required by `parse_args` (M2.8 behaviour) and still
  ignored in the replay path.
- **No mid-day target**. Sub-day resolution remains out of scope
  per the M0.2 calendar contract.
- **No `--target-date` for `--verify`**. The flag and `--verify`
  can be combined, but the comparison still happens against the
  loaded source save's final state. With a target before the
  source's last entry, mismatches are expected and reported as
  usual; the user interprets them. No special-casing inside
  `compare_states`.
- **No save format change**. Schema stays v9; `target_date` is
  CLI-only.
- **No new gameplay**.
- **No new `state.logs` entry**.
- **No M1 system change**.

## 7. Cross-links

- M2.7 (`m2-7-replay-with-time.md`) — `replay_with_time` is the
  primitive M2.14 truncates and follows.
- M2.8 (`m2-8-replay-cli.md`) — `--replay` is the flag M2.14
  scopes.
- M2.9 (`m2-9-replay-cli-error-paths.md`) — pre-`end_tick`
  no-artefact contract; M2.14's failures slot into it.
- M0.2 (`m0-2-calendar-and-types.md`) — `GameDate::parse`
  precondition.
- M0.9 (`m0-9-runner.md`) — overall `parse_args` / `run` /
  `main()` shape.

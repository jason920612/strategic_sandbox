# M2.8 - Replay CLI harness

Companion notes for `feature/m2-08-replay-cli`. M2.8 wires the M2.7
`replay_with_time` primitive into the runner as a `--replay PATH`
flag. The user runs:

```
leviathan --scenario foo --replay source.json --output replay_out
```

and the runner builds a fresh state from the scenario, replays the
loaded save's command log onto it, then writes the result to
`replay_out/save.json` (plus the usual JSONL / CSV artefacts).

The CLI does **not** auto-compare the replayed state against the
source — the user diffs the two save files themselves. That keeps
the harness small and avoids over-specifying what "match" means.

## 1. Scope

M2.7 shipped the library primitive (`commands::replay_with_time`).
M2.8 is the user-facing surface: a CLI flag, runner dispatch, and
stdout summary line. That's it.

No save format change. No new command kinds. No new lifecycle log
line. No automatic per-field state diff. No replay outside `run()`.

## 2. Public API

`include/leviathan/systems/runner.hpp`:

```cpp
struct RunnerOptions {
    // ... existing flags ...
    std::optional<std::filesystem::path> replay_path;   // M2.8
};

struct RunOutcome {
    // ... existing fields ...
    int replay_commands_replayed = 0;   // M2.8 (zero when --replay not set)
};
```

`parse_args` recognises `--replay PATH`. `usage_text()` gains a
matching entry.

## 3. `run()` dispatch flow

```cpp
core::Result<RunOutcome> run(const RunnerOptions& opts) {
    // ... config load, state build, optional scenario load ...

    if (opts.replay_path.has_value()) {
        // Replay branch:
        // - state.countries must be non-empty (--scenario required).
        // - Load the save at --replay PATH.
        // - If --player not set, inherit player_country from the
        //   loaded save.
        // - begin_tick -> replay_with_time -> end_tick.
        // - Populate outcome.replay_commands_replayed.
        // - Return.
    }

    return run_state(state, opts);   // normal headless run
}
```

Three guard rails worth calling out:

1. **`--scenario` is required.** Replay needs a fresh state baseline
   (countries / factions / policies). Replay does NOT reuse the
   loaded save's countries — that would defeat the "fresh state"
   invariant the M2.6 / M2.7 primitives depend on.
2. **`player_country` auto-inherits** from the loaded save when
   `--player` is absent. This is the most common usage (replay a
   save without re-specifying the player) and avoids a wrong-actor
   footgun. `begin_tick`'s existing `--player` resolution only fires
   when `opts.player_id_code.has_value()`, so the inherited value
   survives.
3. **Output paths follow normal rules.** `out/save.json`,
   `out/events.jsonl`, `out/summary.csv`, etc. are written exactly
   as for a non-replay run. The user diffs them against the source
   save.

## 4. Stdout summary

`main()` prints two extra lines when `--replay` is set:

```
Replay source       : source.json
Commands replayed   : 2
```

Inserted near the existing CSV path lines for visual consistency
with the rest of the summary block.

## 5. CLI examples

```bash
# Build a source save by running the canonical scenario for a year:
# (the runner doesn't auto-submit player commands; for now you'd
# build a source save in test code or via a future M2.x command-
# entry CLI. The flow below uses an externally-prepared source.json.)

# Replay the source's command log onto a fresh world:
./build/bin/Debug/leviathan \
    --config   data/config/simulation.json \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/source.json \
    --output   replay_out/

# The replayed save lands at replay_out/save.json. Diff it against
# source.json to verify equivalence:
diff source.json replay_out/save.json
```

`--days` is still required by parse_args (it's a hard mandatory
flag from M0.9); the replay flow ignores it (replay drives
advancement from the log, not from `opts.days`).

## 6. Failure cases

- **`--replay` without `--scenario`** → rejected with the offending
  flag in the error.
- **`--replay PATH` where PATH doesn't exist / fails to parse** →
  the underlying `save_system::load` error propagates, prefixed by
  `--replay: `.
- **Replay-with-time errors** (out-of-order log, unknown policy,
  unknown budget category, etc.) propagate verbatim from
  `replay_with_time`.

In all failure paths, no save / log / CSV file is written —
`end_tick` is only reached on a successful replay.

## 7. Tests

7 new doctest cases (M2.7 was 513 → M2.8 is 520):

- **`parse_args: --replay plumbed`**: `--replay PATH` populates
  `opts.replay_path`.
- **`parse_args: --replay missing value`** rejected with `--replay`
  in the error.
- **`parse_args: --replay defaults unset`** when absent.
- **`run: --replay without --scenario`** rejected with both flag
  names in the error.
- **`run: --replay with single EnactPolicy reproduces the source`**:
  test fixture builds a source save by loading the canonical
  scenario, setting GER as player, submitting `raise_taxes` via
  `commands::apply_pending`, and writing to disk. Then `rn::run`
  with `--replay` is invoked; the resulting save is loaded back
  and its `applied_commands` + `legal_tax_burden` are compared
  against the source.
- **`run: --replay inherits player_country`**: source has
  `player_country = 0`; `--player` is left unset; the replayed save
  also has `player_country = 0`.
- **`run: --replay of an empty-log save replays zero commands`**:
  outcome's `replay_commands_replayed == 0`.

## 8. What's NOT in scope

Deliberate non-goals:

- **No per-field state comparison.** The CLI writes the replayed
  state to disk; the user runs `diff` to judge equivalence. A
  programmatic comparison API would be a separate sub-milestone
  with its own surface (which fields, what tolerance, output
  format).
- **No `--target-date YYYY-MM-DD` flag.** The replayed state's
  `current_date` ends at the last log entry's `applied_on`,
  inherited from `replay_with_time`. Advancing further would be a
  combined replay-then-step flow worth its own scope.
- **No save format change.** M2.8 only adds a runner flag and a
  RunOutcome field; the on-disk save shape is unchanged at v9.
- **No replay outside `run()`.** The M2.7 library primitive is
  still available for tests / future drivers; M2.8 is specifically
  the CLI wrapper.
- **No new lifecycle log entries.** `state.logs` still gets the
  M0.6 "simulation start" / "simulation end" via begin_tick /
  end_tick — no separate "replay started" entry.
- **No replay against a different scenario.** The user is expected
  to pass the same `--scenario` that produced the source save.
  Validation is the user's responsibility; the runner won't catch
  scenario mismatch on its own.
- **No multi-save replay (chained logs).** One `--replay PATH` at
  a time.

## 9. Cross-links

- RFC-050 §8 — recording was M2.4; M2.8 is the CLI consumer.
- M0.9 (`m0-9-runner.md`) — `run()` / `parse_args` shape M2.8
  extends.
- M2.1 (`m2-1-player-country.md`) — `--player` resolution; M2.8
  auto-inherits when `--player` is absent.
- M2.2 (`m2-2-pause-resume.md`) — `begin_tick` / `end_tick`
  primitives the replay branch uses.
- M2.6 (`m2-6-replay-prototype.md`) — sister library primitive
  `commands::replay`, not used by M2.8 (we always want time
  advancement at the CLI level).
- M2.7 (`m2-7-replay-with-time.md`) — the underlying library
  primitive `commands::replay_with_time`.

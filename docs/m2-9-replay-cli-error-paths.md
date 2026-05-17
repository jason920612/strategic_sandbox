# M2.9 - Replay CLI error-path hardening

Companion notes for `feature/m2-9-replay-error-paths`. M2.9 fills the
gap left by the original M2 sequencing: M2.8 introduced `--replay`,
M2.10–M2.13 built the verify/compare/tolerance toolchain on top of
it, but the failure-path artifact semantics of `--replay` itself were
never spelled out or regression-tested. This PR is the cleanup.

No library behaviour changes. Three regression tests, one doc comment
on `runner::run`, one design note. The runner already had the
guarantee the tests pin down — M2.9 just makes future refactors prove
it still holds.

## 1. Scope

When `--replay PATH` is set, the runner can fail for several reasons
the user can't control from outside:

- The source save at `PATH` is missing or corrupt
  (`save_system::load` failure).
- `--scenario` was not provided, so `state.countries` is empty.
- The replayed log is out of order (`replay_with_time` monotonicity
  check).
- The replayed log references an unknown policy `id_code`, or an
  `AdjustBudget` with an unknown category / non-finite delta.
- The monthly pipeline fails advancing between commands.
- `begin_tick` / `end_tick` reject their preconditions.

In every one of those cases, M2.9 cements the contract: NO output
artefacts (save.json / events.jsonl / summary.csv / countries.csv /
factions.csv) survive. The caller's `output_dir` is left exactly as
it was before `run()` was invoked. Retry doesn't need a clean.

This holds end-to-end because `end_tick` is the only function on the
runner side that touches disk, and every replay-failure path returns
before `end_tick` is reached.

## 2. Public API

No changes to types, options, or function signatures.

`runner::run`'s doc comment gains an explicit "M2.9 contract" block
listing the failure conditions and stating the no-artefact guarantee.
The contract covers the non-replay path too — same reason.

## 3. Implementation

Nothing to implement. The three pieces that already give us the
guarantee:

1. **Source load failure short-circuits before any state mutation.**
   In `run()`'s replay branch, `ss::load(opts.replay_path)` runs
   before `begin_tick`. A failed load returns directly.

2. **`begin_tick` writes no files.** It mutates `state.logs`
   ("simulation start" entry) and may push initial CSV snapshot rows
   into `TickController` buffers, but the buffers are never flushed
   until `end_tick`.

3. **`replay_with_time` and `step_one_day` write no files.** Same
   reasoning. The CSV-row buffers live on the controller.

So a failure from `ss::load`, the `state.countries.empty()` guard,
`begin_tick`, or `replay_with_time` returns before `end_tick`, and
zero files are written. The non-replay `run_state` path obeys the
same shape: every per-day failure returns from inside the loop, and
file writes only happen in `end_tick`.

## 4. Tests

3 new doctest cases (all under `LEVIATHAN_TEST_DATA_DIR` so they can
load the canonical scenario):

- **`--replay with a missing source file fails and writes no
  artifacts`** — point `replay_path` at a path that doesn't exist.
  `run()` fails with "--replay" in the error; none of the five
  artefact paths exist afterward.
- **`--replay with an out-of-order log fails and writes no
  artifacts`** — build an empty-log source via `build_source_save`,
  hand-splice two `AppliedPlayerCommand`s with dates `1930-01-05`
  then `1930-01-03`, save back. `run()` fails with "out-of-order"
  in the error; no artefacts on disk.
- **`--replay with an unknown policy id_code fails and writes no
  artifacts`** — same shape, single entry with `policy_id_code =
  "no_such_policy_id_code"`. `run()` fails with that string in the
  error; no artefacts on disk.

Each test goes through a shared `wire_all_artifacts(opts, dir)`
helper that points all five output paths inside a `TempDir` to
distinct names, and a `check_no_artifacts(paths)` helper that
asserts each path does not exist after the failed run.

Out-of-order and unknown-policy logs are constructed by writing
directly into `state.applied_commands` and re-saving, bypassing
`apply_pending`. That mirrors the on-disk-only failure modes the
contract covers: a malformed save (built by a different binary, an
older scenario, or a manual edit) can hold those entries even
though the in-memory submission path would reject them.

doctest count: M2.13 closed at 550; M2.9 adds 3, landing at 553.

## 5. What's NOT in scope

Deliberate non-goals:

- **No new gameplay**. No new `PlayerCommandKind`, no new policy
  effect, no new simulation system.
- **No save format change**. Schema stays v9.
- **No new `state.logs` entry on failure**. The runner's existing
  error-return shape is the only signal; we don't sprinkle log
  writes on the failure paths.
- **No retry / rollback machinery in the runner**. The guarantee is
  "nothing was written", not "previous artefacts are restored".
  Users who want history keep their own backups.
- **No M1 system change**. The monthly-pipeline failure path is
  reachable via `step_one_day` inside `replay_with_time`, but M2.9
  doesn't touch its internals; it only documents the runner's
  observable behaviour when one of those failures bubbles up.
- **No partial-progress reporting on failure**. `RunOutcome` is
  populated only on success; failure returns `Result<...>::failure`
  carrying a string. Tests prove the artefacts aren't there; we do
  not also need to report "0 commands replayed" or similar.

## 6. Cross-links

- M2.8 (`m2-8-replay-cli.md`) — introduced `--replay`; this PR fills
  in the failure-path artefact semantics it left implicit.
- M2.7 (`m2-7-replay-with-time.md`) — `replay_with_time` is the
  function whose failures we test for here.
- M2.4 (`m2-4-command-log.md`) — the `applied_commands` log that
  replay reads from.
- M0.8 (`m0-8-save-load.md`) — `save_system::load` failure path.
- M0.9 (`m0-9-runner.md`) — original runner layout; `end_tick` is
  still the only artefact-writing function.

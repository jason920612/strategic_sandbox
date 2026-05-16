# M0.9 - Headless simulation runner

Companion notes for `feature/m0-09-runner`. Locks in the CLI surface,
the two-layer split between argument parsing and execution, and the
determinism property the whole M0 milestone has been working towards.

## 1. CLI surface

```
Usage: leviathan [options]

Options:
  --config PATH    Simulation config JSON to load.
                   Defaults to data/config/simulation.json.
  --days N         How many days to advance. REQUIRED. N >= 0.
  --seed N         Override the seed from the config. uint64.
  --output DIR     Directory for output artefacts. Created if
                   missing. Defaults to out/.
  --save PATH      Save file path. Defaults to <output>/save.json.
  --log PATH       JSONL log file path. Defaults to
                   <output>/events.jsonl.
  --help           Show this help and exit.
```

Defaults are chosen so the smallest useful invocation is
`leviathan --days 30`. Reviewers and AI agents who need exact
reproducibility can pin every path; tests do.

`--days` is the only required flag. Running zero days is permitted
(produces a save with `start_date == end_date`), but the runner
refuses to run without an explicit `--days` because invoking the
binary with no flags is almost always a mistake.

## 2. Two-layer split

```cpp
core::Result<RunnerOptions> parse_args(int argc, const char* const* argv);
core::Result<RunOutcome>    run(const RunnerOptions& opts);
```

- **`parse_args`** is pure: it reads `argv`, builds a `RunnerOptions`,
  and returns. No file I/O, no dependence on the filesystem. Tests
  construct fake `argv` arrays and assert on the parse result without
  ever touching the disk.
- **`run`** is the orchestrator: it loads the config, builds the
  state, ticks, and writes the save / log files. Tests run it
  against a per-case `TempDir` and inspect what it wrote.
- **`main()`** is the thin shell that glues both together and prints
  the stdout summary.

This split lets us unit-test argument validation cheaply (no temp
files, no filesystem mocking) and test the side-effecting `run`
separately when we need to assert on file contents.

## 3. What the runner logs

To keep long-run logs manageable, the runner is **selective** about
what it writes to `state.logs`:

- `info "simulation start"` with metadata `{days_requested, seed}`
- For each tick: `info "month rolled over"` and/or
  `info "year rolled over"` only when `TickResult` flags them
- `info "simulation end"` with metadata `{days_advanced}`

There is no per-day log. A 365-day run produces 15 entries
(start + end + 12 month rollovers + 1 year rollover). If verbose
per-day logging is ever wanted, a `--verbose` flag can be added later.

### Determinism note

The metadata above is **path-independent**. The runner intentionally
does NOT include the config / output / save / log file paths in its
log entries, because different runs use different paths (especially
in tests using temp dirs), and including them would break the
byte-identical-output invariant.

## 4. Determinism property

Two runs with the same `RunnerOptions` and the same config file
produce **byte-identical** `save.json` and `events.jsonl`. This is
the first end-to-end realisation of the determinism promise from
RFC-000 §5 rule 10.

It is verified by `run: same seed produces byte-identical save and
log files` in `tests/systems/runner_test.cpp`. The test runs the
same options into two separate temp dirs and `==`-compares the
output files as raw bytes.

What makes this hold:

- **GameDate arithmetic** is deterministic (M0.2 / M0.4).
- **The RNG is counter-indexed splitmix64** with no `<random>` or
  `<random_device>` calls (M0.5). The runner doesn't actually draw
  in M0.9 yet, but the property holds for when it does.
- **JSONL exporter** uses fixed field order and insertion-order
  metadata (M0.6).
- **Save format** uses `ordered_json` so key order is stable (M0.8).
- **Runner logs** don't reference paths.

If any future system breaks determinism (e.g. introduces a draw from
`std::random_device`, sorts metadata alphabetically, includes a
wall-clock timestamp in a log), the determinism test fails loudly.

## 5. Exit codes

- `0`  on success.
- `1`  on any failure (argument error, config load error, file write
  error). The error message is printed to `stderr`; the usage text
  follows for argument errors.

## 6. What's NOT in scope

- **No multi-config / batch mode.** One config, one run.
- **No `--verbose` per-day logging.** Easy to add later; not needed
  by the current acceptance tests.
- **No JSON output for the summary.** Stdout summary is human-readable
  text. Programmatic consumers should read the save / log files
  directly.
- **No load-and-continue.** The runner always builds a fresh
  `GameState` from the config. Resuming from a save is a separate
  feature that will compose `save_system::load` with the runner; not
  in M0.9.
- **No countries / entities loaded by default.** The runner ticks an
  empty world. M0.7's `load_country` exists but isn't wired in here;
  doing so needs a separate flag (`--countries DIR`?) and a decision
  about ID assignment which is out of scope for M0.9.
- **No timeout / wall-clock limit.** A multi-decade run isn't expected
  to take more than a second or two at the M0 simulation depth, so
  there's nothing to cap.

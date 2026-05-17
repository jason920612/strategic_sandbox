# M2.2 - Pause / resume / step primitives

Companion notes for `feature/m2-02-pause-resume`. M2.2 exposes the
day-at-a-time seam that an outer driver (interactive REPL, future
M2.3 command queue, scripted automation) needs to pause the
simulation, do work, then resume. The existing `runner::run_state`
entry point is unchanged in behaviour; it now composes the new
primitives internally.

## 1. Scope

`run_state(state, opts)` previously ran the entire `for (i < days)`
loop in one C++ stack frame with the snapshot buffers + monthly_ticks
counter held as local variables. There was no way to "step one day,
inspect, mutate, step another day" — the only seam was call-the-whole-
thing or don't.

M2.2 extracts three primitives:

```cpp
runner::TickController ctrl;
runner::begin_tick(state, opts, ctrl);             // capture start
for (int i = 0; i < opts.days; ++i) {
    runner::step_one_day(state, opts, ctrl);       // one day at a time
}
runner::end_tick(state, opts, ctrl);               // finalise + write
```

`run_state` is rewritten as a thin composition over these three. By
construction, every byte the original loop wrote (logs, snapshots,
save, JSONL, CSVs) is now written in exactly the same order, so
**M1.17's 5-artefact byte-identical determinism contract still
holds**. The 365-day integration test and the 10-year soak test
still pass unchanged.

That's the whole PR. M1 systems are not touched. No new save schema.
No new CLI flag. No new logs. No M1 behaviour branches on
`player_country` (the M2.1 contract is unchanged).

## 2. Public API — `include/leviathan/systems/runner.hpp`

### `TickController`

```cpp
struct TickController {
    core::GameDate start_date{};
    int monthly_ticks = 0;
    int days_stepped  = 0;
    std::vector<diagnostics::SummaryRow>        summary_rows;
    std::vector<diagnostics::CountrySummaryRow> country_rows;
    std::vector<diagnostics::FactionSummaryRow> faction_rows;
    bool started = false;
    bool ended   = false;
};
```

Runtime / orchestration state held between tick steps. Lives **outside**
`GameState`: a controller is never saved or loaded. Fields are public
for inspection by tests and future drivers. Mutating fields between
`begin_tick` and `end_tick` is unsupported.

### Free functions

```cpp
Result<bool>       begin_tick   (GameState&, const RunnerOptions&, TickController&);
Result<bool>       step_one_day (GameState&, const RunnerOptions&, TickController&);
Result<RunOutcome> end_tick     (GameState&, const RunnerOptions&, TickController&);
```

Lifecycle invariants:

| Call | Pre-condition | Effect on controller |
|---|---|---|
| `begin_tick`  | `!started && !ended` | resolve `--player`, capture `start_date`, emit start log + initial snapshot row(s), set `started=true` |
| `step_one_day`| `started && !ended`  | advance one day, emit month/year logs as appropriate, run monthly pipeline on month boundaries, emit per-month snapshot row(s), increment `days_stepped` and (when applicable) `monthly_ticks` |
| `end_tick`    | `started && !ended`  | emit end log, run sanity_check, emit issue logs + final snapshot row(s), resolve output paths, write save / JSONL / CSV files, return populated `RunOutcome`, set `ended=true` |

Violations return `Result::failure` with a specific message
(`"already started"` / `"not been started"` / `"already ended"`).

### `run_state` post-refactor

```cpp
Result<RunOutcome> run_state(GameState& state, const RunnerOptions& opts) {
    if (opts.days < 0) return Result::failure("--days must be >= 0");
    TickController ctrl;
    if (auto b = begin_tick(state, opts, ctrl); !b) return failure(b);
    for (int i = 0; i < opts.days; ++i) {
        if (auto s = step_one_day(state, opts, ctrl); !s) return failure(s);
    }
    return end_tick(state, opts, ctrl);
}
```

Externally visible behaviour: identical. The `--days N` headless
runner still writes the same bytes.

## 3. What did NOT move

A few things stay in `run()` (the outer entry point) rather than
`begin_tick`:

- **Config load** (`load_simulation_config`) and **seed override**.
  These produce the initial `GameState`; the controller operates on
  an already-built state.
- **Scenario load** (`scenario_loader::load_into_state`). Tests that
  drive the primitives directly must load the scenario themselves
  (the equivalence test does exactly this; see
  `tests/systems/runner_test.cpp`). This matches the long-standing
  `run_state(state, opts)` contract where the caller is responsible
  for populating countries / factions / policies before calling.

So `run()` is still:

```cpp
Result<RunOutcome> run(const RunnerOptions& opts) {
    auto cfg = load_simulation_config(opts.config_path);
    apply_seed_override(cfg, opts);
    auto state = make_game_state(cfg);
    if (opts.scenario_path) load_into_state(state, *opts.scenario_path);
    return run_state(state, opts);
}
```

## 4. Determinism

M1.17 pinned 5 byte-identical artefacts (save, events, summary CSV,
countries CSV, factions CSV). Two new equivalence tests in M2.2 pin
the refactor:

- **`begin/step*N/end matches run_state byte-for-byte`** — call the
  new primitives directly with the canonical scenario and compare
  every artefact against `run_state(days=N)`.
- **`pause-then-resume produces byte-identical output`** — drive
  15 days, "pause", drive 16 more, compare against
  `run_state(days=31)`. This is the substantive guarantee: a future
  interactive driver can stop in the middle of a run, do work
  (later: process player commands), and continue without changing
  the deterministic output.

## 5. Tests

10 new doctest cases (M2.1 was 452 → M2.2 is 462).

`tests/systems/runner_test.cpp`:

- `begin_tick` double-begin rejected with `"already started"`.
- `step_one_day` before `begin_tick` rejected with `"not been started"`.
- `end_tick` before `begin_tick` rejected with `"not been started"`.
- `step_one_day` after `end_tick` rejected with `"already ended"`.
- `end_tick` after `end_tick` rejected with `"already ended"`.
- Controller counters after begin/step×31/end: `start_date == 1930-01-01`,
  `days_stepped == 31`, `monthly_ticks == 1`, `started == true`,
  `ended == true` after end.
- Equivalence: `begin/step*31/end` produces byte-identical save +
  events + all 3 CSVs as `run_state(days=31)`.
- Pause/resume: `begin/step*15/step*16/end` produces byte-identical
  save + events + summary CSV as `run_state(days=31)`.

Plus **2 drive-by regression tests** addressing the PR #29 reviewer
nit:

- `run` with `--player GER` on an empty world → `save.json` and
  `events.jsonl` do not exist on disk.
- `run` with `--player BOGUS` against a loaded scenario → `save.json`
  and `events.jsonl` do not exist on disk.

These prove the M2.1 "fail before any artefact" claim is held by the
filesystem, not just by the error message.

## 6. What's NOT in scope

Deliberate non-goals:

- **No save format change.** Schema stays at v8 (M2.1).
  `TickController` is runtime-only.
- **No new CLI flag.** Neither `--step` mode nor an interactive REPL
  is shipped. The primitives exist; the consumer doesn't yet.
- **No new lifecycle log.** `begin_tick` / `step_one_day` /
  `end_tick` emit the same M0.9 / M1.10 logs as the old loop, in the
  same order.
- **No M1 system change.** `faction::react`, `stability::tick`,
  `economy::tick`, monthly pipeline, diagnostics, save / load, scenario
  loader — all unchanged.
- **No M1 system reads `player_country`.** Still data-only, same as
  M2.1.
- **No mid-run state save / load resume.** Resume in M2.2 means
  "within a single process", not "round-trip through disk in the
  middle". A future sub-milestone can extend save/load to carry the
  controller's day_stepped / monthly_ticks if needed.
- **No command queue.** M2.3 territory. The primitives exist so a
  command queue can interleave commands between `step_one_day`
  calls, but the queue itself is not in this PR.

## 7. Cross-links

- RFC-090 §M2 — Milestone 2 roadmap; M2.2 is the second M2 sub-
  milestone after M2.1 (player country selection).
- M0.9 (`m0-9-runner.md`) — original `run()` / `parse_args` split.
- M1.10 (`m1-10-runner-monthly-wiring.md`) — introduced the
  `run_state(state, opts)` entry point and the monthly_pipeline
  invocation on month boundaries; M2.2's `step_one_day` preserves
  that wiring.
- M1.14 / M1.16 — per-country / per-faction snapshot cadence (start
  + month_changed + final post-sanity). M2.2 reuses the same
  cadence inside `begin_tick` / `step_one_day` / `end_tick`.
- M1.17 (`milestone-1-result.md`) — pinned the 5-artefact byte-
  identical determinism contract that this refactor preserves.
- M2.1 (`m2-1-player-country.md`) — `--player` resolution moved from
  `run_state` into `begin_tick` (same code, different home).

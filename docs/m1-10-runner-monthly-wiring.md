# M1.10 - Runner monthly pipeline wiring

Companion notes for `feature/m1-10-runner-monthly-wiring`. M1.10
connects the M1.9 `monthly::tick_all_countries` pipeline to the M0.9
headless runner's daily loop so a month boundary actually triggers
the four-country-side-system chain. No new gameplay; no new state
fields; no save-schema change.

## 1. Scope

M1.10's job is to make this happen, every time the runner crosses a
month boundary:

```cpp
const lt::TickResult r = lt::advance_one_day(state);
if (r.month_changed) {
    lg::log_info(state, "time", "runner", "month rolled over");  // M0.9
    auto pipe = mp::tick_all_countries(state);                   // M1.10 NEW
    if (!pipe) return failure(...);                              //   fail fast
    ++monthly_ticks;                                             //   counter
}
```

That's it. Everything else is reorganising the existing code so
tests can call into it with a pre-built `GameState`.

## 2. API changes

### `RunOutcome` gains one field

```cpp
struct RunOutcome {
    // ... existing fields unchanged ...
    int monthly_ticks = 0;   // M1.10: month boundaries crossed
};
```

### `run_state` is split out from `run`

```cpp
// Existing public entry point. Unchanged behaviour for empty-state
// callers; internally now delegates to run_state after building the
// state.
core::Result<RunOutcome> run(const RunnerOptions& opts);

// New public entry point. Operates on a pre-built GameState. Used
// by tests that need to inject countries / factions BEFORE the tick
// loop runs.
core::Result<RunOutcome> run_state(core::GameState& state,
                                   const RunnerOptions& opts);
```

`run()` is now `load config -> make_game_state -> run_state`.
`run_state` owns the tick loop, monthly pipeline invocation,
sanity-check pass, file writes, and outcome construction.

Note: `run_state` reads `state.rng.seed` for the "simulation start"
log metadata, where the previous `run()` body used `cfg.seed`. Both
values are identical by construction (`make_game_state` copies seed
from config), so the log output is unchanged for the existing
`run()` callpath.

## 3. Month-boundary behaviour

- The monthly pipeline is invoked **AFTER** the "month rolled over"
  log line, so the M0.9 log ordering is preserved.
- The pipeline writes no logs itself. The summary CSV does NOT gain
  a new column. The save file does NOT gain new persistent state.
- `monthly_ticks` is the count of month boundaries crossed, not the
  count of countries ticked.

### What happens with an empty `state.countries`

`monthly::tick_all_countries` returns success with
`countries_processed = 0` and an empty `countries` vector. The
runner's `monthly_ticks` counter still increments — the boundary
*was* crossed, the pipeline *did* run.

That matches the M0.9 contract: the headless runner can still be
launched with no fixtures and produces deterministic outputs.

## 4. Why the runner still doesn't load country files

The reviewer's M1.10 sign-off was explicit: only the month-boundary
hook. Loading countries from `data/countries/*.json` is a separate
sub-milestone (scenario loader) with its own design questions
(how to declare faction <-> country links, how to keep
`save_version` stable, what to do when a scenario references a
missing fixture). M1.10 stays narrow.

In practice this means:
- `leviathan --days 365` still ticks an empty world. The monthly
  pipeline runs 12 times and processes 0 countries each time.
- Tests that want to observe pipeline mutations call `run_state`
  with a hand-built state.

## 5. Determinism preserved

The byte-identical determinism property carries forward:

- **Empty state**: pipeline is a no-op, so save / log / summary-CSV
  remain byte-identical between same-seed runs. Pinned by the
  `"empty state runner is unchanged by M1.10 wiring"` test.
- **Non-empty state**: pipeline mutations are deterministic
  (no RNG use in M1.6 / M1.7 / M1.8 / M1.9). Same starting state +
  same `--days` + same `--seed` -> same save / log. Pinned by
  `"12 monthly ticks across 1930 with countries are byte-identical"`.

The save format version stays at **v5**. No new persistent fields
are introduced.

## 6. State touched (at the runner layer)

The runner does not write any new fields directly. It calls into
existing M1.9 entry points; mutations flow through the documented
M1.6 / M1.7 / M1.8 systems.

- **WRITES** at the runner layer: `state.current_date` (via
  TimeSystem), `state.logs` (via LoggingSystem - unchanged log
  pattern from M0.9).
- **READS** at the runner layer: `state.rng.seed` (for the start
  log metadata), `state.countries` / `state.factions` (implicit -
  the monthly pipeline iterates `state.countries`).
- **DOES NOT WRITE** save state, RNG counter, or anything via
  the monthly pipeline that isn't already documented in M1.6 / M1.7
  / M1.8 / M1.9.

## 7. Test coverage (9 new cases)

**Boundary count (3)**: `monthly_ticks` equals 0 for 10 days,
1 for 31 days, 12 for 365 days starting 1930-01-01. The 365-day
case also pins the existing `log_count == 15` to prove no new logs
were added.

**Empty-state determinism (1)**: two runs with the same options
produce byte-identical save + log.

**Save schema (1)**: save.json still contains `"save_version": 5`.
A future bump must update the assertion deliberately.

**`run_state` integration (1)**: build state with 1 country +
1 faction, 31 days. Verify country gdp / stability / tax_revenue /
budget_balance and faction support / loyalty actually changed.
Verify faction.radicalism is unchanged (M1.6 doesn't write to it).
Verify date advanced to 1930-02-01.

**`run_state` determinism (1)**: same hand-built state + same opts
produces byte-identical save + log across two independent
`run_state` invocations on freshly-built states.

**`run_state` edge cases (2)**: days==0 succeeds with
`monthly_ticks == 0`; days<0 is rejected before any mutation.

## 8. What M1.10 deliberately does NOT do

- **No country / faction / policy file loading.** Out of scope.
- **No policy scheduler / active-policy container / duration queue.**
  Policies remain caller-driven via `policy::apply_policy_effects`
  (M1.5).
- **No `last_gdp_growth_rate` field on CountryState.**
- **No save-format bump.** Stays at v5.
- **No new logs from the monthly pipeline.** The pipeline itself
  produces no LogEntry; the runner only logs the canonical M0.9
  "month rolled over" line.
- **No new summary-CSV column.** CSV header stays
  `date,country_count,log_count,seed`.
- **No AI / events / diplomacy / war / coup / civil war / UI / map.**
- **No TimeSystem auto-invocation of the pipeline.** TimeSystem
  stays a pure date-math service; orchestration lives in the runner.
- **No partial-atomicity rollback** on mid-pipeline failure. M1.9
  documents non-atomicity; M1.10 propagates the failure as a
  failure Result with the offending date in the message.
- **No balance rebalancing.**

## 9. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/systems/runner.hpp` | adds `monthly_ticks` field, adds `run_state` declaration; small docstring updates |
| `src/leviathan/systems/runner.cpp` | refactors `run` into a thin wrapper around `run_state`; adds monthly-pipeline call inside the daily loop |
| `tests/systems/runner_test.cpp` | adds 9 cases under the M1.10 banner |
| `docs/m1-10-runner-monthly-wiring.md` | this file |
| `docs/README.md`, `README.md`, `rfc/README.md` | M1.10 status / progress |

No source-file additions, no CMakeLists changes. Total tests:
**318 -> 327** (+9). Save format version: still **v5**.

## 10. Risks / things to watch

- **`run_state` is now a public surface.** Tests use it; callers
  can compose richer scenarios. The risk is that a future PR
  silently duplicates logic between `run` and `run_state` instead
  of routing through `run_state`. Mitigation: `run` is intentionally
  a 25-line wrapper now.
- **Mid-pipeline failure aborts the run.** A failing
  `economy::tick` in month 3 of a 365-day run produces a failure
  Result. Files written before the failure (none in the current
  layout — writes happen at the end) are unaffected, but state
  mutations from earlier sub-systems / earlier months stay in
  place. M1.9 documented this as a deliberate non-atomic choice.
- **`monthly_ticks` is a count, not a status enum.** It does not
  distinguish "ran but failed", "ran with 0 countries", "ran with
  N countries". M1.x can promote it if a richer signal is needed.
- **Determinism property depends on no RNG use anywhere in the
  monthly pipeline.** As of M1.10 the sub-systems satisfy that.
  A future PR that introduces RNG-using behaviour into any of
  faction / stability / economy must re-verify the byte-identical
  tests.

## 11. Next sub-milestone

Likely candidates (RFC-090 §M1, post-M1.10):

- **M1.11 - economy -> stability coupling.** Add
  `last_gdp_growth_rate` to `CountryState` (save format `v5 -> v6`
  per the [[feedback_save_version]] rule) and feed it into the
  RFC-080 §5 `EconomicGrowth` term in `stability::tick`.
- **M1.12 - scenario loader.** Read `data/countries/*.json` and
  `data/factions/*.json` from the runner, wiring `country_id_code`
  to numeric `CountryId` and populating `state.factions` from the
  on-disk fixtures. First time the headless binary produces real
  per-country state changes end-to-end.

Per the M1 pacing rule: do **not** start M1.11 (or M1.12) until
M1.10 is reviewed and merged.

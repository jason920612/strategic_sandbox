# Project Leviathan

一款以 1930–2000 全球政治沙盒為背景的即時暫停制國家領導人模擬遊戲。玩家不直接微操軍隊，而是透過法律、行政命令、預算、任命、外交與戰略目標影響國家。國內派系、官僚、軍方、情報部門、地方利益集團、技術菁英、媒體、學生、宗教、工會、農民等會依照自身利益自動反應，讓世界局勢自然演化、混沌且不可完全預測。

> Code name: **Project Leviathan**. Final title TBD — see `rfc/RFC-000-overview.md`.

## Status

- Phase: **Milestone 2 — player-operation prototype (in progress).**
  M1 single-country internal-politics prototype is closed.
- Latest shipped sub-milestone: **M2.9 — Replay CLI error-path
  hardening.** Documentation + regression-test PR with zero library
  behaviour change. `runner::run`'s doc comment gains an explicit
  "M2.9 contract" block stating that `--replay`-mode failures
  (missing/corrupt source save, missing `--scenario`, out-of-order
  log, unknown policy id_code, bad `AdjustBudget` payload, monthly
  pipeline failure, `begin_tick` / `end_tick` rejection) write
  ZERO output artefacts (save.json / events.jsonl / summary.csv /
  countries.csv / factions.csv). 3 new doctest cases pin the
  guarantee end-to-end: missing source file, out-of-order log,
  unknown policy id_code — each wires all five artefact paths and
  asserts none exist after the failed run. No save format bump
  (still v9); no new flag; no M1 system change. Closes the gap M2.8
  left open: --replay shipped but its failure-path artefact
  semantics weren't spelled out.
- Previously shipped: **M2.13 — Verify tolerance CLI** (`--verify-
  tolerance FLOAT` overrides M2.10's default `1e-9` via
  `diagnostics::CompareOptions`; parses through a new exception-
  free `parse_nonneg_double` helper; closes the M2 replay-CLI family
  `--replay` / `--verify` / `--verify-strict` / `--verify-tolerance`;
  no save format bump).
- Next sub-milestone candidates: **M2.14** (further
  `PlayerCommandKind` variants — `ChangeTaxBurden`,
  `ToggleMartialLaw`, …; save-neutral additive enum) or
  **M2.15** (relative-tolerance support in `CompareOptions` —
  upgrade M2.10's absolute-only comparison to handle large-
  magnitude fields better; save-neutral).
- M0 closed. M1 closed. See `docs/milestone-0-result.md` for the
  M0 exit report, `docs/milestone-1-result.md` for the M1 exit
  report, and `rfc/RFC-090-roadmap.md` for the full milestone map.

`GameState` is a passive container. Systems shipped in M0:
`leviathan::systems::time` (date advance + boundary detection);
`leviathan::systems::random` (deterministic splitmix64 RNG, no
`<random>`); `leviathan::systems::logging` (explicit-only logging
with byte-stable JSONL); `leviathan::systems::data_loader` (JSON
config + country parsers via nlohmann/json);
`leviathan::systems::save_system` (JSON round-trip with `save_version`
/ `rng_algorithm_version` gates); `leviathan::systems::runner` (CLI
`leviathan --days N [--config ...] [--seed ...] [--output ...]
[--summary-csv ...]`); `leviathan::systems::diagnostics`
(observation-only `snapshot()` + summary CSV + `sanity_check()`).
Two runs with the same options produce byte-identical save, log,
and summary-CSV files. M0 closes with a full end-to-end integration
test (`tests/integration/m0_end_to_end_test.cpp`) that loads three
country JSON files, ticks 365 days, saves, loads back, and verifies
the round-trip.

**Milestone 1** (single-country internal politics prototype,
RFC-090 §M1) is complete; **Milestone 2** (player-operation prototype,
RFC-090 §M2) has begun with M2.1 + M2.2 + M2.3 + M2.4 + M2.5 + M2.6
+ M2.7 + M2.8 + M2.9 + M2.10 + M2.11 + M2.12 + M2.13 merged. Thirty
sub-milestones shipped:
M1.1 CountryState fields; M1.2 FactionState; M1.3 BudgetState
(seven categories, no sum-to-1 enforcement); M1.4 PolicyData +
PolicyEffect; M1.5 PolicySystem `apply_policy_effects` (first real
gameplay effect, atomic via pre-flight); M1.6 FactionSystem `react`
(linear-toward-equilibrium loyalty / support drift); M1.7
StabilitySystem `tick` (first country-side dynamic, stripped-down
RFC-080 §5); M1.8 EconomySystem `tick` (RFC-080 §3 tax revenue,
expenditure = `gdp × sum_budget × 0.20`, stripped-down RFC-080 §4
GDP growth); M1.9 MonthlyPipeline `tick_country` /
`tick_all_countries` (first composition sub-milestone with canonical
order `faction::react → stability::tick → economy::tick`); M1.10
runner monthly pipeline wiring (every `month_changed` invokes
`monthly::tick_all_countries`; `run_state(state, opts)` exposed for
test injection); M1.11 scenario loader (`--scenario PATH` flag +
`scenario_loader::load_into_state` compose the M0.7 / M1.1 / M1.2
/ M1.4 parsers into a manifest-driven loader; `leviathan --days
365 --scenario data/scenarios/1930_minimal.json` produces a
non-empty world end-to-end without test-only injection); M1.12
economy → stability coupling (new `CountryState::last_gdp_growth_rate`
field, `economy::tick` writes it, `stability::tick` reads it as the
RFC-080 §5 `EconomicGrowth` term with `kEconomicGrowthWeight = 2.0`;
monthly pipeline order unchanged, intentional one-month lag;
save format bumped v5 → v6, first M1 save-schema bump); M1.13
scenario starting policies (manifest gains optional
`starting_policies` array of `{policy, actor}` id_code pairs;
loader applies each via `policy::apply_policy_effects` exactly
once at day 0, with the new fixture
`data/scenarios/1930_with_start_policies.json` enacting
`raise_taxes` + `increase_military_budget` on GER); M1.14
Diagnostics surfaces `last_gdp_growth_rate` — new
`CountrySummaryRow` + `country_snapshot` + per-country CSV writers,
plus opt-in `--countries-csv PATH` runner flag emitting 8 columns
per country per snapshot point (existing `--summary-csv` byte-for-
byte unchanged, M0.10 determinism contract preserved); **M1.15
Policy duration tracking — new `ActivePolicy{policy_id_code,
expires_on}` core type and `CountryState::active_policies` vector;
every successful `policy::apply_policy_effects` records one entry
with `expires_on = current_date + duration_days`; pre-flight
failure appends nothing. Save format bumped v6 → v7 (v6 saves
rejected loudly, v7 country without `active_policies` rejected).
Tracking-only: no expiration sweep, no revert, no scheduler.
`apply_policy_effects` also enforces a runtime cap on
`duration_days` (`kMaxTrackedPolicyDurationDays = 36500`, ~100
years) and rejects negatives, because `GameDate::advance_days` is
a per-day loop; **M1.16 Faction-level diagnostics CSV — new
`FactionSummaryRow` + `faction_snapshot` + per-faction CSV
writers, plus opt-in `--factions-csv PATH` runner flag emitting
9 columns per faction per snapshot point. Existing summary CSV
and per-country CSV both byte-for-byte unchanged (M0.10 + M1.14
contracts preserved). No save-format bump (still v7); **M1.17 M1
exit / integration tests — new
`tests/integration/m1_end_to_end_test.cpp` (1-year scenario run +
10-year soak run + 5-artefact byte-identical determinism), new
`docs/milestone-1-result.md` exit report, drive-by `main()`
milestone-label cleanup. No new system, no new flag, no save
schema change. M1 closes here; **M2.1 Player country selection —
new `GameState::player_country` (`CountryId`, default invalid),
new `--player COUNTRY_IDCODE` runner flag resolved after scenario
load. Save format bumped v7 → v8 (`"player_country"` required at
root; v7 rejected loudly). M1 systems unchanged: no behaviour
branches on `player_country` yet. Opens RFC-090 §M2; **M2.2 Pause
/ resume / step primitives — new `runner::TickController` runtime
struct (lives outside `GameState`) plus three free functions:
`begin_tick` / `step_one_day` / `end_tick`. `run_state` is
rewritten as a thin composition over them; M1.17's 5-artefact
byte-identical determinism contract is preserved (two new
equivalence tests pin it). Misuse rejected. No save format change
(still v8); no new CLI flag; no new logs. Drive-by: 2 regression
tests pinning that bad `--player` writes no on-disk artefacts;
**M2.3 Player command queue — new `core::PlayerCommand` (kind +
`policy_id_code`; M2.3 ships `EnactPolicy`) and new
`systems::commands::{CommandQueue, apply_pending}` module. Queue
is driver-owned (not in `GameState` or `TickController`).
`apply_pending` requires `state.player_country` to index into
`state.countries`, drains in insertion order, dispatches each
`EnactPolicy` through `policy::apply_policy_effects` (reusing
M1.5 atomicity + M1.15 active_policies tracking + duration cap).
Non-atomic across the list; first failure stops with failed cmd
at head. No save format change (still v8); no new flag / log / M1
system change; **M2.4 Player command log — new
`core::AppliedPlayerCommand{applied_on, command}` type and new
`GameState::applied_commands` vector. `commands::apply_pending`
appends one log entry per successful per-command dispatch (after
the M1.5 / M1.15 mutations land), so per-command atomicity covers
the log too; failed commands stay in the queue and do NOT log.
`applied_on` captures `state.current_date` at apply time. **Save
format bumped v8 → v9** with `"applied_commands"` as a required
root-level array; v8 saves rejected loudly; malformed entries
all rejected with `applied_commands[N]` in the error. Foundation
for future deterministic replay (RFC-050 §8). No M1 system
behaviour change; **M2.5 AdjustBudget player command — extends
`PlayerCommandKind` + `PlayerCommand` (new `budget_category` +
`budget_delta` fields). `apply_pending` gains a new switch arm
that validates the 7-category whitelist + finite delta, then
applies `budget.<category> += delta` and clamps to `[0, 1]`.
Per-command atomicity (M2.3) and log-on-success (M2.4) shared
unchanged. Save kind mapping grows to handle `"AdjustBudget"`
with per-kind JSON shape. No save format bump (still v9). Drive-
by: `player_command_kind_to_string` fallback now returns
`"UnknownPlayerCommandKind"` sentinel rather than a real kind
string, addressing the PR #32 reviewer nit; **M2.6 Replay applied
command log prototype — new `systems::commands::replay(state,
log)` free function. For each log entry forces
`state.current_date = applied_on`, builds a 1-element
`CommandQueue`, calls `apply_pending` (reusing M2.3 dispatch +
M2.4 log append + M1.5/M1.15 effect machinery unchanged).
Preconditions: `player_country` valid + `applied_commands`
empty. Atomicity across the log mirrors M2.3 mid-list-failure:
failed entry reported with `replay[N]: ...` in the error,
prior entries stay applied + logged, subsequent entries skipped.
Prototype limits pinned by tests: no time-system advancement
between commands, `current_date` ends at last entry, scenario
must be pre-loaded. No save format change; **M2.7 Replay with
time-system advancement — new
`systems::commands::replay_with_time(state, opts, ctrl, log)`
free function lifting M2.6's "no time advance" limit. For each
log entry, advances via M2.2 `step_one_day` until
`current_date == applied_on` (so M1.10 monthly pipeline runs on
boundaries naturally), then dispatches via 1-element queue +
`apply_pending`. Preconditions add `ctrl.started && !ctrl.ended`
and monotonic non-decreasing dates (addresses PR #34 nit).
Killer equivalence test pins that replay reproduces the original
simulation's state byte-for-byte (current_date, days_stepped,
monthly_ticks, log entries, command effects, and monthly-
pipeline-mutated fields). No save format change; **M2.8 Replay
CLI harness — `--replay PATH` runner flag wires M2.7 into the
CLI. `run()` branches: with `--replay` set (requires
`--scenario`), loads the save at PATH, optionally inherits
`player_country` from the loaded save when `--player` is unset,
runs `begin_tick → replay_with_time → end_tick`, and populates
a new `RunOutcome::replay_commands_replayed` field. `main()`
prints two extra summary lines. The CLI does NOT auto-compare —
user diffs source vs target save files. No save format change;
**M2.9 Replay CLI error-path hardening — doc + tests PR with no
library behaviour change. Cements the contract: a `--replay`
failure from any path (`save_system::load` failure, missing
`--scenario`, out-of-order log, unknown policy id_code,
malformed budget command, monthly pipeline failure, `begin_tick`
/ `end_tick` rejection) writes ZERO output artefacts because
every replay failure returns before `end_tick`, which is the
only function that touches disk. `runner::run`'s doc comment
gains an explicit "M2.9 contract" block; 3 regression tests pin
missing source / out-of-order log / unknown policy id_code with
all five artefact paths wired and checked absent. No save format
change;
**M2.10 State comparison API — new
`systems::diagnostics::compare_states(a, b, opts)` free function
returning a list of `StateMismatch` entries (field_path +
detail). Walks gameplay-relevant fields field-by-field in
canonical order with floating-point tolerance (default 1e-9).
Library-only for now; consumers include replay-equivalence
tests and a future `--verify` CLI flag. Deliberately skips rng,
logs, policies, provinces, events, simulation_config — each
documented with rationale. No save format change; **M2.11
Replay verify CLI — new `--verify` boolean runner flag wires
M2.10 `compare_states` into the M2.8 `--replay` flow. After
`end_tick`, the runner compares the replayed state against the
loaded source save and populates
`RunOutcome::verify_mismatches`. `main()` prints
`Verify mismatches: N` plus one bullet per mismatch.
Informational only (exit code stays 0; strict mode deferred).
Reuses the already-loaded source save. No save format change;
**M2.12 Replay strict mode — new `--verify-strict` boolean
runner flag (requires `--verify`) makes `main()` exit
`EXIT_FAILURE` when M2.11 reports any mismatches. Full mismatch
list still printed first so CI logs capture every divergence.
`run()` semantics unchanged — strict is a `main()`-level exit
policy (library / CLI separation stays clean). Flag-chain
`--verify-strict → --verify → --replay` each with loud rejection
on missing dependency. No save format change; **M2.13 Verify
tolerance CLI — new `--verify-tolerance FLOAT` runner flag
(requires `--verify`) overrides M2.10's default 1e-9 tolerance
when calling `compare_states`. Parses via a new
`parse_nonneg_double` helper that rejects empty / garbage /
non-finite / negative inputs. Plumbed into `run()`'s replay
branch via `diagnostics::CompareOptions`. `main()` prints the
active tolerance when set. Completes the M2 replay-CLI family
(--replay / --verify / --verify-strict / --verify-tolerance).
No save format change.**

## Repository layout

```text
.
├── CMakeLists.txt        Top-level build
├── README.md             This file
├── rfc/                  Design RFCs — read these before changing scope
├── include/leviathan/    Public headers (core/ + systems/)
├── src/                  Simulation core + executable entry point
├── tests/                Unit / integration tests (doctest, per-module)
├── data/                 Game data (JSON): config/simulation.json,
│                         countries/{germany,france,japan}.json (M0.7-M1.3),
│                         factions/ger_*.json (M1.2),
│                         policies/*.json (M1.4),
│                         scenarios/1930_minimal.json (M1.11),
│                         scenarios/1930_with_start_policies.json (M1.13)
├── tools/                Dev / debug tools, currently empty
└── docs/                 Per-milestone design notes (m0-N-*.md) +
                          pr-drafts/ (PR write-ups)
```

## Requirements

- A C++17 compiler:
  - MSVC 19.20+ (Visual Studio 2019 or 2022), or
  - GCC 9+, or
  - Clang 10+
- CMake **3.16 or newer**
- A build tool that CMake can drive (Ninja, Make, MSBuild, Xcode, ...)
- Network access on the **first** configure: the build fetches
  [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 (used by
  the data loader), and the test suite fetches
  [doctest](https://github.com/doctest/doctest) v2.4.11. Both go through
  CMake `FetchContent` (shallow clones, pinned tags). Subsequent
  configures reuse the cached clones in `build/_deps/`.

## Build

From the repo root:

```bash
# Configure (single-config generators)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --config Debug
```

On Windows with Visual Studio, the equivalent is:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

The main executable is produced at `build/bin/leviathan` (`leviathan.exe`
on Windows).

## Run a simulation (M0.9 headless runner)

```bash
# Show usage
./build/bin/Debug/leviathan --help

# 10-day smoke run (everything else falls back to defaults)
./build/bin/Debug/leviathan --days 10

# M1.11 - load the canonical 1930 world so the monthly pipeline
# actually has countries to tick:
./build/bin/Debug/leviathan \
    --days 365 \
    --scenario data/scenarios/1930_minimal.json \
    --seed 12345

# Pinned configuration matching the M0.9 spec example
./build/bin/Debug/leviathan \
    --config data/config/simulation.json \
    --days 365 \
    --seed 12345 \
    --output out/ \
    --save out/save.json \
    --log out/events.jsonl \
    --summary-csv out/summary.csv

# M1.14 - per-country diagnostic CSV: one row per country per snapshot
# point with gdp / stability / last_gdp_growth_rate etc., inspectable
# without round-tripping the save.
./build/bin/Debug/leviathan \
    --days 365 \
    --scenario data/scenarios/1930_minimal.json \
    --countries-csv out/countries.csv

# M1.16 - per-faction diagnostic CSV: one row per faction per snapshot
# point with support / influence / radicalism / loyalty / resources.
# Can be combined with --countries-csv and --summary-csv.
./build/bin/Debug/leviathan \
    --days 365 \
    --scenario data/scenarios/1930_minimal.json \
    --countries-csv out/countries.csv \
    --factions-csv  out/factions.csv

# M1.17 - 10-year soak (RFC-090 §1.17 acceptance criterion).
# Loads the canonical day-0-policies scenario and ticks 3652 days
# (1930-01-01 -> 1940-01-01). 120 monthly pipeline runs.
./build/bin/Debug/leviathan \
    --days     3652 \
    --seed     12345 \
    --scenario data/scenarios/1930_with_start_policies.json

# M2.1 - select a player country. Resolution runs after --scenario
# loads, so --player only makes sense when a scenario is loaded.
# The id_code must match a country in the manifest; bad id_codes
# fail loudly before any tick happens.
./build/bin/Debug/leviathan \
    --days     365 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --player   GER

# M2.8 - replay a previously-saved command log onto a fresh state.
# --replay requires --scenario (for the fresh baseline). When
# --player is unset, player_country is auto-inherited from the
# loaded save. The runner writes the replayed state to save.json
# under --output; diff against the source to verify equivalence.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/source.json \
    --output   replay_out/

# M2.11 - same as above, but auto-compare the replayed state
# against the source via diagnostics::compare_states. Mismatches
# are printed to stdout; exit code stays 0 regardless.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/source.json \
    --verify \
    --output   replay_out/

# M2.12 - same as above but make the process exit EXIT_FAILURE
# on any mismatch. The full mismatch list still prints first so
# CI logs see the divergence; useful as a build/replay regression
# gate.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/golden.json \
    --verify --verify-strict \
    --output   replay_out/

# M2.13 - loosen the verify tolerance for cumulative drift in
# long simulations. Default is 1e-9; pass any finite non-negative
# double via --verify-tolerance to override.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/golden.json \
    --verify --verify-strict --verify-tolerance 1e-6 \
    --output   replay_out/
```

Required flag: `--days`. Everything else has a default
(`--config data/config/simulation.json`, `--output out/`, `--seed`
falls back to the value in the config file, `--save` and `--log` live
under `--output`).

Outputs (paths can be overridden):

| File | Format | Source |
|------|--------|--------|
| `out/save.json`     | Pretty-printed save (M0.8)            | Round-trippable with `save_system::load` |
| `out/events.jsonl`  | One JSON object per line              | M0.6 logging exporter |
| `out/summary.csv`   | Per-snapshot CSV (M0.10, optional)    | `--summary-csv PATH` ; columns: `date,country_count,log_count,seed` |
| stdout              | Plain-text run summary                | start / end dates, log count, sanity-issue count, output paths |

**Determinism**: the same `--config` + `--days` + `--seed` produces
byte-identical `save.json`, `events.jsonl`, **and `summary.csv`**.
Pinned by tests in `tests/systems/runner_test.cpp`
(`byte-identical`, `summary-csv same-seed`).

The runner ticks an empty world (no countries, factions, policies)
in M0.9. Loading countries from a directory will land in a later PR.

## Test

```bash
ctest --test-dir build --output-on-failure
```

For multi-config generators (Visual Studio, Xcode):

```bash
ctest --test-dir build -C Debug --output-on-failure
```

As of M2.9 there are **553 doctest cases**. M0 contributed 179;
M1.1 added 9; M1.2 added 17; M1.3 added 9; M1.4 added 17; M1.5
added 24; M1.6 added 17; M1.7 added 16; M1.8 added 19; M1.9 added
11; M1.10 added 9; M1.11 added 25; M1.12 added 15; M1.13 added 15;
M1.14 added 17; M1.15 added 15; M1.16 added 18; M1.17 added 3
end-to-end integration tests; M2.1 added 17; M2.2 added 10; M2.3
added 8; M2.4 added 13; M2.5 added 11; M2.6 added 9; M2.7 added
10; M2.8 added 7; M2.10 added 12; M2.11 added 5; M2.12 added 5;
M2.13 added 8; M2.9 adds 3 covering the no-artefact contract under
`--replay`: missing source file fails with "--replay" in error and
all five artefact paths absent; out-of-order log fails with
"out-of-order" in error and all five paths absent; unknown policy
id_code fails with `"no_such_policy_id_code"` in error and all
five paths absent. Each test wires save / events / summary CSV /
countries CSV / factions CSV inside a `TempDir` and uses a shared
`wire_all_artifacts` + `check_no_artifacts` helper. Previously
M2.13 added 8 covering `--verify-tolerance`: parse plumbed with
value preserved; default nullopt; missing value rejected;
non-numeric (`"abc"`) rejected with "floating-point" in error;
negative rejected with `">= 0"` in error; without `--verify`
rejected with both flag names; end-to-end: loose tolerance
(`1e-2`) absorbs a `1e-3` `gdp` tweak with no mismatch on that
path; tight tolerance (`1e-6`) catches the same tweak.
Previously M2.12 added 5 covering `--verify-strict`: parse plumbed (with `--verify`),
defaults false, `--verify-strict` without `--verify` rejected
with both flag names; run with strict + matching source succeeds
empty mismatches; run with strict + tweaked source still
succeeds at `run()` level (exit code is `main()`'s concern) but
reports mismatches. Previously M2.11 added 5 covering the
`--verify` CLI flag: parse_args plumbed (with `--replay`),
defaults false, `--verify` without `--replay` rejected with both
flag names in the error; full-replay with matching source
reports zero mismatches; full-replay with manually tweaked
source (`countries[0].legal_tax_burden = 0.99`) detects the
divergence at the documented path. Previously M2.10 added 12
covering the new `compare_states` API: two empty match; identical seeded match;
`current_date` diff with both date strings in detail;
`player_country` diff; country count → `countries.size()`
mismatch; gdp diff on country[0] → `countries[0].gdp` path;
tolerance — within (silent) and outside (reported);
`active_policies` size diff caught with the array path;
`applied_commands` size diff caught; multiple mismatches
collected in canonical order; custom `CompareOptions` tolerance
respected. Previously M2.8 added 7 covering the `--replay` CLI: parse_args plumbed,
missing value rejected, default unset; run --replay without
--scenario rejected; run --replay with single EnactPolicy
reproduces source's `legal_tax_burden` + log; --player auto-
inherits from loaded save when unset; --replay of an empty-log
save replays zero commands. Previously M2.7 added 10 covering
"M2.7: replay_with_time": empty log no-op,
command at start_date (0 advance), command 5 days later (5
advance), command past month boundary (45 days + 1 monthly_tick),
multiple commands at different dates, out-of-order rejected,
ctrl not started rejected, no player_country rejected, non-empty
`applied_commands` rejected, **full equivalence vs original
simulation** (drive source via step + apply, replay onto fresh
target, verify every observable matches including gdp /
stability / last_gdp_growth_rate). Previously M2.6 added 9 in
`tests/systems/commands_test.cpp`: empty log replays cleanly;
single EnactPolicy / single AdjustBudget / mixed-kind log all
replay state + re-log correctly; replayed log mirrors source
byte-equivalent across two dates; `current_date` forced to last
entry's `applied_on` (prototype limit pinned); unknown
policy_id_code mid-list stops with `replay[N]` in the error;
no `player_country` rejects before any replay; non-empty
`applied_commands` rejects the precondition. Previously M2.5
added 11 covering the AdjustBudget kind: 8 commands_test cases (delta mutates target field; negative
delta shrinks; overshoot clamps to 1.0; undershoot clamps to 0.0;
unknown `budget_category` rejected with no mutation; non-finite
`budget_delta` rejected; AdjustBudget log entry carries correct
category + delta; mixed-kind queue applies both in insertion order
with both kinds logged) and 3 save_system_test cases (AdjustBudget
log entry round-trips category + delta; v9 entry missing
`budget_category` rejected; v9 entry missing `budget_delta`
rejected). Previously M2.4 added 13 covering player command log: 5
commands_test cases (successful enact appends one entry; multiple
successes append in insertion order; failed command does NOT log
(per-command atomicity); `applied_on` captures `state.current_date`
at apply time, distinct dates pinned across two `apply_pending`
calls; precondition failure leaves log untouched); 8 save_system
cases (rejects an old v8 save loudly; serialize emits
`"applied_commands": []`; populated round-trip preserves dates +
policy_id_code; v9 missing `applied_commands` rejected; entry
with malformed `applied_on` `"1930-02-30"` rejected; entry with
unknown `kind` `"SomethingBogus"` rejected; entry missing
`policy_id_code` rejected; entry missing `command` sub-object
rejected). Plus an extension of the `game_state_test` baseline
check to assert `applied_commands.empty()`. Save schema is now
v9. Previously M2.3 added 8 in
`tests/systems/commands_test.cpp`: empty queue is a no-op success;
single `EnactPolicy` drains the queue and applies the policy;
successful enact chains into `active_policies` (M1.15 integration:
`expires_on = 1930-03-02` for canonical `raise_taxes` 60-day enact
from `1930-01-01`); multiple commands apply in insertion order;
no `player_country` selected rejected with queue untouched;
`player_country` out-of-range rejected; unknown `policy_id_code`
mid-queue stops at failed cmd (first cmd applied, failed stays at
head, trailing cmd still queued); policy with bad effect target
stops via M1.5 pre-flight rejection. Previously M2.2 added 10
covering pause / resume / step primitives: 6 misuse + counter
cases (`begin_tick` double-begin rejected; `step_one_day` before
begin / after end rejected; `end_tick` before begin / double end
rejected; controller counters `start_date / days_stepped /
monthly_ticks / started / ended` reflect actual lifecycle); 2
equivalence cases (`begin/step×N/end` byte-identical to
`run_state(days=N)` across save + events + all 3 CSVs;
`begin/step×15/step×16/end` byte-identical to
`run_state(days=31)`); 2 drive-by regression cases from the PR
#29 nit (bad `--player` empty-world / unknown id_code both leave
no `save.json` / `events.jsonl` on disk). Previously M2.1 added
17 covering player country selection: 9 save_system cases (rejects an old v7 save loudly,
serialize emits `"player_country": -1`, save+load round-trips
both `-1` and a valid index, v8 missing `player_country` rejected,
non-integer rejected, `< -1` rejected, out-of-range rejected,
above `INT_MAX` rejected); 8 runner cases (`--player` plumbed /
value-missing / default-unset; on empty world rejected with
id_code in message; unknown id_code rejected; `--player GER` →
`player_country: 0`; `--player FRA` → `player_country: 1`; no
`--player` → `player_country: -1`). M1.17 added the 3
end-to-end integration tests in `tests/integration/m1_end_to_end_test.cpp`:
(1) 1-year run that loads the canonical day-0-policies scenario via
the runner, asserts the `active_policies` round-trip (`expires_on
= 1930-03-02` for `raise_taxes`, `1930-01-31` for
`increase_military_budget`), checks `monthly_ticks == 12` and CSV
row counts (1+12+1 snapshot points × N entities); (2) 10-year soak
run pinning RFC-090 §1.17 (3652 days, 120 monthly pipelines, zero
sanity issues, every country's gdp / stability / legitimacy /
last_gdp_growth_rate finite and clamped); (3) 5-artefact
byte-identical determinism check (save / events / summary CSV /
countries CSV / factions CSV) over the 1-year scenario. M1.16
previously added 18 covering the **per-faction CSV**: 9 diagnostics cases (faction_snapshot reads
every field, invalid id rejected with bad index in message,
default-id rejected, empty state rejects any index, header
byte-exact, row well-formed with 8 commas / 9 columns, negative
resources survives format, byte-identical for same row twice,
snapshot doesn't mutate state); 9 runner cases (`--factions-csv`
plumbed/value-missing/default-unset, no-flag no file, empty state
header-only, canonical scenario emits `9 rows = 3 factions × 3
snapshots` for a 31-day run, byte-identical determinism, summary
CSV unchanged when `--factions-csv` is added (M0.10 contract
regression), countries CSV unchanged when `--factions-csv` is
added (M1.14 contract regression)). Save schema remains v7 (no
new persistent state). Each `TEST_CASE` is registered with CTest
individually, so e.g. `ctest -R "faction_snapshot|factions-csv"`
runs just the M1.16 cases.

Previously: M1.15 added 15 covering the **policy duration
tracking** save state plus the new runtime cap on `duration_days`:
9 policy_system cases (successful enact appends one `ActivePolicy`,
pre-flight failure appends nothing, `duration_days == 0` same-day
expiry, multiple enacts preserve insertion order, faction-zero-
match enactment still records, only the actor's list grows,
negative `duration_days` rejected, `duration_days >
kMaxTrackedPolicyDurationDays` rejected, the cap boundary value
itself is accepted); 6 save_system cases (rejects an old v6 save
loudly, serialize emits `"active_policies": []`, save+load
round-trips populated entries with their `expires_on` dates, v7
country missing `active_policies` rejected, entry missing
`policy_id_code` rejected, entry with malformed `expires_on`
rejected); plus an extension to the M1.13 day-0-enactment scenario
test that pins `expires_on = 1930-03-02` for the canonical
`raise_taxes` (60-day) enactment.
Save schema is now v7. Each `TEST_CASE` is registered with CTest
individually, so e.g. `ctest -R "M1.15"` runs just the M1.15 cases.

## Build options

| Option                   | Default                       | Purpose                              |
|--------------------------|-------------------------------|--------------------------------------|
| `LEVIATHAN_BUILD_TESTS`  | `ON` when top-level, else `OFF` | Build and register the test suite. |

Override with `-DLEVIATHAN_BUILD_TESTS=OFF` if you only want the binary.

## Contributing / roadmap

Read the RFCs in order before changing scope:

1. `rfc/README.md`
2. `rfc/RFC-000-overview.md`
3. `rfc/RFC-001-development-contract.md`
4. `rfc/RFC-010-prototype-v0_1.md`
5. `rfc/RFC-060-technical-architecture.md`
6. `rfc/RFC-070-data-formats.md`
7. `rfc/RFC-080-research-formulas.md`
8. `rfc/RFC-090-roadmap.md`

Each Milestone 0 sub-milestone (M0.1 – M0.11) ships as its own PR to
`main`. Do not bundle multiple sub-milestones in one PR.

Per-milestone design notes (locked-in schemas, error formats,
architectural rules) live under `docs/m0-N-*.md`; PR write-ups live
under `docs/pr-drafts/`.

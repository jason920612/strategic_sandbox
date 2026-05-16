# Project Leviathan

一款以 1930–2000 全球政治沙盒為背景的即時暫停制國家領導人模擬遊戲。玩家不直接微操軍隊，而是透過法律、行政命令、預算、任命、外交與戰略目標影響國家。國內派系、官僚、軍方、情報部門、地方利益集團、技術菁英、媒體、學生、宗教、工會、農民等會依照自身利益自動反應，讓世界局勢自然演化、混沌且不可完全預測。

> Code name: **Project Leviathan**. Final title TBD — see `rfc/RFC-000-overview.md`.

## Status

- Phase: **Milestone 1 — single-country internal politics prototype**
- Latest shipped sub-milestone: **M1.16 — Faction-level
  diagnostics CSV.** New `FactionSummaryRow` +
  `faction_snapshot(state, faction)` +
  `write_faction_csv_header` / `write_faction_csv_row` in
  `systems::diagnostics`. New opt-in `--factions-csv PATH` runner
  flag emits 9 columns per faction per snapshot point
  (`date,id_code,country_id_code,type,support,influence,
  radicalism,loyalty,resources`). Same snapshot cadence as
  `--summary-csv` and `--countries-csv`. Existing summary CSV and
  per-country CSV both byte-for-byte unchanged (M0.10 and M1.14
  contracts preserved). No save-format bump (still v7). Drive-by:
  `main()` now prints the per-country / per-faction CSV row counts
  alongside the existing summary print, matching the data already
  in `RunOutcome`.
- Next sub-milestone candidates: **M1.17** (policy expiration
  sweep — first consumer of `active_policies.expires_on`; would add
  a monthly pipeline step but no save schema change) or **M1.18**
  (faction `react` extension — type-specific reactions beyond the
  M1.6 linear-drift rules).
- M0 closed. See `docs/milestone-0-result.md` for the M0 exit report and
  `rfc/RFC-090-roadmap.md` for the full milestone map.

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
RFC-090 §M1) is in progress. Sixteen sub-milestones merged so far:
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
contracts preserved). No save-format bump (still v7).**

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

As of M1.16 there are **432 doctest cases**. M0 contributed 179;
M1.1 added 9; M1.2 added 17; M1.3 added 9; M1.4 added 17; M1.5
added 24; M1.6 added 17; M1.7 added 16; M1.8 added 19; M1.9 added
11; M1.10 added 9; M1.11 added 25; M1.12 added 15; M1.13 added 15;
M1.14 added 17; M1.15 added 15; M1.16 adds 18 covering the
**per-faction CSV**: 9 diagnostics cases (faction_snapshot reads
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

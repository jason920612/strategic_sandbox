# Project Leviathan

一款以 1930–2000 全球政治沙盒為背景的即時暫停制國家領導人模擬遊戲。玩家不直接微操軍隊，而是透過法律、行政命令、預算、任命、外交與戰略目標影響國家。國內派系、官僚、軍方、情報部門、地方利益集團、技術菁英、媒體、學生、宗教、工會、農民等會依照自身利益自動反應，讓世界局勢自然演化、混沌且不可完全預測。

> Code name: **Project Leviathan**. Final title TBD — see `rfc/RFC-000-overview.md`.

## Status

- Phase: **Milestone 1 — single-country internal politics prototype**
- Latest shipped sub-milestone: **M1.10 — runner monthly pipeline
  wiring** — every month boundary in `leviathan --days N` invokes
  `monthly::tick_all_countries(state)`. `RunOutcome.monthly_ticks`
  counts crossings. No new state fields; no save-format bump (still
  v5). `run_state(state, opts)` is exposed so tests can inject
  pre-built countries / factions.
- Next sub-milestone candidates: **M1.11** (economy → stability
  coupling via a new `last_gdp_growth_rate` field — would trigger
  save-format v5 → v6) or **M1.12** (scenario loader reading
  `data/countries/*.json` + `data/factions/*.json`).
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
RFC-090 §M1) is in progress. Ten sub-milestones merged so far:
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
order `faction::react → stability::tick → economy::tick`); **M1.10
runner monthly pipeline wiring — the M0.9 runner now invokes
`monthly::tick_all_countries` on every month boundary**, and a new
`run_state(state, opts)` entry point lets tests inject pre-built
state. `leviathan --days 365` runs the monthly pipeline 12 times
(over an empty country list by default, since the canonical M0.9
runner does not load country fixtures — that's a future scenario-
loader sub-milestone).

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
│                         policies/*.json (M1.4)
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

# Pinned configuration matching the M0.9 spec example
./build/bin/Debug/leviathan \
    --config data/config/simulation.json \
    --days 365 \
    --seed 12345 \
    --output out/ \
    --save out/save.json \
    --log out/events.jsonl \
    --summary-csv out/summary.csv
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

As of M1.10 there are **327 doctest cases**. M0 contributed 179;
M1.1 added 9; M1.2 added 17; M1.3 added 9; M1.4 added 17; M1.5
added 24; M1.6 added 17; M1.7 added 16; M1.8 added 19; M1.9 added
11; M1.10 adds 9 covering the **runner monthly-pipeline wiring**:
`monthly_ticks` counter for 10 / 31 / 365 days (matched against
TimeSystem's `month_changed` semantics with the canonical 1930-01-01
start), empty-state determinism preserved, save schema still v5,
and `run_state` integration with a hand-built 1-country / 1-faction
state where every expected country / faction field actually
mutates. Existing 365-day `log_count == 15` invariant still holds
because M1.10 introduces no new logs. Each `TEST_CASE` is
registered with CTest individually, so e.g.
`ctest -R "m110"` runs just the M1.10 cases.

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

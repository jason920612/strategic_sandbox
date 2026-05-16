# Milestone 0 — exit report

Status: **complete** (M0.1 → M0.11 all merged).

This document is the M0 deliverables ledger. It records what M0 ships,
what is deliberately deferred to M1, and what M1 should tackle first.

## 1. What ships in M0

### 1.1 Build & toolchain

| Sub-milestone | What landed |
|---|---|
| M0.1 | CMake 3.16+, C++17, MSVC `/W4` + GCC/Clang `-Wall -Wextra -Wpedantic -Wconversion`. Top-level + per-target CMakeLists. `.gitattributes`, `.gitignore`. |

### 1.2 Foundational types — `leviathan::core`

| Sub-milestone | Types | Notes |
|---|---|---|
| M0.2 | `StrongId<Tag>` (+ six aliases), `GameDate`, `Result<T, E>`, `string_utils::trim` | Real Gregorian calendar. `Result` permits `T == E` (e.g. `Result<std::string>`). Strong IDs are not interchangeable across tags. |
| M0.3 | `SimulationConfig`, `RandomState`, entity stubs (`CountryState`, `ProvinceState`, `FactionState`, `PolicyData`, `EventDefinition`, `LogEntry`), `GameState`, `make_game_state(config)` | `GameState` has **zero methods**; everything that touches state is a free function. Factory always zeros `rng.counter`. |

### 1.3 Systems — `leviathan::systems`

| Sub-milestone | Namespace | Entry points |
|---|---|---|
| M0.4 | `systems::time` | `advance_one_day(state) -> TickResult`, `advance_days(state, n)` |
| M0.5 | `systems::random` | `next_u64`, `draw_int`, `draw_unit`, `draw_double`, `draw_bool`, `weighted_choice`. Counter-indexed splitmix64. **No `<random>`, no `<random_device>`**. Trace callback for non-determinism debugging. |
| M0.6 | `systems::logging` | `log()` and severity shortcuts; `recent(state, n)`; `write_jsonl_line` / `export_jsonl`. `log()` is the **only** path that writes to `state.logs`. |
| M0.7 | `systems::data_loader` | `parse_simulation_config` / `load_simulation_config` / `parse_country` / `load_country`. nlohmann/json v3.11.3 (FetchContent, pinned, PRIVATE link). |
| M0.8 | `systems::save_system` | `serialize` / `save` / `deserialize` / `load`. `save_version` and `rng_algorithm_version` strict-equality gates. |
| M0.9 | `systems::runner` | `parse_args` (pure) + `run` (orchestrator). Headless CLI: `--config`, `--days`, `--seed`, `--output`, `--save`, `--log`, `--help`. |
| M0.10 | `systems::diagnostics` | `snapshot`, `write_csv_*`, `sanity_check`. Runner gains `--summary-csv`. Observation-only. |

### 1.4 Data

| File | Source |
|---|---|
| `data/config/simulation.json` | M0.7 — RFC-070 §6 minimum |
| `data/countries/germany.json` | M0.7 — RFC-070 §1 minimum (`GER`) |
| `data/countries/france.json` | M0.11 — second exemplar (`FRA`) |
| `data/countries/japan.json` | M0.11 — third exemplar (`JPN`) |

### 1.5 Determinism property

Two runs with the same `RunnerOptions` and the same config files
produce **byte-identical** outputs across three artefacts:

- `out/save.json` (M0.8)
- `out/events.jsonl` (M0.6)
- `out/summary.csv` (M0.10, when `--summary-csv` is set)

This is the end-to-end realisation of RFC-000 §5 rule 10 ("all
randomness must go through seed; long-term goal is replayable").
Pinned by dedicated tests (`byte-identical`, `summary-csv
same-seed`, plus the M0.11 round-trip test).

### 1.6 Test counts at M0 exit

```
> ctest --test-dir build -C Debug --output-on-failure
100% tests passed, 0 tests failed out of 179
```

| Suite | Tests |
|---|---|
| core (ids, result, game_date, string_utils, simulation_config, game_state) | ~50 |
| systems::time | 12 |
| systems::random | 20 |
| systems::logging | 13 |
| systems::data_loader | 23 |
| systems::save_system | 20 |
| systems::runner | 26 |
| systems::diagnostics | 13 |
| integration::m0_end_to_end | 1 (the M0.11 exit-gate) |

## 2. What's deliberately deferred

These appear in the RFCs but were intentionally **NOT** shipped in M0
to keep the milestone scope strict. They are M1 (or later) work.

### 2.1 Entity model

Entity structs in `entities.hpp` carry only ID + name (plus the JSON
loader's economic baseline fields on `CountryState`). M0 does not
attach **any** simulation logic to entities. Specifically:

- No GDP growth (RFC-080 §4)
- No tax revenue (RFC-080 §3)
- No stability mechanics (RFC-080 §5)
- No coup risk / civil-war risk (RFC-080 §6, §7)
- No faction support / influence / radicalism evolution
- No policy enactment or effects

The data model is in place; the *behaviour* isn't.

### 2.2 Information distortion

RFC-050 §1 (hidden truth) and RFC-080 §8 (information accuracy)
remain future work. M0 logs are ground-truth; we don't yet model
`ReportedValue = TrueValue + Bias + Noise`.

### 2.3 Events / world AI

- No event templates or trigger conditions (RFC-050)
- No AI decision utility (RFC-080 §9)
- No diplomatic misperception (RFC-080 §10)
- No wars, alliances, sanctions

### 2.4 UI / map

- No SVG map exporter
- No HTML viewer
- No interactive panels

### 2.5 Replay (vs session resume)

M0.8 ships **session resume** (round-trip a paused game). It does
**not** yet ship deterministic replay (re-run a session from
`start_date` and verify hashes match). The infrastructure is in
place — `rng.counter` is preserved in saves and the
`rng_algorithm_version` field is set — but the per-tick decision log
needed for true replay is not recorded.

### 2.6 Country loading in the runner

The headless runner (M0.9) intentionally does not load any country
JSON. The M0.11 integration test exercises country loading by
composing the systems manually. A `--countries DIR` flag (or an
extension of the simulation config to list country files) is the
natural next step.

### 2.7 CI

Only verified on local Windows / MSVC 19.44 / CMake 4.0.2. No CI
runs against GCC or Clang yet. The CMake is written portably and
the warning sets are configured for all three compilers; CI would
verify that.

## 3. Recommendations for M1

Read RFC-090 §1 (Milestone 1: 單國內政原型) for the full list. The
first task that touches *behaviour*, not just plumbing, should be:

1. **M1.1 — flesh out `CountryState`** with the real simulation fields
   from RFC-060 §3 (legal tax burden, fiscal capacity, central
   control, corruption, stability, military power, threat perception)
   and update `data/countries/*.json` to carry them.
   - Update the M0.7 country loader schema and tests.
   - The M0.11 integration test will likely need a corresponding
     bump in its assertions.

2. **M1.2 — `FactionState` real fields** (support, influence,
   radicalism) — analogous shape change.

3. **M1.4 — `PolicyData` fields + 10 policies** + the M1.8
   "policy enactment" free function. This is where the *first*
   real simulation effect lands.

4. Only **after** Milestone 1 is on its feet should we revisit:
   - Country loading in the runner (a real consumer for it then
     exists)
   - The `--countries DIR` flag
   - Replay infrastructure (it needs decision logs from the
     policy / event systems anyway)

### 3.1 Architectural rules to preserve

These are the M0 design invariants every subsequent milestone must
respect:

- **`GameState` has no methods.** Behaviour goes in free functions in
  `systems::`.
- **Systems are free functions taking `GameState&` (or `const &`).**
  Dependency direction is strictly `systems → core`.
- **Logging is always explicit.** No system writes to `state.logs`
  as a side effect of doing its real job. `systems::logging::log()`
  is the only function that pushes to `state.logs`.
- **Diagnostics is observation-only.** Functions take `const GameState&`.
- **No `<random>` or `<random_device>`.** Use `systems::random::*`.
- **No `<filesystem>` or path metadata in deterministic logs.** The
  byte-identical-output tests will break if a future system embeds
  paths in `log_*` calls.
- **`save_version` / `rng_algorithm_version` are load-bearing.** Bump
  on incompatible schema or algorithm changes; old saves should fail
  loudly.
- **doctest test names must not contain `[brackets]`.** They are
  interpreted as tags and silently break CTest discovery.

### 3.2 Known small-scope tech debt

- `parse_positive_int` in `runner.cpp` allows 0 — the name is slightly
  imprecise. Renaming to `parse_nonnegative_int` is a cosmetic clean-up
  for a quiet PR.
- `Result<bool>` is the success carrier when there's no real payload.
  A `Result<void>` specialisation would be cleaner; not blocking.
- `require_string` / `require_number` / `require_u64` /
  `require_date` helpers are duplicated between `data_loader.cpp`
  and `save_system.cpp`. Extracting them into a private
  `_json_helpers.hpp` becomes worthwhile when a third consumer
  appears.
- nlohmann/json v3.11.3 emits a CMake-policy deprecation warning
  under CMake 4.x. We carry a `CMAKE_POLICY_VERSION_MINIMUM=3.5` shim;
  expect to drop it when upstream tags a release with a modern
  `cmake_minimum_required`.

## 4. How to verify M0 right now

```bash
# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug

# Run the M0 exit-gate test only
ctest --test-dir build -C Debug -R "M0 end-to-end"

# Full suite (179 cases)
ctest --test-dir build -C Debug --output-on-failure

# Full headless run (deterministic given same flags)
./build/bin/Debug/leviathan \
    --config data/config/simulation.json \
    --days 365 \
    --seed 12345 \
    --output out/ \
    --save out/save.json \
    --log out/events.jsonl \
    --summary-csv out/summary.csv
```

## 5. Where the design notes live

| Topic | Note |
|---|---|
| Calendar + core types | `docs/m0-2-calendar-and-types.md` |
| GameState container rules | `docs/m0-3-game-state.md` |
| TimeSystem / free-function-systems pattern | `docs/m0-4-time-system.md` |
| Deterministic RNG | `docs/m0-5-rng-service.md` |
| LoggingSystem & JSONL spec | `docs/m0-6-logging.md` |
| Data loader & JSON schemas | `docs/m0-7-data-loader.md` |
| Save/load schema & version policy | `docs/m0-8-save-load.md` |
| Headless runner CLI | `docs/m0-9-runner.md` |
| Diagnostics & CSV spec | `docs/m0-10-diagnostics.md` |
| **M0 exit report (this file)** | `docs/milestone-0-result.md` |

`docs/README.md` is the canonical index.

M0 closes here. The repo is ready for Milestone 1.

---

## Note (added after M1.1)

Milestone 1 has begun. `CountryState` gained its real runtime numeric
fields in M1.1 and the save format bumped to v2; that is a deliberate
schema change rather than a violation of M0's exit contract. The M0
test count quoted above (179) reflects the M0 exit state; the live
total is higher as M1 progresses. See `docs/m1-1-country-state.md` for
the migration details and `docs/README.md` for the current per-PR
ledger.

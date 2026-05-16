# Project Leviathan

一款以 1930–2000 全球政治沙盒為背景的即時暫停制國家領導人模擬遊戲。玩家不直接微操軍隊，而是透過法律、行政命令、預算、任命、外交與戰略目標影響國家。國內派系、官僚、軍方、情報部門、地方利益集團、技術菁英、媒體、學生、宗教、工會、農民等會依照自身利益自動反應，讓世界局勢自然演化、混沌且不可完全預測。

> Code name: **Project Leviathan**. Final title TBD — see `rfc/RFC-000-overview.md`.

## Status

- Phase: **Milestone 0 — technical skeleton**
- Current sub-milestone: **M0.9 — headless simulation runner**
- See `rfc/RFC-090-roadmap.md` for the full milestone map.

`GameState` is still a passive container. Systems so far:
`leviathan::systems::time` (date advance + boundary detection);
`leviathan::systems::random` (deterministic splitmix64 RNG, no
`<random>`); `leviathan::systems::logging` (explicit-only logging
with byte-stable JSONL); `leviathan::systems::data_loader` (JSON
config + country parsers via nlohmann/json); `leviathan::systems::save_system`
(JSON round-trip with `save_version` / `rng_algorithm_version` gates);
and now `leviathan::systems::runner`, which wires those into a real
CLI (`leviathan --days N [--config ...] [--seed ...] [--output ...]`).
Two runs with the same options produce byte-identical save and log
files. The CSV / diagnostics output milestones (M0.10, M0.11) are
the last pieces of M0.

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
│                         countries/germany.json (M0.7 canonical samples)
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
    --log out/events.jsonl
```

Required flag: `--days`. Everything else has a default
(`--config data/config/simulation.json`, `--output out/`, `--seed`
falls back to the value in the config file, `--save` and `--log` live
under `--output`).

Outputs (paths can be overridden):

| File | Format | Source |
|------|--------|--------|
| `out/save.json`     | Pretty-printed save (M0.8) | Round-trippable with `save_system::load` |
| `out/events.jsonl`  | One JSON object per line   | M0.6 logging exporter |
| stdout              | Plain-text run summary     | start / end dates, log count, output paths |

**Determinism**: the same `--config` + `--days` + `--seed` produces
byte-identical `save.json` and `events.jsonl`. The
`run: same seed produces byte-identical save and log files` test in
`tests/systems/runner_test.cpp` pins this.

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

As of M0.9 there are ~156 doctest cases covering all foundational
types, TimeSystem, RandomService, LoggingSystem, DataLoader,
SaveSystem, and Runner (arg-parse happy and error paths, 3-day and
365-day end-to-end runs, output-dir auto-create, `--seed` override,
and the determinism check: same seed produces **byte-identical**
save and log files). Each `TEST_CASE` is registered with CTest
individually, so e.g. `ctest -R "byte-identical"` runs just the
determinism guard.

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

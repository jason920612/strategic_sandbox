# Project Leviathan

一款以 1930–2000 全球政治沙盒為背景的即時暫停制國家領導人模擬遊戲。玩家不直接微操軍隊，而是透過法律、行政命令、預算、任命、外交與戰略目標影響國家。國內派系、官僚、軍方、情報部門、地方利益集團、技術菁英、媒體、學生、宗教、工會、農民等會依照自身利益自動反應，讓世界局勢自然演化、混沌且不可完全預測。

> Code name: **Project Leviathan**. Final title TBD — see `rfc/RFC-000-overview.md`.

## Status

- Phase: **Milestone 0 — technical skeleton**
- Current sub-milestone: **M0.6 — LoggingSystem**
- See `rfc/RFC-090-roadmap.md` for the full milestone map.

`GameState` is still a passive container. `leviathan::systems::time`
provides `advance_one_day` and `advance_days`; `leviathan::systems::random`
provides `next_u64` / `draw_int` / `draw_unit` / `draw_double` /
`draw_bool` / `weighted_choice` (deterministic splitmix64 on
`(seed, counter)`, no `<random>`); `leviathan::systems::logging`
provides explicit `log()` with `Debug/Info/Warn/Error` severity, a
deterministic ordered-metadata `LogEntry`, `recent(state, n)`, and
byte-stable JSONL export. The data-loader, save/load, and
headless-runner systems remain stubbed and arrive across M0.7 – M0.11.

## Repository layout

```text
.
├── CMakeLists.txt        Top-level build
├── README.md             This file
├── rfc/                  Design RFCs — read these before changing scope
├── include/              Public headers (currently empty)
├── src/                  Simulation core + executable entry point
├── tests/                Unit / integration tests
├── data/                 Game data (JSON), currently empty
├── tools/                Dev / debug tools, currently empty
└── docs/                 Long-form developer docs, currently empty
```

## Requirements

- A C++17 compiler:
  - MSVC 19.20+ (Visual Studio 2019 or 2022), or
  - GCC 9+, or
  - Clang 10+
- CMake **3.16 or newer**
- A build tool that CMake can drive (Ninja, Make, MSBuild, Xcode, ...)
- Network access on the **first** configure: the test suite fetches
  [doctest](https://github.com/doctest/doctest) v2.4.11 via
  `FetchContent`. Subsequent configures reuse the cached clone in
  `build/_deps/`.

No JSON library is required yet; it lands in M0.7.

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
on Windows). Run it to verify the toolchain:

```bash
./build/bin/leviathan
```

You should see a short banner identifying the project and milestone.

## Test

```bash
ctest --test-dir build --output-on-failure
```

For multi-config generators (Visual Studio, Xcode):

```bash
ctest --test-dir build -C Debug --output-on-failure
```

As of M0.6 there are ~94 doctest cases covering all foundational
types, TimeSystem, RandomService, and LoggingSystem (log appends,
severity convenience wrappers, metadata insertion-order preservation,
`recent(n)`, JSONL snapshot incl. escape and metadata rendering, the
"TimeSystem must still not auto-log" regression). Each `TEST_CASE`
is registered with CTest individually, so `ctest -R JSONL` runs just
the export tests.

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

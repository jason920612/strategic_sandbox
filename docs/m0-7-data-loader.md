# M0.7 - DataLoader design notes

Companion notes for `feature/m0-07-json-loader`. Locks in the JSON
schemas, the error-message format, and the architectural rules that
keep DataLoader pure (no GameState mutation, no time advance, no
hard logging coupling).

## 1. Library: nlohmann/json v3.11.3

- Pulled via CMake FetchContent with `GIT_SHALLOW TRUE` and a pinned
  tag. The same `CMAKE_POLICY_VERSION_MINIMUM=3.5` shim we use for
  doctest keeps it building under CMake 4.x.
- Linked **PRIVATE** to `leviathan_systems`. The header is an
  implementation detail of `data_loader.cpp` and never appears in any
  public Leviathan header — the only public surface is the
  `Result<T>`-returning free functions.
- Why nlohmann and not RapidJSON / simdjson?
  - Header-only, no extra link step.
  - Ergonomic API matches our "small typed structs" workflow.
  - Compile-time hit is bounded because only `data_loader.cpp`
    includes it.
- Why pinned to v3.11.3?
  - Stable release as of the M0.7 PR. We do not want
    silent behaviour changes from auto-tracking a moving tag.
  - When a future bump is warranted, it gets its own PR + retest.

## 2. Public API

```cpp
namespace leviathan::systems::data_loader {

core::Result<core::SimulationConfig> parse_simulation_config(
    std::string_view json_text, std::string_view source_label = "<inline>");

core::Result<core::SimulationConfig> load_simulation_config(
    const std::filesystem::path& path);

core::Result<core::CountryState> parse_country(
    std::string_view json_text, std::string_view source_label = "<inline>");

core::Result<core::CountryState> load_country(
    const std::filesystem::path& path);

}
```

`parse_*` functions read raw text. `load_*` functions wrap `parse_*`
with file I/O; they pass the file path as the `source_label` so error
messages name the offending file automatically.

## 3. JSON schemas

### Simulation config — `data/config/simulation.json`

```json
{
  "simulation": {
    "start_date": "YYYY-MM-DD",     // required
    "end_date":   "YYYY-MM-DD",     // optional, defaults to 2000-12-31
    "seed":       <unsigned int>,   // optional, defaults to 0
    "daily_tick": <bool>            // optional, defaults to true
  }
}
```

Constraints:
- `start_date` must parse as a real Gregorian date (see
  `GameDate::parse`).
- `end_date`, if present, must also parse as a real date. We do NOT
  enforce `start_date <= end_date` at parse time; that's a higher-
  level invariant the runner can check.
- `seed` must be a non-negative integer that fits in `uint64_t`.
- Any extra top-level or `simulation`-level keys are silently ignored.
  This lets newer data files load on older runners without breaking,
  and matches RFC-001's "always be permissive at the read boundary".

### Country — `data/countries/<id>.json`

```json
{
  "id":                "<3-letter or so code>",   // required
  "name":              "<official name>",          // required
  "display_name":      "<shown in UI>",            // optional, defaults to name
  "initial_gdp":       <finite number>,            // required
  "initial_stability": <finite number>             // required
}
```

Constraints:
- `id` is a string and stays as `CountryState::id_code`. It is the
  **on-disk** identifier and is stable across saves.
- The numeric `CountryState::id` is left at its `invalid()` default by
  the loader. Assigning the numeric ID is the caller's responsibility
  (typically by insertion order into `state.countries`).
- `initial_gdp` and `initial_stability` must be finite numbers (the
  loader rejects `NaN` / `Inf`).

## 4. Error-message format

```
<source>: <reason>
```

Where `<source>` is the file path for `load_*` or the user-supplied
label for `parse_*`. Concrete categories:

| Trigger | Example |
|---|---|
| Malformed JSON | `data/sim.json: JSON parse error (malformed document)` |
| Wrong top-level shape | `data/sim.json: top-level JSON value is not an object` |
| Missing required field | `data/sim.json: missing required field 'simulation.start_date'` |
| Wrong field type | `data/sim.json: 'simulation.seed' has wrong type (expected unsigned integer)` |
| Invalid date | `data/sim.json: 'simulation.start_date' = "1930-02-30" is not a real Gregorian date` |
| Negative seed | `data/sim.json: 'simulation.seed' is negative` |
| Non-finite number | `data/germany.json: 'initial_gdp' is not finite` |
| File I/O | `does-not-exist/sim.json: cannot open file for reading` |

The bad value is always echoed back to the user (where applicable) so
a human reading the message can locate the offending byte in the file
without re-parsing it themselves.

## 5. Architectural rules

### 5.1 No GameState mutation in DataLoader

`parse_*` and `load_*` only return values. They never see a
`GameState&`. The composition is left to the caller:

```cpp
auto cfg = dl::load_simulation_config("data/config/simulation.json");
if (cfg.failed()) { /* handle */ }
GameState state = make_game_state(cfg.value());

auto country = dl::load_country("data/countries/germany.json");
if (country.ok()) {
    auto entry = std::move(country).value();
    entry.id = CountryId{0};                // caller decides numeric id
    state.countries.push_back(std::move(entry));
}
```

This keeps the loader testable in isolation and prevents drift toward
"loader does too much".

### 5.2 No time / RNG side effects

The loader does not advance `state.current_date`, does not call
`RandomService`, and is internally deterministic given the same input.
This is verified indirectly via the TimeSystem non-interference test
(`state.logs.empty()` after `advance_days(state, 100)`).

### 5.3 Loggable but not coupled

`data_loader.hpp` does **not** include `logging_system.hpp`. Errors
travel as `Result::failure(message)`. The caller chooses whether to:

- print to stderr (the M0.7 main demo does this), and/or
- call `log_warn` / `log_error` with the message as metadata (also
  demonstrated in main).

This means we can unit-test the loader without touching the logging
system, and we can swap the logging policy in main / the headless
runner without recompiling the loader.

## 6. Test data files

- `data/config/simulation.json` — canonical config matching the
  RFC-070 §6 example.
- `data/countries/germany.json` — canonical country matching RFC-070 §1
  (minimal subset — full RFC-070 country shape includes faction lists,
  fiscal capacity, threat perception, etc., which M1+ will introduce).

CMake injects `LEVIATHAN_TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/data` into
the test binary so file-loading tests find these regardless of the
working directory. Tests that don't need real files use `parse_*` with
inline string literals so they don't depend on the data layout.

## 7. What's still NOT in scope

- **Schema validation as a separate pass.** The current code performs
  shape-and-type checks inline with the parse. If we ever need to
  surface multiple errors per file (e.g. "fields A, B, and C are
  wrong"), we'll add a real validator pass. Not needed for M0.7.
- **Hot-reload.** Files are read once. Watching for changes lives in
  tooling, not in the simulation core.
- **Comments / trailing commas / JSON5.** We use strict JSON. Author
  comments can live in adjacent `.md` files.
- **Numeric ID assignment.** Already covered above.
- **Cross-file references.** The minimal country file has no
  references to other entities. When factions / policies join in M1,
  cross-references will probably resolve via the string `id_code`,
  not the numeric ID.

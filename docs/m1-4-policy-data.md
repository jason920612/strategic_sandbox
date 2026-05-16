# M1.4 - PolicyData schema (+ shared JSON helper extraction)

Companion notes for `feature/m1-04-policy-data-schema`. Fourth M1
data-model PR. Adds `PolicyData` with a typed `PolicyEffect` vector,
ten policy fixtures spanning RFC-010 §2.6 categories, and (the
reviewer's recommended refactor) extracts the duplicated JSON
validation helpers into a shared private header.

**No policy effect application.** The effects are stored as
`{target, op, value}` triples; M1.5 (PolicySystem) will interpret
them.

## 1. Shared JSON helpers — `src/leviathan/systems/internal/json_helpers.hpp`

Before M1.4, `fmt_err`, `navigate`, `require_string`, `require_number`,
`require_u64`, `require_ratio`, and `require_nonneg_number` lived in
**three** places:

- The anonymous namespace of `data_loader.cpp`
- The anonymous namespace of `save_system.cpp`
- (Effectively duplicated again as lambdas in
  `country_from_json`'s and `faction_from_json`'s `load_num`.)

M1.4 introduces a private header in `src/leviathan/systems/internal/`
under namespace `leviathan::systems::detail`. The `leviathan_systems`
CMake target gets `target_include_directories(... PRIVATE
${CMAKE_CURRENT_SOURCE_DIR})` so both `.cpp` files can write
`#include "internal/json_helpers.hpp"`. The public surface
(`include/leviathan/...`) never references the internal header.

What lives in the shared helpers:

```cpp
namespace leviathan::systems::detail {
  using json = nlohmann::ordered_json;

  std::string fmt_err(...);
  const json* navigate(const json&, std::string_view);
  Result<std::string>   require_string(...);
  Result<double>        require_number(...);
  Result<double>        require_nonneg_number(...);
  Result<double>        require_ratio(...);
  Result<std::uint64_t> require_u64(...);
}
```

DataLoader-specific helpers (`require_date`, `read_whole_file`) stay
in `data_loader.cpp`'s anonymous namespace because they have no
second consumer. SaveSystem also keeps its own `require_date` (used
only for `current_date` and log timestamps); extracting that helper
is a future refactor if a third caller appears.

**No behaviour change** from this refactor — the previous 214 tests
all still pass without modification. The extraction is mechanical.

## 2. `PolicyEffect` + `PolicyData` field set

```cpp
struct PolicyEffect {
    std::string target;   // "country.military_power", "faction:military.support", ...
    std::string op;       // "add" / "set" / "mul" / ... (free-form for M1.4)
    double      value = 0.0;
};

struct PolicyData {
    PolicyId    id;             // numeric, caller-assigned
    std::string id_code;        // "increase_military_budget"
    std::string name;           // "Increase Military Budget"
    std::string category;       // "budget" / "tax" / "media" / ...

    int    duration_days = 0;   // >= 0
    double admin_cost    = 0.0; // [0, 1] - share of administrative capacity

    std::vector<PolicyEffect> effects;
};
```

### Why typed effects now, instead of deferring to M1.5?

Two options were considered:

- Store effects as `std::string` JSON blobs (lazy-parse in M1.5).
- Store as typed `PolicyEffect` structs.

The typed option won because the JSON parser is already here — we'd
just be deferring exactly the same parse to M1.5. Worse, lazy-parsing
means the loader doesn't validate effect shape on load, so a typo in
a policy file isn't caught until the policy is enacted. With typed
effects, malformed `effects` arrays fail at config-load time with
clear `policies[N]: effects[K]: 'target' missing` messages.

### `op` and `target` are free-form strings

Same reasoning as `FactionState::type` in M1.2: data-driven
flexibility wins over up-front enum dispatch. If M1.5+ ends up
branching heavily on `op`, we'll introduce a typed enum and a
`op_from_string` validator.

## 3. JSON schema (DataLoader)

```jsonc
{
  "id":            "increase_military_budget",  // required
  "name":          "Increase Military Budget",   // required
  "category":      "budget",                     // required
  "duration_days": 30,                           // required, integer, >= 0
  "admin_cost":    0.10,                         // required, [0, 1]
  "effects": [                                   // required (may be [])
    { "target": "country.military_power",   "op": "add", "value":  0.03 },
    { "target": "faction:military.support", "op": "add", "value":  0.08 },
    { "target": "faction:workers.support",  "op": "add", "value": -0.03 }
  ]
}
```

Each effect requires `target` (string), `op` (string), `value`
(finite number). Errors place the policy + effect context in the
path:

```
policy.json: missing required field 'id'
policy.json: 'admin_cost' must be in [0, 1] (got 1.500000)
policy.json: 'duration_days' is negative
policy.json: effects[0]: 'target' has wrong type (expected string)
policy.json: effects[2]: missing required field 'value'
```

### `duration_days` validation

`duration_days` is read via `require_u64` (which rejects negatives
in the spec sense — it parses unsigned only) and then range-checked
against `INT_MAX` before truncating to `int`. Same uint64-boundary
pattern as `simulation.seed` in M0.7 and the numeric CountryId
range checks in M0.8.

## 4. Save format change: v4 → v5

`kSaveFormatVersion: 4 → 5`. v4 saves now fail loudly:

```
save.json: unsupported save_version 4 (this binary supports 5)
```

The rule from M1.2 / M1.3 applies again: every PR adding content to
a previously-absent / previously-empty payload bumps the version.
v4 saves had `policies: []`; M1.4 saves can have populated policies
with the new field set. New regression test
`"deserialize: an old v4 save is rejected loudly"` pins this.

## 5. Drive-by from PR #13 (already in M1.3) — none new in M1.4

M1.3 already fixed the `CountryId::underlying_type` issue in
`faction_from_json`. M1.4 introduces no drive-by fixes.

## 6. Policy fixtures (10 files under `data/policies/`)

| File | category | duration | admin_cost | # effects | flavour |
|---|---|---|---|---|---|
| `increase_military_budget.json` | budget | 30 | 0.10 | 3 | M+ workers- |
| `cut_military_budget.json`      | budget | 30 | 0.08 | 4 | M- workers+ balance+ |
| `raise_taxes.json`              | tax    | 60 | 0.12 | 3 | tax+ workers- elites- |
| `lower_taxes.json`              | tax    | 60 | 0.08 | 4 | tax- balance- |
| `expand_welfare.json`           | welfare | 90 | 0.15 | 3 | stability+ workers+ balance- |
| `increase_education.json`       | education | 90 | 0.10 | 4 | admin+ students+ |
| `press_censorship.json`         | media  | 30 | 0.12 | 4 | control+ media- legitimacy- |
| `press_freedom.json`            | media  | 30 | 0.05 | 3 | legitimacy+ media+ M- |
| `intelligence_expansion.json`   | intelligence | 60 | 0.10 | 4 | control+ intel+ |
| `administrative_reform.json`    | admin  | 180 | 0.20 | 4 | admin+ corruption- bureaucracy- |

These cover the seven RFC-010 §2.6 policy categories (tax, military
budget, education, media, intelligence, welfare, admin reform). The
values are M1.4 placeholders; balance tuning happens once M1.5
makes them load-bearing.

## 7. What M1.4 deliberately does NOT do

- **No policy enactment.** Nothing reads `effects[].target` to apply
  changes. M1.5 is PolicySystem.
- **No `op` interpretation.** "add" / "set" / "mul" are stored
  strings; nothing branches on them yet.
- **No `target` parsing.** The path syntax (`country.X`,
  `faction:type.X`) is documented but not parsed. M1.5 will write
  a `parse_target` helper.
- **No faction-policy preference link.** `FactionState.preferred_policies`
  from M1.2 stores policy id_codes as strings; nothing resolves them
  against the policy table.
- **No active-policy queue on `GameState`.** Policies are loaded /
  saved as templates (`state.policies`); a separate "policies in
  flight" container will arrive when enactment is on the table.
- **No `Diagnostics::sanity_check` rule for policies.** Could add
  `"policy effect target doesn't resolve"` once a resolver exists.

## 8. Migration impact

| File | Δ |
|---|---|
| `src/leviathan/systems/internal/json_helpers.hpp` | **new** — shared helpers |
| `src/leviathan/systems/internal/json_helpers.cpp` | **new** — impls |
| `src/leviathan/systems/CMakeLists.txt` | adds the new .cpp + PRIVATE include dir |
| `src/leviathan/systems/data_loader.cpp` | dedup: removes anon-namespace helpers; `using` declarations re-export them; adds `parse_policy` / `load_policy` |
| `src/leviathan/systems/save_system.cpp` | dedup likewise; adds `policy_to_json` / `policy_from_json`; serialise() / deserialise() handle the policies array |
| `include/leviathan/core/entities.hpp` | `+PolicyEffect`, expanded `PolicyData` |
| `include/leviathan/systems/data_loader.hpp` | new schema doc + `parse_policy` / `load_policy` declarations |
| `include/leviathan/systems/save_system.hpp` | `kSaveFormatVersion` 4 → 5, version-history comment |
| `data/policies/*.json` | **10 new fixtures** |
| `tests/systems/data_loader_test.cpp` | +12 cases (parse_policy happy, empty effects, missing fields, range checks, file load) |
| `tests/systems/save_system_test.cpp` | +5 cases (v4-rejected, policies round-trip + 2 effect-shape failures), sweep of inline `save_version: 4` → 5, `build_seeded_state` adds a policy |
| `tests/integration/m0_end_to_end_test.cpp` | new Step 3b loads 3 policies; round-trip assertions extended |
| `data/policies/` and `data/factions/` continue side-by-side | no relationship enforced yet |

Total tests: **214 → 231** (+17).

## 9. Risks / things to watch

- **Save format v5 is now load-bearing.** Same rule: future PolicyData
  shape changes must extend forward-compatibly or bump to v6 and
  reject v5 saves.
- **`PolicyEffect::target` is free-form.** A typo like
  `"countryy.military_power"` won't be flagged at load time; the
  M1.5 resolver will fail loudly when it can't match the path.
  Adding a validator at load time requires committing to a path
  grammar — postponed to M1.5.
- **The shared helpers are nlohmann/json-aware** (`using json = ordered_json`).
  If we ever decide to swap JSON libraries, both consumers and the
  internal header switch together — small surface, contained change.
- **`internal/` namespace convention** (`leviathan::systems::detail`)
  is now established. Future private helpers should go under it,
  not in per-file anonymous namespaces, when more than one consumer
  exists.

## 10. Next sub-milestone

**M1.5 — PolicySystem apply effects.** The first M1 sub-milestone
that produces a real *effect*. Will:

- Parse the `target` path syntax (`country.X`,
  `faction:type.X`, ...).
- Branch on `op` (initially `"add"` and `"set"`).
- Apply each effect to the relevant `CountryState` / `FactionState`
  field.
- Be invoked explicitly by a caller (free function over
  `GameState&`).

M1.5 is also where `Diagnostics::sanity_check` could grow rules like
"policy effect targets resolve" once the resolver exists.

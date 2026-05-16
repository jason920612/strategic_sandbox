# M1.1 - CountryState real fields

Companion notes for `feature/m1-01-country-state-fields`. The first
Milestone 1 PR. Pure data-model and plumbing change: the struct grows
runtime numeric fields, the JSON config schema lists them as required,
the save format bumps to v2, and the loader rejects malformed /
out-of-range values. **No simulation logic** runs on these fields yet.

## 1. Field set

```cpp
struct CountryState {
    CountryId   id;             // numeric handle (caller-assigned)
    std::string id_code;        // on-disk identifier, e.g. "GER"
    std::string name;
    std::string display_name;

    // Absolute economic state
    double gdp;
    double tax_revenue;       // runtime-only, not in JSON config
    double budget_balance;    // runtime-only, can be negative

    // Fiscal / administrative ratios in [0, 1]
    double legal_tax_burden;
    double fiscal_capacity;
    double administrative_efficiency;
    double central_control;
    double corruption;

    // Political ratios in [0, 1]
    double stability;
    double legitimacy;

    // Strategic ratios in [0, 1]
    double military_power;
    double threat_perception;
};
```

Naming convention:

- **`gdp` / `tax_revenue` / `budget_balance`** are absolute amounts.
  They have no upper bound and `budget_balance` can be negative.
  `gdp` cannot be negative.
- **Everything else is a ratio in `[0, 1]`.** This includes
  `corruption` (a leakage fraction, per Besley & Persson) and
  `threat_perception` (subjective worry, normalised).

`initial_gdp` / `initial_stability` are NOT fields on the struct.
They live in the JSON config and the loader maps them to the
runtime `gdp` / `stability` fields. The naming difference makes the
config readable ("this is the starting value") while keeping the
runtime struct compact.

## 2. JSON config schema (DataLoader)

`data/countries/<code>.json` must contain every field below:

| Field                       | Type        | Range   | Notes |
|-----------------------------|-------------|---------|-------|
| `id`                        | string      | -       | On-disk identifier |
| `name`                      | string      | -       | Required |
| `display_name`              | string      | -       | Optional, defaults to `name` |
| `initial_gdp`               | number      | `>= 0`  | → runtime `gdp` |
| `initial_stability`         | number      | `[0,1]` | → runtime `stability` |
| `legal_tax_burden`          | number      | `[0,1]` | Required |
| `fiscal_capacity`           | number      | `[0,1]` | Required |
| `administrative_efficiency` | number      | `[0,1]` | Required |
| `central_control`           | number      | `[0,1]` | Required |
| `corruption`                | number      | `[0,1]` | Required |
| `legitimacy`                | number      | `[0,1]` | Required |
| `military_power`            | number      | `[0,1]` | Required |
| `threat_perception`         | number      | `[0,1]` | Required |

Missing fields, wrong types, non-finite numbers, and out-of-range
values all produce `Result::failure` with a message naming the
offending field. Error examples:

```
country.json: missing required field 'legal_tax_burden'
country.json: 'initial_gdp' must be >= 0 (got -1.000000)
country.json: 'corruption' must be in [0, 1] (got 1.500000)
country.json: 'central_control' must be in [0, 1] (got -0.100000)
```

The loader deliberately does **not** silently default missing fields
to 0 — M1.1's purpose is data quality.

## 3. Save format change: v1 → v2

M0.8 shipped `kSaveFormatVersion = 1`. M1.1 bumps it to `2`. The
country-array entries in a v2 save have the full M1.1 shape:

```json
{
  "id": 0,
  "id_code": "GER",
  "name": "Germany",
  "display_name": "Germany",
  "gdp": 100.0,
  "tax_revenue": 0.0,
  "budget_balance": 0.0,
  "legal_tax_burden": 0.20,
  "fiscal_capacity": 0.50,
  "administrative_efficiency": 0.55,
  "central_control": 0.60,
  "corruption": 0.25,
  "stability": 0.55,
  "legitimacy": 0.55,
  "military_power": 0.50,
  "threat_perception": 0.30
}
```

Field order is fixed and pinned by tests. Existing v1 saves now fail
to load with:

```
save.json: unsupported save_version 1 (this binary supports 2)
```

This is the intended behaviour from M0.8's design: bump on
incompatible change, fail loudly on old versions. No migration path
is provided (and intentionally so — no v1 saves exist in the wild,
since M0 was a technical skeleton).

The new regression test `deserialize: an old v1 save is rejected
loudly` pins this behaviour going forward.

### SaveSystem range validation

SaveSystem does **not** enforce `[0, 1]` ratio ranges or `gdp >= 0`
on load. It checks types only. The rationale: if a save was produced
by a correct DataLoader (which enforces ranges at config time) and
the simulation systems preserve invariants, the saved values will
always be in range. Range enforcement is a DataLoader concern
because that's where untrusted human-authored input enters the
system.

If we ever need run-time validation of save contents, the M0.10
`Diagnostics::sanity_check` is the natural extension point — it
already handles cross-entity invariants and returns `Issue`s rather
than panicking.

## 4. What M1.1 deliberately does NOT do

- **No simulation logic on the new fields.** Nothing reads
  `corruption` to compute tax leakage, nothing reads
  `military_power` to drive war outcomes. M1.5 (PolicySystem) is
  the earliest milestone that produces a real *effect*.
- **No new free functions.** No `apply_growth(state)`, no
  `update_tax_revenue(state)`, no `decay_stability(state)`.
- **No new entity types.** Faction expansion is M1.2; budget
  expansion is M1.3; policy data is M1.4.
- **No new diagnostics rules.** `sanity_check` could grow rules like
  "stability changed by more than X in one month" once economy
  ticks exist, but until effects exist there's nothing to flag.
- **No CSV columns added.** The summary CSV remains
  `date,country_count,log_count,seed`. Bigger columns require a
  schema-version bump on the CSV and a real consumer.
- **Runner doesn't gain country loading.** Still composed manually
  in the M0.11 integration test and in any M1+ tests that need
  populated states.

## 5. Migration impact

Files touched by the M1.1 PR:

- `include/leviathan/core/entities.hpp` — struct expansion
- `include/leviathan/systems/data_loader.hpp` — schema doc
- `src/leviathan/systems/data_loader.cpp` — new `require_ratio` /
  `require_nonneg_number` helpers; parse_country rewritten
- `include/leviathan/systems/save_system.hpp` — version bump
- `src/leviathan/systems/save_system.cpp` — country serialisation
- `data/countries/{germany,france,japan}.json` — new fields added
- `tests/systems/data_loader_test.cpp` — happy-path + new error
  paths (range, missing field)
- `tests/systems/save_system_test.cpp` — round-trip extended,
  inline JSON fixtures bumped to v2, new "v1 rejected" test
- `tests/integration/m0_end_to_end_test.cpp` — no test-code change;
  it reads countries via DataLoader and just propagates the new
  fields. Asserts on `id_code` / `name` still hold.

Total test count: **179 → 188** (9 new cases: 7 new error-path /
range tests in `data_loader_test`, 1 missing-field test in
`save_system_test`, 1 "v1 rejected" regression).

## 6. Risks / things to watch

- **DataLoader changes flow on to the M0.11 integration test.** All
  three country fixtures now MUST have every M1.1 field present and
  in range; the integration test loads them and bubbles up any
  parse failure. Adding a country file going forward needs to
  include the full M1.1 schema or the test will fail.
- **The `gdp` field is no longer named `initial_gdp` on the struct.**
  Any future code that reads `country.gdp` is reading the runtime
  value, not the original config value. If we ever need to know
  the original, a M1.x PR can add `initial_gdp` back as a separate
  immutable field. For now, `make_game_state` and DataLoader treat
  the loaded JSON config as the starting state directly.
- **Save format v2 is now load-bearing.** Any future PR that changes
  the country shape on disk must either (a) extend the schema in
  forward-compatible ways (new optional columns / fields), or
  (b) bump `kSaveFormatVersion` again to v3 and accept that v2
  saves cannot resume. The byte-identical determinism tests catch
  accidental shape changes.

## 7. Next sub-milestone

**M1.2 — `FactionState` real fields.** Same template: shape change,
loader / save updates, JSON fixtures, tests, design notes, no
behaviour. The recommended fields are listed in RFC-060 §3 (support,
influence, radicalism) plus the broader set in the M1 prompt
(loyalty, resources, preferred policies placeholder).

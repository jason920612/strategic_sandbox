# M1.14 - Diagnostics surfaces `last_gdp_growth_rate`

Companion notes for `feature/m1-14-diagnostics-surfaces-growth`. M1.14
makes the per-country runtime numerics (including the M1.12
`last_gdp_growth_rate`) inspectable from the headless runner output
without round-tripping the save file. **Opt-in**, **additive**, and
**preserves M0.10's byte-identical determinism contract** on the
existing summary CSV.

## 1. Scope

M1.12 added `last_gdp_growth_rate` to `CountryState`. Until now the
only way to see what the monthly pipeline produced for each country
was to deserialise the save file. M1.14 adds a per-country CSV output
format and a runner flag to opt in.

That's the whole PR.

## 2. Public API — `include/leviathan/systems/diagnostics.hpp`

```cpp
struct CountrySummaryRow {
    core::GameDate date;
    std::string id_code;              // e.g. "GER"
    double       gdp                  = 0.0;
    double       tax_revenue          = 0.0;
    double       budget_balance       = 0.0;
    double       stability            = 0.0;
    double       legitimacy           = 0.0;
    double       last_gdp_growth_rate = 0.0;   // the M1.14 motivator
};

core::Result<CountrySummaryRow> country_snapshot(const core::GameState& state,
                                                 core::CountryId country);

void write_country_csv_header(std::ostream& out);
void write_country_csv_row(std::ostream& out, const CountrySummaryRow& row);
```

Pure observation. `country_snapshot` returns failure for an invalid
`CountryId` index — failure path is reserved for misuse, since the
runner only ever calls it with indexes pulled from
`state.countries.size()`.

## 3. CSV format

Header (always exactly):
```
date,id_code,gdp,tax_revenue,budget_balance,stability,legitimacy,last_gdp_growth_rate
```

One data row per country per snapshot point. Snapshot cadence
**exactly mirrors** the M0.10 summary CSV cadence:

- 1× at the start (before any tick).
- 1× at each `TickResult.month_changed` boundary.
- 1× at the final post-sanity-check state.

For each snapshot point, the runner iterates `state.countries` in
vector order and emits one row per country. With N countries and S
snapshot points the file has `1 + N×S` lines (1 header + N×S data
rows).

Doubles are formatted with `std::scientific` + `std::setprecision(17)`
so the double → text → double round trip is exact and same-process
output is byte-identical (the strings are ugly for humans but
that's the priceof determinism — see §6).

## 4. Runner flag

```bash
leviathan --days 365 \
          --scenario data/scenarios/1930_minimal.json \
          --countries-csv out/countries.csv
```

- Optional; absent ⇒ no per-country file is written.
- Default unset (same default as `--summary-csv`).
- Independent of `--summary-csv`: either, both, or neither can be
  passed.
- The output path's parent directory is created if missing (same
  rule as `--save` / `--log` / `--summary-csv`).

`RunOutcome` gains:

```cpp
std::optional<std::filesystem::path> countries_csv_path;
std::size_t                          countries_csv_rows = 0;
```

`countries_csv_rows` counts data rows (header not included).

## 5. Snapshot cadence — why this exact pattern

Matches M0.10's existing summary-CSV cadence so the two files
align row-for-row:

- For a 31-day run from 1930-01-01: 1 start + 1 month boundary + 1
  final = **3 summary rows**. With 3 countries that's **9 data rows**
  in countries.csv (`3 snapshots × 3 countries`).
- For a 365-day run starting 1930-01-01: 1 start + 12 month
  boundaries + 1 final = 14 summary rows. With 3 countries that's
  42 data rows.

Pinned by the
`"run: --countries-csv + scenario emits one row per country per
snapshot point"` test.

## 6. Determinism

The new CSV is fully deterministic:

- `country_snapshot` is pure observation; same state → same row.
- `write_country_csv_row` uses `std::scientific` + precision 17 —
  same double value → same string.
- Snapshot cadence is driven by deterministic
  `TickResult.month_changed` flags, not wall time.
- The runner iterates `state.countries` in vector order (which is
  load order, also deterministic).

Pinned by two tests:

- `"run: --countries-csv preserves byte-identical determinism on
  same seed"` — runs the same scenario twice with the same seed and
  options, checks `countries.csv` is byte-identical.
- `"run: --countries-csv does NOT change --summary-csv output (M0.10
  contract)"` — two runs, one with `--countries-csv` set and one
  without, summary CSVs must be byte-identical. Regression for
  M0.10's contract.

## 7. State touched

- **`country_snapshot`**: reads
  `state.current_date`, `state.countries[i].{id_code, gdp,
  tax_revenue, budget_balance, stability, legitimacy,
  last_gdp_growth_rate}`. Writes nothing.
- **runner with `--countries-csv`**: adds an in-memory buffer of
  `CountrySummaryRow`s and writes one file at the end. Does NOT
  add new logs, does NOT advance RNG, does NOT mutate date.

Pinned by `country_snapshot: does NOT mutate state`.

## 8. Tests — 17 new cases (382 → 399)

### Diagnostics (9)

- `country_snapshot` reads every field verbatim.
- Invalid CountryId rejected with the bad index in the message.
- Default-constructed CountryId rejected.
- Empty `state.countries` rejects any index.
- Header is the exact 8-column string.
- Row is well-formed with 7 commas + 1 newline; starts with
  `<date>,<id_code>`; final column carries a fractional value.
- Negative `budget_balance` survives the format.
- Row is byte-identical for the same input row twice.
- `country_snapshot` does NOT mutate state.

### Runner (8)

- `--countries-csv` plumbed; missing value rejected; default unset.
- `run` without `--countries-csv` writes no file.
- Empty-state run with `--countries-csv` writes header-only file
  (no data rows).
- 31-day run + canonical scenario emits `3 snapshots × 3 countries
  = 9 data rows` containing every loaded `id_code` and a
  scientific-notation growth value.
- Same seed + same scenario + same days → byte-identical
  countries-CSV.
- Adding `--countries-csv` does NOT change `--summary-csv` output
  byte-for-byte (M0.10 contract preserved).

## 9. What M1.14 deliberately does NOT do

- **No new `SummaryRow` columns.** The 4-column summary CSV stays
  `date,country_count,log_count,seed`. M0.10's byte-identical
  determinism contract is preserved.
- **No new state shape on `CountryState`** / `FactionState` /
  `PolicyData`.
- **No save-format bump.** Stays at v6.
- **No new sanity-check rule.** `sanity_check` is unchanged.
- **No active-policy diagnostics.** That's M1.15 territory.
- **No faction-level CSV.** Only countries. A future per-faction CSV
  is a natural follow-up; M1.14 stays focused.
- **No AI / events / war / diplomacy / UI.**
- **No new logs.** The runner's log stream is unchanged.
- **No formula tuning / balance pass.**
- **No streaming I/O.** Rows are accumulated in memory and written
  once at the end (same pattern as M0.10's summary CSV).
- **No JSON-formatted variant of the per-country diagnostics.**
  CSV only.

## 10. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/systems/diagnostics.hpp` | adds `CountrySummaryRow` + 3 free functions |
| `src/leviathan/systems/diagnostics.cpp` | implements the 3 free functions + `fmt_double` helper |
| `include/leviathan/systems/runner.hpp` | adds `countries_csv_path` to RunnerOptions + RunOutcome; adds `countries_csv_rows` to RunOutcome |
| `src/leviathan/systems/runner.cpp` | `--countries-csv` flag parse, per-country snapshot buffer, mirrored cadence, file write |
| `tests/systems/diagnostics_test.cpp` | 9 new cases |
| `tests/systems/runner_test.cpp` | 8 new cases |
| `docs/m1-14-diagnostics-surfaces-growth.md` | this file |
| `docs/README.md`, `README.md`, `rfc/README.md` | M1.14 status / progress |

No CMake changes. No new fixture. Save format: still **v6**. Total
tests: **382 → 399** (+17).

## 11. Risks / things to watch

- **Memory cost grows with N countries × S snapshot points.** At
  N=3, S=14 (one year) that's 42 small structs — trivial. A future
  10-year multi-region scenario could buffer tens of thousands of
  rows. If that matters, switch to streaming write at each
  snapshot point; the file format is append-friendly so no schema
  change is needed.
- **`std::scientific` + precision 17 is ugly for humans.** Numbers
  like `0.0035` render as `3.50000…e-03`. Determinism beats
  prettiness here; a future "pretty mode" can be a separate
  feature flag (e.g. `--countries-csv-format human`).
- **Vector-order iteration relies on scenario-loader assignment
  order.** That order is currently deterministic (M1.11 assigns by
  manifest array index) but a future loader change must preserve
  it or the byte-identical contract breaks.
- **`runner.cpp` still has the hardcoded `2147483647`** in
  `parse_positive_int` (re-flagged since M1.10 / M1.11 / M1.12 /
  M1.13 reviews). Not blocking.

## 12. Next sub-milestone

Likely candidates (RFC-090 §M1):

- **M1.15 — policy duration tracking.** Add per-country active-
  policy list (`policy_id_code + expires_on`); TimeSystem removes
  expired entries; saves persist them. **Would require a
  save-format bump (`v6 → v7`).**
- **M1.16 — faction-level CSV.** Mirror M1.14 for factions:
  per-faction snapshot type + `--factions-csv PATH` flag. No save
  format change.

Per the M1 pacing rule: do **not** start the next sub-milestone
until M1.14 is reviewed and merged.

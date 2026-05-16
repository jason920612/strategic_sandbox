# M0.10 - Diagnostics output

Companion notes for `feature/m0-10-diagnostics`. Locks in the
summary-CSV format, the sanity-check rules, and the rule that
diagnostics is **observation only** - it never mutates `GameState`.

## 1. What lives in `systems::diagnostics`

```cpp
struct SummaryRow {
    core::GameDate date;
    std::size_t    country_count;
    std::size_t    log_count;
    std::uint64_t  seed;
};

SummaryRow         snapshot(const GameState&);
void               write_csv_header(std::ostream&);
void               write_csv_row(std::ostream&, const SummaryRow&);

enum class Severity { Error };
struct Issue { Severity severity; std::string code; std::string message; };
std::vector<Issue> sanity_check(const GameState&);
```

Both `snapshot` and `sanity_check` take `const GameState&`. They
**never** mutate state. If a check finds something wrong it RETURNS
an Issue; deciding what to do with it (log it, abort, ignore in
release) is the caller's job.

## 2. CSV format - byte stable

```
date,country_count,log_count,seed
1930-01-01,0,1,12345
1930-02-01,0,2,12345
...
1931-01-01,0,15,12345
```

- ASCII only, LF line endings (`\n`), no quoting (none of our fields
  ever contains a comma).
- Header is a fixed string. **Don't reorder columns** - downstream
  parsers and the byte-identical-output tests will break.
- `date` is ISO-8601 short form (`YYYY-MM-DD`), same as everywhere
  else in the project.
- `country_count` and `log_count` are decimal `size_t`.
- `seed` is decimal `uint64_t` (full range supported, see the
  uint64-boundary test).

If we ever need to add columns, **append on the right**. Downstream
readers should ignore extra columns past their schema.

## 3. Sanity checks

The current rules, in the order `sanity_check` emits them:

| `code`                | When fired |
|-----------------------|-------------|
| `invalid_date`        | `state.current_date.is_valid()` is false |
| `invalid_country_id`  | a `CountryState::id` is not valid (default-constructed or otherwise unset) |
| `duplicate_country_id`| two or more countries share the same numeric `id` |

`Severity` is currently `Error`-only. Warnings / Info are reserved
for later milestones if needed.

The list is **deliberately short**:

- **No "seed unset" check.** `seed == 0` is a perfectly valid
  intentional choice (e.g. for null-distribution tests). Flagging it
  would either be noise or would break the M0.9 byte-identical test
  on default-seed configs.
- **No "no countries" check.** The runner intentionally ticks an
  empty world in M0.9 / M0.10.

Each *distinct* duplicate id is reported once even if the id appears
3+ times - the second occurrence triggers, subsequent occurrences are
suppressed. This keeps the issue list usefully short when a save file
is severely broken.

## 4. Runner integration

`RunnerOptions` gains one optional field:

```cpp
std::optional<std::filesystem::path> summary_csv_path;
```

…wired to the new `--summary-csv PATH` CLI flag.

Snapshot cadence inside `run()`:

1. After the `simulation start` log -> **row 1** (start snapshot).
2. After every tick that flips `TickResult.month_changed` -> one row.
3. After the `simulation end` log AND after any sanity-check issues
   are appended to `state.logs` -> **last row** (post-sanity snapshot).

Snapshots are buffered in memory and written to disk as one CSV at
the end of the run. M0 scale is comfortable with this: a 365-day run
produces at most ~14 rows.

### Order matters

```
log "simulation end"
sanity_check(state)
for each issue: log_error("diagnostics", ..., {"code": iss.code})
snapshot (final row)
```

Sanity issues are logged BEFORE the final snapshot so the final row's
`log_count` reflects them. Tests that simulate broken state see the
issue count rise both in `events.jsonl` and in the CSV's last row.

## 5. Determinism

Two runs with the same `RunnerOptions` + same config file produce
**byte-identical** `summary.csv`. The determinism guarantee from M0.9
extends to this third output artefact:

- Snapshots only read deterministic fields (`current_date`, container
  sizes, `rng.seed`) - no wall-clock, no PID, no order-dependent map
  iteration.
- Sanity-check output is deterministic (set lookups + ordered loops
  over `state.countries`).
- Issue-to-log conversion uses path-independent metadata (`{code}`,
  not paths).

A dedicated test (`run: --summary-csv same-seed two-run CSV is
byte-identical`) pins this.

The M0.9 byte-identical save/log invariant **also** survives M0.10's
sanity-check integration - covered by a regression test:
`run: M0.9 byte-identical guarantee survives sanity-check integration`.

## 6. What's NOT in scope for M0.10

- **Schema versioning for the CSV.** No `schema_version` column.
  Append-only column policy + the byte-identical test together cover
  the common cases. If we ever need readers to support multiple
  schema versions we will revisit.
- **Streaming CSV writes.** The current implementation buffers rows in
  memory and writes once at the end of the run. Streaming would
  allow inspection mid-run but complicates error handling.
- **Sanity-check severity beyond Error.** Warnings / Info will land
  when there's a real consumer. M0.10 only needs Error.
- **`--summary-every-days N`.** Month-boundary cadence + start + end
  is enough for the M0.10 spec. Adding a knob without a caller would
  be premature.
- **Sanity-check on file load (M0.8 SaveSystem).** SaveSystem already
  rejects malformed saves at the structural level (missing fields,
  wrong types, version mismatch). Cross-field invariants (duplicate
  IDs in a save) could be run by SaveSystem, but for now the runner
  surfaces them at end-of-run, which is enough.

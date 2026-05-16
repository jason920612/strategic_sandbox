# M1.16 - Faction-level diagnostics CSV

Companion notes for `feature/m1-16-faction-csv`. M1.16 adds a
per-faction CSV output mirroring the M1.14 per-country pattern: the
runner now accepts `--factions-csv PATH`, snapshots every faction at
the same cadence as `--summary-csv` and `--countries-csv`, and writes
the buffered rows once at the end. **Opt-in**, **additive**, and
**preserves every existing byte-identical determinism contract**
(M0.10 summary CSV, M1.14 country CSV).

## 1. Scope

M1.6 introduced `FactionState` runtime numerics (support / influence /
radicalism / loyalty / resources) and M1.6's `faction::react` started
mutating them. Until M1.16 the only way to observe per-faction
trajectories across a multi-month run was to round-trip the save file.
M1.16 ships the missing diagnostic surface.

That's the whole PR. Same shape as M1.14, just for factions.

## 2. Public API — `include/leviathan/systems/diagnostics.hpp`

```cpp
struct FactionSummaryRow {
    core::GameDate date;
    std::string id_code;          // e.g. "GER_military"
    std::string country_id_code;  // e.g. "GER"
    std::string type;             // e.g. "military"
    double      support    = 0.0;
    double      influence  = 0.0;
    double      radicalism = 0.0;
    double      loyalty    = 0.0;
    double      resources  = 0.0;
};

core::Result<FactionSummaryRow> faction_snapshot(const core::GameState& state,
                                                 core::FactionId faction);

void write_faction_csv_header(std::ostream& out);
void write_faction_csv_row(std::ostream& out, const FactionSummaryRow& row);
```

Pure observation. `faction_snapshot` returns failure for an invalid
`FactionId` index — same misuse-only failure model as
`country_snapshot`.

We deliberately denormalise `country_id_code` and `type` into the row.
External tooling can group rows by `country_id_code` ("show me every
GER faction over time") or by `type` ("show me every military faction
over time") without re-joining against another file. Two strings per
row is cheap; the per-snapshot row count is bounded by faction count,
which never exceeds a few dozen.

## 3. CSV format

Header (always exactly):
```
date,id_code,country_id_code,type,support,influence,radicalism,loyalty,resources
```

One data row per faction per snapshot point. Snapshot cadence
**exactly mirrors** the M0.10 / M1.14 cadence:

- 1× at the start (before any tick).
- 1× at each `TickResult.month_changed` boundary.
- 1× at the final post-sanity-check state.

For each snapshot point, the runner iterates `state.factions` in
vector order and emits one row per faction. With F factions and S
snapshot points, the file contains `F × S` data rows after the
single header line.

Doubles are written via `std::scientific` + `std::setprecision(17)`
(same `fmt_double` helper used by `write_country_csv_row`) so the
round-trip `double → text → double` is exact and same-state always
produces byte-identical text.

## 4. Runner integration — `--factions-csv PATH`

`RunnerOptions` gains:

```cpp
std::optional<std::filesystem::path> factions_csv_path;
```

When unset (the default), no faction CSV is written. When set:

- `run_state` allocates a `std::vector<FactionSummaryRow>` buffer
  alongside the existing summary / countries buffers.
- A `snapshot_all_factions(state)` helper mirrors
  `snapshot_all_countries`. The same three call sites populate it
  (start, each `month_changed`, final post-sanity).
- After the tick loop, the buffer is rendered through
  `write_faction_csv_header` + `write_faction_csv_row` and written
  to disk via the existing `write_string_to_file` helper.

`RunOutcome` gains `factions_csv_path` and `factions_csv_rows`. The
row counter excludes the header.

`main()` prints both the per-country and per-faction CSV paths +
row counts when set (the per-country print line is a drive-by
cleanup — M1.14 added the data but never wired it into the stdout
summary).

## 5. Tests

18 new doctest cases (M1.15 was 414 → M1.16 is 432).

`tests/systems/diagnostics_test.cpp` (9 cases):

- `faction_snapshot` reads every documented field verbatim
- invalid `FactionId` rejected with the bad index in the message
- default-constructed `FactionId` rejected
- empty `state.factions` → any index rejected
- `write_faction_csv_header` emits the canonical 9-column header
- `write_faction_csv_row` emits a well-formed line with 8 commas /
  9 columns, ends with `\n`, no trailing comma
- negative `resources` survives the formatter (`-2.5...` substring)
- byte-identical for the same row twice
- `faction_snapshot` does not mutate state

`tests/systems/runner_test.cpp` (9 cases, 3 parse-only + 6 wrapped
in `LEVIATHAN_TEST_DATA_DIR`):

- `--factions-csv` flag plumbed through to `RunnerOptions`
- `--factions-csv` with no value is rejected
- `--factions-csv` defaults to unset when absent
- without `--factions-csv` no file is written, `factions_csv_rows == 0`
- with `--factions-csv` on an empty state, file contains only the
  header
- canonical scenario 31-day run produces
  `9 rows = 3 factions × 3 snapshots`
- same-seed determinism: two runs produce byte-identical faction CSV
- `--factions-csv` does NOT change the `--summary-csv` byte output
  (M0.10 contract regression)
- `--factions-csv` does NOT change the `--countries-csv` byte output
  (M1.14 contract regression)

## 6. What's NOT in scope

Deliberate non-goals:

- **No save-format change.** The save schema stays at v7 (M1.15).
- **No new `FactionState` / `CountryState` / `PolicyData` shape
  change.** CSV reads what already exists.
- **No new sanity checks.** `sanity_check` is untouched; faction
  invariants (e.g. country reference points at a real country) are
  not enforced here.
- **No JSON variant.** Same as M1.14 — if a `.json` variant is ever
  wanted it lives in a later sub-milestone.
- **No streaming I/O.** Rows accumulate in memory; flush happens
  once at the end. With at most a few dozen factions × a few hundred
  snapshots per long run, memory is not a concern.
- **No per-country aggregation.** Tooling can group rows by
  `country_id_code` if it wants to.
- **No balance pass / AI / events / war / monthly pipeline change.**

## 7. Cross-links

- M0.10 (`m0-10-diagnostics.md`) — `SummaryRow` + `write_csv_*`
  helpers + the `sanity_check` framework that M1.16 leaves alone.
- M1.2 (`m1-2-faction-state.md`) — the `FactionState` runtime fields
  M1.16 reads.
- M1.6 (`m1-6-faction-reactions.md`) — first system that mutates
  faction support / loyalty; M1.16 makes those mutations
  observable across a multi-month run.
- M1.14 (`m1-14-diagnostics-surfaces-growth.md`) — direct template
  for this PR (per-country shape, snapshot cadence, double-format
  rule).

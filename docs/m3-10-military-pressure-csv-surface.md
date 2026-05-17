# M3.10 - InterestGroup `military_pressure` outcome CSV surface

Companion notes for `feature/m3-10-military-pressure-csv`.

M3.10 retrofits the M3.6 trace-CSV pattern onto M3.9. The
M3.9 PR landed the `military_pressure` system and surfaced
its trace vector through `MonthlyOutcome` but deliberately
did not write a new artefact file. M3.10 wires that vector
into a 9th unconditional runner artefact —
`interest_group_military_pressure.csv` — so the M3.9 outcomes
become observable on disk without re-running the simulation.

No new gameplay. No formula change to any M3.X system. Save
schema unchanged (still v11). No new CLI flag.

## 1. Scope

After M3.9 the M3 reaction-loop reverse direction had two
mutating systems each with an in-memory trace vector but only
TWO of them on disk:

| System | Lands on disk via |
|---|---|
| M3.3 `country_feedback` | `interest_group_country_feedback.csv` (M3.6) |
| M3.4 `authority_pressure` | `interest_group_authority_pressure.csv` (M3.6) |
| M3.9 `military_pressure` | (nothing — trace vector ephemeral) |

M3.10 closes the asymmetry. The artefact set grows from 8
files to 9. The artefact write order in `end_tick` becomes:

```text
save.json
events.jsonl
summary.csv                              (when --summary-csv set)
countries.csv                            (when --countries-csv set)
factions.csv                             (when --factions-csv set)
interest_groups.csv                      (always; M3.5)
interest_group_country_feedback.csv      (always; M3.6)
interest_group_authority_pressure.csv    (always; M3.6)
interest_group_military_pressure.csv     (always; M3.10)
```

This deliberately mirrors how M3.6 closed the M3.4 → trace-
CSV gap after M3.4 originally shipped without one.

## 2. CSV format

```text
date,country_id,country_id_code,matched_groups,weight_sum,
weighted_military_loyalty,target_military_loyalty,
military_loyalty_before,military_loyalty_after,
military_loyalty_delta
```

Ten columns. Column names mirror `MilitaryPressureTraceRow`
field names so the CSV reader's mental model matches the
source struct. `country_id_code` runs through `csv_escape`
(RFC 4180) for the same defensive safety-net reason as the
M3.6 writers. Doubles use `std::scientific` +
`setprecision(17)` for byte-exact round-trip.

`target_military_loyalty` equals `weighted_military_loyalty`
by definition (the M3.9 formula assigns one from the other).
Both surfaced so a future formula extension can diverge
them without renaming columns.

## 3. Cadence

One row per actually-mutated country per monthly pipeline
tick — the same emission rule as the M3.6 CSVs. Skipped
countries (no Military group, zero total Military influence)
produce no row. No row is emitted at `begin_tick` or
`end_tick` because the trace files record real mutations,
not state snapshots.

Canonical scenarios author zero interest groups, so the new
file is header-only for `--scenario data/scenarios/1930_minimal.json`
and the like. Empty `state.interest_groups` still produces a
header-only file (the artefact set stays constant).

## 4. Public API

`include/leviathan/systems/diagnostics.hpp`:

```cpp
namespace leviathan::systems::diagnostics {

void write_military_pressure_csv_header(std::ostream& out);
void write_military_pressure_csv_row(
    std::ostream& out,
    const interest_group::MilitaryPressureTraceRow& row);

}  // namespace
```

`include/leviathan/systems/runner.hpp`:

```cpp
struct RunnerOptions {
    // ...
    std::optional<std::filesystem::path>
        interest_group_military_pressure_csv_path;
};

struct RunOutcome {
    // ...
    std::filesystem::path  interest_group_military_pressure_csv_path;
    std::size_t            interest_group_military_pressure_csv_rows = 0;
};

struct TickController {
    // ...
    std::vector<interest_group::MilitaryPressureTraceRow>
        interest_group_military_pressure_rows;
};
```

No CLI flag. `RunnerOptions::interest_group_military_pressure_csv_path`
is reachable programmatically only; the default path is
`<output_dir>/interest_group_military_pressure.csv`.

## 5. Runner integration

`step_one_day` drains the M3.9 trace vector from each
`MonthlyOutcome` into the controller alongside the existing
M3.3 / M3.4 vectors:

```cpp
for (auto& row : outcome.interest_group_military_pressure_trace_rows) {
    ctrl.interest_group_military_pressure_rows.push_back(std::move(row));
}
```

`end_tick` writes the buffer to `interest_group_military_pressure.csv`
unconditionally as the 9th artefact, after the M3.6
`interest_group_authority_pressure.csv` write.

`main()` prints the new artefact path + row count next to
the existing M3.6 lines.

### M2.9 contract extension

The pre-`end_tick` no-artefact contract automatically
extends from 8 files to 9 because `end_tick` is still the
only function on the runner side that writes to disk. The
`runner_test.cpp` `ArtifactPaths` /
`wire_all_artifacts` / `check_no_artifacts` helpers grow
the new `military_pressure_csv` field; every existing M2.9
regression test inherits the extended assertion.

Mid-`end_tick` atomicity is still NOT a goal. The nine
writes happen sequentially; a disk failure between files
can leave a partial set. Deliberately deferred from
M2.9 / M3.5 / M3.6 / M3.10.

## 6. Determinism

The M1.17 / M2.22 / M3.7 byte-identical determinism tests
extend from 8 to 9 artefacts. Canonical scenarios author
zero interest groups, so the four M3 files are all
header-only in those tests; the comparison still pins
byte-for-byte equality between two same-seed runs.

A new `run: M3.10 military_pressure CSV preserves
byte-identical determinism on same seed` runner test
independently pins the M3.10 contract.

## 7. Save schema

**Unchanged: v11.** Runtime artefact output only. Trace
rows live in `MonthlyOutcome` for the duration of one
month-boundary then drain into `TickController` buffers
before `end_tick` flushes them to disk. Nothing reaches
`GameState`.

## 8. What is NOT in scope

- no save schema bump;
- no new state fields;
- no formula change to any M3.X system;
- no new `InterestGroupKind` variants;
- no new gameplay, event triggers, AI, UI, REPL;
- no new CLI flag (programmatic path override only);
- no new `PlayerCommandKind`;
- no command-gate integration;
- no `intelligence_capability` / `media_control` sibling
  channels (each its own future PR);
- no atomic `end_tick` writes;
- no `--target-date` interaction beyond existing replay
  flow.

Drive-by: stripped the lone `[[feedback_pr_workflow]]`
wiki-link from `docs/m3-9-interest-group-military-pressure.md`
because that syntax is a Claude memory-file convention,
not a repo-doc convention. Flagged by the PR #58 reviewer
as a non-blocker.

## 9. Test surface

10 new doctest cases:

`diagnostics_test.cpp` (4 cases):
- `write_military_pressure_csv_header`: documented column
  list pinned.
- `write_military_pressure_csv_row`: shape (prefix +
  comma count + scientific notation).
- `write_military_pressure_csv_row`: `country_id_code`
  with comma is RFC-4180 quoted.
- `write_military_pressure_csv_row`: byte-identical
  re-emit.

`runner_test.cpp` (M3.10 section, 5 cases):
- empty world → header-only file.
- default path resolution.
- path override honoured.
- 31-day run with one Military group → exactly one data
  row; row contains `GER` and scientific-notation
  numerics.
- byte-identical determinism on same seed.
- M3.10 writer does NOT change pre-M3.10 byte streams
  (M0.10 / M1.14 / M1.16 / M3.5 / M3.6 contracts all
  preserved).

Two integration tests extended:
- `m1_end_to_end_test.cpp` and `m2_end_to_end_test.cpp`
  both add `interest_group_military_pressure.csv` to
  their byte-identical determinism gates (8 → 9
  artefacts).
- `m3_end_to_end_test.cpp` Test B's artefact-existence
  + data-row assertions grew from 8 to 9 (the helper
  state already had a Military group from M3.9, so
  the new CSV gets a row on the month boundary).
- `m3_end_to_end_test.cpp` Test C compares 9 files
  byte-identical.

`runner_test.cpp` `ArtifactPaths` /
`wire_all_artifacts` / `check_no_artifacts` helpers
grew the 9th `military_pressure_csv` field, so every
existing M2.9 regression test now checks 9 absent
paths instead of 8.

Total: 776 doctest cases on this branch (+10 vs main's
766).

## 10. Future M3.11+ candidates (none committed)

- `intelligence_capability` sibling channel (kind TBD —
  Bureaucracy subset? new Intelligence kind?).
- `media_control` sibling channel (Media-kind loyalty →
  media_control).
- Interest-group integration into the M2.18 / M2.19
  command-execution gate as an additional input beyond
  the already-drifting `bureaucratic_compliance`.
- Influence drift driven by event / policy outcomes.
- M3.2 `react` per-mutation trace as a third trace CSV
  (would grow the artefact set to 10).
- Atomic `end_tick` writes (temp-file + rename).

Per the M-pacing rule, the next sub-milestone starts
only when the reviewer's approval message names a
direction or defaults to the top of this list.

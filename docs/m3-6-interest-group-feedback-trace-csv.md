# M3.6 - InterestGroup feedback outcome diagnostics / CSV trace surface

Companion notes for `feature/m3-06-interest-group-feedback-trace-csv`.

M3.6 is the **outcome trace** complement to M3.5's state
surface. M3.5 dumped what the interest-group POD currently
looks like; M3.6 dumps what the M3.3 / M3.4 reverse-direction
systems just did to country state, every time they do it. No
gameplay, no formula changes, no save-schema work.

## 1. Scope

After M3.5 the artefact set was six files:

```text
save.json
events.jsonl
summary.csv            (opt-in)
countries.csv          (opt-in)
factions.csv           (opt-in)
interest_groups.csv    (unconditional)
```

M3.6 adds two more, both unconditional:

```text
interest_group_country_feedback.csv      (M3.3 outcome trace)
interest_group_authority_pressure.csv    (M3.4 outcome trace)
```

Total now eight files. The two new CSVs follow the M3.5 pattern:
no `--interest-group-*-csv` CLI flag, programmatic path overrides
on `RunnerOptions`, default to `<output_dir>/<name>.csv`, header
appears even when zero rows are emitted.

### Why state and outcome are separate files

`interest_groups.csv` (M3.5) snapshots state at fixed cadence
points (start / each `month_changed` / final post-sanity). It
answers "what does this group look like *now*?".

The two M3.6 traces answer "what did the formula do *this
month*?". The questions look similar but they are not
interchangeable:

- The state CSV cares about totals, not deltas. The trace
  CSVs care about deltas, not totals.
- The state CSV is emitted at three snapshot points per run
  (and any monthly boundaries between them). The trace CSVs
  are emitted only when the corresponding system actually
  mutated a country.
- Mixing them in one file would force one of the two
  semantics on the other, and a downstream script would have
  to filter rows by column presence — a worse experience
  than two well-shaped files.

## 2. Cadence

The trace files are **not** state snapshots. They record one
row per actual mutation. Concretely:

- `interest_group_country_feedback.csv` — one row per country
  whose `stability` was actually drifted by
  `interest_group::country_feedback`. Skipped countries (no
  matching groups; `weight_sum <= 0`) produce no row.
- `interest_group_authority_pressure.csv` — one row per
  country whose
  `government_authority.bureaucratic_compliance` was actually
  drifted by `interest_group::authority_pressure`. Same skip
  rules (no Bureaucracy group; zero total influence).

Row counts therefore equal `CountryFeedbackOutcome::countries_updated`
and `AuthorityPressureOutcome::countries_updated` respectively
on each monthly tick, and the run total is the sum over every
month boundary the run crossed.

No `begin_tick` or `end_tick` snapshot row is emitted into
either trace file. That is deliberate: a synthetic row at
those moments would have no real before/after pair.

## 3. Public API

`include/leviathan/systems/interest_group_system.hpp`:

```cpp
namespace leviathan::systems::interest_group {

struct CountryFeedbackTraceRow {
    core::GameDate date;
    int            country_id      = -1;
    std::string    country_id_code;
    int            matched_groups  = 0;
    double         weight_sum             = 0.0;
    double         weighted_radicalism    = 0.0;
    double         target_stability       = 0.0;
    double         stability_before       = 0.0;
    double         stability_after        = 0.0;  // post-clamp
    double         stability_delta        = 0.0;  // after - before
};

struct AuthorityPressureTraceRow {
    core::GameDate date;
    int            country_id      = -1;
    std::string    country_id_code;
    int            matched_groups  = 0;
    double         weight_sum                       = 0.0;
    double         weighted_bureaucracy_loyalty     = 0.0;
    double         target_bureaucratic_compliance   = 0.0;
    double         bureaucratic_compliance_before   = 0.0;
    double         bureaucratic_compliance_after    = 0.0;  // post-clamp
    double         bureaucratic_compliance_delta    = 0.0;
};

core::Result<CountryFeedbackOutcome> country_feedback(
    core::GameState& state,
    std::vector<CountryFeedbackTraceRow>* trace_out = nullptr);

core::Result<AuthorityPressureOutcome> authority_pressure(
    core::GameState& state,
    std::vector<AuthorityPressureTraceRow>* trace_out = nullptr);

}  // namespace
```

### Trace pointer semantics

- Default-null. Pre-M3.6 callers get byte-identical behaviour:
  same formula, same mutation, same `countries_updated`.
- When non-null, the function **appends** one row per
  country it actually updates. Skipped countries produce no
  row. Preflight failure produces no row at all (rows are
  only emitted during the apply phase, which runs strictly
  after preflight).
- The row is recorded after the clamp so `*_after` reflects
  what the rest of the simulation will read this month.
- The pointer never changes the formula or the
  `*Outcome::countries_updated` counter.

## 4. CSV formats

`interest_group_country_feedback.csv`:

```text
date,country_id,country_id_code,matched_groups,weight_sum,
weighted_radicalism,target_stability,stability_before,
stability_after,stability_delta
```

`interest_group_authority_pressure.csv`:

```text
date,country_id,country_id_code,matched_groups,weight_sum,
weighted_bureaucracy_loyalty,target_bureaucratic_compliance,
bureaucratic_compliance_before,bureaucratic_compliance_after,
bureaucratic_compliance_delta
```

Doubles use `std::scientific` + `setprecision(17)` (M1.14 /
M1.16 / M3.5 convention). `country_id_code` runs through the
M3.5 `csv_escape` helper (RFC 4180); the canonical scenarios
use plain ASCII id_codes so escaping is a safety net rather
than a regular event.

`target_bureaucratic_compliance` equals
`weighted_bureaucracy_loyalty` by definition (the M3.4 formula
sets one from the other). Surfacing both keeps the column
header readable for downstream consumers and lets a future
formula extension diverge them without renaming columns.

## 5. Monthly pipeline integration

`MonthlyOutcome` gains two trace vectors:

```cpp
struct MonthlyOutcome {
    // ... existing fields ...
    std::vector<interest_group::CountryFeedbackTraceRow>
        interest_group_country_feedback_trace_rows;
    std::vector<interest_group::AuthorityPressureTraceRow>
        interest_group_authority_pressure_trace_rows;
};
```

`monthly::tick_all_countries` passes pointers to those vectors
into the two systems. Empty vectors after a successful tick mean
"no country was actually updated this month" (canonical
scenarios with zero interest groups, or scenarios where every
country was skipped because its groups had zero influence).

The trace vectors are **not** persisted: they live inside the
ephemeral `MonthlyOutcome` and get drained into the runner's
`TickController` buffers before the outcome is discarded.

## 6. Runner integration

`TickController`:

```cpp
std::vector<interest_group::CountryFeedbackTraceRow>
    interest_group_country_feedback_rows;
std::vector<interest_group::AuthorityPressureTraceRow>
    interest_group_authority_pressure_rows;
```

`step_one_day`: when the monthly pipeline succeeds, move-append
both trace vectors from the outcome into the controller. (Move
is intentional — the outcome is local and discarded after
the boundary.)

`RunnerOptions` adds two optional path overrides:

```cpp
std::optional<std::filesystem::path>
    interest_group_country_feedback_csv_path;
std::optional<std::filesystem::path>
    interest_group_authority_pressure_csv_path;
```

Defaults:

```text
<output_dir>/interest_group_country_feedback.csv
<output_dir>/interest_group_authority_pressure.csv
```

No CLI flag. Programmatic access only.

`RunOutcome` gains the resolved paths plus row counts:

```cpp
std::filesystem::path interest_group_country_feedback_csv_path;
std::size_t           interest_group_country_feedback_csv_rows = 0;
std::filesystem::path interest_group_authority_pressure_csv_path;
std::size_t           interest_group_authority_pressure_csv_rows = 0;
```

`end_tick` writes both files unconditionally as the 7th and 8th
artefacts, in this order:

```text
save.json
events.jsonl
summary.csv                              (when --summary-csv set)
countries.csv                            (when --countries-csv set)
factions.csv                             (when --factions-csv set)
interest_groups.csv                      (always; M3.5)
interest_group_country_feedback.csv      (always; M3.6)
interest_group_authority_pressure.csv    (always; M3.6)
```

### M2.9 contract extension

The pre-`end_tick` no-artefact contract automatically extends
to the two new files because `end_tick` is still the only
function on the runner side that writes to disk. The
`runner_test.cpp` `ArtifactPaths` / `wire_all_artifacts` /
`check_no_artifacts` helpers grow two new fields each so every
M2.9 regression test continues to assert "no artefact on
failure" against the full set.

Mid-`end_tick` atomicity is still NOT a goal. The eight writes
remain sequential; an I/O failure between files can leave a
partial set on disk. Deliberately deferred from M2.9 and
M3.5, deliberately deferred from M3.6 as well.

## 7. Save schema

**Unchanged: v11.** Runtime artefact output only. The trace
vectors are not persisted anywhere — they live in the local
`MonthlyOutcome` for the duration of one boundary, get
drained into the runner's `TickController` buffers, and end
up on disk as CSV. Nothing reaches `state.applied_commands` /
`state.logs` / save.json.

## 8. Determinism

The M1.17 / M2.22 byte-identical determinism tests now check
**eight** artefacts instead of six. Canonical scenarios author
zero interest groups so the three M3 files are all header-only
in those tests; the comparison still pins exact byte-for-byte
equality between two same-seed runs.

A new runner test pins byte-identical equality of both M3.6
trace CSVs on the same seed independently of the integration
tests.

## 9. What is NOT in scope

Explicit non-goals so the next sub-milestone can pick from a
clean list:

- no save schema bump (still v11);
- no new state fields;
- no new `InterestGroupKind` variants;
- no formula changes to M3.2 / M3.3 / M3.4;
- no new gameplay, event triggers, AI, UI, REPL;
- no new CLI flag (programmatic path overrides only);
- no new `PlayerCommandKind`;
- no command-gate integration;
- no per-tick state delta CSV (still just outcome traces of
  M3.3 / M3.4);
- no formula trace for M3.2 `react` (M3.2 mutates every
  group every month so a "per-mutation" trace would
  effectively duplicate `interest_groups.csv`);
- no atomic `end_tick` writes (sequential; gap inherited
  from M2.9 / M3.5);
- no `--target-date` interaction beyond existing replay flow;
- no cross-border behaviour;
- no automatic interest-group generation;
- no per-kind balancing changes.

## 10. Test surface

24 new doctest cases:

`interest_group_system_test.cpp` (10 cases):
- `country_feedback` null-pointer baseline preserved;
- `country_feedback` non-null pointer emits one row per
  updated country with exact formula numerics;
- `country_feedback` skipped country produces no row;
- `country_feedback` preflight failure produces no partial
  rows (and no mutation);
- `country_feedback` row order = country iteration order;
- `authority_pressure` null-pointer baseline preserved;
- `authority_pressure` non-null pointer emits one row per
  updated country with exact formula numerics;
- `authority_pressure` skipped country (non-Bureaucracy
  group) produces no row;
- `authority_pressure` preflight failure produces no partial
  rows;
- (one shared helper).

`diagnostics_test.cpp` (7 cases): both headers pinned; row
shape + comma count for both writers; comma-in-`country_id_code`
quoted; byte-identical re-emit for both writers.

`runner_test.cpp` (5 cases): empty-world → both CSVs
header-only; default-path resolution; path overrides; hand-
built groups produce rows after a month boundary; byte-
identical determinism on same seed; M3.6 writers do not
perturb the six pre-M3.6 byte streams.

`monthly_pipeline_test.cpp` (2 cases): trace rows surface in
`MonthlyOutcome` after a real tick; empty trace vectors when
no interest groups exist.

Two existing integration tests (M1.17, M2.22) extended to
compare both new CSVs as the 7th and 8th byte-identical
artefacts.

## 11. Future M3.7+ candidates (none committed)

- `interest_group::react` (M3.2) per-mutation trace — gated on
  whether the user wants per-group rows or a single per-call
  row;
- atomic `end_tick` writes (temp-file + rename);
- formula intermediates for the M2.18 / M2.19 command-execution
  gate (downstream consumer of M3.4 authority pressure);
- structured diff CLI between two trace CSVs;
- per-kind aggregate CSV summarising contributions to each
  reverse-direction system.

Per the M-pacing rule (see [[feedback_pr_workflow]]), M3.7
waits for an explicit reviewer spec.

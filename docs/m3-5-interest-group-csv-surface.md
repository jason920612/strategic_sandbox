# M3.5 - InterestGroup reaction diagnostics / CSV surface

Companion notes for `feature/m3-05-interest-group-csv-surface`.

M3.5 does not add gameplay. It surfaces the M3.1–M3.4 interest-
group state and reaction results as a stable, observable artefact
so future regressions land on a CSV diff instead of in a save.json
hex-dump. Naming and column shape mirror the M1.14 / M1.16
per-country / per-faction CSVs so external tooling that already
parses those can extend trivially.

## 1. Scope

After M3.4 the interest-group reaction loop is fully wired
(group ↔ country, three directions). The only thing missing was a
way to inspect the loop *without* loading a binary save and
without re-running the simulation.

M3.5 adds exactly one artefact: `interest_groups.csv`, written by
the runner on the same cadence as the existing CSVs (start +
each `month_changed` + final post-sanity). Unlike the M0.10 /
M1.14 / M1.16 CSVs, this one is **unconditional** — the runner
always writes it, header-only when `state.interest_groups` is
empty. The artefact set is therefore now six files:
`save.json`, `events.jsonl`, `summary.csv`, `countries.csv`,
`factions.csv`, `interest_groups.csv`.

### Why unconditional

Per-country (M1.14) and per-faction (M1.16) CSVs were opt-in via
a `--countries-csv` / `--factions-csv` flag. M3.5 deliberately
breaks that pattern: there is **no `--interest-groups-csv` flag**.
Reasons:

- Interest groups are the central M3 surface. We want every run
  to drop the file so future bug reports always contain it.
- Empty scenarios still produce a header-only file. The artefact
  set is constant, not "5 sometimes / 6 sometimes".
- A programmatic path override (`RunnerOptions::interest_groups_csv_path`)
  still exists for tests that want to redirect the output.

## 2. Public API

`include/leviathan/systems/diagnostics.hpp`:

```cpp
namespace leviathan::systems::diagnostics {

struct InterestGroupSummaryRow {
    core::GameDate date;
    std::string    id_code;           // e.g. "ger_bureaucracy"
    std::string    name;              // e.g. "German Bureaucracy"
    std::string    kind;              // canonical enum spelling
    int            country_id      = -1;   // CountryId::value()
    std::string    country_id_code;        // owning country's id_code
    double         influence       = 0.0;
    double         loyalty         = 0.0;
    double         radicalism      = 0.0;
};

core::Result<InterestGroupSummaryRow> interest_group_snapshot(
    const core::GameState& state, std::size_t group_index);

void write_interest_group_csv_header(std::ostream& out);
void write_interest_group_csv_row(std::ostream& out,
                                  const InterestGroupSummaryRow& row);

std::string csv_escape(std::string_view field);   // RFC 4180

}  // namespace
```

### CSV format

```text
date,id_code,name,kind,country_id,country_id_code,influence,loyalty,radicalism
```

- `date` — `state.current_date` at snapshot time, in
  `YYYY-MM-DD`.
- `id_code` — `InterestGroupState::id_code`.
- `name` — `InterestGroupState::name`. RFC-4180 quoted /
  escaped when it contains `,`, `"`, `\n`, or `\r`.
- `kind` — canonical enum spelling from
  `core::interest_group_kind_to_string` (`Bureaucracy`,
  `Military`, etc.).
- `country_id` — raw `CountryId::value()`. Negative or
  out-of-range values reject at snapshot time.
- `country_id_code` — the owning country's
  `CountryState::id_code`, looked up from `country_id`.
- `influence` / `loyalty` / `radicalism` — three M3.1 ratios,
  formatted with `std::scientific` + `setprecision(17)` for
  byte-exact round-trip.

Row order is `state.interest_groups` vector order. No sorting,
no grouping — save / scenario_loader / diagnostics all read this
vector in index order, so the CSV stays diff-stable.

### What this file does NOT contain

Intentionally absent from M3.5:

- formula intermediates (`weighted_loyalty`,
  `target_compliance`, `feedback_delta`, `authority_delta`);
- per-tick deltas;
- aggregate columns (per-country group counts, per-kind sums);
- the gate decision history.

These are diagnostics on the *outcome* of the M3.2–M3.4 systems
and belong in a future "diagnostics outcome surface" PR. M3.5
is purely a state-surface snapshot.

## 3. Runner / artefact contract

`RunnerOptions::interest_groups_csv_path` is a new
`std::optional<std::filesystem::path>` defaulting to
`<output_dir>/interest_groups.csv`. There is no
`parse_args` flag — the field is reachable programmatically only.

`RunOutcome` gains:

```cpp
std::filesystem::path interest_groups_csv_path;        // always set
std::size_t           interest_groups_csv_rows = 0;
```

`TickController` gains
`std::vector<diagnostics::InterestGroupSummaryRow> interest_group_rows`.
`begin_tick` / `step_one_day` / `end_tick` snapshot interest
groups on every snapshot point regardless of any opt-in
flag.

Write order inside `end_tick`:

```text
save.json
events.jsonl
summary.csv          (when --summary-csv set)
countries.csv        (when --countries-csv set)
factions.csv         (when --factions-csv set)
interest_groups.csv  (always)
```

### M2.9 contract extends naturally

Every replay-mode failure path the M2.9 doc lists returns
**before** `end_tick`, and `end_tick` is still the only function
that touches disk. With M3.5, that "no artefact survives a
pre-`end_tick` failure" guarantee now covers six files instead of
five. The `runner_test.cpp` `wire_all_artifacts` /
`check_no_artifacts` helpers grow the new field.

Mid-`end_tick` atomicity is still NOT a goal (the writes happen
sequentially; a disk failure between files leaves a partial set).
That gap was deliberately deferred from M2.9 and is also
deliberately deferred from M3.5.

## 4. Shared kind ↔ string helper

Before M3.5 the `InterestGroupKind` ↔ string mapping was
duplicated in `save_system.cpp` and `scenario_loader.cpp` with
a comment justifying the duplication (avoiding `scenario_loader
→ save_system` layering inversion).

M3.5 extracts the helper to a new core header:

- `include/leviathan/core/interest_group_kind.hpp`
- `src/leviathan/core/interest_group_kind.cpp`

Both functions live in `leviathan::core`, so save / scenario /
diagnostics all depend on the same single source of truth.
Neither layer is inverted (core is below all three). Adding a
new `InterestGroupKind` variant now requires exactly one switch
edit.

Function shape unchanged:

```cpp
std::string interest_group_kind_to_string(InterestGroupKind k);
Result<InterestGroupKind> interest_group_kind_from_string(std::string_view s);
```

Sentinel fallback `"UnknownInterestGroupKind"` from the
`to_string` switch preserved (mirrors
`player_command_kind_to_string`).

## 5. Save schema

**No save schema bump.** Save format remains `v11`. M3.5 only
adds runtime artefact output; no persistent state field changes.

## 6. Determinism

The five-artefact byte-identical determinism test from M1.17 /
M2.22 grows to **six** artefacts. Both
`tests/integration/m1_end_to_end_test.cpp` and
`tests/integration/m2_end_to_end_test.cpp` add an
`interest_groups.csv` `read_file` comparison at the end of their
determinism case. Canonical scenarios author zero interest
groups so the file is header-only, but it must round-trip
byte-for-byte regardless.

## 7. CSV escaping policy

`csv_escape(std::string_view)` is a tiny RFC-4180 helper:

- pass-through if the field contains no `,`, `"`, `\n`, or `\r`;
- otherwise wrap in `"..."` and double every embedded `"`.

Currently only `name` is realistically dirty (scenario authors
might write `"Workers, Engineers, and Allies"`). `id_code` and
`kind` are validated upstream to use safe characters but go
through the same helper so a future loosening of those rules
cannot silently produce a malformed CSV.

## 8. What is NOT in scope

Explicit non-goals so the next sub-milestone can pick from the
clean list:

- no new state fields;
- no new `InterestGroupKind` variants;
- no formula changes to M3.2 / M3.3 / M3.4;
- no new gameplay, event triggers, AI, UI, REPL, CLI flag;
- no new `PlayerCommandKind`;
- no command-gate integration with interest groups;
- no weighted aggregate diagnostics (formula trace);
- no per-tick delta CSV;
- no `--target-date` interaction beyond the existing replay
  flow;
- no atomic `end_tick` artefact writing (still sequential,
  documented gap);
- no save schema bump.

## 9. Test surface

Twenty-four new doctest cases:

`diagnostics_test.cpp` (M3.5 section, sixteen cases):

- `csv_escape`: pass-through, comma triggers quoting, embedded
  quote doubled, CR/LF triggers quoting.
- `interest_group_snapshot`: reads every field; out-of-range
  index rejected; empty `interest_groups` rejected; invalid
  country reference rejected loudly; default `CountryId{}`
  rejected; does not mutate state.
- `write_interest_group_csv_header`: documented columns pinned.
- `write_interest_group_csv_row`: prefix shape; nine fields
  separated by eight commas; name with comma quoted; name with
  embedded quote escaped; byte-identical re-emit; every
  `InterestGroupKind` variant round-trips through the kind
  column.

`runner_test.cpp` (M3.5 section, eight cases):

- empty world still produces a header-only file;
- default path is `<output_dir>/interest_groups.csv`;
- `interest_groups_csv_path` override honoured (default path
  NOT written);
- scenario with hand-built groups emits one row per group per
  snapshot point;
- vector-order preservation (zeta-alpha-mu retained, not
  alphabetical);
- byte-identical determinism on same seed;
- invalid country reference fails before any artefact lands;
- `--summary-csv` byte stream unchanged by the new writer
  (M0.10 contract preserved).

Two existing integration tests extended:

- `m1_end_to_end_test.cpp` and `m2_end_to_end_test.cpp` both
  add `interest_groups.csv` to their byte-identical
  determinism gates (5 → 6 artefacts).

`runner_test.cpp` `wire_all_artifacts` / `check_no_artifacts`
helpers gain the sixth path; every M2.9 regression test inherits
the extended assertion automatically.

## 10. Future M3.6+ candidates (none committed)

- formula intermediates CSV (weighted loyalty / radicalism per
  country per snapshot point);
- per-kind aggregate CSV;
- atomic `end_tick` artefact writes (temp-file + rename);
- structured diff output between two interest_groups.csv files
  via `diagnostics::compare_states` invoked from a CLI utility;
- `--interest-groups-csv` CLI flag if a user reports a real need
  to redirect the file (so far the programmatic override is
  enough for tests).

Per the M-pacing rule, M3.6 waits for an explicit reviewer
green-light with a specific scope.

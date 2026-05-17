# M3.8 - Canonical scenario interest-group fixtures

Companion notes for `feature/m3-08-canonical-interest-group-fixtures`.

M3.8 is a **data-fixture PR**, not a system or formula PR. It
adds a small `interest_groups` block to the two canonical
scenario manifests so the M3.5 state surface and the M3.6
formula-trace CSVs produce real data rows when the runner is
driven by the canonical world. Up to M3.7 these CSVs were
header-only on canonical runs because canonical scenarios
authored zero interest groups (covered explicitly in the
M3.7 hand-built integration tests, but never on the canonical
path).

## 1. Scope

What ships:

```text
data/scenarios/1930_minimal.json              + 3 Bureaucracy groups
data/scenarios/1930_with_start_policies.json  + 3 Bureaucracy groups
tests/systems/scenario_loader_test.cpp        canonical load test
                                              gains +3 IG assertions
tests/systems/runner_test.cpp                 + 1 M3.8 canonical-
                                              data-row test
tests/integration/m1_end_to_end_test.cpp      comment-only refresh
tests/integration/m2_end_to_end_test.cpp      comment-only refresh
```

Each canonical country gets exactly one Bureaucracy interest
group:

| id_code            | country | influence | loyalty | radicalism |
| ------------------ | ------- | --------- | ------- | ---------- |
| `ger_bureaucracy`  | GER     | 0.55      | 0.50    | 0.10       |
| `fra_bureaucracy`  | FRA     | 0.55      | 0.50    | 0.10       |
| `jpn_bureaucracy`  | JPN     | 0.55      | 0.50    | 0.10       |

Why these numbers:

- **`kind` = Bureaucracy.** Bureaucracy is the only kind the
  M3.4 `authority_pressure` step reads, so a single Bureaucracy
  group per country is enough to drive all three reverse-
  direction systems (M3.2 react / M3.3 country_feedback /
  M3.4 authority_pressure). Adding Military / Workers /
  other kinds is deliberately deferred until a sub-milestone
  introduces the matching gameplay system.
- **`influence = 0.55`.** Non-zero so M3.3 weight_sum and
  M3.4 weight_sum stay positive; mid-range so no single kind
  dominates a future multi-kind aggregate.
- **`loyalty = 0.50`.** Same as country
  `government_authority.bureaucratic_compliance` default
  (0.5), so the M3.4 drift step starts neutral and only the
  M3.2 react step's small drift each month moves the target
  in subsequent ticks.
- **`radicalism = 0.10`.** Low but non-zero so M3.3 has a
  positive `weighted_radicalism` (otherwise the target
  stability collapses to 1.0 and tests can't see direction).

## 2. What this fixture does NOT do

```text
no new system
no new formula
no new artefact (still 8)
no save schema bump (still v11)
no new loader semantics
no auto-generation of interest groups
no new InterestGroupKind
no Military / Workers / Media / etc. groups yet
no military_pressure / intelligence_pressure / media_pressure
no event triggers
no command-gate diagnostic surface
no command-gate formula change
no AI / UI / REPL / CLI surface
no new PlayerCommandKind
no runner CLI flag
no M3 close-out
no docs/milestone-3-result.md
no M4
```

The fixture is the smallest possible nudge needed to take the
canonical path off the header-only branch. The reviewer
chooses any future widening (Military groups, multi-group
countries, per-country tuning).

## 3. Effect on existing tests

The M1.17 / M2.22 byte-identical determinism contracts are
**unchanged in shape** — they compare two runs of the same
scenario against each other, so going from header-only to
data rows on both sides preserves the equality. Only the
explanatory comments in `m1_end_to_end_test.cpp` and
`m2_end_to_end_test.cpp` needed a refresh; no asserts
moved.

The header-only runner tests (M3.5 / M3.6) are unaffected
because they use the empty-world / no-scenario path, not the
canonical scenario. The M3.7 hand-built integration tests
are likewise untouched because they construct their own
state.

The canonical scenario_loader test gains six new asserts
pinning the 3-group shape and `kind` / `country` /
`influence` / `loyalty` / `radicalism` values so a future
manifest edit can't silently drift the fixtures.

The new M3.8 runner test (`tests/systems/runner_test.cpp`)
runs the canonical 1930_minimal scenario for 31 days and
asserts:

```text
interest_groups_csv_rows                   == 9   (3 groups × 3 snapshots)
interest_group_country_feedback_csv_rows   == 3   (one monthly tick × 3 countries)
interest_group_authority_pressure_csv_rows == 3   (same)
```

plus presence of each country's id_code in each M3 CSV.

## 4. Why both canonical scenarios were touched

`1930_minimal.json` and `1930_with_start_policies.json` are
both used by the integration / runner tests. Keeping the two
interest-group blocks in sync (identical fixtures) means a
future scenario test can pick either manifest without
worrying about which one has the M3 data.

## 5. M3 status

**M3 remains in progress.** M3.8 is one of the smaller
sub-milestones; it ships data, not behaviour. M3 still has
no exit report — no `docs/milestone-3-result.md` and no
"M3 closed" wording anywhere. The next M3 sub-milestone is
not committed; reviewer chooses.

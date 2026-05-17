# M1.17 - M1 exit / end-to-end integration tests

Backfilled per-sub-milestone design note for M1.17. The
canonical M1 exit ledger lives in
[`milestone-1-result.md`](milestone-1-result.md); this file
covers the M1.17-specific deliverable in isolation so the
per-sub-milestone naming convention is unbroken across M1.

## 1. Scope

M1.17 is the **exit / integration sub-milestone** for M1.
It does NOT add a new gameplay system; it adds the end-to-end
test surface that pins M1 as a closed milestone.

What shipped:

```text
tests/integration/m1_end_to_end_test.cpp    3 new doctest cases
docs/milestone-1-result.md                  M1 exit report
README.md / docs/README.md / rfc/README.md  flipped to "M1 closed"
```

The three integration cases:

1. **Scenario load → day-0 enactment → 365-day tick → save /
   load round-trip.** Drives the canonical
   `1930_with_start_policies.json` through the runner with
   every diagnostic CSV opted in. Verifies the loader
   populates 3 countries / 3 factions / 10 policies, applies
   the two day-0 starting policies on GER, fires 12 monthly
   pipelines across 1930, records the expected `ActivePolicy`
   shape (`expires_on = current_date + duration_days`),
   produces 14 / 42 / 42 rows in summary / countries /
   factions CSVs, and round-trips the save with
   `active_policies` and `last_gdp_growth_rate` intact.

2. **10-year soak run.** RFC-090 §1.17's acceptance
   criterion: "跑 10 年單國測試". 3652 days =
   1930-01-01 → 1940-01-01 (years 1932 and 1936 are leap),
   120 monthly pipelines, zero sanity issues, every numeric
   field finite and clamped, completes in under 100 ms.

3. **5-artefact byte-identical determinism.** Two runs of the
   canonical 365-day scenario into independent temp dirs
   produce byte-identical `save.json` / `events.jsonl` /
   `summary.csv` / `countries.csv` / `factions.csv`. Pins
   M0.10 / M1.14 / M1.16 cross-CSV-isolation contracts at
   one site so a future system that quietly introduces
   non-determinism (RNG misuse, iteration-order dependence,
   path-metadata leak) fails this gate loudly.

## 2. Why this is its own sub-milestone

The 1-year + 10-year + determinism trio is what makes "M1
closed" meaningful. Without it the individual sub-milestones
each pinned their own slice but nothing exercised the full
pipeline end-to-end at the M1 frontier. M1.17 fills that gap
and then writes `milestone-1-result.md` to record what M1
shipped, what was deliberately deferred, and the
architectural invariants every M2+ milestone must preserve.

## 3. What M1.17 does NOT do

```text
no new system
no new formula
no new state field
no new artefact
no save schema bump
no new CLI flag
no new logs
no M2 work
```

The contract M1.17 ships is purely "the M1 surface composes
end-to-end" plus the exit report. Anything that would change
the binary's behaviour is by definition M2-or-later scope.

## 4. Cross-references

- [`milestone-1-result.md`](milestone-1-result.md) — canonical
  M1 exit report (every M1.x sub-milestone summarised, deferred
  items, M2+ invariants, test counts at M1 exit, run-the-tests
  instructions).
- [`m1-9-monthly-pipeline.md`](m1-9-monthly-pipeline.md) — the
  monthly composition pinned by the 1-year and 10-year cases.
- [`m1-14-diagnostics-surfaces-growth.md`](m1-14-diagnostics-surfaces-growth.md)
  and [`m1-16-faction-csv.md`](m1-16-faction-csv.md) — the
  per-country / per-faction CSVs whose byte-identical
  determinism the third case pins.

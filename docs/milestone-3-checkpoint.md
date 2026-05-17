# Milestone 3 Checkpoint

**HISTORICAL — M3.7 mid-milestone snapshot.** This was the
checkpoint as of M3.7 (M3 still in progress at the time).
M3 has since CLOSED with M3.11; the canonical M3 exit
reference is `milestone-3-result.md`. The content below is
preserved for archaeology only and may lag the current main
branch where M3.8 / M3.9 / M3.10 / M3.11 layered on top of
the M3.7 picture described here.

For M1 and M2 exit reports, see `milestone-1-result.md`
and `milestone-2-result.md`.

## What M3 has shipped so far

| Sub-milestone | What it added |
|--|--|
| **M3.1** (PR #50) | `core::InterestGroupKind` enum + `core::InterestGroupState` POD + `GameState::interest_groups` vector. Save format **v10 → v11**. Data-only; no system reads or writes the new fields. |
| **M3.2** (PR #51) | `interest_group::react(state)` — drift every group's `loyalty` toward `country.stability` and `radicalism` toward `1 - country.stability` at rate `0.05`. Wired as the FINAL global step of `monthly::tick_all_countries`. |
| **M3.3** (PR #52) | `interest_group::country_feedback(state)` — drift each country's `stability` toward `1 - influence_weighted_radicalism` at rate `0.02`. Wired AFTER M3.2 inside `tick_all_countries` so it reads the freshly-drifted radicalism. |
| **M3.4** (PR #53) | `interest_group::authority_pressure(state)` — drift each country's `government_authority.bureaucratic_compliance` toward the influence-weighted loyalty of its **Bureaucracy-kind** groups at rate `0.01`. Wired AFTER M3.3 inside `tick_all_countries`. |
| **M3.5** (PR #54) | `interest_groups.csv` — unconditional state-surface artefact (9 columns), with snapshot cadence matching the existing state CSVs. Extracted shared `core/interest_group_kind.{hpp,cpp}` helper. |
| **M3.6** (PR #55) | `interest_group_country_feedback.csv` + `interest_group_authority_pressure.csv` — unconditional outcome-trace artefacts (10 columns each). `country_feedback` / `authority_pressure` gain an optional `trace_out` pointer; default-null preserves pre-M3.6 callers. |
| **M3.7** (this PR) | Integration test + checkpoint doc. No new code path, no new artefact. |

## Current M3 reaction-loop dataflow

```text
                ┌────────────────────────────────┐
                │   country.stability            │
                │   country.bureaucratic_compliance │
                └────────────────────────────────┘
                          │       ▲       ▲
                  M3.2    │       │ M3.3  │ M3.4
                  rate    │       │ rate  │ rate
                  0.05    │       │ 0.02  │ 0.01
                          ▼       │       │
   ┌────────────────────────────────────────────┐
   │   InterestGroupState                       │
   │     loyalty                                │
   │     radicalism                             │
   │     influence  (NOT drifted yet)           │
   │     kind       (NOT drifted; structural)   │
   └────────────────────────────────────────────┘
                          │       │       │
                          │       │       │  Bureaucracy-kind only
              all groups  │       │       │
              (M3.2)      ▼       ▼       ▼
                       ┌────────────────────┐
              state    │ interest_groups.csv │  (M3.5)
              surface  └────────────────────┘

              outcome  ┌─────────────────────────────────────┐
              trace    │ interest_group_country_feedback.csv │  (M3.6, M3.3 outcomes)
                       └─────────────────────────────────────┘
                       ┌──────────────────────────────────────────┐
                       │ interest_group_authority_pressure.csv    │  (M3.6, M3.4 outcomes)
                       └──────────────────────────────────────────┘
```

Rate ladder: each successive reverse-direction step is slower
than the previous one (`0.05 → 0.02 → 0.01`) so the closed loop
stays well-damped. None of the three M3 systems use RNG; the
loop is fully deterministic.

Inside `monthly::tick_all_countries`:

```text
for each country in state.countries (vector order):
    faction::react(state, country)              // M1.6
    stability::tick(state, country)             // M1.7 (reads M1.12 last_gdp_growth_rate)
    economy::tick(state, country)               // M1.8 (writes last_gdp_growth_rate)
interest_group::react(state)                    // M3.2 (per group)
interest_group::country_feedback(state, &cf_trace)   // M3.3 (per updated country)
interest_group::authority_pressure(state, &ap_trace) // M3.4 (per updated country)
```

`cf_trace` and `ap_trace` are vectors inside `MonthlyOutcome`;
the runner drains them in `step_one_day` into `TickController`
buffers and `end_tick` writes the two M3.6 CSV files.

## Runner artefact set (8 files)

```text
save.json                                      always written
events.jsonl                                   always written
summary.csv                                    opt-in via --summary-csv     (M0.10)
countries.csv                                  opt-in via --countries-csv   (M1.14)
factions.csv                                   opt-in via --factions-csv    (M1.16)
interest_groups.csv                            always written               (M3.5)
interest_group_country_feedback.csv            always written               (M3.6)
interest_group_authority_pressure.csv          always written               (M3.6)
```

`end_tick` writes these in the order listed above.

### M2.9 contract still holds

A `run()` failure before `end_tick` writes ZERO artefacts. M3.5
and M3.6 grew the set to 6 / 8 files; the no-artefact-on-failure
guarantee carried with it because `end_tick` is still the only
function on the runner side that touches disk.

### Mid-`end_tick` is still NOT atomic

The eight writes happen sequentially. A disk failure between
files 4 and 5 leaves a partial set on disk. Atomic temp-file +
rename was deliberately deferred from M2.9, M3.5, M3.6, and
M3.7.

## Current M3 invariants

These should hold across every future M3.X PR unless the PR
explicitly relaxes one of them and the relaxation is documented
in this checkpoint update or in the M3 exit report:

```text
save format remains v11
no M3 events / state.logs entries yet
no AI yet
no UI / CLI / REPL command interface for M3
no new PlayerCommandKind from M3
no new InterestGroupKind variants since M3.1
interest groups do not directly create logs or events
all three M3 reaction systems are deterministic and RNG-free
M2 command gates (M2.18 EnactPolicy, M2.19 AdjustBudget) are
    unchanged, BUT bureaucratic_compliance now drifts via M3.4 —
    a low-loyalty Bureaucracy can drag the gate's input
    downward over time
end_tick still writes artefacts sequentially, not atomically
canonical scenarios author zero interest groups, so all three
    M3 CSVs are header-only when no scenario customisation
    adds them
M1 monthly pipeline order (faction::react → stability::tick →
    economy::tick) is byte-identical pre-M3
trace-out pointers (M3.6) never change formula, mutation,
    or *Outcome::countries_updated
```

## Deferred items (NOT in scope for M3 so far)

Pulled forward from the M3.1–M3.6 design notes so the next
M3.X PR can see them in one place. None of these are
committed; reviewer must give an explicit spec before any of
them ship.

```text
event generation from interest-group thresholds
strike / protest / coup / civil-war mechanics
automatic interest-group generation for canonical scenarios
policy preferences / demands system
cross-border interest-group influence
per-kind formulas (Military / Workers / Religious / etc.)
sibling authority channels:
    Military-kind loyalty       → military_loyalty
    Intelligence-kind loyalty   → intelligence_capability
    Media-kind loyalty          → media_control
weighted multi-input feedback (loyalty alongside radicalism
    for stability, or radicalism alongside loyalty for
    authority)
influence drift driven by event / policy outcomes
interest-group aggregates read inside the M2.18 / M2.19
    command-execution gate (beyond the existing implicit
    effect via bureaucratic_compliance drift)
UI / REPL / CLI surfaces for inspecting interest groups
atomic end_tick writes (temp-file + rename)
M3.2 react per-mutation trace CSV (currently no trace because
    M3.2 mutates every group every month — a per-mutation
    trace would effectively duplicate interest_groups.csv)
strengthening the null-trace baseline tests with byte-for-byte
    null-vs-non-null comparison (PR #55 reviewer non-blocker)
```

## Test surface at M3.7

Three new integration tests in `tests/integration/m3_end_to_end_test.cpp`:

1. **One-month reaction loop.** Hand-built state with one
   country + one Bureaucracy group; `monthly::tick_all_countries`
   mutates all three M3 fields (group loyalty / radicalism,
   country stability, country bureaucratic_compliance) and the
   three counters / trace vectors agree.
2. **Runner emits all 8 artefacts.** 31-day run with a single
   month boundary; every artefact lands on disk and the three
   M3 CSVs have data rows beyond the header.
3. **Byte-identical 8-artefact determinism.** Two same-seed
   runs of the same hand-built state produce byte-identical
   output across all 8 artefacts.

Unit tests already pin exact arithmetic per system; the
integration tests deliberately use loose `!= before` checks
and `>= 1` counters so they survive future balance tweaks
that the unit-test suite will catch separately.

## Suggested M3.8 candidates (none committed)

Listed roughly in increasing scope. The M-pacing rule applies:
nothing starts until the reviewer hands over an explicit spec.

- Strengthen the null-trace baseline tests (PR #55 reviewer's
  non-blocker nit — byte-for-byte null-trace vs non-null-trace
  run comparison).
- A sibling authority channel (e.g. Military-kind loyalty →
  `military_loyalty`).
- A radicalism-driven legitimacy or corruption drift, mirroring
  M3.3's shape on a different country field.
- Influence drift driven by event / policy outcomes.
- Reading interest-group aggregates inside the M2.18 / M2.19
  command-execution gate as an additional input.

M3 stays open. M3 exit report (the equivalent of
`milestone-1-result.md` / `milestone-2-result.md`) is NOT
written here; that document waits for the M3 close-out PR
whenever the reviewer signals it.

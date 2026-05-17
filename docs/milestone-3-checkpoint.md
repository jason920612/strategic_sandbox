# Milestone 3 Checkpoint

Status: in progress

Companion notes for `feature/m3-07-reaction-loop-integration-checkpoint`.

M3.7 is a checkpoint, not an exit report. M3 remains in
progress: this sub-milestone adds integration tests that pin
the M3.1–M3.6 reaction loop at the seam between M3 and any
future milestone, and a short doc summarising where M3 is
right now. No new system, formula, artefact, gameplay branch,
save-schema bump, or close-out.

The companion exit report (`docs/milestone-3-result.md`) is
deliberately not written yet. M3 has more sub-milestones to
ship; the close-out lands when the reviewer decides M3 is
done, not at M3.7.

## 1. Current M3 dataflow

```text
country.stability
  -> M3.2 interest_group::react
  -> group.loyalty / group.radicalism

group.radicalism + influence
  -> M3.3 interest_group::country_feedback
  -> country.stability

Bureaucracy group loyalty + influence
  -> M3.4 interest_group::authority_pressure
  -> country.government_authority.bureaucratic_compliance

state / outcomes
  -> M3.5 interest_groups.csv
  -> M3.6 interest_group_country_feedback.csv
          interest_group_authority_pressure.csv
```

Rate ladder (deliberately decreasing so the closed loop stays
well-damped):

```text
group mood        (M3.2)  0.05
country stability (M3.3)  0.02
authority         (M3.4)  0.01
```

Per-leg arithmetic and edge cases are already pinned by the
unit tests under `tests/systems/interest_group_system_test.cpp`.

## 2. Current artifacts

End-of-run produces eight files. M1.17 fixed the five-file
shape; M3.5 added the sixth; M3.6 added the seventh and eighth.

```text
save.json                                  (M0.8,  required)
events.jsonl                               (M0.6,  required)
summary.csv                                (M0.10, opt-in)
countries.csv                              (M1.14, opt-in)
factions.csv                               (M1.16, opt-in)
interest_groups.csv                        (M3.5,  unconditional)
interest_group_country_feedback.csv        (M3.6,  unconditional)
interest_group_authority_pressure.csv      (M3.6,  unconditional)
```

The three M3 artefacts always appear; canonical scenarios
author zero interest groups so they are header-only in the
M1.17 / M2 integration runs. M3.7's `m3_end_to_end_test`
covers the data-row path with a hand-built state.

## 3. Current invariants

These properties hold at the M3.7 checkpoint. M3.7 itself does
not add any of them — it only documents and pins.

```text
save format remains v11
M3 remains in progress
no M3 close-out yet (no docs/milestone-3-result.md)
no M3 events yet
no AI yet
no UI / CLI / REPL command interface yet
no new PlayerCommandKind in M3
interest groups do not directly create logs/events
reaction systems are deterministic and RNG-free
M2 command gates are unchanged
bureaucratic_compliance can now drift through M3.4 and is
  therefore a downstream input to existing M2 gates
end_tick still writes artifacts sequentially, not transactionally
```

The last invariant is the same caveat M3.6 documented on
`runner::run`: a mid-`end_tick` I/O failure can leave a partial
set of files on disk. Switching `end_tick` to temp-file +
rename would close it; M3.7 does not.

## 4. Integration test coverage

`tests/integration/m3_end_to_end_test.cpp` adds three cases:

1. **One-month reaction loop.** A hand-built state with one
   country + one Bureaucracy interest group + one faction goes
   through `monthly::tick_all_countries` once. Asserts every
   reverse-direction counter ticked, every mutable field
   actually changed, and each trace vector got one row whose
   post-mutation value equals the corresponding live state
   field.

2. **8-artefact runner run with data rows.** Same hand-built
   state through `runner::run_state` for 31 days (crosses one
   month boundary). Asserts all eight files exist on disk, the
   three M3 files contain data rows (not header-only), and the
   `RunOutcome` counters report the expected non-zero row
   counts.

3. **Byte-identical determinism with M3 mutation.** Two runs
   of the same hand-built state through `run_state` into
   independent temp dirs produce byte-identical files across
   all eight artefacts. This is the same shape as M1.17's and
   M2.22's determinism contract, but covering the path where
   the M3 systems actually mutate state.

## 5. Deferred items

These are intentionally not in M3 yet. M3.7 lists them so
later sub-milestones (or a post-M3 milestone) have a single
reference for what was explicitly out of scope.

```text
interest group generation for canonical scenarios
event generation from interest group thresholds
strike / protest / coup / civil-war mechanics
policy preference / demands system
cross-border influence
per-kind formulas
additional authority pressure channels:
  - military_loyalty
  - intelligence_capability
  - media_control
command-gate diagnostic / UI surface
command-gate integration beyond existing downstream
  bureaucratic_compliance effect
UI / REPL / CLI command surfaces
atomic end_tick writes
M3 close-out / exit report
```

## 6. What M3.7 does not do

For symmetry with the per-sub-milestone notes:

```text
no new system
no new formula
no new artefact
no save schema bump
no new state fields
no new InterestGroupKind
no new PlayerCommandKind
no event triggers
no logs from interest groups
no AI / UI / REPL / CLI surface
no command gate formula changes
no command gate diagnostics
no runner CLI flags
no atomic artifact writing fix
no M3 close-out
no docs/milestone-3-result.md
no "M3 closed" wording
no M4
```

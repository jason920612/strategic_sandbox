# Milestone 3 Result

**Status: closed.**

M3 set out to make political actors a first-class part of the
simulation: a typed data layer for interest groups, a small
closed reaction loop between country state and group state,
two reverse-direction channels feeding country `stability`
and `bureaucratic_compliance`, three observability artefacts
covering both group state and per-mutation outcome traces, an
integration checkpoint pinning the loop, and finally a small
canonical-scenario fixture so the artefacts carry real data
on canonical runs. Nine sub-milestones delivered that.

## 1. What M3 shipped

| Sub-milestone | Title | Highlights |
|---------------|-------|------------|
| **M3.1** | InterestGroupState / political actors skeleton | New `core::InterestGroupKind` (10 variants) + `core::InterestGroupState` POD + root-level `GameState::interest_groups`. Save format v10 → v11. Optional in scenario JSON, required at save layer. Data-only; no system reads. |
| **M3.2** | InterestGroupReactionSystem skeleton | First M3 system to mutate the data layer. `interest_group::react(state)` drifts each group's `loyalty` toward `country.stability` and `radicalism` toward `1 - stability` at rate `0.05`. Wired into `tick_all_countries` as the global step after every per-country pipeline. |
| **M3.3** | InterestGroup country feedback | Closes the loop: `country.stability` drifts toward `1 - influence_weighted_radicalism` at rate `0.02`. Slower than M3.2 by design; the only mutation surface is `country.stability`. |
| **M3.4** | InterestGroup-derived authority pressure | Second reverse-direction channel: `country.government_authority.bureaucratic_compliance` drifts toward influence-weighted loyalty over **Bureaucracy-kind** groups at rate `0.01`. Completes the rate ladder mood (0.05) → stability (0.02) → authority (0.01). |
| **M3.5** | InterestGroup reaction diagnostics / CSV surface | First M3 observability artefact: `interest_groups.csv` written unconditionally by `end_tick`. Nine fixed columns; vector-order preserved. Drive-by extracted the `InterestGroupKind` ↔ string mapping into shared `core/interest_group_kind.{hpp,cpp}`. Artefact set 5 → 6. |
| **M3.6** | InterestGroup feedback outcome diagnostics / CSV trace surface | Two new unconditional CSVs: `interest_group_country_feedback.csv` and `interest_group_authority_pressure.csv`, one row per actual mutation. New `CountryFeedbackTraceRow` + `AuthorityPressureTraceRow` POD types; trace pointer is optional and default-null on `country_feedback` / `authority_pressure`. Artefact set 6 → 8. |
| **M3.7** | M3 reaction-loop integration checkpoint | `tests/integration/m3_end_to_end_test.cpp` with three cases pinning the M3.1–M3.6 loop at the seam between M3 and any future milestone. New `docs/milestone-3-checkpoint.md` snapshotting dataflow / artefacts / invariants / deferred items. No new system, formula, artefact, or schema bump. |
| **M3.8** | Canonical scenario interest-group fixtures | One Bureaucracy interest group per canonical country (GER / FRA / JPN) added to `data/scenarios/1930_minimal.json` and `data/scenarios/1930_with_start_policies.json`. Takes the canonical path off the header-only branch so the three M3 CSVs now carry 9 / 3 / 3 data rows on a 31-day canonical run. Data-only. |
| **M3.9** | M3 exit / close-out | This sub-milestone. New `docs/milestone-3-result.md` (this file). READMEs flipped to "M3 closed". `docs/milestone-3-checkpoint.md` annotated as historical. No code, no formula, no fixture, no test change. M3 closes here. |

## 2. Final M3 dataflow

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

canonical fixtures
  -> M3.8 1930_minimal / 1930_with_start_policies
     each include GER / FRA / JPN Bureaucracy groups
```

The loop is intentionally well-damped: the rate ladder
`0.05 → 0.02 → 0.01` keeps the outermost leg from
oscillating, and the canonical Bureaucracy groups sit at
mid-range numeric defaults so a long run produces visible
movement without crossing the `[0, 1]` clamps.

## 3. Final artefact contract

M3 closes with eight runner artefacts:

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

`end_tick` writes the eight files **sequentially, not
transactionally** — a mid-`end_tick` I/O failure can leave
a partial set on disk. The pre-`end_tick` no-artefact
contract from M2.9 (failures before `end_tick` is reached
write zero files) continues to hold for the seventh and
eighth files automatically because `end_tick` is still the
only function that touches disk.

## 4. Save schema

```text
save format remains v11
```

M3 bumped the schema exactly once (v10 → v11 at M3.1, to
make the `interest_groups` block a required root-level
array). Every subsequent M3 sub-milestone (M3.2 – M3.8)
was deliberately schema-neutral; M3.9 is doc-only. v11 is
the new floor; the next persistent-state addition will
bump it under the M0.8 strict-equality + version-history
rule.

## 5. Architectural invariants every future milestone must preserve

These are the rules M3 added on top of the M0 / M1 / M2
invariants. Future milestones must not silently break them.

- **M3 systems are deterministic and RNG-free.** `react`,
  `country_feedback`, and `authority_pressure` are pure
  reads of state plus deterministic drift formulas. Any
  future M3-adjacent system that needs RNG must be a new
  module, not an overload of these names.
- **M3 systems do not create logs or events.** `state.logs`
  and the (still empty) event surface are untouched by
  every M3 system. Interest groups do not directly trigger
  events yet.
- **The reaction loop is closed and well-damped.** The rate
  ladder `0.05 → 0.02 → 0.01` is load-bearing for the
  loop's stability; any new feedback leg must either fit
  inside the ladder or document why a different rate is
  safe.
- **`tick_all_countries` runs the three M3 global steps in
  the canonical order** (`react` → `country_feedback` →
  `authority_pressure`) after every per-country pipeline.
  Reordering changes observable arithmetic; existing tests
  pin the order.
- **M2 command gates are unchanged by M3.** The M2.18 /
  M2.19 thresholds, the `evaluate` shape, and the
  `apply_pending` / `try_apply_pending` surfaces are
  byte-identical with M2.22. `bureaucratic_compliance` now
  drifts through M3.4 and is therefore a downstream input
  to those existing gates — preserve that data flow but do
  not change the gates themselves without a dedicated PR.
- **`interest_groups.csv` row order follows
  `state.interest_groups` insertion order.** No sort. The
  shared `core::interest_group_kind_to_string` is the one
  source of truth for kind strings across save / scenario /
  diagnostics.
- **M3.6 trace rows are emitted only on actual mutation.**
  Skipped countries produce no row; preflight failure
  produces no partial rows; passing the trace pointer
  changes neither the formula nor the mutation nor the
  `*_countries_updated` counters.
- **Canonical scenarios now author minimal Bureaucracy
  interest groups** (one per country). The three M3 CSVs
  on canonical runs are no longer header-only. Tests that
  assumed header-only on canonical scenarios were corrected
  in M3.8; new tests added under M3+ must reflect the
  data-row baseline.
- **The artefact set is eight files.** Adding a ninth
  requires its own sub-milestone with the cadence /
  determinism / pre-`end_tick` contracts documented
  alongside.
- **Save format v11 is the new floor.** Future persistent
  state bumps it under M0.8's strict-equality rule.

## 6. Deferred items

These are intentionally not in M3. They are listed so a
future sub-milestone or post-M3 milestone has one canonical
reference for what M3 explicitly did not ship.

- Military-kind pressure on `military_loyalty`.
- Intelligence-kind pressure on `intelligence_capability`.
- Media-kind pressure on `media_control`.
- Interest-group generation / richer country-specific
  political maps (canonical scenarios still author exactly
  one Bureaucracy group per country).
- Policy preference / demands system.
- Event generation from interest-group thresholds.
- Strike / protest / coup / civil-war mechanics.
- Cross-border influence.
- Per-kind formulas and balance tuning.
- Command-gate diagnostic / UI surface.
- Command-gate integration beyond the existing downstream
  `bureaucratic_compliance` effect.
- UI / REPL / CLI command surfaces beyond the existing
  runner flags.
- Atomic `end_tick` writes (temp-file + rename).

## 7. Recommended next milestone candidates

Next milestone direction should be chosen explicitly by the
reviewer.

Candidates:

- **RFC-090 M4** — SVG map + UI. Picks up where M2 / M3
  left off on the player-visible surface side.
- **RFC-090 M5** — event engine. Closest neighbour to the
  M3 reaction loop on the gameplay side; would unlock the
  "interest-group thresholds trigger events" deferred item
  above.
- **Post-M3 governance diagnostics**, *if* deliberately
  scoped outside RFC-090 numbering. A previous attempt
  drifted into inventing M4.X numbers; any future
  governance-side work should either follow RFC-090 numbers
  or label itself as a non-RFC follow-up from the outset
  (the lesson recorded after the 2026-05-17 force-reset).

M3.9 deliberately does **not** open or claim any of the
above. No "M4 in progress" wording lands in this PR; the
next milestone starts when the reviewer says so.

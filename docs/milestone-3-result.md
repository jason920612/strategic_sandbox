# Milestone 3 Exit Report

**Status: closed.** M3.1 through M3.11 shipped on `main`.
M3 as shipped — the internal-politics / interest-group
reaction layer — is complete enough to hand off to future
work. Closing M3 means: no more M3.X branches; future work
in the reaction-loop space ships as a post-M3 governance
follow-up (or as a future RFC-090-numbered milestone),
with this document as the contract that work must preserve.

> **RFC alignment note (retrofitted after merge).** This
> exit report originally cited "RFC-090 §M3" as the
> matching RFC milestone for the shipped work. That mapping
> is a **historical mislabel**: RFC-090 §M3 is multi-country
> simulation (20–30 country data, AI policy selection,
> relationship value, threat value), not interest-group
> reaction. The shipped work is closer to RFC-090 §M7
> 派系深化. See `docs/rfc-alignment-note-post-m3.md` for the
> full drift documentation. This note is observational; the
> body below is left as-is for historical accuracy.

Predecessors: `milestone-1-result.md` (single-country
internal politics prototype) and `milestone-2-result.md`
(player-operation prototype). M0 has `milestone-0-result.md`.

The earlier `milestone-3-checkpoint.md` was a mid-M3
snapshot (after M3.7) and is now historical — superseded
by this document.

## 1. What M3 shipped

| Sub-milestone | What it added |
|--|--|
| **M3.1** | `core::InterestGroupKind` enum (10 variants), `core::InterestGroupState` POD, `GameState::interest_groups` root-level vector. **Save format v10 → v11.** Data-only; no system read the new fields. |
| **M3.2** | `interest_group::react(state)` — first M3 mutating system. Drift every group's `loyalty` toward `country.stability` and `radicalism` toward `1 - country.stability` at rate `0.05`. Wired as the FINAL global step in `monthly::tick_all_countries` (post-tick stability). |
| **M3.3** | `interest_group::country_feedback(state)` — drift each country's `stability` toward `1 - influence_weighted_radicalism` at rate `0.02`. Wired AFTER M3.2. First reverse-direction channel. |
| **M3.4** | `interest_group::authority_pressure(state)` — drift each country's `government_authority.bureaucratic_compliance` toward influence-weighted loyalty of its Bureaucracy-kind groups at rate `0.01`. Wired AFTER M3.3. Second reverse-direction channel. |
| **M3.5** | `interest_groups.csv` — 9-column unconditional state-surface artefact at snapshot points (start / month_changed / final). Extracted shared `core/interest_group_kind.{hpp,cpp}` helper so save / scenario / diagnostics route through one kind ↔ string mapping. Artefact set 5 → 6. |
| **M3.6** | `interest_group_country_feedback.csv` + `interest_group_authority_pressure.csv` — 10-column unconditional outcome-trace CSVs (one row per actually-mutated country per monthly tick). New `country_feedback` / `authority_pressure` optional `trace_out` pointer (default-null = byte-identical baseline). `MonthlyOutcome` surfaces the trace vectors; `TickController` drains them in `step_one_day`. Artefact set 6 → 8. |
| **M3.7** | Mid-M3 integration checkpoint (NOT an exit report). New `tests/integration/m3_end_to_end_test.cpp` + `docs/milestone-3-checkpoint.md`. No new code path, no new artefact. M3 stayed in progress at this point. |
| **M3.8** | Strengthened M3.6 null-trace baseline tests to byte-for-byte equivalence using `diagnostics::compare_states` with `CompareOptions{0.0}`. Tests-only; closed PR #55 reviewer non-blocker. No library change. |
| **M3.9** | `interest_group::military_pressure(state, trace_out = nullptr)` — sibling of M3.4. Military-kind loyalty → `government_authority.military_loyalty` at the same `0.01` rate (M3.4 and M3.9 are siblings on the authority layer, not nested). Wired as the FOURTH global step in `tick_all_countries`. New `MilitaryPressureOutcome` + `MilitaryPressureTraceRow`. Artefact set unchanged (8). |
| **M3.10** | `interest_group_military_pressure.csv` — retrofits the M3.6 trace-CSV pattern onto M3.9. New 9th unconditional artefact. Artefact set 8 → 9. Drive-by: removed a stray `[[wiki-link]]` from the M3.9 design note. |
| **M3.11** | This PR — M3 close-out. 3 new long-running integration tests in `tests/integration/m3_end_to_end_test.cpp` (1-year full M3 surface, 10-year soak, save round-trip across 9 artefacts) + this exit report + READMEs flipped to "M3 closed". No new code path, no new artefact, no save schema bump. |

## 2. The M3 reaction loop

After all four M3 reaction systems shipped, the monthly
pipeline runs in this order inside `monthly::tick_all_countries`:

```text
for each country (vector order):
    M1.6  faction::react   (state, country)
    M1.7  stability::tick  (state, country)     ← reads last_gdp_growth_rate
    M1.8  economy::tick    (state, country)     ← writes last_gdp_growth_rate
M3.2  interest_group::react              (state)
M3.3  interest_group::country_feedback   (state, &cf_trace)
M3.4  interest_group::authority_pressure (state, &ap_trace)
M3.9  interest_group::military_pressure  (state, &mp_trace)
```

Rate ladder (deliberately decreasing so the closed loop
stays well-damped):

| System | Mutates | Rate |
|--|--|--|
| M3.2 `react` | group loyalty + radicalism (every group) | `0.05` |
| M3.3 `country_feedback` | country.stability | `0.02` |
| M3.4 `authority_pressure` | bureaucratic_compliance | `0.01` |
| M3.9 `military_pressure` | military_loyalty | `0.01` |

M3.4 and M3.9 share `0.01` because they sit at the same
"authority" layer — siblings, not nested. The other two
`government_authority` sub-fields (`intelligence_capability`,
`media_control`) stay inert; their channels are deferred
M4+ work.

## 3. Runner artefact set at M3 close (9 files)

```text
save.json                                      always written
events.jsonl                                   always written
summary.csv                                    opt-in via --summary-csv    (M0.10)
countries.csv                                  opt-in via --countries-csv  (M1.14)
factions.csv                                   opt-in via --factions-csv   (M1.16)
interest_groups.csv                            always written              (M3.5)
interest_group_country_feedback.csv            always written              (M3.6)
interest_group_authority_pressure.csv          always written              (M3.6)
interest_group_military_pressure.csv           always written              (M3.10)
```

`end_tick` writes them in the order listed above.

### M2.9 contract still holds at 9 files

A `run()` failure before `end_tick` writes ZERO artefacts.
Every M3 sub-milestone that grew the set (M3.5: 5→6,
M3.6: 6→8, M3.10: 8→9) extended the contract automatically
because `end_tick` is still the only function on the
runner side that touches disk. `runner_test.cpp`'s
`ArtifactPaths` / `wire_all_artifacts` / `check_no_artifacts`
helpers now check 9 absent paths instead of 5.

### Mid-`end_tick` is still NOT atomic

The nine writes happen sequentially. A disk failure
between files leaves a partial set on disk. Atomic
temp-file + rename was deliberately deferred from M2.9 /
M3.5 / M3.6 / M3.10 / M3.11 and is the cleanest M4+
follow-up candidate if a real CI / production deployment
needs the guarantee.

## 4. Invariants every M4+ milestone must preserve

These survived M3 close. New work that breaks one of them
needs a paragraph in its design note explaining why and a
matching update to this document.

```text
save format remains v11 unless the new milestone
    explicitly bumps it (with a v11-rejection test on the
    way in and a documented migration path)
all four M3 reaction systems are deterministic and RNG-free
M2 command gates (M2.18 EnactPolicy, M2.19 AdjustBudget)
    still read only bureaucratic_compliance — M3 did NOT
    add interest-group inputs to the gate; future channel
    work has to integrate explicitly if it wants the gate
    to see it
end_tick is still the only function on the runner side
    that writes to disk; the 9-artefact pre-end_tick
    no-artefact contract follows from this
end_tick still writes artefacts sequentially, not
    atomically; mid-end_tick failure tolerance is an
    open M4+ candidate
canonical scenarios author zero interest groups, so all
    four M3 CSVs are header-only by default — adding
    interest groups to a scenario is an authoring choice
    that no canonical fixture has made yet
M1 monthly pipeline order (faction::react -> stability::tick
    -> economy::tick) is byte-identical pre-M3
M3 global steps run in the documented order (M3.2 -> M3.3
    -> M3.4 -> M3.9) inside tick_all_countries; siblings
    on the same rate ladder layer (M3.4 / M3.9) could in
    principle commute, but the explicit order is pinned
    by tests
trace-out pointers (M3.6 / M3.9) never change formula,
    mutation, or *Outcome::countries_updated; null-vs-non-null
    state is byte-identical (M3.8 contract)
the four authority sub-fields (bureaucratic_compliance /
    military_loyalty / intelligence_capability /
    media_control) all live inside government_authority
    (M2.16 shape); a future system pressing on any of them
    should follow the M3.4 / M3.9 sibling pattern
```

## 5. Deferred items

Pulled forward from M3.1–M3.10 design notes and the M3.7
checkpoint deferred list. None of these are committed;
reviewer must give an explicit spec before any of them ship.

```text
sibling authority channels:
    Intelligence-kind loyalty → intelligence_capability
    Media-kind loyalty        → media_control
    (each one its own future PR; both deferred from M3.9
     even though authority_pressure / military_pressure
     established the pattern)

interest-group integration into the M2.18 / M2.19
    command-execution gate as an additional input beyond
    the already-drifting bureaucratic_compliance

influence drift driven by event / policy outcomes

M3.2 `react` per-mutation trace as a 10th artefact

second feedback input weighting loyalty alongside
    radicalism for the stability channel (M3.3)

event generation from interest-group thresholds (strike
    if Workers radicalism > X, coup if Military loyalty < Y,
    etc.) — would require the new event system in M4+

strike / protest / coup / civil-war mechanics built on
    top of the M3 reaction layer

automatic interest-group generation for canonical
    scenarios (1930_minimal.json etc.) so the artefact
    set is non-header-only out of the box

policy preference / demands system per interest group

cross-border interest-group influence (diaspora,
    foreign-funded media, transnational religious
    networks — root-level vector design from M3.1
    anticipated this but no system reads it yet)

atomic end_tick writes (temp-file + rename) — long-deferred
    from M2.9 / M3.5 / M3.6 / M3.10 / M3.11

per-kind balancing changes (rates beyond 0.05 / 0.02 /
    0.01; per-kind loyalty / radicalism / influence dynamics)
```

## 6. Recommendations for M4

The reviewer has not yet named M4. Plausible directions
the project's structure makes natural at this point:

- **International / diplomacy layer** (RFC-040). M3
  closed an internal-politics loop; M4 could open the
  multi-country interaction layer the M3.1 root-level
  vector design anticipated.
- **Event system** (RFC-050). The M3 reaction loop
  produces observable state shifts (loyalty drift,
  compliance drift) but nothing currently *triggers* on
  them. An event-trigger framework would unlock the
  strike / protest / coup mechanics on top of M3 state.
- **Authority-layer completion** (intelligence /
  media siblings) — a small follow-up that ships in
  the same shape as M3.9 / M3.10.
- **Command-gate integration** — let M2.18 / M2.19
  read interest-group aggregates so commands actually
  feel different at low loyalty.
- **Atomic `end_tick`** — long-deferred infra fix
  rather than gameplay; reasonable cleanup-first move
  before the artefact set grows further.

The M-pacing rule still applies: reviewer green-light
required before any of these ship.

## 7. Test surface at M3 close

`tests/integration/m3_end_to_end_test.cpp` ends M3 with
six test cases:

- **M3.7 test A** — one-month reaction loop runs all
  four M3 systems on a hand-built single-Bureaucracy +
  single-Military state.
- **M3.7 test B** — runner emits all 9 artefacts with
  data rows in the four M3 CSVs after a 31-day run.
- **M3.7 test C** — byte-identical 9-artefact
  determinism on same seed.
- **M3.11 test 1** — 365-day run on a hand-built
  3-country / 9-group state exercises every M3 system;
  asserts counters match expected per-month-times-country
  arithmetic and every M3-mutated field is still inside
  `[0, 1]` after a year.
- **M3.11 test 2** — 10-year soak run keeps every M3
  field inside `[0, 1]` and produces no sanity issues
  (the only assertion the soak run makes — unit tests
  already pin per-tick numerics).
- **M3.11 test 3** — save round-trip preserves
  `interest_groups` entry-by-entry and the four
  `government_authority` sub-fields, plus a second
  same-seed run reproduces byte-identical output
  across all 9 artefacts.

Unit tests in `tests/systems/interest_group_system_test.cpp`
+ `tests/systems/monthly_pipeline_test.cpp` +
`tests/systems/diagnostics_test.cpp` +
`tests/systems/runner_test.cpp` continue to pin per-tick
arithmetic, preflight failure paths, sibling-field
invariants, and the trace-pointer null-vs-non-null
byte-identical guarantee.

Total doctest count at M3 close: 779.

## 8. What is NOT in scope for M3.11

This PR is the M3 close-out. It deliberately does NOT:

- bump the save schema (still v11)
- add new state fields
- add a new artefact (still 9)
- change any M3.X formula
- add new gameplay
- add new `InterestGroupKind` variants
- add new `PlayerCommandKind`
- add events / state.logs from M3
- add AI / UI / CLI / REPL surfaces
- integrate interest-group aggregates into M2.18 / M2.19
- ship the `intelligence_capability` / `media_control`
  sibling channels
- make `end_tick` atomic

Anything from §5 above starts as a post-M3 governance
follow-up PR (or as a future RFC-090-numbered milestone)
with its own spec.

**M3 closes here.**

# M3.8 - null-trace baseline strengthened to byte-for-byte equivalence

Companion notes for `feature/m3-08-null-trace-baseline-comparison`.

M3.8 is a tiny tests-only follow-up to M3.6. PR #55's reviewer
flagged that the M3.6 null-trace baseline tests only checked
"a mutation happened" — they did not prove that the resulting
state is byte-identical between `trace_out = nullptr` and
`trace_out = &vec`. M3.8 closes that gap.

No library change. No artefact change. No doc change beyond
this note + index updates. Save format still v11. Artefact
set still 8 files.

## 1. The gap

The M3.6 tests "null trace pointer = M3.3 baseline behaviour"
and the parallel one for `authority_pressure` looked like:

```cpp
GameState s = m36_state_ger_two_groups();
const double before = s.countries[0].stability;
const auto r = ig::country_feedback(s, /*trace_out=*/nullptr);
REQUIRE(r.ok());
CHECK(r.value().countries_updated == 1);
CHECK(s.countries[0].stability != doctest::Approx(before).epsilon(1e-18));
```

This proves the system runs end-to-end and the mutation happens.
It does NOT prove that passing a non-null trace pointer produces
the same numeric result. A latent bug where the trace path
silently uses a different code branch (e.g. a divergent
intermediate, a reordered arithmetic op, an unintentional
re-clamp) would slip through.

## 2. The fix

For each of `country_feedback` and `authority_pressure`, add
three tests that:

1. Build a non-trivial initial state.
2. Copy it into `s_null` and `s_trace`.
3. Call the system on `s_null` with `trace_out = nullptr` and
   on `s_trace` with `trace_out = &vec`.
4. Assert
   `diagnostics::compare_states(s_null, s_trace, {0.0}).empty()`.
   Tolerance `0.0` is intentional — we want a regression to fail
   loudly, not get masked by 1e-9 slack.
5. Spot-check the specific mutated double field with `==` so
   the failure message names the right path when a regression
   hits.

Three scenarios per system:

- Single-group state (reuses the M3.6 `m36_state_ger_two_groups`
  helper).
- Multi-country state (new local helper
  `m38_multi_country_state`: GER + FRA each with one Bureaucracy
  + one Workers group).
- Skipped-country state (no matching aggregate → no mutation,
  no trace row). Easy place to accidentally regress if a future
  refactor pushes a stub row before the skip check.

Six new doctest cases total.

## 3. Why `compare_states` + tolerance `0.0`

`diagnostics::compare_states` (M2.10) already walks every
gameplay-relevant field on a `GameState`:

- `current_date`, `player_country`;
- every `countries[i]` identity + numerics + budget + government_authority
  + active_policies;
- every `factions[i]` identity + numerics + preferred_policies size;
- every `applied_commands[i]`;
- every `interest_groups[i]` identity + ratios.

Default tolerance is `1e-9` (M0.8 round-trip precision). M3.8
overrides it to `0.0`: the trace pointer must not perturb a
single ULP. If a future change reorders multiplications, splits
the formula across helpers, or introduces a temporary in the
trace-path-only branch, `tolerance = 0.0` catches it.

We could in principle do `memcmp` on the two `GameState`s, but
that ties the test to padding / layout details that the
language standard does not promise. `compare_states` reads every
gameplay-relevant field by name, which is the right contract
for the abstraction the systems live in.

## 4. What is NOT in scope

- no formula change;
- no library change (no `country_feedback` /
  `authority_pressure` shape touched);
- no `MonthlyOutcome` change;
- no runner change;
- no new artefact;
- no save schema bump (still v11);
- no doc change beyond this note + the three READMEs +
  `project_milestone_state.md`.

## 5. Test surface

Six new doctest cases in
`tests/systems/interest_group_system_test.cpp`, M3.8 section
at the end:

```text
country_feedback: null vs non-null trace produce byte-identical state (single group)
country_feedback: null vs non-null trace produce byte-identical state (multi-country)
country_feedback: null vs non-null trace are byte-identical when every country is skipped
authority_pressure: null vs non-null trace produce byte-identical state (single group)
authority_pressure: null vs non-null trace produce byte-identical state (multi-country)
authority_pressure: null vs non-null trace are byte-identical when every country is skipped
```

The pre-M3.6 "M3.6 - formula-trace pointer arg" section is
kept untouched — those tests still document the per-row shape
contract that this PR's stricter checks complement.

## 6. Future M3.9+ candidates (none committed)

The reviewer's M3.7-approval message named M3.8 specifically;
M3.9 onwards is open. Natural candidates remain the same list
from `milestone-3-checkpoint.md`:

- a sibling authority channel (Military / Intelligence /
  Media loyalty → matching `government_authority` sub-field);
- interest-group integration into the M2.18 / M2.19
  command-execution gate as an additional input;
- influence drift driven by event / policy outcomes;
- a second feedback input weighting loyalty alongside
  radicalism for the stability channel;
- M3.2 `react` per-mutation trace as a third trace CSV.

Per the M-pacing rule, M3.9 implementation starts only when
the reviewer approves M3.8 and either names a direction or
defaults to the top of the candidate list.

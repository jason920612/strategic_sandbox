# Hardening: strict numeric validation and asymptotic ratio `add`

**Status:** post-M6.7 hardening sweep (not an RFC-090 milestone).
**Save format:** unchanged (`v18`).
**Artefact contract:** unchanged (11).
**RFC milestone progression:** none — M6 stays at M6.7.

This note documents the project-wide hardening sweep that landed on
branch `feature/hardening-strict-numeric-validation`. It is a single
cross-cutting PR; the per-system source changes carry their own
explanatory comments and are not re-narrated file-by-file here.

---

## 1. Why this hardening sweep was needed

Late-game numeric bugs were nearly impossible to debug under the
pre-sweep code. The engine wrote ratio fields with the pattern

```cpp
field = std::clamp(field + delta, 0.0, 1.0);
```

so a single bad upstream value (e.g. a `NaN` produced by an earlier
arithmetic mistake) could silently saturate the field to `0` or `1`
and propagate as "look, the country has lost all stability" without
any error report naming the offending input.

The same pattern hid in division denominators (`std::max(std::abs(gdp)
* 0.01, 1.0)`), in `random_service::draw_bool` (which previously
treated `NaN` probability as `false`), and in structural fallbacks
like `resolve_followup_ids` skipping unresolvable event ids.

The directive (`feedback_no_silent_degradation`, 2026-05-19): every
such site must `Result::failure` loudly, naming the entity kind,
`id_code`, field path, and offending value. PR #114 (M6.7) applied
the rule to `information_accuracy::compute_for_country` only. This
sweep extends it across the codebase.

## 2. Two sweeps in one PR — kept textually distinct

The PR body and this note keep the two sweeps separate because the
failure modes they catch differ.

### Sweep A — numeric validation

Targets abnormal numeric values: `NaN`, `±Inf`, out-of-range ratio,
negative on a documented non-negative field, non-finite result of a
runtime computation. Affected files:

- `src/leviathan/systems/policy_system.cpp`
- `src/leviathan/systems/commands.cpp` (`AdjustBudget`)
- `src/leviathan/systems/faction_system.cpp`
- `src/leviathan/systems/stability_system.cpp`
- `src/leviathan/systems/economy_system.cpp`
- `src/leviathan/systems/interest_group_system.cpp`
- `src/leviathan/systems/effect_desire.cpp`
- `src/leviathan/systems/ai_policy.cpp`
- `src/leviathan/systems/random_service.cpp`
- shared header `include/leviathan/systems/internal/numeric_guards.hpp`
  (`require_unit_ratio`, `require_finite_double`,
  `require_nonneg_finite`)

The unified error message format is

```
<module>: <entity_kind> '<id_code>' <field_name> = <value>
is not a finite ratio in [0, 1]
```

(or `is not a finite value` / `is not a finite non-negative value`
for the other guards), so a single grep over a log file recovers the
exact source.

### Sweep B — strict structural fallback

Targets non-numeric silent fallbacks that previously degraded the
engine path:

- `src/leviathan/systems/event_effects.cpp` — `resolve_followup_ids`
  no longer silently skips unknown `id_code`; vacuous actors now
  fail; `select_best_option_for_country` returns `Result<const
  EventOption*>`.
- `src/leviathan/systems/event_engine.cpp` — propagates Result on
  every option score and follow-up lookup.
- `src/leviathan/systems/annual_stats.cpp` — `snapshot` is now
  `Result<AnnualRow>` and rejects an empty `state.countries`.
- `src/leviathan/runner/runner.cpp` — gates the year-boundary
  `annual_stats::snapshot` call with `!state.countries.empty()` (see
  §7 below).

## 3. Why strict no-clamp exposed a bounded-ratio update problem

Once the silent clamp was removed, the long-horizon AI policy loop
revealed an architectural tension: ratio targets live in `[0, 1]`,
linear `add` accumulates without bound, and a 25 567-day (1930→2000)
compliance sweep applies hundreds of policy effects per country. The
combination is incompatible with strict no-overshoot validation —
within a handful of months under linear-`add` the rejection rate
saturates.

The compliance scenario’s 1930→2000 run reproduced this failure mode
deterministically: `cut_military_budget` overshooting `faction:
military.support` at month 4, then a cascade of rejections that
never recovered. The candidate-validate-commit pattern guarantees
atomicity but cannot magically reduce the linear-`add` delta below
the remaining headroom.

## 4. Why asymptotic-add was selected as the rebalance

The fix is to change the **shape** of the update so that the strict
validator passes by construction on ratio targets. Replacing linear
`add` with an asymptotic form does that:

```
positive delta: new = old + delta * (1 - old)
negative delta: new = old + delta * old
```

Far from the bound, the formula is close to linear (small `delta`
on a 0.10 ratio lands close to `0.10 + delta`). Near the bound,
each successive `+delta` produces a smaller absolute change — the
shape converges toward `1.0` (or `0.0`) without crossing.

### Research-grounded — what the literature actually supports

- **Bounded institutional indicators / indexes are common in Polity-
  and V-Dem-style measurement.** This supports keeping the game's
  ratio fields on `[0, 1]` and supports the idea that ratio updates
  should respect a bound. Sources: Marshall & Jaggers 2002 *Polity
  IV Project: Dataset Users' Manual*; Coppedge et al. 2011 *V-Dem:
  A New Way to Measure Democracy*.
- **State capacity / institutions are slow-moving stocks in the
  Besley-Persson literature.** This supports having inertia and
  diminishing returns near the bound — it is consistent with an
  asymptotic update shape. Sources: Besley & Persson 2009 *The
  Origins of State Capacity: Property Rights, Taxation and
  Politics*; Besley & Persson 2010 *State Capacity, Conflict, and
  Development*.

### Model assumption — what is NOT proven by the literature

- **The exact functional form** `new = old + delta * (1 - old)` /
  `new = old + delta * old` is a **game-model choice** that is
  consistent with the bounded / diminishing-returns observations
  above. It is NOT derived from any single paper. A logistic update
  `new = sigmoid(logit(old) + k * delta)` would be an equally
  defensible alternative; this project picked the simpler closed
  form.
- **The specific deltas, drift rates, capacity thresholds, growth
  weights, and policy effect magnitudes** are authored gameplay
  parameters. None of them are calibrated against a named dataset.
- **The engineering consequence** that the strict validator passes
  by construction is a useful side-effect of the formula shape, not
  a research claim.

### Forbidden wording (kept here as a reminder)

These statements would overstate what the literature supports and
must not appear in design notes, source comments, or test docstrings:

- "Polity / V-Dem prove the `new = old + delta * (1 - old)` update
  equation."
- "Besley & Persson justify the magnitude `−0.04` for
  `cut_military_budget`'s effect on `country.military_power`."
- "The literature proves diminishing returns at the bound."

Use "modelling assumption inspired by …" or "consistent with the
bounded-indicator literature, but a game-model choice" wherever the
writing veers in this direction.

## 5. What is research-grounded vs game-model assumption

**Research-grounded** (supported as plausible by the cited
literature, not derived from a specific empirical estimate):

- The `[0, 1]` bound on ratio fields.
- The qualitative claim that bounded institutional indicators have
  diminishing-returns dynamics near saturation.
- The qualitative claim that state capacity is a slow-moving stock.
- The qualitative claim that growth accounting decomposes long-run
  output into additive contributions from capital, labour, human
  capital, and TFP-style residuals — see the linear `add` discussion
  in §7.

**Model assumption** (authored gameplay parameters; NOT measured
empirical estimates):

- The exact `delta * (1 - old)` / `delta * old` functional form.
- All AI capacity thresholds and pressure thresholds:
  `ai_policy.cpp::kPressureThreshold = 0.80`,
  `kCapacityLowMax = 0.30`, `kCapacityMediumMax = 0.60`.
- All drift rates:
  `faction_system.hpp::kSupportDriftRate = 0.05`,
  `kLoyaltyDriftRate = 0.10`,
  `stability_system.hpp::kStabilityDriftRate = 0.10`,
  `kEconomicGrowthWeight = 2.0`,
  `interest_group_system.hpp::kInterestGroupReactionRate = 0.05`,
  `kInterestGroupCountryFeedbackRate = 0.02`,
  `kInterestGroupAuthorityPressureRate = 0.01`.
- All economy growth coefficients:
  `economy_system.hpp::kBaseGrowth = 0.005`,
  `kEducationGrowthWeight = 0.005`,
  `kInfrastructureGrowthWeight = 0.005`,
  `kIndustryGrowthWeight = 0.010`,
  `kAdminEfficiencyGrowthWeight = 0.005`,
  `kPoliticalInstabilityDrag = 0.010`,
  `kCorruptionGrowthDrag = 0.005`.
- All information-accuracy weights and floor:
  `information_accuracy.hpp::kInformationAccuracyCapabilityWeight =
  0.7`, `kInformationAccuracyBudgetWeight = 0.3`,
  `kInformationAccuracyCorruptionWeight = 0.4`,
  `kMinInformationAccuracy = 0.4`.
- Every `value` in `data/policies/*.json` (per-policy effect
  magnitudes).

Game-balance tuning of these constants is **out of scope** for this
PR. The PR's scope is the formula shape + strict validation; the
coefficients ride along unchanged so that the canonical determinism
checks remain meaningful within the new baseline.

## 6. Linear `add` retained for non-ratio fields

`country.gdp`, `country.budget_balance`, `country.tax_revenue`, and
`faction.resources` are unbounded (or one-sided-bounded) numeric
stocks. Linear `add` on these fields is consistent with the
additive-flow pattern used in standard growth accounting (Barro-
style framework: long-run output is the sum of additive
contributions from capital deepening, labour growth, human capital,
and a TFP residual). The choice of additive flow is modelling-
aligned with that literature; the **specific coefficients** (the
seven growth-weight constants listed in §5) remain authored gameplay
parameters and NOT measured empirical estimates. We do not claim
Barro's papers calibrate the values of `kEducationGrowthWeight`,
etc.

Linear `add` on these unbounded fields still goes through strict
validation: the candidate must be finite. The strict guard rejects
`NaN`/`±Inf` and (where the field is documented non-negative) rejects
results that would land below zero — for example
`economy_system.cpp` rejects when the post-add `gdp` would be
negative, replacing the previous silent `if (c.gdp < 0) c.gdp = 0;`
floor.

## 7. Empty-world contract resolution

Strict validation requires `annual_stats::snapshot` to reject an
empty `state.countries`, because a "zero-row world-stats" output
is structurally meaningless. The empty-world `leviathan.exe` run
(no `--scenario` flag) remains a valid time-tick exercise; the
runner now **gates the year-boundary call** with
`!state.countries.empty()`. On an empty world the annual-stats CSV
simply does not emit a row at that year boundary. Tests pin this
behaviour.

## 8. Test triage rubric

The 58 failing test cases that surfaced when the asymptotic formula
landed were triaged into three buckets. The labels appear in the
inline test comments so future readers can see which kind of claim
each test is making.

### Bucket M — Mechanical invariant (synthetic values OK)

The test claims an algebraic / boundary / `Result`-propagation /
atomicity property. Updates: recompute the expected value with the
asymptotic formula and add a one-line `// Mechanical asymptotic-
add formula test: <inputs> → <expected>.` comment. Synthetic values
are explicitly allowed for this bucket because the test is
verifying formula mechanics, not a behavioural claim. Most of the
58 cases fell here.

### Bucket B — Behavioural (must be a trajectory-shape invariant)

The test claims a simulation trajectory or per-country behavioural
outcome. Updates: rewrite the assertion as a qualitative
trajectory-shape invariant ("stability increases and remains
bounded"; "each subsequent same-policy application produces smaller
marginal change") rather than a pinned numeric value. A small
number of cases were rewritten this way; the bias-noise distortion
behaviour and follow-up event chain were the largest examples.
Each rewritten test carries a one-line citation to the bounded /
diminishing-returns claim it pins.

### Bucket D — Delete (no longer meaningful)

A handful of tests had pinned the **silent-clamp** as the asserted
behaviour ("clamps to 1.0 on overshoot"). Under asymptotic-add,
overshoot is impossible — the assertion has no analogue. These
were either deleted or repurposed into a "rejects out-of-range
input" check. The trajectory tests in §9 cover the qualitative
shape these tested.

## 9. New trajectory tests

Added in `tests/systems/asymptotic_add_trajectory_test.cpp` per
`feedback_trajectory_observation_tests`. Four cases:

1. **Bounded-from-above approach** — repeated `+0.10` on
   `stability` starting at `0.20`. Asserts monotonic increase,
   no crossing of `1.0`, and strict diminishing marginal change.
2. **Bounded-from-below approach** — symmetric mirror with `-0.10`
   on `corruption` starting at `0.80`.
3. **No-overshoot fuzz** — 1 000 random `(old, delta)` pairs with
   `old ∈ [0, 1]`, `delta ∈ [-1, 1]`. Asserts the candidate stays
   in `[0, 1]` for every pair. Mechanical invariant.
4. **Symmetry around the midpoint** — algebraic property that
   positive and negative `add` of equal magnitude at symmetric
   starting points produce candidates equidistant from `0.5`.

The long-horizon strict-pass property is covered by the existing
`integration/rcr_1_rfc_compliance_test.cpp` 25 567-day sweep, which
still completes with `sanity_issues == 0`.

## 10. Determinism contract under the new baseline

This PR deliberately rebakes the canonical numeric baselines. The
`1930_minimal` 365-day save and the compliance 25 567-day save
both diverge from PR #114, because asymptotic-add changes the post-
value of every ratio-target `add`. The new baselines are
**self-consistent within this branch**:

- `cmake --build build --config Debug` produces a clean build.
- `build/bin/Debug/leviathan_tests.exe` reports `1251 / 1251`
  passing test cases, `95 876` assertions, `0` failures. The
  jump from the initial `1235 / 69 706` snapshot covers the
  17 additional hardening cases that landed alongside the
  Result-promotion of `weighted_choice`,
  `rank_weighted_events`, and `select_weighted_event` (NaN /
  ±Inf / negative / empty rejection + `state.rng.counter`
  stable on failure).
- `leviathan.exe --scenario data/scenarios/1930_minimal.json --days 365`
  completes with `Sanity issues : 0` and is byte-identical across
  repeated same-seed runs (same-branch determinism pin).
- `data/scenarios/1930_rfc_compliance.json` 25 567-day sweep
  (1930→2000) completes with `Sanity issues : 0`.

No byte-identical comparison against PR #114 is required or claimed
— the formula migration was a deliberate, documented break.

## 11. Forward work

- Game-balance tuning of the authored constants listed in §5 is a
  separate non-research PR. `feedback_numbers_from_research`
  governs new numeric formulas / parameters / thresholds going
  forward: any value representing economic / political / military /
  social behaviour must trace to an RFC-080 §2 cited source, or be
  explicitly labelled as a game-model assumption (like the
  constants above).
- RFC-090 progression resumes at M6.8 once explicitly green-lit by
  the user. This hardening PR does NOT advance the milestone
  counter.
- Two new feedback memories supporting this PR:
  `feedback_no_silent_degradation` (project-wide rule),
  `feedback_api_signature_expresses_failure` (signatures must
  return `Result<T>` if they can fail),
  `feedback_hardening_not_milestone` (cross-cutting sweeps live on
  `feature/hardening-{slug}` branches and disclaim milestone
  numbering),
  `feedback_numbers_from_research` (research-grounding discipline
  for behavioural numerics),
  `feedback_trajectory_observation_tests` (observe trajectory shape
  over time, not a single pinned value),
  `feedback_pr_framing_precision` (precise PR title / disclaimer
  wording for hardening sweeps).

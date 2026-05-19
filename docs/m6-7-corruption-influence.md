# M6.7 — corruption influence on `information_accuracy`

Companion notes for
`feature/m6-7-corruption-influence`.

**This is RFC-090 M6** (hidden truth / information distortion),
seventh sub-milestone. M6.7 implements **only** RFC-090 §6.7:

> 6.7 加入腐敗影響

M6.6 shipped the intelligence-budget baseline body of
`leviathan::systems::information_accuracy::compute_for_country`:
`accuracy = 0.4 + 0.6 × (0.7·intelligence_capability + 0.3·budget.intelligence)`
in `[0.4, 1.0]`. M6.7 layers on the `-Corruption` term per
RFC-080 §8 so corruption explicitly erodes accuracy.

## 1. The formula

`compute_for_country(state, country)` returns

```
intel_score    = 0.7 × country.government_authority.intelligence_capability
               + 0.3 × country.budget.intelligence
m6_6_baseline  = 0.4 + 0.6 × intel_score
accuracy       = m6_6_baseline - 0.4 × country.corruption
```

With all three ratio inputs validated to `[0, 1]`, accuracy is
in `[0.0, 1.0]` by construction:

| intel_score | corruption | accuracy | meaning                                   |
|-------------|-----------:|---------:|-------------------------------------------|
| 1.0         | 0.0        | 1.0      | no-distortion ceiling                     |
| 1.0         | 1.0        | 0.6      | high capacity but rotten — still partial  |
| 0.0         | 0.0        | 0.4      | M6.6 contribution floor                   |
| 0.0         | 1.0        | 0.0      | full blackout                             |

## 2. Public header surface

`include/leviathan/systems/information_accuracy.hpp` now exposes
four `inline constexpr` constants:

```cpp
inline constexpr double kPlaceholderInformationAccuracy        = 1.0;  // no-distortion ceiling
inline constexpr double kMinInformationAccuracy                = 0.4;  // M6.6 contribution floor
inline constexpr double kInformationAccuracyCapabilityWeight   = 0.7;  // M6.6
inline constexpr double kInformationAccuracyBudgetWeight       = 0.3;  // M6.6
inline constexpr double kInformationAccuracyCorruptionWeight   = 0.4;  // M6.7 (new)
```

`kInformationAccuracyCorruptionWeight = 0.4` is chosen
symmetrically with `kMinInformationAccuracy = 0.4`: maximum
corruption (1.0) can subtract exactly enough to drive the
zero-intelligence floor to 0.0. The function-level range
becomes the full `[0.0, 1.0]` interval.

`kMinInformationAccuracy`'s semantic also graduates:

- **M6.6**: function-level lower bound (corruption-blind).
- **M6.7+**: floor of the **M6.6 contribution alone**. The
  corruption subtraction can push the effective total below
  `kMinInformationAccuracy`; the constant documents the M6.6
  baseline term, not the helper's final lower bound.

## 3. Mapping to RFC-080 §8

RFC-080 §8 specifies the full information-accuracy formula as a
sum of positive intelligence-side terms and a sum of negative
distortion-side terms:

```
InformationAccuracy = BaseAccuracy
                    + IntelligenceCapacity
                    + MediaFreedomSignal
                    + BureaucraticProfessionalism
                    + AuditCapacity
                    - Corruption
                    - FactionCapture
                    - LeaderIsolation
                    - LocalAutonomyOpacity
```

After M6.7 the implementation covers two slots:

| Term                       | Status         | Sub-milestone     |
|----------------------------|----------------|-------------------|
| BaseAccuracy               | shipped        | M6.6 (floor 0.4)  |
| IntelligenceCapacity       | shipped        | M6.6 (split into capability + budget)        |
| MediaFreedomSignal         | deferred       | no RFC-090 task   |
| BureaucraticProfessionalism| deferred       | no RFC-090 task   |
| AuditCapacity              | deferred       | no RFC-090 task   |
| **Corruption**             | **shipped**    | **M6.7 (this PR)**|
| FactionCapture             | deferred       | no RFC-090 task   |
| LeaderIsolation            | deferred       | no RFC-090 task   |
| LocalAutonomyOpacity       | deferred       | no RFC-090 task   |

Per `feedback_stripped_formulas`, the deferred terms have no
RFC-090 task assigned yet and stay in the helper's docstring as
explicit forward references.

## 4. Validation policy change — `feedback_no_silent_degradation`

M6.6 used `std::clamp` defensively on finite out-of-range
intelligence inputs and rejected only the non-finite ones.
Reviewer guidance during the M6.7 kick-off (2026-05-19) was:

> **不要在異常數值出現時直接降級或適應處理而是要報錯**
>
> Don't silently degrade or adapt when abnormal values appear in
> code — report errors instead.

This is now captured in `memory/feedback_no_silent_degradation.md`.
M6.7 brings the whole `compute_for_country` body into compliance:

- Each of the three ratio inputs
  (`government_authority.intelligence_capability`,
  `budget.intelligence`, `corruption`) is validated through a
  single anonymous-namespace helper `require_unit_ratio`.
  Failure conditions: NaN, ±Inf, value < 0.0, value > 1.0.
- On failure the helper returns `Result::failure` with an error
  message that names the offending country `id_code`, the
  offending field, and the offending numeric value (e.g.,
  `"country 'GER' corruption = 1.500000 is not a finite ratio in
  [0, 1]"`).
- The check order is capability → budget → corruption, so the
  diagnostic message is deterministic when more than one input
  is bad.
- The output-side `std::clamp(accuracy, 0.0, 1.0)` immediately
  before `return` is retained as a single-ULP floating-point
  safety net (rounding may produce `1.0000000000000002` for
  some weight combinations). It is **not** an input-validation
  fallback — input validation lives in `require_unit_ratio`.

## 5. Out of scope for M6.7

The M6.7 PR strictly does NOT:

- bump the save schema. `country.corruption` has been on
  CountryState since M1.1; M6.6's intelligence inputs since
  M2.16 / M1.3. Save format stays at **v18**; artefact contract
  stays at **11**.
- add a debug-mode bypass (RFC-090 §6.8 scope).
- wire a downstream caller (RFC-090 §6.9 scope is the first
  non-debug consumer). `event_evaluator` / `event_firer` /
  `event_effects` / `event_engine` / `monthly_pipeline` /
  `runner` are all untouched. `reported_value::from_true_value`
  and `bias_noise::sample_for_event` are not modified.
- consume `state.rng`. M6.7 is a pure read; same state + same
  country → same result. Canonical determinism baselines stay
  byte-identical because no production caller exists.
- add the remaining RFC-080 §8 terms
  (`-FactionCapture` / `-LeaderIsolation` /
  `-LocalAutonomyOpacity` /
  `+MediaFreedomSignal` / `+BureaucraticProfessionalism` /
  `+AuditCapacity`). No RFC-090 sub-milestone covers those yet.
- add a `compute_for_event` variant. The event-actor lookup
  belongs to whichever future sub-milestone wires the helper
  into the event pipeline.
- mutate any scenario / data file.

## 6. Verification

- **1229 doctest cases / 64274 assertions** (verified via direct
  `leviathan_tests.exe` per `feedback_ctest_masks_doctest`).
  Net change vs PR #113: +13 cases / +78 assertions.
- The M6.7 test set covers:
  - **M6.6 baseline preserved at corruption = 0** — full
    affine formula pinned against seven `(cap, bud)` samples
    plus the capability/budget DOES-consult tests and the
    monotonicity sweep.
  - **M6.7 corruption term**:
    - zero corruption preserves the M6.6 baseline
    - max corruption + zero intel = 0.0 (full blackout)
    - max corruption + max intel = 0.6 (= 1.0 − 0.4)
    - documented formula pinned across seven
      `(cap, bud, corruption)` samples
    - monotonically non-increasing in corruption
    - corruption DOES affect accuracy (strictly different
      at low vs high corruption); delta is exactly
      `kInformationAccuracyCorruptionWeight × Δcorruption`
    - the M6.6 floor is NOT a lower bound on the M6.7 total
  - **strict input validation** (per
    `feedback_no_silent_degradation`):
    - finite out-of-range capability / budget / corruption all
      rejected
    - NaN / +Inf / -Inf capability / budget / corruption all
      rejected
    - error message names the offending field + the offending
      numeric value
    - deterministic short-circuit ordering: capability →
      budget → corruption
    - non-finite failure does not mutate state
  - preserved validation surface (invalid CountryId / empty
    state.countries / `CountryId::invalid()`)
  - preserved purity (no GameState mutation across two
    countries + a failed call, via save-layer serialize diff)
  - preserved determinism (three repeated calls)
  - stable public constants:
    `kPlaceholderInformationAccuracy = 1.0`,
    `kMinInformationAccuracy = 0.4`, weights sum to 1.0,
    `kInformationAccuracyCorruptionWeight = 0.4 ==
    kMinInformationAccuracy`.
- **Canonical 1930_minimal 365-day run produces byte-identical
  save.json to both the PR #111 baseline AND the PR #113
  baseline.** Verified via `diff` returning empty against
  `out/issue_112_canonical_365/save.json` and
  `out/m6_6_canonical_365/save.json`. M1.17 / M2 / M3 / M4 / M5
  byte-identical determinism baselines stay green because no
  production code path calls `compute_for_country`.

## 7. Reviewer-discipline notes

- **No silent degradation**: per
  `feedback_no_silent_degradation`, abnormal numerics must fail
  loudly. M6.7's `require_unit_ratio` helper enforces this for
  the whole `compute_for_country` body — both the M6.7 corruption
  input AND M6.6's pre-existing intelligence inputs (which were
  silently clamped in PR #113). The PR brings the whole helper
  into one consistent discipline.
- **Stripped formula, document deferred**: per
  `feedback_stripped_formulas`, M6.7 ships the RFC-080 §8
  `-Corruption` term while explicitly enumerating the remaining
  five deferred terms in the cpp / hpp / design note.
- **No save format bump unless adding persistent field**: per
  `feedback_save_version`, `country.corruption` already exists;
  save schema stays at v18.
- **Helper-only vs. wired behaviour**: per
  `feedback_rfc_is_contract`, an RFC item is complete only when
  wired into ordinary simulation. RFC-090 §6.7 covers the
  *corruption influence on `information_accuracy`* only; the §M6
  family as a whole becomes wired when RFC-090 §6.9 consumes the
  accuracy in normal play. The audit-doc §6.7 row marks as
  wired only when a downstream caller exists.

# M6.6 — intelligence-budget influence on `information_accuracy`

Companion notes for
`feature/m6-6-intelligence-budget-influence`.

**This is RFC-090 M6** (hidden truth / information distortion),
sixth sub-milestone. M6.6 implements **only** RFC-090 §6.6:

> 6.6 加入情報預算影響

M6.3 shipped the function shape of
`leviathan::systems::information_accuracy::compute_for_country`
with a placeholder body that always returned the public constant
`kPlaceholderInformationAccuracy = 1.0`. M6.6 replaces that body
with the intelligence-budget formula and graduates the range from
the single-point `{1.0}` to the closed interval `[0.4, 1.0]`.

## 1. The formula

`compute_for_country(state, country)` returns

```
intel_score = 0.7 × country.government_authority.intelligence_capability
            + 0.3 × country.budget.intelligence
accuracy    = 0.4 + 0.6 × intel_score
```

Both intelligence inputs are clamped to `[0, 1]` defensively
before the weighted sum (the data layer already pins ratios in
range; the clamp guards against hand-built test fixtures and
future schema drift). The result is then clamped one more time
into `[kMinInformationAccuracy, 1.0] = [0.4, 1.0]` for the same
defensive reason.

Non-finite inputs are handled separately. `std::clamp(NaN, 0, 1)`
returns NaN, so the helper first checks each raw intelligence
input with `std::isfinite` and returns `Result::failure` (naming
the offending `country.id_code` and the offending field) if
either value is NaN or ±Inf. This prevents a corrupted state
from leaking a NaN accuracy past the documented closed range and
poisoning the downstream consumer (M6.4 `reported_value::from_true_value`,
which propagates the NaN into reported numeric values). Finite
out-of-range values still clamp defensively — only non-finite
inputs fail loudly.

Two existing CountryState fields feed the formula:

- `country.government_authority.intelligence_capability`
  ([0, 1], defaults to 0.5) — M2.16 RFC-020 §3 "state control"
  ratio. The more direct signal: how capable the country's
  intelligence apparatus is at producing accurate reports
  *right now*.
- `country.budget.intelligence` ([0, 1], defaults to 0.0) — M1.3
  RFC-030 per-category budget allocation. The slower-moving
  funding side; would feed capability over time once a future
  M-driver wires the long-term relationship. M6.6 reads it
  directly because no such driver exists yet.

## 2. Public header surface

`include/leviathan/systems/information_accuracy.hpp` exposes
three new `inline constexpr` constants alongside the existing
`kPlaceholderInformationAccuracy`:

```cpp
inline constexpr double kPlaceholderInformationAccuracy        = 1.0;  // "no-distortion ceiling"
inline constexpr double kMinInformationAccuracy                = 0.4;  // M6.6 floor
inline constexpr double kInformationAccuracyCapabilityWeight   = 0.7;
inline constexpr double kInformationAccuracyBudgetWeight       = 0.3;
```

Why expose every load-bearing number:

- Tests pin the formula against the constants rather than the
  literal numbers, so a future M-driver rebalance (changing
  weights to e.g. `0.6 / 0.4`) only needs to edit the header.
- M6.7 (corruption term) and M6.9 (non-debug consumer) will read
  `kMinInformationAccuracy` to reason about the M6.6 contribution
  when layering on top — M6.7 subtracts a corruption term that
  may push effective accuracy below 0.4; this constant pins the
  M6.6 contribution alone.

`kPlaceholderInformationAccuracy` is unchanged numerically — but
its semantic graduated:

- **M6.3**: "the value the placeholder body returns
  unconditionally."
- **M6.6+**: "the no-distortion ceiling — the value
  `compute_for_country` returns when both intelligence inputs
  are maxed (1.0)."

## 3. Why an affine floor rather than a pure multiplier

The naive interpretation of RFC-090 §6.6 ("加入情報預算影響") is
`accuracy = intel_score` directly. M6.6 deliberately wraps that
in `0.4 + 0.6 × intel_score`:

- RFC-080 §8 lays out the full accuracy formula starting with a
  `BaseAccuracy` term: `BaseAccuracy + IntelligenceCapacity +
  MediaFreedomSignal + BureaucraticProfessionalism +
  AuditCapacity - Corruption - FactionCapture - LeaderIsolation
  - LocalAutonomyOpacity`. The M6.6 affine wrap reserves the 0.4
  floor as that BaseAccuracy contribution — a country with no
  intelligence apparatus still gets a baseline-grade report
  rather than a complete blackout.
- The 0.6 multiplier on `intel_score` keeps the per-input
  contribution bounded in `[0, 0.6]`, leaving headroom for the
  remaining RFC-080 §8 terms when their own RFC-090
  sub-milestones land (RFC-090 §6.7 covers corruption; the
  other terms have no §M6 task assigned yet). Sub-milestones
  that *subtract* from accuracy can push the effective result
  below the M6.6 floor — only the M6.6 contribution itself
  stays at or above 0.4.

## 4. Composition with the M6 pipeline

```
M6.3 helper (NOW M6.6 body)          M6.4 reported_value           M6.5 bias_noise          RFC-090 §6.9 (future)
information_accuracy::                reported_value::               bias_noise::             composes M6.4 reported +
compute_for_country(state, country)   from_true_value(true, acc)    sample_for_event(...)    M6.5 noise to hide
  ↳ returns accuracy ∈ [0.4, 1.0]      ↳ returns true × accuracy     ↳ returns deterministic   visible_report toward
                                                                       hash noise in           true_cause
                                                                       [-amp, +amp]
```

`compute_for_country` is the upstream accuracy producer; M6.4
`from_true_value` is the only direct consumer. M6.5
`sample_for_event` runs in parallel and does *not* depend on
accuracy.

M6.4's reported-value test was rewritten to assert the M6.6
contract: a country with maxed intelligence
(`intelligence_capability=1.0, budget.intelligence=1.0`) still
returns 1.0 (the truth verbatim is preserved); a country with
zero intelligence on both axes returns 0.4 (true_value damps
toward zero proportionally).

## 5. Out of scope for M6.6 (forward-stable)

The M6.6 PR strictly does NOT:

- bump the save schema. Both intelligence inputs already live on
  `CountryState` since M2.16 / M1.3; no new persistent field
  ships. Save format stays at v18 (the issue #112 corrective bump
  remains the most recent save change); artefact contract stays
  at 11.
- add a corruption term (RFC-090 §6.7 / M6.7).
- add a debug-mode bypass (RFC-090 §6.8 / M6.8).
- wire a downstream caller (RFC-090 §6.9 / M6.9 will be the
  first). `event_evaluator` / `event_firer` / `event_effects` /
  `event_engine` / `monthly_pipeline` / `runner` are all
  untouched. Helpers `reported_value::from_true_value` and
  `bias_noise::sample_for_event` are not modified — the only
  call site changes are test-only.
- consume `state.rng`. M6.6 is a pure read; same state + same
  country → same result. Canonical determinism baselines stay
  byte-identical because no production caller exists yet.
- modify the M6.5 bias / noise hash design. The noise primitive
  uses its own FNV-1a + splitmix64 path (per M6.5's load-bearing
  decision); M6.6 doesn't touch it.
- add a `compute_for_event` variant. RFC-090 §6.6's task is the
  per-country influence; a per-event helper would belong to
  whichever future sub-milestone wires the helper into the
  event pipeline.
- mutate any scenario / data file. The compliance scenario's
  20-country fixture already authors both intelligence inputs
  from its 1930 baseline values; M6.6 reads what's already
  there.

## 6. Verification

- **1216 doctest cases / 64196 assertions** (verified via direct
  `leviathan_tests.exe` per the `feedback_ctest_masks_doctest`
  rule). The M6.6 test set is 26 cases:
  - maxed-input → 1.0; zero-input → 0.4
  - pinned affine formula across seven `(cap, bud)` samples
  - range invariant over a swept `(cap, bud)` grid
  - DOES consult `intelligence_capability` (low-vs-high)
  - DOES consult `budget.intelligence` (low-vs-high)
  - capability weight dominates budget weight
  - monotonic in both axes (sweep test, prev ≤ current)
  - out-of-range inputs clamped defensively (negative + >1)
  - **non-finite intelligence_capability rejected** for NaN /
    +Inf / -Inf — error message names the offending country
    `id_code` and the offending field
  - **non-finite budget.intelligence rejected** for NaN /
    +Inf / -Inf — error message names the offending country
    `id_code` and the offending field
  - **capability-NaN short-circuits before budget-NaN** so the
    failure message is deterministic when both inputs are bad
  - **non-finite failure does not mutate state** (raw field
    values unchanged after a failing call)
  - does NOT consume corruption (M6.7 scope) — two states
    differing only in corruption return identical accuracy
  - preserved validation surface: invalid CountryId / empty
    state.countries / `CountryId::invalid()`
  - preserved purity (no GameState mutation across two
    countries + a failed call, via save-layer serialize diff)
  - preserved determinism (three repeated calls)
  - stable public constants: `kPlaceholderInformationAccuracy =
    1.0`, `kMinInformationAccuracy = 0.4`, weights sum to 1.0
- **Reported-value composition test rewritten**: the M6.4 →
  M6.3 composition test that asserted "any country yields
  accuracy=1.0, reported equals true_value verbatim" was rewritten
  to author full intelligence on the country
  (`intelligence_capability=1.0, budget.intelligence=1.0`) so the
  assertion still holds under M6.6. A new partner test pins the
  damping case: zero-intel country yields `accuracy = 0.4`,
  reported = `0.4 × true_value`.
- **Canonical 1930_minimal 365-day run produces byte-identical
  save.json to the PR #111 baseline.** Verified via
  `diff out/issue_112_canonical_365/save.json
       out/m6_6_canonical_365/save.json` returning empty. M1.17 /
  M2 / M3 / M4 / M5 byte-identical determinism baselines stay
  green because no production code path calls
  `compute_for_country` yet.

## 7. Reviewer-discipline notes

- **Respect RFC over preserving skeleton minimalism**: M6.6 is
  the first M6 sub-milestone to ship a real formula body rather
  than a skeleton. Per `feedback_respect_rfc_over_skeleton`,
  shipping the actual RFC-described behaviour beats clinging to
  the placeholder.
- **No save format bump unless adding persistent field**: per
  `feedback_save_version`, M6.6 reads two existing fields and
  adds none, so the save schema stays at v18.
- **Helper-only vs. wired behaviour**: per `feedback_rfc_is_contract`,
  an RFC item is complete only when wired into ordinary
  simulation. RFC-090 §6.6 covers the *intelligence-budget
  influence on `information_accuracy`* only; the §M6 family as
  a whole is wired when RFC-090 §6.9 (`非 debug 模式隱藏真相`)
  consumes the accuracy in normal play. M6.6 on its own is
  observable through tests and the public API; the audit-doc
  row for §6.6 should mark as wired only when a downstream
  caller exists.

# M1.8 - Economy month-end tick (minimal)

Companion notes for `feature/m1-08-economy-tick`. The second
country-side dynamic (after M1.7 stability). Same template:
explicit-call free function, no monthly tick / runner / AI / event
wiring.

## 1. API

```cpp
namespace leviathan::systems::economy {

struct EconomyOutcome {
    double previous_gdp;
    double new_gdp;
    double tax_revenue;        // overwrites state.country.tax_revenue
    double expenditure;        // not stored on state
    double budget_delta;       // tax_revenue - expenditure
    double new_budget_balance;
    double gdp_growth_rate;    // fractional, e.g. 0.005 = +0.5%
};

// All eight constants are header-exported.
inline constexpr double kExpenditureScale            = 0.20;
inline constexpr double kBaseGrowth                  = 0.005;
inline constexpr double kEducationGrowthWeight       = 0.005;
inline constexpr double kInfrastructureGrowthWeight  = 0.005;
inline constexpr double kIndustryGrowthWeight        = 0.010;
inline constexpr double kAdminEfficiencyGrowthWeight = 0.005;
inline constexpr double kPoliticalInstabilityDrag    = 0.010;
inline constexpr double kCorruptionGrowthDrag        = 0.005;

core::Result<EconomyOutcome> tick(core::GameState& state,
                                  core::CountryId  country);

}
```

## 2. Three formulas

### Tax revenue (RFC-080 §3 verbatim)

```text
tax_revenue = gdp
            * legal_tax_burden
            * fiscal_capacity
            * central_control
            * (1 - corruption)
```

Overwrites `state.country.tax_revenue` (per-tick amount, not
cumulative). The cumulative net is what `budget_balance` tracks.

Pinned to exact arithmetic by the
`"tax_revenue uses the RFC-080 §3 formula exactly"` test with the
canonical GER 1930 inputs: `100 × 0.20 × 0.50 × 0.60 × 0.75 = 4.5`.

### Expenditure (M1.8 simplification)

```text
sum_budget   = administration + military + education + welfare
             + intelligence + infrastructure + industry
expenditure  = gdp * sum_budget * 0.20
```

With `sum_budget = 1.0` (canonical "balanced" budget), expenditure is
20% of GDP. Under- or over-allocated budgets produce proportionally
smaller / larger spending obligations. M1.3 deliberately did NOT
enforce `sum_budget == 1.0`, and this formula stays consistent with
that: a budget that sums to 0.5 spends half as much, a budget that
sums to 1.5 spends 50% more.

### Budget balance

```text
budget_balance += (tax_revenue - expenditure)
```

Can go negative (deficit accumulates). **No clamp** — this is the
signal the future budget-crisis term in stability tick (RFC-080 §5)
will read.

### GDP growth (stripped-down RFC-080 §4)

```text
growth_rate = kBaseGrowth                              (= 0.005)
            + kEducationGrowthWeight       * budget.education
            + kInfrastructureGrowthWeight  * budget.infrastructure
            + kIndustryGrowthWeight        * budget.industry
            + kAdminEfficiencyGrowthWeight * administrative_efficiency
            - kPoliticalInstabilityDrag    * (1 - stability)
            - kCorruptionGrowthDrag        * corruption

gdp *= (1 + growth_rate)
```

Constants sized so the canonical GER 1930 country
(`stability = 0.55`, `corruption = 0.25`, `admin_efficiency = 0.55`,
balanced budget) produces:

```
0.005
+ 0.005*0.10  + 0.005*0.10 + 0.010*0.05 + 0.005*0.55
- 0.010*0.45  - 0.005*0.25
= 0.00350   (0.35% monthly, compounds to ~4.3% / year)
```

Compounded 12 times: `(1.0035)^12 ≈ 1.0428`. Pinned by the
`"12 monthly ticks compound to roughly annual baseline"` test.

`gdp` is **floored at 0** so a chain of pathological recessions can
asymptote toward zero but never go negative. (With current
constants and any DataLoader-valid input, the worst single-tick
`growth_rate` is `−0.010` — a 1% shrink — so the floor is defensive
only.)

### Skipped RFC-080 §4 terms

`InflationPressure` and `WarDamage` are deliberately omitted —
neither has input data in `GameState` yet. They land as additive
extensions once inflation / war systems exist:

```text
- b_inflation * inflation_pressure   // M1.x
- b_war       * war_damage           // M1.x
```

## 3. State touched

- **WRITES** `c.gdp`, `c.tax_revenue`, `c.budget_balance` only.
- **READS** every other CountryState numeric field (legal_tax_burden,
  fiscal_capacity, central_control, corruption, stability,
  administrative_efficiency, every `budget.*`).
- **DOES NOT WRITE** any other country field. Pinned by the "does
  NOT modify country fields outside the economy" test that
  verifies 11 fields are unchanged after a tick.
- **DOES NOT TOUCH** faction state, save state, RNG, or logs.

## 4. Test coverage (19 cases)

**Tax revenue (2)**: exact arithmetic; overwrite-not-accumulate.

**Expenditure (2)**: exact arithmetic; under-allocated budget produces
proportionally smaller spend.

**Budget balance (2)**: delta = revenue − expenditure; accumulates
across ticks (verified to be approximately doubled after 2 ticks).

**Growth (5)**: exact arithmetic with all seven terms exercised;
constants match spec; gdp=0 edge case (growth still computed,
applies to 0); positive-growth case; recession case
(`growth_rate = −0.010` exactly).

**Multi-step (1)**: 12 ticks compound to `100 → ~104.28`.

**Isolation (3)**: only target country touched; faction state
unchanged; eleven non-economy CountryState fields unchanged.

**Outcome (1)**: every struct field populated correctly.

**Error paths (3)**: invalid CountryId, default-constructed CountryId,
empty state — all rejected with state unchanged.

## 5. What M1.8 deliberately does NOT do

- **No monthly tick / runner wiring.** Caller decides when to tick.
- **No inflation.** No inflation field, no monetary policy.
- **No war damage.** No war system.
- **No debt / interest.** Negative `budget_balance` doesn't accrue
  interest yet.
- **No category-level spending breakdown.** The expenditure formula
  collapses every budget category into a single scaled sum. Per-
  category spend tracking lands when factions react to specific
  categories (M1.x).
- **No forced budget cuts on extreme deficit.** Deficit just
  accumulates.
- **No coupling with M1.7 stability tick.** Stability doesn't yet
  read `gdp_growth_rate` as the RFC-080 §5
  `EconomicGrowth` input; that's a follow-up that requires deciding
  where to store the latest growth rate (probably a new
  `last_gdp_growth_rate` field on CountryState).
- **No production / trade / industries.** Pure aggregate.
- **No `Diagnostics::sanity_check` rule** for "budget_balance grew
  by an implausible amount this tick".

## 6. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/systems/economy_system.hpp` | **new** — public API + 8 constants |
| `src/leviathan/systems/economy_system.cpp` | **new** — implementation |
| `src/leviathan/systems/CMakeLists.txt` | adds `economy_system.cpp` |
| `tests/systems/economy_system_test.cpp` | **new** — 19 cases |
| `tests/CMakeLists.txt` | adds the test source |
| `docs/m1-8-economy-tick.md` | this file |
| `docs/README.md`, `README.md`, `rfc/README.md` | status / progress notes |

Total tests: **288 → 307** (+19).

## 7. Risks / things to watch

- **All eight constants are placeholders.** Tuned for testability:
  the canonical GER 1930 country produces a slightly positive
  monthly growth rate that compounds to a sensible-looking annual
  figure. A real balance pass after M1.9 monthly-pipeline
  integration should treat them as variables to fit.
- **`expenditure` formula doesn't enforce budget allocation
  semantics.** A country with `sum_budget = 1.5` spends 1.5× the
  baseline. That's the deliberate consequence of M1.3's "no
  sum-to-1 enforcement" decision; if that decision ever reverses,
  the formula doesn't need to change.
- **GDP grows multiplicatively each tick.** Long-running games will
  see large GDP numbers. No nominal-vs-real distinction; no
  inflation deflator. Future work.
- **No interaction with M1.5 PolicySystem yet.** A policy that adds
  to `country.budget.military` will shift the next economy tick's
  spend, but a one-shot policy doesn't accumulate. The monthly
  pipeline (M1.9) will decide the canonical order.
- **`gdp` floor at 0** is defensive. Current constants can't drive
  growth below `−0.010` per tick under DataLoader-valid input, so
  the floor never fires in well-formed scenarios. A future PR that
  adds `InflationPressure` or `WarDamage` terms with bigger
  coefficients should re-validate the floor's necessity.

## 8. Next sub-milestone

The natural follow-up is **M1.9 — monthly pipeline**: compose the
four explicit-call building blocks into one caller, invoked on the
month-boundary signal from M0.4 TimeSystem:

```
// Hypothetical monthly_tick(state, actor):
policy::apply_policy_effects(state, actor, enacted_policy);  // if any enacted this month
faction::react(state, actor);
stability::tick(state, actor);
economy::tick(state, actor);
```

After that, the runner can wire `monthly_tick` to
`TickResult.month_changed` and we'll have a fully autonomous
single-country simulation.

Per the M1 pacing rule: do **not** start M1.9 until M1.8 is reviewed
and merged.

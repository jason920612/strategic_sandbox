# M1.7 - Stability tick (minimal)

Companion notes for `feature/m1-07-stability-tick`. The first
country-side dynamic in the project. Same template as M1.6: explicit
call, free function, deterministic linear-toward-equilibrium step,
two-digit constants chosen for testability.

Per the M1.6 review: **keep it explicit-call. Do not wire into
runner or monthly pipeline yet.**

## 1. API

```cpp
namespace leviathan::systems::stability {

struct StabilityOutcome {
    double previous_stability;
    double new_stability;
    double target_stability;   // value new_stability is drifting toward
};

inline constexpr double kSupportWeight    = 0.5;
inline constexpr double kLegitimacyWeight = 0.5;
inline constexpr double kCorruptionWeight = 0.3;
inline constexpr double kRadicalismWeight = 0.2;
inline constexpr double kStabilityDriftRate = 0.10;

inline constexpr double kNoFactionsSupportDefault    = 0.5;
inline constexpr double kNoFactionsRadicalismDefault = 0.5;

core::Result<StabilityOutcome> tick(core::GameState& state,
                                    core::CountryId  country);

}
```

Constants are header-exported so tests reference them by name. A
balance-tuning PR can find every knob in one place.

## 2. Formula

A **stripped-down** RFC-080 §5. Only consumes inputs that exist in
`GameState` as of M1.6:

```text
avg_support     = mean(f.support  for f in factions if f.country == actor)
                | kNoFactionsSupportDefault   when no factions match
avg_radicalism  = mean(f.radicalism for f in factions if f.country == actor)
                | kNoFactionsRadicalismDefault when no factions match

raw_target = 0.5 * avg_support
           + 0.5 * country.legitimacy
           - 0.3 * country.corruption
           - 0.2 * avg_radicalism

target     = clamp(raw_target, 0, 1)
stability += (target - stability) * 0.10
stability  = clamp(stability, 0, 1)
```

### What this formula intentionally skips

RFC-080 §5's full formula also references:

- `WelfareSatisfaction` — needs a welfare-budget-vs-population
  derivation; not in M1.7 scope.
- `EconomicGrowth` — needs `GDP delta`; M1.8 economy tick.
- `InequalityProxy` — no inequality data in `GameState` yet.
- `WarWeariness` — no war system.
- `BudgetCrisis` — could be derived from `budget_balance < threshold`;
  deferred until economy tick lands so the threshold is meaningful.

Adding these is a strict superset operation: each new input adds
another `± weight * value` term to `raw_target`, and the existing
tests don't have to change as long as the existing weights stay
where they are.

### Why default to 0.5 when no factions match?

`0.5` is the neutral midpoint of a ratio. "No factions known" doesn't
mean "no support" or "no radicalism" — it means "no political
pressure either way". With 0.5 / 0.5 defaults, a country with no
factions has `raw_target = 0.5*0.5 + 0.5*legitimacy - 0.3*corruption
- 0.2*0.5`, which leaves legitimacy and corruption as the dominant
inputs. Pinned by the
`"tick: country with no factions uses 0.5 / 0.5 defaults"` test.

### Why a stripped-down formula in M1.7?

Two reasons:

1. **Inputs that don't exist yet shouldn't be faked.** Defaulting
   them to plausible values is worse than leaving them out —
   tests would lock in numbers that have no source of truth.
2. **The full formula is a strict superset.** Adding a term in M1.8
   (`+ c3 * economic_growth`) is mechanical; the tests for the
   existing terms still hold because they exercise `economic_growth
   = 0` paths.

## 3. State touched

- **WRITES** `state.countries[country].stability` only.
- **READS** `country.legitimacy`, `country.corruption`,
  `state.factions[*].country`, `state.factions[*].support`,
  `state.factions[*].radicalism`.
- **DOES NOT WRITE** to any faction field. Pinned by the
  `"tick: does NOT modify any faction state"` test, which verifies
  all 5 faction fields are unchanged after a tick.

## 4. Country filter

`tick(state, X)` iterates **all** of `state.factions` and only
includes those whose `FactionState::country == X` in the averages.
Factions belonging to other countries are skipped. Pinned by:

- `"tick: country with only foreign-country factions uses defaults"`
- `"tick: averages only factions in the target country"`
- `"tick: only target country's stability changes"`

## 5. Clamping

Two distinct clamps:

1. **Target clamp**: `raw_target` after the linear combo can be
   negative (heavy corruption + high radicalism) or above 1
   (very supportive factions + very legitimate regime). Clamp to
   `[0, 1]` before drifting.
2. **Stability clamp**: even with a clamped target, drift from a
   pathological starting `stability` (e.g. `1.2` or `-0.5`) can
   produce out-of-range values. Clamp again after the drift.

Both are pinned by dedicated tests with pathological inputs.

## 6. Convergence

Geometric decay at rate `0.10` per call. From `stability=0.20` toward
`target=0.50`, after 50 steps the remaining error is approximately
`0.9^50 * |0.50 - 0.20| ~= 0.005 * 0.30 = 0.0015`. The test uses
`epsilon(0.01)` matching, which is the right semantics for
geometric (not exact) convergence.

## 7. Drive-by from PR #17 review — none

PR #17's reviewer found and the M1.6 fix-up handled the `<cmath>`
include. No new drive-bys in M1.7.

## 8. What M1.7 deliberately does NOT do

- **No monthly tick / TimeSystem integration.** The runner doesn't
  call `tick`. Caller decides when.
- **No economy or budget influence on stability** (deferred to M1.8
  once economic_growth / budget_crisis are computed).
- **No `WelfareSatisfaction`, `InequalityProxy`, `WarWeariness`
  terms** — those need inputs that don't exist yet (welfare-
  satisfaction derivation, inequality data, war system).
- **No type-weighted faction averages.** All factions in the country
  contribute equally. Influence-weighted averages would be a richer
  signal but require more design.
- **No `Diagnostics::sanity_check` rule** for stability bounds.
  Clamping makes it trivially `[0, 1]` after a tick.
- **No save format change.** Mutating `country.stability` doesn't
  change the on-disk shape; save format stays at v5.
- **No coupling with FactionSystem `react`.** A caller that wants
  both per "month" would compose them explicitly (or wait for
  M1.x's monthly pipeline).

## 9. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/systems/stability_system.hpp` | **new** — public API + constants |
| `src/leviathan/systems/stability_system.cpp` | **new** — implementation |
| `src/leviathan/systems/CMakeLists.txt` | adds `stability_system.cpp` |
| `tests/systems/stability_system_test.cpp` | **new** — 16 cases |
| `tests/CMakeLists.txt` | adds the test source |
| `docs/m1-7-stability-tick.md` | this file |
| `docs/README.md`, `README.md`, `rfc/README.md` | status / progress notes |

Total tests: **272 → 288** (+16).

## 10. Risks / things to watch

- **Coefficients are placeholders.** `0.5 / 0.5 / 0.3 / 0.2 / 0.10`
  chosen for testability and ensuring the formula spans the full
  `[0, 1]` target range at extreme inputs. M1.8+ balance work
  should treat them as variables to tune.
- **No-factions defaults bias the answer toward neutral.** A
  country with deliberately zero factions doesn't get penalised
  for "no political base"; M1.x can revisit if it matters for
  gameplay.
- **The formula assumes ratio fields are pre-clamped to `[0, 1]`.**
  DataLoader enforces this for config-loaded countries; M1.5
  PolicySystem clamps after applying effects. If a future system
  mutates `country.legitimacy` without clamping, the target
  computation still works (raw_target is clamped before drift),
  but the input semantics break.
- **`influence` and `loyalty` from `FactionState` are not consumed
  yet.** A future stability-tick variant should plausibly use
  `influence`-weighted averages (more politically powerful factions
  drive stability more). Out of scope for M1.7.

## 11. Next sub-milestone

Likely candidates (RFC-090 §M1):

- **M1.8 — Economy month-end tick** per RFC-080 §3 / §4: derive
  `tax_revenue = GDP × LegalTaxBurden × FiscalCapacity × CentralControl × (1 - Corruption)`,
  step `GDP` based on infrastructure / industry investment, rebalance
  `budget_balance` against tax revenue minus spending. Once this
  lands, M1.x can extend the stability formula with
  `economic_growth` and `budget_crisis` terms.
- **M1.9 — Monthly pipeline** wiring policy enactment → faction
  reaction → stability tick → economy tick into a single explicit
  caller. After that, the runner can drive the pipeline on month
  boundaries (using the M0.4 `TickResult.month_changed` flag).

M1.5 + M1.6 + M1.7 are the three building blocks the monthly
pipeline will compose:

```
policy::apply_policy_effects(state, actor, p);   // when a policy enacts
faction::react(state, actor);                    // every month
stability::tick(state, actor);                   // every month, after react
```

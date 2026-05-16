# M1.12 - Economy → Stability coupling

Companion notes for `feature/m1-12-economy-stability-coupling`. M1.12
adds **one new state field** + **one new formula term** + **one save-
schema bump**. It does not change the monthly pipeline ordering and
does not introduce any new gameplay system.

## 1. Scope

`stability::tick` (M1.7) was always supposed to read RFC-080 §5's
`EconomicGrowth` term, but M1.7 shipped a stripped-down formula
because no economy growth rate existed yet. M1.8 introduced
`gdp_growth_rate` as a per-tick local in EconomySystem; M1.12
**persists** it on CountryState so stability can read it.

That's the whole PR.

## 2. New state field

```cpp
struct CountryState {
    // … all the existing fields …
    double last_gdp_growth_rate = 0.0;   // M1.12
};
```

Semantics:

- **Fractional monthly growth rate.** `0.0035 == +0.35% / month`.
  Same magnitude as `EconomyOutcome::gdp_growth_rate`.
- **Default 0.0.** Fresh `GameState`s and DataLoader-parsed
  countries both start at 0.
- **Written by `economy::tick`** on every successful tick (including
  the gdp == 0 edge case). Overwrites the previous value.
- **Read by `stability::tick`** as the RFC-080 §5 `EconomicGrowth`
  input.
- **Persisted** in saves (required field as of save format v6).
- **Not loaded from country JSON.** Treated as runtime-only; the
  on-disk country fixtures stay unchanged.

## 3. Save format bump: v5 → v6

```cpp
inline constexpr std::uint32_t kSaveFormatVersion = 6;
```

`country_to_json` writes `last_gdp_growth_rate` after
`threat_perception`; `country_from_json` requires it (no default
fallback). The strict-equality version gate at load time
already rejects v5 saves with the canonical
"unsupported save_version 5 (this binary supports 6)" message.

Regression tests pin:

- A handcrafted v5 save is rejected loudly (regression for the bump
  rule).
- A v6 save that omits `last_gdp_growth_rate` is rejected with the
  field name in the message.
- A v6 save round-trip preserves the value exactly.

This is the **first M1 save-schema bump.** Previous M1.x PRs all
explicitly avoided changes that would force one; M1.12 finally
spends that budget because adding the field as runtime-only would
mean losing it on save/load (or worse: silently defaulting back to
0 mid-run).

## 4. EconomySystem write

```cpp
// economy_system.cpp - after computing out.new_gdp:
c.last_gdp_growth_rate = out.gdp_growth_rate;
```

Placement notes:

- Runs unconditionally on every successful tick.
- Runs even when `gdp == 0` (the formula doesn't depend on GDP);
  pinned by test.
- Does NOT run on `invalid CountryId` failure — the function
  returns earlier without mutating state.
- Order within the function: AFTER the GDP floor branch, so the
  field reflects exactly the rate that was just applied (or
  attempted, in pathological cases).

## 5. StabilitySystem term

```cpp
// stability_system.hpp
inline constexpr double kEconomicGrowthWeight = 2.0;
```

```text
target =  kSupportWeight        * avg_faction_support
       +  kLegitimacyWeight     * country.legitimacy
       -  kCorruptionWeight     * country.corruption
       -  kRadicalismWeight     * avg_faction_radicalism
       +  kEconomicGrowthWeight * country.last_gdp_growth_rate   ← M1.12
```

### Why 2.0?

`last_gdp_growth_rate` is a fractional monthly value, typically very
small. For the canonical GER 1930 baseline (monthly growth
`0.00350`), the term contributes `2.0 × 0.00350 = 0.0070` to the
target — comparable in magnitude to a 1% legitimacy swing.

Trade-offs considered:

- **Too small (e.g. 0.5)**: the term would be drowned out by every
  other input. Stability wouldn't visibly track economic health.
- **Too large (e.g. 20)**: a `+0.35% / month` economy would push
  target by `0.07`, almost overwhelming corruption / radicalism
  terms. A one-month recession spike would slam target around.
- **2.0**: meaningful at the margin (≈0.007 typical contribution),
  not large enough to dominate.

This is a *placeholder for testability*, not a balance-pass result.
A later balance PR may move it; M1.12 deliberately doesn't do that.

### Pre-M1.12 regression guard

Every M1.7 stability test that used a default-built `CountryState`
saw `last_gdp_growth_rate = 0.0`, so the EconomicGrowth term
contributes nothing and the M1.7 test arithmetic still holds. A
dedicated test pins this: same M1.7 setup → same target → same
new_stability.

## 6. One-month lag (intentional)

The monthly pipeline order from M1.9 is **unchanged**:

```
faction::react   ->  stability::tick   ->  economy::tick
```

This means in any single monthly call:

1. `faction::react` mutates faction support / loyalty.
2. `stability::tick` reads `last_gdp_growth_rate` — which is
   whatever **last month's** `economy::tick` left there.
3. `economy::tick` computes this month's growth and writes it
   into `last_gdp_growth_rate`, ready for next month's stability.

So **stability is always one month behind the economy.**

### Why not reorder to economy → stability?

Considered and rejected:

- The M1.9 canonical order has a named test that pins it as
  observable; flipping it silently would have been a regression
  trap. M1.12 should not be the PR that quietly invalidates the
  M1.9 ordering proof.
- The intuition behind the canonical order (faction sentiment →
  political stability → economic activity) is consistent with
  "economy reacts last to today's political climate". Letting
  stability read this month's growth would invert that.
- A one-month lag is realistic: the political economy literature
  generally agrees that real economic indicators take weeks to
  months to register in regime stability metrics.
- Operationally simpler: stability never has to peek into
  economy's per-tick output. Both systems remain orderable
  free functions with no implicit coupling beyond the named field.

The lag is pinned by tests:

- One-month lag test: first monthly tick sees
  `last_gdp_growth_rate = 0.0` (default), the second sees the rate
  the first tick published.
- Ordering regression test: in a scenario where canonical order
  produces a known target, the test re-derives that target with
  `last_gdp_growth_rate = 0.0` — which is only true if stability
  ran before economy.

## 7. Determinism

- The field is written deterministically by `economy::tick` (no
  RNG, pure arithmetic).
- Stability reads it deterministically.
- Same-seed + same-scenario + same-days produces byte-identical
  save + log + summary CSV.
- M0.9 byte-identical guarantee is preserved.
- One existing runner test (`run: --scenario same seed + days
  produces byte-identical save and log`) acts as the determinism
  regression for non-empty state with the coupling running.

## 8. State touched

- **EconomySystem now WRITES** `last_gdp_growth_rate` in addition
  to `gdp`, `tax_revenue`, `budget_balance`.
- **StabilitySystem READS** `last_gdp_growth_rate` (and the existing
  legitimacy / corruption / faction state). Does NOT write to it.
- **SaveSystem WRITES + READS** the field as part of the v6 country
  shape.
- **DataLoader does NOT read** the field from JSON; loaded
  countries start with `last_gdp_growth_rate = 0.0`.

## 9. Test coverage (15 new cases)

| Group | Count | What it pins |
|---|---|---|
| Save bump | 3 | v5 rejected; v6 missing field rejected; v6 round-trip preserves value |
| Economy writes | 3 | `c.last_gdp_growth_rate` matches `outcome.gdp_growth_rate`; gdp=0 still writes; invalid id leaves field unchanged |
| Stability reads | 6 | `kEconomicGrowthWeight == 2.0`; positive growth raises target; negative growth lowers target; zero growth = pre-M1.12 target; pathological growth clamped; stability doesn't write the field |
| Monthly pipeline | 2 | One-month lag: first tick sees 0, second tick sees first tick's growth; ordering regression (faction → stability → economy preserved) |
| Runner save | 1 | `--scenario`-driven save contains `"last_gdp_growth_rate"` AND round-trip via `save_system::load` recovers a non-zero value |

Plus updates to existing tests:

- All save_system tests with country JSON literals updated to
  include `"last_gdp_growth_rate"`.
- All save_version assertions changed `5 → 6`.
- The full round-trip comparator (data_loader_test) gains a
  `last_gdp_growth_rate` field check.

Total: **352 → 367** (+15).

## 10. What M1.12 deliberately does NOT do

- **No policy scheduling / enactment queue / active-policy
  container.** Reviewer's strict-scope warning honoured verbatim.
- **No starting-policies in scenario.** Manifest schema unchanged.
- **No AI / events / diplomacy / war / coup / civil war.**
- **No inflation / debt / interest / WarDamage / WelfareSatisfaction
  / InequalityProxy / BudgetCrisis** terms. Those land when their
  input systems exist.
- **No reordering of the M1.9 monthly pipeline.** Documented above;
  pinned by tests.
- **No balance pass.** `kEconomicGrowthWeight = 2.0` is a
  placeholder. A future tuning PR may revisit; M1.12 keeps it
  conservative-but-meaningful.
- **No country JSON schema change.** `data/countries/*.json`
  fixtures are unchanged. The field is runtime-only at load time.
- **No new constants on `CountryState`.** Only the one field.
- **No `last_*_rate` for other economy fields** (tax_revenue,
  budget_balance change rate). M1.12 stays focused.
- **No `Diagnostics::sanity_check` rule** for "growth rate
  diverged across two months". Possible follow-up.
- **No partial-atomicity recovery** for mid-pipeline failure
  (documented in M1.9 / M1.10 as known).

## 11. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/core/entities.hpp` | adds `last_gdp_growth_rate` field on `CountryState` |
| `include/leviathan/systems/save_system.hpp` | bumps `kSaveFormatVersion` 5 → 6; v6 entry in version history |
| `src/leviathan/systems/save_system.cpp` | serialise + deserialise the new field |
| `src/leviathan/systems/economy_system.cpp` | writes `c.last_gdp_growth_rate` at end of tick |
| `include/leviathan/systems/stability_system.hpp` | adds `kEconomicGrowthWeight`, updates formula docstring |
| `src/leviathan/systems/stability_system.cpp` | adds the term to `raw_target` |
| `tests/systems/save_system_test.cpp` | v5→v6 updates, new tests |
| `tests/systems/economy_system_test.cpp` | new writes tests |
| `tests/systems/stability_system_test.cpp` | new reads tests |
| `tests/systems/monthly_pipeline_test.cpp` | one-month-lag tests |
| `tests/systems/data_loader_test.cpp` | default-0.0 assertion |
| `tests/systems/runner_test.cpp` | save-contains-field test, v5→v6 updates |
| `docs/m1-12-economy-stability-coupling.md` | this file |
| `docs/README.md`, `README.md`, `rfc/README.md` | M1.12 status / progress |

Total tests: **352 → 367** (+15). Save format version: **v5 → v6**.

## 12. Risks / things to watch

- **`kEconomicGrowthWeight = 2.0` is a placeholder.** Not a
  balance-pass result. A future PR that tunes coefficients should
  re-derive it against multi-year scenario runs.
- **One-month lag is documented behaviour, not a workaround.**
  Future contributors who "fix" the lag by reordering the pipeline
  will fail the canonical-order test. The lag is the design.
- **Save-format bump invalidates every pre-M1.12 save.** No
  migration path; documented in the v6 version-history entry.
  Acceptable because saves are session-resume only (no replay
  guarantee), and the only live save users are the test fixtures
  in this repo.
- **`last_gdp_growth_rate` is a single-tick snapshot.** A future
  PR that wants a moving-average smoothed signal would need a new
  field or a new system; the current contract is "value of the
  most recent tick".
- **`Diagnostics::snapshot` / summary CSV does NOT yet expose the
  growth rate.** Not a regression (M0.10's CSV columns are fixed)
  but a candidate addition.

## 13. Next sub-milestone

Likely candidates (RFC-090 §M1):

- **M1.13 — policy enactment from scenario.** Extend the manifest
  with a "starting_policies" list and hook the loader into
  `policy::apply_policy_effects` so a scenario can declare day-0
  policy state.
- **M1.14 — Diagnostics surfaces last_gdp_growth_rate.** Add a
  CSV column or a snapshot field so multi-month runs are
  inspectable without round-tripping the save.

Per the M1 pacing rule: do **not** start M1.13 (or anything else)
until M1.12 is reviewed and merged.

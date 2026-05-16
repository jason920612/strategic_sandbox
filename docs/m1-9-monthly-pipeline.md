# M1.9 - Monthly pipeline (minimal)

Companion notes for the M1.9 PR. M1.9 is the **first composition
sub-milestone**: it adds no new gameplay, no new state fields, no new
public types beyond the pipeline-outcome structs. It wires the
existing M1.6 / M1.7 / M1.8 explicit-call systems into one caller.

## 1. Scope

Pure composition. Three sub-systems, one canonical order, two entry
points (single country / all countries), one PR's worth of tests.
That's it.

What this PR does NOT do is documented in §7 below — keep that
section honest, it is the contract the M1.10+ author has to honour.

## 2. API

```cpp
namespace leviathan::systems::monthly {

struct CountryMonthlyOutcome {
    core::CountryId             country;
    faction::ReactionOutcome    faction;
    stability::StabilityOutcome stability;
    economy::EconomyOutcome     economy;
};

struct MonthlyOutcome {
    int countries_processed = 0;
    std::vector<CountryMonthlyOutcome> countries;
};

core::Result<CountryMonthlyOutcome> tick_country(core::GameState& state,
                                                 core::CountryId  country);

core::Result<MonthlyOutcome> tick_all_countries(core::GameState& state);

}
```

No constants are introduced — every weight / rate lives in the
respective sub-system header.

## 3. Canonical order

```text
1.  faction::react   (state, country)
2.  stability::tick  (state, country)
3.  economy::tick    (state, country)
```

The order is **observable**:

- `stability::tick` reads `faction.support` / `faction.radicalism`
  that `faction::react` just wrote. With faction support drifting
  toward `legitimacy`, a faction with `support = 0` and
  `legitimacy = 1` becomes `support = 0.05` after react, and the
  stability target picks that up immediately.
- `economy::tick`'s political-instability drag reads
  `(1 - stability)`. If `stability::tick` just nudged stability up
  from 0.0 to 0.0425, the drag drops from `0.010` to `0.009575`.

The pipeline test
`"tick_country runs faction -> stability -> economy in canonical
order"` pins each link with exact arithmetic, so any future PR that
silently flips the order (or skips a sub-system) breaks a named test.

### Why this order, not policy → faction → economy → stability?

The M1.8 design note sketched a hypothetical `policy ->
faction -> stability -> economy` order. M1.9 deliberately drops the
policy step from the pipeline:

- Policies are caller-driven events, not month-boundary background
  updates. M1.5's `apply_policy_effects` is invoked when a policy is
  enacted; M1.9 has no policy-enactment scheduler, no active-policy
  container, no duration queue.
- Whoever owns the future enactment scheduler decides when to call
  `policy::apply_policy_effects`. M1.9 doesn't constrain that.

The final faction → stability → economy ordering also matches the
data-flow intuition: factions react to the regime → regime
stability adjusts to faction sentiment → economic activity adjusts
to political climate. Reversing any link is testable as broken.

## 4. State touched

The pipeline itself touches **nothing new**. Every mutation is by
the underlying sub-system:

- `faction::react` writes `factions[*].support` and
  `factions[*].loyalty` for factions belonging to the target country.
- `stability::tick` writes `countries[country].stability`.
- `economy::tick` writes `countries[country].gdp`,
  `tax_revenue`, `budget_balance`.

The pipeline itself **does NOT**:

- write `state.current_date` — pinned by
  `"tick_country does NOT change state.current_date"`.
- append to `state.logs` — pinned by
  `"tick_country does NOT append to state.logs"`.
- mutate `state.rng.seed` or `state.rng.counter` — pinned by
  `"tick_country does NOT advance state.rng.counter"` and the
  `tick_all_countries` invariant test.
- change save schema — `kSaveFormatVersion` stays at v5.

## 5. Error semantics

`tick_country`:

- Returns failure if any sub-system fails. The error message names
  the failing sub-system so callers can route it.
- Is **not atomic across sub-systems.** A `stability::tick` failure
  leaves faction-side mutations from the just-run `faction::react`
  in place; an `economy::tick` failure leaves faction + stability
  mutations in place. M1.9 documents this rather than papering over
  it — atomic monthly ticks are an open design question that needs
  its own PR (likely M2 once we have multiple failure modes worth
  unwinding).
- For the specific case "CountryId is invalid", `faction::react` is
  the first sub-system called and the only one with a pre-flight
  check that runs before any mutation, so an invalid id fails the
  whole pipeline with state untouched. Pinned by
  `"tick_country rejects an invalid CountryId without state
  mutation"`.

`tick_all_countries`:

- Iterates `state.countries` in vector order using each index as the
  CountryId.
- Fails fast on the first sub-failure. The partial successes
  captured so far are discarded with the failure result.
- On an empty `state.countries`, returns success with
  `countries_processed = 0` and an empty `countries` vector.

## 6. Test coverage (11 cases)

**Canonical order (1)**: exact-arithmetic proof that any reordering
of the three sub-systems produces a different result. Touches
faction loyalty (uses old stability), faction support (writes
new value), stability target (uses new support), and economy
growth rate (uses new stability).

**Country filter (1)**: two countries, two factions; verifies
ticking country 0 leaves country 1's gdp / stability /
budget_balance / tax_revenue and its faction support / loyalty /
radicalism untouched.

**`tick_all_countries` (2)**: processes 3 countries in order with
correct outcome vector; empty state returns processed = 0.

**Error paths (2)**: invalid `CountryId{99}` rejected with state
untouched; default-constructed `CountryId{}` rejected.

**Invariants (4)**: date unchanged; logs unchanged; RNG seed +
counter unchanged; the combined "date / logs / RNG untouched after
`tick_all_countries`" sweep.

**Outcome struct (1)**: every nested sub-outcome field is populated
to match the post-pipeline GameState.

## 7. What M1.9 deliberately does NOT do

- **No policy enactment** in the pipeline. No
  `policy::apply_policy_effects` call. No active-policy container.
  No duration tracking. No automatic policy selection / AI.
- **No runner integration.** M0.9's `runner.cpp` does not call
  `tick_country` or `tick_all_countries`. The caller decides when a
  month boundary occurred.
- **No `TickResult.month_changed` hookup.** Same reason: the M0.9
  runner stays untouched. Wiring it is the natural next step
  (likely M1.10).
- **No logging from the pipeline.** A future logging-on-monthly PR
  can add a `LogEntry` per `tick_country` call — without M1.9's
  layout changing.
- **No RNG.** Nothing in the pipeline reads or advances
  `state.rng.counter`. The three sub-systems also avoid RNG today.
- **No save-schema change.** `kSaveFormatVersion` stays at v5. No
  new persistent state.
- **No new `CountryState` / `FactionState` / `PolicyData` fields.**
  Specifically, no `last_gdp_growth_rate` field — that's the
  follow-up the M1.8 design note flagged and it stays a follow-up.
- **No diplomacy / war / events / coup / civil war.**
- **No UI / SVG / map.**
- **No constant rebalancing or formula tuning.**
- **No `Diagnostics::sanity_check` rule** for "country processed
  but no faction state changed" or similar.
- **No partial-atomicity rollback.** Documented in §5.

## 8. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/systems/monthly_pipeline.hpp` | **new** — public API |
| `src/leviathan/systems/monthly_pipeline.cpp` | **new** — implementation |
| `src/leviathan/systems/CMakeLists.txt` | adds `monthly_pipeline.cpp` |
| `tests/systems/monthly_pipeline_test.cpp` | **new** — 11 cases |
| `tests/CMakeLists.txt` | adds the test source |
| `docs/m1-9-monthly-pipeline.md` | this file |
| `docs/README.md`, `README.md`, `rfc/README.md` | M1.9 status / progress |

Total tests: **307 → 318** (+11).

No save format change. No public-header change to existing systems.

## 9. Risks / things to watch

- **Order is fixed without an empirical balance pass.** The chain
  faction → stability → economy was chosen on data-flow grounds.
  Once the monthly pipeline runs over real fixtures for many ticks
  (M1.10+), the order may need to change. Because the order is
  pinned by named tests rather than an enum or strategy parameter,
  changing it requires updating both the implementation and the
  pinning test in the same PR — the test failure is the safety net.
- **Mid-pipeline failure leaves partial state.** Documented in §5.
  Worth revisiting only when at least two sub-systems gain
  failure modes that aren't pure id-validation.
- **`tick_all_countries` failure discards captured outcomes.** That
  is the simple-correct behaviour for now. If a future caller
  (diagnostics, replay tooling) wants to see "country 0 succeeded,
  country 1 failed", a richer return type is the right answer; the
  current `Result<MonthlyOutcome>` is intentionally coarse.
- **No protection against duplicate-id state.** If
  `state.countries` somehow contains two entries with the same
  `id_code`, the pipeline still ticks each by index. That's not a
  pipeline concern; DataLoader / Diagnostics already cover
  duplicate-id detection.

## 10. Next sub-milestone

Likely candidates (RFC-090 §M1):

- **M1.10** — Wire `monthly::tick_all_countries` to
  `TickResult.month_changed` from the M0.9 runner. The pipeline
  invocation becomes part of the headless `leviathan --days N`
  flow. Determinism property (M0.9) needs to be re-verified.
- **M1.11** — Decide where `last_gdp_growth_rate` lives (probably a
  new `CountryState` field; would trigger save-format `v5 → v6`)
  and wire `stability::tick` to use it as the RFC-080 §5
  EconomicGrowth input.

Per the M1 pacing rule: do **not** start the next sub-milestone
until M1.9 is reviewed and merged.

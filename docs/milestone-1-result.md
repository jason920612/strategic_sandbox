# Milestone 1 — exit report

Status: **complete** (M1.1 → M1.17 all merged).

This document is the M1 deliverables ledger. It records what M1 ships,
what is deliberately deferred to M2 and beyond, and what the next
milestone should tackle first.

M1 closes the **single-country internal-politics prototype**: a
headless simulation that loads a 1930 world manifest, runs a
deterministic monthly pipeline of `faction::react → stability::tick
→ economy::tick`, applies day-0 starting policies, tracks each
policy's expiration date, and surfaces per-country / per-faction
diagnostics via three opt-in CSV streams. The save format
round-trips the full state shape (v7).

## 1. What ships in M1

### 1.1 State shape (`leviathan::core::entities`)

| Sub-milestone | What landed |
|---|---|
| M1.1 | `CountryState` runtime fields: gdp, tax_revenue, budget_balance, legal_tax_burden, fiscal_capacity, administrative_efficiency, central_control, corruption, stability, legitimacy, military_power, threat_perception. Save format v1 → v2. |
| M1.2 | `FactionState` runtime fields (id_code, country_id_code, type, support, influence, radicalism, loyalty, resources, preferred_policies). Save format v2 → v3. |
| M1.3 | `BudgetState` (7-category ratio struct: administration, military, education, welfare, intelligence, infrastructure, industry) nested in `CountryState`. **No sum-to-1 enforcement.** Save format v3 → v4. |
| M1.4 | `PolicyData` + `PolicyEffect{target, op, value}`. Ten policy fixtures spanning RFC-010 §2.6 categories. Save format v4 → v5. Shared `internal/json_helpers.hpp` extracted. |
| M1.12 | `CountryState::last_gdp_growth_rate` added; written by `economy::tick`, read by `stability::tick` as the RFC-080 §5 `EconomicGrowth` term (`kEconomicGrowthWeight = 2.0`). Save format v5 → v6. |
| M1.15 | `ActivePolicy{policy_id_code, expires_on}` + `CountryState::active_policies` vector (append-only). Save format v6 → v7. |

### 1.2 Systems — first real gameplay effects

| Sub-milestone | Namespace | Entry points |
|---|---|---|
| M1.5 | `systems::policy` | `apply_policy_effects(state, actor, policy)`. Targets `country.<field>`, `country.budget.<category>`, `faction:<type>.<field>`. Ops `add` / `set`. Pre-flight rejection → state unchanged. Ratio fields clamped to `[0, 1]` post-op. M1.15 added `duration_days` bound check (negatives + `kMaxTrackedPolicyDurationDays = 36500`). |
| M1.6 | `systems::faction` | `react(state, country)`. Two linear-toward-equilibrium rules: loyalty drifts toward stability at 0.10, support drifts toward legitimacy at 0.05. Clamped. |
| M1.7 | `systems::stability` | `tick(state, country)`. Target = `0.5·avg_support + 0.5·legitimacy − 0.3·corruption − 0.2·avg_radicalism + 2.0·last_gdp_growth_rate` (the EconomicGrowth term landed in M1.12); stability drifts toward target at 0.10. |
| M1.8 | `systems::economy` | `tick(state, country)`. Tax revenue (RFC-080 §3), expenditure = `gdp × Σbudget × 0.20`, GDP growth (stripped-down RFC-080 §4 — political instability + corruption penalties, education / infra / industry / admin bonuses). Writes `last_gdp_growth_rate`. |
| M1.9 | `systems::monthly` | `tick_country` and `tick_all_countries`. Canonical order `faction::react → stability::tick → economy::tick` is **observable** (exact-arithmetic ordering test). Mid-pipeline failure is documented as non-atomic. |

### 1.3 Orchestration

| Sub-milestone | What landed |
|---|---|
| M1.10 | Runner monthly wiring. `runner::run` invokes `monthly::tick_all_countries` on every `TickResult.month_changed` boundary after the canonical "month rolled over" log. New `RunOutcome.monthly_ticks` counter. New public `run_state(state, opts)` entry point. |
| M1.11 | Scenario loader. New `systems::scenario_loader::load_into_state(state, manifest_path)`. Manifest schema: `{ "scenario": { "countries":[], "factions":[], "policies":[] } }`. New `--scenario PATH` CLI flag. Canonical `data/scenarios/1930_minimal.json` (3 countries + 3 GER factions + 10 policies). |
| M1.13 | Scenario starting policies. Manifest gains optional `starting_policies` array of `{policy, actor}` id_code pairs; loader calls `apply_policy_effects` exactly once per entry at day 0. New fixture `data/scenarios/1930_with_start_policies.json`. |

### 1.4 Diagnostics surfaces

| Sub-milestone | What landed |
|---|---|
| M1.14 | Per-country CSV. New `CountrySummaryRow` + `country_snapshot(state, country)` + `write_country_csv_*`. New opt-in `--countries-csv PATH` flag, 8 columns per country per snapshot point (`date,id_code,gdp,tax_revenue,budget_balance,stability,legitimacy,last_gdp_growth_rate`). Doubles `std::scientific` + `setprecision(17)`. |
| M1.16 | Per-faction CSV. New `FactionSummaryRow` + `faction_snapshot(state, faction)` + `write_faction_csv_*`. New opt-in `--factions-csv PATH` flag, 9 columns per faction per snapshot point. Drive-by: `main()` now prints CSV row counts. |

### 1.5 Data fixtures

| File | Source |
|---|---|
| `data/countries/{germany,france,japan}.json` | Updated through M1.1 (numeric fields) and M1.3 (budget block) |
| `data/factions/ger_{military,workers,bureaucracy}.json` | M1.2 |
| `data/policies/{raise_taxes, lower_taxes, expand_welfare, increase_education, increase_military_budget, cut_military_budget, administrative_reform, intelligence_expansion, press_censorship, press_freedom}.json` | M1.4 — ten fixtures spanning RFC-010 §2.6 categories |
| `data/scenarios/1930_minimal.json` | M1.11 |
| `data/scenarios/1930_with_start_policies.json` | M1.13 — enacts `raise_taxes` + `increase_military_budget` on GER at day 0 |

### 1.6 Determinism property (extended through M1)

Two runs with the same `RunnerOptions` and the same config files
produce **byte-identical** outputs across **five** artefacts:

- `out/save.json` (M0.8, schema bumped each time state shape grew)
- `out/events.jsonl` (M0.6)
- `out/summary.csv` (M0.10, when `--summary-csv` is set)
- `out/countries.csv` (M1.14, when `--countries-csv` is set)
- `out/factions.csv` (M1.16, when `--factions-csv` is set)

Pinned by:
- The M0.9 / M0.10 byte-identical tests in `runner_test.cpp`.
- M1.14 / M1.16 cross-contract tests (`--countries-csv does NOT
  change --summary-csv`, `--factions-csv does NOT change either`).
- **The new `M1 end-to-end: same seed produces byte-identical
  save / log / all three CSVs` integration test** that exercises
  all five artefacts together over a 365-day scenario run.

### 1.7 Test counts at M1 exit

```
> ctest --test-dir build -C Debug --output-on-failure
100% tests passed, 0 tests failed out of 435
```

| Suite | Tests |
|---|---|
| M0 carry-over (core + systems::time/random/logging/data_loader/save_system/runner/diagnostics + M0 integration) | 179 |
| systems::policy (M1.4–M1.5, M1.15) | ~50 |
| systems::faction (M1.2, M1.6) | ~30 |
| systems::stability (M1.7, M1.12) | ~25 |
| systems::economy (M1.8) | 19 |
| systems::monthly_pipeline (M1.9) | 11 |
| systems::scenario_loader (M1.11, M1.13) | 25 |
| systems::diagnostics extras (M1.14, M1.16) | 18 |
| systems::runner extras (M1.10, M1.11, M1.14, M1.16) | ~30 |
| systems::save_system extras (M1.1–M1.4, M1.12, M1.15) | ~30 |
| **integration::m1_end_to_end (this PR)** | **3** (1-year + 10-year soak + 5-artefact determinism) |

The 10-year soak run exercises 120 monthly pipeline calls and
3652 day advances, completes in under 100 ms, and ends with zero
sanity issues. RFC-090 §1.17 explicitly listed "跑 10 年單國測試"
as the M1 acceptance criterion; this PR pins it.

## 2. What's deliberately deferred to M2 and beyond

These appear in the RFCs but were intentionally **NOT** shipped in M1.

### 2.1 Policy lifecycle

- **No expiration sweep.** `CountryState::active_policies` grows
  monotonically through a run. Nothing removes entries whose
  `expires_on` has passed (M1.15 explicit non-goal).
- **No effect revert.** When `expires_on` arrives, the numeric
  mutations applied at enactment time stay applied.
- **No dedup.** Re-enacting the same `policy_id_code` records a
  second entry.
- **No scheduler / AI / event-triggered enactment.** The only
  callers of `apply_policy_effects` are direct test usage and the
  M1.13 scenario day-0 path.

### 2.2 RFC-080 formula terms not yet wired

`stability::tick` (M1.7 / M1.12) only consumes
`avg_support / legitimacy / corruption / avg_radicalism /
last_gdp_growth_rate`. The remaining RFC-080 §5 terms are
deferred until their input systems ship:

- `WelfareSatisfaction` — needs welfare-policy / inflation modelling.
- `InequalityProxy` — needs class / income model.
- `WarWeariness` — needs M8 (diplomacy & war).
- `BudgetCrisis` — needs explicit fiscal-stress modelling.

`economy::tick` (M1.8) similarly skips RFC-080 §4
`InflationPressure` and `WarDamage`.

`kEconomicGrowthWeight = 2.0` (M1.12) is a placeholder; M1 closes
with no balance pass. M1 deliberately ships data flow first,
numerics-tuning second.

### 2.3 Multi-country / international layer

M1 simulates each country in isolation. There is no:

- relationship / threat / alliance state (RFC-040)
- trade or capital flow between countries
- diplomatic events (RFC-080 §10)
- war / war damage / war weariness (RFC-090 §M8–M9)

The state container has space (`provinces`, `events` are reserved
empty arrays in the save), but no behaviour reads or writes
across countries.

### 2.4 Faction `react` extension

M1.6's two linear-toward-equilibrium rules are type-agnostic.
Faction-type-specific reactions (military reacts differently from
workers from bureaucracy from intelligence) are not implemented.

### 2.5 Information distortion

Same as M0: RFC-050 §1 and RFC-080 §8 (`ReportedValue = TrueValue
+ Bias + Noise`) remain future work. M1 logs and CSVs are
ground-truth.

### 2.6 UI / map / interactive controls

M1 remains headless. No SVG map, no HTML viewer, no player
commands. M2 (per RFC-090) introduces the player-interaction
loop.

### 2.7 Replay (vs session resume)

Same as M0: save format round-trips a paused game but does not
ship deterministic replay from `start_date`. The
`rng_algorithm_version` field is in place; the per-tick decision
log is not.

### 2.8 CSV quoting

`id_code`, `country_id_code`, and `type` strings are emitted
verbatim into CSV rows. All current data uses comma-free,
newline-free identifiers. If any future fixture introduces an
exotic id_code, CSV escaping (or a character-set constraint on
id_codes) must land before that fixture merges.

## 3. Recommendations for M2

Per RFC-090 §M2 the next milestone is the **player-operation
prototype**: a single human picks a country and starts issuing
commands (pause / resume, speed control, budget adjustments,
policy enactment). The runtime systems M1 ships are exactly what
M2 needs to *react to* commands.

Suggested first steps:

1. **M2.1 — Player country selection.** A `--player COUNTRY_IDCODE`
   flag (or interactive prompt) that pins one country as the
   player. Save format-neutral; just a `GameState::player_country`
   field with `CountryId::invalid()` default.
2. **M2.2 — Pause / resume.** The runner currently advances days
   in a tight loop; M2 introduces a tick-rate controller that
   can stop / resume / step single days.
3. **M2.3 — Player command queue.** A first-class command struct
   submitted by an outer driver (CLI for now, eventually UI).
   Initial commands: enact a policy, change budget allocation.
4. **M2.4 — Player command log.** Mirrors RFC-050 §8 "玩家命令需
   記錄". Each command appends a `LogEntry` and (later) feeds the
   replay log mentioned in §2.7 above.

**Do not** during M2:

- Add a second country's behaviour (M3 territory).
- Add events (M5+).
- Add a map / UI (M4+) — keep M2 headless; CLI commands or a
  scripted input file are sufficient.
- Touch save schema unless the player_country field forces it
  (it probably does → v7 → v8).

### 3.1 Architectural rules preserved from M0 and M1

These rules every subsequent milestone must respect (M0 set most of
them; M1 added the last three):

- **`GameState` has no methods.** Behaviour lives in
  `systems::` free functions.
- **Systems take `GameState&` (or `const&`) and return
  `Result<T>`.** Dependency direction strictly `systems → core`.
  (M1.15 noted: avoid inverting this when adding cross-system
  validators — DataLoader does not depend on PolicySystem.)
- **Logging is always explicit.** `systems::logging::log()` is the
  only function that pushes to `state.logs`.
- **Diagnostics is observation-only.** `const GameState&`.
- **No `<random>` or `<random_device>`.** Use `systems::random::*`.
- **No `<filesystem>` paths in deterministic logs / CSVs.**
- **`save_version` / `rng_algorithm_version` are load-bearing.**
  Strict-equality gate; bump on any incompatible schema change.
- **doctest test names must not contain `[brackets]`.**
- **Effects are atomic per-call.** `policy::apply_policy_effects`
  uses pre-flight resolution + clamping; mid-call failure leaves
  state unchanged. M1.15 extended this to cover the new
  `active_policies` side effect.
- **Save format bumps are strict-equality.** v6 saves are
  rejected loudly by v7 binaries, even if the byte layout would
  almost round-trip; we do not silent-pad missing fields.
- **CSV outputs are append-additive only.** New columns go on the
  right; existing column order is byte-pinned by tests. Bumping
  an existing column's position is breaking.

### 3.2 Known small-scope tech debt

- `runner.cpp::parse_positive_int` still hard-codes `2147483647`
  instead of `std::numeric_limits<int>::max()`. Quiet stand-alone
  PR territory. Re-flagged in the M1.10 review.
- `main.cpp` still includes a `<random>`-free runner welcome line
  but its milestone label `"Milestone 0.10 — headless runner with
  diagnostics."` was rewritten by M1.17 to be milestone-neutral.
  Future milestones do not need to maintain a label here.
- `Result<bool>` continues to serve as the "no payload" carrier.
  A `Result<void>` specialisation would be cleaner; not blocking.
- The M1.13 day-0 enactment loop is **not** atomic across multiple
  starting policies. Mid-list failure leaves partial state.
  Documented in `m1-13-scenario-starting-policies.md`; revisit
  if scenario authors start hitting partial-load corruption.
- `active_policies` has no expiration sweep yet (M1.15 explicit
  non-goal). The natural place is a new `systems::policy::expire`
  step in the monthly pipeline; deferred so M1 ships clean.

## 4. How to verify M1 right now

```bash
# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug

# Run only the M1 exit-gate integration tests
ctest --test-dir build -C Debug -R "M1 end-to-end"

# Full suite (435 cases)
ctest --test-dir build -C Debug --output-on-failure

# Canonical 1-year run with every diagnostic CSV
./build/bin/Debug/leviathan \
    --config        data/config/simulation.json \
    --days          365 \
    --seed          12345 \
    --scenario      data/scenarios/1930_with_start_policies.json \
    --output        out/ \
    --summary-csv   out/summary.csv \
    --countries-csv out/countries.csv \
    --factions-csv  out/factions.csv

# 10-year soak (matches RFC-090 §1.17 acceptance criterion)
./build/bin/Debug/leviathan \
    --days     3652 \
    --seed     12345 \
    --scenario data/scenarios/1930_with_start_policies.json
```

## 5. Where the design notes live

| Topic | Note |
|---|---|
| CountryState fields | `docs/m1-1-country-state.md` |
| FactionState fields | `docs/m1-2-faction-state.md` |
| BudgetState | `docs/m1-3-budget.md` |
| PolicyData + PolicyEffect | `docs/m1-4-policy-data.md` |
| PolicySystem `apply_policy_effects` | `docs/m1-5-policy-system.md` |
| FactionSystem `react` | `docs/m1-6-faction-reactions.md` |
| StabilitySystem `tick` | `docs/m1-7-stability-tick.md` |
| EconomySystem `tick` | `docs/m1-8-economy-tick.md` |
| MonthlyPipeline | `docs/m1-9-monthly-pipeline.md` |
| Runner monthly wiring | `docs/m1-10-runner-monthly-wiring.md` |
| Scenario loader | `docs/m1-11-scenario-loader.md` |
| Economy → stability coupling | `docs/m1-12-economy-stability-coupling.md` |
| Scenario starting policies | `docs/m1-13-scenario-starting-policies.md` |
| Per-country diagnostics CSV | `docs/m1-14-diagnostics-surfaces-growth.md` |
| Policy duration tracking | `docs/m1-15-policy-duration-tracking.md` |
| Per-faction diagnostics CSV | `docs/m1-16-faction-csv.md` |
| **M1 exit report (this file)** | `docs/milestone-1-result.md` |

`docs/README.md` is the canonical index.

M1 closes here. The repo is ready for Milestone 2.

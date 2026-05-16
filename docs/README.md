# docs/

Per-milestone design notes and PR description drafts.

## What lives here

- **`m0-N-*.md`** — design notes that get written **alongside** each
  sub-milestone PR. They capture decisions that the PR itself touched
  only briefly: type-design rationale, error-message format, edge-case
  policies, deliberate non-goals. Treat them as the canonical answer
  to "why was it done this way?" once a PR is merged.
- **`pr-drafts/`** — local working copies of PR descriptions. The
  canonical copy lives on GitHub (each `gh pr edit` updates it). The
  drafts folder is git-ignored on purpose; it's a scratch space for
  composing the next PR body, not a deliverable.

## Index of design notes

| File | Sub-milestone | Covers |
|------|---------------|--------|
| [`m0-2-calendar-and-types.md`](m0-2-calendar-and-types.md) | M0.2 | Proleptic Gregorian decision, `StrongId<Tag>` shape, `Result<T, E>` semantics, doctest framework choice, the no-`[brackets]`-in-test-names gotcha. |
| [`m0-3-game-state.md`](m0-3-game-state.md) | M0.3 | The "GameState is dumb data, systems are free functions" rule. Why `make_game_state` is a free function (not a constructor). Entity-stub policy. Why `rng.counter` resets on `make_game_state`. |
| [`m0-4-time-system.md`](m0-4-time-system.md) | M0.4 | Free-function-systems pattern. `systems → core` dependency direction. `TickResult` + loop-on-`advance_one_day` as the "minimal pipeline". TimeSystem non-interference contract. |
| [`m0-5-rng-service.md`](m0-5-rng-service.md) | M0.5 | Why we avoid `<random>` and `<random_device>`. Counter-indexed splitmix64 algorithm. Edge-case rules for `weighted_choice`. Tag / trace-hook design. |
| [`m0-6-logging.md`](m0-6-logging.md) | M0.6 | Full `LogEntry` shape. The "logging is always explicit" rule. Byte-stable JSONL format + escape rules. Why no JSON library for emit. Insertion-order-preserving metadata. |
| [`m0-7-data-loader.md`](m0-7-data-loader.md) | M0.7 | nlohmann/json v3.11.3 pinning. Simulation + country JSON schemas. Error-message format. The "no GameState mutation, no time/RNG side effects, loggable but not coupled" architectural rules. |
| [`m0-8-save-load.md`](m0-8-save-load.md) | M0.8 | Full save-file schema. `save_version` / `rng_algorithm_version` strict-equality gates. The "session resume, NOT deterministic replay" distinction (and why `rng.counter` is preserved anyway). |
| [`m0-9-runner.md`](m0-9-runner.md) | M0.9 | Headless CLI: flag table, defaults, the determinism property (same seed → byte-identical save + log), the two-layer `parse_args` / `run` split. |
| [`m0-10-diagnostics.md`](m0-10-diagnostics.md) | M0.10 | `systems::diagnostics`: observation-only `snapshot`, byte-stable CSV format, `sanity_check` (invalid date, invalid / duplicate CountryId). The `--summary-csv` flag and how snapshot cadence interacts with month-boundary logs. |
| [`milestone-0-result.md`](milestone-0-result.md) | **M0 exit report** | What M0 ships (every sub-milestone), what is deliberately deferred (entity behaviour, events, AI, replay), recommendations for M1, and the architectural invariants every M1+ milestone must preserve. |
| [`m1-1-country-state.md`](m1-1-country-state.md) | M1.1 | `CountryState` runtime numeric fields (gdp, tax_revenue, budget_balance, legal_tax_burden, fiscal_capacity, administrative_efficiency, central_control, corruption, stability, legitimacy, military_power, threat_perception). JSON schema, range validation, save-format v2 bump, old-v1-rejection behaviour. **No simulation logic** at this stage. |
| [`m1-2-faction-state.md`](m1-2-faction-state.md) | M1.2 | `FactionState` runtime fields (id_code, country_id_code, type, support, influence, radicalism, loyalty, resources, preferred_policies). JSON schema, range validation, save-format **v3** bump (with reasoning for not relying on the M0.8 "reserved-empty-array" forward-compat note), integration test loads three factions and verifies round-trip. **No reaction logic** at this stage. |
| [`m1-3-budget.md`](m1-3-budget.md) | M1.3 | `BudgetState` (seven-category ratio struct: administration, military, education, welfare, intelligence, infrastructure, industry) embedded in `CountryState`. JSON schema with nested `budget` object, range validation per category but **no sum-to-1 enforcement**, save-format **v4** bump. Drive-by `CountryId::underlying_type` fix in `faction_from_json`. **No economy tick** at this stage. |
| [`m1-4-policy-data.md`](m1-4-policy-data.md) | M1.4 | `PolicyData` + `PolicyEffect{target, op, value}`. Ten policy fixtures spanning RFC-010 §2.6 categories. Save-format **v5** bump (v4 saves rejected). **Shared JSON helpers** finally extracted into `src/leviathan/systems/internal/json_helpers.hpp` under namespace `leviathan::systems::detail`; both DataLoader and SaveSystem now go through it. **No effect application** at this stage — M1.5 interprets target / op. |
| [`m1-5-policy-system.md`](m1-5-policy-system.md) | M1.5 | **First real gameplay effect.** `leviathan::systems::policy::apply_policy_effects(state, actor, policy)`. Resolves `country.<field>` / `country.budget.<cat>` / `faction:<type>.<field>` targets; ops `add` / `set`; ratio fields clamped to `[0, 1]` post-op. **Atomic via pre-flight**: any unresolvable effect fails the whole call and leaves state untouched. No duration queue, no AI, no runner integration yet. |
| [`m1-6-faction-reactions.md`](m1-6-faction-reactions.md) | M1.6 | **First faction-side dynamics.** `leviathan::systems::faction::react(state, country)`: two linear-toward-equilibrium rules — `loyalty` drifts toward `country.stability` at rate 0.10, `support` drifts toward `country.legitimacy` at rate 0.05. Clamped to `[0, 1]`. `influence` / `radicalism` / `resources` / identity fields untouched. Drive-by: pre-flight `std::isfinite` check on `PolicyEffect.value`. No tick, no AI, no type-specific reactions yet. |
| [`m1-7-stability-tick.md`](m1-7-stability-tick.md) | M1.7 | **First country-side dynamic.** `leviathan::systems::stability::tick(state, country)`: stripped-down RFC-080 §5 formula. Target = `0.5*avg_support + 0.5*legitimacy − 0.3*corruption − 0.2*avg_radicalism`, clamped `[0,1]`. `stability` drifts toward target at rate 0.10. No factions → 0.5 / 0.5 defaults. Reads faction state but never writes it. Skipped RFC-080 §5 inputs (welfare / economic growth / inequality / war / budget crisis) wait for M1.8+ systems to land. |
| [`m1-8-economy-tick.md`](m1-8-economy-tick.md) | M1.8 | **Second country-side dynamic.** `leviathan::systems::economy::tick(state, country)`: three formulas. Tax revenue = `gdp × legal_tax_burden × fiscal_capacity × central_control × (1 - corruption)` (RFC-080 §3 verbatim). Expenditure = `gdp × sum_budget × 0.20`. `budget_balance += (revenue - expenditure)`. GDP growth = `kBase + kEdu*budget.education + kInfra*budget.infrastructure + kInd*budget.industry + kAdmin*admin_efficiency − kPolitical*(1−stability) − kCorruption*corruption`; `gdp *= 1 + growth`. Skipped RFC-080 §4 terms (InflationPressure, WarDamage) await their input systems. |
| [`m1-9-monthly-pipeline.md`](m1-9-monthly-pipeline.md) | M1.9 | **First composition sub-milestone.** `leviathan::systems::monthly::tick_country(state, country)` runs `faction::react` → `stability::tick` → `economy::tick` in canonical order. `tick_all_countries(state)` iterates `state.countries` in vector order with fail-fast semantics; empty state succeeds with `processed=0`. `CountryMonthlyOutcome` aggregates the three sub-outcomes. The canonical order is **observable** — pinned by an exact-arithmetic test where any reordering produces a different result. No new state fields; no save schema change; no RNG / log / date side effects; no runner wiring (M1.10's job); no policy step (caller-driven). Mid-pipeline failure is documented as non-atomic. |
| [`m1-10-runner-monthly-wiring.md`](m1-10-runner-monthly-wiring.md) | M1.10 | **Runner monthly pipeline wiring.** `runner::run` now invokes `monthly::tick_all_countries(state)` on every `TickResult.month_changed` boundary, after the canonical "month rolled over" log line. `RunOutcome` gains `int monthly_ticks` counting boundaries crossed. A new `run_state(state, opts)` public entry point operates on a pre-built `GameState` so tests can inject countries / factions before the loop. No save-format bump (still v5); no new log lines; no new CSV column; no country-file loading (deferred to a scenario-loader sub-milestone). Determinism property survives for both empty and non-empty state — pinned by same-seed byte-identical save / log tests. |
| [`m1-11-scenario-loader.md`](m1-11-scenario-loader.md) | M1.11 | **Scenario loader for runner.** `leviathan::systems::scenario_loader::load_into_state(state, manifest_path)` composes the M0.7 / M1.1 / M1.2 / M1.4 parsers into a manifest-driven loader. Manifest schema: `{ "scenario": { "countries":[…], "factions":[…], "policies":[…] } }` — paths are relative to `manifest_path.parent_path().parent_path()`. IDs assigned by vector index; duplicate `id_code` or missing faction → country reference is rejected. New `--scenario PATH` CLI flag; without it the runner ticks an empty world (M1.10 contract preserved). Canonical `data/scenarios/1930_minimal.json` loads 3 countries + 3 factions + 10 policies. **No save-format bump (still v5)**, no policy enactment, no new state shape, no RNG / log / date side effects, no atomic-load rollback. |
| [`m1-12-economy-stability-coupling.md`](m1-12-economy-stability-coupling.md) | M1.12 | **Economy → stability coupling.** `CountryState` gains `last_gdp_growth_rate` (default 0.0; runtime-only at load time). `economy::tick` writes it on every successful tick. `stability::tick` reads it as the RFC-080 §5 `EconomicGrowth` term with `kEconomicGrowthWeight = 2.0`. Monthly pipeline order is unchanged (faction → stability → economy) — produces an intentional one-month lag where stability sees last month's growth. **Save format bumped v5 → v6** (first M1 save-schema bump); v5 saves and v6 saves missing the field both rejected loudly. No policy scheduling, no AI / events / war, no balance pass — `kEconomicGrowthWeight` is a placeholder. |
| [`m1-13-scenario-starting-policies.md`](m1-13-scenario-starting-policies.md) | M1.13 | **Scenario starting policies.** `ScenarioManifest` gains an optional `starting_policies` array of `{policy, actor}` id_code pairs. After loading countries / factions / policies, the loader resolves each entry and invokes `policy::apply_policy_effects(state, actor, policy)` exactly once. Manifests without the key still load (M1.11 back-compat). Entries apply in manifest order; multiple entries that touch the same field accumulate. Unknown policy / unknown actor rejected with the id_code in the message. New fixture `data/scenarios/1930_with_start_policies.json` enacts `raise_taxes` + `increase_military_budget` on GER. **No save-format bump (stays at v6)**, no duration queue, no scheduler, no AI, no new state shape, no monthly involvement. Mid-list apply failure leaves partial state (documented non-atomic). |
| [`m1-14-diagnostics-surfaces-growth.md`](m1-14-diagnostics-surfaces-growth.md) | M1.14 | **Diagnostics surfaces `last_gdp_growth_rate`.** New `Diagnostics::CountrySummaryRow` + `country_snapshot(state, country)` + `write_country_csv_header` / `write_country_csv_row` free functions. New opt-in `--countries-csv PATH` runner flag emits one row per country per snapshot point (cadence mirrors `--summary-csv`: start + each `month_changed` + final post-sanity). 8 columns: `date,id_code,gdp,tax_revenue,budget_balance,stability,legitimacy,last_gdp_growth_rate`. Doubles formatted with `std::scientific` + `setprecision(17)` for deterministic round-trip. Existing `--summary-csv` 4-column format byte-for-byte unchanged (M0.10 contract preserved). **No save-format bump (still v6)**, no new state shape, no faction-level CSV, no JSON variant. |

## Reading order

If you're new to the codebase:

1. Start with the top-level `README.md` for current status, build,
   and test instructions.
2. Skim `rfc/README.md` and the RFC documents it indexes for the
   high-level design intent.
3. Read the milestone notes here **in order** (M0.2 → M0.10 → M1.1
   → M1.2 → M1.3 → M1.4 → M1.5 → M1.6 → M1.7 → M1.8 → M1.9 → M1.10
   → M1.11 → M1.12 → M1.13 → M1.14). They build on each other and
   each one tries to call out the rules a future contributor must
   not silently break.

## What's next

Two candidates after M1.14 merges:

- **M1.15 — policy duration tracking.** Introduce a per-country
  active-policy list (`policy_id_code + expires_on`) so M1.4's
  `duration_days` field finally has runtime meaning. **This would
  bump save format `v6 → v7`** because the active-policy container
  is new persistent state.
- **M1.16 — faction-level CSV.** Mirror the M1.14 per-country
  diagnostic pattern for factions: new `FactionSummaryRow` +
  `faction_snapshot` + `--factions-csv PATH` flag. No save-format
  change.

Per the M1 pacing rule, the next sub-milestone is **not** started
until M1.14 is merged.

## When to add a new file

A new design note belongs here whenever a PR lands that does any of:

- Introduces a new core type or system in `leviathan::` and pins a
  rule about its shape or behaviour.
- Locks in an on-disk or wire-format invariant.
- Makes an architectural decision that later code will be tempted to
  unwind (e.g. "GameState has no methods", "systems never write to
  `state.logs` as a side effect").

Conversely, do **not** create a design note for things `git log`
already covers: refactors, bug fixes, dependency bumps, doc-only
changes. Use the PR description for those.

## Conventions

- File name: `m0-N-<short-slug>.md`. Section 1 always opens with what
  rule(s) this PR locks in. Section "What's NOT in scope" is
  encouraged — explicit non-goals age much better than implicit ones.
- Cross-link with relative paths so `mkdocs` / IDE preview Just Work.
- Keep examples small and copy-pastable.
- When a later milestone changes a rule that an earlier note set,
  update the older note in the same PR rather than letting the two
  drift apart. Don't write "see also M0.X for the new rule"; rewrite
  the original in place.

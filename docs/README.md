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
| [`m1-15-policy-duration-tracking.md`](m1-15-policy-duration-tracking.md) | M1.15 | **Policy duration tracking.** New `ActivePolicy{policy_id_code, expires_on}` core type and `CountryState::active_policies` vector (default empty, append-only). Every successful `policy::apply_policy_effects` records one entry with `expires_on = current_date + duration_days`. Atomicity covers the new side effect: pre-flight failure appends nothing. `duration_days == 0` is allowed and records a same-day-expiry entry. Runtime cap `kMaxTrackedPolicyDurationDays = 36500` (~100 years) is enforced inside `apply_policy_effects`; negative or over-cap `duration_days` is rejected before any mutation (PolicySystem is the last line of defense, since `GameDate::advance_days` is a per-day loop and `data_loader -> policy_system` would invert layering). **Save format bumped v6 → v7** (v6 saves rejected loudly; v7 country missing `active_policies` rejected; entry parse errors point at `active_policies[N]`). Tracking-only — no expiration sweep, no effect revert, no scheduler, no AI, no dedup, no JSON-config / DataLoader change. |
| [`m1-16-faction-csv.md`](m1-16-faction-csv.md) | M1.16 | **Faction-level diagnostics CSV.** New `Diagnostics::FactionSummaryRow` + `faction_snapshot(state, faction)` + `write_faction_csv_header` / `write_faction_csv_row` free functions. New opt-in `--factions-csv PATH` runner flag emits 9 columns per faction per snapshot point (cadence mirrors `--summary-csv` and `--countries-csv`: start + each `month_changed` + final post-sanity). Columns: `date,id_code,country_id_code,type,support,influence,radicalism,loyalty,resources`. Doubles formatted with `std::scientific` + `setprecision(17)`. Existing summary CSV (M0.10) and per-country CSV (M1.14) both byte-for-byte unchanged. **No save-format bump (still v7).** Drive-by: `main()` now also prints per-country and per-faction CSV row counts (M1.14 omitted the country print line). |
| [`milestone-1-result.md`](milestone-1-result.md) | **M1 exit report** | What M1 ships (every sub-milestone M1.1–M1.16 + the M1.17 integration tests), the five-artefact determinism contract, deferred items (expiration sweep, effect revert, full scheduler, AI / events / war / diplomacy, balance pass, faction `react` extension, CSV quoting, multi-country / international layer, replay vs session resume), recommendations for M2 (player-operation prototype per RFC-090 §M2), the architectural invariants every M2+ milestone must preserve, and known small-scope tech debt. **M1 closes here.** |
| [`m2-1-player-country.md`](m2-1-player-country.md) | M2.1 | **Player country selection (Milestone 2 kickoff).** New `GameState::player_country` (`CountryId`, default `invalid()`). New `--player COUNTRY_IDCODE` runner flag; resolved in `run_state` after scenario load by linear scan against `state.countries[i].id_code`. Failure cases: empty world, unknown id_code — both rejected before any tick / log / snapshot is emitted, with the offending id_code in the error message. **Save format bumped v7 → v8.** `"player_country"` is a required root-level integer (-1 = headless; non-negative must index into `countries`); v7 saves rejected loudly; non-integer / `< -1` / out-of-range / above `INT_MAX` all rejected with specific messages. **No system reads the field yet.** None of M1's systems (faction::react / stability::tick / economy::tick / monthly pipeline / diagnostics) branch on `player_country`; M1's 5-artefact byte-identical determinism contract therefore still passes unchanged. Pause/resume, command queue, command log, UI, AI, events, multi-player all deliberately out of scope (M2.2+). |
| [`m2-2-pause-resume.md`](m2-2-pause-resume.md) | M2.2 | **Pause / resume / step primitives.** Extract the runner's day-at-a-time loop into three public free functions backed by a new `runner::TickController` runtime struct (lives outside `GameState`, never saved). `begin_tick` resolves `--player` + captures `start_date` + emits the start log + initial snapshot row. `step_one_day` advances one day, emits month / year logs, runs the M1.10 monthly pipeline on month boundaries, appends per-month snapshot rows. `end_tick` emits the end log + sanity_check + final snapshot row, resolves output paths, writes save / JSONL / CSV files, returns `RunOutcome`. `run_state` is rewritten as a thin composition; M1.17's 5-artefact byte-identical determinism contract is preserved by construction (pinned by two equivalence tests: `begin/step×N/end == run_state(days=N)` and a `15+16` pause/resume case). Misuse paths (double begin, step before begin, step after end, double end) rejected with specific messages. Drive-by: 2 regression tests pin that bad `--player` (empty world / unknown id_code) leaves no `save.json` / `events.jsonl` on disk. **No save-format bump (still v8), no new CLI flag, no new logs, no M1 system change.** |
| [`m2-3-command-queue.md`](m2-3-command-queue.md) | M2.3 | **Player command queue.** New `core::PlayerCommand{kind, policy_id_code}` data type (`PlayerCommandKind::EnactPolicy` is the only kind in M2.3). New `systems::commands::{CommandQueue, ApplyOutcome, apply_pending}` module. The queue is owned by an outer driver (not part of `GameState`, not part of `runner::TickController`). `apply_pending` requires `state.player_country` to index into `state.countries`, drains in insertion order, dispatches each `EnactPolicy` through `policy::apply_policy_effects` (reusing M1.5 atomicity, M1.15 active_policies tracking, M1.15 duration cap). Non-atomic across the list: first failure stops with the failed command at the head of the queue; previously-applied commands stay applied. **No save-format bump (still v8), no new CLI flag, no new logs, no auto-drain inside `step_one_day`, no other command kinds, no command log, no queue persistence, no M1 system change.** |
| [`m2-4-command-log.md`](m2-4-command-log.md) | M2.4 | **Player command log.** New `core::AppliedPlayerCommand{applied_on, command}` type and new `GameState::applied_commands` vector. `systems::commands::apply_pending` appends one log entry per successful per-command dispatch (after the M1.5 / M1.15 state mutation lands), so per-command atomicity covers the log; failed commands stay in the queue and do NOT log. `applied_on` captures `state.current_date` at apply time. **Save format bumped v8 → v9** with `"applied_commands"` as a required root-level array of `{applied_on, command: {kind, policy_id_code}}` objects; v8 saves rejected loudly; missing array / malformed `applied_on` / unknown `kind` / missing `policy_id_code` / missing `command` sub-object all rejected with `applied_commands[N]` in the error. Foundation for future deterministic replay (RFC-050 §8). **No replay implementation yet, no log compaction, no log entries for failed commands, no new `PlayerCommandKind` variants, no new CLI flag, no new lifecycle log line, no auto-drain inside `step_one_day`, no M1 system change.** |
| [`m2-5-adjust-budget.md`](m2-5-adjust-budget.md) | M2.5 | **AdjustBudget player command.** Adds `PlayerCommandKind::AdjustBudget` + two new payload fields on `PlayerCommand` (`budget_category`, `budget_delta`). `commands::apply_pending` gains a new switch arm: validates the 7-category whitelist + that `budget_delta` is finite, applies `budget.<category> += delta` and clamps to `[0, 1]` (same M1.5 ratio-clamp policy). Per-command atomicity + M2.4 log-on-success shared unchanged — failed `AdjustBudget` does not log; successful one logs with `budget_category` + `budget_delta` in the entry. `save_system` kind ↔ string mapping grows; per-kind JSON shape emits only the relevant payload (`EnactPolicy` keeps `policy_id_code`; `AdjustBudget` emits `budget_category` + `budget_delta`). **No save format bump (still v9)** — array shape unchanged, only the kind-string set grew; existing strict-required-fields-per-kind validator already gates old binaries. Drive-by: PR #32 reviewer nit — `player_command_kind_to_string` fallback now returns the `"UnknownPlayerCommandKind"` sentinel instead of a real kind string, so unhandled-enum bugs surface loudly. **No replay, no UI, no AI, no other command kinds, no new CLI flag, no automatic sum-to-1 budget enforcement, no M1 system change.** |

## Reading order

If you're new to the codebase:

1. Start with the top-level `README.md` for current status, build,
   and test instructions.
2. Skim `rfc/README.md` and the RFC documents it indexes for the
   high-level design intent.
3. Read the milestone notes here **in order** (M0.2 → M0.10 → M1.1
   → M1.2 → M1.3 → M1.4 → M1.5 → M1.6 → M1.7 → M1.8 → M1.9 → M1.10
   → M1.11 → M1.12 → M1.13 → M1.14 → M1.15 → M1.16 →
   `milestone-1-result.md` → M2.1 → M2.2 → M2.3 → M2.4 → M2.5).
   They build on each other and each one tries to call out the
   rules a future contributor must not silently break.

## What's next

**M2 has begun.** M2.1 (player country selection), M2.2 (pause /
resume / step primitives), M2.3 (player command queue), M2.4
(player command log), and M2.5 (AdjustBudget command) shipped.
Suggested next M2 sub-milestones:

- **M2.6 — Deterministic replay.** First consumer of the M2.4
  log: re-issue every entry of a save's `applied_commands` against
  a fresh-loaded scenario and verify final state convergence.
  Save-format-neutral.
- **M2.7 — More command kinds.** Continue growing
  `PlayerCommandKind` (`ChangeTaxBurden`, `ToggleMartialLaw`, ...).
  Each new kind adds its own dispatch arm and per-kind JSON shape,
  same pattern M2.5 set.

The deferred-from-M1 items (expiration sweep, effect revert,
faction `react` extension, balance pass) are NOT M2 work and can
land later as targeted follow-ups when the player loop needs them.

Per the M-pacing rule, the next sub-milestone is **not** started
until M2.5 is merged.

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

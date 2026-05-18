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
| [`m1-17-end-to-end-tests.md`](m1-17-end-to-end-tests.md) | M1.17 | **M1 exit / end-to-end integration tests.** Three new doctest cases in `tests/integration/m1_end_to_end_test.cpp`: (1) scenario load → day-0 enactment → 365-day tick → save / load round-trip on the canonical `1930_with_start_policies.json` (asserts the M1.13 day-0 ActivePolicy entries, 12 monthly pipelines fired across 1930, 14 / 42 / 42 rows in summary / countries / factions CSVs); (2) 10-year soak run satisfying RFC-090 §1.17's "跑 10 年單國測試" acceptance criterion (3652 days, 120 monthly pipelines, zero sanity issues, all numeric fields finite and clamped, runs in under 100 ms); (3) 5-artefact byte-identical determinism across `save.json` / `events.jsonl` / `summary.csv` / `countries.csv` / `factions.csv`. Also publishes `milestone-1-result.md`. **No new system, no new formula, no new artefact, no save schema bump, no new CLI flag.** Backfilled per-sub-milestone design note; canonical M1 ledger remains in `milestone-1-result.md`. **M1 closes here.** |
| [`milestone-1-result.md`](milestone-1-result.md) | **M1 exit report** | What M1 ships (every sub-milestone M1.1–M1.16 + the M1.17 integration tests), the five-artefact determinism contract, deferred items (expiration sweep, effect revert, full scheduler, AI / events / war / diplomacy, balance pass, faction `react` extension, CSV quoting, multi-country / international layer, replay vs session resume), recommendations for M2 (player-operation prototype per RFC-090 §M2), the architectural invariants every M2+ milestone must preserve, and known small-scope tech debt. **M1 closes here.** |
| [`m2-1-player-country.md`](m2-1-player-country.md) | M2.1 | **Player country selection (Milestone 2 kickoff).** New `GameState::player_country` (`CountryId`, default `invalid()`). New `--player COUNTRY_IDCODE` runner flag; resolved in `run_state` after scenario load by linear scan against `state.countries[i].id_code`. Failure cases: empty world, unknown id_code — both rejected before any tick / log / snapshot is emitted, with the offending id_code in the error message. **Save format bumped v7 → v8.** `"player_country"` is a required root-level integer (-1 = headless; non-negative must index into `countries`); v7 saves rejected loudly; non-integer / `< -1` / out-of-range / above `INT_MAX` all rejected with specific messages. **No system reads the field yet.** None of M1's systems (faction::react / stability::tick / economy::tick / monthly pipeline / diagnostics) branch on `player_country`; M1's 5-artefact byte-identical determinism contract therefore still passes unchanged. Pause/resume, command queue, command log, UI, AI, events, multi-player all deliberately out of scope (M2.2+). |
| [`m2-2-pause-resume.md`](m2-2-pause-resume.md) | M2.2 | **Pause / resume / step primitives.** Extract the runner's day-at-a-time loop into three public free functions backed by a new `runner::TickController` runtime struct (lives outside `GameState`, never saved). `begin_tick` resolves `--player` + captures `start_date` + emits the start log + initial snapshot row. `step_one_day` advances one day, emits month / year logs, runs the M1.10 monthly pipeline on month boundaries, appends per-month snapshot rows. `end_tick` emits the end log + sanity_check + final snapshot row, resolves output paths, writes save / JSONL / CSV files, returns `RunOutcome`. `run_state` is rewritten as a thin composition; M1.17's 5-artefact byte-identical determinism contract is preserved by construction (pinned by two equivalence tests: `begin/step×N/end == run_state(days=N)` and a `15+16` pause/resume case). Misuse paths (double begin, step before begin, step after end, double end) rejected with specific messages. Drive-by: 2 regression tests pin that bad `--player` (empty world / unknown id_code) leaves no `save.json` / `events.jsonl` on disk. **No save-format bump (still v8), no new CLI flag, no new logs, no M1 system change.** |
| [`m2-3-command-queue.md`](m2-3-command-queue.md) | M2.3 | **Player command queue.** New `core::PlayerCommand{kind, policy_id_code}` data type (`PlayerCommandKind::EnactPolicy` is the only kind in M2.3). New `systems::commands::{CommandQueue, ApplyOutcome, apply_pending}` module. The queue is owned by an outer driver (not part of `GameState`, not part of `runner::TickController`). `apply_pending` requires `state.player_country` to index into `state.countries`, drains in insertion order, dispatches each `EnactPolicy` through `policy::apply_policy_effects` (reusing M1.5 atomicity, M1.15 active_policies tracking, M1.15 duration cap). Non-atomic across the list: first failure stops with the failed command at the head of the queue; previously-applied commands stay applied. **No save-format bump (still v8), no new CLI flag, no new logs, no auto-drain inside `step_one_day`, no other command kinds, no command log, no queue persistence, no M1 system change.** |
| [`m2-4-command-log.md`](m2-4-command-log.md) | M2.4 | **Player command log.** New `core::AppliedPlayerCommand{applied_on, command}` type and new `GameState::applied_commands` vector. `systems::commands::apply_pending` appends one log entry per successful per-command dispatch (after the M1.5 / M1.15 state mutation lands), so per-command atomicity covers the log; failed commands stay in the queue and do NOT log. `applied_on` captures `state.current_date` at apply time. **Save format bumped v8 → v9** with `"applied_commands"` as a required root-level array of `{applied_on, command: {kind, policy_id_code}}` objects; v8 saves rejected loudly; missing array / malformed `applied_on` / unknown `kind` / missing `policy_id_code` / missing `command` sub-object all rejected with `applied_commands[N]` in the error. Foundation for future deterministic replay (RFC-050 §8). **No replay implementation yet, no log compaction, no log entries for failed commands, no new `PlayerCommandKind` variants, no new CLI flag, no new lifecycle log line, no auto-drain inside `step_one_day`, no M1 system change.** |
| [`m2-5-adjust-budget.md`](m2-5-adjust-budget.md) | M2.5 | **AdjustBudget player command.** Adds `PlayerCommandKind::AdjustBudget` + two new payload fields on `PlayerCommand` (`budget_category`, `budget_delta`). `commands::apply_pending` gains a new switch arm: validates the 7-category whitelist + that `budget_delta` is finite, applies `budget.<category> += delta` and clamps to `[0, 1]` (same M1.5 ratio-clamp policy). Per-command atomicity + M2.4 log-on-success shared unchanged — failed `AdjustBudget` does not log; successful one logs with `budget_category` + `budget_delta` in the entry. `save_system` kind ↔ string mapping grows; per-kind JSON shape emits only the relevant payload (`EnactPolicy` keeps `policy_id_code`; `AdjustBudget` emits `budget_category` + `budget_delta`). **No save format bump (still v9)** — array shape unchanged, only the kind-string set grew; existing strict-required-fields-per-kind validator already gates old binaries. Drive-by: PR #32 reviewer nit — `player_command_kind_to_string` fallback now returns the `"UnknownPlayerCommandKind"` sentinel instead of a real kind string, so unhandled-enum bugs surface loudly. **No replay, no UI, no AI, no other command kinds, no new CLI flag, no automatic sum-to-1 budget enforcement, no M1 system change.** |
| [`m2-6-replay-prototype.md`](m2-6-replay-prototype.md) | M2.6 | **Replay applied command log prototype.** New `systems::commands::replay(state, log)` free function + `ReplayOutcome` struct. For each entry: forces `state.current_date = entry.applied_on`, builds a 1-element `CommandQueue`, calls `apply_pending`. The 1-elem-per-entry approach inherits every M2.3 dispatch + M2.4 log-append + M1.5/M1.15 effect machinery guarantee without duplicating logic. **Preconditions**: `state.player_country` valid + `state.applied_commands` empty (replay would otherwise mix new entries with prior ones; callers replay onto a freshly-loaded scenario, not a reloaded save). **Atomicity across the log** mirrors M2.3 mid-list-failure: failed entry reported with `replay[N]: ...` in the error; prior entries stay applied + logged; later entries skipped. **Prototype limits pinned by tests**: no time-system advancement between commands; `current_date` ends at the last entry's `applied_on` (not the source's actual final date); scenario must be pre-loaded by caller. **No save-format bump (still v9), no new CLI flag, no new log line, no divergence detection, no M1 system change.** Foundation for M2.7's full-replay variant that integrates with the M2.2 `step_one_day` primitive. |
| [`m2-7-replay-with-time.md`](m2-7-replay-with-time.md) | M2.7 | **Replay with time-system advancement.** New `systems::commands::replay_with_time(state, opts, ctrl, log)` free function. Lifts M2.6's "no time advance" limit by interleaving day-by-day advancement (via M2.2 `step_one_day`) with command dispatch: for each entry, advances until `current_date == applied_on`, then runs 1-element queue through `apply_pending`. The M1.10 monthly pipeline therefore runs naturally on any month boundary between two consecutive entries. **Preconditions**: M2.6's pair + `ctrl.started && !ctrl.ended` + monotonic non-decreasing dates (addresses PR #34 nit). **Atomicity** preserves M2.3 mid-list-failure shape with the documented caveat that `state.current_date` may have advanced partway on a `step_one_day` failure. **Killer equivalence test pins behavior-preservation**: replaying a recorded log onto a fresh state reproduces the original simulation's `current_date`, `days_stepped`, `monthly_ticks`, every log entry, the command-effect fields, AND the monthly-pipeline-mutated fields (`gdp`, `stability`, `last_gdp_growth_rate`). M2.6 `replay` stays alongside for time-stripped replay. `commands.hpp` now includes `runner.hpp` (acyclic: runner does not include commands). **No save-format bump (still v9), no new CLI flag, no UI, no AI, no event integration, no divergence-report API, no transactional rollback, no new state.logs entry.** |
| [`m2-8-replay-cli.md`](m2-8-replay-cli.md) | M2.8 | **Replay CLI harness.** Wires M2.7 into the runner via a new `--replay PATH` flag. `RunnerOptions` gains `replay_path`; `RunOutcome` gains `replay_commands_replayed`. `run()` branches: when `--replay` is set, requires `--scenario` for the fresh baseline, loads the save at PATH, optionally inherits `player_country` from the loaded save (when `--player` is unset), runs `begin_tick → replay_with_time(loaded.applied_commands) → end_tick`, populates the outcome counter, and returns. `main()` prints `Replay source` + `Commands replayed` lines when active. The CLI does NOT auto-compare the replayed state against the source — the user diffs the two save files themselves. **No per-field state-comparison API, no `--target-date` flag, no save format change, no replay outside `run()`, no new lifecycle log entries, no replay against a different scenario, no multi-save replay chains.** Foundation for M2.10+ programmatic divergence detection. |
| [`m2-9-replay-cli-error-paths.md`](m2-9-replay-cli-error-paths.md) | M2.9 | **Replay CLI error-path hardening.** Doc + tests PR with zero library behaviour change. Cements the **pre-`end_tick`** contract: when `--replay` is set and the run fails before `end_tick` is reached (`save_system::load` failure on the source save, empty-`state.countries` replay precondition, `begin_tick` rejection, `replay_with_time` failure on out-of-order log / unknown policy id_code / malformed budget command / monthly pipeline failure), the runner writes ZERO output artefacts (save.json / events.jsonl / summary.csv / countries.csv / factions.csv) because `end_tick` is the only function that touches disk and none of those failure paths reach it. Failures INSIDE `end_tick` itself are explicitly NOT covered — its five writes are sequential and non-transactional, so a mid-`end_tick` I/O failure can leave a partial set on disk; making `end_tick` atomic (temp-file + rename) is a deliberate non-goal of this PR. `runner::run`'s doc comment gains an explicit "M2.9 contract" block; 3 regression tests pin the pre-`end_tick` guarantee for missing source / out-of-order log / unknown policy id_code via shared `wire_all_artifacts` + `check_no_artifacts` helpers. Closes the gap M2.8 left open: --replay shipped but its failure-path artefact semantics weren't spelled out. **No save format change, no new flag, no new gameplay, no new `state.logs` entry on failure, no retry/rollback machinery, no atomic `end_tick` (out of scope for M2.9), no M1 system change, no replay-mode partial-progress reporting.** |
| [`m2-10-state-comparison.md`](m2-10-state-comparison.md) | M2.10 | **State comparison API.** New `systems::diagnostics::compare_states(a, b, opts)` free function + `StateMismatch` / `CompareOptions` structs. Walks two `GameState`s field-by-field in canonical order and returns a list of mismatches; empty list = match. **Compared fields**: `current_date`, `player_country`, every country's identity strings + 13 numerics + 7 budget categories + `active_policies` entries, every faction's identity + 5 numerics + preferred_policies count, every `applied_commands` entry (date + kind + payload). **Deliberately skipped** (each with rationale in the design note): `rng` (no divergent-RNG model yet), `logs` (begin/end_tick boilerplate noise), `policies` (immutable templates), `provinces` / `events` (still reserved-empty), `simulation_config` (not in GameState). Floating-point tolerance defaults to `1e-9` (matches M0.8 save round-trip precision); customisable. `field_path` mirrors save JSON addressing (`countries[0].budget.military`) so the same string is usable from CLI output / test asserts / error messages. Library-only — anticipated consumers: replay-equivalence integration tests and a future `--verify` CLI flag. **No save-format bump (still v9), no CLI integration in this PR, no relative-tolerance option, no log / rng / policy comparison, no mismatch-budget cap, no M1 system change.** |
| [`m2-11-replay-verify.md`](m2-11-replay-verify.md) | M2.11 | **Replay verify CLI.** New `--verify` boolean runner flag (requires `--replay`) wires M2.10 `compare_states` into the M2.8 replay flow. After `end_tick` succeeds, the runner calls `compare_states(replayed_state, loaded_source)` and populates `RunOutcome::verify_mismatches`. `main()` prints `Verify mismatches: N` plus one bullet per mismatch (`  - <field_path> : <detail>`). **Informational only** — exit code stays 0 regardless of mismatch count; artefacts (save / JSONL / CSV) are still written so the user can forensically inspect. Reuses the already-loaded source save (no extra disk I/O). `parse_args` rejects `--verify` without `--replay` with both flag names in the error. **No save-format bump (still v9), no strict fail-on-mismatch mode (`--verify-strict` is a future candidate), no CLI tolerance knob, no `--verify` outside `--replay`, no mismatch-list truncation, no M1 system change.** |
| [`m2-12-verify-strict.md`](m2-12-verify-strict.md) | M2.12 | **Replay strict mode.** New `--verify-strict` boolean runner flag (requires `--verify`) makes `main()` exit `EXIT_FAILURE` when M2.11 detects any mismatches. The full mismatch list still prints to stdout before the non-zero exit so CI logs capture every divergence. **Architectural decision**: `run()` semantics unchanged — it still returns success when the simulation+replay completes; strict mode is a `main()`-level exit-code policy. Tradeoff is one extra line of policy in `main()`; benefit is library/CLI separation stays clean and other consumers (tests, future embedders) can apply their own policy. `parse_args` rejects `--verify-strict` without `--verify` with both flag names in the error. Flag-chain: `--verify-strict` → `--verify` → `--replay`. **No save-format bump (still v9), no `--verify-tolerance` CLI knob (M2.13 candidate), no structured-diff output format, no mismatch-count threshold (strict is binary: any mismatch fails), no `run()` behaviour change, no M1 system change.** |
| [`m2-13-verify-tolerance.md`](m2-13-verify-tolerance.md) | M2.13 | **Verify tolerance CLI.** New `--verify-tolerance FLOAT` runner flag (requires `--verify`) overrides M2.10's default `1e-9` `CompareOptions::double_tolerance` when calling `compare_states`. Parses via a new exception-free `parse_nonneg_double` helper that rejects empty input, trailing garbage (`"1.5x"`), non-finite values (`NaN`/`Inf`), and negatives at parse time with the flag name + bad value in the error. Plumbed into `run()`'s replay branch by building a `diagnostics::CompareOptions` with the override applied only when set. `main()` prints `Verify tolerance: <value>` when active so CI logs show which tolerance produced the mismatch count. **Completes the M2 replay-CLI family** (`--replay` / `--verify` / `--verify-strict` / `--verify-tolerance`). **No save-format bump (still v9), no library behaviour change beyond passing the override through, no relative tolerance, no per-field tolerance, no new gameplay.** |
| [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md) | **M4 checkpoint** (M4.9) | M4 status snapshot at the M4.9 moment. Single-page contract spec covering: the 10-artefact set (M3 close shipped 8; M4.2 added `provinces.svg`; M4.5 added `map.html`); the SVG body shape shared between `provinces.svg` and `map.html` via `render_svg_root` (viewBox + per-node `<circle>` + `<text>` with the M4.8 four-data-* identity surface); the HTML wrapper shape (HTML5 doc + 6-rule `<style>` + inline SVG + `<ul class="legend">` with one row per country); the DOM identity contract a future clickable UI can grep against (province keys: `data-id` / `data-owner` / `data-owner-code` / `data-name` on both `<circle>` and `<text>`; country keys: `<li data-owner="N">` rows in the legend; coordinate space; owner colour); invariants future M4.x sub-milestones must preserve (deterministic + valid + additive-only data-* growth + provinces.svg ↔ map.html SVG body sync + per-element no-inline-style + legend 1:1 + 10 artefacts + v12 save floor + the M4.5/M4.6/M4.7 "no" lists); and deferred items (JavaScript / clickable UI / hover / tooltips / state mutation / neighbour adjacency / terrain / responsive sizing / atomic `end_tick` writes / M4 close-out). **M4 remains in progress** — `milestone-4-result.md` deliberately not written yet. |
| [`m4-17-aria-labels-skeleton.md`](m4-17-aria-labels-skeleton.md) | M4.17 | **ARIA labels skeleton.** Makes the M4.15-focusable / M4.10-clickable province markers **screen-reader-readable**. Every `<circle>` and `<text>` now carries `role="button"` + `aria-label="<name>, <owner_name>"`. The aria-label is composed at render time: full form `<name>, <owner_name>` when the owner index resolves; fallback `<name>` alone (no trailing comma) when invalid. The composed string is XML-attribute-escaped via the M4.2 helper, so a name with `& < > " '` cannot break attribute syntax. Same single bounds check as `data-owner-code` / `data-owner-name` gates the fallback. Both attrs go on both elements (M4.8/M4.13 uniform-identity-surface pattern). Lives in `render_svg_root` so `provinces.svg` picks up both attrs too. Legend swatch `<circle>` elements stay decorative (no `role`, no `aria-label`). **This narrowly REVERSES the M4.15/M4.16 "no ARIA" non-goal**: only `role="button"` + `aria-label` ship. The broader still-deferred ARIA surface (`aria-selected`, `aria-current`, `aria-pressed`, `aria-live`, `aria-describedby`, `aria-labelledby`) lands in a future dedicated A11Y sub-milestone. The M4.15/M4.16 unit tests retune accordingly: their over-broad `role=` / `aria-label=` absence checks become narrower-ARIA-surface absence checks. `role="button"` matches the click + Enter/Space activation model exactly; `role="link"` / `role="option"` / no role were rejected with reasoning in the design note. aria-label values match what the M4.10/M4.11 details panel renders for `Province Name` / `Owner Name` rows, so sighted + screen-reader users get the same identity. M4.10's XSS-safe DOM API, no-network discipline, asymmetric one-inline-script invariant, M4.12 `.selected` surface, M4.13 five-attr DOM contract, M4.15 `tabindex` + keydown handler, M4.16 `:focus-visible` rings all carry over unchanged (additive only). **M4 remains in progress** — no `docs/milestone-4-result.md`. **Save format stays v12** — aria-label is composed from existing `ProvinceNode` + `state.countries` fields, not a new persistent state field. `provinces.svg` bytes DID change (two new attrs on every `<circle>` + `<text>` — additive only); `map.html` bytes did change (same SVG body). **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass. 7 new doctest cases (878 total). **No `aria-selected`, no `aria-current`, no `aria-pressed`, no `aria-live`, no `aria-describedby`, no `aria-labelledby`, no `role` other than `"button"`, no `<title>` / `<desc>` child elements on markers, no state mutation, no commands, no AI, no events, no selection persistence, no tooltip, no hover, no animation, no keyboard shortcut for the panel, no save schema bump, no new state field / artefact / fixture / `InterestGroupKind` / `PlayerCommandKind`, no rename of the M4.8 / M4.13 data-* keys, no second `<script>`, no `<script src=>`, no `<script type=>`, no `<link>`, no external CSS / font / `<iframe>` / `<img>`, no `fetch` / XHR / storage / history / navigation APIs, no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`, no inline event attributes, no per-element inline `style="..."`, no `<meta name="viewport">`, no neighbour / adjacency edges, no terrain / resources / population overlays, no runner CLI flag, no M4 close-out, no `docs/milestone-4-result.md`, no "M4 closed" wording.** |
| [`m4-16-focus-visible-skeleton.md`](m4-16-focus-visible-skeleton.md) | M4.16 | **Focus-visible styling skeleton.** Makes M4.15's keyboard focus VISIBLE. Pure CSS — no JavaScript change, no markup change beyond the `<style>` block. Four new CSS rules in the M4.6 `<style>` block: `svg circle:focus { outline: none; }` + `svg circle:focus-visible { outline: none; stroke: #1976d2; stroke-width: 4; }` + `svg text:focus { outline: none; }` + `svg text:focus-visible { outline: 2px solid #1976d2; outline-offset: 2px; }`. The bare-`:focus` rules suppress the browser's default focus outline so the `:focus-visible` styling wins. Uses `:focus-visible` (NOT bare `:focus`) so the ring appears for keyboard-triggered focus only — mouse clicks paint only the M4.12 `.selected` black stroke; keyboard Tab paints only the M4.16 blue ring; keyboard activate (Enter/Space) paints both (correct dual-state cue). Colour `#1976d2` (Material Blue 700) chosen to contrast with the M4.3 owner palette and the M4.12 `#000000` `.selected` stroke. Circle uses stroke-based ring (matches shape outline); text uses CSS `outline` + `outline-offset` (rectangular ring around the text bounding box). M4.10's XSS-safe DOM API, no-network discipline, asymmetric one-inline-script invariant, M4.12 `.selected` surface, M4.13 five-attr DOM contract, and M4.15 `tabindex="0"` + keydown handler all carry over unchanged (additive only). **Still NO ARIA polish** — `role=` / `aria-label=` / `aria-selected=` / `aria-current=` / `aria-pressed=` all explicitly absent (tests pin this). That lands in a future dedicated A11Y sub-milestone. **M4 remains in progress** — no `docs/milestone-4-result.md`. **Save format stays v12.** `provinces.svg` bytes UNCHANGED from M4.15 (focus CSS is HTML-only); `map.html` bytes did change (four new CSS rules). **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass. 5 new doctest cases (871 total). **No state mutation, no commands, no AI, no events, no selection persistence, no tooltip, no hover, no animation / transition, no `:focus-visible` polyfill (modern browsers only; old browsers fall back to no ring, no regression), no save schema bump, no new state field / artefact / fixture / `InterestGroupKind` / `PlayerCommandKind`, no rename of the M4.8 / M4.13 data-* keys, no second `<script>`, no `<script src=>`, no `<script type=>`, no `<link>`, no external CSS / font / `<iframe>` / `<img>`, no `fetch` / XHR / storage / history / navigation APIs, no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`, no inline event attributes, no per-element inline `style="..."`, no `<meta name="viewport">`, no neighbour / adjacency edges, no terrain / resources / population overlays, no runner CLI flag, no change to `provinces.svg` bytes, no M4 close-out, no `docs/milestone-4-result.md`, no "M4 closed" wording.** |
| [`m4-15-keyboard-focus-skeleton.md`](m4-15-keyboard-focus-skeleton.md) | M4.15 | **Keyboard focus accessibility skeleton.** First keyboard-input surface for the M4 viewer. Every `<circle>` and every `<text>` in the SVG body now carries `tabindex="0"` (rendered in `render_svg_root`, so the standalone `provinces.svg` picks it up too); the inline `<script>` in `map.html` registers a `keydown` listener alongside the existing `click` listener so pressing **Enter** or **Space** while focused on a province fires the same `selectProvince + showDetails` pair the click runs. The keydown handler calls `event.preventDefault()` for Space (suppresses default page-scroll). Click and keydown handlers share a per-element `activate()` closure so the effect cannot drift between modalities. Legend swatch `<circle>` elements in `map.html` are emitted separately (inside `render_map_html`, not in `render_svg_root`) and lack `tabindex` — they stay out of the tab order. **Explicit non-goal: NO ARIA polish** — no `role=`, no `aria-label=`, no `aria-selected=`, no `aria-current=`, no `aria-pressed=`, no `:focus` / `:focus-visible` CSS, no `tabindex` values other than `"0"`, no keyboard shortcut for the panel, no skip-link, no focus management between renders. That lands in a future dedicated A11Y sub-milestone. M4.10's XSS-safe DOM API (`createElement` + `textContent` only; no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`), no-network discipline, asymmetric one-inline-script invariant, M4.12's transient `.selected` surface, and the M4.8 + M4.13 five-attr DOM contract all carry over unchanged (additive only). **M4 remains in progress** — no `docs/milestone-4-result.md`; M4.15 is one more skeleton sub-milestone, not an exit. **Save format stays v12** — `tabindex` is render-time only, not a new field on `ProvinceNode`. `provinces.svg` bytes DID change (new `tabindex="0"` on every `<circle>` + `<text>` — additive only); `map.html` bytes did change (same SVG body + refactored listener loop + new keydown wiring in the script). **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass. 9 new doctest cases (866 total: 6 svg_export + 1 integration test E + 2 standalone-SVG / cross-checks). **No state mutation, no commands, no AI, no events emitted by keyboard activation, no selection persistence, no multi-select / shift-Enter / right-click, no hover, no tooltip, no animation, no save schema bump, no new state field / artefact / fixture / `InterestGroupKind` / `PlayerCommandKind`, no rename of the M4.8 / M4.13 data-* keys, no second `<script>`, no `<script src=>`, no `<script type=>`, no `<link>`, no external CSS / font / `<iframe>` / `<img>`, no `fetch` / XHR / storage / history / navigation APIs, no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`, no inline event attributes (`onkeydown=` / ...; uses `addEventListener` exclusively), no per-element inline `style="..."`, no `<meta name="viewport">`, no CSS animations / transitions / media queries / `@import` / `@font-face`, no neighbour / adjacency edges, no terrain / resources / population overlays, no runner CLI flag, no M4 close-out, no `docs/milestone-4-result.md`, no "M4 closed" wording.** |
| [`m4-14-checkpoint-refresh.md`](m4-14-checkpoint-refresh.md) | M4.14 | **DOM contract checkpoint refresh.** Mirrors M4.9's role for the M4 reaction loop: zero new behaviour, just a refreshed status snapshot + one new integration assertion. Refreshes `docs/milestone-4-checkpoint.md` from its original M4.2–M4.8 scope to cover the four surfaces that landed in M4.10 (first inline `<script>` in `map.html`; asymmetric JS boundary — `provinces.svg` stays script-free), M4.11 (details panel `<dt>` labels decoupled from raw `data-*` keys), M4.12 (transient `.selected` class + `circle.selected` / `text.selected` CSS + `selectProvince(el)` helper — purely DOM-level, no persistence), and M4.13 (fifth `data-owner-name` attribute on every `<circle>` and `<text>`, derived from `state.countries[owner].name`). The refreshed checkpoint now enumerates 13 CSS selectors in the `<style>` block (was 6 at M4.9), an interactivity-surface section listing `<div id="details">` / `.selected` / `selectProvince` / `showDetails` / the 5-entry `fields` array, and a reworked deferred-items list bucketed into HOVER+TOOLTIPS / KEYBOARD+A11Y / PERSISTENT SELECTION / DOM EXTENSIONS / VISUAL POLISH / INFRASTRUCTURE. Adds **one** new integration assertion (`tests/integration/m4_dom_contract_test.cpp` test D): the canonical `map.html` script carries the M4.13-era five-entry fields list — both the five raw `data-*` attribute names and the five human-readable labels appear verbatim inside the inline `<script>`; `provinces.svg` carries none of the JS-literal forms. End-to-end mirror of the M4.11/M4.13 svg_export_test unit cases through the actual runner / canonical fixture path. **M4 remains in progress** — no `docs/milestone-4-result.md`; M4.14 is a checkpoint refresh, not an exit. Renderer bytes byte-identical with M4.13 — only tests + docs ship. **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass. 1 new doctest case (857 total). **No new system / formula / artefact / state field / fixture, no save schema bump, no new feature surface, no rename of any data-* attribute, no change to the click handler / details panel / `.selected` CSS / fields array bytes, no `<meta name="viewport">`, no CSS animations / transitions / media queries, no adjacency / terrain / overlays, no events / AI / commands, no hover / tooltip / keyboard nav / `aria-*` polish, no selection persistence, no runner CLI flag, no atomic `end_tick` writes, no M4 close-out, no `docs/milestone-4-result.md`, no "M4 closed" wording, no change to `provinces.svg` or `map.html` bytes.** |
| [`m4-13-details-owner-name-polish.md`](m4-13-details-owner-name-polish.md) | M4.13 | **Details panel owner-name polish.** Widens the M4.8 identity surface by one attribute and the M4.11 details-panel `fields` array by one row. Every `<circle>` and every `<text>` in the SVG body now also carries `data-owner-name`, resolved from `state.countries[owner.value()].name` (or `""` when the owner index is invalid — same defensive fallback as `data-owner-code`). A single bounds check covers both lookups; the value is XML-attribute-escaped via the M4.2 helper. The M4.11 `fields` array grows from four to five entries — `{ attr: "data-owner-name", label: "Owner Name" }` — so the details panel renders five dt/dd pairs (`Province ID` / `Owner Index` / `Owner Code` / `Owner Name` / `Province Name`). **Save format stays v12** — `data-owner-name` is **derived** from `state.countries` at render time, not a new field on `ProvinceNode`. The alternative path (DOM-walking the legend `<li>` for the country name) was rejected in favour of the uniform-DOM-surface route so future hover / tooltip / clickable-UI sub-milestones get the country name via one `getAttribute("data-owner-name")` call without coupling to the legend structure. M4.10's XSS-safe DOM API, no-network discipline, asymmetric one-inline-script invariant, M4.12's transient `.selected` surface, and the M4.8 keys themselves all carry over unchanged (additive only — no rename). **M4 remains in progress** — no `docs/milestone-4-result.md`; M4.13 is one more additive widening. `provinces.svg` bytes DID change (the new attribute on every `<circle>` + `<text>` — additive only); `map.html` bytes did change (same SVG body + the new fifth `fields` entry). **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass. 8 new doctest cases (856 total). **No new field on `ProvinceNode`, no save schema bump, no new state field, no new artefact, no new fixture, no new `InterestGroupKind` / `PlayerCommandKind`, no rename of the M4.8 data-* keys, no state mutation, no commands, no AI, no events, no selection persistence, no multi-select / right-click, no hover state, no tooltips, no keyboard navigation / focus ring / `aria-*` polish, no animation, no second `<script>`, no `<script src=>`, no `<script type=>`, no `<link>`, no external CSS / font / `<iframe>` / `<img>`, no `fetch` / XHR / storage / history / navigation APIs, no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`, no inline event attributes, no per-element inline `style="..."`, no `<meta name="viewport">`, no CSS animations / transitions / media queries / `@import` / `@font-face`, no neighbour / adjacency edges, no terrain / resources / population overlays, no runner CLI flag, no M4 close-out, no `docs/milestone-4-result.md`, no "M4 closed" wording.** |
| [`m4-12-selected-state-css-skeleton.md`](m4-12-selected-state-css-skeleton.md) | M4.12 | **Clickable UI selected-state CSS skeleton.** Layers a transient selection highlight on top of the M4.10 click handler / M4.11 details labels. Two new CSS rules in the M4.6 `<style>` block: `svg circle.selected { stroke: #000000; stroke-width: 3; }` and `svg text.selected { font-weight: bold; }`. The click handler now also calls a `selectProvince(el)` helper that uses `classList.remove("selected")` on every prior `.selected` node and `classList.add("selected")` on every node sharing the clicked element's `data-id` (clicking either the `<circle>` or the `<text>` highlights the whole province pair — fulfils the M4.8 design intent that "a future clickable UI can address either element uniformly"). The selection walks the pre-collected `nodes` NodeList and compares `data-id` strings, so the attribute value never re-enters a CSS-selector parser at runtime. Initial render carries **NO `class="selected"`** anywhere; the class only appears at click time. **Selection is purely DOM-level**: never written into `GameState`, never persisted across reloads (no `localStorage` / `sessionStorage` / cookie / URL fragment). M4.10's XSS-safe DOM API (`createElement` + `textContent` only; no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`), no-network discipline (no `fetch` / `XMLHttpRequest`), and asymmetric "exactly one inline `<script>` in `map.html`; `provinces.svg` stays script-free" invariant all carry over unchanged. The M4.8 `data-*` DOM contract on `<circle>` / `<text>` is **NOT renamed**. **M4 remains in progress** — no `docs/milestone-4-result.md`; M4.12 is the first selection-surface skeleton, not an exit. `provinces.svg` bytes unchanged from M4.8; `map.html` bytes did change (two new CSS rules + new `selectProvince` helper + extended click listener). **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass. 5 new doctest cases (848 total). **No state mutation, no commands, no AI, no events emitted by the selection, no selection persistence, no multi-select / right-click / context menu, no hover state, no tooltips, no keyboard navigation / focus ring / `aria-*` polish, no animation / transition on the highlight, no save schema bump, no new state field, no new artefact, no new fixture, no new `InterestGroupKind` / `PlayerCommandKind`, no rename of the M4.8 data-* DOM contract keys, no second `<script>`, no `<script src=>`, no `<script type=>`, no `<link>`, no external CSS / font / `<iframe>` / `<img>`, no `fetch` / XHR / storage / history / navigation APIs, no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`, no `className` string concatenation, no `setAttribute("class", ...)`, no inline event attributes, no per-element inline `style="..."`, no `<meta name="viewport">`, no CSS animations / transitions / media queries / `@import` / `@font-face`, no neighbour / adjacency edges, no terrain / resources / population overlays, no runner CLI flag, no change to `provinces.svg` bytes, no M4 close-out, no `docs/milestone-4-result.md`, no "M4 closed" wording.** |
| [`m4-11-details-labels-polish.md`](m4-11-details-labels-polish.md) | M4.11 | **Clickable UI details labels polish.** Pure UX polish on the M4.10 click handler. The `<dt>` labels rendered into the `<div id="details">` panel are decoupled from the raw `data-*` attribute names: `getAttribute` still reads the M4.8 DOM contract keys (`data-id` / `data-owner` / `data-owner-code` / `data-name` — **NOT renamed**; the `<circle>` / `<text>` surface is byte-identical with M4.10), but each `<dt>` now renders a fixed human-readable label (`Province ID` / `Owner Index` / `Owner Code` / `Province Name`). One JS literal change in `render_map_html`: `var keys = [...]` → `var fields = [{attr, label}, ...]`, with `dt.textContent = f.label` and `dd.textContent = el.getAttribute(f.attr) || ""`. M4.10's XSS-safe DOM API (`createElement` + `textContent` only; no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`), no-storage / no-network discipline (no `fetch` / `XMLHttpRequest` / `localStorage` / `sessionStorage` / `history.pushState` / `window.location` / `navigator`), and asymmetric "exactly one inline `<script>` in `map.html`; `provinces.svg` stays script-free" invariant all carry over unchanged. **M4 remains in progress** — no `docs/milestone-4-result.md`; M4.11 is a UX polish, not an exit. `provinces.svg` bytes unchanged from M4.8; `map.html` bytes did change (four new label strings + new `fields` array structure). **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass. 4 new doctest cases (843 total). **No rename of the M4.8 data-* DOM contract keys, no state mutation, no commands, no AI, no events emitted by the click, no selection persistence, no multi-select / right-click / context menu, no hover, no tooltip, no keyboard navigation / focus ring / `aria-*` polish, no animation, no save schema bump, no new state field, no new artefact, no new fixture, no new `InterestGroupKind` / `PlayerCommandKind`, no second `<script>`, no `<script src=>`, no `<script type=>`, no `<link>`, no external CSS / font / `<iframe>` / `<img>`, no `fetch` / XHR / storage / history / navigation APIs, no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`, no inline event attributes, no per-element inline `style="..."`, no `<meta name="viewport">`, no CSS animations / transitions / media queries / `@import` / `@font-face`, no neighbour / adjacency edges, no terrain / resources / population overlays, no runner CLI flag, no change to `provinces.svg` bytes, no M4 close-out, no `docs/milestone-4-result.md`, no "M4 closed" wording.** |
| [`m4-10-clickable-ui-skeleton.md`](m4-10-clickable-ui-skeleton.md) | M4.10 | **HTML clickable UI skeleton.** First JavaScript in `map.html`. A single inline `<script>` IIFE at the end of `<body>` attaches one `click` listener per `svg circle[data-id], svg text[data-id]` element; the listener reads the four M4.8 `data-*` attributes off the clicked element via `getAttribute` and renders them as a `<dl>` inside a new `<div id="details">` placeholder that sits between the inline SVG and the M4.7 legend. Placeholder starts with `<p class="details-empty">Click a province to see its details.</p>`; first click replaces the placeholder, subsequent clicks replace the previous `<dl>`. Handler is **stateless + XSS-safe**: `createElement` + `textContent` only (no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`), and never calls `fetch` / `XMLHttpRequest` / `localStorage` / `sessionStorage` / `history.pushState` / `window.location` / `navigator`. The selector deliberately skips the M4.7 legend swatch `<circle>` elements (no `data-id`), keeping the legend non-clickable. Four new CSS rules (`.details` + dl/dt/dd + `.details-empty` + `svg circle[data-id], svg text[data-id] { cursor: pointer; }`) live in the M4.6 `<style>` block — **no per-element inline `style="..."`**; M4.6 single-CSS-surface contract holds. JavaScript boundary is now **asymmetric**: `provinces.svg` stays fully script-free; `map.html` carries EXACTLY ONE inline script (no `src=`, no `type=`). M4.9's integration test C splits to enforce the new asymmetric invariant; M4.5/M4.6 "no `<script>`" unit test retunes. **M4 remains in progress** — no `docs/milestone-4-result.md`; M4.10 is one more skeleton sub-milestone, not an exit. `provinces.svg` bytes unchanged from M4.8; `map.html` bytes did change (new CSS + placeholder + script). **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass. 8 new doctest cases (7 svg_export + 1 runner; 839 total). **No state mutation, no commands, no AI, no events emitted by the click, no selection persistence, no multi-select / right-click / context menu, no hover state, no tooltips, no keyboard navigation / focus ring / `aria-*` polish, no animation, no save schema bump, no new state field, no new artefact, no new fixture, no new `InterestGroupKind` / `PlayerCommandKind`, no second `<script>`, no `<script src=>`, no `<script type=>`, no `<link>`, no external CSS / font / `<iframe>` / `<img>`, no `fetch` / `XMLHttpRequest` / `localStorage` / `sessionStorage` / `history.pushState` / `window.location` / `navigator`, no `innerHTML` / `outerHTML` / `document.write` / `eval` / `Function`, no inline event attributes, no per-element inline `style="..."`, no `<meta name="viewport">`, no CSS animations / transitions / media queries / `@import` / `@font-face`, no neighbour / adjacency edges, no terrain / resources / population overlays, no runner CLI flag, no change to `provinces.svg` bytes, no M4 close-out, no `docs/milestone-4-result.md`, no "M4 closed" wording.** |
| [`m4-9-dom-contract-checkpoint.md`](m4-9-dom-contract-checkpoint.md) | M4.9 | **HTML DOM contract checkpoint.** Per-sub-milestone design note for the M4.9 PR. Three new integration cases (`tests/integration/m4_dom_contract_test.cpp`): (1) every canonical province surfaces all four `data-*` attributes on both `<circle>` and `<text>` in both `provinces.svg` and `map.html`; (2) `map.html` legend carries one `<li data-owner="N">` per `state.countries[i]` with each row's body containing the country's `id_code`; (3) no-interactivity invariant — both artefacts have no `<script>` / `<link>` / inline event attributes / per-element `style="..."`, and `provinces.svg` additionally has no `<style>` / `font-family`. Mirrors M3.7's role for the M3 reaction loop: ZERO new behaviour, just pin the existing contract before the next sub-milestone (clickable UI etc.) consciously edits it. **M4 remains in progress.** **Renderer bytes unchanged from M4.8** — only tests + docs ship. **Artefact set unchanged (still 10); save format unchanged (still v12);** byte-identical determinism contracts continue. 3 new doctest cases (830 total). **No new system, no new formula, no new artefact, no save schema bump, no new state field, no new fixture, no JavaScript / `<script>` / `<link>` / inline event attributes / per-element inline `style="..."` / `<meta name="viewport">` / CSS animations / click handlers / clickable UI / hover state / tooltips / state mutation / neighbour adjacency edges / terrain / events / AI / command integration / runner CLI flag / atomic `end_tick` writes / M4 close-out / `docs/milestone-4-result.md` / "M4 closed" wording / change to `provinces.svg` or `map.html` bytes.** |
| [`m4-8-province-data-attributes-skeleton.md`](m4-8-province-data-attributes-skeleton.md) | M4.8 | **HTML static province data attributes skeleton.** Widens the identity surface inside the SVG body: every `<circle>` and every `<text>` now carries the same four read-only `data-*` attributes (`data-id`, `data-owner`, `data-owner-code`, `data-name`) so a future clickable UI / DOM script can address either element uniformly without DOM-walking siblings. `data-owner-code` resolves to `state.countries[owner.value()].id_code` when valid, or `""` otherwise (defensive fallback for hand-built states; save / scenario layers reject invalid owners at load time). `data-name` mirrors the `<text>` body content but exposes it as a uniform attribute for programmatic lookup. All four data-* values XML-attribute-escaped via the M4.2 helper. **First M4.x sub-milestone since M4.4 that deliberately edits the standalone SVG body** — the change is purely additive (no removed attributes, no rendered-pixel movement); SVG-to-PNG pipelines and vector tools see no visual difference. Both `provinces.svg` and `map.html` pick up the new attributes for free since they share the `render_svg_root` helper. All M4.5 / M4.6 / M4.7 nots preserved: no JavaScript, no `<script>`, no `<link>`, no inline event attributes, no inline `style="..."` per element, no `<meta name="viewport">`, no CSS animations / transitions / media queries / `@import` / `@font-face`, no click handlers, no clickable UI, no hover state, no tooltips, no state mutation. **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass by construction. 8 new doctest cases (7 svg_export + 1 runner; M4.4 empty-name test retuned to anchor on the new stable surface; 827 total). **M4 in progress.** **No JavaScript, no click handlers, no event handlers / hover state / tooltips, no state mutation (data-* are read-only viewer surface), no `<script>` / no `<link>` / no inline event attributes / no inline `style="..."` per element, no `<meta name="viewport">`, no CSS animations / transitions / media queries / `@import` / `@font-face`, no new artefact, no save schema bump, no new state field, no new `InterestGroupKind` / `PlayerCommandKind`, no runner CLI flag, no events / AI / command integration, no ownership-dynamics layer, no neighbour / adjacency edges, no terrain / resources / population overlays, no new gameplay, no atomic `end_tick` writes, no per-province colour override, no new `<circle>` / `<text>` presentation attributes (the change is data-* only).** |
| [`m4-7-html-legend-skeleton.md`](m4-7-html-legend-skeleton.md) | M4.7 | **HTML legend skeleton.** Adds a static `<ul class="legend">` to `map.html` immediately after the inline SVG so a viewer can decode which palette colour belongs to which country. One `<li data-owner="N">` per `state.countries[i]`, in vector order; content is a tiny 16x16 inline SVG swatch coloured via `color_for_owner(CountryId{i})` followed by `"id_code &mdash; name"` text. Per-row swatch is inline SVG (not an HTML element with `background-color`) so the M4.6 "no inline `style="..."` per element" constraint stays unbroken — `fill` on `<circle>` is an SVG presentation attribute, not an HTML inline style. Three new CSS rules (`.legend`, `.legend li`, `.legend .swatch`) join the M4.6 block for layout (list-style removal, centred `max-width: 1000px`, flex layout for swatch + text, fixed swatch size). Legend text content XML-text-escaped via the M4.4 helper. Empty `state.countries` produces an empty `<ul>` (always-present-file contract preserved). **`provinces.svg` byte output unchanged** — legend lives only in the HTML wrapper; the standalone SVG path stays CSS-free / legend-free for downstream consumers. All M4.5 / M4.6 nots preserved: no JavaScript, no `<script>`, no `<link>`, no inline event attributes, no inline `style="..."` per element, no `<meta name="viewport">`, no CSS animations / transitions / media queries / `@import` / `@font-face`. M4.4 `<text>` font-family / font-size contract on the elements themselves preserved. **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass by construction. 9 new doctest cases (8 svg_export + 1 runner; 819 total). **M4 in progress.** **No JavaScript / `<script>`, no `<link>` external stylesheet, no inline event attributes, no inline `style="..."`, no `<meta name="viewport">`, no CSS animations / transitions / media queries / `@import` / `@font-face`, no click handlers, no clickable UI, no hover state, no tooltips, no state mutation from the viewer, no font-family / font-size on the SVG `<text>` elements themselves, no ownership dynamics, no neighbour / adjacency edges, no terrain / resources / population overlays, no events, no AI, no command integration, no new `PlayerCommandKind`, no runner CLI flag, no save schema bump, no new state field, no new gameplay, no atomic `end_tick` writes, no change to `provinces.svg` bytes.** |
| [`m4-6-html-viewer-css-skeleton.md`](m4-6-html-viewer-css-skeleton.md) | M4.6 | **HTML viewer minimal CSS skeleton.** Adds the smallest possible inline `<style>` block to the M4.5 HTML wrapper — three CSS selectors: `body` (zero margin + 20px padding + `#f0f0f0` page bg so the white SVG card pops), `svg` (`display: block` + `margin: 0 auto` to centre + 1px `#888` border + `#ffffff` background so the SVG looks like a card), `svg text` (`font-family: sans-serif` so labels are more readable than the browser's serif default for SVG `<text>`). `<style>` sits at 2-space indent inside `<head>` alongside `<meta>` + `<title>`. **`provinces.svg` byte output unchanged** — CSS lives only in the HTML wrapper; the standalone-SVG path stays CSS-free for downstream consumers (SVG-to-PNG pipelines, vector tools). All M4.5 nots preserved: no JavaScript, no `<script>`, no `<link>`, no inline event attributes, no `<meta name="viewport">`, no inline `style="..."` on individual elements. M4.4 `<text>` font-family / font-size contract preserved on the elements themselves; only the CSS selector `svg text` sets the font and applies only to the HTML viewer. **Artefact set unchanged (still 10); save format unchanged (still v12);** M1.17 / M2.22 / M3.7 byte-identical determinism contracts continue to pass by construction. 6 new doctest cases (5 svg_export + 1 runner; the M4.5 "no `<style>`" test was retuned to drop its `<style>` assertion and add an inline-`style=` check; 810 total). **M4 in progress.** **No JavaScript / `<script>`, no `<link>` external stylesheet, no inline event attributes, no inline `style="..."`, no `<meta name="viewport">`, no per-province / per-element CSS override, no CSS animations / transitions / media queries / `@import` / `@font-face`, no click handlers, no clickable UI, no hover state, no tooltips, no state mutation from the viewer, no legend, no font-family / font-size on the `<text>` elements themselves, no ownership dynamics, no neighbour / adjacency edges, no terrain / resources / population overlays, no events, no AI, no command integration, no new `PlayerCommandKind`, no runner CLI flag, no save schema bump, no new state field, no new gameplay, no atomic `end_tick` writes, no change to `provinces.svg` bytes.** |
| [`m4-5-html-viewer-skeleton.md`](m4-5-html-viewer-skeleton.md) | M4.5 | **HTML viewer skeleton.** Wraps the existing SVG body in a minimal HTML5 document so the map opens cleanly in a browser. New public functions on `leviathan::systems::svg_export`: `render_map_html(state) → std::string` and `write_map_html(state, path) → Result<bool>`. Internal refactor extracted a shared `render_svg_root` helper so `render_provinces` continues to emit exactly the same bytes (existing M4.x tests stay green without modification). HTML shape: `<!DOCTYPE html>` + `<html lang="en">` + minimal `<head>` (`<meta charset="UTF-8">` + `<title>`) + `<body>` with the inline `<svg>` body (no XML prolog — invalid inside HTML). No CSS / no JavaScript / no `<style>` / no `<script>` / no `<link>` / no inline event handlers — M4.5 ships the minimum wrapper. Inline (not external-reference) embedding so the file is self-contained — no `file://` vs `http://` CORS pitfalls. `end_tick` writes `map.html` UNCONDITIONALLY as the **10th artefact** (mirrors M3.5 / M3.6 / M4.2 pattern; satisfies the M3-exit-report §5 "growing the set needs its own sub-milestone" rule for the second time). `RunnerOptions::map_html_path` optional override (no CLI flag); default `<output_dir>/map.html`; `RunOutcome::map_html_path` carries resolution. M2.9 pre-`end_tick` no-artefact contract extends automatically; M3.6 mid-`end_tick` non-transactional caveat extends similarly. M1.17 / M2.22 / M3.7 byte-identical determinism contracts extended from 9 to 10 artefacts. **Artefact set now 10; save format unchanged (still v12); `provinces.svg` bytes unchanged from M4.4.** 12 new doctest cases (7 svg_export + 5 runner; 804 total). **M4 in progress.** **No click handlers, no clickable UI, no event handlers, no hover state, no tooltips, no state mutation from the viewer, no legend / colour key, no CSS / JavaScript / `<style>` / `<script>` / `<link>` / inline event attributes, no `<meta name="viewport">`, no `font-family` / `font-size` on `<text>`, no ownership dynamics, no neighbour / adjacency edges, no terrain / resources / population overlays, no events, no AI, no command integration, no new `PlayerCommandKind`, no runner CLI flag, no save schema bump, no new state field, no new gameplay, no atomic `end_tick` writes.** |
| [`m4-4-svg-labels-skeleton.md`](m4-4-svg-labels-skeleton.md) | M4.4 | **SVG labels skeleton.** Adds one `<text>` label per `<circle>` in `provinces.svg`. Each label is positioned at `(cx, cy + kLabelYOffset)` with `text-anchor="middle"`; content is the XML-text-escaped `ProvinceNode::name`. New public constant `kLabelYOffset = 22.0` on `leviathan::systems::svg_export`. New `xml_text_escape` helper (anonymous namespace) escapes only `& < >` per the XML 1.0 §2.4 text-content rules; the M4.2 `xml_attr_escape` (which also handles `" '`) continues to cover `data-id` unchanged. `<circle>` and `<text>` are interleaved per node (one of each, in `state.provinces` order). No `font-family` / `font-size` / `fill` on `<text>` — SVG consumer default applies; typography deferred to a future presentation sub-milestone. Empty `name` still emits an empty-bodied `<text>` so the renderer is total under hand-built states (the save / scenario layers reject empty names in production paths). Every other SVG byte (viewBox, circle attributes, owner-driven palette, `data-id` (XML-escaped) / `data-owner` identity, insertion order, fixed-precision coords, LF terminators, header-only-on-empty) byte-identical with M4.3. **Artefact set unchanged (still 9); save format unchanged (still v12)**; M1.17 / M2.22 / M3.7 byte-identical determinism contracts still pass by construction. 8 new doctest cases (7 svg_export + 1 runner; 792 total). **M4 in progress.** **No HTML viewer, no clickable UI, no event handlers, no hover state / tooltips, no legend / colour key, no `font-family` / `font-size` / `fill` on `<text>`, no label collision detection, no per-province label override, no rich text / multi-line labels, no ownership dynamics, no neighbour / adjacency edges, no terrain / resources / population overlays, no events, no AI, no command integration, no new `PlayerCommandKind`, no runner CLI flag, no new artefact, no save schema bump, no new state field, no new gameplay, no atomic `end_tick` writes.** |
| [`m4-3-svg-owner-color-skeleton.md`](m4-3-svg-owner-color-skeleton.md) | M4.3 | **SVG owner-color skeleton.** Replaces M4.2's hardcoded `fill="black"` with a deterministic per-owner palette lookup. New public symbols on `leviathan::systems::svg_export`: `kOwnerPalette` (10-entry `constexpr std::array<string_view, 10>` of hex-RGB strings), `kOwnerPaletteSize`, `kOwnerFallbackFill` (`#888888`), and `color_for_owner(CountryId) → string_view`. Palette indexed by `owner.value() % kOwnerPaletteSize` (modulo wraps; future growth-by-appending preserves existing owner→colour mappings); negative owner returns the defensive fallback so the renderer is total under hand-built states even though the save / scenario layer rejects invalid owners. Canonical owners GER / FRA / JPN map to entries 0 / 1 / 2 (steel blue / indian red / goldenrod). Every other SVG attribute — viewBox, circle radius, `data-id` (still XML-escaped), `data-owner`, insertion order, fixed-precision coords, LF terminators, header-only-on-empty — is byte-identical with M4.2. **Artefact set unchanged (still 9)**. **Save format unchanged (still v12)**. M1.17 / M2.22 / M3.7 byte-identical determinism contracts still pass by construction (same state → same colours → same bytes). 7 new doctest cases (6 svg_export + 1 runner; 784 total). **M4 in progress.** **No HTML viewer, no clickable UI, no event handlers, no hover state, no labels / text elements, no legend / colour key, no per-province colour override, no ownership dynamics, no neighbour / adjacency edges, no terrain / resources / population overlays, no events, no AI, no command integration, no new `PlayerCommandKind`, no runner CLI flag, no new artefact, no save schema bump, no new state field, no new gameplay, no atomic `end_tick` writes.** |
| [`m4-2-svg-exporter-skeleton.md`](m4-2-svg-exporter-skeleton.md) | M4.2 | **SVG exporter skeleton.** First renderer for the M4.1 `ProvinceNode` data layer. New `leviathan::systems::svg_export` module with `render_provinces(state) → std::string` (pure transform) and `write_provinces(state, path) → Result<bool>` (render + file write). Output is a deterministic SVG with `viewBox="0 0 1000 1000"`, one `<circle>` per province at `cx = node.x * 1000` / `cy = node.y * 1000` / `r=8` / `fill="black"`, plus `data-id` + `data-owner` identity attributes; insertion order preserved; LF terminators; `std::fixed` + `setprecision(2)` for byte-stable coords; empty `state.provinces` produces a header-only `<svg>`. `end_tick` writes `provinces.svg` UNCONDITIONALLY as the **9th artefact** (mirrors M3.5 / M3.6 pattern; per `milestone-3-result.md` §5 a 9th artefact requires its own sub-milestone with the contracts documented — M4.2 is it). `RunnerOptions::provinces_svg_path` optional override (no CLI flag); default `<output_dir>/provinces.svg`; `RunOutcome::provinces_svg_path` carries resolution. M2.9 pre-`end_tick` no-artefact contract extends automatically; M3.6 mid-`end_tick` non-transactional caveat extends similarly (still a deferred item). M1 / M2 / M3 integration byte-identical determinism tests extended 8 → 9. Branch name carries explicit `rfc090-` prefix to disambiguate from the rolled-back invented-M4.X work. 12 new doctest cases (8 svg_export + 5 runner; 776 total). **M4 in progress.** **No HTML viewer, no clickable UI, no event handlers, no hover state, no map colours, no per-country palette, no ownership dynamics, no neighbour / adjacency edges, no controller-vs-owner split, no terrain / resources / population overlays, no labels / text elements, no events, no AI, no command integration, no new `PlayerCommandKind`, no runner CLI flag, no save schema bump (still v12), no new state field, no new gameplay, no atomic `end_tick` writes.** |
| [`m4-1-svg-map-data-skeleton.md`](m4-1-svg-map-data-skeleton.md) | **M4.1** (opens RFC-090 §M4) | **SVG map data skeleton.** First sub-milestone of RFC-090 §M4 (SVG map + UI). Replaces the dead M0 `ProvinceState{id, owner}` stub with a typed `core::ProvinceNode { id_code, name, owner, x, y }` where `x` / `y` are normalised `[0, 1]` map coordinates. `GameState::provinces` becomes a typed vector but no system reads it yet — M4.1 is **data only**; the future SVG exporter / HTML viewer / clickable map consumes it. **Save format bumped v11 → v12**: `provinces` array REQUIRED at the save layer (empty allowed), every entry validated (non-empty id_code + name, owner resolving into `state.countries`, x / y finite in `[0, 1]`, duplicate id_code rejected); v11 saves rejected loudly. Scenario loader gains an OPTIONAL root-level `provinces` array of file paths pointing at per-file province manifests with `{ "provinces": [ {id, name, owner, x, y}, ... ] }`; manifests authored before M4.1 stay valid (missing key parses as empty); cross-file id_code uniqueness enforced. New canonical fixture `data/provinces/1930_core_nodes.json` ships three nodes (`berlin` / `paris` / `tokyo` owned by GER / FRA / JPN), wired into both canonical scenario manifests. `ScenarioLoadOutcome` gains `provinces_loaded`. `diagnostics::compare_states` walks the provinces vector (size + per-field paths). 19 new doctest cases (8 save_system + 8 scenario_loader + 3 diagnostics; 764 total). **M4 in progress.** **No SVG exporter, no HTML viewer, no clickable UI, no province rendering, no map colours, no ownership dynamics, no neighbour adjacency, no terrain / resources / population, no war / fronts / movement, no events, no AI, no command integration, no new `PlayerCommandKind`, no runner CLI flag, no new artefact (still 8), no CSV for provinces, no changes to M3 formulas, no changes to M2 command gates, no diplomacy, no M5 event-engine work.** |
| [`milestone-3-result.md`](milestone-3-result.md) | **M3 exit report** (M3.9) | What M3 ships (every sub-milestone M3.1–M3.8 plus the M3.9 close-out doc), the final reaction-loop dataflow, the eight-artefact contract, the save-format v11 floor, the architectural invariants every future milestone must preserve (M3 systems deterministic and RNG-free; no logs / events from interest groups; rate ladder 0.05 → 0.02 → 0.01 load-bearing; canonical `tick_all_countries` order; M2 command gates byte-identical with M2.22; `bureaucratic_compliance` is a downstream input to M2 gates via M3.4; canonical scenarios now author minimal Bureaucracy groups; 8-artefact set; v11 save floor), deferred items (Military / Intelligence / Media pressure channels, richer political maps, policy preference system, event triggers from thresholds, strike / protest / coup / civil-war, cross-border influence, per-kind formulas, command-gate diagnostic / UI surface, command-gate integration beyond `bureaucratic_compliance`, UI / REPL / CLI command surfaces beyond runner flags, atomic `end_tick` writes), and **neutral next-milestone candidates** (RFC-090 M4 SVG map + UI / RFC-090 M5 event engine / deliberately non-RFC-numbered post-M3 governance follow-up — reviewer chooses; M3.9 does **not** open or claim any of them, and no "M4 in progress" wording lands in this PR). **M3 closes here.** |
| [`m3-9-m3-closeout.md`](m3-9-m3-closeout.md) | M3.9 | **M3 close-out.** Doc-only PR that publishes `docs/milestone-3-result.md` (the M3 exit report with seven sections: M3.1–M3.9 ledger, final reaction-loop dataflow, eight-artefact contract, save-format v11 floor, architectural invariants every future milestone must preserve, deferred items, neutral next-milestone candidates). Annotates `docs/milestone-3-checkpoint.md` with a "historical checkpoint" top note pointing to the exit report; keeps the rest for archaeology. Flips all three READMEs to "M3 closed" / "Latest shipped: M3.9" / "Next milestone: TBD — requires explicit reviewer direction". Mirrors M1.17 / M2.22's exit role (no per-sub-milestone gameplay deliverable; the exit-doc deliverables are the close-out). **No new system, no new formula, no new artefact (still 8), no save schema bump (still v11), no new state field, no new `InterestGroupKind`, no new fixture, no new test, no `PlayerCommandKind`, no event, no log, no AI / UI / REPL / CLI surface, no command-gate change, no runner CLI flag, no atomic `end_tick` writes.** 745 doctest cases unchanged. No "M4 in progress" wording lands in this PR; M3.9 makes no claim about which milestone is next. Backfilled per-sub-milestone design note; canonical M3 ledger is `milestone-3-result.md`. **M3 closes here.** |
| [`m3-8-canonical-interest-group-fixtures.md`](m3-8-canonical-interest-group-fixtures.md) | M3.8 | **Canonical scenario interest-group fixtures.** Data-only PR: adds one Bureaucracy interest group per canonical country (`ger_bureaucracy` / `fra_bureaucracy` / `jpn_bureaucracy`, each with `influence=0.55, loyalty=0.50, radicalism=0.10`) to `data/scenarios/1930_minimal.json` and `data/scenarios/1930_with_start_policies.json`. Up through M3.7 the three M3 CSVs (`interest_groups.csv` / `interest_group_country_feedback.csv` / `interest_group_authority_pressure.csv`) were header-only on canonical-scenario runs; M3.8 takes the canonical path off the header-only branch so a 31-day canonical run now produces 9 / 3 / 3 data rows respectively. Bureaucracy is the only kind chosen because the M3.4 `authority_pressure` step reads only Bureaucracy-kind groups, so a single Bureaucracy group per country exercises all three reverse-direction systems (M3.2 react / M3.3 country_feedback / M3.4 authority_pressure) without introducing any unimplemented gameplay. Canonical `scenario_loader` test gains 6 new asserts pinning the 3-group shape; M1.17 / M2.22 byte-identical determinism contracts are unchanged in shape (only the explanatory "canonical scenarios author zero interest groups" comments needed a refresh). New `runner_test` case asserts 9 / 3 / 3 row counts + presence of each country's id_code in each M3 CSV. **M3 remains in progress** — no `docs/milestone-3-result.md`, no "M3 closed" wording, no M4. **No new system, no new formula, no new artefact (still 8), no save schema bump (still v11), no loader semantic change, no auto-generation of interest groups, no new `InterestGroupKind`, no Military / Workers / Media / etc. groups yet, no `military_pressure` / `intelligence_pressure` / `media_pressure`, no event triggers, no command-gate diagnostic surface, no command-gate formula change, no AI / UI / REPL / CLI, no new `PlayerCommandKind`, no runner CLI flag, no atomic `end_tick` writes.** |
| [`milestone-3-checkpoint.md`](milestone-3-checkpoint.md) | **M3.7 checkpoint** | **M3 reaction-loop integration checkpoint** (M3 still in progress). Three new integration tests in `tests/integration/m3_end_to_end_test.cpp`: (1) one-month `monthly::tick_all_countries` fires every M3 leg against a hand-built one-country / one-Bureaucracy-group state and asserts every reverse-direction counter, every mutable field changed, and each trace vector got one row whose post-mutation value matches the live state field; (2) `runner::run_state` emits all eight artefacts with actual M3 data rows (covers the data-row path that canonical-scenario integration runs leave header-only); (3) two byte-identical hand-built states produce byte-identical 8 artefacts (M1.17 / M2.22 determinism contract extended to the M3-mutation path). The checkpoint doc documents the current dataflow, the eight artefacts, the invariants future sub-milestones must preserve, and the deferred items (events, AI, UI / REPL / CLI surfaces, atomic `end_tick` writes, M3 close-out, etc.) that intentionally did not ship yet. **M3 remains in progress** — no `docs/milestone-3-result.md`, no "M3 closed" wording, no M4. **No new system, no new formula, no new artefact, no save schema bump (still v11), no new state field, no new `InterestGroupKind`, no new `PlayerCommandKind`, no events, no logs from interest groups, no AI / UI / REPL / CLI surface, no command gate formula change, no command-gate diagnostic surface, no runner CLI flag, no atomic `end_tick` writes.** |
| [`m3-7-reaction-loop-integration-checkpoint.md`](m3-7-reaction-loop-integration-checkpoint.md) | M3.7 | **M3 reaction-loop integration checkpoint.** Per-sub-milestone design note companion to `milestone-3-checkpoint.md` (which captures the M3 status snapshot at the M3.7 moment, now annotated historical). Pins the M3.1–M3.6 reaction loop at the seam between M3 and any future milestone via three new doctest cases in `tests/integration/m3_end_to_end_test.cpp` (in-memory monthly tick + 8-artefact run + byte-identical determinism on the M3-mutation path), plus the checkpoint doc. M3.7 is **a checkpoint, not an exit** — the close-out is M3.9. Reviewer's spec was explicit: pin the loop but do not close M3; the 2026-05-17 force-reset history (see `milestone-3-result.md` §7) is the recorded reason. **No new system, no new formula, no new artefact (still 8), no save schema bump (still v11 at this point), no new state field, no new `InterestGroupKind`, no new `PlayerCommandKind`, no events, no logs from interest groups, no AI / UI / REPL / CLI surface, no command-gate change, no runner CLI flag, no atomic `end_tick` writes, no M3 close-out, no M4.** Backfilled per-sub-milestone design note; the checkpoint snapshot is in `milestone-3-checkpoint.md`. |
| [`m3-6-interest-group-feedback-trace-csv.md`](m3-6-interest-group-feedback-trace-csv.md) | M3.6 | **InterestGroup feedback outcome diagnostics / CSV trace surface.** Outcome-trace complement to M3.5's state surface. Two new UNCONDITIONAL CSVs: `interest_group_country_feedback.csv` (M3.3 outcome trace, 10 columns including `weight_sum` / `weighted_radicalism` / `target_stability` / `stability_before` / `stability_after` / `stability_delta`) and `interest_group_authority_pressure.csv` (M3.4 outcome trace, 10 columns including `weight_sum` / `weighted_bureaucracy_loyalty` / `target_bureaucratic_compliance` / `bureaucratic_compliance_before` / `*_after` / `*_delta`). Cadence: one row per actual mutation; skipped countries produce no row; preflight failure produces no partial rows. New `interest_group::CountryFeedbackTraceRow` + `AuthorityPressureTraceRow` POD types; `country_feedback` / `authority_pressure` gain an optional `std::vector<...>* trace_out = nullptr` arg (default-null = byte-identical with M3.3 / M3.4 baseline). `MonthlyOutcome` gains two trace vectors that `tick_all_countries` populates; `TickController` drains them in `step_one_day`. Diagnostics: `write_country_feedback_csv_header / _row` + `write_authority_pressure_csv_header / _row`. `RunnerOptions` gains two optional path overrides; **no CLI flag**. `RunOutcome` gains two paths + two row counters. `main()` prints both. Drive-by: none. **Save format unchanged (still v11)**; M1.17 / M2.22 byte-identical determinism contracts extend from 6 → 8 artefacts (canonical scenarios produce header-only trace files because they author zero interest groups). **M2.9 pre-`end_tick` no-artefact contract** automatically extends to the 7th and 8th files because `end_tick` is still the only function that writes. 24 new doctest cases. **No new gameplay, no new `PlayerCommandKind`, no new `InterestGroupKind` variants, no formula change to M3.2 / M3.3 / M3.4, no per-tick state delta CSV, no `react` (M3.2) per-mutation trace, no events / AI / UI / REPL, no new CLI flag, no command-gate integration, no atomic `end_tick` writes, no `--target-date` interaction beyond existing replay flow.** |
| [`m3-5-interest-group-csv-surface.md`](m3-5-interest-group-csv-surface.md) | M3.5 | **InterestGroup reaction diagnostics / CSV surface.** First M3 observability artefact: new `interest_groups.csv` written unconditionally by `end_tick` alongside `save.json` / `events.jsonl` / the three opt-in CSVs. Nine fixed columns: `date,id_code,name,kind,country_id,country_id_code,influence,loyalty,radicalism`. Snapshot cadence mirrors the existing CSVs (start + each `month_changed` + final post-sanity); vector-order preserved (not sorted). New `diagnostics::InterestGroupSummaryRow` + `interest_group_snapshot` + `write_interest_group_csv_header` / `write_interest_group_csv_row` + tiny `csv_escape` (RFC 4180) helper. `RunnerOptions::interest_groups_csv_path` is an optional override; **no `--interest-groups-csv` CLI flag** — keeping the artefact on by default is more useful than gating it on opt-in. `RunOutcome` gains `interest_groups_csv_path` + `interest_groups_csv_rows`; `TickController` gains `interest_group_rows`. Invalid `country` reference rejected loudly at snapshot time (no silent bogus `country_id_code`). Empty `state.interest_groups` produces a header-only file. Drive-by: extracted the `InterestGroupKind` ↔ string mapping (previously duplicated in `save_system.cpp` + `scenario_loader.cpp`) into new shared `core/interest_group_kind.{hpp,cpp}` so save / scenario / diagnostics route through one source of truth. **Save format stays v11.** M1.17 / M2.22 byte-identical determinism contracts extend from 5 → 6 artefacts (the new file is empty-but-stable in canonical scenarios). **M2.9 pre-`end_tick` no-artefact contract** automatically extends to the sixth file (end_tick is still the only function that writes). 24 new doctest cases. **No new gameplay, no new `PlayerCommandKind`, no new `InterestGroupKind` variants, no formula change to M3.2 / M3.3 / M3.4, no per-tick delta or formula-intermediates CSV, no events / AI / UI / REPL, no new CLI flag, no command-gate integration, no atomic `end_tick` writes, no `--target-date` interaction beyond existing replay flow.** |
| [`m3-4-interest-group-authority-pressure.md`](m3-4-interest-group-authority-pressure.md) | M3.4 | **InterestGroup-derived authority pressure skeleton.** Opens the second reverse-direction channel in the M3 reaction loop: interest groups press on `country.government_authority.bureaucratic_compliance`. Extends `interest_group_system` with `kInterestGroupAuthorityPressureRate = 0.01`, `AuthorityPressureOutcome { countries_updated }`, and `authority_pressure(state)` free function. For each country, computes influence-weighted loyalty over **Bureaucracy-kind** groups only and drifts `bureaucratic_compliance` toward that target at rate 0.01, clamped to `[0, 1]`. Countries with no Bureaucracy groups or zero total Bureaucracy influence skipped. Mutation surface restricted to `bureaucratic_compliance`: the other three authority sub-fields, plus country `stability` / `legitimacy` / `corruption`, are byte-identical. Strict preflight on inputs actually read (group.country / influence / loyalty / country.bureaucratic_compliance, all finite + `[0, 1]`); `radicalism` and `stability` deliberately NOT preflighted here. Wired into `tick_all_countries` as the THIRD global step, AFTER M3.3's `country_feedback`, completing the rate ladder mood (0.05) → stability (0.02) → authority (0.01). `MonthlyOutcome` gains `int interest_group_authority_countries_updated`. 19 new doctest cases. The M2.18 `EnactPolicy` gate is now a downstream consumer of the loop but M3.4 does NOT change the gate formula. **No save schema bump (still v11), no new state fields, no new `InterestGroupKind` variants, no mutation of `military_loyalty` / `intelligence_capability` / `media_control`, no mutation of `legitimacy` / `corruption` / `stability` / `central_control` / `administrative_efficiency`, no additional aggregate inputs (radicalism does not feed this step), no per-kind / per-country / per-output rate, no weighted multi-input formula beyond influence-weighted Bureaucracy loyalty, no RNG / probabilistic behaviour, no events / `state.logs` entry / AI / UI / CLI / REPL, no coup / strike / protest / civil war / cross-border behaviour, no automatic group generation, no command-gate integration, no faction reaction changes, no policy preference system, no `tick_country` change.** |
| [`m3-3-interest-group-country-feedback.md`](m3-3-interest-group-country-feedback.md) | M3.3 | **InterestGroup country feedback skeleton.** Closes the M3 reaction loop: interest groups push back on country state. Extends the M3.2 `interest_group_system` module with `kInterestGroupCountryFeedbackRate = 0.02`, `CountryFeedbackOutcome { countries_updated }`, and `country_feedback(state)` free function. Per country, computes influence-weighted radicalism (`sum(g.influence * g.radicalism) / sum(g.influence)` over matching groups with `influence > 0`) and drifts `country.stability` toward `1.0 - weighted_radicalism` at rate 0.02, clamped to `[0, 1]`. Countries with no matching groups or zero total influence skipped. The single mutation surface is `country.stability` — `legitimacy`, `government_authority`, `corruption`, `central_control`, and `administrative_efficiency` are all untouched. Strict preflight (group.country + influence + radicalism + country.stability all validated finite + `[0, 1]` before any country mutates; one NaN would otherwise poison stability). Wired into the monthly pipeline as the FINAL step of `tick_all_countries` AFTER M3.2's `react`, so it reads just-updated radicalism. `MonthlyOutcome` gains `int interest_group_countries_updated`. Slower 0.02 rate (vs M3.2's 0.05) damps the closed loop. 14 new doctest cases. Drive-by: M3.2 monthly-pipeline integration test's exact-arithmetic assertion demoted to a directional check (the M3.3 step also mutates `country.stability` post-react). **No save schema bump (still v11), no new state fields, no new InterestGroupKind variants, no additional mutation targets, no additional aggregate inputs (loyalty does not feed this step), no per-kind / per-country / per-output rate, no RNG / probabilistic behaviour, no events / `state.logs` entry / AI / UI / CLI / REPL, no coup / strike / protest / civil war / cross-border behaviour, no automatic group generation, no command-gate integration, no `government_authority` mutation, no faction reaction changes, no policy preference system, no `tick_country` change.** |
| [`m3-2-interest-group-reaction-system.md`](m3-2-interest-group-reaction-system.md) | M3.2 | **InterestGroupReactionSystem skeleton.** First M3 system to mutate the M3.1 data layer. New module `leviathan::systems::interest_group` with constant `kInterestGroupReactionRate = 0.05`, `ReactionOutcome { groups_updated }`, and `react(state)` free function. Reaction is a linear-toward-equilibrium drift on two fields driven by `country.stability`: `loyalty += (stability - loyalty) * 0.05`, `radicalism += ((1 - stability) - radicalism) * 0.05`, clamped to `[0, 1]`. `influence`, `kind`, `country`, `id_code`, and `name` are untouched. Preflight-validates every `group.country` against `state.countries` BEFORE mutating any group (atomicity). Wired into the monthly pipeline as the FINAL step of `tick_all_countries`, after every per-country `faction::react → stability::tick → economy::tick` runs; the global step reads each country's post-tick stability. `MonthlyOutcome` gains `int interest_groups_updated`. 13 new doctest cases. **No save schema bump (still v11), no influence drift, no per-kind formula, no events / AI / UI / scheduler, no country aggregate effect (interest groups do not push back on country state — M3.3+ candidate), no command-gate integration, no faction reaction changes, no RNG / probabilistic behaviour, no strikes / protests / coups / civil-war / cross-border behaviour, no new `PlayerCommandKind`, no new CSV column.** |
| [`m3-1-interest-group-state.md`](m3-1-interest-group-state.md) | M3.1 | **InterestGroupState / political actors skeleton.** Opens M3. New `core::InterestGroupKind` enum (10 variants spanning Bureaucracy / Military / Workers / Farmers / Religious / Media / Students / LocalElites / Business / Technocrats) and `core::InterestGroupState` POD (id_code / name / kind / country / influence / loyalty / radicalism). New `GameState::interest_groups` root-level vector (each entry's `country` points back to the country). **Save format bumped v10 → v11**: block REQUIRED at save layer (empty allowed) with strict per-entry validation (non-empty id_code + name, known kind string, country index resolving into `state.countries`, three ratios in `[0, 1]` via `require_ratio`, duplicate id_code rejected). `scenario_loader` accepts OPTIONAL `interest_groups` block in scenario JSON; missing → empty vector; present-but-malformed rejected with `interest_groups[N]` path. `diagnostics::compare_states` walks the array under `interest_groups[N].*`. **Data only** — no M1 / M2 system reads or writes the new fields; M1 monthly pipeline and M2 command path are byte-identical. Future M3 sub-milestones (reactions, command-resistance contributions, event triggers, AI) layer behaviour onto this shape. 20 new doctest cases. **No new CLI flag, no new PlayerCommandKind, no new CSV column, no `state.logs` entry, no replay primitive change, no command-gate integration, no automatic per-country generation, no demands / preferred policies / armed strength / ideology / foreign links, no monthly reaction system, no event triggers, no AI / UI / scheduler, no M1 / M2 system change.** |
| [`milestone-2-result.md`](milestone-2-result.md) | **M2 exit report** | What M2 ships (every sub-milestone M2.1–M2.21 plus the M2.22 integration tests), the architectural invariants every M3+ milestone must preserve, deferred items (Delayed / Distorted execution outcomes, scheduler, RNG-based resistance, persistent attempted-command log, CLI script flag, runner-level rejection surface, expanded authority fields beyond the four M2.16 fields, authority drift, faction reactions to player commands, multi-country interaction, weighted multi-input formulas, atomic `end_tick` writes, relative tolerance in `CompareOptions`), recommendations for M3+ (`Delayed` outcomes, runner-level rejection surface, CLI `--script PATH`, authority drift system, faction reaction, multi-country), and the test surface at M2 close. **M2 closes here.** |
| [`m2-22-end-to-end-tests.md`](m2-22-end-to-end-tests.md) | M2.22 | **M2 exit / end-to-end integration tests.** Three new doctest cases in `tests/integration/m2_end_to_end_test.cpp`: (1) `apply_command_script` + `replay_with_time` + `compare_states` equivalence on a fresh source state vs an independent target state (pins M2.4 / M2.7 / M2.20 / M2.21 contract that a successful script round-trips through the replay log without drift); (2) order-execution gate atomicity across `EnactPolicy` + `AdjustBudget` against a country with low `bureaucratic_compliance` but high `military_loyalty` (military adjustment lands, EnactPolicy rejected, trailing welfare adjustment untouched — M2.3 mid-list atomicity inherited through M2.18 / M2.19 / M2.20 / M2.21); (3) 5-artefact byte-identical determinism with M2 commands applied (M1.17's contract extended through M2's command + gate path). Also publishes `milestone-2-result.md`. **No new system, no new formula, no new artefact, no save schema bump, no new CLI flag, no new PlayerCommandKind.** Backfilled per-sub-milestone design note; canonical M2 ledger remains in `milestone-2-result.md`. **M2 closes here.** |
| [`m2-21-command-script-driver.md`](m2-21-command-script-driver.md) | M2.21 | **Command script driver helper.** Library-only convenience function `commands::apply_command_script(state, vector<PlayerCommand>)` on top of M2.20's `try_apply_pending` surface. Takes a one-shot script (`std::vector<PlayerCommand>`), builds a local `CommandQueue`, dispatches through `try_apply_pending`. Outcome reuses M2.20's `ApplyWithReportOutcome` (no parallel struct). Routing inherited unchanged: empty script → success + nullopt rejection; full drain → success + nullopt rejection; gate rejection → success + populated record; non-execution failure (precondition / NaN delta / unknown policy / unknown category) → `Result::failure`. Input vector NOT mutated (helper copies into the local queue). Three-line implementation; entire point is the call-site ergonomics. **No runner / RunOutcome / `main()` / CLI / replay / save schema change.** **No remaining-queue surface for the trailing commands after a mid-script rejection** — callers that need them should keep using `try_apply_pending` directly. **No persistent attempted-command log, no `state.logs` entry, no new `PlayerCommandKind`, no new CSV column, no threshold / formula change, no DataLoader / policy effect / runner / M1 system change.** |
| [`m2-20-command-rejection-reporting.md`](m2-20-command-rejection-reporting.md) | M2.20 | **Command rejection reporting.** Makes M2.18 / M2.19 order-execution rejections observable as structured data without changing `apply_pending` semantics. New POD `commands::RejectionRecord { kind, policy_id_code, budget_category, compliance, threshold, resistance }`. New wrapper `commands::ApplyWithReportOutcome { apply, rejection }`. New free function `commands::try_apply_pending(state, queue)` drains the queue exactly like `apply_pending` (same precondition, same atomicity — rejected command stays at head, no mutation, no log) but surfaces order-execution rejections as `Result::success` carrying the populated record. Non-execution errors (precondition / NaN delta / unknown policy / unknown category) still return `Result::failure` so genuine validation never gets swallowed. Internal refactor extracts a `dispatch_one` helper in `commands.cpp`'s anonymous namespace shared by both functions; `apply_pending`'s legacy rejection error string is byte-identical via a `format_rejection_message` helper. Existing M2.18 / M2.19 / replay tests pass unchanged. Drive-by: refreshed a stale M2.18-only comment in `order_execution.cpp` that PR #46 review flagged. **No save format change (still v10), no `apply_pending` signature or behaviour change, no persistent attempted-command log, no `state.logs` entry, no `RunOutcome` rejection surface (M2.21 candidate), no DataLoader / replay primitive / runner / CLI / M1 system change.** |
| [`m2-19-adjust-budget-execution-gate.md`](m2-19-adjust-budget-execution-gate.md) | M2.19 | **AdjustBudget execution gate.** Extends M2.18's command-rejection shape to `AdjustBudget` with a single category-aware twist: `command.budget_category == "military"` gates on `military_loyalty`, every other category still gates on `bureaucratic_compliance`. New constant `kAdjustBudgetComplianceThreshold = 0.3` matches M2.18's value to keep canonical default-0.5 scenarios Accepted. The `AdjustBudget` arm in `evaluate()` selects the authority input, computes `resistance = 1.0 - selected`, returns `Accepted` when `selected >= 0.3` else `Rejected`. `commands::apply_pending` gains a pre-flight gate block structurally identical to M2.18's `EnactPolicy` block; rejected `AdjustBudget` short-circuits with an error naming `order_execution`, `rejected`, `AdjustBudget`, offending `budget_category`, selected compliance, and threshold. M2.3 / M2.4 mid-list-failure atomicity preserved. Replay compatibility holds for default-0.5 saves. **No save format change (still v10), no `Delayed` / `Distorted` outcomes, no bespoke per-category inputs beyond `military` ⇒ `military_loyalty`, no weighted multi-input formula, no probabilistic / RNG gate, no scheduler, no `state.logs` entry on rejection, no `RunOutcome` rejection counter (M2.20 candidate), no DataLoader / policy effect / replay primitive / runner / M1 system change.** |
| [`m2-18-enact-policy-execution-gate.md`](m2-18-enact-policy-execution-gate.md) | M2.18 | **EnactPolicy execution gate.** First M2 sub-milestone where a player command can be **rejected**. `order_execution` grows three pieces: constant `kEnactPolicyComplianceThreshold = 0.3`, `Rejected` variant on `ExecutionStatus`, and `resistance` field on `OrderExecutionOutcome` (`1.0 - bureaucratic_compliance` for `EnactPolicy`; `0.0` for kinds without a gate yet). `evaluate()` branches on `command.kind`: `EnactPolicy` returns `Accepted` when `bureaucratic_compliance >= 0.3` and `Rejected` otherwise; `AdjustBudget` stays unconditionally `Accepted`. `commands::apply_pending` calls `evaluate` BEFORE the M2.3 policy lookup for `EnactPolicy` and short-circuits on `Rejected` with a `Result::failure` whose error names `order_execution`, `rejected`, and the policy id_code. The rejected command stays at the head of the queue and is NOT appended to `state.applied_commands` (mirrors M2.3 / M2.4 mid-list-failure atomicity). Threshold 0.3 chosen so canonical default-0.5 scenarios accept unchanged (no regression). Replay compatibility preserved: M2.7 `replay_with_time` calls `apply_pending` per entry; every entry in a v10 save was Accepted under default 0.5 authority, so re-running re-Accepts. **No save format change (still v10), no `AdjustBudget` gate, no `Delayed` / `Distorted` variants, no scheduler, no RNG, no `state.logs` entry on rejection, no `RunOutcome` field counting rejected commands, no DataLoader / policy effect / runner / M1 system change.** |
| [`m2-17-order-execution-skeleton.md`](m2-17-order-execution-skeleton.md) | M2.17 | **OrderExecutionSystem skeleton.** First M2 system that reads the M2.16 `government_authority` block. New module `leviathan::systems::order_execution` ships three types and one free function: `OrderExecutionInputs` (4-field snapshot of the actor country's authority ratios, defaults 0.5), `ExecutionStatus` enum (only `Accepted` shipped; `Rejected` / `Delayed` / `Distorted` reserved by name for M2.18+), `OrderExecutionOutcome { status, inputs }`, and `evaluate(state, command) → Result<OrderExecutionOutcome>`. `evaluate` mirrors the M2.3 `apply_pending` preconditions (valid `player_country` indexing into countries), snapshots the actor's authority into the outcome, always returns `Accepted`, and leaves state byte-identical. **No caller wires the function in yet** — `commands::apply_pending` is byte-identical with M2.5 / M2.16. **No `resistance` field** in the outcome: ship the API surface without pretending the formula shape is decided; M2.18+ introduces the formula and the resistance representation together. CMake wires the new module into `leviathan_systems` and the new test file into `leviathan_tests`. **No save format change, no new `PlayerCommandKind`, no new CSV column, no `state.logs` entry, no replay change, no policy effect change, no DataLoader change, no AI/events/UI/scheduler.** |
| [`m2-16-government-authority-state.md`](m2-16-government-authority-state.md) | M2.16 | **GovernmentAuthorityState.** First M2 gameplay-state extension. New `core::GovernmentAuthorityState` POD with four `[0, 1]` ratio fields defaulting to `0.5` (`bureaucratic_compliance`, `military_loyalty`, `intelligence_capability`, `media_control` — a stripped subset of RFC-020 §3「國家掌控力」). Added to `CountryState` as `government_authority`. **Save format bumped v9 → v10** — block REQUIRED at save layer (`require_ratio` per sub-field); DataLoader keeps it OPTIONAL in country JSON (missing → all 0.5; present → validated, including out-of-range rejection). `diagnostics::compare_states` walks the four sub-fields under `countries[N].government_authority.*`. Drive-by: every `"save_version": 9` JSON literal in tests bumped to `10` and existing hand-built v10 country JSON blocks gained the new block. **Data-only**: zero M1 systems read or write the new fields; M1 monthly pipeline and M2 command path byte-identical. Deferred from RFC-020 §3 with explicit documentation: `local_control` (distinct from existing `central_control`), `legal_mandate`, `leader_prestige`, `party_organization` — each awaits a future PR with a real gameplay consumer. **No new gameplay logic, no policy effect target, no new `PlayerCommandKind`, no new CSV column, no scenario fixture changes, no `state.logs` entry, no M1 system change.** |
| [`m2-14-replay-target-date.md`](m2-14-replay-target-date.md) | M2.14 | **Replay target-date CLI.** New `--target-date YYYY-MM-DD` runner flag (requires `--replay`) scopes the M2.8 replay flow to a specific calendar day. Two effects in one flag: **log truncation** (entries with `applied_on > target_date` are skipped before `replay_with_time` runs, monotonic-non-decreasing guarantee from M2.7 makes this a single forward scan) + **post-replay extension** (`step_one_day` loop until `current_date == target_date`, so M1.10 monthly pipeline fires naturally on every month boundary crossed). Parsed via `core::GameDate::parse`; scenario-start precondition validated in `run()` before any tick, putting bad target_date under the M2.9 pre-`end_tick` no-artefact contract. `RunnerOptions` gains `std::optional<core::GameDate> target_date`; `main()` prints `Target date: <value>` in the replay block. `replay_with_time` and `step_one_day` semantics are unchanged — M2.14 is glue. New dated-log test helper `build_source_with_dated_log` hand-splices `AppliedPlayerCommand` entries at chosen dates so truncation can be exercised without going through `apply_pending`. **No save-format bump (still v9), no `--target-date` outside `--replay`, no mid-day target, no special interaction with `--verify`, no new gameplay, no new `state.logs` entry, no M1 system change.** |

## Reading order

If you're new to the codebase:

1. Start with the top-level `README.md` for current status, build,
   and test instructions.
2. Skim `rfc/README.md` and the RFC documents it indexes for the
   high-level design intent.
3. Read the milestone notes here **in order** (M0.2 → M0.10 → M1.1
   → M1.2 → M1.3 → M1.4 → M1.5 → M1.6 → M1.7 → M1.8 → M1.9 → M1.10
   → M1.11 → M1.12 → M1.13 → M1.14 → M1.15 → M1.16 → M1.17 →
   `milestone-1-result.md` → M2.1 → M2.2 → M2.3 → M2.4 → M2.5 →
   M2.6 → M2.7 → M2.8 → M2.9 → M2.10 → M2.11 → M2.12 → M2.13 →
   M2.14 → M2.16 → M2.17 → M2.18 → M2.19 → M2.20 → M2.21 →
   M2.22 → `milestone-2-result.md` → M3.1 → M3.2 → M3.3 →
   M3.4 → M3.5 → M3.6 → M3.7 (+ `milestone-3-checkpoint.md`
   for the M3.7-moment status snapshot) → M3.8 → M3.9
   (+ `milestone-3-result.md` for the M3 exit report) →
   M4.1 → M4.2 → M4.3 → M4.4 → M4.5 → M4.6 → M4.7 → M4.8 →
   M4.9 (+ `milestone-4-checkpoint.md` for the M4-in-progress
   snapshot at the M4.9 moment, refreshed at M4.14) → M4.10
   → M4.11 → M4.12 → M4.13 → M4.14 → M4.15 → M4.16 → M4.17). They build on each other
   and each one tries to
   call out the rules a future contributor must not silently
   break.

## What's next

**M2 closed.** M2.1–M2.22 shipped. See `milestone-2-result.md`
for the full M2 exit ledger.

**M3 closed.** M3.1 + M3.2 + M3.3 + M3.4 + M3.5 + M3.6 + M3.7 + M3.8 + M3.9 shipped; see `milestone-3-result.md` for the full M3 exit ledger:

- **M3.1** introduced the data shape (InterestGroupKind enum
  + InterestGroupState POD + root-level vector + save format
  v11 bump + scenario_loader hook + diagnostics walk). No
  system read the fields.
- **M3.2** mutates the data layer for the first time: monthly
  `loyalty` / `radicalism` drift toward
  `country.stability` / `1.0 - country.stability` at rate
  0.05, wired into `tick_all_countries` as the final step.
- **M3.3** closes the reaction loop on `country.stability`:
  each country's `stability` drifts toward
  `1.0 - influence_weighted_radicalism` of its own groups at
  rate 0.02. Wired as a second global step in
  `tick_all_countries` after M3.2's `react` so it reads the
  just-updated radicalism.
- **M3.4** opens the second reverse-direction channel: each
  country's `government_authority.bureaucratic_compliance`
  drifts toward influence-weighted loyalty over its
  **Bureaucracy-kind** groups at rate 0.01. Wired as a third
  global step after M3.3's `country_feedback`. Completes the
  rate ladder (0.05 → 0.02 → 0.01).
- **M3.5** ships the first M3 observability surface:
  `interest_groups.csv` is now written unconditionally by
  `end_tick` alongside `save.json` / `events.jsonl` / the
  three opt-in CSVs. Nine fixed columns
  (`date,id_code,name,kind,country_id,country_id_code,influence,
  loyalty,radicalism`); vector-order preserved; canonical
  scenarios produce a header-only file. Drive-by extracted the
  `InterestGroupKind` ↔ string mapping into shared
  `core/interest_group_kind.{hpp,cpp}` so save / scenario /
  diagnostics route through one source of truth. **No save
  schema bump (still v11), no new CLI flag, no new
  gameplay.** Determinism contract grows from 5 → 6
  byte-identical artefacts.
- **M3.6** ships the outcome-trace complement to M3.5's
  state surface. Two new unconditional CSVs —
  `interest_group_country_feedback.csv` (M3.3 outcomes) and
  `interest_group_authority_pressure.csv` (M3.4 outcomes) —
  emit one row per *actually-mutated* country per monthly
  pipeline. Ten columns each, including `weight_sum`,
  `weighted_radicalism` / `weighted_bureaucracy_loyalty`,
  `target_*`, and `before` / `after` / `delta`. Skipped
  countries produce no row; preflight failure produces no
  partial rows. New
  `interest_group::CountryFeedbackTraceRow` +
  `AuthorityPressureTraceRow` types; `country_feedback` /
  `authority_pressure` gain an optional
  `std::vector<...>* trace_out = nullptr` argument that
  defaults to byte-identical pre-M3.6 behaviour.
  `MonthlyOutcome` surfaces the trace vectors;
  `TickController` drains them in `step_one_day`.
  `RunnerOptions` gains two optional path overrides; **no
  CLI flag**. `RunOutcome` gains two paths + two row
  counters. **No save schema bump (still v11), no formula
  change, no new gameplay.** Determinism contract grows
  from 6 → 8 byte-identical artefacts. M2.9 pre-`end_tick`
  no-artefact contract automatically extends to the 7th and
  8th files.
- **M3.7** is a **checkpoint, not a feature**. Pins the
  M3.1–M3.6 reaction loop at the seam between M3 and any
  future milestone with three new integration tests
  (`tests/integration/m3_end_to_end_test.cpp`) and a short
  checkpoint doc (`docs/milestone-3-checkpoint.md`). The
  tests cover (1) one-month `monthly::tick_all_countries`
  driving every M3 leg against a hand-built state, (2)
  `runner::run_state` emitting all eight artefacts with
  actual M3 data rows, (3) byte-identical determinism on
  the M3-mutation path. The doc lists the current dataflow,
  the eight artefacts, the invariants future sub-milestones
  must preserve, and the deferred items (events, AI, UI /
  REPL / CLI surfaces, atomic `end_tick` writes, M3
  close-out). **M3 remains in progress** — no
  `docs/milestone-3-result.md`, no "M3 closed" wording, no
  M4. **No new system, formula, artefact, save schema bump,
  state field, `InterestGroupKind`, `PlayerCommandKind`,
  event, log, AI / UI / REPL / CLI surface, command-gate
  formula change, command-gate diagnostic, runner CLI flag,
  or atomic `end_tick` writes.**
- **M3.8** is a **data-fixture sub-milestone, not a system
  PR**. Adds one Bureaucracy interest group per canonical
  country (`ger_bureaucracy` / `fra_bureaucracy` /
  `jpn_bureaucracy`, each `influence=0.55, loyalty=0.50,
  radicalism=0.10`) to the two canonical scenario manifests
  so the three M3 CSVs now contain real data rows on
  canonical-scenario runs (9 / 3 / 3 rows in the 31-day
  run, pinned by a new `runner_test` case). Bureaucracy is
  the only kind chosen so the fixture exercises all three
  reverse-direction systems (M3.4 reads only Bureaucracy
  groups) without introducing any unimplemented gameplay.
  **M3 remains in progress at this point** — no exit
  report yet. **No new system, formula, artefact (still 8),
  save schema bump (still v11), loader semantic change,
  auto-generation, new `InterestGroupKind`, Military /
  Workers / Media / etc. groups yet, `military_pressure`
  / `intelligence_pressure` / `media_pressure`, event
  triggers, command-gate diagnostic surface, command-gate
  formula change, AI / UI / REPL / CLI, `PlayerCommandKind`,
  runner CLI flag, or atomic `end_tick` writes.**
- **M3.9** is the **M3 close-out**. Doc-only PR that
  publishes `docs/milestone-3-result.md` (the M3 exit
  report), annotates `docs/milestone-3-checkpoint.md` as
  historical, and flips all three READMEs to "M3 closed".
  Tests unchanged (745 doctest cases); M3.7's integration
  tests and M3.8's canonical-data-row test already cover
  the loop / 8-artefact / canonical-data-row paths. **M3
  closes here.** No new system, no new formula, no new
  artefact (still 8), no save schema bump (still v11), no
  new state field, no new `InterestGroupKind`, no new
  fixture, no new test, no `PlayerCommandKind`, no event,
  no log, no AI / UI / REPL / CLI surface, no command-gate
  change, no runner CLI flag, no atomic `end_tick` writes,
  no M4, no post-M3 governance follow-up wording.

The deferred-from-M1 items (expiration sweep, effect revert,
faction `react` extension, balance pass) are NOT M2 / M3 work
and can land later as targeted follow-ups when the player loop
needs them.

M2 closed with M2.22. **M3 closed with M3.9** after M3.1 (data layer) +
M3.2 (country → group mood) + M3.3 (group radicalism → country
stability) + M3.4 (group loyalty → country bureaucratic
compliance) + M3.5 (interest_groups.csv observability surface) +
M3.6 (outcome-trace CSVs for M3.3 / M3.4) + M3.7 (reaction-loop
integration checkpoint) + M3.8 (canonical interest-group fixtures) +
M3.9 (exit report + READMEs flipped).

**Milestone 4 (SVG map + UI, RFC-090 §M4) is now in progress.**
- **M4.1** opens M4 with the typed `ProvinceNode` map-node data
  layer (id_code + name + owner + normalised x/y), a save schema
  bump v11 → v12 making `provinces` a required root-level array,
  an optional `provinces` block in scenario manifests pointing at
  per-file province manifests, a tiny canonical fixture
  (`berlin` / `paris` / `tokyo` owned by GER / FRA / JPN), a
  `diagnostics::compare_states` walk over the new vector, and 19
  new doctest cases. **Data only** — no SVG exporter, no HTML
  viewer, no clickable UI, no ownership dynamics, no neighbour
  adjacency, no terrain / resources / population, no events / AI /
  command integration / runner CLI flag / new artefact / CSV for
  provinces. M3 formulas and M2 command gates byte-identical.
- **M4.2** ships the first renderer: a new
  `leviathan::systems::svg_export` module turns
  `state.provinces` into a deterministic SVG document
  (`viewBox="0 0 1000 1000"`, one `<circle>` per node with
  `data-id` / `data-owner` identity attributes, byte-stable
  `std::fixed` + `setprecision(2)` coords). `end_tick` writes
  `provinces.svg` unconditionally as the **9th artefact**
  (mirrors the M3.5 / M3.6 unconditional pattern; the
  M3-exit-report §5 "adding a 9th requires its own
  sub-milestone" rule is satisfied here). `RunnerOptions::
  provinces_svg_path` optional override + no CLI flag.
  Integration tests m1 / m2 / m3 byte-identical determinism
  contracts extended 8 → 9. Branch name carries explicit
  `rfc090-` prefix to disambiguate from the rolled-back
  invented-M4.X work. 12 new doctest cases (776 total). **No
  HTML viewer, no clickable UI, no event handlers, no map
  colours, no per-country palette, no ownership dynamics,
  no adjacency edges, no controller-vs-owner split, no
  terrain / resources / population overlays, no labels /
  text elements, no events, no AI, no command integration,
  no new `PlayerCommandKind`, no runner CLI flag, no save
  schema bump (still v12), no new state field, no new
  gameplay, no atomic `end_tick` writes.**
- **M4.3** layers a deterministic per-owner palette onto the
  M4.2 renderer. Replaces `fill="black"` with a 10-entry
  fixed table (`kOwnerPalette`) indexed by
  `owner.value() % kOwnerPaletteSize`; canonical GER / FRA /
  JPN map to entries 0 / 1 / 2 (steel blue / indian red /
  goldenrod). Negative owner gets a defensive `#888888`
  fallback so the renderer is total under hand-built states.
  Public API: new `color_for_owner(CountryId)` + palette
  constants exposed in the header so tests / future callers
  can compute the expected colour without re-deriving the
  modulo + lookup. Every other SVG byte is unchanged
  (viewBox, circle radius, `data-id` (XML-escaped),
  `data-owner`, insertion order, coord precision, LF
  terminators, header-only-on-empty). **Artefact set
  unchanged (still 9); save format unchanged (still v12)**;
  M1.17 / M2.22 / M3.7 byte-identical determinism still
  passes by construction. 7 new doctest cases (784 total).
  **No HTML viewer, no clickable UI, no event handlers, no
  hover state, no labels, no legend, no per-province
  colour override, no ownership dynamics, no adjacency
  edges, no terrain, no events / AI / command integration /
  new `PlayerCommandKind` / runner CLI flag / new artefact /
  save schema bump / new state field / new gameplay /
  atomic `end_tick` writes.**
- **M4.4** adds one `<text>` label per `<circle>` in
  `provinces.svg`. Each label positioned at
  `(cx, cy + kLabelYOffset)` (new public constant, 22.0)
  with `text-anchor="middle"`; content is the
  XML-text-escaped `ProvinceNode::name`. New
  `xml_text_escape` helper (escapes `& < >` per XML 1.0
  §2.4 text-content rules) sits alongside the M4.2
  `xml_attr_escape` (which also escapes `" '` for attribute
  contexts). `<circle>` and `<text>` are interleaved per
  node; no `font-family` / `font-size` / `fill` on
  `<text>` (SVG consumer default applies; typography
  deferred). Empty `name` still emits an empty-bodied
  `<text>` so the renderer is total. Every other SVG byte
  is unchanged from M4.3. **Artefact set unchanged (still
  9); save format unchanged (still v12)**; M1.17 / M2.22 /
  M3.7 byte-identical determinism still passes by
  construction. 8 new doctest cases (792 total). **No HTML
  viewer, no clickable UI, no event handlers, no hover
  state / tooltips, no legend, no font-family / font-size /
  fill on `<text>`, no label collision detection, no
  per-province label override, no rich text / multi-line
  labels, no ownership dynamics, no adjacency edges, no
  terrain, no events / AI / command integration / new
  `PlayerCommandKind` / runner CLI flag / new artefact /
  save schema bump / new state field / new gameplay /
  atomic `end_tick` writes.**
- **M4.5** wraps the M4.2–M4.4 SVG body in a minimal HTML5
  document (`map.html`) so the map opens cleanly in a
  browser without the raw-XML chrome standalone `.svg`
  files attract. New public functions
  `render_map_html(state)` / `write_map_html(state, path)`;
  internal refactor extracted a shared `render_svg_root`
  helper so `render_provinces` continues to emit
  byte-identical output. HTML body inlines the SVG (no
  external reference, no CORS pitfalls). No CSS / no
  JavaScript / no `<style>` / no `<script>` / no `<link>`
  / no inline event handlers — minimum wrapper only.
  `end_tick` writes `map.html` unconditionally as the
  **10th artefact** (satisfies the M3-exit-report §5
  "growing the set needs its own sub-milestone" rule for
  the second time). `RunnerOptions::map_html_path`
  optional override; no CLI flag. M2.9 pre-`end_tick`
  no-artefact contract extends automatically; M3.6
  mid-`end_tick` non-transactional caveat extends
  similarly. M1.17 / M2.22 / M3.7 byte-identical
  determinism contracts extended 9 → 10. **Artefact set
  now 10; save format unchanged (still v12);
  `provinces.svg` bytes unchanged from M4.4.** 12 new
  doctest cases (804 total). **No click handlers, no
  clickable UI, no event handlers, no hover state, no
  tooltips, no state mutation from the viewer, no legend,
  no CSS / JavaScript / `<style>` / `<script>` / `<link>`
  / inline event attributes, no `<meta name="viewport">`,
  no font-family / font-size on `<text>`, no ownership
  dynamics, no adjacency edges, no terrain, no events / AI
  / command integration / new `PlayerCommandKind` / runner
  CLI flag / save schema bump / new state field / new
  gameplay / atomic `end_tick` writes.**
- **M4.6** adds the smallest possible inline `<style>`
  block to the M4.5 HTML wrapper. Three CSS selectors:
  `body { margin: 0; padding: 20px; background-color:
  #f0f0f0; }` (centre the card on a neutral page),
  `svg { display: block; margin: 0 auto; border: 1px solid
  #888; background-color: #ffffff; }` (centre + bordered
  card), `svg text { font-family: sans-serif; }` (fix the
  browser's serif default for SVG `<text>` so small labels
  are more readable). `<style>` sits at 2-space indent
  inside `<head>`. **`provinces.svg` byte output
  unchanged** — CSS lives only in the HTML wrapper. All
  M4.5 nots preserved: no JavaScript, no `<script>`, no
  `<link>`, no inline event attributes, no
  `<meta name="viewport">`, no inline `style="..."` on
  individual elements. **Artefact set unchanged (still 10);
  save format unchanged (still v12);** M1.17 / M2.22 / M3.7
  byte-identical determinism contracts continue to pass by
  construction. 6 new doctest cases (810 total). **No
  JavaScript / `<script>`, no `<link>`, no inline event
  attributes, no inline `style="..."`, no
  `<meta name="viewport">`, no per-element CSS override,
  no CSS animations / transitions / media queries /
  `@import` / `@font-face`, no click handlers, no
  clickable UI, no hover state, no tooltips, no state
  mutation from the viewer, no legend, no font-family /
  font-size on `<text>` elements themselves, no ownership
  dynamics, no adjacency edges, no terrain, no events / AI
  / command integration / new `PlayerCommandKind` / runner
  CLI flag / save schema bump / new state field / new
  gameplay / atomic `end_tick` writes / change to
  `provinces.svg` bytes.**
- **M4.7** adds a static `<ul class="legend">` to `map.html`
  right after the inline SVG so a viewer can decode which
  palette colour belongs to which country. One
  `<li data-owner="N">` per `state.countries[i]`, in vector
  order; row content = a tiny 16x16 inline SVG swatch
  (`color_for_owner(CountryId{i})`) plus `"id_code &mdash;
  name"` text. Per-row swatch is inline SVG (not an HTML
  element with `background-color`) so the M4.6 "no inline
  `style="..."` per element" constraint stays unbroken.
  Three new CSS rules join the M4.6 block (`.legend`,
  `.legend li`, `.legend .swatch`) for layout only. Text
  XML-text-escaped. Empty `state.countries` → empty `<ul>`
  (always-present file contract preserved).
  **`provinces.svg` byte output unchanged** — legend lives
  only in the HTML wrapper. **Artefact set unchanged (still
  10); save format unchanged (still v12);** byte-identical
  determinism contracts continue by construction. 9 new
  doctest cases (819 total). **No JavaScript / `<script>`,
  no `<link>`, no inline event attributes, no inline
  `style="..."`, no `<meta name="viewport">`, no CSS
  animations / transitions / media queries / `@import` /
  `@font-face`, no click handlers, no clickable UI, no
  hover state, no tooltips, no state mutation from the
  viewer, no font-family / font-size on `<text>` elements
  themselves, no ownership dynamics, no adjacency edges,
  no terrain, no events / AI / command integration / new
  `PlayerCommandKind` / runner CLI flag / save schema bump
  / new state field / new gameplay / atomic `end_tick`
  writes / change to `provinces.svg` bytes.**
- **M4.8** widens the identity surface inside the SVG body
  itself: every `<circle>` and every `<text>` now carries
  the same four read-only `data-*` attributes (`data-id`,
  `data-owner`, `data-owner-code`, `data-name`) so a future
  clickable UI / DOM script can address either element
  uniformly. `data-owner-code` resolves via
  `state.countries[owner.value()].id_code` (empty fallback
  for invalid owners); `data-name` mirrors the `<text>`
  body content as a uniform attribute. All values
  XML-attribute-escaped. **First M4.x sub-milestone since
  M4.4 to deliberately edit the standalone SVG body** —
  change is additive only (no removed attributes, no
  visual difference); both `provinces.svg` and `map.html`
  pick up the new attrs since they share `render_svg_root`.
  All prior M4.x nots preserved. **Artefact set unchanged
  (still 10); save format unchanged (still v12);**
  byte-identical determinism contracts continue to pass by
  construction. 8 new doctest cases (827 total). **No
  JavaScript, no click handlers, no event handlers / hover
  state / tooltips, no state mutation, no `<script>` / no
  `<link>` / no inline event attributes / no inline
  `style="..."` per element, no `<meta name="viewport">`,
  no CSS animations / transitions / media queries /
  `@import` / `@font-face`, no new artefact, no save
  schema bump, no new state field, no new
  `InterestGroupKind` / `PlayerCommandKind`, no runner
  CLI flag, no events / AI / command integration, no
  ownership-dynamics layer, no adjacency edges, no
  terrain, no new gameplay, no atomic `end_tick`, no
  per-province colour override, no new `<circle>` /
  `<text>` presentation attributes (the change is data-*
  only).**
- **M4.9** is a **checkpoint, not a feature** — mirrors
  M3.7's role for the M3 reaction loop. Pins the M4.2–M4.8
  SVG / HTML DOM contract via three new integration tests
  (`tests/integration/m4_dom_contract_test.cpp`) and a
  single-page snapshot (`docs/milestone-4-checkpoint.md`).
  Tests: (1) uniform data-* identity surface across both
  artefacts; (2) legend 1:1 with `state.countries`;
  (3) no-interactivity invariant. Snapshot doc lists the
  artefact set, SVG body shape, HTML wrapper shape,
  identity-surface DOM lookups, future-milestone
  invariants, and deferred items in one place. **M4
  remains in progress** — no exit report; renderer bytes
  unchanged from M4.8. **Artefact set unchanged (still
  10); save format unchanged (still v12).** 3 new doctest
  cases (830 total). **No new system / formula / artefact
  / save schema bump / state field / fixture / JavaScript
  / `<script>` / `<link>` / inline event attributes /
  per-element inline `style="..."` / `<meta name="viewport">`
  / CSS animations / click handlers / clickable UI /
  hover state / tooltips / state mutation / adjacency
  edges / terrain / events / AI / command integration /
  runner CLI flag / atomic `end_tick` writes / M4
  close-out / `milestone-4-result.md` / "M4 closed"
  wording / change to `provinces.svg` or `map.html`
  bytes.**

- **M4.10** is the **first JavaScript in `map.html`**.
  Single inline `<script>` IIFE at end of `<body>` attaches
  one `click` listener per `svg circle[data-id], svg
  text[data-id]` element; the listener reads the four M4.8
  `data-*` attrs via `getAttribute` and renders them as a
  `<dl>` inside a new `<div id="details">` placeholder
  between the SVG and the M4.7 legend. Stateless +
  XSS-safe: `createElement` + `textContent` only; no
  `innerHTML` / `outerHTML` / `document.write` / `eval` /
  `Function`; no `fetch` / `XMLHttpRequest` / `localStorage`
  / `sessionStorage` / `history.pushState` /
  `window.location` / `navigator`. Selector skips legend
  swatch `<circle>` elements (no `data-id`). Four new CSS
  rules in the M4.6 `<style>` block; no per-element inline
  `style="..."`. JavaScript boundary is now **asymmetric**:
  `provinces.svg` stays fully script-free; `map.html`
  carries EXACTLY ONE inline script (no `src=`, no
  `type=`). M4.9 integration test C splits to enforce the
  new asymmetric invariant. **M4 remains in progress** —
  no exit report. **Artefact set unchanged (still 10);
  save format unchanged (still v12);** `provinces.svg`
  bytes unchanged from M4.8; `map.html` bytes did change.
  8 new doctest cases (839 total). **No state mutation,
  no commands, no AI, no events emitted by the click, no
  selection persistence, no multi-select / right-click, no
  hover state, no tooltips, no keyboard nav / `aria-*`,
  no animation, no save schema bump, no new state field /
  artefact / fixture / `InterestGroupKind` /
  `PlayerCommandKind`, no second `<script>`, no `<script
  src=>`, no `<script type=>`, no `<link>`, no external
  CSS / font / `<iframe>` / `<img>`, no `fetch` / XHR /
  storage / history / navigation APIs, no `innerHTML` /
  `outerHTML` / `document.write` / `eval` / `Function`,
  no inline event attributes, no per-element inline
  `style="..."`, no `<meta name="viewport">`, no CSS
  animations / transitions / media queries / `@import` /
  `@font-face`, no adjacency / terrain / overlays, no
  runner CLI flag, no change to `provinces.svg` bytes,
  no M4 close-out.**

- **M4.11** is a **pure UX polish** on the M4.10 click
  handler. The `<dt>` labels rendered into the
  `<div id="details">` panel are decoupled from the raw
  `data-*` attribute names: `getAttribute` still reads the
  M4.8 DOM contract keys (`data-id` / `data-owner` /
  `data-owner-code` / `data-name` — **NOT renamed**), but
  each `<dt>` now renders a fixed human-readable label
  (`Province ID` / `Owner Index` / `Owner Code` /
  `Province Name`). One JS literal change: `var keys = [...]`
  → `var fields = [{attr, label}, ...]`. M4.10's XSS-safe
  DOM API, no-storage / no-network discipline, and
  asymmetric "exactly one inline `<script>` in `map.html`;
  `provinces.svg` stays script-free" invariant all carry
  over unchanged. **M4 remains in progress** — no exit
  report. **Artefact set unchanged (still 10); save format
  unchanged (still v12);** `provinces.svg` bytes unchanged
  from M4.8; `map.html` bytes did change (four new label
  strings). 4 new doctest cases (843 total). **No rename
  of the M4.8 data-* DOM contract keys, no state mutation,
  no commands, no AI, no events, no selection persistence,
  no multi-select / right-click, no hover, no tooltip, no
  keyboard nav / `aria-*`, no animation, no save schema
  bump, no new state field / artefact / fixture /
  `InterestGroupKind` / `PlayerCommandKind`, no second
  `<script>`, no `<script src=>` / `<script type=>`, no
  `<link>`, no external CSS / font / `<iframe>` / `<img>`,
  no `fetch` / XHR / storage / history / navigation APIs,
  no `innerHTML` / `outerHTML` / `document.write` / `eval`
  / `Function`, no inline event attributes, no per-element
  inline `style="..."`, no `<meta name="viewport">`, no
  CSS animations / transitions / media queries / `@import`
  / `@font-face`, no adjacency / terrain / overlays, no
  runner CLI flag, no change to `provinces.svg` bytes, no
  M4 close-out.**

- **M4.12** layers a **transient selection highlight** on
  top of the M4.10 click handler / M4.11 labels. Two new
  CSS rules — `svg circle.selected { stroke: #000000;
  stroke-width: 3; }` and `svg text.selected { font-weight:
  bold; }` — in the M4.6 `<style>` block. The click
  handler also calls a `selectProvince(el)` helper that
  uses `classList.remove("selected")` on every prior
  `.selected` and `classList.add("selected")` on every
  node sharing the clicked element's `data-id` (so
  clicking either the circle or the text highlights the
  whole province pair). Walks the pre-collected `nodes`
  NodeList and compares `data-id` strings; the attribute
  value never re-enters a CSS-selector parser at runtime.
  Initial render has **NO `class="selected"`** anywhere.
  **Selection is purely DOM-level**: never written into
  `GameState`, never persisted across reloads (no
  `localStorage` / `sessionStorage` / cookie / URL
  fragment). M4.10's XSS-safe DOM API, no-network
  discipline, and asymmetric "exactly one inline
  `<script>` in `map.html`; `provinces.svg` stays
  script-free" invariant all carry over unchanged. The
  M4.8 `data-*` DOM contract is **NOT renamed**. **M4
  remains in progress** — no exit report. **Artefact set
  unchanged (still 10); save format unchanged (still
  v12);** `provinces.svg` bytes unchanged from M4.8;
  `map.html` bytes did change. 5 new doctest cases (848
  total). **No state mutation, no commands, no AI, no
  events, no selection persistence, no multi-select /
  right-click, no hover, no tooltip, no keyboard nav /
  `aria-*`, no animation, no save schema bump, no new
  state field / artefact / fixture / `InterestGroupKind`
  / `PlayerCommandKind`, no rename of the M4.8 data-*
  contract, no second `<script>`, no `<script src=>` /
  `<script type=>`, no `<link>`, no external CSS / font /
  `<iframe>` / `<img>`, no `fetch` / XHR / storage /
  history / navigation APIs, no `innerHTML` / `outerHTML`
  / `document.write` / `eval` / `Function`, no
  `className` string concatenation, no `setAttribute(
  "class", ...)`, no inline event attributes, no
  per-element inline `style="..."`, no `<meta
  name="viewport">`, no CSS animations / transitions /
  media queries / `@import` / `@font-face`, no adjacency
  / terrain / overlays, no runner CLI flag, no change to
  `provinces.svg` bytes, no M4 close-out.**

- **M4.13** widens the **M4.8 identity surface by one
  attribute** (`data-owner-name`) and the M4.11 details
  panel by one row (`Owner Name`). The new attribute
  appears on every `<circle>` and every `<text>` in the
  SVG body, resolved from
  `state.countries[owner.value()].name` (or `""` when the
  owner index is invalid — same defensive fallback as
  `data-owner-code`); the single bounds check covers
  both lookups. XML-attribute-escaped via the M4.2
  helper. The M4.11 `fields` array grows from 4 to 5
  entries — `{ attr: "data-owner-name", label: "Owner
  Name" }`. **Save format stays v12** — `data-owner-name`
  is derived from `state.countries`, not a new field on
  `ProvinceNode`. The alternative path (DOM-walking the
  legend) was rejected so the future DOM surface stays
  uniform. M4.10's XSS-safe DOM API, no-network
  discipline, asymmetric one-inline-script invariant,
  M4.12's transient `.selected` surface, and the M4.8
  keys themselves all carry over unchanged (additive
  only). **M4 remains in progress** — no exit report.
  **Artefact set unchanged (still 10); save format
  unchanged (still v12);** `provinces.svg` bytes DID
  change (the new attribute — additive only); `map.html`
  bytes did change. 8 new doctest cases (856 total).
  **No new field on `ProvinceNode`, no save schema bump,
  no rename of the M4.8 data-* keys, no state mutation,
  no commands, no AI, no events, no selection
  persistence, no multi-select / right-click, no hover,
  no tooltip, no keyboard nav / `aria-*`, no animation,
  no second `<script>`, no `<script src=>` / `<script
  type=>`, no `<link>`, no external CSS / font /
  `<iframe>` / `<img>`, no `fetch` / XHR / storage /
  history / navigation APIs, no `innerHTML` /
  `outerHTML` / `document.write` / `eval` / `Function`,
  no inline event attributes, no per-element inline
  `style="..."`, no `<meta name="viewport">`, no CSS
  animations / transitions / media queries / `@import`
  / `@font-face`, no adjacency / terrain / overlays, no
  runner CLI flag, no M4 close-out.**

- **M4.14** is a **DOM contract checkpoint refresh** —
  mirrors M4.9's role; zero new behaviour. Refreshes
  `docs/milestone-4-checkpoint.md` from M4.2–M4.8 scope
  to cover M4.10 (first inline `<script>` + asymmetric
  JS boundary), M4.11 (decoupled `<dt>` labels), M4.12
  (transient `.selected` + `selectProvince(el)` —
  purely DOM-level), and M4.13 (fifth `data-owner-name`
  attribute). Adds one new integration test (test D)
  pinning the canonical `map.html` script's M4.13-era
  five-entry `fields` list (five `data-*` names + five
  human-readable labels) end-to-end. **M4 remains in
  progress** — no exit report. **Renderer bytes
  byte-identical with M4.13** — only tests + docs ship.
  **Artefact set unchanged (still 10); save format
  unchanged (still v12).** 1 new doctest case (857
  total). **No new feature, no schema bump, no rename
  of any data-* attribute, no hover / tooltip / keyboard
  nav / `aria-*`, no selection persistence, no M4
  close-out.**

- **M4.15** is the **first keyboard-input surface**.
  Every `<circle>` and `<text>` carries `tabindex="0"`
  (rendered in `render_svg_root`, so `provinces.svg`
  picks it up too); the inline `<script>` in `map.html`
  registers a `keydown` listener alongside `click` so
  Enter / Space fires the same
  `selectProvince + showDetails` pair the click runs (with
  `event.preventDefault()` on Space). Click and keydown
  share a per-element `activate()` closure. Legend swatch
  `<circle>` elements stay tabindex-free (emitted in
  `render_map_html`, not in `render_svg_root`).
  **Explicit non-goal: NO ARIA polish** (no `role=` /
  `aria-label=` / `aria-selected=` / `aria-current=` /
  `aria-pressed=` / `:focus` CSS / `tabindex` values
  other than `"0"`). That lands in a future dedicated
  A11Y sub-milestone. M4.10's XSS-safe DOM API,
  no-network discipline, asymmetric one-inline-script
  invariant, M4.12 `.selected` surface, and the M4.8 +
  M4.13 five-attr DOM contract all carry over unchanged.
  **M4 remains in progress** — no exit report. **Save
  format stays v12** — `tabindex` is render-time only.
  `provinces.svg` bytes DID change (additive only);
  `map.html` bytes did change. 9 new doctest cases (866
  total).

- **M4.16** makes M4.15's **keyboard focus visible**.
  Pure CSS: four new rules in the M4.6 `<style>` block —
  `svg circle:focus { outline: none; }`,
  `svg circle:focus-visible { outline: none; stroke:
  #1976d2; stroke-width: 4; }`,
  `svg text:focus { outline: none; }`,
  `svg text:focus-visible { outline: 2px solid #1976d2;
  outline-offset: 2px; }`. Uses `:focus-visible` (NOT
  bare `:focus`) so mouse-clicks don't trip the ring,
  keeping the M4.12 black `.selected` stroke visually
  distinct from the M4.16 blue keyboard-focus indicator.
  Bare `:focus { outline: none; }` neutralises the
  browser's default focus outline. **Still NO ARIA
  polish** — that lands in a future dedicated A11Y
  sub-milestone. **M4 remains in progress.** **Artefact
  set unchanged (still 10); save format unchanged (still
  v12).** `provinces.svg` bytes UNCHANGED from M4.15
  (focus CSS is HTML-only); `map.html` bytes did change.
  5 new doctest cases (871 total). **No JS change, no
  ARIA, no tooltip, no hover, no animation, no schema
  bump.**

- **M4.17** adds the **screen-reader name** for the
  M4.15-focusable / M4.10-clickable province markers.
  Every `<circle>` and `<text>` carries `role="button"`
  + `aria-label="<name>, <owner_name>"` (fallback
  `<name>` alone when owner is invalid). Composed at
  render time, XML-attribute-escaped as a single value;
  same single bounds check as `data-owner-code` /
  `data-owner-name` gates the fallback. Lives in
  `render_svg_root` so `provinces.svg` picks it up too.
  Legend swatch circles stay decorative.
  **Narrowly reverses the M4.15/M4.16 "no ARIA"
  non-goal** — only `role="button"` + `aria-label` ship;
  the broader ARIA surface (`aria-selected`,
  `aria-current`, `aria-pressed`, `aria-live`,
  `aria-describedby`, `aria-labelledby`) stays deferred
  to a future dedicated A11Y sub-milestone. The
  M4.15/M4.16 unit tests retune from over-broad
  `role=` / `aria-label=` absence checks to narrower-
  ARIA-surface absence checks. **M4 remains in
  progress.** **Save format stays v12** — aria-label is
  composed from existing fields, not new state. 7 new
  doctest cases (878 total). **No state mutation, no
  schema bump, no commands, no events, no tooltip, no
  hover, no animation, no selection persistence.**

Per the M-pacing rule the next sub-milestone (M4.18) waits for
an explicit reviewer green-light. Candidates: broader ARIA
polish (`aria-selected` on the M4.12 `.selected` markers,
`aria-live` region on the details panel for click-update
announcements), hover state / tooltips reusing the same
XSS-safe DOM API, selection persistence on the
clicked node, neighbour adjacency edges, `<meta
name="viewport">` + media queries for responsive sizing.

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

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

## Reading order

If you're new to the codebase:

1. Start with the top-level `README.md` for current status, build,
   and test instructions.
2. Skim `rfc/README.md` and the RFC documents it indexes for the
   high-level design intent.
3. Read the milestone notes here **in order** (M0.2 → M0.10 → M1.1
   → M1.2 → M1.3 → M1.4 → M1.5 → M1.6 → M1.7). They build on each
   other and each one tries to call out the rules a future
   contributor must not silently break.

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

# M2.10 - State comparison API

Companion notes for `feature/m2-10-state-comparison`. M2.10 introduces
`diagnostics::compare_states`: a free function that walks two
`GameState`s field-by-field and returns a list of `StateMismatch`
entries. Empty list = states match. This is the programmatic answer
to the question M2.8's CLI deliberately deferred: *did this replay
produce the same state as the source?*

## 1. Scope

M2.8 ships `--replay PATH`, which writes the replayed state to disk
and lets the user `diff` source vs target save files. That works,
but a save-byte diff:

- catches everything, including logs / rng counter / boilerplate
  that almost always differs across paths,
- gives no structured answer (a 200-line diff doesn't tell you
  which gameplay field actually diverged),
- can't be checked programmatically by a test or by a future
  `--verify` CLI flag.

M2.10 ships the structured answer. One free function, one struct
for the result, one options struct for tolerance. No CLI integration
yet (that's a future sub-milestone candidate).

## 2. Public API

`include/leviathan/systems/diagnostics.hpp`:

```cpp
struct StateMismatch {
    std::string field_path;   // e.g. "countries[0].budget.military"
    std::string detail;       // e.g. "0.4 != 0.5 (tolerance 1e-9)"
};

struct CompareOptions {
    double double_tolerance = 1e-9;
};

std::vector<StateMismatch> compare_states(
    const core::GameState& a,
    const core::GameState& b,
    const CompareOptions& opts = {});
```

`field_path` mirrors the M2.4 / M0.8 save JSON addressing convention
(`countries[0].budget.military`, `applied_commands[1].command.budget_delta`,
etc.) so the same path string is usable from CLI output, error
messages, or test assertions.

Empty result = states match (modulo the deliberately-skipped fields,
see §4). Non-empty = ordered mismatches.

## 3. What's compared

Walked in canonical order:

1. `current_date` — strict `GameDate::operator==`.
2. `player_country` — strict int comparison on `CountryId::value()`.
3. **`countries.size()` first**, then for each country:
   - `id_code`, `name`, `display_name` (strict string).
   - All 13 numeric fields (`gdp`, `tax_revenue`, `budget_balance`,
     `legal_tax_burden`, `fiscal_capacity`,
     `administrative_efficiency`, `central_control`, `corruption`,
     `stability`, `legitimacy`, `military_power`,
     `threat_perception`, `last_gdp_growth_rate`) with tolerance.
   - All 7 budget categories with tolerance.
   - `active_policies.size()` first, then per-entry
     `policy_id_code` (strict) and `expires_on` (strict GameDate).
4. **`factions.size()` first**, then for each faction:
   - `id_code`, `country_id_code`, `type` (strict string).
   - All 5 numeric fields (`support`, `influence`, `radicalism`,
     `loyalty`, `resources`) with tolerance.
   - `preferred_policies.size()` (strict int).
5. **`applied_commands.size()` first**, then for each entry:
   - `applied_on` (strict GameDate).
   - `command.kind` (strict int via enum cast).
   - `command.policy_id_code`, `command.budget_category` (strict
     string).
   - `command.budget_delta` (with tolerance).

The size-first pattern means a size mismatch produces exactly one
entry (`...size()`) instead of N per-element entries — the test for
"different country count" pins this.

## 4. What's deliberately skipped

Decisions are documented in the header so a future contributor sees
them up front:

- **`rng`**: M2 replay does not yet reach into RNG. A future
  divergent-RNG comparison would want its own helper.
- **`logs`**: begin_tick / end_tick / month-rolled-over entries
  produce boilerplate that almost always differs across paths.
  Replay equivalence cares about end-state, not log breadcrumbs.
- **`policies`**: immutable templates loaded from disk. If they
  differ between two states, the scenario fixtures differ — that's
  a config bug, not a gameplay divergence.
- **`provinces`, `events`**: still reserved-empty in M2.
- **`simulation_config`**: not part of `GameState`.

If a future use case needs to compare any of these, the right move
is a sibling helper (`compare_logs`, `compare_rng`, etc.), not
expanding `compare_states`'s scope.

## 5. Tolerance semantics

`approx_equal(a, b, tol)` uses:

```cpp
if (a == b) return true;                       // catches exact + same-sign Inf
if (!isfinite(a) || !isfinite(b)) return false;// one NaN/Inf, one finite -> diff
return std::abs(a - b) <= tol;
```

So:

- Exact equality is fast-pathed (covers `0.0 == 0.0` without
  arithmetic).
- Two NaN values report as different (NaN != NaN by IEEE).
- Two same-sign Inf values match.
- Otherwise: standard absolute tolerance.

Default `double_tolerance = 1e-9` matches M0.8's save round-trip
precision. Calls that need looser tolerance (cumulative drift in
long simulations) pass a custom `CompareOptions`.

## 6. Tests

12 new doctest cases (M2.8 was 520 → M2.10 is 532):

- Two empty `GameState`s match (0 mismatches).
- Two identical seeded states match.
- Different `current_date` reports 1 mismatch with path
  `current_date` and both date strings in the detail.
- Different `player_country` reports 1 mismatch with both values.
- Different country count reports `countries.size()` mismatch.
- `gdp` diff on country[0] produces path `countries[0].gdp`.
- Tolerance: `1e-12` diff is silent at default `1e-9` tolerance.
- Tolerance: `1e-6` diff is reported at default `1e-9` tolerance.
- `active_policies` size mismatch on country[0] caught with the
  array path.
- `applied_commands` size mismatch caught.
- Multiple mismatches collected in canonical order
  (`current_date`, `player_country`, `countries[0].gdp`).
- Custom `CompareOptions::double_tolerance = 1e-2` accepts a `1e-3`
  diff that the default tolerance would reject.

## 7. Use cases

The function is library-only in M2.10. Anticipated consumers:

- **Replay equivalence tests**: an integration test can run a
  simulation, save it, then run `--replay` on a fresh state,
  load both saves, and assert `compare_states(...).empty()`.
- **Future `--verify` CLI flag** (M2.11 candidate): the runner's
  `--replay` flow auto-runs `compare_states` and reports
  mismatches to stdout.
- **Future "did the save round-trip cleanly?" tests** beyond what
  M0.11 / M1.17 already cover at the byte level.

## 8. What's NOT in scope

- **No CLI integration.** No `--verify` flag yet. The function is
  a library primitive callers wire up themselves.
- **No log / rng / policy comparison.** See §4 for rationale.
- **No relative-tolerance option.** Just absolute. The state's
  numeric ranges (mostly `[0, 1]` ratios + GDP in low hundreds)
  make absolute tolerance adequate.
- **No mismatch-budget cap.** If 1000 fields differ, all 1000 are
  reported. Caller truncates if needed.
- **No save format change.**
- **No M1 system change.**
- **No new `state.logs` entry.**

## 9. Cross-links

- M0.10 (`m0-10-diagnostics.md`) — `sanity_check` is the other
  observation-only entry point in this namespace; same pattern.
- M0.8 (`m0-8-save-load.md`) — the save JSON shape that
  `field_path` mirrors.
- M2.4 (`m2-4-command-log.md`) — `applied_commands` shape.
- M2.5 (`m2-5-adjust-budget.md`) — `AdjustBudget` payload shape.
- M2.7 (`m2-7-replay-with-time.md`) — `replay_with_time`
  primitive; `compare_states` is the natural verification
  companion.
- M2.8 (`m2-8-replay-cli.md`) — `--replay` CLI; a future
  `--verify` flag would call `compare_states`.

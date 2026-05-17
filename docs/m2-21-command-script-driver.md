# M2.21 - Command script driver helper

Companion notes for `feature/m2-21-command-script-driver`. M2.21
adds a single library-only convenience function on top of the
M2.20 `try_apply_pending` surface, for callers that hand the
command system a one-shot **script** (a `std::vector<PlayerCommand>`)
rather than maintaining a long-lived `CommandQueue`.

The function is a thin wrapper. It does not introduce new types,
not change `try_apply_pending` / `apply_pending` semantics, not
touch the runner, and not change save schema. Its only job is to
make the "try this list of commands, tell me what landed" use case
obvious from the public API.

## 1. Scope

After M2.20 a caller that wants structured rejection from a one-
shot batch of commands still has to:

1. Allocate a local `CommandQueue`.
2. Copy the script into `q.pending`.
3. Call `try_apply_pending(state, q)`.

That's three lines of boilerplate every test, REPL, or future
agent driver will repeat. M2.21 collapses it to one call:

```cpp
auto r = commands::apply_command_script(state, script);
```

with the exact same outcome shape (`ApplyWithReportOutcome`) and
the exact same routing rules (rejection ⇒ success + record;
non-execution error ⇒ failure).

The function is **library-only**. No runner option, no CLI flag,
no `RunOutcome` change, no `main()` change, no replay change.
Wiring `try_apply_pending` (or this helper) into the runner is
deferred to a later sub-milestone when the runner actually has a
script-input entry point to plug it into.

## 2. Public API

`include/leviathan/systems/commands.hpp`:

```cpp
namespace leviathan::systems::commands {

core::Result<ApplyWithReportOutcome> apply_command_script(
    core::GameState& state,
    const std::vector<core::PlayerCommand>& script);

}  // namespace
```

### Semantics (inherited from M2.20 `try_apply_pending`)

- **Empty `script`** → `Result::success` with
  `apply.commands_applied == 0`, `rejection == std::nullopt`.
  Nothing is appended to `state.applied_commands`.
- **Full drain** → `Result::success` with
  `apply.commands_applied == script.size()`, `rejection ==
  std::nullopt`. Every command is appended to
  `state.applied_commands` in script order.
- **Order-execution gate rejection** (M2.18 EnactPolicy or M2.19
  AdjustBudget) → `Result::success` with `apply.commands_applied`
  counting the strictly prior successes and `rejection`
  populated for the first rejected command. The rejected command
  and any commands behind it in the script are **not** surfaced
  through the return value (see §5).
- **Non-execution failure** (precondition / NaN `budget_delta` /
  unknown policy id_code / unknown budget_category / policy-
  effect resolution failure) → `Result::failure` with the same
  error shape `try_apply_pending` produces.

### Invariants

- The input `script` vector is NOT mutated. The helper builds a
  local `CommandQueue` whose internal vector copies `script`'s
  contents, and `try_apply_pending` pops from that local queue.
- `state` mutation semantics are entirely inherited from the
  underlying per-command dispatch — only successful commands
  mutate state and append to `applied_commands`. Rejection /
  failure paths leave state byte-identical for the affected
  command (prior successes in the script stay applied).
- Determinism: identical `(state, script)` inputs produce
  identical results (no RNG / no time advancement inside the
  helper).

## 3. Implementation

`src/leviathan/systems/commands.cpp`:

```cpp
core::Result<ApplyWithReportOutcome> apply_command_script(
        core::GameState& state,
        const std::vector<core::PlayerCommand>& script) {
    CommandQueue q;
    q.pending = script;
    return try_apply_pending(state, q);
}
```

Three lines including the local queue. The whole point of M2.21
is the *interface* (one obvious call site for the scripted
use case), not new behaviour.

## 4. Tests

8 new doctest cases in `tests/systems/commands_test.cpp`
(M2.20 closed at 616 → M2.21 lands at 624):

1. Empty script → success, `commands_applied == 0`,
   no rejection, `applied_commands` empty.
2. Full success script (`EnactPolicy("raise_taxes")` +
   `AdjustBudget("military", +0.02)`) → both effects land,
   both appended to `applied_commands` in script order.
3. EnactPolicy rejected at compliance 0.1 → success with
   `RejectionRecord{kind=EnactPolicy, policy_id_code,
   compliance=0.1, threshold=0.3}`, `apply.commands_applied
   == 0`, `applied_commands` empty.
4. AdjustBudget(military) rejected at military_loyalty 0.05
   → rejection records military_loyalty as `compliance`
   (selected-input contract from M2.19).
5. Mid-script rejection: `[AdjustBudget(military, mil=0.9),
   EnactPolicy(bureau=0.05), AdjustBudget(welfare)]` →
   success, `apply.commands_applied == 1`, rejection on the
   EnactPolicy entry. The military adjustment landed and
   logged; the trailing welfare adjustment did NOT (helper
   does not surface remaining commands).
6. Unknown policy id_code → `Result::failure` with "unknown
   policy id_code" in the error.
7. Invalid `player_country` → `Result::failure` naming
   `player_country`.
8. Input vector is not mutated: capture the script before the
   call, run it through a full-success script, verify the
   vector survives unchanged element-by-element.

## 5. What's NOT in scope

Deliberate non-goals:

- **No remaining-queue surface.** The rejected command and any
  trailing commands in the script are not surfaced through the
  return value. Callers that need the tail should build a
  `CommandQueue` directly and call `try_apply_pending` — that
  surface already exists (M2.20).
- **No runner change.** `runner::run` / `replay_with_time` /
  `RunOutcome` / `main()` are byte-identical.
- **No CLI flag.** There's no `--script PATH` or similar in
  M2.21.
- **No persistent attempted-command log.** The structured
  rejection lives only in the in-process return value; nothing
  is written to `state.applied_commands`, no save schema change,
  no separate `state.rejected_commands` runtime container, no
  CSV.
- **No new `state.logs` entry on rejection.**
- **No save format change.** Still v10.
- **No DataLoader / policy effect / replay primitive / runner /
  CLI / M1 system change.**
- **No new `PlayerCommandKind`, no new CSV column.**
- **No threshold / formula change to M2.18 / M2.19 gates.**
- **No new struct.** M2.20's `ApplyWithReportOutcome` is the
  return type; we don't ship a parallel `CommandScriptOutcome`.

## 6. Cross-links

- M2.20 (`m2-20-command-rejection-reporting.md`) — the
  `try_apply_pending` surface this helper wraps.
- M2.19 (`m2-19-adjust-budget-execution-gate.md`) — the
  category-aware gate that supplies the rejection record's
  `compliance` value for AdjustBudget commands.
- M2.18 (`m2-18-enact-policy-execution-gate.md`) — the
  EnactPolicy gate that supplies the record for EnactPolicy.
- M2.3 (`m2-3-command-queue.md`) — mid-list-failure atomicity
  rule the helper inherits.
- RFC-020 §2 — execution resistance catalogue;
  `apply_command_script` is the first ergonomic entry point for
  presenting batch-script rejections to a future UI / REPL /
  agent driver.

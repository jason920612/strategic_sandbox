# M2.17 - OrderExecutionSystem skeleton

Companion notes for `feature/m2-17-order-execution-skeleton`. M2.17
introduces the first M2 system that reads the M2.16
`government_authority` block. Deliberately minimal: this PR ships
the shape of a future order-execution evaluator without changing
any existing behaviour. No caller wires the function in yet;
`commands::apply_pending` continues to act exactly as in M2.5.

## 1. Scope

RFC-020 §2-§3 sketch a future where player commands are not
guaranteed to succeed — the state machine, faction reaction, and
authority levels all gate execution. M2.17 lays down a stable seam
for that future: a new module with the types and the entry function
in their final shape. The behaviour stops at "always accept", so
no current call site can become subtly dependent on the placeholder
before the real formula lands.

The piece reserved for a future PR (M2.18+ candidate): a
`bureaucratic_compliance`-gated formula returning `Rejected /
Delayed / Distorted` statuses and wiring `commands::apply_pending`
through `evaluate()`.

## 2. Public API

`include/leviathan/systems/order_execution.hpp`:

```cpp
namespace leviathan::systems::order_execution {

struct OrderExecutionInputs {
    double bureaucratic_compliance  = 0.5;
    double military_loyalty         = 0.5;
    double intelligence_capability  = 0.5;
    double media_control            = 0.5;
};

enum class ExecutionStatus {
    Accepted,
    // Future: Rejected, Delayed, Distorted
};

struct OrderExecutionOutcome {
    ExecutionStatus      status = ExecutionStatus::Accepted;
    OrderExecutionInputs inputs = {};
};

core::Result<OrderExecutionOutcome> evaluate(
    const core::GameState& state,
    const core::PlayerCommand& command);

}  // namespace
```

### `OrderExecutionInputs`

A 4-field snapshot of the actor country's `government_authority`
block. Captured into the outcome so callers can diagnose how a
future formula would have read its inputs even though the skeleton
itself does nothing with them. Defaults match
`GovernmentAuthorityState` (0.5 across the board).

### `ExecutionStatus`

A coarse-grained outcome enum. M2.17 produces only `Accepted`. The
header reserves three future variants by name:

- `Rejected` — execution failed outright; no state mutation.
- `Delayed` — execution accepted but spread across multiple days
  (the future scheduler decides the timing).
- `Distorted` — execution accepted but with modified effects (e.g.
  partial application, redirected to a different actor).

The enum is shipped now with a single variant so future callers can
already `switch` on it without an API break.

### `evaluate(state, command)`

Pure read of `state` and `command`.

**Preconditions** (mirror `commands::apply_pending`):

- `state.player_country` must be valid (>= 0).
- `state.player_country` must index into `state.countries`.

Either violation returns `Result::failure` whose message names
`order_execution::evaluate` and the offending state shape.

**On success**:

- Snapshots the actor country's `government_authority` into the
  returned `OrderExecutionInputs`.
- Returns `ExecutionStatus::Accepted`.
- Leaves `state` and `command` byte-identical.

The function reads `command` only to honour the future API shape
— M2.17 produces the same outcome for every kind. M2.18+ will
branch on `command.kind` and command-specific resistance terms.

## 3. Why `resistance` is NOT in the outcome

A single `resistance = 0.0` field would look harmless but pretends
the formula shape is already decided. Two concrete risks:

1. **Premature semantics.** Downstream code might start treating
   `outcome.resistance == 0.0` as "no resistance computed yet",
   while later the same field means "resistance computed and the
   answer is zero" — those are different signals that should not
   share a single representation.
2. **Cementing a numeric type.** A future formula may want a
   tuple (per-faction resistance, per-target resistance) instead of
   a single double; or a finer enum tier. Reserving the name now
   constrains the future shape unnecessarily.

The user-facing decision was therefore: ship the inputs snapshot
+ the status enum, defer the resistance scalar to the same PR that
introduces the formula.

## 4. Why this PR doesn't wire into `apply_pending`

`commands::apply_pending` ran for two M2 sub-milestones (M2.3 +
M2.4) before any execution-resistance concept existed. Wiring
`evaluate` into the dispatch path in the same PR that introduces
`evaluate` itself would:

- Couple the skeleton with the formula PR (M2.18+) because the
  wired path needs SOMETHING to gate on, and the only thing the
  skeleton has is `Accepted`.
- Force an immediate decision about logging / outcome surfacing.

So M2.17 ships the API surface in isolation. M2.18 will (a)
introduce the gating formula, (b) wire it into `apply_pending`,
(c) decide how rejected commands surface in the applied-commands
log, and (d) refresh diagnostics if needed. Each of those is its
own scope.

## 5. Tests

10 new doctest cases under `tests/systems/order_execution_test.cpp`
(M2.16 closed at 575 → M2.17 lands at 585):

`Preconditions` (3):

- No player country selected → failure with `player_country` in
  the error.
- player_country out of range → failure.
- Empty countries with any selection → failure.

`Success path` (3):

- Valid selection returns `Accepted`.
- `inputs` mirror the actor country's `government_authority`
  fields one-for-one.
- `evaluate` reads the *selected* country (index 1), not always
  `countries[0]`.

`Non-mutation + determinism + kind independence` (3):

- `state` is byte-identical after `evaluate` returns.
- `EnactPolicy` and `AdjustBudget` produce identical outcomes
  (the skeleton ignores `command.kind`).
- Repeated calls produce identical outcomes (deterministic).

`Default construction` (1):

- A default-constructed `OrderExecutionOutcome` has
  `status == Accepted` and all four `inputs` ratios at 0.5.

CMake wires the new module into `leviathan_systems` and the new
test file into `leviathan_tests`. Each `TEST_CASE` is registered
with CTest individually so `ctest -R "evaluate|OrderExecution"`
runs just the M2.17 set.

## 6. What's NOT in scope

Deliberate non-goals:

- **No `resistance` field.** Deferred to the same PR that
  introduces the formula.
- **No `Rejected` / `Delayed` / `Distorted` outcomes.** Variants
  reserved by name in the enum; behaviour deferred.
- **No `commands::apply_pending` change.** The player command path
  is byte-identical with M2.5 / M2.16.
- **No formula over `inputs`.** The skeleton just snapshots.
- **No new `PlayerCommandKind`.** `command` is taken by const ref
  for future-proofing but its `.kind` and payload are ignored.
- **No new state mutation.** `evaluate` is a pure read.
- **No save format change.** Schema stays v10.
- **No DataLoader change.**
- **No policy effect change.**
- **No new `state.logs` entry.**
- **No AI / events / UI / scheduler.**
- **No replay change.** `commands::replay_with_time` doesn't call
  `evaluate`.
- **No CSV column** for `inputs`. Adding a diagnostics surface
  would be premature before the formula lands.

## 7. Cross-links

- M2.16 (`m2-16-government-authority-state.md`) — the
  `GovernmentAuthorityState` block this skeleton snapshots.
- M2.3 (`m2-3-command-queue.md`) — the precondition shape this
  function mirrors (`apply_pending`).
- RFC-020 §2-§3 — the future-state list this skeleton makes
  representable.
- M1.5 (`m1-5-policy-system.md`) — precedent for a system landing
  before any runner wiring.

# M2.18 - EnactPolicy execution gate

Companion notes for `feature/m2-18-enact-policy-execution-gate`.
M2.18 is the first M2 sub-milestone where a player command can be
**rejected**: an `EnactPolicy` whose actor country has
`bureaucratic_compliance < 0.3` short-circuits inside
`commands::apply_pending` without touching state, the queue head,
or the applied-commands log.

This is the smallest possible step from M2.17's "always Accepted"
skeleton: one new enum variant, one new outcome field, one
constant threshold, one switch arm in `evaluate`, and one
early-return guard in `apply_pending`'s `EnactPolicy` branch.
`AdjustBudget` and every other future kind stay unconditionally
accepted until they get their own gate PR.

## 1. Scope

RFC-020 §2 lists a dozen ways a policy can fail to execute
(bureaucratic delay, local resistance, military insubordination,
…). M2.18 picks the single most direct one — bureaucratic
compliance — and ships its hard yes/no form. The threshold is a
**constant**, not configurable: the M2.18 system has no scenario
hook, no policy-specific tuning, no per-target weight. Those land
when the gate has demonstrated value.

### Stripped formula

```cpp
inline constexpr double kEnactPolicyComplianceThreshold = 0.3;

// In evaluate(), for PlayerCommandKind::EnactPolicy only:
const double compliance = inputs.bureaucratic_compliance;
outcome.resistance = 1.0 - compliance;
outcome.status =
    (compliance >= kEnactPolicyComplianceThreshold)
        ? ExecutionStatus::Accepted
        : ExecutionStatus::Rejected;
```

### Threshold rationale (0.3)

- Canonical scenarios load with `government_authority` defaulting
  to `0.5` across the board (M2.16). At `0.3`, every existing
  scenario fixture passes the gate unchanged — no test breakage,
  no scenario regression.
- A `0.5` threshold would put canonical defaults exactly on the
  boundary and would feel arbitrarily strict for "average"
  countries authoring a mid-range compliance value (0.4, 0.45).
- A `0.3` floor turns the gate into "low-compliance authoritarian
  / collapsing regimes visibly stall", which is the gameplay
  signal RFC-020 §2 calls out.

## 2. Public API changes

### `order_execution.hpp`

- New constant `kEnactPolicyComplianceThreshold = 0.3`.
- `ExecutionStatus` gains the `Rejected` variant. `Delayed` and
  `Distorted` stay reserved by name in the enum comment.
- `OrderExecutionOutcome` gains `double resistance = 0.0`.
- `evaluate()`'s post-snapshot section now switches on
  `command.kind`. Only `EnactPolicy` evaluates the gate; every
  other kind keeps the M2.17 behaviour (`Accepted`, resistance
  `0.0`).

### `commands::apply_pending` (M2.5+)

Inside the `EnactPolicy` switch arm, before the M2.3 policy
lookup, `apply_pending` now calls `order_execution::evaluate`:

```cpp
auto eval_r = oe::evaluate(state, cmd);
if (!eval_r) {
    return Result::failure(... + ": order_execution::evaluate failed: " + ...);
}
if (eval_r.value().status == oe::ExecutionStatus::Rejected) {
    return Result::failure(
        ctx + ": EnactPolicy '" + cmd.policy_id_code +
        "' rejected by order_execution gate"
        " (bureaucratic_compliance=... < threshold=0.3)");
}
// existing policy lookup + apply_policy_effects path follows.
```

Failure semantics match the M2.3 mid-list-failure rule:

- No `state` mutation (policy is never looked up, never applied).
- The rejected command stays at the head of the queue.
- `state.applied_commands` is **not** appended.
- Previously-applied commands in the same drain stay applied
  and stay logged.

`AdjustBudget` is unaffected.

## 3. What's NOT in scope

Deliberate non-goals:

- **No `AdjustBudget` gate.** The switch arm in `evaluate` keeps
  `AdjustBudget` at `Accepted` with `resistance = 0.0`. A future
  PR will pick the right authority term (probably
  `bureaucratic_compliance` again, possibly with
  `corruption` / `administrative_efficiency` factored in) and
  ship its own threshold.
- **No multi-input / weighted formula.** A composite over
  `bureaucratic_compliance × military_loyalty × …` would freeze
  weights without gameplay evidence to back them.
- **No probabilistic gate.** RNG is intentionally absent: M2.18
  reads pure data and produces a pure yes/no.
- **No `Delayed` / `Distorted` outcomes.** Enum variants stay
  reserved by name only.
- **No scheduler / pending-execution queue.** Rejected commands
  do not auto-retry.
- **No `state.logs` entry on rejection.** The runner / driver
  surfaces the error from `apply_pending`'s `Result::failure`;
  no system writes a "command rejected" log line.
- **No `RunOutcome` field counting rejected commands.**
- **No save format change.** Still v10.
- **No DataLoader change.** Countries still default to `0.5`
  authority when the block is omitted.
- **No policy effect change.** PolicySystem is untouched.
- **No replay change.** `commands::replay_with_time` calls
  `apply_pending` per entry just as before; the gate fires
  inside that call. A replay of a save built under
  default-`0.5` authority therefore still succeeds because
  every command in the log was originally Accepted.
- **No new `PlayerCommandKind`.**
- **No new CSV column** for resistance.
- **No M1 system change.** M1 monthly pipeline behaviour is
  byte-identical.

## 4. Tests

10 new doctest cases (M2.17 closed at 585 → M2.18 lands at 595).

`tests/systems/order_execution_test.cpp` (6):

- `EnactPolicy` at compliance `0.3` → `Accepted`, `resistance` 0.7.
- `EnactPolicy` at compliance `0.299` → `Rejected`,
  `resistance` ≈ 0.701.
- `EnactPolicy` resistance equals `1.0 - bureaucratic_compliance`
  across `{0.0, 0.1, 0.5, 0.75, 1.0}`.
- `EnactPolicy` at default `0.5` still accepts (regression for
  scenario-loaded countries).
- `AdjustBudget` at compliance `0.01` still `Accepted` with
  `resistance == 0.0`.
- Rejected `EnactPolicy` path is non-mutating (the
  `government_authority` block, `logs.size()`,
  `applied_commands.size()` all stay byte-identical).

Plus updates to two M2.17 tests:

- "default OrderExecutionOutcome" now also pins `resistance == 0.0`.
- The "EnactPolicy and AdjustBudget produce identical outcomes"
  test split conceptually: same status + same inputs (still true),
  but resistance differs (now checked).

`tests/systems/commands_test.cpp` (4):

- `apply_pending` `EnactPolicy` is accepted at default 0.5
  compliance — full success path (queue drained, applied_commands
  appended).
- `apply_pending` `EnactPolicy` rejected when compliance < 0.3 —
  failure result, error mentions `order_execution` + `rejected`
  + the policy id_code, no state mutation, queue head intact,
  no `applied_commands` entry.
- `apply_pending` rejected `EnactPolicy` stops a mid-list queue —
  preceding `AdjustBudget` lands and logs; rejected `EnactPolicy`
  stays at head; trailing `EnactPolicy` stays queued.
- `apply_pending` `AdjustBudget` unaffected by very low
  compliance (regression for the explicit non-goal).

## 5. Replay compatibility

`commands::replay_with_time` (M2.7) walks `applied_commands`
entries and re-invokes `apply_pending` for each. M2.18 is
backwards compatible: every entry in a v10 save was originally
Accepted, so re-running `apply_pending` against the same actor
country (whose `government_authority` round-trips byte-identical)
re-Accepts in the same order. No replay regression for saves
written before or after M2.18.

A save authored by hand with low compliance AND a recorded
EnactPolicy entry would now reject on replay — but that's the
intended behaviour: the log claims the player issued the command,
the gate says "no, that wouldn't have gone through". Future PRs
that decide to record rejected attempts separately would do so by
adding a parallel structure rather than overloading
`applied_commands`.

## 6. Cross-links

- M2.17 (`m2-17-order-execution-skeleton.md`) — the skeleton this
  PR turns into a real gate.
- M2.16 (`m2-16-government-authority-state.md`) — supplies
  `bureaucratic_compliance` to the gate.
- M2.3 (`m2-3-command-queue.md`) — `apply_pending`'s mid-list
  failure semantics that M2.18 piggybacks on.
- M2.4 (`m2-4-command-log.md`) — per-command atomicity for the
  applied_commands log. M2.18 inherits the rule that failed
  commands do not log.
- M2.7 (`m2-7-replay-with-time.md`) — replay path that calls
  `apply_pending`. M2.18 needs to preserve replay determinism for
  default-0.5 saves.
- RFC-020 §2 — the full list of execution-resistance categories
  M2.18 ships one of.

# M2.19 - AdjustBudget execution gate

Companion notes for `feature/m2-19-adjust-budget-execution-gate`.
M2.19 extends the M2.18 command-gating shape to
`PlayerCommandKind::AdjustBudget` with a single category-aware
twist: the `"military"` budget category gates on
`military_loyalty`, every other category still gates on
`bureaucratic_compliance`.

## 1. Scope

M2.18 introduced rejection by gating `EnactPolicy` on a single
authority input. M2.19 keeps that shape (one input, one constant
threshold, deterministic yes/no) but selects the input by
`command.budget_category`:

```cpp
const double selected =
    (command.budget_category == "military")
        ? inputs.military_loyalty
        : inputs.bureaucratic_compliance;
outcome.resistance = 1.0 - selected;
outcome.status =
    (selected >= kAdjustBudgetComplianceThreshold)
        ? ExecutionStatus::Accepted
        : ExecutionStatus::Rejected;
```

The single category branch captures the gameplay intuition that
the armed forces resist a low-loyalty regime trying to reshape
their own budget, while the other six categories
(`administration`, `education`, `welfare`, `intelligence`,
`infrastructure`, `industry`) still route through the bureaucracy
exactly like `EnactPolicy`. Adding bespoke per-category inputs
(e.g. `"intelligence"` ⇒ `intelligence_capability`,
`"welfare"` ⇒ a future `welfare_capacity` field) waits for its own
PR with a real consumer.

### Threshold rationale

`kAdjustBudgetComplianceThreshold = 0.3` matches the M2.18
`EnactPolicy` value. Reasons:

- Canonical scenarios load with every `government_authority`
  field at `0.5` (M2.16 DataLoader default). 0.3 keeps every
  existing fixture / test in the `Accepted` lane — no
  regression.
- A different value (e.g. 0.2 for "budget is easier than legal
  change") would be premature tuning: M2.18 has been live for one
  PR. Keep both knobs at 0.3 until gameplay signals which should
  diverge.

## 2. Public API changes

`order_execution.hpp`:

- New constant `kAdjustBudgetComplianceThreshold = 0.3` next to
  `kEnactPolicyComplianceThreshold`.
- Top-of-file doc + the per-kind paragraph on
  `OrderExecutionOutcome` updated to spell out the M2.19 formula
  and which category routes through which input.
- `ExecutionStatus`, `OrderExecutionOutcome`, and the
  `evaluate(state, command)` signature are **unchanged**. M2.19
  doesn't grow the type surface; it adds behaviour inside an
  existing switch arm.

`order_execution.cpp` — `evaluate`:

The previous M2.17 placeholder ("no gate evaluated, resistance
stays 0.0") inside the `AdjustBudget` arm is replaced with the
category-aware compute above.

`commands::apply_pending` — `AdjustBudget` arm:

A new pre-flight block, structurally identical to the M2.18
`EnactPolicy` block, runs **before** the existing finite-delta +
category-whitelist checks. On `Rejected` it returns
`Result::failure` whose error names `order_execution`,
`rejected`, `AdjustBudget`, the offending `budget_category`, the
selected compliance value, and the threshold:

```
commands::apply_pending[3]: AdjustBudget category 'military'
rejected by order_execution gate (compliance=0.1 < threshold=0.3)
```

Failure semantics match M2.18 / M2.3:

- No `state` mutation (budget pointer never touched).
- No queue pop.
- No `state.applied_commands` append.
- Previously-applied commands in the same drain stay applied
  and stay logged.

## 3. What's NOT in scope

Deliberate non-goals:

- **No `Delayed` / `Distorted` outcomes.** Same M2.18 stance.
- **No weighted multi-input formula.** Each (kind, category)
  pair still reads exactly one authority input.
- **No bespoke per-category inputs beyond `"military"` ⇒
  `military_loyalty`.** `"intelligence"`, `"welfare"`, etc.
  still flow through `bureaucratic_compliance`. Authority-input
  routing for those categories is a future PR.
- **No probabilistic / RNG gate.**
- **No scheduler / pending-execution queue.**
- **No `state.logs` entry on rejection.**
- **No `RunOutcome` field counting rejected commands.**
- **No save format change.** Still v10.
- **No DataLoader change.**
- **No new `PlayerCommandKind`.**
- **No new CSV column.**
- **No M1 system change.**

## 4. Tests

11 new doctest cases (M2.18 closed at 595 → M2.19 lands at 606).

`tests/systems/order_execution_test.cpp` (7):

- `AdjustBudget(military)` at `military_loyalty == 0.3` accepts;
  resistance 0.7.
- `AdjustBudget(military)` at `military_loyalty == 0.299` rejects;
  resistance ≈ 0.701.
- `AdjustBudget(military)` ignores `bureaucratic_compliance ==
  1.0` when `military_loyalty == 0.1` — still rejects.
- `AdjustBudget(non-military)` rejects when
  `bureaucratic_compliance == 0.2` even with `military_loyalty ==
  0.9`. Iterates `administration / education / welfare /
  intelligence / infrastructure / industry` so a future routing
  change can't silently break the contract for any of them.
- `AdjustBudget(non-military)` (welfare) at
  `bureaucratic_compliance == 0.9` accepts even with
  `military_loyalty == 0.01`.
- `AdjustBudget` with every authority field at the default `0.5`
  accepts for both `military` and a non-military category.
- `AdjustBudget` rejected path leaves the country
  `government_authority`, `logs.size()`, and
  `applied_commands.size()` byte-identical.

Plus the two M2.18 tests it inherited got refreshed in place:

- "EnactPolicy and AdjustBudget snapshot the same inputs but
  compute different resistance" — assertion on
  `adjust.resistance` updated from `0.0` to `1.0 - 0.84` (now
  pinned to `military_loyalty`).
- "evaluate AdjustBudget: bypasses the gate even at very low
  compliance" renamed to "low bureaucratic_compliance is
  irrelevant if military_loyalty is high" with the assertion
  flipped to reflect the M2.19 routing.

`tests/systems/commands_test.cpp` (4):

- `AdjustBudget(military)` rejected when `military_loyalty <
  0.3` — error names `order_execution` + `rejected` +
  `AdjustBudget` + `military`; state unchanged; queue head
  intact; `applied_commands` untouched.
- `AdjustBudget(welfare)` rejected when
  `bureaucratic_compliance < 0.3` even when `military_loyalty`
  is `0.95` — confirms non-military category does not benefit
  from military loyalty.
- `AdjustBudget(military)` accepted by `military_loyalty == 0.8`
  even when `bureaucratic_compliance == 0.05` — confirms
  category split actually splits.
- Mid-list rejection: prior `AdjustBudget(military)` lands and
  logs; `AdjustBudget(welfare)` rejected at the gate stays at
  head; trailing `EnactPolicy` (also would have been rejected
  but the queue stopped first) stays queued.

Drive-by: the M2.18 "AdjustBudget unaffected by low compliance"
test gets a more honest name ("low bureaucratic_compliance still
applies if military_loyalty is high") and the assertion now also
pins the unaffected `military_loyalty` value.

## 5. Replay compatibility

`commands::replay_with_time` (M2.7) walks `applied_commands`
entries and re-invokes `apply_pending` per entry. Every v10 save
written before M2.19 had its `AdjustBudget` entries applied under
**default `0.5` authority across all fields**, so:

- The `"military"` category re-evaluates against
  `military_loyalty == 0.5` ⇒ accepts.
- Non-military categories re-evaluate against
  `bureaucratic_compliance == 0.5` ⇒ accepts.

Conclusion: no replay regression on existing saves. The same
caveat from M2.18 applies — a hand-edited save with low authority
+ a previously-Accepted command in the log will now reject on
replay, but that is the intended deterministic-determinism
behaviour.

## 6. Cross-links

- M2.18 (`m2-18-enact-policy-execution-gate.md`) — formula and
  wiring shape this PR mirrors.
- M2.17 (`m2-17-order-execution-skeleton.md`) — the skeleton
  this PR continues to extend.
- M2.16 (`m2-16-government-authority-state.md`) — supplies both
  `bureaucratic_compliance` and `military_loyalty` to the gate.
- M2.5 (`m2-5-adjust-budget.md`) — the `AdjustBudget` shape
  M2.19 now gates.
- M2.3 / M2.4 — `apply_pending`'s mid-list-failure semantics
  inherited unchanged.
- RFC-020 §2 — the execution-resistance catalogue M2.19 lands
  one more entry of.

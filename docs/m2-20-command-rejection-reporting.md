# M2.20 - Command rejection reporting

Companion notes for `feature/m2-20-command-rejection-reporting`.
M2.20 makes the M2.18 / M2.19 order-execution rejection
**observable as structured data** without changing the existing
`apply_pending` failure semantics. It adds:

- A new `RejectionRecord` POD describing which command was
  rejected and why (kind, identifier, the selected compliance
  value, threshold, resistance).
- A new `ApplyWithReportOutcome` that wraps the existing
  `ApplyOutcome` and an `std::optional<RejectionRecord>`.
- A new free function `commands::try_apply_pending` that drains
  the queue exactly like `apply_pending` does, but surfaces
  order-execution rejections as `Result::success` carrying the
  structured record instead of as `Result::failure` carrying a
  free-form string.

The legacy `apply_pending` keeps the M2.18 / M2.19 surface:
rejection still returns `Result::failure` with the byte-identical
error message. Existing callers (M2.7 `replay_with_time`, the
M2.18 / M2.19 tests, every driver) keep working without changes.

## 1. Scope

M2.18 and M2.19 introduced the first command-rejecting gates but
deliberately reported rejection only through the
`Result::failure` error string. That was the right call for
short-circuiting the queue drain and keeping `apply_pending`'s
mid-list-failure atomicity intact, but it makes programmatic
inspection awkward: callers have to parse the error string to
know "was this rejected by the gate, or did the policy lookup
fail?".

M2.20 adds the narrow alternative. The reviewer's constraint:
**do not change the existing `apply_pending` semantics, and do
not start persisting attempted commands**. The new helper is
opt-in; legacy callers see no behaviour change at all.

### What `try_apply_pending` actually does

Same precondition as `apply_pending` (valid `player_country`
indexing into `state.countries`). Drains the queue head one
command at a time:

- **Successful command** → mutate state, pop the queue head,
  append to `state.applied_commands`. Increment
  `result.apply.commands_applied`. Continue.
- **Order-execution rejection** (M2.18 / M2.19) → state stays
  untouched, queue head stays put, no `applied_commands`
  append. Populate `result.rejection` with the
  `RejectionRecord` and return `Result::success(result)`.
- **Real validation error** (unknown policy id_code, unknown
  budget category, non-finite delta, policy-effect resolution
  failure) → return `Result::failure(error_string)`.
  `try_apply_pending` never silently swallows these.

### Routing summary

| Outcome | `apply_pending` | `try_apply_pending` |
|---------|-----------------|---------------------|
| Full drain | `Result::success(ApplyOutcome)` | `Result::success(ApplyWithReportOutcome{apply, rejection=nullopt})` |
| Gate rejection | `Result::failure(legacy error)` | `Result::success(ApplyWithReportOutcome{apply, rejection=Record{...}})` |
| Precondition failure | `Result::failure` | `Result::failure` |
| Unknown policy / category / NaN delta / effect error | `Result::failure` | `Result::failure` |

The two helpers share their per-command logic via an internal
`detail::dispatch_one` extraction (anonymous namespace inside
`commands.cpp`) so they can diverge in surface without
duplicating dispatch logic.

## 2. Public API additions

`include/leviathan/systems/commands.hpp`:

```cpp
struct RejectionRecord {
    core::PlayerCommandKind kind{};
    std::string  policy_id_code;    // EnactPolicy only, else empty.
    std::string  budget_category;   // AdjustBudget only, else empty.
    double       compliance = 0.0;  // The authority input the gate read.
    double       threshold  = 0.0;  // The threshold the gate compared against.
    double       resistance = 0.0;  // 1.0 - compliance, surfaced for diagnostics.
};

struct ApplyWithReportOutcome {
    ApplyOutcome                   apply;
    std::optional<RejectionRecord> rejection;
};

core::Result<ApplyWithReportOutcome> try_apply_pending(
    core::GameState& state, CommandQueue& q);
```

### `RejectionRecord` field semantics

- `kind` always names the rejected command's
  `PlayerCommandKind`. Identifies which of the two id fields
  below is meaningful.
- `policy_id_code` carries the rejected `EnactPolicy` command's
  identifier; empty for `AdjustBudget`.
- `budget_category` carries the rejected `AdjustBudget`
  command's category; empty for `EnactPolicy`.
- `compliance` is the **selected** authority input the gate
  read. For `EnactPolicy` this is `bureaucratic_compliance`. For
  `AdjustBudget`(`"military"`) it is `military_loyalty`; for
  every other `AdjustBudget` category it is again
  `bureaucratic_compliance`. The record stores the resolved
  value, not which field was selected.
- `threshold` records whichever gate constant fired
  (`kEnactPolicyComplianceThreshold` or
  `kAdjustBudgetComplianceThreshold`; both currently 0.3).
- `resistance` mirrors `OrderExecutionOutcome::resistance`
  (= `1.0 - compliance`) for callers that prefer to render
  rejection as a resistance level rather than a compliance
  shortfall.

### `ApplyWithReportOutcome` invariants

- `rejection == std::nullopt` ⇒ the queue drained fully;
  `apply.commands_applied` equals the original
  `q.pending.size()`.
- `rejection.has_value()` ⇒ the drain stopped at the head;
  `apply.commands_applied` counts the strictly prior successful
  commands. The queue still contains the rejected command at
  the head plus any commands behind it (same shape as
  `apply_pending` leaves the queue on rejection).

## 3. Implementation

The bulk of M2.20 is plumbing: an internal `DispatchOutcome`
struct in `commands.cpp`'s anonymous namespace + a `dispatch_one`
helper that owns the per-command branching. `apply_pending` and
`try_apply_pending` both call `dispatch_one` and branch on the
result:

```cpp
auto r = dispatch_one(state, cmd, ctx);
if (!r) {
    // Real validation / system error.
    return Result::failure(std::move(r.error()));
}
if (r.value().rejection.has_value()) {
    // Gate rejection. apply_pending formats the legacy error
    // string; try_apply_pending surfaces the record as success.
    ...
}
// Applied. Pop the queue, append applied_commands.
```

`apply_pending` formats its rejection error via a private
`format_rejection_message(ctx, RejectionRecord)` helper. That
helper produces the same byte-for-byte string that the M2.18 /
M2.19 inline formatters did, so every existing apply_pending
caller (and every existing test asserting on substring contents
of the error) sees no change.

### Drive-by

`order_execution.cpp` had a stale M2.18-only comment ("Only
EnactPolicy is evaluated in this PR") above the `evaluate`
switch — flagged by the PR #46 review. Updated to mention both
the M2.18 `EnactPolicy` arm and the M2.19 `AdjustBudget` arm.
No behaviour change.

## 4. Tests

10 new doctest cases in `tests/systems/commands_test.cpp`
(M2.19 closed at 606 → M2.20 lands at 616). All pass; no
existing test was modified.

`try_apply_pending` happy + rejection paths:

- Full drain (single `EnactPolicy` at default 0.5 compliance) →
  success, `commands_applied == 1`, `rejection == nullopt`,
  `applied_commands.size() == 1`.
- `EnactPolicy` rejected at compliance 0.1 → success with
  `RejectionRecord{kind=EnactPolicy, policy_id_code="raise_taxes",
  compliance=0.1, threshold=0.3, resistance=0.9}`. Atomicity
  pinned: tax_burden, queue head, and `applied_commands` are
  all unchanged.
- `AdjustBudget("military")` rejected at military_loyalty 0.05 →
  success with `RejectionRecord{kind=AdjustBudget,
  budget_category="military", compliance=0.05, threshold=0.3,
  resistance=0.95}`. `compliance` records the **selected**
  input — military_loyalty — not bureaucratic_compliance.
- `AdjustBudget("welfare")` rejected at bureaucratic_compliance
  0.2 → `compliance` records `0.2`, even though military_loyalty
  is 0.95. The selected-input contract is honoured.

`try_apply_pending` non-execution failures (must stay failure):

- Unknown policy id_code → `Result::failure`, error mentions
  `unknown policy id_code`.
- Unknown budget_category → `Result::failure`, error mentions
  `unknown budget_category`.
- Non-finite `budget_delta` (NaN) → `Result::failure`, error
  mentions `not finite`.
- Invalid `player_country` (no countries loaded) →
  `Result::failure`, error names `try_apply_pending` and
  `player_country` so the source of the precondition is
  unambiguous.

Mid-list + backward compat:

- Mid-list `[AdjustBudget(military, mil_loyalty=0.9),
  EnactPolicy(raise_taxes, bureaucratic=0.05),
  AdjustBudget(welfare)]` → success with `commands_applied == 1`,
  `rejection.kind == EnactPolicy`,
  `rejection.policy_id_code == "raise_taxes"`. The military
  AdjustBudget landed and logged; the rejected EnactPolicy
  stays at the queue head; the trailing AdjustBudget(welfare)
  stays behind it untouched.
- `apply_pending` rejection still returns `Result::failure`
  with the legacy `order_execution` / `rejected` / id-code
  contents. Existing M2.18 / M2.19 tests untouched.

## 5. Replay compatibility

`commands::replay_with_time` still calls `apply_pending` per log
entry; its failure-on-rejection short-circuit is byte-identical
with M2.18 / M2.19. No replay-determinism change. Saves written
under default-0.5 authority continue to replay without
regression for the same reasons as M2.18 / M2.19.

## 6. What's NOT in scope

Deliberate non-goals:

- **No `apply_pending` signature change.** Existing callers
  (M2.7 `replay_with_time`, M2.18 / M2.19 tests, every driver)
  see byte-identical behaviour.
- **No `apply_pending` semantic change.** Rejection still
  returns `Result::failure` with the legacy formatted error.
- **No `RunOutcome` rejection counter / list.** The new
  surface lives on the command-system side only. A runner-level
  surface (per-day rejections during a replay, etc.) is a
  future PR.
- **No persistent attempted-command log.** `RejectionRecord`
  lives only in the in-process outcome; nothing is written to
  `state.applied_commands`, no save schema change, no
  separate `state.rejected_commands` runtime container, and no
  CSV.
- **No new `state.logs` entry on rejection.**
- **No save format change.** Still v10.
- **No DataLoader change.**
- **No replay primitive change.**
- **No CLI change.** `--replay` / `--verify` / `--target-date`
  etc. behave identically.
- **No new `PlayerCommandKind`.**
- **No new CSV column.**
- **No AI / events / UI / scheduler.**
- **No threshold / formula change.** M2.18 / M2.19 constants
  and routing untouched.

## 7. Cross-links

- M2.18 (`m2-18-enact-policy-execution-gate.md`) — formula and
  rejection-as-failure shape this PR preserves on
  `apply_pending` while adding the structured surface beside it.
- M2.19 (`m2-19-adjust-budget-execution-gate.md`) — category-
  aware AdjustBudget gate that supplies the `selected`
  compliance value M2.20's `RejectionRecord` records.
- M2.17 (`m2-17-order-execution-skeleton.md`) — the underlying
  `evaluate()` call whose outcome `dispatch_one` consumes.
- M2.3 (`m2-3-command-queue.md`) — mid-list-failure atomicity
  rule that `try_apply_pending` continues to honour
  (rejected command stays at queue head, no state mutation,
  no log).
- RFC-020 §2 — execution resistance catalogue;
  `RejectionRecord` is the first programmatic surface for
  presenting those resistances to a UI / forensic consumer.

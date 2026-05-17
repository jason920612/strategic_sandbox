# M4.1 - Command gate diagnostics surface

Companion notes for `feature/m4-01-command-gate-diagnostics`.

M4.1 opens **Milestone 4 — command / governance integration**.
It is a small, read-only diagnostic surface bridging the M2
command-execution gates (M2.18 `EnactPolicy` / M2.19
`AdjustBudget`) and the M3-mutable authority state
(`bureaucratic_compliance`, `military_loyalty`). Future M4.X
work (UI, command feedback, AI suggestion, structured logs,
event triggers) will read this surface instead of
reverse-engineering the gate.

No new gameplay command. No formula change. No new artefact.
No save schema bump. No event system, no trigger system, no
persistent event log. M3 stays closed.

## 1. Scope

The two M2 command gates already read `GovernmentAuthorityState`
through `order_execution::evaluate`. The decision is buried inside
`apply_pending` (M2.3) which mutates state in the same call.
That makes the gate hard to inspect from outside the apply path:

- a UI can't ask "would EnactPolicy on country X be accepted?"
  without actually trying to enact and rolling back;
- an AI helper can't suggest "raise compliance before issuing
  this command" without re-deriving the threshold and
  field-selection rule;
- a structured log can't write the gate inputs without
  duplicating the formula.

M4.1 adds two pure-read free functions that explain the gate
decision without touching state. They take the actor
`CountryId` directly (rather than reading `state.player_country`)
so a caller can ask "what would the gate decide for country X
right now?" without temporarily flipping the player selection.

## 2. Public API

`include/leviathan/systems/commands.hpp`:

```cpp
namespace leviathan::systems::commands {

enum class CommandGateKind {
    EnactPolicy,
    AdjustBudget,
};

struct CommandGateDiagnostic {
    CommandGateKind gate{};
    core::CountryId country{};
    std::string     country_id_code;
    std::string     target;           // "policy:<id>" or "budget:<category>"
    std::string     authority_field;  // "bureaucratic_compliance" or "military_loyalty"
    double          authority_value = 0.0;
    double          threshold       = 0.0;
    bool            allowed         = false;
};

core::Result<CommandGateDiagnostic> diagnose_enact_policy_gate(
    const core::GameState& state,
    core::CountryId country,
    const std::string& policy_id_code);

core::Result<CommandGateDiagnostic> diagnose_adjust_budget_gate(
    const core::GameState& state,
    core::CountryId country,
    const std::string& budget_category);

}  // namespace
```

### `diagnose_enact_policy_gate`

- Validates `country` indexes into `state.countries`.
- Reads only the country's `id_code` and
  `government_authority.bureaucratic_compliance`.
- Threshold: `order_execution::kEnactPolicyComplianceThreshold`
  (currently `0.3`).
- Returns:
  - `gate = EnactPolicy`
  - `target = "policy:<policy_id_code>"`
  - `authority_field = "bureaucratic_compliance"`
  - `authority_value = country.government_authority.bureaucratic_compliance`
  - `threshold = 0.3`
  - `allowed = authority_value >= threshold`

Does **not** check whether the policy exists in
`state.policies`. The diagnostic is gate-only; the policy
lookup is `apply_pending`'s downstream concern.

### `diagnose_adjust_budget_gate`

- Validates `country` indexes into `state.countries`.
- Reads the authority field relevant to the budget category:
  - `"military"` → `military_loyalty`;
  - every other string → `bureaucratic_compliance`.
- Threshold: `order_execution::kAdjustBudgetComplianceThreshold`
  (currently `0.3`).

Mirrors `order_execution::evaluate`'s M2.19 routing exactly: the
diagnostic does **not** validate that `budget_category` is one
of the seven canonical `BudgetState` fields. That whitelist
check happens later in `apply_pending`'s switch arm — the gate
itself doesn't enforce it. Unknown categories still route to
`bureaucratic_compliance` and produce a diagnostic; the apply
path will reject them separately.

## 3. Why `std::string` for `budget_category` instead of an enum

The spec's API sketch used `core::BudgetCategory`, but the
existing data model represents budget categories as `std::string`
fields on `PlayerCommand` (`PlayerCommand::budget_category`).
Introducing a new enum would either:

- duplicate that string representation across two types and
  invite drift, OR
- migrate every existing callsite — a refactor far beyond M4.1
  scope.

M4.1 uses `std::string` to match the existing model. If a
future PR introduces a real enum (with explicit conversion to
the on-disk string), the diagnostic API can adopt it then.

## 4. Why these helpers don't call `order_execution::evaluate`

`evaluate` reads `state.player_country` as the actor country.
The diagnostic takes a `CountryId` explicitly so callers can
ask the gate question for any country, not just the currently-
selected player. Wrapping `evaluate` with a temporary
`player_country` swap would mutate state (even briefly) — a
non-starter for a pure-read API.

Instead, the diagnostics reuse the **same constants and the
same field-selection rule** from `order_execution.hpp`:

- `kEnactPolicyComplianceThreshold`
- `kAdjustBudgetComplianceThreshold`
- the `"military"` → `military_loyalty`, else →
  `bureaucratic_compliance` rule

Test cases at the end of the M4.1 section in
`commands_test.cpp` (the "agrees with apply_pending" group) pin
that the diagnostic's `allowed` flag matches what
`apply_pending` actually decides on the same inputs. A future
change that diverges the constants or the routing without
updating both call sites will fail those mirror tests loudly.

## 5. What is NOT in scope

Per the M4.1 spec, this PR deliberately does not:

- add a new gameplay command;
- add a new `PlayerCommandKind`;
- change any command formula or threshold;
- change any mutation behaviour;
- change any replay or runner behaviour;
- add a new artefact;
- bump the save schema (still v11);
- add an event system / persistent event log / trigger system;
- add AI, UI, CLI, or REPL surfaces;
- add diplomacy or international layer work;
- change interest-group formulas;
- add new interest-group channels (`intelligence_capability`
  / `media_control` are still future work);
- reopen M3;
- close M4.

The helper is purely read-only — it never mutates `GameState`,
the command queue, applied commands, countries, policies,
budgets, interest groups, or logs.

## 6. Test surface

15 new doctest cases in `tests/systems/commands_test.cpp`
M4.1 section:

- `diagnose_enact_policy_gate`:
  - boundary `bureaucratic_compliance == 0.30` accepts
    (matches `>= threshold` rule);
  - below `0.30` rejects;
  - `military_loyalty` is ignored even when it would have
    flipped the decision;
  - works without the policy existing in `state.policies`;
  - invalid CountryId → failure;
  - default-constructed `CountryId{}` → failure.
- `diagnose_adjust_budget_gate`:
  - `"military"` reads `military_loyalty`, not
    `bureaucratic_compliance`;
  - non-military category (`"welfare"`) reads
    `bureaucratic_compliance`;
  - unknown category (`"ponies"`) still routes to
    `bureaucratic_compliance` (mirrors the gate's behaviour,
    not the apply path's whitelist);
  - invalid CountryId → failure.
- Mirror group (no formula drift):
  - accepted `EnactPolicy` → diagnostic `allowed` AND
    `apply_pending` succeeds;
  - rejected `EnactPolicy` → diagnostic `!allowed` AND
    `apply_pending` returns failure with `"rejected"` in
    the error;
  - accepted military `AdjustBudget` → diagnostic
    `allowed` AND `apply_pending` succeeds;
  - rejected military `AdjustBudget` → diagnostic
    `!allowed` AND `apply_pending` returns failure with
    `"rejected"` AND `"military"` in the error.
- Non-mutation regression: both helpers leave
  `current_date`, `logs`, `applied_commands`, and both
  authority fields byte-identical.

Doctest count: 779 → 794 (+15).

## 7. Future M4.2+ candidates (none committed)

- Surface the diagnostic on `RunOutcome` so a CLI run can
  print "would-have-rejected" decisions for the player
  country.
- Make the M2.20 `RejectionRecord` carry a
  `CommandGateDiagnostic` so the rejection path reuses the
  same struct.
- Add the same diagnostic helpers for hypothetical future
  command kinds (e.g. an `AppointMinister` command) once
  they exist.
- Cross-system explainability: combine the gate diagnostic
  with the M3.6 / M3.10 trace CSVs to show "why" the gate
  decided the way it did over a multi-month run.

Per the M-pacing rule, M4.2 starts only when the reviewer
names a direction in the M4.1 approval message.

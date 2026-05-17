# M2.5 - AdjustBudget player command

Companion notes for `feature/m2-05-adjust-budget`. M2.5 grows the
M2 command vocabulary by exactly one variant: `AdjustBudget`, which
lets the player nudge one of the seven budget categories on their
selected country. Everything else stays the same — the queue, the
log, the save format, the runner integration — all reused.

## 1. Scope

M2.3 introduced the dispatch infrastructure with `EnactPolicy` as
the only command kind. M2.4 made the log persistent. The reviewer
gate for M2.5 was explicit:

> "只新增一種 command、重用 validation/atomicity，不做 replay、
> 不做 UI、不做 AI"

So this PR ships:

- one new enum variant `PlayerCommandKind::AdjustBudget`,
- two new payload fields on `PlayerCommand`
  (`budget_category`, `budget_delta`),
- one new switch arm inside `commands::apply_pending`,
- two new switch arms inside `save_system.cpp` (serialise +
  deserialise per kind),
- a drive-by hardening of the `player_command_kind_to_string`
  fallback (PR #32 reviewer nit).

That's it. No save format bump, no new CLI flag, no new lifecycle
log, no replay, no UI, no AI.

## 2. Public API

`include/leviathan/core/player_commands.hpp`:

```cpp
enum class PlayerCommandKind {
    EnactPolicy,
    AdjustBudget,    // M2.5
};

struct PlayerCommand {
    PlayerCommandKind kind = PlayerCommandKind::EnactPolicy;

    // EnactPolicy payload.
    std::string policy_id_code;

    // AdjustBudget payload (M2.5).
    std::string budget_category;
    double      budget_delta = 0.0;
};
```

The struct stays flat — every kind's payload coexists in one struct
rather than going through `std::variant`. The unused fields per
command are cheap (a few bytes), and the JSON serialiser emits only
the kind-relevant fields (see §4). When `PlayerCommandKind` grows
again, future PRs add fields here in the same way.

## 3. Dispatch — `systems::commands::apply_pending`

The function gains one new arm:

```cpp
case core::PlayerCommandKind::AdjustBudget: {
    if (!std::isfinite(cmd.budget_delta)) {
        return failure("AdjustBudget budget_delta is not finite");
    }
    auto& country = state.countries[player_country.value()];
    double* field = budget_field_ptr(country, cmd.budget_category);
    if (field == nullptr) {
        return failure("AdjustBudget unknown budget_category '...'");
    }
    *field = std::clamp(*field + cmd.budget_delta, 0.0, 1.0);
    break;
}
```

Properties this preserves:

- **Precondition unchanged.** Same `state.player_country` must
  index into `state.countries`. `apply_pending` rejects the
  whole call before any command runs if that's not true.
- **Per-command atomicity.** A bad category or a non-finite delta
  fails this command; previously-applied commands stay applied and
  logged; the failing command stays at the queue head; later
  commands stay queued. Same shape as M2.3 / M2.4 `EnactPolicy`.
- **Log append on success.** The M2.4 append site
  (`state.applied_commands.push_back(...)`) is shared, so a
  successful `AdjustBudget` produces one log entry with the
  AdjustBudget payload. A failed one does not log.

`budget_field_ptr` is a tiny local helper (7-string switch) — the
M1.5 policy system has its own internal helper of the same shape,
but exposing it would invert the layering. Duplicating 7 if-strings
is the cheaper move.

Valid categories: `administration`, `military`, `education`,
`welfare`, `intelligence`, `infrastructure`, `industry` — the same
seven `BudgetState` fields M1.3 introduced.

## 4. Save format — no version bump

The on-disk shape of `applied_commands` is unchanged: still an
array of `{applied_on, command:{kind, <payload>}}` objects. What
changes is which kind strings are valid and which payload fields
each kind serialises.

Per-kind JSON shape:

- **EnactPolicy** (unchanged from M2.4):
  ```json
  { "kind": "EnactPolicy", "policy_id_code": "raise_taxes" }
  ```
- **AdjustBudget** (new):
  ```json
  { "kind": "AdjustBudget", "budget_category": "military",
    "budget_delta": 0.05 }
  ```

The serialiser branches on `kind`; the deserialiser does the same
after reading `kind`. An entry of unknown kind is rejected with the
existing `unknown player command kind '...'` error — that strict
gate was already in place from M2.4, so an old M2.4 binary loading
a v9 save written by M2.5+ that contains an `AdjustBudget` entry
will fail loudly at the kind validator. No silent acceptance.

**Why no `kSaveFormatVersion` bump?** The persistent state shape
(the array itself, the entry envelope, the loader's strict
key-required policy) is byte-identical to v9 — only the set of
valid `kind` strings grew. The kind validator already provides the
loud-rejection guarantee that strict-equality version gates exist
for. Bumping to v10 here would force every M2.4 test fixture and
M0.10 / M1.14 / M1.16 byte-identical determinism contract to
churn, with no actual safety benefit beyond what the kind validator
already provides.

This decision is recorded so a future contributor can revisit it
when adding the next kind. The rule: bump on schema shape changes,
not on additive enum growth.

## 5. Drive-by: PR #32 exhaustive-switch hardening

The PR #32 reviewer flagged:

> `player_command_kind_to_string` 的 switch fallback 目前回傳
> `"EnactPolicy"`。現在 enum 只有一個值，不會出事；未來新增
> command kind 時，要記得讓 compiler warning 或測試抓到漏處理。

Two changes in `save_system.cpp`:

- Added the new `AdjustBudget` arm to both
  `player_command_kind_to_string` and
  `player_command_kind_from_string`.
- Changed `player_command_kind_to_string`'s fallback return value
  from `"EnactPolicy"` (a real kind, would silently corrupt saves
  if reached) to `"UnknownPlayerCommandKind"` (a sentinel that
  loud-fails any save it touches). The compiler's `-Wswitch` is the
  primary defence against unhandled enum cases; this is the
  runtime backstop.

The `from_string` rejection message also gained the new kind in its
"expected" list:
`unknown player command kind 'X' (expected EnactPolicy|AdjustBudget)`.

## 6. Tests

11 new doctest cases (M2.4 was 483 → M2.5 is 494).

`tests/systems/commands_test.cpp` (8 new cases in an "M2.5:
AdjustBudget command kind" section):

- AdjustBudget on `military` mutates `budget.military` by the
  delta.
- Negative delta shrinks the field (`education 0.10 -> 0.05`).
- Overshoot clamps to `1.0` (`military 0.35 + 1.0 -> 1.0`).
- Undershoot clamps to `0.0` (`welfare 0.10 - 1.0 -> 0.0`).
- Unknown `budget_category` rejected with `AdjustBudget` and the
  bad string in the error; state unchanged; queue head intact; log
  empty.
- Non-finite `budget_delta` rejected with `not finite` in the
  error.
- Successful enact appends the correct M2.4 log entry (correct
  `applied_on`, kind, category, delta).
- Mixed-kind queue (`EnactPolicy` + `AdjustBudget`) applies both in
  insertion order; log gets both entries in the same order.

`tests/systems/save_system_test.cpp` (3 new cases):

- `save + load` of an `AdjustBudget` log entry round-trips
  category + delta.
- v9 entry with `kind: AdjustBudget` missing `budget_category`
  rejected with `applied_commands[N]` + `budget_category` in the
  error.
- v9 entry with `kind: AdjustBudget` missing `budget_delta`
  rejected with `applied_commands[N]` + `budget_delta` in the
  error.

## 7. What's NOT in scope

Deliberate non-goals:

- **No save format change.** Still v9. See §4 for the rationale.
- **No replay.** M2.6 territory.
- **No other command kinds.** No `ChangeTaxBurden`, no
  `ToggleEvent`, no `Pause` command.
- **No UI / CLI flag.** Players still queue commands through the
  external driver (test code today).
- **No `set` semantics.** M2.5 ships delta-add only; if absolute
  setting is wanted later, it would be a new kind or a new flag
  on the payload, not a rework of this one.
- **No automatic budget validation.** The 7-category sum is still
  not enforced to equal 1.0 (deliberately, per M1.3); the player
  can over- or under-allocate.
- **No new `state.logs` entry.** The replay log is its own
  surface.

## 8. Cross-links

- RFC-090 §M2 task 2.4 — "預算調整命令" — this PR satisfies it.
- M1.3 (`m1-3-budget.md`) — `BudgetState` shape; the seven fields
  are the AdjustBudget category whitelist.
- M1.5 (`m1-5-policy-system.md`) — the policy system uses the same
  ratio-clamp policy AdjustBudget reuses (`[0, 1]` post-op).
- M2.3 (`m2-3-command-queue.md`) — dispatch infrastructure;
  M2.5 adds one more arm.
- M2.4 (`m2-4-command-log.md`) — log append site; M2.5 inherits
  it unchanged.

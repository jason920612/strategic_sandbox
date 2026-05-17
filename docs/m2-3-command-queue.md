# M2.3 - Player command queue

Companion notes for `feature/m2-03-command-queue`. M2.3 introduces the
smallest useful surface for the M2 player-operation prototype: a
runtime command queue, a single command type (`EnactPolicy`), and a
free function that drains the queue by dispatching each entry through
the M1 systems.

The M2.2 `begin_tick / step_one_day / end_tick` primitives gave an
external driver the seam to *pause* between days. M2.3 gives that
driver something to *do* during the pause: stage commands, then apply
them before the next `step_one_day`.

## 1. Scope

M1 ran headless: scenarios loaded, the monthly pipeline ticked, M1.13
day-0 policies fired through `policy::apply_policy_effects` exactly
once at load time. M2.1 added the `--player COUNTRY_IDCODE`
selection. M2.2 added the day-at-a-time seam. M2.3 supplies the
"player issues a command" call path.

That's the whole PR. One command kind. One free function. No save
format change. No CLI flag. No automatic integration with
`step_one_day` (the driver decides when to drain).

## 2. Public API

### Core type — `include/leviathan/core/player_commands.hpp`

```cpp
namespace leviathan::core {

enum class PlayerCommandKind {
    EnactPolicy,
};

struct PlayerCommand {
    PlayerCommandKind kind = PlayerCommandKind::EnactPolicy;
    std::string policy_id_code;   // EnactPolicy payload
};

}  // namespace leviathan::core
```

Future M2.x sub-milestones will extend `PlayerCommandKind`
(`AdjustBudget`, `ChangeTaxBurden`, ...). Each variant adds whatever
payload fields it needs; existing fields stay backward-compatible.

### Queue + dispatch — `include/leviathan/systems/commands.hpp`

```cpp
namespace leviathan::systems::commands {

struct CommandQueue {
    std::vector<core::PlayerCommand> pending;
};

struct ApplyOutcome {
    int commands_applied = 0;
};

core::Result<ApplyOutcome> apply_pending(core::GameState& state,
                                         CommandQueue& q);

}  // namespace leviathan::systems::commands
```

## 3. Architecture

The queue is **owned by an outer driver** — not part of `GameState`,
not part of `runner::TickController`. Three consequences:

1. **No save format change.** `CommandQueue` is runtime-only; save
   format stays at v8.
2. **No cross-module dependency between `commands` and `runner`.**
   `commands.hpp` does not include `runner.hpp` and vice versa. The
   M2.4 command log will introduce its own persistence story.
3. **Driver controls cadence.** A driver can drain the queue
   between days, multiple times per day, never — whatever suits the
   product. M2.3 does not wire `apply_pending` into the runner loop
   automatically.

The expected driver pattern:

```cpp
runner::TickController ctrl;
commands::CommandQueue q;
runner::begin_tick(state, opts, ctrl);
for (int i = 0; i < opts.days; ++i) {
    // Outer driver stages commands (CLI input, future UI, ...):
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});
    // Drain before stepping so the policy takes effect this day:
    auto applied = commands::apply_pending(state, q);
    runner::step_one_day(state, opts, ctrl);
}
runner::end_tick(state, opts, ctrl);
```

## 4. Atomicity

`apply_pending` is **non-atomic across commands** but **atomic per
command**. Each `EnactPolicy` is dispatched through
`policy::apply_policy_effects`, which has had pre-flight-then-apply
atomicity since M1.5. So within one command:

- All targets / ops are pre-flight-resolved.
- If any resolution fails, state is unchanged for that command and the
  command stays at the head of the queue.

Across the whole queue, M2.3 follows the M1.13 precedent:

- Commands are processed in insertion order.
- On the first failure, processing stops.
- Previously-applied commands stay applied; the failed command stays
  at the head of the queue; later commands stay queued.

Callers can fix state (or the command payload) and retry; the queue
is the natural "what's left to do" container.

### Precondition

`apply_pending` rejects with no mutation if
`state.player_country` is not a valid index into `state.countries`:

```cpp
if (!state.player_country.valid() ||
    state.player_country.value() < 0 ||
    static_cast<size_t>(state.player_country.value()) >= state.countries.size()) {
    return failure("state.player_country is not a valid index ...");
}
```

This is the runtime expression of the M2.1 contract: an EnactPolicy
without a selected player country has no actor.

## 5. Tests

8 new doctest cases (M2.2 was 462 → M2.3 is 470). All in
`tests/systems/commands_test.cpp`:

- **Empty queue** is a no-op success (`commands_applied == 0`).
- **Single EnactPolicy** drains the queue and mutates the country
  (`legal_tax_burden 0.20 -> 0.25` via `raise_taxes`).
- **Successful enact chains into `active_policies` (M1.15)**: GER's
  list grows by one entry with `expires_on = 1930-03-02`
  (`current_date + duration_days = 1930-01-01 + 60 days`). This pins
  the integration with the M1.15 duration tracker.
- **Multiple commands** apply in insertion order; both effects land;
  `active_policies` carries entries in the same order.
- **No `player_country`** rejected with `player_country` in the
  error; queue untouched; country baseline unchanged; no
  active_policies appended.
- **`player_country` out of range** (valid()==true but past
  `countries.size()`) rejected with the same `player_country` error.
- **Unknown `policy_id_code`** mid-queue stops at the failed command.
  First command applied; failed command at head; trailing valid
  command still queued. `legal_tax_burden` changed,
  `budget.education` unchanged.
- **Policy with bad effect target** (the underlying
  `policy::apply_policy_effects` rejects at pre-flight) stops at the
  failed command. Same queue-and-state shape.

## 6. What's NOT in scope

Deliberate non-goals:

- **No save format change.** The queue is runtime-only.
- **No new CLI flag.** No `--command FILE` or interactive REPL yet.
- **No auto-drain inside `step_one_day`.** The driver decides when
  to call `apply_pending`.
- **No other command kinds.** Only `EnactPolicy`. Budget adjustment,
  tax burden change, etc. land in later sub-milestones.
- **No M1 system change.** `policy::apply_policy_effects` is reused
  unchanged; M1.5 atomicity, M1.15 duration tracking, M1.15
  `kMaxTrackedPolicyDurationDays` cap all apply.
- **No command log.** M2.4 introduces a persistent log of *applied*
  commands; M2.3 only manages *pending* commands.
- **No queue persistence across save/load.** A future sub-milestone
  can extend save_system to carry `CommandQueue` contents (would bump
  save format v8 → v9), but M2.3 deliberately punts.
- **No multi-player command interleaving.** Single
  `state.player_country` is intentional.
- **No new logs.** `apply_pending` mutates state through
  `policy::apply_policy_effects`; it does not emit a "command
  applied" log entry. M2.4 will own that.

## 7. Cross-links

- RFC-090 §M2 — Milestone 2 roadmap.
- M1.5 (`m1-5-policy-system.md`) — `apply_policy_effects` is the
  per-command primitive M2.3 dispatches through.
- M1.13 (`m1-13-scenario-starting-policies.md`) — same
  "process-list-of-things, non-atomic across the list" pattern; the
  scenario loader's day-0 enactment is structurally identical to a
  one-shot `apply_pending` call.
- M1.15 (`m1-15-policy-duration-tracking.md`) — successful enacts via
  the command queue automatically populate `active_policies`.
- M2.1 (`m2-1-player-country.md`) — `state.player_country` is the
  actor `apply_pending` requires.
- M2.2 (`m2-2-pause-resume.md`) — `runner::TickController` is the
  seam an outer driver pairs with `CommandQueue`.

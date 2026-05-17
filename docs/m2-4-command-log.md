# M2.4 - Player command log

Companion notes for `feature/m2-04-command-log`. M2.4 turns the
M2.3 command queue into a record that survives save / load:
`systems::commands::apply_pending` now appends a log entry on every
successful per-command dispatch, and the save format bumps from v8
to v9 to round-trip the new state. This is the foundation a later
sub-milestone will build on for deterministic replay
(RFC-050 §8 "玩家命令需記錄").

## 1. Scope

M2.3 introduced a runtime command queue and a per-command dispatch
through `policy::apply_policy_effects`. The queue itself is
ephemeral; once a command succeeds, it pops off the queue and the
trace evaporates. M2.4 closes that gap with a single new field on
`GameState`, an append-only write at the end of the successful-apply
path, and a strict save schema bump.

That's the whole PR. No replay implementation, no log compaction, no
new CLI flag, no new command kinds, no new logs in `state.logs`.

## 2. Public API — `include/leviathan/core/player_commands.hpp`

```cpp
struct AppliedPlayerCommand {
    GameDate      applied_on{};   // state.current_date at apply time
    PlayerCommand command{};      // the command that was applied
};
```

The struct nests `PlayerCommand` rather than flattening its fields,
so future `PlayerCommandKind` variants can grow their own payload
without forcing the log entry shape to grow horizontally.

## 3. `GameState::applied_commands`

```cpp
struct GameState {
    // ...
    std::vector<AppliedPlayerCommand> applied_commands;   // M2.4
};
```

Default empty. Append-only at the
`systems::commands::apply_pending` site; no simulation system reads
or writes the field. This keeps M1's deterministic pipeline
completely unaware of the log — the M1.17 5-artefact byte-identical
contract is unaffected.

## 4. Append site — `systems::commands::apply_pending`

The M2.3 dispatch loop already had a clear "this command succeeded"
boundary:

```cpp
// inside the while-loop, after policy::apply_policy_effects ok()
q.pending.erase(q.pending.begin());
++outcome.commands_applied;
state.applied_commands.push_back(
    core::AppliedPlayerCommand{state.current_date, cmd});   // M2.4
```

Properties this preserves:

- **Per-command atomicity**: the log entry is appended only AFTER
  the state mutation succeeds. A pre-flight rejection inside
  `policy::apply_policy_effects` leaves both state and the log
  untouched for that command — exactly what the PR #31 reviewer
  asked us to maintain.
- **Cross-list non-atomic** (matches M2.3): if a later command in
  the same `apply_pending` call fails, earlier log entries stay in
  place, since their commands already mutated state. The failed
  command's entry is *not* appended.
- **`applied_on` captures the apply-time date**, not the submission
  date. If a driver submits a command on day 1 and only invokes
  `apply_pending` on day 6, the log records day 6. A test pins this.

## 5. Save format v8 → v9

History (from `save_system.hpp`):

```
v8 (M2.1) - GameState gained `player_country` at the root.
v9 (M2.4) - GameState gained `applied_commands` (a vector of
            `{applied_on, command}` records the player command
            queue appends on every successful enactment).
            A v8 save would silently drop the replay log on reload.
```

New root-level array in the save JSON:

```json
{
  "save_version": 9,
  "rng_algorithm_version": 1,
  "current_date": "1930-06-15",
  "player_country": 0,
  "rng": { ... },
  "countries": [ ... ],
  ...
  "applied_commands": [
    {
      "applied_on": "1930-02-15",
      "command": { "kind": "EnactPolicy", "policy_id_code": "raise_taxes" }
    },
    {
      "applied_on": "1930-04-01",
      "command": { "kind": "EnactPolicy", "policy_id_code": "increase_education" }
    }
  ]
}
```

Loader rules:

- `save_version != 9` rejected loudly (same gate as v1..v8).
- `applied_commands` missing on the root → hard failure.
- `applied_commands` not an array → hard failure.
- Each entry must be a JSON object with:
  - `applied_on`: a valid Gregorian date (`YYYY-MM-DD`).
    `1930-02-30` is rejected by the existing proleptic Gregorian
    parser with the bad string in the error.
  - `command`: a JSON object with:
    - `kind`: one of the recognised strings. M2.4 only ships
      `"EnactPolicy"`. Unknown kinds rejected with the bad string
      in the error.
    - `policy_id_code`: a string. Missing is rejected.
- Error paths name the array index: `applied_commands[0]: ...`.

Insertion order matches the in-memory order, so a save → load
round trip preserves the apply sequence verbatim.

## 6. Tests

13 new doctest cases (M2.3 was 470 → M2.4 is 483).

`tests/core/game_state_test.cpp` (extended baseline case): default
GameState has `applied_commands.empty() == true`.

`tests/systems/commands_test.cpp` (5 new cases in an "M2.4: command
log appended on success" section):

- successful single enact appends one entry; fields match
- multiple successes append in insertion order
- a failed command does NOT append (per-command atomicity for the
  log)
- `applied_on` captures `state.current_date` at apply time —
  bump the date between two `apply_pending` calls; the two log
  entries carry the two distinct dates
- a precondition failure (no `player_country`) leaves the log empty

`tests/systems/save_system_test.cpp` (8 new cases):

- rejects an old v8 save loudly
- serialize emits `"applied_commands": []` for a default state
- save+load round-trips populated entries (two entries on
  `build_seeded_state`, dates and `policy_id_code` preserved)
- v9 missing `applied_commands` rejected
- v9 entry with malformed `applied_on` (`1930-02-30`) rejected with
  the bad date string in the error
- v9 entry with unknown `kind` (`"SomethingBogus"`) rejected
- v9 entry missing `policy_id_code` rejected
- v9 entry missing `command` sub-object rejected

Every existing `"save_version": 8` fixture was bulk-updated to
`"save_version": 9`, and the test names that quoted `v8` were
renamed to `v9`. The `--scenario` and 5-artefact integration tests
keep passing unchanged — the save loader now expects
`applied_commands`, and the serialiser always emits at least an
empty array, so any save the runner writes round-trips cleanly.

## 7. What's NOT in scope

Deliberate non-goals:

- **No replay implementation.** M2.4 only stores the log. Replay
  belongs to a later sub-milestone (probably M2.5+ or M2.6).
- **No log compaction / truncation.** The list grows monotonically
  for the lifetime of a save.
- **No log entries for failed commands.** Per-command atomicity is
  the rule; failures stay in the queue, not the log.
- **No new `PlayerCommandKind` variants.** Still `EnactPolicy`
  only. New variants land in their own sub-milestone and extend
  the kind-string mapping in `save_system.cpp` accordingly.
- **No new CLI flag.** No `--apply-on-step`, no replay flag.
- **No new lifecycle log entry in `state.logs`.** The replay log is
  its own surface, separate from the M0.6 simulation log.
- **No commit of the queue itself.** `systems::commands::CommandQueue`
  stays runtime-only; only *applied* commands persist.
- **No auto-drain inside `step_one_day`.** Driver-controlled cadence
  unchanged from M2.3.

## 8. Cross-links

- RFC-090 §M2 — Milestone 2 roadmap; RFC's task 2.6 ("命令 log")
  is the spirit M2.4 implements.
- RFC-050 §8 — "玩家命令需記錄" (player commands must be
  recorded) — the design constraint M2.4 satisfies.
- M2.1 (`m2-1-player-country.md`) — previous save-format bump
  (v7 → v8). M2.4 follows the same strict-required-field rejection
  pattern.
- M2.2 (`m2-2-pause-resume.md`) — the runner primitives that the
  outer driver pairs with `CommandQueue` + `apply_pending`.
- M2.3 (`m2-3-command-queue.md`) — the queue + dispatch that M2.4
  extends with persistence.
- M1.15 (`m1-15-policy-duration-tracking.md`) — same loud-rejection
  pattern for a save-format bump introducing a new container.

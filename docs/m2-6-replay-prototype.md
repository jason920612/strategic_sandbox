# M2.6 - Replay applied command log prototype

Companion notes for `feature/m2-06-replay-prototype`. M2.6 is the
first consumer of the M2.4 `applied_commands` log: a free function
that re-applies a recorded command sequence onto a freshly-built
target state. Foundation for full deterministic replay; deliberately
a **prototype** with documented limits.

## 1. Scope

M2.4 stored what the player did. M2.5 grew the vocabulary of "what".
M2.6 ships the smallest useful "re-do it": given the log and a fresh
state, produce matching log entries and matching state effects for
the recorded commands.

That's the whole PR. One new free function. No save format change,
no state shape change, no new CLI flag, no new log line, no
simulation-system involvement.

## 2. Public API

`include/leviathan/systems/commands.hpp`:

```cpp
namespace leviathan::systems::commands {

struct ReplayOutcome {
    int commands_replayed = 0;
};

core::Result<ReplayOutcome> replay(
    core::GameState& target_state,
    const std::vector<core::AppliedPlayerCommand>& log);

}  // namespace leviathan::systems::commands
```

## 3. Implementation strategy

For each entry in `log`:

1. **Force the date.** `target_state.current_date = entry.applied_on`.
   This makes the M2.4 log-append site record the same `applied_on`
   the source did.
2. **Build a 1-element `CommandQueue`** containing `entry.command`.
3. **Call `apply_pending(target_state, q)`.** The existing dispatch
   path runs: precondition check, kind switch (M2.3 EnactPolicy +
   M2.5 AdjustBudget), per-command atomicity, M1.5/M1.15 effect
   application, M2.4 log-on-success append.

```cpp
for (size_t i = 0; i < log.size(); ++i) {
    target_state.current_date = log[i].applied_on;
    CommandQueue q;
    q.pending.push_back(log[i].command);
    auto r = apply_pending(target_state, q);
    if (!r) return failure("commands::replay[" + i + "]: " + r.error());
    ++outcome.commands_replayed;
}
```

The 1-element-queue approach is intentional. It guarantees `replay`
inherits **every** invariant `apply_pending` already enforces — we
do not duplicate the dispatch logic, do not invent a parallel log-
append path, do not re-implement M1.5 atomicity.

## 4. Preconditions

`replay` rejects loudly without mutating state if either fails:

- **`target_state.player_country` must be valid and index into
  `target_state.countries`.** Without it, the M2.3 dispatch
  precondition would fire at the first command anyway. We surface
  the rejection up front so the error message is clearer
  (`replay: target_state.player_country is not a valid index ...`)
  rather than `replay[0]: apply_pending: ...`.
- **`target_state.applied_commands` must be empty.** Otherwise the
  replayed entries would mix with the prior ones, defeating the
  "log mirrors source" guarantee. Callers that want to replay
  should build a FRESH state from the scenario loader, **not**
  reload a save and replay on top.

## 5. Atomicity across the log

Same shape as M2.3 / M2.4 mid-list failure:

- On any per-entry `apply_pending` failure, replay returns
  `Result::failure` with the entry's index in the error message
  (`replay[N]: ...`).
- Entries `[0, N)` are applied + logged in `target_state`.
- Entry `N` is **not** applied (per-command atomicity inside
  `apply_pending` kicks in).
- Entries `(N, end)` are **not** replayed.

Caller can inspect `target_state.applied_commands.size()` to see
how far replay got, fix the discrepancy, and rebuild a new replay
attempt against another fresh state.

## 6. Limitations (M2.6 is a prototype)

Pinned by tests and called out here so a future contributor doesn't
re-discover them:

1. **No time-system advancement between commands.** Replay forces
   `current_date` to each entry's `applied_on` but does NOT run
   `time::advance_one_day` or `monthly::tick_all_countries` between
   them. If the source state's monthly pipeline produced effects
   (M1.6 faction drift, M1.7 stability tick, M1.8 economy tick),
   those effects are **not** reproduced by replay.
2. **`target_state.current_date` ends at the last entry's
   `applied_on`.** It does NOT advance to match the source state's
   final `current_date`. A test pins this so the limitation is
   visible.
3. **Caller-loaded scenario.** Replay does not load countries /
   factions / policies. The caller must already have a populated
   `target_state` (typically via `scenario_loader::load_into_state`)
   before calling `replay`.
4. **No divergence detection.** Replay does not compare the
   replayed state against any reference. The caller compares fields
   directly (CSV diff, save-byte-diff, hand-rolled assertions).

A future sub-milestone (M2.7+) will lift these limits by integrating
replay with the M2.2 `step_one_day` primitive so the time-system
advances naturally between command applies and the final
`current_date` matches the source.

## 7. Tests

9 new doctest cases (M2.5 was 494 → M2.6 is 503), all in
`tests/systems/commands_test.cpp` under an "M2.6: replay
applied-command log" section:

- **Empty log** is a no-op success (`commands_replayed == 0`,
  `applied_commands.empty()`).
- **Single EnactPolicy entry** replays: state mutates,
  `applied_commands` ends with one entry whose `applied_on` and
  payload match.
- **Single AdjustBudget entry** replays: same shape.
- **Mixed-kind log** replays in insertion order; both effects
  land.
- **Replayed log mirrors source byte-equivalent.** Build a source
  log by submitting through `apply_pending` across two distinct
  dates (1930-01-05 EnactPolicy + 1930-04-12 AdjustBudget). Replay
  that log into a fresh state. Verify entry-by-entry that
  `applied_on`, `kind`, and payload fields match. Also verify state
  effects (`legal_tax_burden`, `budget.infrastructure`) match.
- **`current_date` forced to last entry's `applied_on`.** Pins the
  prototype limit: with a log entry on 1930-06-15 against a state
  starting at 1930-01-01, replay ends with
  `current_date == 1930-06-15`.
- **Unknown `policy_id_code` mid-list** stops with
  `replay[1]: ... not_a_real_policy ...` in the error.
  Entry 0 stays applied + logged; entry 1 not applied; entry 2
  (if any) not replayed.
- **No `player_country` selected** rejects with `player_country` in
  the error; `applied_commands` stays empty.
- **Non-empty `applied_commands`** rejects with
  `applied_commands must be ...` in the error; the pre-existing
  entry stays untouched.

## 8. What's NOT in scope

Deliberate non-goals:

- **No time-system involvement.** No `time::advance_one_day` calls
  inside `replay`; the M2.6 prototype is "apply commands at the
  recorded dates" only.
- **No monthly pipeline ticks during replay.** Same reason: M2.6
  ships the command-replay primitive, not full timeline replay.
- **No scenario reload.** Caller's responsibility.
- **No divergence comparison.** Caller compares state directly.
- **No save format change.** Schema stays v9.
- **No new `state.logs` entry.** Replay produces the same
  per-command log entries as the original; it does not emit
  lifecycle events ("replay started" / "replay finished").
- **No command outcome capture.** The M2.4 reviewer nit about
  recording old/new values in the log stays deferred — it's
  orthogonal to replay.
- **No CLI flag.** No `--replay PATH`. Replay is a library
  primitive; an outer driver wires it up.
- **No multi-player.** Single `state.player_country` actor.
- **No M1 system change.** All five (faction::react, stability::tick,
  economy::tick, monthly pipeline, diagnostics) are unchanged.

## 9. Cross-links

- RFC-050 §8 — "玩家命令需記錄" — the recording side; M2.6 closes
  the loop on the consumption side.
- M2.3 (`m2-3-command-queue.md`) — the dispatch infrastructure
  `replay` invokes one command at a time.
- M2.4 (`m2-4-command-log.md`) — the log shape `replay` consumes;
  same per-command atomicity for the log append.
- M2.5 (`m2-5-adjust-budget.md`) — second command kind; replay
  automatically supports both kinds because it goes through
  `apply_pending`.
- M2.2 (`m2-2-pause-resume.md`) — the future M2.7+ "replay with
  time-system advancement" will integrate with the
  `step_one_day` primitive from this header.

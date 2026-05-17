# M2.11 - Replay verify CLI

Companion notes for `feature/m2-11-replay-verify`. M2.11 wires the
M2.10 `diagnostics::compare_states` primitive into the M2.8 `--replay`
flow via a new `--verify` flag. After replay completes, the runner
auto-compares the replayed state against the loaded source save and
reports mismatches.

Informational only — the run still succeeds regardless of mismatch
count, and `main()` prints the field paths to stdout. A strict
fail-on-mismatch mode is a deliberate future-scope decision; M2.11
ships the smallest useful wiring.

## 1. Scope

M2.8 shipped `--replay PATH` and explicitly said "user diffs save
files themselves". M2.10 shipped the comparison primitive. M2.11
shipped the obvious connector.

That's the whole PR. One boolean flag. One outcome field. One
post-replay call site. No CLI ergonomics beyond the existing
`Replay source` / `Commands replayed` block. No save format
change.

## 2. Public API

`include/leviathan/systems/runner.hpp`:

```cpp
struct RunnerOptions {
    // ... existing flags ...
    bool verify = false;   // M2.11; requires --replay
};

struct RunOutcome {
    // ... existing fields ...
    std::vector<diagnostics::StateMismatch> verify_mismatches;
};
```

`parse_args` recognises `--verify` (no value; presence-only) and
rejects it loudly if `--replay` is not also set:

```
--verify requires --replay (--verify is a no-op without a source
save to compare the replayed state against)
```

## 3. `run()` integration

The replay branch (M2.8) becomes:

```cpp
auto outcome = std::move(end_r).value();
outcome.replay_commands_replayed = replay_r.value().commands_replayed;
if (opts.verify) {
    outcome.verify_mismatches =
        diagnostics::compare_states(state, loaded);
}
return success(std::move(outcome));
```

Two things worth calling out:

1. **Source save is already loaded.** The replay flow loaded it
   right before `begin_tick` to extract `applied_commands` (and the
   optional `player_country`). M2.11 reuses that local `loaded`
   value rather than re-reading from disk.
2. **Verify runs AFTER `end_tick`.** `end_tick` writes the save /
   JSONL / CSV artefacts; verify is the last step. If verify
   uncovers mismatches, the artefacts are still on disk for the
   user to inspect.

## 4. `main()` stdout

When `opts.verify` is set, `main()` prints one summary line plus
one bullet per mismatch:

```
Verify mismatches   : 1
  - countries[0].legal_tax_burden : 2.5e-01 != 9.9e-01 (tolerance 1e-09)
```

Zero mismatches still prints the summary line (`Verify mismatches : 0`)
— makes the line stable so external scripts can grep it. Exit
code stays 0 regardless of mismatch count.

## 5. Tests

5 new doctest cases (M2.10 was 532 → M2.11 is 537):

- **`parse_args: --verify plumbed`** when combined with `--replay`:
  `opts.verify == true`.
- **`parse_args: --verify defaults false`** when absent.
- **`parse_args: --verify without --replay rejected`** with both
  flag names in the error.
- **`run: --replay + --verify with matching source`** produces
  `verify_mismatches.empty()`. Uses the same `build_source_save`
  helper M2.8 introduced.
- **`run: --replay + --verify on a tweaked source detects
  mismatch`**: build a source save, mutate `countries[0].legal_tax_burden`
  to `0.99` directly on the saved state, re-save. Run with
  `--replay --verify`. Replay produces the deterministic
  `0.25` (default 0.20 + raise_taxes 0.05); compare_states catches
  the divergence at the documented path
  (`countries[0].legal_tax_burden`).

## 6. Use cases

- **Replay regression CI**: a script runs `leviathan --replay
  golden.json --verify --scenario foo` and greps stdout for
  `Verify mismatches   : 0`. Drift fails the CI run, but the
  underlying simulation still completes (the artefacts land on
  disk for forensic inspection).
- **Quick local sanity check** during M1.x / M2.x rebalancing:
  re-run a known save with `--verify` to confirm formula tweaks
  haven't accidentally shifted state.
- **Future strict-mode hook**: a later sub-milestone can introduce
  `--verify-strict` that fails the run on non-zero mismatches. The
  current `outcome.verify_mismatches.size()` is the natural input.

## 7. What's NOT in scope

Deliberate non-goals:

- **No strict fail-on-mismatch mode** (`--verify-strict`) — exit
  code stays 0. The current contract is "informational"; a strict
  variant deserves its own sub-milestone with a clear decision
  about partial success semantics.
- **No CLI knob for `CompareOptions::double_tolerance`** — the
  default `1e-9` is used. A future `--verify-tolerance` flag is
  easy to add when needed.
- **No `--verify` outside `--replay`** — what would it compare
  against? Could be added if a "verify two saves" tool ever makes
  sense, but that's not the current goal.
- **No mismatch-list truncation** — if 1000 fields differ, all 1000
  print. Reviewer can pipe to `head` if needed.
- **No save format change.** Schema stays v9.
- **No new `state.logs` entry.** Verify is observation-only;
  doesn't touch state.
- **No M1 system change.** All gameplay systems untouched.

## 8. Cross-links

- M2.8 (`m2-8-replay-cli.md`) — `--replay` flow that M2.11 extends.
- M2.10 (`m2-10-state-comparison.md`) — `compare_states` primitive
  M2.11 invokes; `StateMismatch` is the field-path shape M2.11
  reports.
- M0.9 (`m0-9-runner.md`) — `parse_args` / `run` / stdout summary
  layout M2.11 plugs into.

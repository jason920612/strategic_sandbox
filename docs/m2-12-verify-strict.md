# M2.12 - Replay strict mode

Companion notes for `feature/m2-12-verify-strict`. M2.12 adds the
`--verify-strict` flag, which makes the process exit `EXIT_FAILURE`
when M2.11's `--verify` detects mismatches. The full mismatch list
is still printed to stdout before the non-zero exit so CI logs
capture every divergence.

This is the smallest possible step from "informational verify" to
"CI-gateable verify". One flag, one main() exit-code branch. No
library-level behaviour change.

## 1. Scope

M2.11 left exit code at 0 regardless of mismatch count. That's
fine for interactive use but unusable for CI gating: a green build
that silently replayed a divergent save is the exact scenario
strict mode exists to catch.

M2.12 ships:

- one new boolean flag (`--verify-strict`),
- one `parse_args` validation (`--verify-strict` requires
  `--verify`),
- one `main()` exit-code branch after the existing summary print.

That's the whole PR. No save format change. No new state field
(beyond `RunnerOptions::verify_strict`). No `run()` behaviour
change. No new CLI flags beyond strict.

## 2. Public API

`include/leviathan/systems/runner.hpp`:

```cpp
struct RunnerOptions {
    // ... existing flags ...
    bool verify        = false;   // M2.11; requires --replay
    bool verify_strict = false;   // M2.12; requires --verify
};
```

`parse_args` rejects `--verify-strict` without `--verify`:

```
--verify-strict requires --verify (strict mode is a policy on top
of the verify step; without --verify there are no mismatches to
gate on)
```

Flag-chain: `--verify-strict` → `--verify` → `--replay`. The
flag-chain failure messages always name both adjacent flags so
the user can fix the missing dependency in one read.

## 3. Architectural decision: `run()` unchanged

`run()` still returns success when the simulation + replay
completes. **Strict mode is a `main()`-level exit-code policy**,
not a `Result::failure` from the library.

Why:

- The simulation finished. The artefacts (save / JSONL / CSV)
  were written. From the library's perspective, nothing went
  wrong — only the verification step found a difference.
- Treating mismatch as `Result::failure` would force the caller
  to handle two distinct failure shapes (real failures vs
  verification failures). Keeping `run()` clean lets every caller
  use the same success path.
- `main()` is the right layer to decide "this is a build-gate
  policy, exit non-zero". Other consumers (tests, future
  embedders) can apply their own policy.

The tradeoff is one more line of policy logic in `main()`. The
benefit is library/CLI separation stays clean.

## 4. `main()` exit logic

After the existing summary block (which already prints all
mismatch bullets when `--verify` is set), `main()` checks:

```cpp
if (opts.verify_strict && !outcome.verify_mismatches.empty()) {
    std::cout << "Strict mode: failing run on "
              << outcome.verify_mismatches.size()
              << " mismatch(es).\n";
    return EXIT_FAILURE;
}
```

Important: the mismatch list prints **before** the strict failure
message. CI logs get the full forensic detail (which fields
diverged, by how much) plus the explicit "we failed because of
strict mode" line — both in one run.

Exit code stays 0 when:

- `--verify-strict` is unset (M2.11 behaviour), OR
- `--verify-strict` is set but `verify_mismatches` is empty.

## 5. CI example

```bash
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   golden/run_2026_q1.json \
    --verify --verify-strict \
    --output   replay_out/
echo "exit code: $?"   # 0 if clean replay; non-zero on drift
```

Pipe into your CI's `set -e` / `script` runner. Drift fails the
job; the log still contains the per-field paths so the developer
can pinpoint the regression.

## 6. Tests

5 new doctest cases (M2.11 was 537 → M2.12 is 542):

- **`parse_args: --verify-strict plumbed`** (combined with
  `--verify --replay`): `opts.verify_strict == true`.
- **`parse_args: --verify-strict defaults false`** when absent.
- **`parse_args: --verify-strict without --verify rejected`**:
  error message contains both flag names.
- **`run: --verify-strict on matching source succeeds`** with
  `verify_mismatches.empty()`. Confirms strict didn't accidentally
  alter `run()` semantics on the happy path.
- **`run: --verify-strict on tweaked source still succeeds at
  `run()`**: `Result` ok, mismatches populated. Confirms strict
  mode is `main()`'s responsibility — `run()` is unchanged.

Exit-code testing is not done at the doctest layer (would require
spawning the binary). The behaviour is small enough that the
unit-tested option/outcome state is sufficient evidence.

## 7. What's NOT in scope

Deliberate non-goals:

- **No `--verify-tolerance` CLI knob** — still uses M2.10's
  default `1e-9`. M2.13 candidate.
- **No structured diff output** (machine-readable JSON,
  per-category buckets, etc.). The text bullets are enough for
  CI grep / human reading.
- **No save format change.**
- **No new `state.logs` lifecycle entry.**
- **No M1 system change.**
- **No `--verify-strict` outside `--replay --verify`** — would be
  a no-op flag.
- **No mismatch-count threshold** (e.g. "fail only if > 5
  mismatches"). Strict mode is binary: any mismatch is a failure.

## 8. Cross-links

- M2.10 (`m2-10-state-comparison.md`) — `compare_states` is the
  primitive whose output strict mode acts on.
- M2.11 (`m2-11-replay-verify.md`) — `--verify` flag M2.12 sits
  on top of; same flag-chain validation pattern.
- M0.9 (`m0-9-runner.md`) — `parse_args` / `run` / `main()` shape.

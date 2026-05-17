# M2.13 - Verify tolerance CLI

Companion notes for `feature/m2-13-verify-tolerance`. M2.13 adds a
single CLI knob — `--verify-tolerance FLOAT` — that overrides the
`CompareOptions::double_tolerance` value M2.10 hard-defaults to
`1e-9`. Requires `--verify`. No other behaviour changes.

This is the last anticipated ergonomics flag in the M2 replay
family. After M2.13, the replay flow has:
- `--replay PATH` (M2.8) — what to replay
- `--verify` (M2.11) — auto-compare result vs source
- `--verify-strict` (M2.12) — CI exit-code gate
- `--verify-tolerance FLOAT` (M2.13) — tune the gate

## 1. Scope

The M2.10 default tolerance (`1e-9`) is tight by design — it matches
M0.8's save round-trip precision. That's the right default for
"did this replay reproduce the original?" but too tight when
cumulative drift over a long simulation introduces legitimate
small differences.

M2.13 adds one CLI flag to override the default at the call site.
No library behaviour changes; M2.10's `CompareOptions::double_tolerance`
field has been the right shape all along.

That's the whole PR. One flag, one parse helper, one plumbing call,
one stdout line.

## 2. Public API

`include/leviathan/systems/runner.hpp`:

```cpp
struct RunnerOptions {
    // ...
    std::optional<double> verify_tolerance;   // M2.13
};
```

`std::nullopt` means "use the M2.10 default (1e-9)". Any
non-negative finite double override is supplied via
`--verify-tolerance N`.

`parse_args` validations:
- Requires `--verify` (no effect without it).
- Value must be parseable as a finite double (`std::strtod` with
  full-consumption check).
- Value must be `>= 0`.

Flag-chain: `--verify-tolerance` → `--verify` → `--replay`. Each
violation fails loudly with both adjacent flag names in the error.

## 3. Implementation

### Parse helper

A new private `parse_nonneg_double(text, flag)` lives next to
`parse_uint64` / `parse_positive_int`. Exception-free; uses
`std::strtod` with full-string-consumption and finiteness checks.

```cpp
Result<double> parse_nonneg_double(string_view text, string_view flag) {
    if (text.empty()) return failure("value is empty");
    const string buf(text);
    char* end = nullptr;
    const double v = std::strtod(buf.c_str(), &end);
    if (end == buf.c_str() || *end != '\0') return failure("not a float");
    if (!std::isfinite(v)) return failure("not finite");
    if (v < 0.0) return failure(">= 0");
    return success(v);
}
```

Rejects: empty string, garbage characters (`"1.5x"`), trailing
content, `NaN` / `Inf` (`isfinite` check), and negative values.

### Plumb into `compare_states`

In `run()`'s replay branch:

```cpp
if (opts.verify) {
    diagnostics::CompareOptions cmp_opts;
    if (opts.verify_tolerance.has_value()) {
        cmp_opts.double_tolerance = opts.verify_tolerance.value();
    }
    outcome.verify_mismatches =
        diagnostics::compare_states(state, loaded, cmp_opts);
}
```

The struct default still fires when the user doesn't pass the flag,
so backward compatibility is automatic.

### main() stdout

When `--verify-tolerance` is set, `main()` prints one extra line
in the verify block:

```
Verify mismatches   : 2
  - countries[0].gdp : ...
  - countries[0].budget.military : ...
Verify tolerance    : 0.001
```

The line is informational — CI logs see exactly which tolerance
produced the reported mismatch count.

## 4. CLI examples

```bash
# Default 1e-9 tolerance (M2.10 behaviour, unchanged):
leviathan --scenario foo --replay src.json --verify ...

# Loosen to 1e-6 for a long simulation with legitimate drift:
leviathan --scenario foo --replay src.json --verify \
          --verify-tolerance 1e-6 ...

# Strict mode + custom tolerance:
leviathan --scenario foo --replay golden.json \
          --verify --verify-strict \
          --verify-tolerance 1e-4 ...
```

## 5. Tests

8 new doctest cases (M2.12 was 542 → M2.13 is 550):

`parse_args`:
- **`--verify-tolerance plumbed`** (with `--verify --replay`):
  `opts.verify_tolerance == 1e-3`.
- **`--verify-tolerance defaults nullopt`** when absent.
- **`--verify-tolerance without a value rejected`** with the flag
  name in the error.
- **`--verify-tolerance non-numeric rejected`** (`"abc"`) with the
  flag name + bad value + "floating-point" in the error.
- **`--verify-tolerance negative rejected`** (`-1e-3`) with `">= 0"`
  in the error.
- **`--verify-tolerance without --verify rejected`** with both flag
  names in the error.

`run()` end-to-end:
- **`Loose tolerance absorbs a small mismatch`**: build source via
  the existing `build_source_save` helper, tweak `gdp` by `+1e-3`,
  re-save. Run with `--verify --verify-tolerance 1e-2`. The
  `countries[0].gdp` path is NOT in the mismatch list (loose
  tolerance silently accepts the diff).
- **`Tight tolerance catches the same mismatch`**: same source,
  `--verify-tolerance 1e-6`. The `countries[0].gdp` mismatch
  appears.

## 6. What's NOT in scope

Deliberate non-goals:

- **No relative tolerance**. The M2.10 `compare_states` only
  supports absolute tolerance; M2.13 inherits that. The state's
  numeric ranges (mostly `[0, 1]` ratios + GDP in the low
  hundreds) make absolute tolerance adequate.
- **No per-field tolerance**. One tolerance value applies to
  every numeric field in the walk. Per-field would require a
  bigger `CompareOptions` shape and a parser format; deferred
  until a real need shows up.
- **No save format change** — schema stays v9.
- **No new gameplay**.
- **No new `state.logs` entry**.
- **No M1 system change**.

## 7. Cross-links

- M2.10 (`m2-10-state-comparison.md`) — `compare_states` +
  `CompareOptions`; M2.13 plumbs into the existing
  `double_tolerance` field.
- M2.11 (`m2-11-replay-verify.md`) — `--verify` flag M2.13
  requires.
- M2.12 (`m2-12-verify-strict.md`) — `--verify-strict`; M2.13
  composes cleanly (loosen tolerance, then gate strictly on
  whatever remains).
- M0.9 (`m0-9-runner.md`) — `parse_args` / `run` / `main()`
  shape.

# M0.5 - RNG service design notes

Companion notes for `feature/m0-05-rng`. Records the algorithm, the
edge-case rules, and the determinism guarantees that the rest of the
project will treat as load-bearing.

## 1. Why we do not use `<random>` directly

The C++ standard `<random>` library has two problems for a deterministic
multi-platform simulation:

1. **`std::random_device` is non-deterministic by design.** It is a
   black box that may or may not be backed by OS entropy. Calling it
   would silently break replay.
2. **`std::uniform_int_distribution` and `std::uniform_real_distribution`
   are implementation-defined across vendors.** Same `std::mt19937`
   state plus same distribution parameters yield *different* outputs
   on MSVC vs libstdc++ vs libc++. A save file produced on Windows
   would therefore not replay identically on Linux.

Both are banned in the simulation core. The RNG service exposes the
only sanctioned entry points; lint / review should catch any direct
`<random>` use that sneaks in later.

## 2. Algorithm

Counter-indexed splitmix64 finaliser on `(seed, counter)`.

```cpp
rng.counter += 1;
uint64_t z = rng.seed + rng.counter * 0x9E3779B97F4A7C15ull;
z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
return z ^ (z >> 31);
```

Properties:

- **Counter-based.** Any draw is reproducible from `(seed, counter)`
  alone, no need to replay the preceding sequence. This unlocks
  things like "give me the 1000th draw" or "fork a sub-stream by
  seeding from a draw".
- **Cross-platform deterministic.** Every operation is on `uint64_t`
  with well-defined overflow semantics, and the only floating-point
  step (`top 53 bits / 2^53` in `draw_unit`) is exact in IEEE-754.
  No `std::*_distribution` involved.
- **Fast.** Three multiplies and three xor-shifts per draw, all
  64-bit ALU ops. The `mix` function and `next_u64` are leaf
  functions that the compiler inlines comfortably.

The constants are Sebastiano Vigna's published splitmix64 finalizer
(public domain). They are well studied and pass standard PRNG tests
(BigCrush, PractRand) for our use cases. We are *not* claiming
cryptographic quality - never use this for keys / nonces / passwords.

## 3. Edge-case rules

`weighted_choice`:

| Input | Behaviour |
|-------|-----------|
| Empty vector | Precondition violation - asserts |
| Negative weight | Precondition violation - asserts |
| Non-finite weight (NaN, Inf) | Precondition violation - asserts |
| All weights zero | Returns 0. **Consumes one `draw_unit` anyway** so the counter advances identically to the normal path. Asserts do *not* fire because zero is a non-negative finite value. |
| One element | Always returns 0; one draw consumed. |
| Mixed zero and non-zero | Zero-weighted indices are never selected. |

`draw_bool`:

| `probability` | Behaviour |
|---------------|-----------|
| In `[0, 1]` | True with that probability. |
| Below 0 | Clamped to 0; always returns false. |
| Above 1 | Clamped to 1; always returns true. |
| NaN | Treated as 0 (NaN-safe; the `std::clamp` chain would otherwise propagate NaN). |
| **Any** | Consumes exactly one draw. |

`draw_int`:

| Input | Behaviour |
|-------|-----------|
| `min > max` | Precondition violation - asserts |
| `min == max` | Always returns that value; one draw consumed. |
| `[INT_MIN, INT_MAX]` | Works (arithmetic is performed in `int64_t`). |
| Otherwise | Uniform integer in `[min, max]` (inclusive). |

Modulo bias note: `draw_int` uses `raw % span`. The bias is at most
`span / 2^64`, which is vanishingly small for any range we'd realistically
use (e.g. `span = 1000` yields a bias of `5.4 * 10^-17`). If a future
caller genuinely needs unbiased uniformity (e.g. shuffle in a security
context), it should layer rejection sampling on top of `next_u64`.

## 4. Tag parameter and trace hook

Every draw function takes an optional `std::string_view tag` defaulting
to `""`. The tag is a debug label, **not** part of the determinism
contract: same seed + same call order yields identical draws regardless
of tag.

Why bother carrying it then? Because we *will* want to debug
non-determinism eventually (a save file that replays differently than
expected). At that point, swapping in a trace callback that records
each draw's `(tag, counter, raw)` lets us diff two runs and find
exactly where they diverged - and what category of draw caused it.

The hook is process-global and `nullptr` by default. In the hot path
it costs one branch per draw. Tests demonstrate the round-trip:
install a hook, do some draws, check that the recorded raw values
match what `next_u64` would have produced from `(seed, original_counter)`.

## 5. Determinism guarantee

Given:
- the same algorithm (the one in this file)
- the same `RandomState{seed, counter}` snapshot
- the same call sequence (same functions in same order, same parameters)

The outputs are **bit-identical**. This holds across MSVC, GCC, Clang,
Windows / Linux / macOS, debug / release, and optimisation levels.

If we ever change the algorithm, identical `(seed, counter)` may yield
different draws. That is the open question flagged in
`include/leviathan/core/random_state.hpp`. The likely fix is one of:

- An `algorithm_version` field in `RandomState` (and saves) so old
  saves keep using the old algorithm.
- A versioned trait class threaded through the service.

M0.8 (SaveSystem) is the natural place to make that call.

## 6. Naming gotcha worth remembering

doctest interprets `[xyz]` inside a `TEST_CASE` name as a **tag**, not
literal text. Two intuitively-correct test names broke CTest discovery
mid-development of this PR:

- `TEST_CASE("draw_unit stays in [0, 1)")` ❌
- `TEST_CASE("draw_double respects [min, max) ...")` ❌

Renamed to:

- `TEST_CASE("draw_unit stays in 0-to-1 exclusive range")` ✓
- `TEST_CASE("draw_double respects min-to-max range and produces spread")` ✓

Lesson for future tests: avoid `[` and `]` in test-case names; use
words instead. Round parens `(` and `)` are safe.

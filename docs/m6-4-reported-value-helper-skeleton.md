# M6.4 - reported_value helper skeleton

Companion notes for
`feature/rfc090-m6-04-reported-value-helper-skeleton`.

**This is RFC-090 M6** (hidden truth / information distortion),
fourth sub-milestone. M6.4 implements **only** RFC-090 §6.4:

> 6.4 實作 reported value

A small new free-function helper module:

```cpp
namespace leviathan::systems::reported_value {

core::Result<double> from_true_value(double true_value,
                                     double accuracy);

}
```

M6.4 ships the **helper SHAPE only** — the body is a placeholder
formula:

```text
reported_value = true_value * accuracy
```

At `accuracy = 1.0` (the M6.3 `kPlaceholderInformationAccuracy`
ceiling), reported equals `true_value` verbatim — the player
sees the truth. At `accuracy = 0.0`, reported equals 0 — the
player sees nothing useful. Intermediate values linearly
interpolate between truth and nothing.

## 1. Composition with the M6 pipeline

```text
M6.3 information_accuracy::compute_for_country(state, country)
       -> double accuracy in [0, 1]
                                |
                                v
M6.4 reported_value::from_true_value(true_value, accuracy)
       -> double reported_value
                                |
                                v
M6.5 bias / noise (future)      adds RNG distortion on top
M6.9 non-debug mode (future)    uses reported_value to hide
                                visible_report toward true_cause
M6.8 debug mode (future)        bypasses M6.4 (shows truth)
```

**M6.4 does NOT modify M6.3's `compute_for_country` body** —
per the PR #102 reviewer's discipline note:

> M6.4 應避免改 formula body.

The M6.3 placeholder (`return kPlaceholderInformationAccuracy
= 1.0`) stays verbatim. M6.4 is purely additive: a new module
alongside M6.3.

## 2. Scope

What ships:

```text
leviathan::systems::reported_value                          (new module)
   from_true_value(true_value, accuracy) -> Result<double>  (new free fn)
include/leviathan/systems/reported_value.hpp                (new header)
src/leviathan/systems/reported_value.cpp                    (new impl)
tests/systems/reported_value_test.cpp                       (16 new doctest cases)
CMakeLists updates for the new sources
```

What does NOT change:

```text
no save format bump (still v16)
no new state field
no new artefact (still 10)
no new runner CLI flag / RunnerOptions field
no events.jsonl semantic change
no new PlayerCommandKind
no scenario_loader change
no event_evaluator / event_firer / event_effects /
   event_engine / monthly_pipeline / runner code change
no information_accuracy module body change
   (M6.3 placeholder stays at 1.0)
no canonical fixture change (the helper is opt-in by
   caller; no system invokes it yet)
no M6.5+ work
no M1/M2/M3/M4/M5 / M6.1 / M6.2 / M6.3 systems' external
   behaviour change
no rebake of M1.17 / M2 / M3 / M4 / M5 byte-identical
   determinism baselines (no code path consumes this helper
   yet; M6.5+ / M6.9 will)
```

## 3. API shape

```cpp
namespace leviathan::systems::reported_value {

core::Result<double> from_true_value(double true_value,
                                     double accuracy);

}
```

Three reasons the API is one pure function over two doubles:

- **No GameState parameter.** Unlike M6.3 (which takes
  `state, country` because the future M6.6 / M6.7 will read
  per-country fields), M6.4 operates on already-extracted
  numeric values. Country-level wiring is the caller's job;
  M6.4 stays orthogonal to GameState shape.
- **`Result<double>` not a sentinel.** Same rationale as M6.3:
  NaN breaks determinism, asserts crash, sentinels (like -1.0)
  leak into formulas. `Result::failure` lets the caller
  branch cleanly on bad inputs.
- **No event-specific signature.** A future `from_event(state,
  instance)` convenience wrapper that resolves the country
  + the trigger / effect value automatically belongs alongside
  the M6.9 hiding pipeline — M6.4 ships only the primitive.

## 4. Why the placeholder formula is `true_value * accuracy`

Two candidate shapes were considered:

- **Pure pass-through**: `reported_value = true_value` regardless
  of accuracy. Honest about "no formula yet" but doesn't use
  accuracy at all — contradicts the spec ("把 true value +
  information_accuracy 轉成 reported value").
- **Linear interpolation toward 0**: `reported_value =
  true_value * accuracy`. Uses both inputs; at accuracy=1.0
  reported equals truth (matches M6.3 ceiling); at
  accuracy=0.0 reported equals 0 (player sees nothing
  useful); intermediate values blend monotonically. Negative
  true_values (e.g. an effect's -0.02) damp toward 0
  correctly.

M6.4 picks the linear interpolation. The "toward 0" choice
also reflects the natural fallback: if you don't know the
truth, you should default to "no information" (zero magnitude)
rather than some neutral midpoint that would itself be a
biased guess.

M6.5 (bias / noise) will wrap this with RNG-driven distortion
on top — the formula body of M6.4 stays the linear path; M6.5
ADDS randomness. M6.4's pure-deterministic shape is the
no-RNG floor.

## 5. Validation

Both inputs are validated:

```text
true_value : required finite double
accuracy   : required finite double in [0, 1]
```

Failure messages include the offending value formatted for
NaN / ±∞ clarity. Mirrors the M1.5 / M5.6 effect-value
validation style.

## 6. Tests added (16)

- **Happy path (4)**: accuracy=1.0 returns true_value
  verbatim (M6.3 ceiling); accuracy=0.0 returns 0
  regardless of true_value (positive / negative / large);
  midpoint accuracy=0.5 returns half of true_value;
  true_value=0 returns 0 for any valid accuracy.
- **Monotonicity (1)**: higher accuracy → higher
  reported_value for a fixed positive true_value.
- **Negative true_value (2)**: -0.02 (canonical M5.1 effect)
  round-trips at accuracy=1.0; damps toward 0 at low
  accuracy.
- **Validation (6)**: NaN / ±∞ true_value rejected;
  NaN / ±∞ accuracy rejected; accuracy below 0 rejected;
  accuracy above 1 rejected.
- **Determinism (1)**: three repeated identical-input
  calls return identical values.
- **Composition with M6.3 (2)**: M6.3 → M6.4 pipeline returns
  true_value verbatim under M6.3's current placeholder
  accuracy=1.0 (trigger threshold 0.30 stays 0.30); failed
  M6.3 lookup is checkable BEFORE M6.4 is called (M6.4 itself
  with valid inputs still succeeds).

All 1074 M6.3-era tests still pass byte-identically.

Total: **1090 doctest cases / 62522 assertions** (was 1074 /
62459 at M6.3 close).

## 7. What's deferred (the M6.5+ horizon, per RFC-090)

```text
M6.5  bias / noise         (randomised distortion that wraps
                            M6.4; M6.5 introduces RNG, M6.4
                            stays deterministic)
M6.6  intelligence budget  (reduces M6.3 accuracy below 1.0;
                            no change to M6.4 body)
M6.7  corruption           (reduces M6.3 accuracy below 1.0;
                            no change to M6.4 body)
M6.8  debug mode           (bypasses M6.4 entirely)
M6.9  non-debug mode       (uses M6.4 result to hide
                            visible_report toward true_cause)
```

Each is its own dedicated sub-milestone PR. M6.4 ships the
deterministic core that M6.5–M6.9 compose around.

## 8. What M6.4 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no save format bump (still v16)
no new state field
no new artefact (still 10)
no new RunnerOptions field / CLI flag
no new PlayerCommandKind
no scenario_loader change
no event-module / monthly_pipeline / runner code change
no information_accuracy body change
   (M6.3 placeholder stays at 1.0; M6.4 only CALLS
    compute_for_country in tests + design — never modifies
    its body, per PR #102 reviewer rule)
no canonical fixture change
no consumer in any current system —
   the helper is callable but no one calls it yet
no event-aware variant (from_event) — that's M6.9 scope
no bias / noise (M6.5)
no intelligence-budget formula (M6.6)
no corruption formula (M6.7)
no debug mode (M6.8)
no non-debug hiding (M6.9)
no EventReport type / artefact
no events.jsonl semantic change
no UI / map integration
no balance pass
no RNG draws (the helper is deterministic; M6.5 will
   introduce RNG when it lands)
no rebake of M1.17 / M2 / M3 / M4 / M5 byte-identical
   determinism baselines (no consumer = no determinism
   drift)
no M6.5 work
no docs/milestone-6-checkpoint.md
no docs/milestone-6-result.md
no "M6 closed" wording
```

M6 remains in progress. M6.4 ships the reported-value
primitive; the next sub-milestone is M6.5 (`bias / noise`)
per RFC-090 §6.5, but M6.5 is **unspec'd here** and waits
for an explicit `做M6.5` go-ahead.

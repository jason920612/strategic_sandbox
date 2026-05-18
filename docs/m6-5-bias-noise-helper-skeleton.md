# M6.5 - bias_noise helper skeleton

Companion notes for
`feature/rfc090-m6-05-bias-noise-helper-skeleton`.

**This is RFC-090 M6** (hidden truth / information distortion),
fifth sub-milestone. M6.5 implements **only** RFC-090 §6.5:

> 6.5 實作 bias/noise

A small new free-function helper:

```cpp
namespace leviathan::systems::bias_noise {

inline constexpr double kPlaceholderNoiseAmplitude = 0.0;

core::Result<double> sample_for_event(
    const std::string&     event_id_code,
    const std::string&     country_id_code,
    const core::GameDate&  fired_on,
    double                 amplitude = kPlaceholderNoiseAmplitude);

}
```

M6.5 produces a deterministic noise value in
`[-amplitude, +amplitude]` from a stable identifier triple
(event id_code, country id_code, fire date). **Default
amplitude = 0.0** (placeholder fast path returns 0). Callers
that opt into noise pass a positive amplitude in `[0, 1]`
explicitly.

## 1. The load-bearing design decision: deterministic hash, NOT `state.rng`

This is M6.5's most important call. The PR #103 reviewer
flagged it explicitly:

> 我會偏好 M6.5 先做 deterministic hash noise helper，不要
> 消耗 state.rng，除非你明確想讓資訊失真進入 simulation RNG
> stream。

**M6.5 honours that.** Reasoning preserved here for future-me:

- `state.rng` is the only mutable RNG state in `GameState`.
  If M6.5 consumed it via `random_service::next_double(...)`,
  every event fire would advance `state.rng.counter`,
  shifting every downstream RNG draw by any future
  RNG-using system.
- That shift would perturb M5 / M1.17 / M2 / M3 / M4
  byte-identical determinism baselines. Either each future
  PR rebakes them, or M6.5's noise gets ripped out.
- **Deterministic hash on stable inputs** (event id_code,
  country id_code, fire date) is purely a function of inputs
  known at fire time. Two identical fires in two identical
  states produce identical noise — same shape as if the
  event report was prepared "the same way last month".

## 2. Composition with the M6 pipeline

```text
M6.3 information_accuracy::compute_for_country(state, country)
       -> double accuracy in [0, 1]

M6.4 reported_value::from_true_value(true_value, accuracy)
       -> double reported  (linear interpolation toward 0)

M6.5 bias_noise::sample_for_event(event_id, country_id,
                                  fired_on, amplitude)
       -> double noise in [-amplitude, +amplitude]

final_visible = M6.4 reported + M6.5 noise              (M6.9)
```

M6.5 itself **never** combines with M6.4. The caller (M6.9
non-debug hiding) owns the `reported + noise` sum. M6.5 is
the pure noise primitive.

## 3. Scope

What ships:

```text
leviathan::systems::bias_noise                          (new module)
   kPlaceholderNoiseAmplitude = 0.0                     (public constant)
   sample_for_event(...) -> Result<double>              (new free fn)
include/leviathan/systems/bias_noise.hpp                (new header)
src/leviathan/systems/bias_noise.cpp                    (new impl)
tests/systems/bias_noise_test.cpp                       (17 new doctest cases)
CMakeLists updates for the new sources
```

What does NOT change:

```text
no save format bump (still v16)
no new state field
no `state.rng` consumption (the load-bearing decision)
no new artefact (still 10)
no new runner CLI flag / RunnerOptions field
no events.jsonl semantic change
no new PlayerCommandKind
no scenario_loader change
no event_evaluator / event_firer / event_effects /
   event_engine / monthly_pipeline / runner code change
no random_service code change (M6.5 reimplements
   splitmix64 finalize locally rather than depending on
   the RNG service's private finalize — see §5)
no canonical fixture change (the helper is opt-in by
   caller; no system invokes it yet)
no M6.6+ work
no consumer wired — M6.9 (non-debug hiding) will be the
   first caller to pass a non-zero amplitude
no rebake of M1.17 / M2 / M3 / M4 / M5 byte-identical
   determinism baselines
```

## 4. Hash design (FNV-1a + splitmix64)

The implementation uses two well-known integer-arithmetic
primitives:

- **FNV-1a 64-bit** to mix the string inputs (event id_code,
  country id_code) byte-by-byte into a running uint64 hash.
  Standard constants (offset basis `0xcbf29ce484222325`,
  prime `0x100000001b3`).
- **splitmix64 finalize** (Sebastiano Vigna, public domain)
  to scramble the final hash. Three mix steps of XOR-shift +
  multiply. Identical algorithm to the M0.5 RNG service's
  private `finalize` function, but reimplemented locally so
  `bias_noise` has zero coupling to `random_service`.

Why not `std::hash<std::string>`:

- Implementation-defined output across compilers / libstdc++
  versions. M6.5 needs cross-build determinism (so a save
  generated on Windows + MSVC matches one generated on Linux
  + GCC), so `std::hash` is rejected.

Why not depend on the RNG service's finalize:

- The RNG service's `finalize` is in an anonymous namespace
  (private to `random_service.cpp`). Exposing it as a public
  function would couple `bias_noise` to the RNG service's
  module layout. The function is 3 lines; reimplementing it
  here keeps both modules independently testable.
- A future refactor PR could extract a shared `internal/mix64.hpp`
  for both sites if useful. M6.5 doesn't ship that refactor.

### Key composition

```cpp
hash = FNV1a(event_id_code, offset_basis)
hash = FNV1a_mix_byte(0, hash)                   // NUL separator
hash = FNV1a(country_id_code, hash)
hash = FNV1a_mix_u64(year*10000 + month*100 + day, hash)
hash = splitmix64_finalize(hash)
```

The **NUL separator** between `event_id_code` and
`country_id_code` is essential — without it, `"abcd"+"ef"`
and `"abc"+"def"` would hash identically. A test
(`"abcd"+"ef" != "abc"+"def"`) pins the separator's effect.

The **date packed as `year*10000 + month*100 + day`** gives a
stable byte-order-independent representation of the date.
The 64-bit packing easily fits up to year 999999.

## 5. Output scaling to `[-amplitude, +amplitude]`

```cpp
unit     = (hash >> 11) / 2^53        // -> [0, 1)
centered = unit - 0.5                  // -> [-0.5, +0.5)
noise    = centered * 2.0 * amplitude  // -> [-amplitude, +amplitude)
```

Using the top 53 bits maps cleanly into a double's mantissa
without losing precision. Same trick `std::uniform_real_distribution`
uses, hand-rolled so the output is deterministic across
compilers.

Tests pin: `noise` is finite, within `[-amplitude, +amplitude]`,
and stable across consecutive calls with identical inputs.

## 6. Placeholder amplitude fast path

```cpp
if (amplitude == kPlaceholderNoiseAmplitude) {   // == 0.0
    return Result::success(0.0);
}
```

This short-circuits the hash entirely when amplitude is
exactly 0. Two reasons:

- **Performance**: skip the hash for the no-op case (M6.5
  era — no caller passes non-zero amplitude yet).
- **Clarity in tests**: callers that don't care about noise
  yet see a clean 0 return without depending on the hash
  output happening to be near zero for some specific input.

`amplitude * hash_in_[-0.5, 0.5)` would still produce 0 if
the amplitude were `0.0`, so this fast path is purely an
optimisation + clarity choice, not a behavioural one.

## 7. Validation

```text
event_id_code   : required non-empty string
country_id_code : required non-empty string
amplitude       : required finite double in [0, 1]
```

Failure messages include the offending value formatted for
NaN / ±∞ / out-of-range clarity. Mirrors the M6.3 /
M6.4 / M1.5 validation style.

## 8. Tests added (17)

- **Placeholder fast path (3)**: default amplitude returns 0;
  explicit amplitude=0 returns 0; `kPlaceholderNoiseAmplitude`
  is `0.0`.
- **Happy path (2)**: result is in `[-amplitude, +amplitude]`
  for various dates at amplitude 0.05; amplitude=1.0 produces
  values in `[-1, +1]`.
- **Determinism (1)**: three repeated identical-input calls
  return the same noise.
- **Input separation (4)**: different event_id_code,
  country_id_code, and fired_on each produce different noise;
  NUL-separator test pins that `"abcd"+"ef" != "abc"+"def"`.
- **Validation (6)**: empty event_id_code / country_id_code
  rejected; NaN / ±∞ amplitude rejected; amplitude<0 /
  amplitude>1 rejected.
- **Cross-build stability (1)**: pinned exact output value
  for canonical inputs (`low_stability_unrest`, `GER`,
  `1930-03-15`, amplitude=0.1 → 0.00128381...). Any future
  PR that retunes the hash formula will trip this test and
  must rebake the expected value + document the rebake in
  the design note.

All 1090 M6.4-era tests still pass byte-identically.

Total: **1107 doctest cases / 62585 assertions** (was 1090 /
62522 at M6.4 close).

## 9. What's deferred (the M6.6+ horizon, per RFC-090)

```text
M6.6  intelligence budget    (reduces M6.3 accuracy below 1.0;
                              no change to M6.5 amplitude)
M6.7  corruption             (reduces M6.3 accuracy below 1.0;
                              no change to M6.5 amplitude)
M6.8  debug mode             (bypasses M6.4 + M6.5 entirely;
                              player sees truth)
M6.9  non-debug mode         (composes M6.4 + M6.5 into the
                              final visible_report; first
                              caller to pass non-zero
                              amplitude to M6.5)
```

Each is its own dedicated sub-milestone PR. M6.5 ships the
deterministic noise primitive that M6.9 will consume.

## 10. What M6.5 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no save format bump (still v16)
no new state field
no state.rng consumption (the load-bearing call)
no new artefact (still 10)
no new RunnerOptions field / CLI flag
no new PlayerCommandKind
no scenario_loader change
no event-module / monthly_pipeline / runner code change
no random_service change (splitmix64 finalize is
   reimplemented locally; the RNG service is untouched)
no canonical fixture change
no consumer in any current system —
   helper callable but unused; M6.9 will start using it
no event-aware variant (sample_for_event_instance that
   resolves country_id_code from the instance's first
   actor) — that's M6.9 scope
no automatic composition with M6.4 reported value — M6.9
   owns the reported + noise sum
no intelligence-budget weighting (M6.6 modulates upstream
   accuracy, not M6.5 amplitude)
no corruption weighting (M6.7, same reason)
no debug mode (M6.8)
no non-debug hiding (M6.9)
no EventReport type / artefact
no events.jsonl semantic change
no UI / map integration
no balance pass
no rebake of M1.17 / M2 / M3 / M4 / M5 byte-identical
   determinism baselines
no M6.6 work
no docs/milestone-6-checkpoint.md
no docs/milestone-6-result.md
no "M6 closed" wording
```

M6 remains in progress. M6.5 ships the deterministic noise
primitive; the next sub-milestone is M6.6 (`intelligence
budget influence`) per RFC-090 §6.6, but M6.6 is **unspec'd
here** and waits for an explicit `做M6.6` go-ahead.

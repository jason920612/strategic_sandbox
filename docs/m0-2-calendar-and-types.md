# M0.2 — Calendar and core type decisions

Companion notes for `feature/m0-02-core-types`. Locks in the design choices
that the rest of Milestone 0 (and the rest of the simulation core) will rely
on, so future PRs don't have to re-litigate them.

## 1. Calendar: proleptic Gregorian, real leap years

`GameDate` uses the proleptic Gregorian calendar.

Leap-year rule (verbatim from `GameDate::is_leap_year`):

```cpp
(year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)
```

`days_in_month()` returns 29 for February in leap years, 28 otherwise, and
the usual 30/31 for the rest.

### Why real leap years from M0.2

The prototype's headline timeline is **1930 → 2000**, which contains 18
ordinary leap years (1932, 1936, ..., 1996), one ordinary non-leap century
(1900 — outside range but adjacent), and one century-divisible-by-400 leap
(2000). If we shipped a simplified 365-day-only calendar we would either:

- Have to re-derive every date-driven test once we switch to real years, or
- Quietly drift one day per leap year (≈ 18 days off by 1996, ≈ 19 by 2000),
  which is exactly the kind of silent miscalibration that breaks
  deterministic replays.

Real Gregorian arithmetic is also cheap — a single `is_leap_year` branch in
`days_in_month` — so there's no performance argument for a simplification.

### What is explicitly NOT supported

- **BC / proleptic negative years.** `is_valid()` rejects `year < 1`. The
  prototype timeline never needs them and rejecting them up front catches
  arithmetic underflow.
- **Sub-day resolution.** Tick base is one day, per RFC-000 §3 and RFC-060
  §4. We will not introduce hours/minutes until a feature actually requires
  them (likely never).
- **Time zones.** All dates are wall-clock dates in an unspecified single
  zone. There is no UTC offset.
- **Negative `advance_days`.** Simulation time only moves forward.
  `advance_days(-1)` is a precondition violation, not a feature request.

## 2. Strong ID types

The six domain IDs (`CountryId`, `ProvinceId`, `FactionId`, `PolicyId`,
`EventId`, `CharacterId`) are aliases of `StrongId<Tag>`, a tag-templated
wrapper around a single `int`.

```cpp
template <typename Tag>
class StrongId {
    int value_ = -1;  // -1 = invalid
public:
    constexpr StrongId() noexcept = default;
    constexpr explicit StrongId(int value) noexcept;
    // value(), valid(), ==, !=, <, <=, >, >=, invalid()
};
```

Properties:

- **Distinct types.** `CountryId` and `ProvinceId` instantiate `StrongId`
  with different empty tag structs, so they are not interchangeable. The
  compiler rejects `country_id == province_id`.
- **Explicit construction.** No implicit `int -> StrongId` conversion. You
  must write `CountryId{42}`.
- **Invalid sentinel.** Default-constructed IDs carry `-1`. `valid()` is
  the explicit check; `StrongId::invalid()` is the explicit value.
- **Hashable.** `std::hash` is specialised so `std::unordered_map<CountryId, T>`
  works out of the box (used heavily by future systems).

Underlying type is currently `int`. That gives `~2.1B` valid IDs (with `-1`
reserved), well past the world-province count of any plausible scenario.
Widening to `int64_t` later is a one-line change inside `StrongId` and is
not blocked.

## 3. Result type

`Result<T, E = std::string>` is a small variant-based success-or-error type.

Design choices:

- **No exceptions across the simulation boundary.** RFC-060 §6 implies
  deterministic, headless runs; exception unwinding plus library code is
  hostile to deterministic replays. The simulation core will return
  `Result` rather than throw.
- **`E` defaults to `std::string`.** The dominant case is "the data
  loader rejected this file, here's the human-readable message".
  Structured error types (e.g. enums, error categories) are supported
  by overriding `E`.
- **Asserts on misuse.** Calling `.value()` on a failure or `.error()` on a
  success is a programmer error and trips an `assert`. We are not using
  `std::optional`-style "empty access yields default" semantics — that
  would hide bugs.

## 4. String utilities

Deliberately one function: `string_utils::trim`. It is the only utility
`GameDate::parse` needs. Adding `split`, `starts_with`, `ends_with`, etc.
before a real caller materialises would violate the "no premature
scaffolding" rule in RFC-001 §4.

`trim` recognises the same six whitespace characters as the C locale's
`isspace` (space, tab, newline, carriage return, vertical tab, form feed)
and is **not** locale-sensitive — important for cross-platform determinism.

## 5. Test framework: doctest

doctest 2.4.11 is pulled at configure time via CMake `FetchContent`
(`GIT_SHALLOW TRUE`). doctest was chosen over GoogleTest and Catch2 because:

- Single-header, fast to compile, no link-time runtime.
- One executable with auto-discovered cases via `doctest_discover_tests`.
- Trivially easy to drop and replace if we later move to a different
  framework — its `TEST_CASE` / `CHECK` / `REQUIRE` macros are nearly
  identical to Catch2's.

Network requirement on first configure is the obvious tradeoff. Offline
builds can be supported later by setting
`FETCHCONTENT_SOURCE_DIR_DOCTEST=<vendored path>` if/when that becomes a
real constraint.

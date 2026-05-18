# M6.3 - information_accuracy helper skeleton

Companion notes for
`feature/rfc090-m6-03-information-accuracy-helper-skeleton`.

**This is RFC-090 M6** (hidden truth / information distortion),
third sub-milestone. M6.3 implements **only** RFC-090 §6.3:

> 6.3 實作 information_accuracy

A small new free-function helper module:

```cpp
namespace leviathan::systems::information_accuracy {

inline constexpr double kPlaceholderInformationAccuracy = 1.0;

core::Result<double> compute_for_country(
    const core::GameState& state,
    core::CountryId        country);

}
```

M6.3 ships the **helper SHAPE only** — the body is a placeholder
that returns 1.0 unconditionally for any valid country. Later
M6 sub-milestones add the actual distortion model:

```text
M6.4 reported value         — uses this accuracy to derive
                              numeric reported value
M6.5 bias / noise           — adds randomised distortion
M6.6 intelligence budget    — weights accuracy DOWN by
                              missing intel budget
M6.7 corruption             — weights accuracy DOWN by
                              corruption
M6.8 debug mode             — bypasses accuracy entirely
M6.9 non-debug mode         — uses computed accuracy to
                              hide visible_report toward
                              true_cause
```

**M6.3 does NOT bump the save schema.** Per the PR #101
reviewer's explicit note:

> M6.3 要做 information_accuracy 時，建議不要再 bump save
> schema，除非真的新增 persistent field。先做 pure helper
> / system surface 比較乾淨。

M6.3 introduces zero persistent state. The helper reads only
existing `GameState` fields (and for M6.3, only the validity
of the `CountryId` index — not even the country's content).
M6.6 may introduce an `intelligence_budget` field (or reuse
an existing one like `budget.intelligence`) and bump the save
format at THAT time; M6.7 similarly for corruption coupling
if needed. M6.3 stays save-bump-free.

## 1. Scope

What ships:

```text
leviathan::systems::information_accuracy                  (new module)
   kPlaceholderInformationAccuracy = 1.0                  (public constant)
   compute_for_country(state, country) -> Result<double>  (new free fn)
include/leviathan/systems/information_accuracy.hpp        (new header)
src/leviathan/systems/information_accuracy.cpp            (new impl)
tests/systems/information_accuracy_test.cpp               (9 new doctest cases)
CMakeLists updates for the new sources
```

What does NOT change:

```text
no save format bump (still v16)
no new state field (no GameState / CountryState /
   EventDefinition / EventInstance / EventInstanceActor
   field; the helper is read-only over existing state)
no new artefact (still 10)
no new runner CLI flag / RunnerOptions field
no events.jsonl semantic change
no new PlayerCommandKind
no scenario_loader change
no event_evaluator / event_firer / event_effects /
   event_engine / monthly_pipeline / runner code change
no canonical fixture change (the helper is opt-in by
   caller; no system invokes it yet)
no M6.4+ work (one sub-milestone per PR)
no M1/M2/M3/M4/M5 / M6.1 / M6.2 systems' external
   behaviour change
no rebake of M1.17 / M2 / M3 / M4 / M5 byte-identical
   determinism baselines (no code path consumes the
   helper yet; M6.4 will start using it)
```

## 2. API shape

```cpp
namespace leviathan::systems::information_accuracy {

inline constexpr double kPlaceholderInformationAccuracy = 1.0;

core::Result<double> compute_for_country(
    const core::GameState& state,
    core::CountryId        country);

}
```

Three reasons the API is one function + one constant:

- **`compute_for_country` is the only entry point M6.3
  ships.** M6.4 will add a `compute_for_event` variant
  that resolves the country from
  `instance.actors.front().country_id_code` and delegates
  to `compute_for_country`. Keeping the country form
  primary lets M6.4 be additive without a refactor.
- **The constant is greppable.** Future readers reading
  the placeholder body can grep
  `kPlaceholderInformationAccuracy` to find every site
  that depends on the "no-distortion ceiling" semantics.
  When M6.6 / M6.7 land, they replace the BODY of
  `compute_for_country` but the constant remains — it
  becomes the ceiling that the formula approaches when
  intelligence is maxed and corruption is zero.
- **`Result<double>` over a sentinel.** NaN would break
  the determinism contract; an assert would crash; a
  documented sentinel like -1.0 leaks into formulas that
  expect a `[0, 1]` ratio. `Result::failure` is
  consistent with the M1.5 / M5.6 helper style.

## 3. Why the placeholder is 1.0

Three candidate values were considered:

- **0.0** (full distortion). Honest about "no information
  yet" but pessimistic; M6.9 non-debug hiding would have
  to invert it back. Bad shape for the M6.6/M6.7 ramp.
- **0.5** (half distortion). Arbitrary; no calibration
  basis.
- **1.0** (no distortion). Honest about "the formula
  isn't wired yet". M6.6/M6.7 will reduce this below 1.0
  based on intelligence / corruption — the value drops
  as those inputs get worse. M6.4 reading the helper
  during this M6.3 era will see 1.0 and report the
  truth verbatim, which is the correct fallback before
  the distortion pipeline lands.

M6.3 picks 1.0 for that reason. Documented in the
header.

## 4. What `compute_for_country` does NOT consult yet

A test pins that the placeholder body **does not read any
country field**:

```cpp
TEST_CASE("M6.3 compute_for_country: placeholder body does
          NOT consult country fields yet") {
    // Two states differing only in corruption: same result.
    // When M6.7 lands, this test should be REWRITTEN to pin
    // the expected difference.
}
```

That test deliberately encodes the M6.3 contract: the body
is empty of formula. M6.6 / M6.7 will tighten it.

## 5. Validation

`compute_for_country` rejects invalid country handles with:

```text
information_accuracy::compute_for_country: actor CountryId N
is not a valid index into state.countries
```

The shape mirrors the M1.5 / M5.6 `policy::apply_effects_to_actor`
validation message so a future refactor can pull both into a
shared helper if useful.

Tests cover: out-of-range index, `CountryId::invalid()`, and
empty `state.countries`.

## 6. Purity / determinism

Pinned by two tests:

- **Purity**: `compute_for_country` is called repeatedly on
  the same state (including a failed call with an invalid
  index); the state's full serialised bytes are identical
  before and after.
- **Determinism**: three calls on identical inputs return
  identical values.

## 7. Tests added (9)

- **Happy path (3)**: valid country returns the placeholder
  constant; result is in `[0, 1]`; placeholder body does
  NOT consult country fields (the M6.7-anticipation test).
- **Validation (3)**: out-of-range `CountryId` rejected;
  `CountryId::invalid()` rejected; empty `state.countries`
  with `CountryId{0}` rejected.
- **Purity / determinism (2)**: helper does NOT mutate
  GameState (pinned via `ss::serialize(before) ==
  ss::serialize(after)`); helper is deterministic.
- **Public constant (1)**: `kPlaceholderInformationAccuracy
  == 1.0` is stable.

Test counts: 1074 doctest cases / 62459 assertions
(was 1065 / 62436 at M6.2 close).

## 8. What's deferred (the M6.4+ horizon, per RFC-090)

```text
M6.4  reported value          (uses this accuracy to derive
                               numeric reported value of an
                               event's trigger / effect)
M6.5  bias / noise            (randomised distortion on top
                               of the accuracy gate)
M6.6  intelligence budget     (weights accuracy DOWN by
                               missing intelligence budget)
M6.7  corruption              (weights accuracy DOWN by
                               corruption)
M6.8  debug mode              (bypasses accuracy entirely)
M6.9  non-debug mode          (uses computed accuracy to
                               hide visible_report toward
                               true_cause)
```

Each is its own dedicated sub-milestone PR. M6.3 ships the
helper SHAPE that all of the above will consume / replace
the body of.

## 9. What M6.3 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no save format bump (still v16)
no new state field
no new artefact (still 10)
no new RunnerOptions field / CLI flag
no new PlayerCommandKind
no scenario_loader change
no event-module / monthly_pipeline / runner code change
no canonical fixture change
no consumer in any current system —
   the helper is callable but no one calls it yet
no per-event variant (compute_for_event is M6.4 scope)
no reported value (M6.4)
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
no M6.4 work
no docs/milestone-6-checkpoint.md
no docs/milestone-6-result.md
no "M6 closed" wording
```

M6 remains in progress. M6.3 ships the helper skeleton; the
next sub-milestone is M6.4 (`reported value`) per RFC-090
§6.4, but M6.4 is **unspec'd here** and waits for an
explicit `做M6.4` go-ahead.

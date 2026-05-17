# M3.2 - InterestGroupReactionSystem skeleton

Companion notes for `feature/m3-2-interest-group-reaction-system`.
M3.2 introduces the first system that **mutates** the M3.1
`GameState::interest_groups` data layer. Deliberately the
smallest deterministic shape that counts as a "reaction layer":
each interest group's `loyalty` drifts linearly toward its
country's `stability`, and its `radicalism` drifts linearly
toward `1.0 - stability`. Nothing else moves.

## 1. Scope

M3.1 introduced the data layout for political actors. M3.2 turns
the data into a live reaction loop: every monthly tick, every
interest group's mood adjusts toward the country's overall
political stability. Concretely:

```text
loyalty    += (country.stability         - loyalty)    * 0.05
radicalism += ((1.0 - country.stability) - radicalism) * 0.05
```

Both fields are clamped to `[0, 1]` after the step. `influence`,
`kind`, `country`, `id_code`, and `name` are untouched. No event
fires, no command resistance changes, no faction state mutates,
no country state pushes back.

### Why these two fields, this rate, this driver

- **`loyalty` is the most natural "do you support the current
  regime" signal.** Tying it to stability is the simplest
  defensible link: a stable regime keeps its political actors
  loyal; an unstable one loses them.
- **`radicalism` is the natural complement.** Drifting it toward
  `1.0 - stability` means a collapsing regime feeds escalation.
  This sets up future protest / strike / coup math without
  shipping any of it yet.
- **`influence` stays put.** Political weight is structural; the
  M3.2 rate would make it oscillate too quickly. Defer to a
  later sub-milestone with real gameplay evidence.
- **`kInterestGroupReactionRate = 0.05`** matches the gentler
  end of the M1.6 faction rates (0.05 / 0.10). Slow enough that
  a few months don't fully converge; fast enough that a year-
  long shift is visible.
- **Single-input driver (only `country.stability`).** Following
  the stripped-formula precedent (M1.6 / M1.7 / M2.18 / M2.19),
  M3.2 picks the smallest defensible input set. Future PRs
  may layer corruption / authority / legitimacy weights when
  gameplay needs them.

## 2. Public API

`include/leviathan/systems/interest_group_system.hpp`:

```cpp
namespace leviathan::systems::interest_group {

inline constexpr double kInterestGroupReactionRate = 0.05;

struct ReactionOutcome {
    int groups_updated = 0;
};

core::Result<ReactionOutcome> react(core::GameState& state);

}  // namespace
```

`react(state)` walks `state.interest_groups` in vector order and
applies the formula to every entry. The function is pure: no
logs, no RNG, no time advancement, no country / faction / policy
state mutation, no event emission.

### Preflight atomicity

Before mutating any group, `react` validates every
`group.country` against `state.countries`. If any entry has a
bad country index, the function returns `Result::failure` and
**no** group's `loyalty` / `radicalism` was touched.

This is stricter than the early M1 / M2 stripped-formula
systems (which were documented as non-atomic mid-list). M3.2's
preflight pass is cheap (size-N walk), makes the test surface
obvious, and keeps the door open for future per-group failure
modes without a refactor.

### Integration into the monthly pipeline

`src/leviathan/systems/monthly_pipeline.cpp` extends
`tick_all_countries` with one new step that runs AFTER every
per-country tick has settled:

```text
1.  for each country:
        faction::react   (state, country)
        stability::tick  (state, country)
        economy::tick    (state, country)
2.  interest_group::react (state)            <- M3.2 new
```

The global step runs LAST so it reads each country's
**post-tick** stability. The reverse order would coupling
`react` to whichever country gets ticked first; running it
globally afterward keeps the contract clean.

`MonthlyOutcome` gains an `int interest_groups_updated` field
that mirrors `ReactionOutcome::groups_updated`. The existing
`countries_processed` counter and the per-country `countries`
vector are untouched.

A failure inside `react` (today: invalid country index) fails
the whole `tick_all_countries` call, same as any sub-system
failure.

## 3. Save format

**No change.** Still v11. M3.2 only mutates existing M3.1 fields.

## 4. Tests

13 new doctest cases (M3.1 closed at 647 → M3.2 lands at 660).

`tests/systems/interest_group_system_test.cpp` (11):

- Empty state → success, `groups_updated == 0`.
- Empty interest groups with non-empty countries → success.
- High stability (0.8) drifts loyalty 0.4 → 0.42, radicalism
  0.6 → 0.58.
- Low stability (0.2) drifts loyalty 0.6 → 0.58, radicalism
  0.1 → 0.135.
- Stability at exact mid-point with group already at 0.5 leaves
  values unchanged (delta zero on both sides).
- Multi-group / multi-country: each group reads its own
  country's stability.
- `influence` is never touched.
- `kind`, `country`, `id_code`, `name` are never touched.
- Clamp keeps `loyalty` and `radicalism` in `[0, 1]` even when
  inputs start at the edges.
- Invalid country index fails preflight; valid groups stay
  byte-identical (atomicity).
- Sentinel `CountryId::invalid()` (-1) fails preflight too.

`tests/systems/monthly_pipeline_test.cpp` (2):

- `tick_all_countries` runs `interest_group::react` after every
  per-country tick. Two countries with one interest group each;
  after the call, every interest group's loyalty / radicalism
  matches `0.5 * 0.95 + post_tick_stability * 0.05` (the closed
  form of one drift step from the 0.5 baseline). Existing M1
  systems still ran (GDP shifted via `economy::tick`).
- `tick_all_countries` with empty `state.interest_groups` still
  succeeds with `interest_groups_updated == 0`. Regression for
  every M1 / M2 test fixture that doesn't author groups.

The M1.17 / M2.22 integration tests pass unchanged: they use
canonical scenarios that don't load any interest groups, so the
new monthly step processes zero groups and the byte-identical
determinism contract holds.

## 5. What's NOT in scope

Deliberate non-goals:

- **No save schema bump.** The M3.1 v11 shape carries M3.2's
  drift through reload without change.
- **No new fields on `InterestGroupState`.**
- **No new `InterestGroupKind` variants.**
- **No per-kind drift rate.** The 0.05 constant applies
  uniformly.
- **No `country_authority` / `corruption` / `legitimacy` /
  `policy preferences` inputs.** Only `country.stability`
  drives the drift.
- **No weighted multi-input formula.**
- **No RNG / probabilistic behaviour.**
- **No country aggregate effect.** Interest groups do NOT push
  back on `country.stability`, `country.legitimacy`,
  `country.government_authority`, or `country.corruption`. The
  reverse direction is reserved for M3.3+.
- **No event triggers.** No `state.logs` entry on drift.
- **No coup / strike / protest / civil war.**
- **No cross-border / foreign-influence behaviour.**
- **No AI, no UI, no REPL, no CLI flag.**
- **No new `PlayerCommandKind`, no command-system integration.**
- **No `government_authority` mutation.**
- **No automatic generation of default groups.**
- **No faction reaction changes** — `faction::react` is
  byte-identical.

## 6. Cross-links

- M3.1 (`m3-1-interest-group-state.md`) — the data layer this
  PR mutates.
- M1.6 (`m1-6-faction-reactions.md`) — the structural precedent
  for a stripped linear-toward-equilibrium reaction system.
- M1.9 / M1.10 (`m1-9-monthly-pipeline.md` /
  `m1-10-runner-monthly-wiring.md`) — the monthly pipeline
  whose `tick_all_countries` M3.2 extends.
- M1.7 (`m1-7-stability-tick.md`) — supplies the
  `country.stability` value that drives M3.2's drift.
- RFC-020 §5 — long-term faction / interest-group catalogue;
  M3.2 implements one of its drift mechanics.
- `milestone-2-result.md` — the architectural invariants M3
  must continue to honour (free-function systems, GameState
  stays passive, determinism for 5 artefacts, etc.).

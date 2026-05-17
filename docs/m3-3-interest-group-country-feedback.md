# M3.3 - InterestGroup country feedback skeleton

Companion notes for `feature/m3-3-interest-group-country-feedback`.
M3.3 closes the reaction loop M3.1 + M3.2 opened: interest
groups now push back on country state. The push-back is the
**smallest** deterministic shape that counts as a closed loop:
a single output field (`country.stability`), a single input
aggregate (influence-weighted `radicalism`), a single rate
constant (`0.02`), and a single integration point (the new
final step of `tick_all_countries`).

## 1. Scope

M3.2 shipped the country -> interest-group direction (groups
read post-tick stability and drift their `loyalty` /
`radicalism`). M3.3 ships the inverse direction. The two steps
run consecutively at the bottom of `tick_all_countries`:

```text
for each country:
    faction::react  (state, country)
    stability::tick (state, country)
    economy::tick   (state, country)
interest_group::react           (state)   # M3.2
interest_group::country_feedback(state)   # M3.3 NEW
```

Running M3.3 after M3.2 means it reads the **just-updated**
group radicalism. Running both globally (not per-country) keeps
the contract clean: every country processes its own M1 sub-
systems first, then M3 reacts at the state level once.

### Why this exact shape

- **Only `country.stability`.** Stability is the natural first
  feedback target — RFC-080 §5's stability formula already
  treats average faction support / radicalism as inputs.
  Mutating `legitimacy`, `government_authority`, `corruption`,
  `central_control`, or `administrative_efficiency` from this
  step would open larger feedback loops we don't yet have
  formula evidence for; defer to M3.4+.
- **Only influence-weighted `radicalism`.** A single aggregate
  input keeps the formula readable and avoids
  `loyalty - radicalism` cancellations. Using `influence` as
  the weight gives the previously-passive `influence` field
  its first job without mutating it.
- **Rate `0.02`** — slower than M3.2's `0.05`. The slower
  outer leg of the closed loop is the standard trick to keep
  two-step linear systems from oscillating. Country state
  should also feel "heavier" than group mood in any plausible
  gameplay reading.
- **Skip countries with no matching groups or zero total
  influence.** Existing scenarios author zero interest groups,
  so every country in M1.17 / M2.22 / typical M3.x fixtures
  hits this skip path and sees byte-identical stability.

## 2. Public API

`include/leviathan/systems/interest_group_system.hpp` (extended):

```cpp
namespace leviathan::systems::interest_group {

inline constexpr double kInterestGroupCountryFeedbackRate = 0.02;

struct CountryFeedbackOutcome {
    int countries_updated = 0;
};

core::Result<CountryFeedbackOutcome> country_feedback(
    core::GameState& state);

}  // namespace
```

### Formula

For each country index `ci`:

```cpp
double weighted_sum = 0.0;
double weight_sum   = 0.0;
for (const auto& g : state.interest_groups) {
    if (g.country.value() != ci)  continue;
    if (g.influence <= 0.0)       continue;
    weighted_sum += g.influence * g.radicalism;
    weight_sum   += g.influence;
}
if (weight_sum <= 0.0)            continue;   // skip

const double weighted_radicalism = weighted_sum / weight_sum;
const double target_stability    = 1.0 - weighted_radicalism;

country.stability += (target_stability - country.stability)
                     * kInterestGroupCountryFeedbackRate;
country.stability = std::clamp(country.stability, 0.0, 1.0);
++outcome.countries_updated;
```

### Preflight atomicity (strict)

Before any country is mutated, `country_feedback` validates the
whole input:

- Every `group.country` is valid AND indexes into
  `state.countries`.
- Every `group.influence` is finite and in `[0, 1]`.
- Every `group.radicalism` is finite and in `[0, 1]`.
- Every `country.stability` is finite and in `[0, 1]`.

A single bad entry makes the function return
`Result::failure` and leaves every country's `stability`
byte-identical. M3.3 is stricter than M3.2 here because a NaN
propagating into `country.stability` would silently poison
future ticks; the cost is two extra walks (O(groups) +
O(countries)) on a path that already touches both.

### Mutation surface

The single mutation is `country.stability`. M3.3 does NOT
touch:

- any other `CountryState` field (`gdp`, `legitimacy`,
  `corruption`, `government_authority`, `legal_tax_burden`,
  ...),
- any `InterestGroupState` field (the feedback reads them
  read-only),
- factions, policies, applied_commands, logs, the RNG, the
  date, or any save metadata.

## 3. Monthly pipeline integration

`monthly_pipeline.cpp`'s `tick_all_countries` gains a second
global step right after M3.2's `interest_group::react`:

```cpp
auto fb = interest_group::country_feedback(state);
if (!fb.ok()) return failure(...);
out.interest_group_countries_updated = fb.value().countries_updated;
```

`MonthlyOutcome` gains:

```cpp
int interest_group_countries_updated = 0;
```

Existing fields (`countries_processed`, `countries`,
`interest_groups_updated`) are unchanged. The per-country
`tick_country` is byte-identical — the new step lives only at
the `tick_all_countries` aggregation level.

A failure inside `country_feedback` fails the whole
`tick_all_countries` call, same shape as any sub-system
failure.

## 4. Save format

**No change.** Still v11. M3.3 only mutates the existing
`CountryState::stability` field (introduced in M1.1).

## 5. Tests

14 new doctest cases (M3.2 closed at 660 → M3.3 lands at 674).

`tests/systems/interest_group_system_test.cpp` (12):

- Empty state succeeds, `countries_updated == 0`.
- Country with no matching groups is skipped (stability
  untouched).
- High `radicalism` (0.6) lowers `stability` from 0.8 → 0.792.
- Low `radicalism` (0.1) raises `stability` from 0.4 → 0.41.
- Influence-weighted aggregate over two groups (weights
  0.75 / 0.25) reproduces `0.503` from `0.5 + (0.65 - 0.5)
  * 0.02`.
- Zero-influence groups are ignored (country with one zero-
  influence group is skipped).
- Multi-country: GER and FRA update independently from their
  own groups.
- Interest groups themselves are never mutated by
  `country_feedback`.
- Preflight: invalid group `country` index fails, no country
  mutates.
- Preflight: non-finite group `radicalism` fails.
- Preflight: out-of-range group `influence` (1.5) fails.
- Preflight: non-finite `country.stability` fails.
- Clamp keeps stability in `[0, 1]`.

`tests/systems/monthly_pipeline_test.cpp` (2):

- `tick_all_countries` runs M3.2 react then M3.3
  `country_feedback`. Both counters set to 1. Group's
  `radicalism` moved off the starting 0.5; country's
  `stability` moved off its pre-tick value. Re-running
  `country_feedback` on the resulting state reproduces the
  closed-form expectation — pinning that the integration ran
  `country_feedback` AFTER `react` (had it run before, the
  re-run on the resulting state would have used a different
  radicalism and the closed form would diverge).
- The existing M3.2 zero-groups test extended to also check
  `interest_group_countries_updated == 0` — regression for
  every M1 / M2 fixture that doesn't author interest groups.

Drive-by: the M3.2 monthly-pipeline integration test had an
exact-arithmetic check for post-react loyalty equal to
`0.5 * 0.95 + post_tick_stability * 0.05`. After M3.3 wires in
`country_feedback`, the `country.stability` read back off
`state` has been mutated twice per tick (once by
`stability::tick`, once by `country_feedback`), so duplicating
the M3.3 formula inside the M3.2 assertion would couple two
tests. The exact-arithmetic check was demoted to a directional
check (loyalty / radicalism moved off 0.5); the new M3.3
monthly test pins the layered behaviour.

The M1.17 / M2.22 integration tests pass unchanged: their
canonical scenarios author zero interest groups, so M3.2
processes zero groups and M3.3 finds no matching groups per
country and skips all of them; every artefact stays byte-
identical.

## 6. What's NOT in scope

Deliberate non-goals:

- **No save schema bump** (still v11).
- **No new state fields** on `CountryState` or
  `InterestGroupState`.
- **No new `InterestGroupKind` variants.**
- **No mutation of `legitimacy`, `government_authority`,
  `corruption`, `central_control`, or
  `administrative_efficiency`.** Stability is the only output.
- **No additional aggregate inputs** beyond influence-weighted
  radicalism — `loyalty` does not feed `country_feedback`.
- **No per-kind / per-country / per-output rate.** The 0.02
  constant is uniform.
- **No RNG / probabilistic behaviour.**
- **No event triggers, no `state.logs` entry, no AI, no UI /
  CLI / REPL.**
- **No coup / strike / protest / civil war.**
- **No cross-border / foreign-influence behaviour.**
- **No automatic generation of default groups.**
- **No `government_authority` mutation.**
- **No command-gate integration** (`apply_pending` /
  `try_apply_pending` paths unchanged).
- **No faction reaction changes** — `faction::react` is
  byte-identical.
- **No policy preference system.**
- **No changes to M2 thresholds / formulas.**
- **No `tick_country` change** — M3.3 lives exclusively at
  the `tick_all_countries` aggregation level.

## 7. Cross-links

- M3.1 (`m3-1-interest-group-state.md`) — supplies the
  `interest_groups` vector that M3.3 reads.
- M3.2 (`m3-2-interest-group-reaction-system.md`) — supplies
  the just-drifted `radicalism` that M3.3 aggregates.
- M1.6 (`m1-6-faction-reactions.md`) — structural precedent
  for a single-rate linear-toward-equilibrium reaction system.
- M1.7 (`m1-7-stability-tick.md`) — supplies the
  `country.stability` field M3.3 mutates.
- M1.9 / M1.10 (`m1-9-monthly-pipeline.md` /
  `m1-10-runner-monthly-wiring.md`) — the monthly pipeline
  whose `tick_all_countries` M3.3 extends.
- RFC-080 §5 — long-term stability formula that, in a future
  milestone, may consume interest-group state directly rather
  than via this `country_feedback` indirection.
- `milestone-2-result.md` — architectural invariants every
  M3 sub-milestone continues to honour (free-function
  systems, GameState stays passive, determinism for 5
  artefacts, etc.).

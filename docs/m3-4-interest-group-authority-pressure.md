# M3.4 - InterestGroup-derived authority pressure skeleton

Companion notes for `feature/m3-4-interest-group-authority-pressure`.
M3.4 opens the **second** reverse-direction channel inside the M3
reaction loop: interest groups now press not just on
`country.stability` (M3.3) but also on
`country.government_authority.bureaucratic_compliance`. The
press-on-authority leg runs at the slowest rate in the ladder
yet (`0.01`) so the closed loop stays well-damped.

## 1. Scope

After M3.3 the closed loop was:

```text
country.stability  ->  group.loyalty / radicalism   (M3.2)
group.radicalism   ->  country.stability             (M3.3)
```

M3.4 adds:

```text
group.loyalty (Bureaucracy-kind only) -> country.bureaucratic_compliance
```

The press only touches one authority sub-field
(`bureaucratic_compliance`); the other three M2.16 fields
(`military_loyalty`, `intelligence_capability`,
`media_control`) stay where they are. The press is sourced from
**Bureaucracy-kind** groups only — Workers / Military /
Religious / etc. groups are ignored. The aggregate is influence-
weighted loyalty (mirroring M3.3's influence-weighted radicalism
shape but on a different field).

### Why this exact shape

- **`bureaucratic_compliance` is the natural first target.** It
  is the M2.18 `EnactPolicy` gate input; tying it to interest-
  group state closes the smallest meaningful command-resistance
  feedback loop without touching the gate itself.
- **Bureaucracy-kind only.** "Bureaucratic compliance" reads as
  the bureaucracy's willingness to execute orders, so the
  bureaucracy's own loyalty is the most defensible input. Other
  kinds (military, workers, students) will get their own
  channels in future PRs.
- **Loyalty, not radicalism.** M3.3 already uses radicalism to
  drive stability. Reusing the same input for authority would
  double-count one negative signal; using loyalty keeps the two
  output paths sourced from independent fields.
- **Rate 0.01.** Forms a clean ladder with M3.2 (0.05) and M3.3
  (0.02). Country `government_authority` should be the most
  structural / slow-to-change of the three outputs.

## 2. Public API

`include/leviathan/systems/interest_group_system.hpp`:

```cpp
namespace leviathan::systems::interest_group {

inline constexpr double kInterestGroupAuthorityPressureRate = 0.01;

struct AuthorityPressureOutcome {
    int countries_updated = 0;
};

core::Result<AuthorityPressureOutcome> authority_pressure(
    core::GameState& state);

}  // namespace
```

### Formula

For each country index `ci`:

```cpp
double weighted_sum = 0.0;
double weight_sum   = 0.0;
for (const auto& g : state.interest_groups) {
    if (g.country.value() != ci)                                continue;
    if (g.kind != core::InterestGroupKind::Bureaucracy)         continue;
    if (g.influence <= 0.0)                                     continue;
    weighted_sum += g.influence * g.loyalty;
    weight_sum   += g.influence;
}
if (weight_sum <= 0.0)                                          continue;

const double target = weighted_sum / weight_sum;
auto& bc = state.countries[ci].government_authority
               .bureaucratic_compliance;
bc += (target - bc) * kInterestGroupAuthorityPressureRate;
bc  = std::clamp(bc, 0.0, 1.0);
++outcome.countries_updated;
```

If no Bureaucracy-kind group matches the country (or every match
has zero influence), the country is **skipped** (no mutation,
not counted).

### Mutation surface

The single mutation is
`country.government_authority.bureaucratic_compliance`. M3.4
does NOT touch:

- the other three `government_authority` sub-fields
  (`military_loyalty`, `intelligence_capability`,
  `media_control`),
- any other `CountryState` field (`stability`, `legitimacy`,
  `corruption`, `central_control`,
  `administrative_efficiency`, etc.),
- any `InterestGroupState` field (read-only),
- factions, policies, applied_commands, logs, RNG, the date,
  save metadata.

### Strict preflight

Before any country mutates, the function validates only the
inputs it actually reads:

- Every `group.country` valid + indexes into `state.countries`.
- Every `group.influence` finite + `[0, 1]`.
- Every `group.loyalty` finite + `[0, 1]`.
- Every `country.government_authority.bureaucratic_compliance`
  finite + `[0, 1]`.

`group.radicalism` and `country.stability` are deliberately
NOT preflighted here — M3.4 doesn't read them and a malformed
radicalism would be caught by M3.2 / M3.3 anyway.

A single bad entry returns `Result::failure` and leaves every
country's `bureaucratic_compliance` byte-identical.

## 3. Monthly pipeline integration

`monthly_pipeline.cpp`'s `tick_all_countries` gains a third
global step right after M3.3's `country_feedback`:

```cpp
auto ap = interest_group::authority_pressure(state);
if (!ap.ok()) return failure(...);
out.interest_group_authority_countries_updated =
    ap.value().countries_updated;
```

`MonthlyOutcome` gains:

```cpp
int interest_group_authority_countries_updated = 0;
```

Pipeline order:

```text
for each country:
    faction::react    (state, country)
    stability::tick   (state, country)
    economy::tick     (state, country)
interest_group::react             (state)   # M3.2 (rate 0.05)
interest_group::country_feedback  (state)   # M3.3 (rate 0.02)
interest_group::authority_pressure(state)   # M3.4 (rate 0.01)
```

The three global steps form a decreasing-rate ladder:

```text
group mood (M3.2)        0.05
country stability (M3.3) 0.02
authority (M3.4)         0.01
```

Each outer leg is slower than the inner one to keep the
closed-loop dynamics well-damped. A failure inside
`authority_pressure` fails the whole `tick_all_countries` call,
same shape as any sub-system failure.

## 4. Save format

**No change.** Still v11. M3.4 only mutates the existing
`CountryState::government_authority::bureaucratic_compliance`
field (introduced in M2.16).

## 5. Tests

19 new doctest cases (M3.3 closed at 674 → M3.4 lands at 693).

`tests/systems/interest_group_system_test.cpp` (17):

- Empty state succeeds, `countries_updated == 0`.
- Country with no Bureaucracy group → skipped.
- Single Bureaucracy group, high loyalty (0.8): compliance
  drifts 0.4 → 0.404.
- Single Bureaucracy group, low loyalty (0.2): compliance
  drifts 0.8 → 0.794.
- Influence-weighted aggregate over two Bureaucracy groups
  (weights 0.75 / 0.25, loyalties 0.2 / 0.8) gives 0.4985.
- Non-Bureaucracy groups (Military, Workers) are ignored even
  when they would otherwise dominate the aggregate.
- Zero-influence Bureaucracy groups are ignored.
- Multi-country independent updates (GER target 0.8 → 0.503,
  FRA target 0.2 → 0.497).
- Interest groups themselves are never mutated.
- The OTHER three authority sub-fields (`military_loyalty`,
  `intelligence_capability`, `media_control`) are byte-identical
  after the step.
- `stability` / `legitimacy` / `corruption` are byte-identical.
- Invalid `group.country` fails preflight; no country mutates.
- Non-finite `group.loyalty` fails preflight.
- Out-of-range `group.influence` (1.5) fails preflight.
- Non-finite `bureaucratic_compliance` fails preflight.
- Clamp keeps compliance in `[0, 1]`.

`tests/systems/monthly_pipeline_test.cpp` (2):

- `tick_all_countries` runs M3.2 → M3.3 → M3.4 in that order.
  All four counters set (`countries_processed`,
  `interest_groups_updated`,
  `interest_group_countries_updated`,
  `interest_group_authority_countries_updated`). Group
  `loyalty` moves off the starting 0.5. **Ordering pin**:
  re-running `authority_pressure` on the resulting state
  reproduces the closed-form expectation, which holds only if
  the pipeline ran `authority_pressure` AFTER `react`.
- `tick_all_countries` with a non-Bureaucracy interest group
  (e.g. Military) still succeeds; M3.4 skips the country and
  `interest_group_authority_countries_updated == 0` while M3.2
  / M3.3 still run as before.

The M1.17 / M2.22 integration tests pass unchanged: canonical
scenarios author zero interest groups, so M3.4's per-country
skip path fires for every country and
`bureaucratic_compliance` stays byte-identical.

## 6. What's NOT in scope

Deliberate non-goals:

- **No save schema bump** (still v11).
- **No new state fields** on `CountryState` or
  `InterestGroupState`.
- **No new `InterestGroupKind` variants.**
- **No mutation of `military_loyalty`, `intelligence_capability`,
  or `media_control`.** Bureaucracy compliance is the only
  authority output in M3.4.
- **No mutation of `legitimacy`, `corruption`, `stability`,
  `central_control`, or `administrative_efficiency`.**
- **No additional aggregate inputs.** `radicalism` does NOT
  feed this step.
- **No per-kind / per-country / per-output rate.** The 0.01
  constant is uniform.
- **No weighted multi-input formula** beyond influence-weighted
  Bureaucracy loyalty.
- **No RNG / probabilistic behaviour.**
- **No events / `state.logs` entry, no AI, no UI / CLI / REPL.**
- **No coup / strike / protest / civil war / cross-border.**
- **No automatic group generation.**
- **No command-gate integration.** `apply_pending` /
  `try_apply_pending` paths byte-identical; the M2.18 / M2.19
  thresholds and formulas unchanged.
- **No new `PlayerCommandKind` variants.**
- **No faction reaction changes.**
- **No `tick_country` change.**
- **No M1 / M2 system change.**

## 7. Cross-links

- M3.1 (`m3-1-interest-group-state.md`) — supplies the
  `interest_groups` vector M3.4 reads.
- M3.2 (`m3-2-interest-group-reaction-system.md`) — supplies
  the just-drifted `loyalty` value M3.4 aggregates.
- M3.3 (`m3-3-interest-group-country-feedback.md`) — sibling
  reverse-direction step that runs immediately before M3.4 in
  the monthly pipeline; both steps share the strict preflight
  pattern.
- M2.16 (`m2-16-government-authority-state.md`) — supplies the
  `bureaucratic_compliance` field M3.4 mutates.
- M2.18 (`m2-18-enact-policy-execution-gate.md`) — consumes
  `bureaucratic_compliance` as the EnactPolicy gate threshold
  input. M3.4 deliberately does NOT change the gate formula;
  the closed loop is "interest groups → authority → existing
  gate".
- M1.7 (`m1-7-stability-tick.md`) — the country-side dynamic
  that M3.3 / M3.4 sit downstream of.
- RFC-020 §3 — long-term "state control" dimensions that
  M3.4 begins wiring into the reaction layer.
- `milestone-2-result.md` — architectural invariants every
  M3 sub-milestone continues to honour.

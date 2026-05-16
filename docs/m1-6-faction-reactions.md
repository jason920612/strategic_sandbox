# M1.6 - Faction reaction logic (minimal)

Companion notes for `feature/m1-06-faction-reactions`. The first
faction-side dynamics in the project.

Per the M1.5 review (`保持小步`): **one explicit-call free function,
two reaction rules, no monthly tick, no AI, no events, no
type-specific behaviour**. Everything richer waits for later
sub-milestones once the test suite and balance work demand it.

## 1. API

```cpp
namespace leviathan::systems::faction {

struct ReactionOutcome {
    int factions_updated = 0;  // count of factions touched
};

inline constexpr double kLoyaltyDriftRate = 0.10;
inline constexpr double kSupportDriftRate = 0.05;

core::Result<ReactionOutcome> react(core::GameState& state,
                                    core::CountryId  country);

}
```

Same free-function shape as M0.4 TimeSystem, M0.5 RandomService, etc.
`GameState` stays methodless.

## 2. Reaction rules

```text
loyalty += (country.stability  - loyalty) * 0.10   // ratio, clamped [0, 1]
support += (country.legitimacy - support) * 0.05   // ratio, clamped [0, 1]
```

That's it. Two rules, both linear convergence toward a country-state-
derived equilibrium. The constants are exported in the header so tests
reference `kLoyaltyDriftRate` rather than rehard-coding `0.10`.

### Why these two, and only these two?

- **`loyalty → stability`**: the most defensible single link in
  RFC-080 §5 (a stable regime sustains loyalty; an unstable one
  doesn't).
- **`support → legitimacy`**: a faction's popular backing tracks
  the regime's perceived right to rule.

Both are pure linear arithmetic on `double`. The test suite verifies
them with exact `doctest::Approx` math, including:

- One-step delta exact match against the formula
- Equilibrium no-op (faction == country target)
- Above-target moves down (works for both signs)
- 50-step convergence within `epsilon(0.01)` (geometric decay
  property)

### Why slower for support than for loyalty?

Loyalty is a "personal" disposition that updates faster on stability
signals; popular support is sticky and changes more slowly. The 2:1
rate split (`0.10` vs `0.05`) is a deliberate placeholder that the
balance work in M1.8 / M1.9 can replace with researched constants.

## 3. What changes — and what deliberately doesn't

| Field | M1.6 behaviour |
|---|---|
| `loyalty`    | Drifts toward `country.stability`, rate 0.10 |
| `support`    | Drifts toward `country.legitimacy`, rate 0.05 |
| `influence`  | **Unchanged** |
| `radicalism` | **Unchanged** |
| `resources`  | **Unchanged** |
| `id` / `country` / `id_code` / `country_id_code` / `name` / `type` / `preferred_policies` | **Unchanged** |

The "untouched fields" list is pinned by a dedicated test so a future
PR can't quietly start mutating them.

## 4. Country filter and edge cases

- `react(state, X)` iterates **all** of `state.factions` and only
  touches those whose `FactionState::country == X`.
- Factions belonging to other countries are skipped — tests confirm
  a 2-country setup updates only the requested country's factions.
- A country with **zero factions in `state.factions`** returns
  success with `factions_updated = 0`. Not a failure.
- An **invalid `CountryId`** (out of bounds or default-constructed)
  fails with a message naming the bad value; **no faction is
  modified**.
- Clamping happens **after** the step, so pathological inputs like
  `stability = 10.0` or `legitimacy = -2.0` (which the DataLoader
  rejects but a test or future system could manufacture) still
  produce well-defined `[0, 1]` outputs.

## 5. Drive-by from PR #16 review — `isfinite` in PolicySystem

The reviewer noted that `apply_policy_effects` assumed
`PolicyEffect::value` was finite (because DataLoader's `require_number`
rejects non-finite). A manually-constructed `PolicyData` (or one
loaded by a future bypass path) could carry NaN / Inf and corrupt
state.

Fixed: pre-flight now rejects non-finite values per effect, before
any state mutation:

```cpp
if (!std::isfinite(e.value)) {
    return Result::failure("effects[" + std::to_string(i) +
                           "]: value is not finite");
}
```

Two new regressions pin this:

- `apply: non-finite effect value is rejected at pre-flight` (NaN)
- `apply: positive infinity effect value is rejected at pre-flight`

The atomicity guarantee from M1.5 holds — pre-flight failure leaves
state untouched.

## 6. What M1.6 deliberately does NOT do

- **No monthly tick / TimeSystem integration.** The runner doesn't
  call `react` yet. Caller decides when.
- **No type-specific reactions.** Military / workers / media all run
  the same rule. RFC-020 describes type-specific dynamics; that's
  M1.7 / M1.8 territory.
- **No faction-vs-faction interaction.** A faction's state doesn't
  read other factions' state.
- **No `influence` / `radicalism` / `resources` dynamics.** Those
  need richer driver inputs (corruption, policy enactments, recent
  events) that the test suite can't pin yet.
- **No `Diagnostics::sanity_check` rule** for "faction state is
  within bounds after a reaction step". Clamping makes this
  trivially true; if a future rule mutates without clamping we can
  add the check.
- **No AI / runner enactment.**
- **No save format change.** Reactions mutate already-serialised
  fields; the save schema stays at v5.

## 7. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/systems/faction_system.hpp` | **new** — public API + rate constants |
| `src/leviathan/systems/faction_system.cpp` | **new** — implementation |
| `src/leviathan/systems/CMakeLists.txt` | adds `faction_system.cpp` |
| `tests/systems/faction_system_test.cpp` | **new** — 15 cases |
| `tests/CMakeLists.txt` | adds the test source |
| `src/leviathan/systems/policy_system.cpp` | `<cmath>` include + pre-flight `isfinite` check |
| `tests/systems/policy_system_test.cpp` | `<limits>` include + 2 NaN/Inf regressions |
| `docs/m1-6-faction-reactions.md` | this file |
| `docs/README.md`, `README.md`, `rfc/README.md` | status / progress notes |

Total tests: **255 → 272** (+17 — 15 faction-system, 2 policy isfinite).

## 8. Risks / things to watch

- **The reaction constants are placeholder values.** `0.10` and
  `0.05` were chosen for testability, not realism. M1.8 balance
  work should treat them as variables to tune, ideally backed by
  RFC-080 references.
- **The rules don't depend on faction `type`**, so every faction
  drifts toward the same equilibrium. That's clearly wrong for the
  eventual game (the military reacts differently to corruption than
  the workers do), but it's the simplest non-trivial first step.
- **No idempotence at low precision.** Repeated calls converge
  geometrically; they don't reach exact equilibrium in finite
  steps. Tests use `epsilon` matching, which is the right semantics
  but a contributor accustomed to integer arithmetic should be
  warned.
- **The runner still doesn't call `react`.** That's by design for
  M1.6 — the call is explicit. M1.7 / M1.8 will wire it into the
  monthly tick when economy and stability ticks join.

## 9. Next sub-milestone

The user's M1 plan didn't enumerate beyond M1.5. Likely candidates
(RFC-090 §M1):

- **M1.7 — Stability tick** per RFC-080 §5: `country.stability`
  updates based on `AverageFactionSupport`, `Radicalism`,
  `EconomicGrowth`, `Legitimacy`, etc. M1.6's faction reactions
  produce the `AverageFactionSupport` input.
- **M1.8 — Economy month-end tick** per RFC-080 §3 / §4: tax
  revenue derived from `GDP × LegalTaxBurden × FiscalCapacity ×
  CentralControl × (1 - CorruptionLeakage)`; budget rebalances
  against revenue.
- **M1.x — Monthly pipeline** wiring `react` + stability tick +
  economy tick into a single explicit caller, then into the runner.

PolicySystem (M1.5) and FactionSystem (M1.6) are the two free-
function building blocks the future monthly tick will compose.

# M2.16 - GovernmentAuthorityState

Companion notes for `feature/m2-16-government-authority-state`. M2.16
is the first M2 gameplay-state extension: it adds a stripped-down
authority sub-struct to `CountryState`, bumps the save format
**v9 → v10**, and plumbs the four new fields through DataLoader,
SaveSystem, and `diagnostics::compare_states`. No system reads or
writes the new fields yet; M2.17+ will start consuming them as the
RFC-020 §2-§3 command-execution-resistance / information-accuracy
inputs.

## 1. Scope

RFC-020 §3「國家掌控力」lists ten authority dimensions; M1.1
already captured two of them (`corruption`, `administrative_efficiency`).
M2.16 ships a stripped subset of four more, each a `[0, 1]` ratio
defaulting to `0.5`:

| Field                       | RFC-020 §3 source     | Future consumer (M2.17+ candidates)   |
|-----------------------------|------------------------|----------------------------------------|
| `bureaucratic_compliance`   | 官僚服從度             | Policy enactment resistance            |
| `military_loyalty`          | 軍方忠誠               | Coup risk, military-budget gate        |
| `intelligence_capability`   | 情報能力               | RFC-080 §8 information accuracy        |
| `media_control`             | 媒體控制               | RFC-080 §8 propaganda / public opinion |

Deliberately deferred to a later sub-milestone (with documentation
in this design note so future readers know what's missing): a
distinct `local_control` (separate from the existing
`CountryState::central_control`), `legal_mandate`,
`leader_prestige`, `party_organization`. Each of those should land
in its own targeted PR when an actual gameplay consumer needs it.

**M2.16 is data-only.** Zero M1 systems read the new fields. The
canonical M1 monthly pipeline (`faction::react → stability::tick →
economy::tick`) and the M2 player command path
(`apply_pending` / `replay_with_time` etc.) are byte-identical with
respect to in-flight behaviour.

## 2. Public API

`include/leviathan/core/entities.hpp`:

```cpp
struct GovernmentAuthorityState {
    double bureaucratic_compliance  = 0.5;
    double military_loyalty         = 0.5;
    double intelligence_capability  = 0.5;
    double media_control            = 0.5;
};

struct CountryState {
    // ... (M1.1 numeric fields)
    BudgetState               budget;
    GovernmentAuthorityState  government_authority;  // M2.16
    std::vector<ActivePolicy> active_policies;
    // ...
};
```

## 3. Persistence

### Save format v9 → v10

`include/leviathan/systems/save_system.hpp` bumps
`kSaveFormatVersion` from `9` to `10`. The version-history block
gains a v10 entry explaining that a v9 save lacks the
`government_authority` block and would silently default every
country to all-`0.5` authority — defaulting on missing-block reload
would mask whatever the original author intended, so we strict-
gate the bump.

At the **save layer** the block is **required**:

- Country JSON object must contain a `government_authority` key.
- The value must be a JSON object.
- The object must contain all four sub-keys, each parsed via
  `detail::require_ratio` (finite + within `[0, 1]`).

A save written from any valid in-memory state always round-trips
the full block (defaults serialise as `0.5`), so the "required"
contract has no friction for normal use.

### DataLoader

`src/leviathan/systems/data_loader.cpp` (`parse_country`) treats
the block as **optional**:

- Missing `government_authority` → every sub-field keeps its
  `GovernmentAuthorityState` default of `0.5`. Existing canonical
  country fixtures (`germany.json`, `france.json`, `japan.json`)
  load unchanged.
- Present `government_authority` must still be a JSON object.
  Partial blocks (e.g. typo in a key name) are rejected loudly so
  a misspelled `bureaucratic_complaince` doesn't silently fall
  back to `0.5` while pretending it was authored.
- Each present sub-field still goes through `require_ratio`
  (finite + `[0, 1]`).

The split — strict at the save layer, permissive at the source-
fixture layer — mirrors the M1.15 `active_policies` rule and
matches the user's design preference: hand-authored source data
can be sparse, but persisted runtime state must be complete.

### compare_states

`diagnostics::compare_states` walks the four new sub-fields in
canonical save-JSON order with the same tolerance machinery as the
budget block. Field paths mirror the on-disk shape:

```
countries[0].government_authority.bureaucratic_compliance
countries[0].government_authority.military_loyalty
countries[0].government_authority.intelligence_capability
countries[0].government_authority.media_control
```

So replay-equivalence tests and `--verify` CLI output reuse the
same identifier readers already know.

## 4. Tests

13 new doctest cases (M2.14 closed at 562 → M2.16 lands at 575).

`tests/core/game_state_test.cpp` (1):

- Default `CountryState.government_authority` baseline (all 0.5).

`tests/systems/data_loader_test.cpp` (5):

- Missing block → defaults to 0.5 across all four sub-fields.
- Present block → values land in the runtime struct.
- Wrong-type block (scalar in place of object) rejected with
  `government_authority` and `JSON object` in the error.
- Missing sub-field rejected with the sub-field name in the error.
- Out-of-range value rejected with both field path and value.

`tests/systems/save_system_test.cpp` (7):

- `serialize` emits the `government_authority` block.
- Round-trip preserves arbitrary values, and untouched sub-fields
  stay at default 0.5.
- v10 country missing the block rejected (mirrors M1.15 pattern
  for `active_policies` and M1.3 for `budget`).
- v10 block missing a sub-key rejected.
- v10 out-of-range value rejected via `require_ratio`.
- An old v9 save is rejected loudly (mirrors every prior
  vN-rejected pattern from M0.8 through M2.4).
- Schema-bump pin: `run` writes `"save_version": 10`.

`tests/systems/diagnostics_test.cpp` (1):

- Two countries with two differing authority sub-fields produce
  two `StateMismatch` entries with the documented field paths in
  canonical order.

Drive-by maintenance on existing tests:

- Every `"save_version": 9` literal in tests bumped to `10`.
- `TEST_CASE` names that referred to "v9 X is rejected" renamed to
  "v10 X is rejected" so the names track the current bump.
- Every hand-built v10 country JSON object that previously
  succeeded got a `government_authority` block injected between
  its budget close and its `active_policies` (or country close).
- The runner test that pins `"save_version": 9` in the emitted
  save updated to `10`.

## 5. CLI examples

None. M2.16 is library + persistence only. The CLI `--replay` /
`--verify` flows automatically pick up the new compare paths.

## 6. What's NOT in scope

Deliberate non-goals (each will land in its own PR when a real
need shows up):

- **No system reads government_authority.** No M1 monthly pipeline
  branch, no M2 command-execution gate, no diagnostics ratio
  outputs.
- **No OrderExecutionSystem / command resistance.** That's the M2.17
  candidate; M2.16 just lays the data down.
- **No new `PlayerCommandKind`.** The new fields are not directly
  mutated by commands yet; future commands like `AppointBureaucrat`
  or `LaunchPropagandaCampaign` would target them.
- **No additional authority sub-fields.** `local_control`,
  `legal_mandate`, `leader_prestige`, `party_organization` are all
  documented as deferred. The struct can grow additively in a
  future save-format bump.
- **No policy effect target type for `country.government_authority.*`.**
  The M1.5 PolicySystem doesn't gain a new target dispatch.
  Existing policies are unaffected.
- **No new CSV column.** `--countries-csv` and `--factions-csv`
  output is byte-identical with M2.14.
- **No scenario-fixture changes.** Canonical
  `data/scenarios/1930_minimal.json` and
  `1930_with_start_policies.json` load unchanged. Country fixtures
  also unchanged.
- **No `state.logs` entry.** M2.16 is data-only.
- **No AI / events / UI work.**

## 7. Cross-links

- RFC-020 §3 — full authority dimension list M2.16 ships a subset of.
- RFC-080 §6 — coup risk formula; `military_loyalty` is the
  obvious future input.
- RFC-080 §8 — information accuracy; `intelligence_capability` and
  `media_control` are the future inputs.
- M1.3 (`m1-3-budget.md`) — nested `BudgetState` pattern this PR
  mirrors.
- M1.15 (`m1-15-policy-duration-tracking.md`) — strict-at-save /
  permissive-at-source schema split.
- M0.8 (`m0-8-save-load.md`) — version-bump discipline.
- M2.10 (`m2-10-state-comparison.md`) — `compare_states` shape.

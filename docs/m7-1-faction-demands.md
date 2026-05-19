# M7.1 — 加入派系要求 (faction demands)

- Status: shipped (PR pending)
- RFC anchors: **RFC-090 §7.1**, **RFC-020 §7**, RFC-080 §1 / §11
- Save format: **v18 → v19**
- Artefact contract: 11 (unchanged)
- Branch: `feature/m7-01-faction-demands`

## 1. Scope

This sub-milestone ships the FIRST observable lifecycle for
faction demands per RFC-020 §7 "派系主動要求":

1. Data layer (`core::FactionDemand` + two closed enums +
   `state.faction_demands` vector).
2. Save round-trip (`v18 → v19`) with strict required-field
   validation and faction / country cross-checks.
3. Scenario-loader empty-state preflight extended from
   **9** containers to **10** (`state.faction_demands`).
4. Diagnostics walk so `compare_states` pins the demand
   audit-trail byte-stably across replay-verify runs.
5. Two new monthly-pipeline steps wired into
   `monthly::tick_all_countries`:
   - **Step 8**: `faction_demands::tick_generate` —
     deterministic generation predicate over
     `state.factions`.
   - **Step 9**: `faction_demands::tick_expire_and_apply` —
     status-flip + asymptotic radicalism / loyalty drift.

## 2. RFC reading (strict)

RFC-090 §7.1 says simply `加入派系要求` (add faction
demands). RFC-020 §7 enumerates six examples that anchor
the demand-kind allowlist M7.1 ships:

| RFC-020 §7 example         | Faction `type` allowlist | Demand kind                                |
| -------------------------- | ------------------------ | ------------------------------------------ |
| 軍方要求增加預算           | `military`               | `IncreaseMilitaryBudget`                   |
| 工會要求提高福利           | `workers`                | `ExpandWelfare`                            |
| 宗教勢力要求教育權         | `religious`              | `ReligiousEducationAuthority`              |
| 地方勢力要求自治           | `local_elites`           | `LocalAutonomy`                            |
| 技術菁英要求研發資金       | `technical_elites`       | `TechnocratResearchFunding`                |
| 情報部門要求監控權         | `intelligence`           | `IntelligenceSurveillanceAuthority`        |

Faction types outside this list (`bureaucracy`, `media`,
`students`, `farmers`, `nationalists`, `aristocracy`, etc.)
generate **no** demand under M7.1 — RFC-020 §7 does not
enumerate them. A future sub-milestone that needs additional
kinds widens both the `FactionDemandKind` enum and the
predicate in lock-step with an additive save bump.

RFC-020 §7 itself does NOT authorise a satisfaction surface
inside §7. M7.1 therefore ships only two statuses
(`Pending`, `Expired`); the satisfaction path (player
command? AI?) is reserved for a later sub-milestone.

## 3. Data layer (save v18 → v19)

### 3.1 New types

```cpp
enum class core::FactionDemandKind {
    IncreaseMilitaryBudget,
    ExpandWelfare,
    ReligiousEducationAuthority,
    LocalAutonomy,
    TechnocratResearchFunding,
    IntelligenceSurveillanceAuthority,
};

enum class core::FactionDemandStatus { Pending, Expired };

struct core::FactionDemand {
    std::string         id_code;
    std::string         faction_id_code;
    std::string         country_id_code;
    FactionDemandKind   kind;
    GameDate            created_on;
    GameDate            expires_on;
    FactionDemandStatus status;
};
```

### 3.2 New container

```cpp
std::vector<FactionDemand> GameState::faction_demands;
```

### 3.3 Save v19 JSON shape

```json
"faction_demands": [
  {
    "id_code": "GER_military_demand_increase_military_budget_1930-04-01",
    "faction_id_code": "GER_military",
    "country_id_code": "GER",
    "kind": "increase_military_budget",
    "created_on": "1930-04-01",
    "expires_on": "1930-05-31",
    "status": "pending"
  }
]
```

Strict loader validation:

- All seven fields required.
- `id_code`, `faction_id_code`, `country_id_code` must be
  non-empty strings.
- `kind` parsed via the closed snake_case allowlist
  (`increase_military_budget` / `expand_welfare` /
  `religious_education_authority` / `local_autonomy` /
  `technocrat_research_funding` /
  `intelligence_surveillance_authority`).
- `status` parsed via the closed snake_case allowlist
  (`pending` / `expired`).
- `created_on` / `expires_on` parsed via
  `GameDate::parse`; `expires_on >= created_on`.
- `faction_id_code` cross-checked against `state.factions`;
  the resolved faction's `country_id_code` must equal the
  demand's `country_id_code`.
- `country_id_code` cross-checked against `state.countries`.

Pre-v19 saves are rejected with the standard
version-mismatch error.

### 3.4 Scenario-loader empty-state preflight

`scenario_loader::load_into_state` now rejects 10 containers
on entry (was 9): the runtime-carryover row gains
`faction_demands` alongside `pending_player_events` and
`event_history`.

## 4. Generation predicate (`tick_generate`)

For each faction `f` in `state.factions` (insertion order,
deterministic):

1. If `f.type` is outside the RFC-020 §7 allowlist, SKIP
   (no demand generated, no validation performed on `f`'s
   numerical fields — that is M1.6 `faction::react`'s
   responsibility).
2. Validate `f.radicalism` is a finite ratio in `[0, 1]`.
   If not, **fail loudly** with the faction's `id_code` and
   the offending value (per
   `feedback_no_silent_degradation`).
3. If `f.radicalism <= kFactionDemandGenerateRadicalism-
   Threshold = 0.50`, SKIP.
4. Validate `f.id_code` and `f.country_id_code` are
   non-empty; validate `f.country_id_code` resolves to an
   entry in `state.countries`; validate `f.loyalty` is a
   finite ratio in `[0, 1]`.
5. If there is already a `Pending` demand for the
   `(faction_id_code, kind)` pair, SKIP (no
   double-generation).
6. Append a new `FactionDemand{kind, status=Pending,
   created_on=current_date, expires_on=current_date +
   kFactionDemandLifetimeDays=60, id_code=
   "{faction_id_code}_demand_{kind_snake}_{created_on}"}`.

Determinism / atomicity:

- Walks `state.factions` in insertion order.
- Pure read except for the `state.faction_demands` append.
- No `state.rng` consumption.
- On any validation failure, returns `Result::failure`
  BEFORE any append (candidate-validate-commit pattern).
  All eligible demands are appended together or none.

## 5. Expiration + drift (`tick_expire_and_apply`)

For each Pending demand `d` whose `expires_on <=
current_date`:

1. Locate the issuing faction by `faction_id_code`.
   **Fail loudly** if the lookup fails (per
   `feedback_no_silent_degradation`).
2. Validate the faction's `radicalism` and `loyalty` are
   finite ratios in `[0, 1]`.
3. Pre-compute the asymptotic drift candidates:
   - `radicalism' = radicalism + 0.05 × (1 - radicalism)`
   - `loyalty'    = loyalty    - 0.03 × loyalty`
4. Verify both candidates land in `[0, 1]` (asymptotic
   form guarantees this when inputs are valid; the check
   guards against silent corruption).
5. Flip `d.status` to `Expired`; mutate the faction's
   radicalism and loyalty.

The asymptotic shape matches the post-PR #115 hardening
convention for ratio fields. Expired demands stay in
`state.faction_demands` as an audit trail; they do NOT
regenerate effects on subsequent ticks (the `Expired`
flip is the gate).

## 6. Game-model coefficients (RFC-080 §1, §11)

Each coefficient is a game-model assumption. The CITED
direction grounding is:

- **`kFactionDemandGenerateRadicalismThreshold = 0.50`**
  - Direction: Collier & Hoeffler "Greed and Grievance in
    Civil War" (grievance-opportunity model) +
    Alesina & Perotti "Income Distribution, Political
    Instability, and Investment" (factions with elevated
    discontent become politically active).
  - Coefficient: game-model midpoint. Neither paper pins
    a numeric activation threshold.

- **`kFactionDemandLifetimeDays = 60`**
  - Direction: RFC-020 §7 does not specify a timeline.
    60 days ≈ two monthly ticks, enough for a player to
    respond when the satisfaction surface lands in a
    later sub-milestone.
  - Coefficient: pure game-model.

- **`kFactionDemandExpireRadicalismAsymptoticDelta = 0.05`**
  - Direction: Alesina-Perotti — unaddressed grievance →
    instability. Positive delta on radicalism.
  - Asymptotic form: matches the post-PR #115 hardening
    convention for ratio fields.
  - Coefficient magnitude: game-model.

- **`kFactionDemandExpireLoyaltyAsymptoticDelta = 0.03`**
  - Direction: an unaddressed demand erodes the faction's
    loyalty to the regime.
  - Smaller magnitude than the radicalism delta so a
    single expired demand is not catastrophic.
  - Coefficient: game-model.

## 7. Monthly pipeline integration

`monthly::tick_all_countries` step list after M7.1:

```
1.  per-country tick_country (faction::react +
                              stability::tick +
                              economy::tick)
2.  interest_group::react              (M3.2)
3.  interest_group::country_feedback   (M3.3)
4.  interest_group::authority_pressure (M3.4)
5.  -- (slot reserved by M3.x)
6.  -- (slot reserved by M3.x)
7.  ai_policy::apply_selected_policies (issue #108)
8.  faction_demands::tick_generate          ← M7.1 (NEW)
9.  faction_demands::tick_expire_and_apply  ← M7.1 (NEW)
10. event_engine::tick_events          (M5.8)
```

Ordering rationale:

- Step 8 runs AFTER ai_policy (so AI mutations on budget /
  authority are observable to future demand kinds that
  read them).
- Step 8 runs BEFORE step 9 — `kFactionDemandLifetimeDays
  > 0` guarantees a freshly-generated demand cannot also
  expire this tick.
- Steps 8 + 9 run BEFORE event_engine::tick_events so M5
  events keyed off faction radicalism observe the
  expiration drift.

## 8. Counters surfaced through `MonthlyOutcome`

`MonthlyOutcome` gains four new counters (zero on a month
with no demand activity):

```cpp
int faction_demands_factions_considered;  // == state.factions.size()
int faction_demands_generated;            // newly Pending demands appended
int faction_demands_expired;              // Pending → Expired transitions
int faction_demands_factions_affected;    // distinct factions drifted
```

## 9. Canonical / compliance scenario impact

- **Canonical `1930_minimal` 365-day sweep**: byte-stable
  with respect to faction radicalism dynamics — canonical
  GER military / workers / bureaucracy radicalism start
  below 0.5 and drift toward `1 - stability ≈ 0.45` under
  M3.2, never crossing the M7.1 threshold. `faction_demands`
  remains `[]` for the whole sweep.
- **Compliance `1930_rfc_compliance` 25 567-day (1930→2000)
  sweep**: completes with `Sanity issues : 0` on both
  debug and non-debug runs. Compliance scenario carries
  only the same three GER factions as canonical, so
  demands are likewise not generated for the full sweep.
  The faction-demand mechanism is exercised by the new
  `tests/systems/faction_demands_test.cpp` unit tests
  (hand-built states above the threshold).

## 10. Tests + verification

- `cmake --build build --config Debug` succeeds.
- `build/bin/Debug/leviathan_tests.exe` reports
  **1325 test cases, 96 375 assertions, 0 failed**
  (verified via direct binary run per
  `feedback_ctest_masks_doctest`).
- Delta from M6 closeout audit (1301 / 96228):
  - +24 unit tests in `tests/systems/faction_demands_test.cpp`
    (enum string round-trip, faction-type → demand-kind
    allowlist, generation predicate at / above /
    below threshold, no double-generation while Pending,
    determinism across repeated runs, strict NaN /
    invalid-country / invalid-date rejections,
    expiration status flip + asymptotic drift, no
    re-trigger after Expired, multi-demand factions_
    affected count, save round-trip, deserialize
    rejections for unknown kind / unresolvable
    faction / missing required field).
  - +123 assertions across those new tests.

## 11. What this PR deliberately does NOT do

- No `Satisfied` lifecycle state. RFC-020 §7 ships
  the active-demand side; the response surface is
  unauthorised by §7 and lives in a later sub-milestone.
- No player-facing command. No `PlayerCommandKind`
  addition. No new CLI flag.
- No `state.rng` consumption.
- No new artefact. `state.faction_demands` surfaces
  through `save.json` and indirectly through
  `interest_groups.csv` (via the radicalism drift on
  expiration).
- No new gameplay system module beyond the M7.1 helper.
- No "M6 closed" claim. M6 closure decision is reserved
  for Jason (see `docs/m6-closeout-audit.md` §1).
- No M7.2+ work started. Per
  `feedback_milestone_direction_gate`, wait for
  explicit go-ahead before opening the next sub-milestone
  PR.

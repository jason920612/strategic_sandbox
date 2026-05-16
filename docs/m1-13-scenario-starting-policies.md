# M1.13 - Scenario starting policies

Companion notes for `feature/m1-13-scenario-starting-policies`. M1.13
lets a scenario manifest declare a set of **day-0 policy enactments**
that the scenario loader applies exactly once during load. No new
gameplay system; no save-format bump; no state-shape change.

## 1. Scope

After M1.11 the scenario manifest could load countries / factions /
policies. After M1.12 stability reads economy growth. But the loaded
policies were inert templates — there was no way for a scenario to say
"GER starts with raised taxes" without hand-injecting state in a test.

M1.13 closes that gap with one optional manifest array:

```json
"starting_policies": [
  { "policy": "raise_taxes",              "actor": "GER" },
  { "policy": "increase_military_budget", "actor": "GER" }
]
```

After loading the manifest's countries / factions / policies, the
loader iterates this array, resolves each entry's `policy` and `actor`
id_codes to numeric IDs, and calls
`policy::apply_policy_effects(state, actor, policy)` exactly once
per entry.

That's it. Subsequent monthly pipeline calls do not look at the
manifest's starting_policies again.

## 2. Manifest schema extension

| Field | Required | Type | Validation |
|---|---|---|---|
| `scenario.starting_policies` | optional | array | Absent ⇒ empty list (M1.11 manifests stay valid) |
| each entry | — | object | Must have `policy` (string) and `actor` (string) |
| `policy` | required | string | Must match a loaded `PolicyData::id_code` |
| `actor` | required | string | Must match a loaded `CountryState::id_code` |

Failure messages name the entry index and the offending id_code so
the user can find the bad line.

## 3. Apply order

Day-0 enactments run **after** countries / factions / policies are
loaded and **in manifest order**. Multiple entries that touch the
same field accumulate: e.g. two policies both `+0.05` on
`country.legal_tax_burden` produce a `+0.10` net change.

This means **manifest order matters** if any policy reads state that
a prior policy wrote (e.g. `set` operations override `add` from an
earlier entry). The order rule is documented and pinned by the
multi-entry test that verifies `0.20 + 0.05 + 0.10 = 0.35` exactly.

## 4. Atomicity

Each individual `policy::apply_policy_effects` call is atomic
(M1.5's pre-flight rule). But across the `starting_policies` array
the loader is **not** atomic: if entry `[2]` fails, entries `[0]`
and `[1]` have already applied and stay applied.

This matches M1.11's documented non-atomic behaviour for partial
file load failures. Atomic multi-step loads remain a deliberate
non-goal until a real recovery scenario justifies the
bookkeeping.

## 5. What M1.13 deliberately does NOT do

- **No duration queue.** Policies' `duration_days` field is read but
  not interpreted; M1.13 applies effects once and is done.
- **No active-policy container** on `GameState`.
- **No monthly policy scheduler.** The monthly pipeline does not see
  starting_policies.
- **No AI / automatic enactment.** Only manifest-declared entries
  fire.
- **No event-triggered policy enactment.**
- **No `state.policies` mutation.** Policies stay inert templates in
  `state.policies`; the only writes are to `CountryState` /
  `FactionState` (per M1.5's existing target paths).
- **No new `CountryState` / `FactionState` / `PolicyData` fields.**
- **No save-format bump.** Stays at **v6**. `apply_policy_effects`
  only writes fields that already persist; the absence of an
  active-policy container means there's nothing new to save.
- **No scenario manifest catalog / discovery.**
- **No RNG, no new logs, no new CSV column.**
- **No balance pass / coefficient tuning.**

## 6. Determinism

The day-0 apply step is:

- **RNG-free.** `policy::apply_policy_effects` is pure arithmetic.
- **Log-free.** The loader does not call into LoggingSystem.
- **Date-free.** `state.current_date` is untouched.
- **Order-stable.** Manifest array order is the on-disk JSON order.

Same-seed + same-scenario + same-days produces byte-identical save +
log + summary CSV. Pinned by a runner test that runs 90 days with
the new fixture and `--seed 0xDA7AC0DE` twice.

## 7. State touched

- **WRITES (via `apply_policy_effects`)**: `CountryState` fields that
  the policy's effects target (anything M1.5's target paths recognise
  — `country.<field>`, `country.budget.<category>`,
  `faction:<type>.<field>`).
- **READS**: `state.policies` (to resolve `policy` id_code),
  `state.countries` (to resolve `actor` id_code and to verify
  faction membership for `faction:*` targets).
- **NO new writes** at the scenario-loader layer; everything flows
  through the existing M1.5 system.

## 8. New canonical fixture

`data/scenarios/1930_with_start_policies.json` — same world as
`1930_minimal.json` plus two day-0 entries:

```json
"starting_policies": [
  { "policy": "raise_taxes",              "actor": "GER" },
  { "policy": "increase_military_budget", "actor": "GER" }
]
```

After load, GER's `legal_tax_burden` is `0.25` (was `0.20`) and
`military_power` is `0.53` (was `0.50`). The fixture exists so the
runner integration test has a stable on-disk artifact, AND so
manual smoke runs (`leviathan --days 365 --scenario
data/scenarios/1930_with_start_policies.json`) produce visibly
different multi-month trajectories from the M1.11 fixture.

## 9. Test coverage (15 new cases)

**parse_manifest (7)**: absent key parses as empty (M1.11
back-compat); happy path; not-array rejected; entry-not-object
rejected; missing `policy` field rejected; missing `actor` field
rejected; wrong-type `policy` field rejected.

**load_into_state (5)**: synthetic temp-dir day-0 enactment changes
`country.legal_tax_burden` by `+0.05`; unknown `policy` id_code
rejected; unknown `actor` id_code rejected; invalid-target policy
propagates `apply_policy_effects` error; multiple entries apply in
order with cumulative effects (`0.20 → 0.25 → 0.35`).

**Runner integration (3)**: canonical fixture with starting policies
produces `legal_tax_burden = 0.25` and `military_power = 0.53` after
day 0; same seed + fixture + 90 days byte-identical save + log; the
M1.11 canonical fixture (no starting_policies) still loads cleanly
with `legal_tax_burden == 0.20`.

**Plus** the existing canonical-1930_minimal test gains one CHECK
that `starting_policies_applied == 0`, pinning the M1.11 fixture's
unchanged behaviour.

Total: **367 → 382** (+15).

## 10. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/systems/scenario_loader.hpp` | adds `StartingPolicy`, fields on `ScenarioManifest` and `ScenarioLoadOutcome` |
| `src/leviathan/systems/scenario_loader.cpp` | parses `starting_policies`; applies each via `policy::apply_policy_effects` |
| `tests/systems/scenario_loader_test.cpp` | 12 new cases + 1 CHECK update |
| `tests/systems/runner_test.cpp` | 3 new cases + new fixture constant |
| `data/scenarios/1930_with_start_policies.json` | **new** canonical fixture |
| `docs/m1-13-scenario-starting-policies.md` | this file |
| `docs/README.md`, `README.md`, `rfc/README.md` | M1.13 status / progress |

No CMake changes. No save-format bump (stays at **v6**). No new
public types beyond `StartingPolicy`. Total tests: **367 → 382**
(+15).

## 11. Risks / things to watch

- **Mid-list apply failure leaves partial state.** Documented above
  and consistent with M1.9 / M1.11 non-atomic stances.
  Revisit when real recovery requirements exist.
- **No diagnostic warning for redundant enactment.** A scenario
  that lists the same policy twice on the same actor will apply
  it twice. That's intentional (some policies make sense to
  stack), but it can also be a typo. A future `sanity_check`
  rule could flag it; M1.13 doesn't.
- **`starting_policies` count is not surfaced on `RunOutcome`.**
  `ScenarioLoadOutcome::starting_policies_applied` reaches the
  loader caller but the runner doesn't propagate it. If
  diagnostics ever needs it, a one-line addition to `run()` will
  suffice.
- **Policies referencing factions the scenario didn't load are
  silent no-ops.** Per M1.5, a `faction:<type>.<field>` target
  that matches zero factions is documented as a no-op. A starting
  policy that enacts e.g. `faction:students.support` on a country
  with no students faction simply does nothing — neither failure
  nor warning. Consistent with M1.5's design; flagged here so
  scenario authors know.
- **Manifest order matters.** Two entries on the same field
  accumulate; documented and tested.

## 12. Next sub-milestone

Likely candidates (RFC-090 §M1):

- **M1.14 — Diagnostics surfaces `last_gdp_growth_rate`.** Carry
  over from M1.12's follow-up list: add a CSV column or snapshot
  field so multi-month runs are inspectable without round-tripping
  the save file.
- **M1.15 — policy duration tracking.** Add a per-country
  active-policy list with `policy_id_code + expires_on` entries;
  TimeSystem removes expired policies. **Would require a save-
  format bump** (`v6 → v7`) for the active-policy container, so
  the [[feedback_save_version]] rule applies.

Per the M1 pacing rule: do **not** start M1.14 or M1.15 until M1.13
is reviewed and merged.

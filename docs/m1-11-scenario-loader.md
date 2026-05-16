# M1.11 - Scenario loader for runner

Companion notes for `feature/m1-11-scenario-loader`. M1.11 lets the
headless runner produce a **non-empty world** without test-only state
injection. It composes M0.7 / M1.1 / M1.2 / M1.4 DataLoader parsers
into a single manifest-driven loader, plus a `--scenario PATH` flag
on the runner. No new gameplay; no new state shape; no save-format
bump.

## 1. Scope

A scenario is described by a tiny JSON manifest that lists the
country / faction / policy fixtures to load. The loader composes
the existing DataLoader parsers, assigns numeric IDs by vector
order, resolves faction `country_id_code` strings to numeric
`CountryId` values, and rejects every plausible mistake (duplicate
id_codes, missing country references, pre-populated state).

Everything else stays out:

- No policy enactment (loader stores templates inert).
- No active-policy container, no duration queue, no scheduler.
- No RNG, no logging, no date mutation, no save-schema change.
- No "scenario discovery" — caller hands the manifest path in.
- No content beyond what M0.7 / M1.1 / M1.2 / M1.4 already parse.

## 2. Manifest schema

```json
{
  "scenario": {
    "countries": [ "countries/germany.json", "countries/france.json" ],
    "factions":  [ "factions/ger_military.json" ],
    "policies":  [ "policies/raise_taxes.json" ]
  }
}
```

All three arrays are required (they may be empty). Each entry is a
**relative path** resolved against
`manifest_path.parent_path().parent_path()`.

Concrete example: a manifest at `data/scenarios/1930_minimal.json`
treats `"countries/germany.json"` as `data/countries/germany.json`.

This convention keeps the manifest portable (no absolute paths) and
matches the existing `data/{countries,factions,policies}/` layout.

## 3. ID assignment

| Entity | Numeric id rule |
|---|---|
| Country | `CountryId{i}` where `i` is the vector index in `manifest.countries`. |
| Faction | `FactionId{i}` where `i` is the vector index in `manifest.factions`. |
| Policy  | `PolicyId{i}`  where `i` is the vector index in `manifest.policies`. |

For factions, `FactionState::country_id_code` (parsed from the
on-disk `"country"` field) is resolved to a numeric `CountryId` by
looking up an already-loaded country with the matching `id_code`.
If no such country exists, the load fails with the offending faction
id_code and the missing country id_code in the message.

## 4. Validation rules

Each rejection produces a `Result::failure` whose message names the
offending file / id_code / path.

- Duplicate `country.id_code`, `faction.id_code`, or
  `policy.id_code` is rejected on the second occurrence.
- A faction whose `country_id_code` doesn't match any loaded
  country is rejected.
- A non-empty `state.countries` / `state.factions` / `state.policies`
  on entry rejects the call. The M1.11 loader does NOT append; it
  starts from a blank state.
- Any DataLoader-level error (missing file, malformed JSON, schema
  violation) is propagated unchanged.

## 5. Runner `--scenario` flag

```
leviathan --days 365 --scenario data/scenarios/1930_minimal.json
```

`RunnerOptions` gains:

```cpp
std::optional<std::filesystem::path> scenario_path;
```

Flow:

1. `run()` loads simulation config and applies `--seed` override.
2. `run()` calls `make_game_state(cfg)`.
3. **If `scenario_path` is set**, `run()` calls
   `scenario_loader::load_into_state(state, opts.scenario_path.value())`.
   Failure aborts before any file is written.
4. `run_state(state, opts)` runs the tick loop (M1.10).

Without `--scenario` the runner ticks an empty world (M0.9 / M1.10
behaviour). M1.11 is **purely additive** at the CLI surface.

## 6. Empty-world behaviour without `--scenario`

Unchanged from M1.10: `monthly_ticks` increments on every month
boundary, but each call to `monthly::tick_all_countries` processes
zero countries. The save file's `countries / factions / policies`
arrays are empty. Same-seed runs remain byte-identical.

## 7. Determinism

The scenario loader does no RNG, no logging, no date mutation. It
reads files in the order the manifest declares. The same manifest
on the same disk produces identical post-load `GameState`s, and
therefore the same-seed-byte-identical guarantee survives.

Pinned by the
`"run: --scenario same seed + days produces byte-identical save and log"`
test.

## 8. State touched

- **WRITES** `state.countries`, `state.factions`, `state.policies`.
  All three start empty (loader rejects otherwise) and are
  populated by index order.
- **DOES NOT WRITE** `state.current_date`, `state.rng`,
  `state.logs`, or any persistent shape — these stay exactly as
  `make_game_state(cfg)` constructed them. Save format is
  unchanged (still `v5`).

## 9. Test coverage (25 new cases)

### Scenario loader (17)

- `parse_manifest` happy path; empty arrays allowed.
- `parse_manifest` rejects: malformed JSON, top-level non-object,
  missing `scenario` wrapper, missing `countries` array, wrong-type
  `countries`, non-string array element.
- `load_into_state` end-to-end with a synthetic temp-dir layout:
  countries/factions/policies all populated, IDs assigned in order,
  faction `country` resolves to the right `CountryId`.
- `load_into_state` rejection cases: duplicate country id_code,
  duplicate faction id_code, duplicate policy id_code, missing
  country reference, pre-populated state, missing manifest file,
  missing fixture file.
- Canonical `data/scenarios/1930_minimal.json` loads cleanly:
  3 countries (`GER`, `FRA`, `JPN`), 3 factions (all GER), 10
  policies (every canonical fixture).

### Runner integration (8)

- `parse_args` plumbs `--scenario`; `--scenario` without value
  is rejected; default state is `nullopt`.
- `run` without `--scenario` still produces an empty-world save
  (M1.10 contract preserved).
- `run` with the canonical scenario produces a save mentioning
  every loaded `id_code` (`GER` / `FRA` / `JPN` / `GER_military` /
  `increase_military_budget`) and still has `save_version: 5`.
- 31-day run actually mutates country state — Germany's GDP no
  longer reads `100.0`, `tax_revenue` is no longer `0.0`.
- Same seed + scenario + days produces byte-identical save + log.
- Bad scenario path fails with the path in the message and
  produces no save / log files (failure happens before
  `run_state`'s file writes).

## 10. What M1.11 deliberately does NOT do

- **No `last_gdp_growth_rate` field on `CountryState`.**
- **No economy → stability coupling.** Stability still doesn't see
  `gdp_growth_rate`.
- **No save-format bump.** Stays at v5.
- **No policy enactment / scheduling / active-policy container /
  duration tracking.** Policies are inert templates after load.
- **No AI / events / diplomacy / war / coup / civil war.**
- **No new logging from the scenario loader or the runner load
  step.** The same M0.9 log stream pattern survives.
- **No RNG use.** Same-seed byte-identical determinism preserved.
- **No partial-atomicity rollback** on mid-load failure. The loader
  leaves whatever partial state the failure-time vectors held —
  consistent with M1.9 pipeline's documented stance.
- **No scenario discovery / catalog.** Manifest path is explicit.
- **No `Diagnostics::sanity_check` rule for scenario-specific
  invariants.** The existing duplicate-id-code rule covers a
  bigger surface now, but no new rule lands here.

## 11. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/systems/scenario_loader.hpp` | **new** — public API |
| `src/leviathan/systems/scenario_loader.cpp` | **new** — implementation |
| `src/leviathan/systems/CMakeLists.txt` | adds `scenario_loader.cpp` |
| `include/leviathan/systems/runner.hpp` | adds `scenario_path` field |
| `src/leviathan/systems/runner.cpp` | adds `--scenario` flag parsing + load step |
| `tests/systems/scenario_loader_test.cpp` | **new** — 17 cases |
| `tests/systems/runner_test.cpp` | adds 8 M1.11 integration cases |
| `tests/CMakeLists.txt` | adds the new test source |
| `data/scenarios/1930_minimal.json` | **new** — canonical scenario |
| `docs/m1-11-scenario-loader.md` | this file |
| `docs/README.md`, `README.md`, `rfc/README.md` | M1.11 status / progress |

Total tests: **327 -> 352** (+25). Save format version: still
**v5**.

## 12. Risks / things to watch

- **Loader leaves partial state on mid-load failure.** Documented
  in the header. The right time to add atomic loading is when a
  failure mode that callers actually want to recover from arises;
  today the cost / benefit doesn't justify the extra two-phase
  bookkeeping.
- **Manifest path resolution assumes a `data/<kind>/...` layout.**
  Scenarios placed outside `data/scenarios/` will resolve relative
  paths against a different grandparent. This is by design — the
  rule keeps manifests portable — but a future PR that adds custom
  scenarios may want a `base_dir` override field on the manifest
  itself.
- **Canonical scenario only has Germany factions.** France and
  Japan load as country shells without factions. Their monthly
  pipeline still runs and `faction::react` no-ops for them
  (M1.6's "no factions" branch). The economy / stability ticks
  use M1.7's `0.5 / 0.5` defaults for no-faction countries.
- **Policy templates load but never run.** Future PRs that wire
  policy enactment must integrate them; the M1.11 contract is
  "they exist in `state.policies`, the loader does not enact
  them".
- **Duplicate-id-code detection is per-kind.** A country and a
  policy can share the same id_code without conflict because
  they're in different namespaces. Document this if it ever
  causes confusion.
- **`runner.cpp` still has the hardcoded `2147483647` from M1.10.**
  Re-flagged in design notes; cleanup is a tiny stand-alone PR.

## 13. Next sub-milestone

Likely candidates (RFC-090 §M1):

- **M1.12 — economy → stability coupling.** Add
  `last_gdp_growth_rate` to `CountryState` (save format
  `v5 → v6` per [[feedback_save_version]]) and feed it into
  `stability::tick` as the RFC-080 §5 `EconomicGrowth` input.
  Now that the runner has fixtures to drive, this stops being
  hypothetical and starts producing observable effects in
  multi-month runs.
- **M1.13 — policy enactment from scenario.** A scenario could
  optionally enact a small set of policies at start time; that
  would need a "starting policies" list in the manifest and a
  hook in the loader to call `policy::apply_policy_effects`.

Per the M1 pacing rule: do **not** start M1.12 (or anything else)
until M1.11 is reviewed and merged.

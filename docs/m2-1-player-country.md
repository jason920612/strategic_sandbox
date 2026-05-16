# M2.1 - Player country selection

Companion notes for `feature/m2-01-player-country-selection`. M2.1
opens Milestone 2 (player-operation prototype) with the smallest
useful seam: a single `CountryId` field on `GameState` that marks
which country the player has chosen, a `--player COUNTRY_IDCODE`
runner flag that resolves the choice from the loaded scenario, and
a save-format bump that round-trips the selection.

**No behaviour change yet.** Pause/resume, command queue, command log,
UI, AI, events — all explicit non-goals (see §6). M2.1 is the data +
plumbing layer that the next M2 sub-milestones will react against.

## 1. Scope

M1 ran the simulation headless: scenarios loaded, the monthly pipeline
ticked, diagnostics were emitted. Nothing in M1 cared *which* country
the user was watching. M2 introduces that distinction.

M2.1 ships:

- a new `GameState::player_country` field (default `CountryId::invalid()`),
- a new `--player COUNTRY_IDCODE` CLI flag,
- runner resolution of the id_code against the loaded scenario,
- save-format bump v7 → v8 so the choice survives a save/load
  round trip.

That's the whole PR. M1 systems are untouched: `faction::react`,
`stability::tick`, `economy::tick`, `monthly::tick_all_countries`,
the diagnostics CSVs, `policy::apply_policy_effects`, all behave
identically. The player choice is data only; no system branches on
it yet.

## 2. Public API

`include/leviathan/core/game_state.hpp`:

```cpp
struct GameState {
    GameDate    current_date{1930, 1, 1};
    RandomState rng{};

    // M2.1: which country the player has selected, or invalid()
    // when running headless / unattended. A valid value MUST index
    // into `countries`; the save loader enforces this.
    CountryId   player_country = CountryId::invalid();

    std::vector<CountryState>    countries;
    // ... (unchanged) ...
};
```

`include/leviathan/systems/runner.hpp`:

```cpp
struct RunnerOptions {
    // ... existing flags ...
    std::optional<std::string> player_id_code;  // --player COUNTRY_IDCODE
};
```

The runner does not currently surface `player_country` in `RunOutcome`
or `main()`'s stdout summary. Reading it back from the save is the
canonical inspection path; tests use that to verify resolution.

## 3. Runner resolution

`run_state(state, opts)` resolves `opts.player_id_code` immediately
after entry, **after** scenario load (which runs in `run()` before
`run_state`) and **before** the initial lifecycle log:

```cpp
if (opts.player_id_code.has_value()) {
    const std::string& want = opts.player_id_code.value();
    if (state.countries.empty()) {
        return Result::failure("--player " + want + " requires a non-empty"
                               " state.countries (typically via --scenario)");
    }
    bool found = false;
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        if (state.countries[i].id_code == want) {
            state.player_country = CountryId{static_cast<int>(i)};
            found = true;
            break;
        }
    }
    if (!found) {
        return Result::failure("--player " + want +
                               ": no country with that id_code in the"
                               " loaded scenario");
    }
}
```

Properties:

- Linear scan is fine — country count is single-digit / low-double-
  digit through M2. A future scenario loader can build an
  `id_code -> CountryId` map if it ever matters.
- Resolution runs in `run_state`, so tests that hand-build a state
  and call `run_state(state, opts)` directly get the same code path.
- Failure short-circuits before any log or snapshot is emitted, so
  a bad `--player` does not leave a half-written artefact.

## 4. Save format v7 → v8

Schema history (from `save_system.hpp`):

```
v7 (M1.15) — CountryState gained active_policies.
v8 (M2.1)  — GameState gained player_country at the root level
             (CountryId; default invalid()/-1 for headless runs).
             A v7 save has no field for it, and silently defaulting
             to invalid() on reload would drop the player's country
             selection.
```

The save JSON gets one new root-level field:

```json
{
  "save_version": 8,
  "rng_algorithm_version": 1,
  "current_date": "1930-01-01",
  "player_country": -1,
  "rng": { "seed": 0, "counter": 0 },
  "countries": [ /* unchanged */ ],
  ...
}
```

Loader rules:

- `save_version != 8` rejected loudly (same gate as v1..v6 / v7).
- `player_country` missing → hard failure
  (`fmt_err(source_label, "missing required field 'player_country'")`).
- `player_country` not an integer → reject.
- `player_country < -1` → reject with the actual value in the error.
- `player_country > INT_MAX` → reject with the CountryId range
  message (matches the existing `country.id` int-range gate).
- `player_country != -1 && player_country >= state.countries.size()`
  → reject with the offending index and the actual country count
  in the error.

The validation runs **after** the `countries` array has been
deserialised, so the index-range check works against the live
count rather than a placeholder.

## 5. Tests

17 new doctest cases (M1.17 was 435 → M2.1 is 452).

`tests/core/game_state_test.cpp` (extended existing case): the
default `GameState` now also asserts `player_country == invalid()`.

`tests/systems/save_system_test.cpp` (9 new cases):

- rejects an old v7 save loudly
- serialize emits `"player_country": -1` for a default GameState
- save+load round-trips `player_country = -1`
- save+load round-trips `player_country = CountryId{1}` (a real
  index from `build_seeded_state`'s 2-country fixture)
- v8 missing `player_country` is rejected
- v8 `player_country` non-integer (string `"GER"`) is rejected
- v8 `player_country = -7` (below -1 sentinel) is rejected with
  the `">= -1"` message
- v8 `player_country = 0` against empty countries is rejected
  with `"out of range"`
- v8 `player_country = 2147483648` (above CountryId int range) is
  rejected with the `"CountryId"` message

Every other existing fixture that previously declared
`"save_version": 7` had `"player_country": -1` added so it still
reaches its intended assertion past the new validation step.

`tests/systems/runner_test.cpp` (8 new cases):

- `--player` flag plumbed into `RunnerOptions::player_id_code`
- `--player` with no value rejected
- `--player` defaults to unset when absent
- `run` with `--player GER` but no scenario / empty world rejected
- `run` with `--player BOGUS` (unknown id_code) rejected with
  `"no country with that id_code"`
- `run` with `--player GER` + canonical scenario → save file
  contains `"player_country": 0`
- `run` with `--player FRA` → save contains `"player_country": 1`
- `run` without `--player` → save contains `"player_country": -1`

## 6. What's NOT in scope

Deliberate non-goals for M2.1, called out so future contributors do
not silently unwind them:

- **No pause / resume / step control.** The runner still advances
  days in a tight loop. M2.2 is the pause/resume sub-milestone.
- **No player command queue.** M2.3 introduces a first-class command
  struct. M2.1 sets `player_country` and stops.
- **No player command log.** M2.4 territory.
- **No UI / map.** Milestone 4.
- **No AI / events.** Far future.
- **No multi-player.** A single `CountryId` field is intentional.
- **No system branches on `player_country`.** None of `faction::react`,
  `stability::tick`, `economy::tick`, `policy::apply_policy_effects`,
  the monthly pipeline, or the diagnostics readers consult the
  field. This keeps every M1 determinism test passing unchanged.
- **No new logs.** `--player` resolution does not emit a log entry
  on success or failure beyond the `Result` error string. Adding a
  "player selected: GER" log would extend the byte-stable log
  artefact and require updating the M1.17 5-artefact determinism
  test; we keep the surface minimal for now.
- **No JSON-config / DataLoader change.** Scenario manifests do not
  carry a player choice. That belongs to the runner (or a later
  scenario-level "default player" field, if it ever becomes
  necessary).

## 7. Cross-links

- RFC-090 §M2 — Milestone 2 roadmap; M2.1 is the first sub-milestone.
- M1.11 (`m1-11-scenario-loader.md`) — scenario loader that populates
  `state.countries`; M2.1 resolves `--player` against the resulting
  vector.
- M1.13 (`m1-13-scenario-starting-policies.md`) — precedent for
  resolving an id_code against a loaded scenario vector.
- M1.15 (`m1-15-policy-duration-tracking.md`) — previous save-format
  bump (v6 → v7); same loud-rejection pattern reused here.
- M1.17 (`milestone-1-result.md`) — M1 exit report; §3 sketched
  the M2.1 shape almost verbatim.

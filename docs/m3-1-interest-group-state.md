# M3.1 - InterestGroupState / political actors skeleton

Companion notes for `feature/m3-1-interest-group-state`. M3.1
opens Milestone 3 with a stripped-down data layer for political
interest groups / actors. The block is intentionally a skeleton:
no system reads or writes the new fields, no monthly reaction
runs, no event fires, no faction consumes them. Future M3 sub-
milestones layer behaviour onto this shape.

## 1. Scope

M2 closed with the player able to issue commands, the government
able to gate those commands by authority levels, and replay
reproducing the whole chain deterministically. The natural next
step is to model the **political actors** those commands are
adjudicated against: bureaucracy, military, workers, farmers,
religious blocs, media, students, local elites, business,
technocrats.

M3.1 ships only the data shape. Behavioural M3 work (reactions,
demands, support shifts, command-resistance contributions, coup
risk inputs, event triggers) lands in M3.2+, each as its own
narrow PR.

### What lands

- New `core::InterestGroupKind` enum (10 variants).
- New `core::InterestGroupState` POD: id_code, name, kind,
  country (numeric handle), influence, loyalty, radicalism.
- New `GameState::interest_groups` root-level vector.
- Save format **v10 → v11** with the block required at the save
  layer and validated entry-by-entry.
- scenario_loader optional `interest_groups` block: missing key
  ⇒ empty list, present block validated.
- `diagnostics::compare_states` walks the new array
  field-by-field.
- 20 new doctest cases.

### What does NOT land

See §6 for the full list. Highlights: no monthly reactions, no
coup / strike / protest, no AI, no event triggers, no command-
resistance integration, no UI, no CLI changes, no demands /
preferred policies / armed strength / ideology / foreign links,
no automatic generation per country.

## 2. Public API

### New core types

```cpp
namespace leviathan::core {

enum class InterestGroupKind {
    Bureaucracy,
    Military,
    Workers,
    Farmers,
    Religious,
    Media,
    Students,
    LocalElites,
    Business,
    Technocrats,
};

struct InterestGroupState {
    std::string       id_code;
    std::string       name;
    InterestGroupKind kind    = InterestGroupKind::Bureaucracy;
    CountryId         country = CountryId::invalid();
    double            influence  = 0.5;
    double            loyalty    = 0.5;
    double            radicalism = 0.0;
};

}  // namespace
```

### `GameState`

```cpp
std::vector<InterestGroupState> interest_groups;
```

Root-level, not per-country. Each entry's `country` field points
back to the country it belongs to. The root-level placement is a
deliberate choice that costs nothing in M3.1 and keeps the door
open for cross-country interaction (foreign-funded media,
transnational religious networks, diaspora pressure) without a
future schema rearrange.

### Default-constructed baseline

A default `InterestGroupState`:

- `id_code` / `name` empty,
- `kind == Bureaucracy` (the first enum value),
- `country` invalid,
- `influence == loyalty == 0.5`, `radicalism == 0.0`.

Pinned by a `tests/core/game_state_test.cpp` case.

## 3. Persistence

### Save format v10 → v11

`include/leviathan/systems/save_system.hpp` bumps
`kSaveFormatVersion` from `10` to `11`. The version-history block
gains a v11 entry documenting that a v10 save lacks the new array
entirely; silently defaulting on reload would drop whatever
interest-group set the user originally authored.

At the **save layer** the block is REQUIRED (empty array allowed)
and every entry is validated:

- `id_code` — non-empty string.
- `name` — non-empty string.
- `kind` — must be a known enum spelling
  (`Bureaucracy` / `Military` / `Workers` / `Farmers` /
   `Religious` / `Media` / `Students` / `LocalElites` /
   `Business` / `Technocrats`).
- `country` — non-negative integer that indexes into
  `state.countries`.
- `influence`, `loyalty`, `radicalism` — finite doubles in
  `[0, 1]` via `require_ratio`.
- Duplicate `id_code` within the array is rejected with the
  offending index named.

### scenario_loader (optional)

`src/leviathan/systems/scenario_loader.cpp` extends
`parse_manifest` with an optional `interest_groups` array. Missing
key ⇒ empty vector (M1.11 / M2.x manifests stay valid). When the
block is present:

- Per-entry shape errors (missing fields, wrong types,
  out-of-range ratios, empty strings) are rejected at parse time.
- Cross-references (`country` id_code → loaded country,
  `kind` string → enum) resolve inside `load_into_state` after
  countries are loaded, so unknown country / unknown kind names
  produce informative errors naming the manifest index.

### Round-trip

`save` always emits the `interest_groups` array (empty if state's
vector is empty). `load` requires the block on v11. A save that
includes a populated array round-trips byte-for-byte through the
load path, including `kind` ↔ string mapping.

## 4. Diagnostics

`diagnostics::compare_states` extends to the new array:

- Size mismatch ⇒ `interest_groups.size()` with `N != M` detail.
- Per-entry mismatches under `interest_groups[N].*` for each
  field (`id_code`, `name`, `kind`, `country`, `influence`,
  `loyalty`, `radicalism`).

Field paths mirror the save JSON shape so the same string is
usable from `--verify` CLI output, test asserts, and error
messages.

## 5. Tests

20 new doctest cases (M2.22 closed at 627 → M3.1 lands at 647).

`tests/core/game_state_test.cpp` (2):

- Baseline `GameState` has `interest_groups` empty.
- Default-constructed `InterestGroupState` has the documented
  defaults.

`tests/systems/save_system_test.cpp` (9):

- An old **v10** save is rejected loudly with `supports 11` in
  the error.
- `serialize` always emits `"interest_groups": []` for an empty
  state.
- Round-trip preserves arbitrary `InterestGroupState` field
  values (kind, country, all three ratios).
- v11 country block schema-pin test (existing) updated to assert
  `"save_version": 11`.
- v11 missing `interest_groups` → rejected.
- v11 `interest_groups` wrong type → rejected.
- v11 entry with unknown `kind` ("FloatingMasons") → rejected,
  error names the offending kind.
- v11 entry with out-of-range `country` index → rejected with
  `interest_groups[N]` + `country` in the error.
- v11 entry with out-of-range ratio → rejected.
- v11 duplicate `id_code` within the array → rejected.

`tests/systems/scenario_loader_test.cpp` (8):

- Manifest without `interest_groups` parses as empty.
- Manifest `interest_groups` not an array → rejected.
- Manifest entry missing required field → rejected.
- Manifest entry ratio out of range → rejected.
- Manifest entry duplicate id_code → rejected.
- `load_into_state` happy path with two groups: kind/country/
  influence/loyalty fields all land.
- `load_into_state` unknown country id_code → rejected with
  `interest_groups[N]` + the bad id_code in error.
- `load_into_state` unknown kind → rejected similarly.

`tests/systems/diagnostics_test.cpp` (2):

- `compare_states` size mismatch produces
  `interest_groups.size()` mismatch with `0 != 1` detail.
- Per-field mismatches at e.g. `interest_groups[0].influence`
  and `interest_groups[0].radicalism`.

`tests/systems/runner_test.cpp` schema-pin (1 updated, not new):

- `run: save schema is now v11 (M3.1 bumped from v10 for
  interest_groups)` — renamed from the M2.16 schema-pin
  test and updated to assert `"save_version": 11`.

The M1.17 / M2.22 integration tests pass unchanged because they
don't author any `interest_groups` content; the empty array
default round-trips through their existing flows.

## 6. What's NOT in scope

Deliberate non-goals:

- **No system reads `interest_groups`.** No M1 monthly pipeline,
  M2 command path, or new M3 reaction system reads the fields.
- **No monthly reaction.** Future M3 work will introduce a
  reaction step driven by stability / corruption / authority.
- **No demands.** `preferred_policies`, `policy_demands`,
  `policy_blocks` — none in M3.1.
- **No ideology / culture / religion specifics.** The enum
  carries only the coarse category.
- **No armed strength / paramilitary capability.** No
  intelligence-network bonus. No propaganda reach.
- **No resources field.** Factions (M1.2) have `resources` for a
  different purpose; interest groups don't yet.
- **No foreign links / cross-border influence.** Each entry
  belongs to exactly one country.
- **No automatic generation.** Existing scenarios load with zero
  interest groups; we do not seed default sets per country.
- **No command-resistance integration.** `commands::apply_pending`
  / `order_execution::evaluate` don't consult interest groups.
- **No `government_authority` integration.** Authority and
  interest groups are separate data layers in M3.1.
- **No event triggers.** No "the bureaucracy publicly defied your
  command" event.
- **No AI.** No interest-group AI decision system.
- **No UI / REPL / CLI changes.** No `--interest-groups-csv`
  diagnostic flag (could land in a future M3 if a real consumer
  appears).
- **No new `PlayerCommandKind` variants** targeting interest
  groups.
- **No coup / strike / protest / civil war.**
- **No M1 / M2 system change.** M1.17 + M2.22 integration tests
  pass unchanged.

## 7. Cross-links

- `milestone-2-result.md` — M2 exit report and the architectural
  invariants M3 must preserve.
- M1.2 (`m1-2-faction-state.md`) — the existing per-country
  `FactionState` carries some of the same flavour (id_code,
  type, ratios) but with a different scope (M1 internal-politics
  drift only). M3.1 introduces a parallel data layer
  deliberately; consolidation, if needed, can happen later.
- M2.16 (`m2-16-government-authority-state.md`) — the same
  "strict-at-save / permissive-at-source-fixture" split this PR
  reuses.
- M0.8 (`m0-8-save-load.md`) — strict-equality version bump
  discipline.
- RFC-020 §5 — the long-term faction / interest-group list M3.1
  ships an initial slice of.

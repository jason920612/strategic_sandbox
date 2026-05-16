# M1.2 - FactionState real fields and fixture data

Companion notes for `feature/m1-02-faction-state-fields`. Same
template as M1.1: shape change, loader, save, fixtures, tests, design
notes. **No behaviour.** Faction reaction logic, support evolution,
policy-preference evaluation — none of those run yet; that's
M1.5 / M1.6 territory.

## 1. Field set

```cpp
struct FactionState {
    // Identity
    FactionId   id;                  // numeric, caller-assigned
    CountryId   country;             // numeric, caller-resolved
    std::string id_code;             // on-disk identifier, e.g. "GER_military"
    std::string country_id_code;     // on-disk link to CountryState::id_code
    std::string name;
    std::string type;                // "military" | "bureaucracy" | "workers" |
                                     // "local_elites" | "media" | "intelligence" |
                                     // "students" | "technical_elites"

    // Behavioural ratios in [0, 1]
    double support;
    double influence;
    double radicalism;
    double loyalty;

    // Absolute, >= 0
    double resources;

    // Policy id_codes this faction is inclined to favour. M1.4 will
    // tighten the semantics; for M1.2 it is a free-form array of
    // strings that survives the JSON round trip.
    std::vector<std::string> preferred_policies;
};
```

### Why a string `type`

RFC-010 §2.5 lists 6-8 faction types. We store it as a string rather
than a C++ enum because:

- New types can be authored from data without recompiling.
- Type strings are stable on disk in saves; an enum would need a
  versioned mapping.
- Mis-typed strings will be caught the moment a system reads them
  (e.g. when "military" is hardcoded somewhere); enum-based dispatch
  fails to compile in the wrong way.

If a future PR introduces hot-path code that switches on type, we'll
reconsider — a compile-time enum + a free `type_from_string` mapper
is cheap to add.

### Why `country_id_code` as a string

Same reasoning as `CountryState::id_code`: the on-disk identifier
is the stable handle. Numeric `id` / `country` are runtime indices
the caller assigns. The integration test demonstrates the
caller-side resolve: it matches the loaded faction's
`country_id_code` against `state.countries[0].id_code` and writes
the numeric link.

## 2. JSON schema (DataLoader)

```jsonc
{
  "id":                "GER_military",   // required
  "country":           "GER",            // required - CountryState id_code
  "type":              "military",       // required (free-form string)
  "name":              "Reichswehr",     // required

  "support":           0.45,             // required, in [0, 1]
  "influence":         0.70,             // required, in [0, 1]
  "radicalism":        0.30,             // required, in [0, 1]
  "loyalty":           0.55,             // required, in [0, 1]
  "resources":         1.20,             // required, >= 0

  "preferred_policies": [                // required (may be []); strings only
    "increase_military_budget",
    "press_censorship"
  ]
}
```

Missing fields, wrong types, non-finite numbers, and out-of-range
values all produce `Result::failure` with the field name in the
message. The validator helpers (`require_ratio`,
`require_nonneg_number`, the new array-of-strings check) match the
M1.1 style.

`preferred_policies` is **required** and must be an array, but the
array may be empty. We deliberately reject silent defaulting: if a
faction author wants "no preferences", they write `[]`.

## 3. Save format change: v2 → v3

`kSaveFormatVersion: 2 → 3`. v2 saves now fail loudly:

```
save.json: unsupported save_version 2 (this binary supports 3)
```

### Why bump when factions were already a "reserved empty array"?

The M0.8 design note said "factions / provinces / policies / events
are written as empty arrays today and tolerated as absent or empty by
the loader. When M1 fleshes out their shapes, we can populate the
arrays without bumping save_version."

That was **optimistic**. Forward-compat against older binaries needs
a bump:

- An M1.1 binary (v2 reader) reading an M1.2 save (v3 shape, factions
  populated) would not know how to parse the factions and would
  silently drop them. That's silent data loss — worse than failing
  loudly.
- Strict-equality version gating prevents this. Old M1.1 binaries
  refuse v3 saves; new M1.2 binaries refuse v2 saves. Migration paths
  can be added later as needed.

Both `kSaveFormatVersion` and the version-history comment in
`save_system.hpp` document this lesson. **Going forward**: every PR
that adds new content to a previously-empty or previously-absent
container in saves bumps `kSaveFormatVersion`. The M0.8 note is
amended in spirit even though its text remains as historical record.

### SaveSystem range validation

Same policy as M1.1: SaveSystem **does not** enforce `[0, 1]` ratio
ranges or `>= 0` on load. Type-check only. Range enforcement is
DataLoader's job. The new round-trip and version-rejection tests
pin the bytes; a future Diagnostics rule could check ratio ranges if
that ever becomes useful.

## 4. Fixtures

Three Germany factions demonstrating type variety:

| File | type | flavour |
|---|---|---|
| `ger_military.json`    | `military`    | Reichswehr; high influence, low loyalty |
| `ger_bureaucracy.json` | `bureaucracy` | Reichsbeamtenschaft; stable, low radicalism |
| `ger_workers.json`     | `workers`     | Free Trade Unions; mid support, mid radicalism |

Values are 1930-flavoured guesses; balance tuning is M1.5+ when
something actually consumes them. Adding fixtures for France and
Japan is straightforward — left for whichever sub-milestone wires up
a real consumer.

## 5. Integration test (M0.11)

The end-to-end integration test grows a faction-loading step (Step
3a): after building the country list, load the three Germany factions,
assign `FactionId{0..2}`, resolve each `country` to `state.countries[0].id`
(GER), push into `state.factions`.

The round-trip assertions at Step 9 now also verify:

- `reloaded.factions.size() == 3`
- Each faction's numeric `id`, `country`, `id_code`, `type`, and
  `preferred_policies` array survive the save / load.
- `id_code`s land in the expected order.

This proves M1.2 composes cleanly with every M0 system —
DataLoader / make_game_state / TimeSystem / LoggingSystem /
Diagnostics / SaveSystem.

## 6. What M1.2 deliberately does NOT do

- **No faction reaction logic.** Nothing reads `radicalism` to drive
  protests, nothing reads `influence` to weight decisions, nothing
  computes faction support changes from policy enactment.
- **No preferred_policies evaluation.** The list is loaded and
  round-tripped, but no code consults it. M1.4 PolicyData and M1.5
  PolicySystem are the earliest milestones where preference
  evaluation makes sense.
- **No `Country ↔ Faction` index** in `GameState`. If a system needs
  to find "all factions in GER" it has to scan `state.factions`. An
  index could be added in M1.x when a hot path demands it; not yet.
- **No `Diagnostics::sanity_check` rules** for factions. Could add
  `"faction country links to non-existent country"` later — out of
  scope for M1.2.
- **No runner support for `--factions DIR`.** Same reasoning as
  countries: runner doesn't load entities; manual composition lives
  in the integration test.

## 7. Test counts

189 → **205** total green (+16 from M1.2):

| File | Δ | Notes |
|---|---|---|
| `data_loader_test.cpp` | +11 | parse_faction happy + 9 errors + canonical file load + missing-file path |
| `save_system_test.cpp` | +4  | v2 rejection regression + faction-in-output smoke + faction wrong-type test + faction missing-field test + faction id-out-of-range test + factions file round-trip |
| `m0_end_to_end_test.cpp` | 0 (same test case, extended with faction assertions) | |

Plus the M1.1 `kSaveFormatVersion: 2` → `3` sweep through every
inline save-fixture in `save_system_test.cpp` (15 string literals).

## 8. Risks / things to watch

- **Save format v3 is now load-bearing.** Same rule as v2 from M1.1:
  any future change to the country / faction shape must either
  extend forward-compatibly (new optional fields) or bump to v4 and
  accept v3 saves cannot resume. The byte-identical determinism
  tests catch accidental shape changes.
- **`FactionState::country` and `country_id_code` can drift.** The
  caller is responsible for keeping them in sync — DataLoader leaves
  `country` invalid, the integration test resolves it from
  `country_id_code`. If a future PR loads factions through a
  different path, it must do the same resolution. A
  `Diagnostics::sanity_check` rule could enforce this when needed.
- **The `type` field is a free-form string.** Authors can typo
  `"miltary"` and the loader won't flag it. Future systems that
  branch on type will fail at runtime, not at load time. Adding a
  `type_from_string` validator is a one-PR change once we have a
  consumer.

## 9. Next sub-milestone

**M1.3 — budget / economy baseline.** Per the user's M1 plan:
country-level budget fields + minimum economy snapshot, no economy
tick. Same template as M1.1 / M1.2: shape + loader + save (likely
v3 → v4) + tests + docs. No GDP-growth formula yet.

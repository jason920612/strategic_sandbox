# M4.1 - SVG map data skeleton

Companion notes for `feature/m4-01-svg-map-data-skeleton`.

**This is RFC-090 M4** (SVG map + UI), not a post-M3 governance
follow-up. M4.1 ships only the **data skeleton** required by a
future SVG exporter / HTML viewer / clickable map — no
renderer, no viewer, no UI, no clickable surface, no map
colours, no ownership dynamics. The next M4 sub-milestones
choose how the data is rendered; M4.1 chooses what the data
is shaped like.

## 1. Scope

What ships:

```text
core::ProvinceNode                       (id_code / name / owner / x / y)
GameState::provinces                     (now typed std::vector<ProvinceNode>)
SaveSystem v11 -> v12                    (provinces required at save layer)
scenario_loader optional provinces[]     (paths to per-file province manifests)
data/provinces/1930_core_nodes.json      (3 nodes: berlin / paris / tokyo)
canonical scenarios get a provinces[]    (1930_minimal + 1930_with_start_policies)
diagnostics::compare_states walks provinces
19 new doctest cases (8 save_system + 8 scenario_loader + 3 diagnostics)
```

The M0 `ProvinceState{id, owner}` stub never had a single
reader and is now gone — `ProvinceNode` replaces it with the
typed shape every future map system actually needs. `ProvinceId`
stays defined in `core/ids.hpp` (the strong-id distinction is
still useful) but `ProvinceNode` does not carry one; the
string `id_code` is the stable identity, mirroring
`CountryState::id_code` and `InterestGroupState::id_code`.

## 2. Data model

```cpp
struct ProvinceNode {
    std::string id_code;
    std::string name;
    CountryId   owner = CountryId::invalid();
    double      x = 0.0;   // normalised [0, 1]
    double      y = 0.0;   // normalised [0, 1]
};
```

Four reasons the type is this small in M4.1:

- A future SVG exporter only needs a stable identifier, a
  human label, the owning country, and a position. Anything
  more is gameplay scope that hasn't been decided yet.
- Normalised `[0, 1]` coordinates defer the projection,
  pixel-size, and SVG-path-shape decisions to the renderer
  sub-milestone. The data layer stays renderer-agnostic.
- `owner` points back to `CountryState` via the strong
  `CountryId` handle (no string lookup in the hot path).
- Deliberately deferred fields — *all listed for the reader
  so it's obvious what M4.1 chose not to ship*: population,
  terrain, resources, neighbour adjacency, ports, fronts,
  raw SVG paths, colours, controller-vs-owner split, claims,
  victory points.

## 3. Save schema bump v11 -> v12

The M0 `provinces` block was always present in the save (the
stub serialised as an empty array). A v11 save's `provinces`
entries (if any) lacked `id_code` / `name` / `x` / `y`;
silently defaulting on reload would either drop user-authored
nodes or fabricate blank ones with `(0, 0)` coordinates. We
bump strictly under the M0.8 rule.

At the save-file level the `provinces` array is **REQUIRED**
(empty array allowed) and every entry is validated:

```text
id_code     non-empty string
name        non-empty string
owner       non-negative integer indexing into state.countries
x           finite double in [0, 1]
y           finite double in [0, 1]
duplicate id_code (across the whole array) rejected
```

No unowned nodes in v12 — the spec is explicit: `owner` must
resolve. If a future sub-milestone needs unowned territory
(neutral provinces, contested zones), it should be a separate
schema bump with the contract chosen at that point.

## 4. Scenario loader

The manifest gains an **optional** root-level `provinces`
array of file paths, mirroring how `countries` / `factions` /
`policies` are spelled:

```json
"provinces": [
  "provinces/1930_core_nodes.json"
]
```

Each referenced file is a JSON object with a `provinces`
array of inline records:

```json
{
  "provinces": [
    {
      "id":    "berlin",
      "name":  "Berlin",
      "owner": "GER",
      "x":     0.52,
      "y":     0.44
    }
  ]
}
```

Cross-file uniqueness of `id_code` is enforced at the
scenario-loader level (the save layer enforces it on
reload too). Owner is a country `id_code` string, resolved
against the previously-loaded countries map and rejected if
no match.

Manifests authored before M4.1 keep working: missing
`provinces` parses as an empty vector.

`ScenarioLoadOutcome` gains `provinces_loaded` so tests and
the runner can pin how many entries actually landed.

## 5. Canonical fixture

`data/provinces/1930_core_nodes.json` ships three nodes:

| id_code | name   | owner | x     | y     |
| ------- | ------ | ----- | ----- | ----- |
| berlin  | Berlin | GER   | 0.52  | 0.44  |
| paris   | Paris  | FRA   | 0.47  | 0.48  |
| tokyo   | Tokyo  | JPN   | 0.83  | 0.55  |

Three is the minimum that exercises both directions of the
shape: one node per canonical country, two of them close
together in normalised map space and one far apart. The
filename is map-node-centric (`1930_core_nodes`), not
geographic — Tokyo is included even though Berlin / Paris
suggest Europe.

Both canonical manifests (`1930_minimal.json` and
`1930_with_start_policies.json`) reference the file. The
`scenario_loader_test` canonical-load case pins the
three-row shape so a future manifest edit can't silently
drift the fixtures.

## 6. Diagnostics

`diagnostics::compare_states` now walks `state.provinces`
after `state.interest_groups` with the same shape used for
the M3.1 walk:

```text
provinces.size()
provinces[N].id_code
provinces[N].name
provinces[N].owner
provinces[N].x
provinces[N].y
```

Tolerance for `x` / `y` reuses the existing
`CompareOptions::double_tolerance` (default `1e-9`).

## 7. Tests added

- **Save (8)**: serialize emits `provinces` array, round-trip
  with a populated entry, v12 missing-`provinces` rejected,
  wrong-type rejected, owner-index-out-of-range rejected,
  `x` out-of-range rejected, duplicate id_code rejected, plus
  the new v11 rejection mirror of the existing v10 rejection.
- **Scenario loader (8)**: manifest provinces absent / not
  array / non-string entry; happy path; unknown owner; x/y
  out of range; duplicate id_code across files; province
  file missing `provinces` array.
- **Diagnostics (3)**: identical provinces produce no
  mismatch; size mismatch reported; per-field differences
  reported on the expected paths.

The canonical scenario_loader test gains six additional
asserts pinning the three-node fixture (`provinces_loaded`
counter + per-node id_code / owner / coordinates).

## 8. What M4.1 explicitly does NOT do

For symmetry with the per-sub-milestone notes:

```text
no SVG exporter
no HTML viewer
no clickable UI
no province rendering
no map colours
no ownership dynamics (owner mutation, controller, claims)
no neighbour adjacency
no terrain / resources / population
no war / fronts / movement
no events
no AI
no command integration
no new PlayerCommandKind
no runner CLI flag
no new artefact (still 8)
no CSV for provinces
no changes to M3 formulas
no changes to M2 command gates
no diplomacy
no M5 event-engine work
```

M4 remains in progress. M4.1 ships the data layer; the next
sub-milestone is unspec'd and waits for the reviewer.

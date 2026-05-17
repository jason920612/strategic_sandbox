# M4.3 - SVG owner-color skeleton

Companion notes for `feature/rfc090-m4-03-svg-owner-color-skeleton`.

This is RFC-090 §M4 (SVG map + UI), continuing where M4.2
left off. M4.2 shipped the first deterministic SVG renderer
for the M4.1 `ProvinceNode` data layer with a single
hardcoded `fill="black"` on every `<circle>`. M4.3 keeps
everything else byte-stable and **just swaps the fill** for a
deterministic per-owner palette lookup. Future M4 sub-
milestones (HTML viewer, clickable map, labels / legend,
neighbour adjacency, terrain) extend from here.

The branch keeps the explicit `rfc090-` prefix to
disambiguate from the 2026-05-17 rolled-back invented-M4.X
governance work (lesson recorded in
`docs/milestone-3-result.md` §7).

## 1. Scope

What ships:

```text
include/leviathan/systems/svg_export.hpp     palette constants
                                             + color_for_owner()
src/leviathan/systems/svg_export.cpp         palette lookup wired
                                             into render_provinces
tests/systems/svg_export_test.cpp            6 new doctest cases
tests/systems/runner_test.cpp                1 new doctest case
docs/m4-3-svg-owner-color-skeleton.md        this file
README.md / docs/README.md / rfc/README.md   M4.3 ledger entries
                                             777 -> 784 doctest cases
```

Net delta: one attribute string per `<circle>` changes
(`fill="black"` → `fill="<palette-lookup>"`). Everything else
about the SVG document — viewBox, circle radius, identity
attributes, insertion order, coord precision, attribute
escaping, header-only-on-empty rule — is byte-identical with
M4.2.

## 2. Palette

10-entry fixed table of hex-RGB colours, declared inline in
`svg_export.hpp` as `inline constexpr std::array<...>
kOwnerPalette`:

| Index | Hex       | Name           | Canonical owner |
| ----- | --------- | -------------- | --------------- |
| 0     | `#4682b4` | steel blue     | GER             |
| 1     | `#cd5c5c` | indian red     | FRA             |
| 2     | `#daa520` | goldenrod      | JPN             |
| 3     | `#2e8b57` | sea green      | —               |
| 4     | `#9370db` | medium purple  | —               |
| 5     | `#ff8c00` | dark orange    | —               |
| 6     | `#008080` | teal           | —               |
| 7     | `#ff1493` | deep pink      | —               |
| 8     | `#6b8e23` | olive drab     | —               |
| 9     | `#6a5acd` | slate blue     | —               |

Why these:

- **Visually distinct under default SVG rendering.** No two
  adjacent indices alias to a similar hue.
- **CSS-named-colour origin.** The hex values match named CSS
  colours (steelblue / indianred / etc.) so a future HTML
  viewer that wants to swap to named-colour rendering can do
  so without renaming the palette concept.
- **10 entries** is enough for canonical (GER / FRA / JPN)
  and for a future small-scenario set (an Axis bloc, Allied
  bloc, Asia bloc, etc.) without forcing the modulo wrap to
  bite immediately.

The mapping is:

```cpp
std::string_view color_for_owner(core::CountryId owner) {
    const auto v = owner.value();
    if (v < 0)               return kOwnerFallbackFill;          // "#888888"
    return kOwnerPalette[static_cast<std::size_t>(v) %
                         kOwnerPaletteSize];
}
```

Two design choices in `color_for_owner`:

- **Modulo wrap, not clamp.** A 12-country scenario gets
  owners 10 and 11 wrapping back to palette[0] and palette[1].
  This is documented as load-bearing in the header: future
  sub-milestones that grow the table can do so by **appending
  only** so existing owner indices keep their colour.
- **Negative-owner fallback `#888888` (mid-grey).**
  Defensive: `ProvinceNode::owner` defaults to
  `CountryId::invalid()` (-1), and although the save / scenario
  loader rejects negative owners, hand-built states in unit
  tests can still construct one. The fallback keeps
  `render_provinces` total — it can never emit
  `fill=""` or run off the end of the palette array.

## 3. Public API change

`svg_export.hpp` exposes three new public symbols:

```cpp
inline constexpr std::array<std::string_view, 10> kOwnerPalette = { ... };
inline constexpr std::size_t kOwnerPaletteSize = kOwnerPalette.size();
inline constexpr std::string_view kOwnerFallbackFill = "#888888";

std::string_view color_for_owner(core::CountryId owner);
```

Exposing the palette publicly lets callers / tests compute
the expected colour for a given owner via
`kOwnerPalette[i]` without re-deriving the modulo + table
lookup at each site. The header documents that appending new
entries is safe; reordering or mutating existing ones requires
coordinated test / golden-file updates.

`render_provinces(state)` and `write_provinces(state, path)`
signatures are unchanged. Callers (just `runner::end_tick`
today) need zero source change.

## 4. Runner integration

None new. M4.2's wiring (unconditional write of
`provinces.svg` as the 9th artefact, optional
`RunnerOptions::provinces_svg_path` override defaulting to
`<output_dir>/provinces.svg`, resolved path on
`RunOutcome::provinces_svg_path`) carries forward unchanged.

The M2.9 pre-`end_tick` no-artefact contract and the M3.6
mid-`end_tick` non-transactional caveat both continue to hold
identically.

## 5. Artefact set stays 9

M4.3 does **not** add a new artefact. The artefact set
remains:

```text
save.json                                  (M0.8,  required)
events.jsonl                               (M0.6,  required)
summary.csv                                (M0.10, opt-in)
countries.csv                              (M1.14, opt-in)
factions.csv                               (M1.16, opt-in)
interest_groups.csv                        (M3.5,  unconditional)
interest_group_country_feedback.csv        (M3.6,  unconditional)
interest_group_authority_pressure.csv      (M3.6,  unconditional)
provinces.svg                              (M4.2,  unconditional)
```

The byte content of `provinces.svg` changes (M4.2 black →
M4.3 owner palette), so M1.17 / M2.22 / M3.7 byte-identical
determinism tests continue to pass by construction (same
state → same colours → same bytes).

## 6. Save format stays v12

M4.3 reads `ProvinceNode::owner` but does not store, derive,
or persist any new field. Save format remains v12.

## 7. Tests added

`tests/systems/svg_export_test.cpp` (6 new cases):

1. `color_for_owner: invalid owner returns the fallback fill`
   — invalid `CountryId` maps to `#888888`.
2. `color_for_owner: indexes the palette directly for small owners`
   — owners `0 .. kOwnerPaletteSize - 1` map to the same
   palette index.
3. `color_for_owner: wraps via modulo when owner.value() >= palette size`
   — owner `kOwnerPaletteSize` aliases to owner 0; owner
   `2 * size + 3` aliases to owner 3.
4. `render_provinces: per-owner fill colour appears on the circle`
   — three nodes with three different owners produce three
   distinct fills; M4.2's `fill="black"` no longer appears.
5. `render_provinces: same owner across multiple nodes gets the same colour`
   — three nodes all owned by 0 produce three identical fills.
6. `render_provinces: invalid owner falls back to the defensive fill`
   — an invalid owner renders with `#888888`; `data-owner="-1"`
   is still emitted.

`tests/systems/runner_test.cpp` (1 new case):

7. `run: canonical scenario uses M4.3 per-owner palette in provinces.svg`
   — pins all three canonical owners (GER / FRA / JPN) against
   `kOwnerPalette[0..2]` via the public constant.

Total: 7 new doctest cases (777 → 784).

## 8. What M4.3 explicitly does NOT do

```text
no HTML viewer
no clickable UI
no event handlers / hover state
no tooltips
no labels / text elements
no legend / colour key inside the SVG
no per-province colour override (palette is owner-keyed only)
no ownership-dynamics layer (owner is read, never written)
no neighbour / adjacency edges
no terrain / resources / population overlays
no events / AI / command integration
no new PlayerCommandKind
no runner CLI flag
no new artefact (still 9)
no save schema bump (still v12)
no new state field
no new gameplay
no atomic end_tick writes
```

The next M4 sub-milestone is unspec'd and waits for the
reviewer.

## 9. Cross-references

- [`m4-2-svg-exporter-skeleton.md`](m4-2-svg-exporter-skeleton.md)
  — the M4.2 renderer M4.3 modifies in one attribute only.
- [`m4-1-svg-map-data-skeleton.md`](m4-1-svg-map-data-skeleton.md)
  — the M4.1 data layer both M4.2 and M4.3 read.
- [`milestone-3-result.md`](milestone-3-result.md) §5 — the
  M3 invariants future milestones must preserve (M4.3 is
  consistent: no schema change, no new artefact, no command-
  gate change, no logs / events from interest groups).

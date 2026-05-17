# M4.8 - HTML static province data attributes skeleton

Companion notes for `feature/rfc090-m4-08-province-data-attrs-skeleton`.

This is RFC-090 §M4 (SVG map + UI), continuing where M4.7
left off. M4.7 added the static legend; M4.8 widens the
identity surface inside the SVG body itself — every
`<circle>` and every `<text>` now carries the same four
read-only `data-*` attributes so a future clickable UI / DOM
script can address either element uniformly without
DOM-walking siblings.

The branch keeps the explicit `rfc090-` prefix to
disambiguate from the 2026-05-17 rolled-back invented-M4.X
governance work (lesson recorded in
`docs/milestone-3-result.md` §7).

## 1. Scope

What ships:

```text
include/leviathan/systems/svg_export.hpp     updated header doc
src/leviathan/systems/svg_export.cpp         render_svg_root now
                                             emits the four
                                             data-* attrs on
                                             both <circle> and
                                             <text>
tests/systems/svg_export_test.cpp            7 new doctest cases
                                             + 1 existing case
                                             retuned (M4.4
                                             empty-name anchor
                                             moved off the now-
                                             altered tag layout)
tests/systems/runner_test.cpp                1 new doctest case
docs/m4-8-province-data-attributes-skeleton.md   this file
README.md / docs/README.md / rfc/README.md   M4.8 ledger entries
                                             819 → 827 doctest cases
```

**`provinces.svg` bytes DID change** in M4.8 — the new
attributes appear on every `<circle>` and every `<text>`.
This is the first M4.x sub-milestone since M4.4 that
deliberately edits the standalone SVG body. The change is
**additive only** (no removed attributes, no rendered-pixel
movement); SVG-to-PNG pipelines and vector tools see no
visual difference. The same change lands in `map.html` for
free, since both artefacts share `render_svg_root`.

## 2. The four data-* attributes

Every `<circle>` and every `<text>` now carries the same set:

| Attribute          | Source                                                                                 | Notes                                                                                                                                |
| ------------------ | -------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| `data-id`          | `ProvinceNode::id_code`                                                                | Existed on `<circle>` since M4.2; M4.8 adds it to `<text>`.                                                                          |
| `data-owner`       | `ProvinceNode::owner.value()` (raw int)                                                | Existed on `<circle>` since M4.2; M4.8 adds it to `<text>`. Negative / out-of-range values still appear verbatim (e.g. `"-1"`).        |
| `data-owner-code`  | `state.countries[owner].id_code`, or `""` when the owner index is invalid              | **New in M4.8.** Country id_code is more stable for future cross-references than the numeric index alone.                            |
| `data-name`        | `ProvinceNode::name`                                                                   | **New in M4.8.** Redundant with the `<text>` body content, but exposes it as an attribute for uniform programmatic lookup.           |

All four values use the M4.2 `xml_attr_escape` helper (`& <
> " '` → entities) since attribute values can carry any of
those characters.

## 3. Output shape

For each `ProvinceNode p` with `owner.value() = N`:

```xml
<circle cx="520.00" cy="440.00" r="8" fill="#4682b4"
        data-id="berlin"
        data-owner="0"
        data-owner-code="GER"
        data-name="Berlin"/>
<text   x="520.00" y="462.00" text-anchor="middle"
        data-id="berlin"
        data-owner="0"
        data-owner-code="GER"
        data-name="Berlin">Berlin</text>
```

(In the actual output the attributes are on a single line per
element; the multi-line layout above is for readability.)

## 4. Owner-code lookup

```cpp
std::string owner_code;
const auto owner_v = p.owner.value();
if (owner_v >= 0 &&
    static_cast<std::size_t>(owner_v) < state.countries.size()) {
    owner_code = state.countries[static_cast<std::size_t>(owner_v)].id_code;
}
```

The empty-string fallback for invalid / out-of-range owners
is defensive: the save / scenario layers reject such entries
at load time so production pipelines never hit this branch,
but a hand-built unit-test state can construct one. Mirrors
the same defensive-fallback strategy as `color_for_owner`
(which returns `kOwnerFallbackFill = "#888888"` for invalid
owners).

## 5. Why on BOTH `<circle>` AND `<text>`

The two elements are SIBLINGS in the DOM (not parent/child).
A future clickable UI that wants to handle a click on the
text label needs the same identity attributes the click on
the circle would surface. Walking the DOM to find a sibling
adds complexity; uniform attributes on both elements remove
the need.

`data-name` is technically redundant on `<text>` since the
name is the element's body content — but the attribute
makes the lookup uniform (`element.getAttribute("data-name")`
works on either element type, while reading body text needs
element-type-specific code).

## 6. Effect on existing artefacts

- **`provinces.svg`** — bytes changed (additive only; new
  attributes on existing elements; no visual difference).
  The M4.5 / M4.6 / M4.7 "no change to provinces.svg"
  guarantee no longer applies; M4.8 explicitly modifies the
  standalone SVG.
- **`map.html`** — bytes changed for the same reason (the
  inline SVG body is the same `render_svg_root` output).
- **Byte-identical determinism contracts (M1.17 / M2.22 /
  M3.7)** — continue to pass by construction. Both same-seed
  runs produce identical attribute values.
- **Artefact set still 10.** **Save format still v12.**
  Runner integration unchanged. No new public API surface.

## 7. Tests

`tests/systems/svg_export_test.cpp` (1 retuned + 7 new):

**Retuned (1)**:

- M4.4's `render_provinces: empty name still emits a
  (visually empty) <text> element` used to anchor on
  `text-anchor="middle"></text>` — but M4.8 puts more
  attributes between `text-anchor="middle"` and `>`, so the
  anchor no longer matches. The assertion was reshaped to
  pin (a) `data-name=""` is present (the M4.8 attribute
  carrying the empty name), (b) `"></text>` (empty body
  close), and (c) the empty name does NOT appear as a `<text>`
  body. Same intent, anchored on M4.8's stable surface.

**New (7)**:

1. `render_provinces: <circle> carries M4.8 data-name and
   data-owner-code attrs`
2. `render_provinces: <text> carries the same four data-*
   attrs as <circle>` — locates the `<text>` opening tag
   and asserts all four attribute substrings appear inside it.
3. `render_provinces: data-* attributes appear twice per
   node (circle + text)` — counts substring occurrences;
   each canonical attribute value appears exactly twice.
4. `render_provinces: data-name + data-owner-code are
   XML-attribute-escaped` — uses an id_code / name with all
   five XML metacharacters and pins the escaped form.
5. `render_provinces: invalid owner → empty
   data-owner-code (defensive)` — `CountryId::invalid()` →
   `data-owner-code=""`.
6. `render_provinces: out-of-range owner → empty
   data-owner-code (defensive)` — owner index 5 with 1
   country loaded → `data-owner-code=""`.
7. `render_provinces: data-owner-code matches
   state.countries[owner].id_code` — three nodes/three
   countries; `data-owner-code` uses the country's
   `id_code`, NOT its `name`.

`tests/systems/runner_test.cpp` (1 new):

8. `run: canonical scenario carries M4.8 data-* attrs on
   both provinces.svg AND map.html` — pins canonical
   berlin/paris/tokyo with their data-name and
   data-owner-code values appearing in **both** artefacts
   (the shared `render_svg_root` helper).

Total: 8 new doctest cases (819 → 827).

## 8. What M4.8 explicitly does NOT do

```text
no JavaScript
no click handlers
no event handlers / hover state / tooltips
no state mutation (data-* attrs are read-only viewer surface)
no <script>, no <link>, no inline event attributes
no inline style="..." per element
no <meta name="viewport">
no CSS animations / transitions / media queries / @import / @font-face
no new artefact (still 10)
no save schema bump (still v12)
no new state field
no new InterestGroupKind / PlayerCommandKind
no runner CLI flag
no events / AI / command integration
no ownership-dynamics layer (owner is read, never written)
no neighbour / adjacency edges
no terrain / resources / population overlays
no new gameplay
no atomic end_tick writes
no per-province colour override
no new <text> / <circle> presentation attributes (the
   change is data-* attributes only; cx / cy / r / fill /
   x / y / text-anchor are unchanged)
```

The next M4 sub-milestone is unspec'd and waits for the
reviewer.

## 9. Cross-references

- [`m4-7-html-legend-skeleton.md`](m4-7-html-legend-skeleton.md)
  — the M4.7 legend M4.8 sits beneath in the M4 progression.
- [`m4-2-svg-exporter-skeleton.md`](m4-2-svg-exporter-skeleton.md)
  — the M4.2 `data-id` / `data-owner` surface M4.8 widens
  with two more attributes and mirrors onto `<text>`.
- [`m4-1-svg-map-data-skeleton.md`](m4-1-svg-map-data-skeleton.md)
  — the `ProvinceNode` data layer M4.8 reads (`id_code`,
  `name`) plus the `state.countries` lookup it does (for
  `owner.id_code`).
- [`milestone-3-result.md`](milestone-3-result.md) §5 — the
  M3 invariants future milestones must preserve (M4.8 is
  consistent: no schema change, no new artefact, no
  command-gate change, no logs / events from interest
  groups).

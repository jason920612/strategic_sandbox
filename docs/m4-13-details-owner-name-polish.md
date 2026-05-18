# M4.13 - details panel owner-name polish

Companion notes for `feature/rfc090-m4-13-details-owner-name-polish`.

M4.13 widens the M4.8 identity surface by one attribute and
the M4.11 details-panel `fields` array by one row. Every
`<circle>` and every `<text>` now also carries
`data-owner-name`, resolved from
`state.countries[owner.value()].name`. The details panel
gains a fifth `Owner Name` row.

## 1. Scope

What ships:

```text
src/leviathan/systems/svg_export.cpp
  - render_svg_root: +1 attribute (`data-owner-name`) on
    both <circle> and <text>. Same defensive bounds
    check as data-owner-code; same XML-attribute-escape.
  - render_map_html click handler: +1 fields entry
    `{ attr: "data-owner-name", label: "Owner Name" }`
    so the details panel renders 5 dt/dd pairs.
include/leviathan/systems/svg_export.hpp
  - Intro paragraph mentions M4.13's surface widening.
  - "What ... deliberately do NOT do" extends to M4.13.
  - Output-shape section: <circle>/<text> attribute list
    grows by one; details-panel attr/label list grows by
    one row.
tests/systems/svg_export_test.cpp                  +8 doctest cases
tests/integration/m4_dom_contract_test.cpp         Test A grows
                                                   the canonical
                                                   fixture struct
                                                   by owner_name
                                                   + adds 1 count
                                                   check per
                                                   province
tests/systems/runner_test.cpp                      +3 data-owner-name
                                                   assertions on the
                                                   M4.8 canonical case
README.md / docs/README.md / rfc/README.md         M4.13 entry,
                                                   M4 still in
                                                   progress
docs/m4-13-details-owner-name-polish.md            this file
```

## 2. The DOM surface change

`render_svg_root` now emits five `data-*` attributes per
element instead of four:

```text
<circle ... data-id=... data-owner=... data-owner-code=...
        data-owner-name=... data-name=.../>
<text   ... data-id=... data-owner=... data-owner-code=...
        data-owner-name=...  data-name=...>NAME</text>
```

The new lookup reuses the M4.8 bounds check:

```cpp
std::string owner_code;
std::string owner_name;
const auto owner_v = p.owner.value();
if (owner_v >= 0 &&
    static_cast<std::size_t>(owner_v) < state.countries.size()) {
    const auto& owning =
        state.countries[static_cast<std::size_t>(owner_v)];
    owner_code = owning.id_code;
    owner_name = owning.name;
}
```

Single bounds check covers both lookups so they cannot
disagree about which entry is valid — when the owner is
out of range, both `data-owner-code` and `data-owner-name`
are emitted as the empty string. Both values go through
`xml_attr_escape` (the M4.2 helper) so a country name
containing `& < > " '` cannot break the attribute syntax.

## 3. Why a new attribute, not a DOM lookup against the legend

The alternative path — having the click handler walk the
`<ul class="legend">` for the matching `<li
data-owner="N">` and extract its text content — was
considered and rejected for three reasons:

1. **Stable future DOM surface.** Adding `data-owner-name`
   alongside `data-owner-code` keeps the M4.8 widening
   pattern uniform: future clickable-UI sub-milestones
   that grep for "what data-* attributes can I read on a
   province?" find one consistent list.
2. **No coupling between the panel and the legend.** A
   future legend rework (collapsible groups, owner
   subsection headers, etc.) can change the legend
   structure without breaking the details panel.
3. **No string parsing.** Extracting the country name
   from the legend `<li>` would mean parsing `"<id_code>
   &mdash; <name>"` — a regex or `split` on `" — "`.
   `getAttribute("data-owner-name")` returns the name
   directly.

The cost is a small (~10–20 bytes per province) growth in
both `provinces.svg` and `map.html`, which is consistent
with the M4.8 widening's cost.

## 4. Save format stays v12

`data-owner-name` is **derived** from `state.countries`
at render time. It is **not** a new field on
`ProvinceNode`:

```text
core::ProvinceNode { id_code, name, owner, x, y }   (unchanged)
core::CountryState { ..., name, ... }                (unchanged)
```

The save schema does not grow. Old v12 saves load
unchanged; new v12 saves are byte-identical with
pre-M4.13 saves of the same `GameState`. The only files
whose bytes change at M4.13 are `provinces.svg` and
`map.html` (the renderer's output).

## 5. What M4.13 does NOT do

```text
no new field on ProvinceNode (data-owner-name is derived)
no save schema bump (still v12)
no new state field
no new artefact (still 10)
no new fixture
no new InterestGroupKind / PlayerCommandKind
no rename of the M4.8 data-id / data-owner /
   data-owner-code / data-name keys (additive only)
no state mutation from the viewer
no commands / player actions / AI integration
no events emitted by the selection
no selection persistence
no multi-select / shift-click / right-click / context menu
no hover state / tooltip / mouseover
no keyboard navigation / focus ring / aria-* / a11y polish
no animation / transition
no second <script>, no <script src=>, no <script type=>
no <link>, no external CSS, no external font
no fetch / XMLHttpRequest / localStorage / sessionStorage /
   history.pushState / window.location / navigator usage
no innerHTML / outerHTML / document.write / eval / Function
no className string concatenation; no setAttribute("class", ...)
no inline event attributes
no per-element inline style="..."
no <meta name="viewport">
no runner CLI flag
no neighbour / adjacency edges
no terrain / resources / population overlays
no M4 close-out
no "M4 closed" wording
```

`provinces.svg` and `map.html` bytes both DID change (the
new attribute on every `<circle>` + `<text>`; the new
fifth `fields` entry in the click handler). The change is
additive — no removed attributes, no rendered-pixel
movement.

## 6. Test coverage

Unit (`tests/systems/svg_export_test.cpp`):

- The M4.8 "data-* attributes appear twice per node"
  canonical-count test gains a `count_occurrences(
  "data-owner-name=\"Germany\"") == 2u` line so the M4.13
  surface stays at parity with the M4.8 four-attribute
  count.
- `render_provinces: <circle> + <text> carry M4.13
  data-owner-name with the owner's name` — pins the
  attribute appears twice (circle + text).
- `render_provinces: M4.13 data-owner-name appears inside
  the <text> opening tag` — pins the attribute on `<text>`
  specifically (uniform identity).
- `render_provinces: M4.13 data-owner-name is
  XML-attribute-escaped` — country name with all five XML
  metacharacters is escaped via the M4.2 helper; raw form
  cannot appear in the attribute body.
- `render_provinces: M4.13 invalid owner → empty
  data-owner-name (defensive)` — `CountryId::invalid()`
  yields `data-owner-name=""`; same single bounds check
  covers `data-owner-code` too.
- `render_provinces: M4.13 out-of-range owner → empty
  data-owner-name (defensive)` — owner index 5 with 1
  country loaded yields the empty form.
- `render_provinces: M4.13 data-owner-name matches
  state.countries[owner].name across multiple countries`
  — three nodes / three countries, each
  `data-owner-name` resolves to the right country's name,
  not its id_code.
- `render_map_html: M4.13 details panel fields array
  carries the fifth Owner Name entry` — pins the M4.11
  fields array grows to 5 entries; the four existing
  labels remain.
- `render_map_html: M4.13 propagates data-owner-name
  through the inline SVG body` — confirms the HTML
  wrapper inherits the new attribute for free (same
  `render_svg_root` helper).

Integration (`tests/integration/m4_dom_contract_test.cpp`):

- Test A's canonical fixture struct grows by an
  `owner_name` field; the check loop adds one
  `count(owner_name_attr) >= 2u` assertion per province.
  This pins the M4.13 attribute end-to-end on the
  canonical scenario for both `provinces.svg` and
  `map.html`.

Runner (`tests/systems/runner_test.cpp`):

- The M4.8 canonical-data-* runner case picks up three
  new substring checks (`data-owner-name="Germany"`,
  `"France"`, `"Japan"`) — one per canonical country.

## 7. Cross-references

- [`m4-8-province-data-attributes-skeleton.md`](m4-8-province-data-attributes-skeleton.md)
  — the M4.8 widening pattern M4.13 mirrors. Same
  defensive bounds check, same XML-attribute escape, same
  uniform parity between `<circle>` and `<text>`.
- [`m4-11-details-labels-polish.md`](m4-11-details-labels-polish.md)
  — the M4.11 details-panel `fields` array M4.13 grows
  from 4 to 5 entries. The label-decoupling pattern (raw
  attr key for `getAttribute`, fixed human-readable
  string for `<dt>`) carries over unchanged.
- [`m4-10-clickable-ui-skeleton.md`](m4-10-clickable-ui-skeleton.md)
  — the M4.10 click handler M4.11 polished and M4.12
  extended. The XSS-safe DOM API, no-storage / no-network
  discipline, asymmetric one-inline-script invariant all
  still hold.
- [`m4-9-dom-contract-checkpoint.md`](m4-9-dom-contract-checkpoint.md)
  — the M4 DOM contract checkpoint. Test A's per-province
  attribute check loop grows by one entry; Test B and
  Test C are unchanged.
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4 status snapshot. The "data-* identity
  surface" row needs an update: five attributes since
  M4.13, not four.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  the M3 invariants every future milestone must
  preserve. M4.13 is consistent: no schema bump, no new
  artefact, no command-gate change, no events / logs
  from the viewer, no state mutation.

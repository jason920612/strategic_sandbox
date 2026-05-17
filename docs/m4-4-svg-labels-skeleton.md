# M4.4 - SVG labels skeleton

Companion notes for `feature/rfc090-m4-04-svg-labels-skeleton`.

This is RFC-090 §M4 (SVG map + UI), continuing where M4.3
left off. M4.3 swapped the renderer's hardcoded
`fill="black"` for a deterministic per-owner palette. M4.4
adds one `<text>` label per node — the smallest possible
labelling surface, deferring fonts, tooltips, legends, and
hover behaviour to later sub-milestones.

The branch keeps the explicit `rfc090-` prefix to
disambiguate from the 2026-05-17 rolled-back invented-M4.X
governance work (lesson recorded in
`docs/milestone-3-result.md` §7).

## 1. Scope

What ships:

```text
include/leviathan/systems/svg_export.hpp     +kLabelYOffset constant
                                             updated header doc
src/leviathan/systems/svg_export.cpp         +xml_text_escape helper
                                             render_provinces emits
                                             a <text> after each
                                             <circle>
tests/systems/svg_export_test.cpp            7 new doctest cases
tests/systems/runner_test.cpp                1 new doctest case
docs/m4-4-svg-labels-skeleton.md             this file
README.md / docs/README.md / rfc/README.md   M4.4 ledger entries
                                             784 → 792 doctest cases
```

Net delta: one new `<text>` element per `<circle>`. Every
other byte in the SVG document — viewBox, circle attributes,
data-id / data-owner identity, owner-driven palette,
insertion order, coord precision, LF terminators,
header-only-on-empty — is byte-identical with M4.3.

## 2. Output shape

```xml
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 1000" width="1000" height="1000">
  <circle cx="520.00" cy="440.00" r="8" fill="#4682b4" data-id="berlin" data-owner="0"/>
  <text x="520.00" y="462.00" text-anchor="middle">Berlin</text>
  <circle cx="470.00" cy="480.00" r="8" fill="#cd5c5c" data-id="paris" data-owner="1"/>
  <text x="470.00" y="502.00" text-anchor="middle">Paris</text>
  <circle cx="830.00" cy="550.00" r="8" fill="#daa520" data-id="tokyo" data-owner="2"/>
  <text x="830.00" y="572.00" text-anchor="middle">Tokyo</text>
</svg>
```

Design choices:

- **One `<text>` per `<circle>`, interleaved.** Each node's
  two elements appear back-to-back in the byte stream, in
  `state.provinces` order. Keeps the rendered output grouped
  the way a human would mentally pair "circle = node, text =
  its label", and avoids two passes over the vector.
- **Position `(cx, cy + kLabelYOffset)`.** `kLabelYOffset =
  22.0` puts the label baseline roughly 14 SVG units below
  the bottom edge of the radius-8 circle. The constant is
  exposed publicly so tests can compute the expected y
  without hardcoding.
- **`text-anchor="middle"`.** Horizontal centring on the
  node. Without it the label's left edge would sit at `cx`,
  visually offsetting from the circle.
- **No `font-family`, no `font-size`, no `fill`.** The SVG
  consumer's default applies (16px sans-serif on most
  renderers, black fill). M4.4 ships minimum labels;
  typography is a future presentation sub-milestone.
- **XML text-content escape, not attribute escape.** A new
  `xml_text_escape` helper (anonymous namespace in
  `svg_export.cpp`) escapes only `&`, `<`, `>` per the
  XML 1.0 §2.4 text-content rules. `"` and `'` are legal as
  literals inside text content and stay untouched. The M4.2
  `xml_attr_escape` helper (which also escapes `"` and `'`)
  continues to handle `data-id`. Keeping the two helpers
  separate makes the escape contract explicit at each call
  site.
- **Empty `name` still emits an empty `<text>` body.** The
  save / scenario layers reject empty names so this branch
  is unreachable in production, but a hand-built unit-test
  state can construct one. Defensive: the renderer stays
  total — exactly one `<text>` per `<circle>` regardless.

## 3. Public API change

`svg_export.hpp` adds one new public constant:

```cpp
inline constexpr double kLabelYOffset = 22.0;
```

The `render_provinces(state)` and `write_provinces(state, path)`
signatures are unchanged. Callers (just `runner::end_tick`
today) need zero source change.

## 4. Runner integration

None new. The M4.2 wiring (unconditional write of
`provinces.svg` as the 9th artefact, optional
`RunnerOptions::provinces_svg_path` override defaulting to
`<output_dir>/provinces.svg`, resolved path on
`RunOutcome::provinces_svg_path`) carries forward unchanged.

The M2.9 pre-`end_tick` no-artefact contract and the M3.6
mid-`end_tick` non-transactional caveat both continue to
hold identically.

## 5. Artefact set stays 9, save format stays v12

M4.4 does **not** add a new artefact or persist any new
field. The 9-artefact set and v12 save format are both
unchanged. M1.17 / M2.22 / M3.7 byte-identical determinism
contracts continue to pass by construction (same state →
same labels → same bytes).

## 6. Tests added

`tests/systems/svg_export_test.cpp` (7 new cases):

1. `render_provinces: empty state emits no <text> elements`
2. `render_provinces: one node emits one <text> with the node's name`
3. `render_provinces: <text> is positioned at (cx, cy + kLabelYOffset)`
4. `render_provinces: <circle> immediately precedes its matching <text>`
5. `render_provinces: <text> content escapes & < > but leaves " and ' literal`
6. `render_provinces: empty name still emits a (visually empty) <text> element`
7. `render_provinces: label rendering is deterministic across repeat calls`

`tests/systems/runner_test.cpp` (1 new case):

8. `run: canonical scenario emits M4.4 <text> labels in provinces.svg`
   — pins `Berlin` / `Paris` / `Tokyo` against the canonical
   fixture and the `text-anchor="middle"` presentation contract.

Total: 8 new doctest cases (784 → 792).

## 7. What M4.4 explicitly does NOT do

```text
no HTML viewer
no clickable UI
no event handlers / hover state
no tooltips
no legend / colour key inside the SVG
no font-family / font-size / fill on <text>
no label collision detection or repositioning
no per-province label override (label is always node.name)
no rich text / multi-line labels
no ownership-dynamics layer (provinces are static)
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

## 8. Cross-references

- [`m4-3-svg-owner-color-skeleton.md`](m4-3-svg-owner-color-skeleton.md)
  — the M4.3 palette M4.4 builds on (every `<text>` still
  paired with its M4.3-coloured `<circle>`).
- [`m4-2-svg-exporter-skeleton.md`](m4-2-svg-exporter-skeleton.md)
  — the M4.2 renderer M4.4 extends in one element only.
- [`m4-1-svg-map-data-skeleton.md`](m4-1-svg-map-data-skeleton.md)
  — the M4.1 data layer that introduced `ProvinceNode::name`
  M4.4 now reads.
- [`milestone-3-result.md`](milestone-3-result.md) §5 — the
  M3 invariants future milestones must preserve (M4.4 is
  consistent: no schema change, no new artefact, no
  command-gate change, no logs / events from interest
  groups).

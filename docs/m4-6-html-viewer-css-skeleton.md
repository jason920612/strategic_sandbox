# M4.6 - HTML viewer minimal CSS skeleton

Companion notes for `feature/rfc090-m4-06-html-viewer-css-skeleton`.

This is RFC-090 §M4 (SVG map + UI), continuing where M4.5
left off. M4.5 added the minimal `map.html` wrapper with
**no** CSS — the SVG rendered with the browser defaults,
which meant the SVG sat top-left against a white page and
the `<text>` labels used the browser's serif fallback (which
renders small labels poorly). M4.6 adds the smallest
possible inline `<style>` block — three CSS selectors that
centre the SVG card on a neutral page, give it a border so
the white-fill body pops, and switch the labels to a
sans-serif font.

The branch keeps the explicit `rfc090-` prefix to
disambiguate from the 2026-05-17 rolled-back invented-M4.X
governance work (lesson recorded in
`docs/milestone-3-result.md` §7).

## 1. Scope

What ships:

```text
include/leviathan/systems/svg_export.hpp     updated header doc
src/leviathan/systems/svg_export.cpp         render_map_html emits
                                             a <style> block
tests/systems/svg_export_test.cpp            5 new doctest cases
                                             + 1 existing case
                                             retuned (M4.5 "no
                                             <style>" assertion
                                             dropped)
tests/systems/runner_test.cpp                1 new doctest case
docs/m4-6-html-viewer-css-skeleton.md        this file
README.md / docs/README.md / rfc/README.md   M4.6 ledger entries
                                             804 → 810 doctest cases
```

`provinces.svg` byte output is **unchanged** by M4.6 — the
CSS lives only in the HTML wrapper. The standalone-SVG path
stays CSS-free so downstream consumers (e.g., an SVG-to-PNG
pipeline, a vector-editing tool) see the same bytes M4.5
produced.

## 2. The three CSS rules

```css
body {
  margin: 0;
  padding: 20px;
  background-color: #f0f0f0;
}
svg {
  display: block;
  margin: 0 auto;
  border: 1px solid #888;
  background-color: #ffffff;
}
svg text {
  font-family: sans-serif;
}
```

Why each rule:

- **`body`**
  - `margin: 0` — zero out the browser's default body margin
    so the page background fills the viewport.
  - `padding: 20px` — small uniform breathing room around the
    centred SVG card.
  - `background-color: #f0f0f0` — neutral light grey so the
    white SVG card visually pops off the page background.
- **`svg`**
  - `display: block` — SVG renders as an inline replaced
    element by default; `block` is needed for
    `margin: 0 auto` to centre it.
  - `margin: 0 auto` — horizontally centre the 1000-unit-wide
    SVG inside the body.
  - `border: 1px solid #888` — thin grey border so the SVG
    looks like a "card", not floating pixels.
  - `background-color: #ffffff` — explicit white fill so the
    border has a clear inside / outside even when the
    embedded SVG content is sparse (e.g., empty
    `state.provinces`).
- **`svg text`**
  - `font-family: sans-serif` — the M4.4 `<text>` elements
    deliberately don't carry their own font (the M4.4 / M4.5
    contract preserved typography deferral on the elements
    themselves). The browser default for SVG `<text>` is
    `serif`, which renders small labels poorly. The CSS
    fix is one selector, three words, applies only to the
    HTML viewer.

That's the entire stylesheet. Three selectors, eight
declarations.

## 3. Output shape

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Leviathan Map</title>
  <style>
  body { margin: 0; padding: 20px; background-color: #f0f0f0; }
  svg { display: block; margin: 0 auto; border: 1px solid #888; background-color: #ffffff; }
  svg text { font-family: sans-serif; }
  </style>
</head>
<body>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 1000" width="1000" height="1000">
  <circle cx="520.00" cy="440.00" r="8" fill="#4682b4" data-id="berlin" data-owner="0"/>
  <text x="520.00" y="462.00" text-anchor="middle">Berlin</text>
  ...
</svg>
</body>
</html>
```

The `<style>` block sits between `<title>` and `</head>`,
at the same 2-space indent as its sibling `<meta>` and
`<title>` elements. Each CSS rule is on its own line for
human readability; the rule body is single-line so the
overall file stays short.

## 4. Determinism / artefact contract / save format

- **`map.html` bytes changed** (by design: the `<style>`
  block is new content). Same-state byte-stability of
  `render_map_html` is preserved: two calls with the same
  `GameState` produce byte-identical output every time.
- **M1.17 / M2.22 / M3.7 byte-identical determinism
  contracts continue to pass by construction** — they
  compare two same-seed runs against each other, and both
  runs emit the same CSS bytes.
- **`provinces.svg` bytes UNCHANGED.** The CSS lives only
  in the HTML wrapper.
- **Artefact set unchanged (still 10).** Save format
  unchanged (still v12). No new state field.
- Runner integration unchanged — the M4.5 wiring carries
  forward without modification.

## 5. Tests added / changed

`tests/systems/svg_export_test.cpp`:

**Modified (1 case)**:

- The M4.5 case `render_map_html: no <style>, <script>,
  <link>, or inline event attributes` was renamed to
  `render_map_html: no <script>, <link>, or inline event
  attributes` and its `<style>` assertion dropped. M4.5's
  other constraints (no `<script>`, no `<link>`, no inline
  event handlers) still hold; the test additionally pins
  that **inline `style="..."` attributes** on individual
  elements remain absent (the `<style>` block is the single
  CSS surface).

**New (5 cases)**:

1. `render_map_html: emits a <style> block in <head>` —
   `<style>` exists and sits inside `<head>`.
2. `render_map_html: body rule centres + backgrounds the page`
   — pins `body { margin: 0; padding: 20px; background-color:
   #f0f0f0; }` (all four substrings).
3. `render_map_html: svg rule centres the SVG with a border`
   — pins `svg { display: block; margin: 0 auto; border:
   1px solid #888; background-color: #ffffff; }`.
4. `render_map_html: svg text rule uses sans-serif for label
   readability` — pins `svg text { font-family: sans-serif; }`.
5. `render_provinces (standalone SVG) does NOT include the
   M4.6 CSS` — pins the SVG-only output stays CSS-free
   (no `<style>`, no `font-family`, no `background-color`).

`tests/systems/runner_test.cpp` (1 new case):

6. `run: canonical scenario carries the M4.6 minimal CSS in
   map.html` — canonical-run `map.html` carries all three
   CSS rules; canonical-run `provinces.svg` does not.

Total: 6 new doctest cases (804 → 810).

## 6. What M4.6 explicitly does NOT do

```text
no JavaScript / <script>
no <link> to an external stylesheet
no inline event attributes (onclick / onmouseover / onload / ...)
no inline style="..." attributes on individual elements
no <meta name="viewport"> (responsive sizing deferred)
no per-province / per-circle / per-text fill override in CSS
no CSS animations / transitions
no media queries
no @import / @font-face
no click handlers / clickable UI / hover state
no tooltips
no state mutation from the viewer
no legend / colour key
no font-family / font-size on the <text> elements themselves
   (M4.4 / M4.5 contract carried forward; only the
   CSS selector `svg text` sets the font)
no ownership-dynamics layer
no neighbour / adjacency edges
no terrain / resources / population overlays
no events / AI / command integration
no new PlayerCommandKind
no runner CLI flag
no new artefact (still 10)
no save schema bump (still v12)
no new state field
no new gameplay
no atomic end_tick writes
no change to provinces.svg bytes
```

The next M4 sub-milestone is unspec'd and waits for the
reviewer.

## 7. Cross-references

- [`m4-5-html-viewer-skeleton.md`](m4-5-html-viewer-skeleton.md)
  — the M4.5 HTML wrapper M4.6 adds CSS to. M4.5's "no
  `<style>`" constraint is the one M4.6 explicitly relaxes
  (in a tightly-scoped way); all other M4.5 nots still hold.
- [`m4-4-svg-labels-skeleton.md`](m4-4-svg-labels-skeleton.md)
  — the M4.4 `<text>` elements whose readability the M4.6
  `svg text { font-family: sans-serif }` rule improves.
- [`m4-2-svg-exporter-skeleton.md`](m4-2-svg-exporter-skeleton.md)
  — the M4.2 SVG renderer the M4.6 wrapper inlines verbatim
  (no byte change).
- [`milestone-3-result.md`](milestone-3-result.md) §5 — the
  M3 invariants future milestones must preserve (M4.6 is
  consistent: no schema change, no new artefact, no
  command-gate change, no logs / events from interest
  groups; the bytes of `map.html` are documented load-bearing
  and same-state-deterministic).

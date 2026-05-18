# M4.16 - focus-visible styling skeleton

Companion notes for `feature/rfc090-m4-16-focus-visible-skeleton`.

M4.16 makes M4.15's keyboard focus VISIBLE. Pure CSS — no
JavaScript change, no markup change beyond the `<style>`
block, no schema or artifact shift.

## 1. Scope

What ships:

```text
src/leviathan/systems/svg_export.cpp
  - render_map_html <style>: +4 CSS rules
      svg circle:focus         { outline: none; }
      svg circle:focus-visible { outline: none;
                                  stroke: #1976d2;
                                  stroke-width: 4; }
      svg text:focus           { outline: none; }
      svg text:focus-visible   { outline: 2px solid #1976d2;
                                  outline-offset: 2px; }
include/leviathan/systems/svg_export.hpp
  - Intro paragraph mentions M4.16 focus-visible CSS.
  - <style> rules list grows by four entries.
  - "What ... deliberately do NOT do" heading extends to M4.16.
tests/systems/svg_export_test.cpp                  +5 doctest cases
README.md / docs/README.md / rfc/README.md         M4.16 entry,
                                                   M4 still in
                                                   progress
docs/m4-16-focus-visible-skeleton.md               this file
```

## 2. Why `:focus-visible`, not bare `:focus`

The CSS distinction matters:

- `:focus` matches **any** focused element — including ones
  focused by mouse click. Using `:focus` for the ring would
  make a click on a province paint **both** the M4.12
  `.selected` stroke (black, set by `selectProvince`) **and**
  the M4.16 focus ring (blue). The two would overlap.
- `:focus-visible` matches only focus the browser considers
  "should be visible" — keyboard tab, programmatic
  `.focus()` after a keyboard interaction, etc. Mouse-click
  focus does NOT match `:focus-visible` in modern browsers,
  so the two states stay separated:
  - mouse click → `.selected` stroke (black, M4.12)
  - keyboard Tab → focus ring (blue, M4.16)
  - keyboard Enter / Space on focused marker →
    `.selected` stroke (black, M4.12) STAYS focused → ring
    appears on top of the .selected stroke

The third case (keyboard activate) is the only state where
both stripes show, which is correct: a keyboard user can
see both "this is selected" and "this still has focus".

## 3. Why neutralise bare `:focus` too

`svg circle:focus { outline: none; }` and
`svg text:focus { outline: none; }` are NOT no-op rules.
They suppress the browser's default focus outline (Chrome
shows a black 2px outline by default on focused SVG
elements). Without these suppressors, mouse-clicked
markers in Chrome would still show the default outline,
again colliding visually with M4.12 `.selected`.

After M4.16:

- mouse click → no browser outline (suppressed); only the
  M4.12 black `.selected` stroke
- keyboard Tab → no browser outline (suppressed); only the
  M4.16 blue `:focus-visible` ring

## 4. Why these colours / shapes

- `#1976d2` (Material Blue 700) was chosen to contrast
  with:
  - the M4.12 `.selected` stroke (`#000000`)
  - the 10-entry M4.3 `kOwnerPalette` (steel blue
    `#4682b4` at slot 0 is the closest; M4.16's brighter,
    higher-saturation blue is visibly different)
- Circle uses `stroke + stroke-width` (4px). Circles can
  show a CSS `outline` in modern browsers, but stroke is
  more reliable across SVG renderers and follows the
  shape outline exactly.
- Text uses `outline` (2px solid + 2px offset). SVG
  `<text>` elements accept CSS `outline` as a
  rectangular ring around the text bounding box in
  modern browsers; that reads as a "this text is
  focused" cue more naturally than a text shadow or a
  background.

## 5. What M4.16 does NOT do

```text
no ARIA polish (no role= / aria-label= / aria-selected= /
   aria-current= / aria-pressed= / aria-describedby= /
   aria-live= / aria-labelledby=)
no tooltip / hover / mouseover
no animation / transition on the ring
no focus-visible polyfill for legacy browsers
   (modern browsers only; old browsers will fall back
    to no ring, no regression)
no persistent focus state across reloads
no focus management between renders
no state mutation, no commands, no AI integration
no events emitted by focus
no selection persistence
no multi-select / shift-Tab special behaviour
no keyboard shortcut for the panel
no skip-link / landmark navigation
no second <script>, no <script src=>, no <script type=>
no <link>, no external CSS, no external font
no fetch / XHR / storage / history / navigation APIs
no innerHTML / outerHTML / document.write / eval / Function
no inline event attributes
no per-element inline style="..." (the focus CSS lives
   in the <style> block; no element carries style="...")
no <meta name="viewport">
no save schema bump (still v12)
no new state field
no new artefact (still 10)
no new fixture
no new InterestGroupKind / PlayerCommandKind
no runner CLI flag
no neighbour / adjacency edges
no terrain / resources / population overlays
no change to provinces.svg bytes (focus CSS is HTML-only)
no M4 close-out
no "M4 closed" wording
```

`provinces.svg` bytes UNCHANGED from M4.15 — the focus CSS
lives entirely in the HTML wrapper's `<style>` block.
`map.html` bytes did change (four new CSS rules).

## 6. Test coverage

Unit (`tests/systems/svg_export_test.cpp`):

- `render_map_html: M4.16 <style> block carries the four
  focus-related rules` — pins all four CSS literals
  verbatim.
- `render_map_html: M4.16 uses :focus-visible (NOT bare
  :focus) for the rings` — pins the behavioural
  distinction: `:focus-visible` MUST appear; bare `:focus`
  may only appear as the `outline: none;` suppressors,
  never with the ring colour/weight literals.
- `render_map_html: M4.16 focus ring colour does NOT
  collide with M4.12 .selected stroke` — confirms the
  M4.12 `.selected` literal (`#000000`) is still present
  alongside the M4.16 `#1976d2`.
- `render_map_html: M4.16 carries NO ARIA polish` —
  explicit absence of `role=`, `aria-label=`,
  `aria-selected=`, `aria-current=`, `aria-pressed=`.
- `render_provinces (standalone SVG) does NOT include the
  M4.16 focus-visible rules` — the standalone SVG path
  stays free of `:focus`, `:focus-visible`, `#1976d2`,
  `outline-offset`.

The M4.9/M4.14 integration tests A/B/C/D/E all stay green
unchanged: A/B/D/E unchanged; C's asymmetric one-script
invariant still holds (still exactly one `<script>`; no
new ARIA / inline event attrs / per-element styles).
M4.10–M4.13/M4.15 unit tests stay green — the new CSS is
additive.

## 7. Cross-references

- [`m4-15-keyboard-focus-skeleton.md`](m4-15-keyboard-focus-skeleton.md)
  — the M4.15 keyboard-focus surface M4.16 styles.
  Without M4.15's `tabindex="0"` + keydown listener,
  M4.16's `:focus-visible` rules would never match.
- [`m4-12-selected-state-css-skeleton.md`](m4-12-selected-state-css-skeleton.md)
  — the M4.12 `.selected` CSS rules M4.16 deliberately
  visually separates from (black .selected stroke vs
  blue focus ring).
- [`m4-6-html-viewer-css-skeleton.md`](m4-6-html-viewer-css-skeleton.md)
  — the M4.6 single-CSS-surface rule (no per-element
  inline `style="..."`). M4.16 obeys it: the new
  `:focus-visible` rules live in the `<style>` block.
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4 status snapshot (refreshed at M4.14). The
  `<style>` block grew from 13 selectors at M4.14 to 17
  at M4.16. The snapshot will need a future refresh.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  M3 invariants. M4.16 preserves all of them.

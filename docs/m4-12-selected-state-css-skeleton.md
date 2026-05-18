# M4.12 - clickable UI selected-state CSS skeleton

Companion notes for `feature/rfc090-m4-12-selected-state-css-skeleton`.

M4.12 layers a transient selection highlight on top of the
M4.10 click handler / M4.11 details labels. Clicking either
the `<circle>` or the `<text>` of a province lights up the
whole province pair (both elements sharing that `data-id`).
Selection is **purely DOM-level** — never written into
`GameState`, never persisted across reloads, lost on
refresh by design.

## 1. Scope

What ships:

```text
src/leviathan/systems/svg_export.cpp
  - render_map_html <style>: +2 CSS rules
      svg circle.selected { stroke: #000000; stroke-width: 3; }
      svg text.selected   { font-weight: bold; }
  - render_map_html <script>: new `selectProvince(el)`
    helper called from the click listener in addition to
    `showDetails(el)`. Uses classList.remove("selected")
    on every prior .selected, then classList.add("selected")
    on every node sharing the clicked element's data-id.
  - The `nodes` querySelectorAll moves above `showDetails`
    so `selectProvince` can re-use the same NodeList.
include/leviathan/systems/svg_export.hpp
  - Intro paragraph mentions M4.12.
  - "What ... deliberately do NOT do" heading extends to
    M4.10 / M4.11 / M4.12.
  - Output-shape section: <style> rules block grows by two;
    body step 4 (script) gains the selectProvince paragraph.
tests/systems/svg_export_test.cpp                  +5 doctest cases
README.md / docs/README.md / rfc/README.md         M4.12 entry,
                                                   M4 still in
                                                   progress
docs/m4-12-selected-state-css-skeleton.md          this file
```

## 2. The selection logic

```js
var nodes = document.querySelectorAll(
  "svg circle[data-id], svg text[data-id]");

function selectProvince(el) {
  var id = el.getAttribute("data-id");
  if (!id) { return; }
  var prev = document.querySelectorAll(".selected");
  for (var p = 0; p < prev.length; p++) {
    prev[p].classList.remove("selected");
  }
  for (var q = 0; q < nodes.length; q++) {
    if (nodes[q].getAttribute("data-id") === id) {
      nodes[q].classList.add("selected");
    }
  }
}
```

Three properties worth pinning:

1. **Whole-pair selection.** Clicking either the circle or
   the text lights up BOTH elements sharing that
   `data-id`. The M4.8 widened identity surface makes this
   a one-line filter — no DOM tree-walking siblings, no
   ordering assumption. This is the design intent the
   M4.8 design note flagged: "a future clickable UI / DOM
   script can address either element without DOM-walking
   siblings".

2. **classList API only.** No `className =
   prev + " selected"` string concatenation (which could
   in principle re-introduce escape issues if combined
   with attribute reads) and no
   `setAttribute("class", ...)`. `classList.add` /
   `.remove` are the dedicated class-manipulation API and
   cannot inject markup.

3. **Manual `nodes` walk over attribute-selector
   short-circuit.** The natural alternative
   `document.querySelectorAll('[data-id="' + id + '"]')`
   would require escaping the id for use inside a CSS
   selector — a hazard worth avoiding. The manual `for`
   loop over the pre-collected `nodes` NodeList compares
   strings, so the `data-id` value never re-enters a
   CSS-selector parser at runtime.

## 3. The CSS additions

```css
svg circle.selected { stroke: #000000; stroke-width: 3; }
svg text.selected   { font-weight: bold; }
```

Both rules live in the M4.6 `<style>` block alongside the
M4.7 legend and M4.10 details rules. **No per-element
inline `style="..."`** — the M4.6 single-CSS-surface
contract holds.

Why these specific values:

- **Black stroke, width 3** on the circle is visible
  against every M4.3 palette colour (steel blue, indian
  red, goldenrod, etc.) without changing the fill
  (preserves the M4.3 owner-colour mapping at a glance).
- **Bold font-weight** on the text matches a familiar
  "this row is selected" idiom from list and table UIs.

No `:hover`, no `:focus`, no transitions, no
`@keyframes` — the M4.6 plain-layout stylesheet rule is
unchanged.

## 4. Selection is DOM-only

The most important non-feature of M4.12 is the absence of
any persistence. Selection state lives entirely on the
DOM elements as a `class` attribute set at click time:

```text
no save schema field for "selected_province"
no GameState field
no localStorage write
no sessionStorage write
no document.cookie write
no history.pushState / history.replaceState
no window.location / URL fragment update
no fetch / XMLHttpRequest call
no events emitted / state.logs entry
```

A page reload always starts with nothing selected. This
keeps the runner artefact contract intact (still 10
files; `map.html` bytes did change but only the inline
script + style block).

## 5. What M4.12 does NOT do

```text
no state mutation (still read-only from GameState's POV)
no commands / player actions / AI integration
no events emitted by the selection
no selection persistence (next reload clears it; no
   localStorage / sessionStorage / cookie / URL fragment)
no multi-select / shift-click / right-click / context menu
no hover state / tooltip / mouseover
no keyboard navigation / focus ring / aria-* / a11y polish
   (separate future sub-milestone)
no animation / transition on the highlight
no second <script>, no <script src=>, no <script type=>
no <link>, no external CSS, no external font
no fetch / XMLHttpRequest / localStorage / sessionStorage /
   history.pushState / window.location / navigator usage
no innerHTML / outerHTML / document.write / eval / Function
no className string concatenation; no setAttribute("class", ...)
no inline event attributes
no per-element inline style="..."
no <meta name="viewport"> (responsive layout deferred)
no save schema bump (still v12)
no new state field
no new artefact (still 10)
no new fixture
no new InterestGroupKind / PlayerCommandKind
no runner CLI flag
no neighbour / adjacency edges
no terrain / resources / population overlays
no change to provinces.svg bytes
no M4 close-out
no "M4 closed" wording
```

## 6. Test coverage

Unit (`tests/systems/svg_export_test.cpp`):

- `render_map_html: M4.12 <style> block carries
  circle.selected + text.selected rules` — pins the two
  CSS rules verbatim.
- `render_map_html: M4.12 click handler uses
  classList.add/remove on "selected"` — pins the
  classList API surface and the `selectProvince` helper
  name.
- `render_map_html: M4.12 initial render has NO
  class="selected" anywhere` — at first paint nothing is
  selected; the class only appears at click time.
- `render_map_html: M4.12 selection is DOM-only — no
  storage / persistence / state mutation surface` —
  reinforces the M4.10 no-persistence discipline and
  explicitly rejects `className` concatenation +
  `setAttribute("class", ...)` (the two paths a future
  refactor could regress into).
- `render_provinces (standalone SVG) does NOT include
  the M4.12 selection CSS or .selected handling` — the
  standalone SVG stays free of `.selected` / `classList` /
  `selectProvince` (HTML-wrapper-only feature).

The M4.9 integration test C, the M4.10 / M4.11 unit tests,
and the M4.10 runner test all stay green unchanged: none
assert anything M4.12 changes (still exactly one inline
`<script>` in `map.html`; still no `<script>` in
`provinces.svg`; still no `<link>` / inline event attrs /
per-element style anywhere; the legend's data-id-less
swatch circles still skip the click handler — same
selector, M4.12 only adds a new helper called from inside
the existing listener).

## 7. Cross-references

- [`m4-11-details-labels-polish.md`](m4-11-details-labels-polish.md)
  — the M4.11 details-panel polish M4.12 layers on top of.
  Both `showDetails` and the new `selectProvince` are
  called from the same per-click listener.
- [`m4-10-clickable-ui-skeleton.md`](m4-10-clickable-ui-skeleton.md)
  — the M4.10 click handler. The XSS-safe DOM API
  discipline, the no-storage / no-network constraint, the
  asymmetric one-inline-script invariant all carry over.
- [`m4-8-province-data-attributes-skeleton.md`](m4-8-province-data-attributes-skeleton.md)
  — the M4.8 widened `data-*` surface. M4.12 explicitly
  fulfils the M4.8 design intent ("a future clickable UI
  / DOM script can address either element uniformly
  without DOM-walking siblings") — the whole-pair
  selection reduces to filtering `nodes` by `data-id`.
- [`m4-9-dom-contract-checkpoint.md`](m4-9-dom-contract-checkpoint.md)
  — the M4 DOM contract checkpoint. M4.12 does not edit
  the M4.8 identity-surface attribute names; integration
  test C continues to pass unchanged (still exactly one
  inline script).
- [`m4-6-html-viewer-css-skeleton.md`](m4-6-html-viewer-css-skeleton.md)
  — the M4.6 single-CSS-surface rule (no per-element
  inline `style="..."`). M4.12 obeys it: the new
  `.selected` rules live in the `<style>` block.
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4 status snapshot. The "no clickable UI / no
  selection state" row of deferred items needs an
  asterisk after M4.12: clickable UI is now partial; a
  selection class exists but no selection persistence.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  the M3 invariants every future milestone must
  preserve. M4.12 is consistent: no schema bump, no new
  artefact, no command-gate change, no events / logs from
  the viewer, no state mutation.

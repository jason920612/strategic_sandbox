# M4.10 - HTML clickable UI skeleton

Companion notes for `feature/rfc090-m4-10-clickable-ui-skeleton`.

M4.10 is the **first JavaScript** in `map.html`. It adds a
stateless click handler that, when a province `<circle>` or
`<text>` element is clicked, populates a read-only details
panel with the clicked element's four `data-*` attributes.
That is the entire feature. **M4 stays in progress**; M4.10
is one more skeleton sub-milestone, not an M4 exit.

The detail surface lives in the HTML wrapper only;
`provinces.svg` stays fully script-free / inert.

## 1. Scope

What ships:

```text
src/leviathan/systems/svg_export.cpp
  - render_map_html: +4 CSS rules (.details / .details dl/dt/dd /
                     .details-empty / svg circle[data-id], svg
                     text[data-id] cursor)
                     +<div id="details" class="details">
                     placeholder between SVG and legend
                     +single end-of-body inline <script> IIFE
                     (the click handler)
include/leviathan/systems/svg_export.hpp
  - intro doc reframes M4.5's "no JavaScript" rule as
    asymmetric since M4.10 (provinces.svg still script-free;
    map.html carries exactly one inline script)
  - "What M4.X deliberately does NOT do" + Output shape
    sections updated for the new placeholder + script
tests/systems/svg_export_test.cpp                  +7 doctest cases,
                                                   1 retuned
tests/systems/runner_test.cpp                      +1 doctest case
tests/integration/m4_dom_contract_test.cpp         Test C split
                                                   into asymmetric
                                                   per-artefact
                                                   invariants
README.md / docs/README.md / rfc/README.md         M4.10 entry,
                                                   M4 still in
                                                   progress
docs/m4-10-clickable-ui-skeleton.md                this file
```

## 2. The click handler

The inline `<script>` is one IIFE at the very end of
`<body>`:

```js
(function () {
  var nodes = document.querySelectorAll(
    "svg circle[data-id], svg text[data-id]"
  );
  var panel = document.getElementById("details");
  if (!panel) { return; }
  for (var i = 0; i < nodes.length; ++i) {
    nodes[i].addEventListener("click", function (ev) {
      var el = ev.currentTarget;
      var dl = document.createElement("dl");
      var keys = ["data-id", "data-owner",
                  "data-owner-code", "data-name"];
      for (var k = 0; k < keys.length; ++k) {
        var dt = document.createElement("dt");
        dt.textContent = keys[k];
        var dd = document.createElement("dd");
        dd.textContent = el.getAttribute(keys[k]) || "";
        dl.appendChild(dt);
        dl.appendChild(dd);
      }
      panel.textContent = "";
      panel.appendChild(dl);
    });
  }
})();
```

Three pinned properties:

1. **Selector scoped to data-id-bearing svg circles + texts.**
   `svg circle[data-id], svg text[data-id]` — this
   deliberately excludes the legend swatch `<circle>`
   elements (M4.7), which carry NO `data-id`. The legend
   stays non-clickable.
2. **XSS-safe DOM API only.** `getAttribute` reads;
   `createElement` + `textContent` write. The script
   contains zero occurrences of `innerHTML` / `outerHTML` /
   `document.write` / `eval` / `Function`. A future
   M4.x sub-milestone that swaps any of these in trips
   `svg_export_test.cpp` test "click handler uses XSS-safe
   DOM API".
3. **No state mutation / no network / no storage.** The
   script contains zero occurrences of `fetch` /
   `XMLHttpRequest` / `localStorage` / `sessionStorage` /
   `history.pushState` / `window.location` / `navigator`.
   Click handlers only mutate the `<div id="details">`
   placeholder. The underlying `GameState` is unreachable
   from the viewer.

## 3. The CSS additions

```css
.details        { margin: 20px auto; max-width: 1000px;
                  font-family: sans-serif; }
.details dl     { margin: 0; }
.details dt     { font-weight: bold; }
.details dd     { margin: 0 0 8px 0; }
.details-empty  { color: #666; font-style: italic; }
svg circle[data-id], svg text[data-id] { cursor: pointer; }
```

All rules live in the M4.6 `<style>` block. **No
per-element inline `style="..."`** — the M4.6
single-CSS-surface contract holds. The `cursor: pointer`
rule uses the same selector as the click handler so the
clickable affordance and the click target stay in sync.

## 4. The HTML placeholder

```html
<div id="details" class="details">
  <p class="details-empty">Click a province to see its details.</p>
</div>
```

Inserted between the inline `<svg>` and the `<ul
class="legend">`. The first click replaces the placeholder
`<p>` with a `<dl>`. Subsequent clicks replace the previous
`<dl>` with a fresh one. The placeholder text is the only
human-language string in the HTML body besides the legend
rows and `<title>`.

## 5. What M4.10 does NOT do

```text
no state mutation (the viewer never writes back into GameState)
no commands / player actions / AI integration
no events emitted by the click
no selection persistence (next click replaces, not accumulates)
no multi-select / shift-click / right-click / context menu
no hover state / tooltip / mouseover
no keyboard navigation / focus ring / aria-* / a11y polish
no animation / transition on the panel repaint
no save schema bump (still v12)
no new state field
no new artefact (still 10; map.html bytes did change)
no new fixture
no new InterestGroupKind / PlayerCommandKind
no runner CLI flag
no second <script>, no <script src=>, no <script type="module">
no <link>, no external CSS, no external font, no <iframe>, no <img>
no fetch / XMLHttpRequest / localStorage / sessionStorage /
   history.pushState / window.location / navigator usage
no innerHTML / outerHTML / document.write / eval / Function
no inline event attributes (onclick="..." / onmouseover="..." / ...)
no per-element inline style="..."
no <meta name="viewport"> (responsive layout deferred)
no neighbour / adjacency edges
no terrain / resources / population overlays
no change to provinces.svg bytes (HTML-wrapper-only feature)
no M4 close-out
no "M4 closed" wording
```

## 6. Test coverage

Unit (`tests/systems/svg_export_test.cpp`):

- `render_map_html: emits <div id="details"> placeholder
  between SVG and legend`
- `render_map_html: emits exactly one inline <script> at
  end of body`
- `render_map_html: click handler uses XSS-safe DOM API
  (no innerHTML / no eval / no document.write)`
- `render_map_html: click handler does NOT mutate state or
  call out (no fetch / no XHR / no storage)`
- `render_map_html: click handler scopes to data-id-bearing
  circles / texts (legend swatches excluded)`
- `render_map_html: <style> block carries the M4.10
  details + cursor rules`
- `render_provinces (standalone SVG) does NOT include the
  M4.10 script or details panel`

Plus the M4.5/M4.6 "no `<script>`" case retuned to
"no `<link>`, no inline event attributes, no per-element
style" — the script presence is now positively asserted by
the new cases above.

Runner (`tests/systems/runner_test.cpp`):

- `run: canonical scenario emits the M4.10 clickable
  surface in map.html only`

Integration (`tests/integration/m4_dom_contract_test.cpp`):

- Test C ("no-stray-interactivity invariant") split into
  asymmetric per-artefact assertions:
  - `provinces.svg`: NO `<script>`, NO `<style>`, NO
    `font-family` (existing inertness intact).
  - `map.html`: EXACTLY ONE `<script>` ... `</script>`
    pair; no `<script src=>`, no `<script type=>`.
  - BOTH: no `<link>`, no common inline event attributes,
    no per-element `style="..."`.

## 7. Cross-references

- [`m4-9-dom-contract-checkpoint.md`](m4-9-dom-contract-checkpoint.md)
  — the M4.9 DOM contract checkpoint M4.10 is the first
  delta against. Test C in `m4_dom_contract_test.cpp`
  splits in this PR; the other two checkpoint invariants
  (uniform identity surface, legend 1:1) are unchanged.
- [`m4-8-province-data-attributes-skeleton.md`](m4-8-province-data-attributes-skeleton.md)
  — the M4.8 widened `data-*` surface is what the click
  handler reads via `getAttribute`. Without M4.8 the
  details panel would have one row, not four.
- [`m4-7-html-legend-skeleton.md`](m4-7-html-legend-skeleton.md)
  — the M4.7 legend swatch `<circle>` elements have NO
  `data-id`, so the M4.10 selector
  `svg circle[data-id]` correctly skips them.
- [`m4-6-html-viewer-css-skeleton.md`](m4-6-html-viewer-css-skeleton.md)
  — the M4.6 single-CSS-surface rule (no per-element
  inline `style="..."`) holds: the new `.details` / cursor
  rules live in the `<style>` block.
- [`m4-5-html-viewer-skeleton.md`](m4-5-html-viewer-skeleton.md)
  — the M4.5 "no JavaScript / no `<script>`" rule is
  **explicitly reframed** by M4.10. It now applies to
  `provinces.svg` only. `map.html` carries exactly one
  inline `<script>`. M4.10's tests pin the new shape
  (single inline script, no `src=`, no `type=`).
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4.9 status snapshot. Section "no interactivity"
  becomes "interactivity is HTML-wrapper-only, exactly one
  inline script" at the next checkpoint.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  the M3 invariants every future milestone must preserve.
  M4.10 is consistent: no schema bump, no new artefact,
  no command-gate change, no events / logs from the
  viewer, no state mutation.

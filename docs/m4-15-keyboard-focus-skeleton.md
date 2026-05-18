# M4.15 - keyboard focus accessibility skeleton

Companion notes for `feature/rfc090-m4-15-keyboard-focus-skeleton`.

M4.15 makes the province markers reachable from the
keyboard. Every `<circle>` and every `<text>` now carries
`tabindex="0"` (rendered in `render_svg_root`, so the
standalone `provinces.svg` picks it up too); the inline
`<script>` in `map.html` registers a `keydown` listener
alongside the existing `click` listener so pressing Enter
or Space while focused on a province fires the same
`selectProvince + showDetails` pair the click runs.

This is a **skeleton** — explicitly NOT a full ARIA polish.
Roles, labels, selected-state attributes, focus-visible
styling, and live regions are deferred to a future
dedicated A11Y sub-milestone.

## 1. Scope

What ships:

```text
src/leviathan/systems/svg_export.cpp
  - render_svg_root: emits `tabindex="0"` on every <circle>
    and every <text>. Lives on the SVG body (shared between
    provinces.svg and map.html). Legend swatch <circle>
    elements (rendered in render_map_html, NOT in
    render_svg_root) stay tabindex-free and lack data-id —
    they stay non-clickable + non-focusable.
  - render_map_html click-handler loop refactored so the
    per-element listener body is shared via an `activate()`
    closure. A second `addEventListener("keydown", ...)`
    listener tests `event.key === "Enter" || event.key === " "`
    and calls `event.preventDefault()` (Space-scroll
    suppression) before invoking `activate()`.

include/leviathan/systems/svg_export.hpp
  - Intro paragraph mentions M4.15 keyboard-focus surface.
  - "What ... deliberately do NOT do" heading extends to
    M4.15.
  - Output-shape section: <circle> + <text> attribute lists
    grow by tabindex="0"; script section gains the M4.15
    keydown paragraph.

tests/systems/svg_export_test.cpp                  +6 doctest cases
tests/integration/m4_dom_contract_test.cpp         +1 test (E)
README.md / docs/README.md / rfc/README.md         M4.15 entry,
                                                   M4 still in
                                                   progress
docs/m4-15-keyboard-focus-skeleton.md              this file
```

## 2. The DOM changes

### SVG body

Every `<circle>` + `<text>` in `render_svg_root` now
emits one more attribute:

```text
<circle ... tabindex="0" data-id=... data-owner=...
        data-owner-code=... data-owner-name=...
        data-name=.../>
<text   ... tabindex="0" data-id=... ...>NAME</text>
```

The attribute is a fixed literal `"0"` — not a value derived
from `ProvinceNode` data. It enters the natural document
tab order (`tabindex="0"`), as opposed to programmatic-only
focus (`tabindex="-1"`) or a custom order (`tabindex="N"`
with N > 0, which is widely considered an anti-pattern).

The legend swatch `<circle>` in `render_map_html` is
emitted separately (inside an inline `<svg class="swatch">`)
and does **not** carry `tabindex`. Two non-goals fall out
of that: the legend doesn't become a focus-trap; and the
M4.10 `svg circle[data-id]` selector already skips it
anyway, so nothing keyboard-activates a legend swatch by
accident.

### Click/keydown listener loop

Before (M4.10–M4.13):

```js
for (var j = 0; j < nodes.length; j++) {
  var el = nodes[j];
  el.addEventListener("click",
    (function(e) { return function() {
      selectProvince(e); showDetails(e); }; })(el));
}
```

After (M4.15):

```js
for (var j = 0; j < nodes.length; j++) {
  (function(el) {
    function activate() {
      selectProvince(el);
      showDetails(el);
    }
    el.addEventListener("click", activate);
    el.addEventListener("keydown", function(ev) {
      if (ev.key === "Enter" || ev.key === " ") {
        ev.preventDefault();
        activate();
      }
    });
  })(nodes[j]);
}
```

Three properties worth pinning:

1. **Shared `activate()` closure.** The click handler and
   the keydown handler invoke the exact same function body
   — they cannot drift in behaviour. If a future
   sub-milestone changes what a click does (e.g. selection
   toggle, focus-out, etc.), the keyboard activation
   inherits it automatically.

2. **`event.key` checks.** Modern best practice (the
   legacy `event.keyCode` / `event.which` are deprecated).
   Enter is `"Enter"`; the Space key is `" "` (a literal
   single-character space). Both forms are stable across
   browsers and DOM Level 3 Events compliant.

3. **`event.preventDefault()` on Space.** Without this,
   pressing Space while focused on a marker scrolls the
   page down by one screen — the browser default. Calling
   it suppresses the scroll so the only effect is the
   intended activation.

## 3. Why a skeleton, not a full ARIA polish

The reviewer's spec was explicit:

> 不要做 ARIA 大改、persistent selection、tooltip、hover、
> commands、state mutation、CLI flag、events、AI、artifact
> 或 schema.

A keyboard-focus skeleton without ARIA polish is
deliberately limited:

- Screen readers will announce the SVG markers as plain
  graphics, not as interactive controls. A `role="button"`
  or `role="checkbox"` would change that announcement.
- There is no `aria-selected` on the `.selected` element,
  so screen readers won't read out the current selection
  state.
- The details panel doesn't have a live region, so updates
  aren't announced as the user navigates.

These are the natural next steps for a future M4.x A11Y
PR, but each one carries its own design decisions
(button vs option vs radio semantics; live region politeness;
labelledby vs label vs title). Keeping them out of M4.15
makes the keyboard-focus surface a one-PR change with a
clear contract: **you can Tab to a province and Enter or
Space activates it**, full stop.

## 4. What M4.15 does NOT do

```text
no ARIA polish (no role= / aria-label= / aria-selected= /
   aria-current= / aria-pressed= / aria-describedby= /
   aria-live= / aria-labelledby=)
no :focus / :focus-visible CSS styling (the browser's
   default focus ring is what users see)
no persistent focus state across reloads
no focus management between renders (e.g. restoring focus
   to the previously-selected element)
no tabindex values other than "0" (no programmatic-only
   focus; no manual ordering)
no keyboard shortcut for the details panel (e.g. Escape to
   clear, Tab to navigate within the panel)
no skip-link / landmark navigation
no screen reader testing infrastructure
no state mutation, no commands, no AI integration
no events emitted by the keyboard activation
no selection persistence
no multi-select / shift-click / shift-Enter / right-click
no hover state, no tooltip, no mouseover
no animation / transition
no second <script>, no <script src=>, no <script type=>
no <link>, no external CSS, no external font
no fetch / XMLHttpRequest / localStorage / sessionStorage /
   history.pushState / window.location / navigator usage
no innerHTML / outerHTML / document.write / eval / Function
no inline event attributes (onkeydown= / ...; the M4.15
   keydown wiring uses addEventListener exclusively)
no per-element inline style="..."
no <meta name="viewport">
no save schema bump (still v12) — tabindex is render-time
   only, not a new field on ProvinceNode
no new state field
no new artefact (still 10)
no new fixture
no new InterestGroupKind / PlayerCommandKind
no runner CLI flag
no neighbour / adjacency edges
no terrain / resources / population overlays
no M4 close-out
no "M4 closed" wording
```

`provinces.svg` AND `map.html` bytes both DID change (the
new `tabindex="0"` attribute on every `<circle>` +
`<text>`; the refactored listener loop + new keydown
wiring in the script). The SVG change is additive; the
script-loop refactor preserves the exact set of effects
(activate = selectProvince + showDetails).

## 5. Test coverage

Unit (`tests/systems/svg_export_test.cpp`):

- `render_provinces: M4.15 every <circle> + <text> carries
  tabindex="0"` — pins the count at 2 per province pair.
- `render_provinces: M4.15 tabindex appears inside the
  <circle> AND <text> opening tags` — locates each opening
  tag and asserts `tabindex="0"` is inside it (not
  hoisted to the root or appended outside).
- `render_map_html: M4.15 legend swatch <circle> elements
  do NOT carry tabindex` — extracts the legend swatch
  `<svg class="swatch">` block and asserts no `tabindex`
  appears inside.
- `render_map_html: M4.15 inline <script> wires a keydown
  listener with Enter + Space + preventDefault` — pins
  the listener shape: both addEventListener calls present,
  the `"Enter"` and `" "` key literals appear,
  `preventDefault` appears, and the shared `activate()`
  helper name is present.
- `render_map_html: M4.15 carries NO ARIA polish (explicit
  non-goal of this skeleton)` — explicit absence checks
  for `role=`, `aria-label=`, `aria-selected=`,
  `aria-current=`, `aria-pressed=`, `tabindex="-1"`,
  `tabindex="1"`.
- `render_provinces (standalone SVG) DOES carry tabindex
  (same SVG body as map.html)` — confirms `render_provinces`
  picks up the M4.15 widening through the shared
  `render_svg_root` helper, but the standalone SVG still
  has no `<script>` / `addEventListener` / `keydown`.

Integration (`tests/integration/m4_dom_contract_test.cpp`):

- New test E: `M4 DOM contract: M4.15 tabindex="0" on every
  canonical province circle+text in both artefacts; legend
  swatch is NOT focusable; keydown listener wired in
  map.html` — end-to-end count: canonical scenario has 3
  provinces × 2 elements = 6 `tabindex="0"` occurrences in
  both `provinces.svg` and `map.html`; legend swatches add
  zero to the count; `map.html` has the keydown listener
  + Enter check + preventDefault; `provinces.svg` carries
  none of the script wiring.

The M4.9/M4.14 integration tests A/B/C/D all stay green
unchanged: A counts data-* attribute substrings (M4.15
doesn't touch them); B checks legend rows (untouched); C's
asymmetric one-script invariant still holds (still exactly
one `<script>`); D's fields-list contract is unchanged
(M4.15 doesn't add a field). The M4.10–M4.13 unit tests
all still pass — they assert presence of `addEventListener`
and the existing handler bits, none of which were removed.

## 6. Cross-references

- [`m4-12-selected-state-css-skeleton.md`](m4-12-selected-state-css-skeleton.md)
  — the M4.12 `selectProvince(el)` helper M4.15's keydown
  listener reuses. The selection contract (transient,
  DOM-only, no persistence) carries over.
- [`m4-11-details-labels-polish.md`](m4-11-details-labels-polish.md)
  — the M4.11 `showDetails(el)` + fields-array surface
  M4.15's keydown listener also invokes via `activate()`.
- [`m4-10-clickable-ui-skeleton.md`](m4-10-clickable-ui-skeleton.md)
  — the M4.10 click handler M4.15 extends. The XSS-safe
  DOM API, no-storage / no-network discipline, asymmetric
  one-inline-script invariant all carry over.
- [`m4-8-province-data-attributes-skeleton.md`](m4-8-province-data-attributes-skeleton.md)
  — the M4.8 widening pattern M4.15 mirrors (one new
  attribute per circle + text). `tabindex` is a fixed
  literal, not a derived value, so the attribute-escape
  helpers don't apply.
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4 status snapshot (refreshed at M4.14). M4.15
  adds keyboard focus to the interactivity-surface
  section; the snapshot will need a future refresh to
  describe it.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  the M3 invariants every future milestone must
  preserve. M4.15 is consistent: no schema bump, no new
  artefact, no command-gate change, no events / logs
  from the viewer, no state mutation.

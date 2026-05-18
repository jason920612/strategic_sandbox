# M4.11 - clickable UI details labels polish

Companion notes for `feature/rfc090-m4-11-details-labels-polish`.

M4.11 is a pure UX polish on the M4.10 click handler. The
`<dt>` labels rendered into the `<div id="details">` panel
are decoupled from the raw `data-*` attribute names. The
M4.8 DOM identity contract on the `<circle>` / `<text>`
elements is **NOT renamed**; `getAttribute` still reads
`data-id` / `data-owner` / `data-owner-code` / `data-name`.
Only the panel's `<dt>` body strings change.

## 1. Scope

What ships:

```text
src/leviathan/systems/svg_export.cpp
  - render_map_html click handler: `var keys = [...]` →
    `var fields = [{attr, label}, ...]`. `dt.textContent`
    now reads `f.label` (human-readable string);
    `getAttribute` still reads `f.attr` (raw data-* key).
include/leviathan/systems/svg_export.hpp
  - Intro paragraph mentions M4.11's label decoupling.
  - Output-shape section's details-panel bullet documents
    the four labels.
tests/systems/svg_export_test.cpp                  +4 doctest cases
README.md / docs/README.md / rfc/README.md         M4.11 entry,
                                                   M4 still in
                                                   progress
docs/m4-11-details-labels-polish.md                this file
```

The four labels:

```text
data-id          → "Province ID"
data-owner       → "Owner Index"
data-owner-code  → "Owner Code"
data-name        → "Province Name"
```

## 2. Why this is a separate sub-milestone, not a follow-up commit

M4.10 shipped the read-only details panel using raw `data-*`
keys as `<dt>` labels because the reviewer's spec was
explicit: stateless, no UI polish in M4.10 itself. M4.11 is
the next M-pacing step — UX polish only, no schema, no
commands, no state mutation, no new behaviour. Keeping it
separate preserves the per-sub-milestone PR discipline and
keeps the per-sub-milestone design notes in shape.

## 3. The renderer change

Before (M4.10):

```js
var keys = ["data-id", "data-owner",
            "data-owner-code", "data-name"];
function showDetails(el) {
  // ...
  for (var i = 0; i < keys.length; i++) {
    var k = keys[i];
    var dt = document.createElement("dt");
    dt.textContent = k;                       // raw attr name
    var dd = document.createElement("dd");
    dd.textContent = el.getAttribute(k) || "";
    // ...
  }
}
```

After (M4.11):

```js
var fields = [
  { attr: "data-id",         label: "Province ID"   },
  { attr: "data-owner",      label: "Owner Index"   },
  { attr: "data-owner-code", label: "Owner Code"    },
  { attr: "data-name",       label: "Province Name" }
];
function showDetails(el) {
  // ...
  for (var i = 0; i < fields.length; i++) {
    var f = fields[i];
    var dt = document.createElement("dt");
    dt.textContent = f.label;                 // human-readable
    var dd = document.createElement("dd");
    dd.textContent = el.getAttribute(f.attr) || "";
    // ...
  }
}
```

The lookup key (the argument to `getAttribute`) is still
the M4.8 DOM contract attribute name. The display label
(the body of the `<dt>`) is a separate string. A future
clickable-UI consumer that DOM-queries the panel for
`Province Name` finds it; one that DOM-queries the SVG for
`data-name` still finds it. The two surfaces are
independent.

## 4. What did NOT change

```text
the M4.8 DOM identity contract on <circle> / <text>
   (the four data-* attribute names — UNCHANGED)
the M4.10 selector "svg circle[data-id], svg text[data-id]"
the M4.10 XSS-safe DOM API (textContent + createElement only;
   no innerHTML / outerHTML / document.write / eval / Function)
the M4.10 no-network / no-storage discipline (no fetch /
   XHR / localStorage / sessionStorage / history.pushState /
   window.location / navigator)
the M4.10 "exactly one inline <script>; no src=; no type="
   invariant
provinces.svg bytes (still byte-identical with M4.8)
the artefact set (still 10 files)
save format (still v12)
the canonical scenario fixtures
the byte-identical determinism contracts
   (M1.17 / M2.22 / M3.7)
```

`map.html` bytes DID change (four new label strings inside
the inline `<script>`; new `fields` array structure).

## 5. What M4.11 does NOT do

```text
no rename of the M4.8 data-* DOM contract keys
no state mutation (still read-only)
no commands / player actions / AI integration
no events emitted by the click
no selection persistence (next click still replaces, not
   accumulates)
no multi-select / shift-click / right-click / context menu
no hover state / tooltip / mouseover
no keyboard navigation / focus ring / aria-* / a11y polish
   (separate future sub-milestone)
no animation / transition on the panel repaint
no second <script>, no <script src=>, no <script type=>
no <link>, no external CSS, no external font, no <iframe>,
   no <img>
no fetch / XMLHttpRequest / localStorage / sessionStorage /
   history.pushState / window.location / navigator usage
no innerHTML / outerHTML / document.write / eval / Function
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

- `render_map_html: M4.11 click handler emits the four
  human-readable <dt> labels` — pins the four label strings
  appear as JS string literals inside the inline script.
- `render_map_html: M4.11 still uses raw data-* attr names
  for getAttribute (M4.8 DOM contract unchanged)` — pins
  the four `data-*` keys still appear and that the
  `getAttribute` / `textContent` API surface is intact.
- `render_map_html: M4.11 changes the labels rendered into
  <dt>, NOT the <circle> / <text> attributes themselves` —
  builds a canonical-ish state and checks the four data-*
  attributes still appear on `<circle>` / `<text>` (the
  M4.8 contract is byte-stable from the SVG side).
- `render_provinces (standalone SVG) does NOT carry the
  M4.11 details labels` — confirms the labels are
  HTML-wrapper-only; the standalone SVG never picks them
  up.

The existing M4.9 integration test C and the M4.10 runner
test both stay green unchanged: neither asserts the `<dt>`
body content, both pin the surface (one `<script>`, no
extra `<link>` / event attrs / per-element style) that
M4.11 leaves alone.

## 7. Cross-references

- [`m4-10-clickable-ui-skeleton.md`](m4-10-clickable-ui-skeleton.md)
  — the M4.10 click handler M4.11 polishes. The XSS-safe
  DOM API, the no-storage / no-network discipline, the
  asymmetric "exactly one inline script in map.html"
  invariant all carry over unchanged.
- [`m4-9-dom-contract-checkpoint.md`](m4-9-dom-contract-checkpoint.md)
  — the M4 DOM contract checkpoint. M4.11 does not edit
  the M4.8 identity-surface attribute names that the
  checkpoint pins; integration test C continues to enforce
  the asymmetric one-script invariant unchanged.
- [`m4-8-province-data-attributes-skeleton.md`](m4-8-province-data-attributes-skeleton.md)
  — the M4.8 widened `data-*` identity surface. M4.11
  reads but does not rename these.
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4 status snapshot. The "details panel labels"
  row gains four entries; nothing else changes.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  the M3 invariants every future milestone must preserve.
  M4.11 is consistent: no schema bump, no new artefact,
  no command-gate change, no events / logs from the
  viewer, no state mutation.

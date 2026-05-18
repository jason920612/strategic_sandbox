# M4.20 - hover tooltip skeleton

Companion notes for `feature/rfc090-m4-20-tooltip-skeleton`.

M4.20 adds a small **hover-status text bar** between the
inline SVG and the M4.10 details panel. Mousing over a
province marker updates the bar with the composed string
`"<name> (<owner-name>)"`; mousing out clears it. Pure
HTML + CSS + minimal JS — no SVG `<title>` child element,
no tooltip popup, no innerHTML, no state mutation.

This is the "tooltip skeleton" the reviewer flagged as
needing a design decision: **status text bar** was chosen
over SVG `<title>` (would conflict with the M4.17
`aria-label` as the accessible name) and over a
position-aware floating tooltip (more scope than a
skeleton).

## 1. Scope

What ships:

```text
src/leviathan/systems/svg_export.cpp
  - render_map_html <style>: +1 CSS rule
      .hover-status { max-width: 1000px; margin: 8px auto;
                       min-height: 1em; font-family: sans-serif;
                       color: #666; font-style: italic;
                       text-align: center; }
  - render_map_html <body>: +1 element
      <p id="hover-status" class="hover-status">&nbsp;</p>
    placed between </svg> and <div id="details"> (so the
    bar sits visually between the map and the click-driven
    panel).
  - render_map_html <script>:
      +1 var: hoverStatus = document.getElementById("hover-status")
      +2 listeners per element (in the same per-element
        loop as click + keydown):
          mouseover → hoverStatus.textContent =
              owner ? (name + " (" + owner + ")") : name
          mouseout  → hoverStatus.textContent = ""
        Reads via getAttribute on data-name + data-owner-name
        (the M4.8 + M4.13 surface — no new attribute).

include/leviathan/systems/svg_export.hpp
  - Intro paragraph reframes the M4.19 "no JS hover" non-goal
    as narrowly reversed by M4.20 (only mouseover + mouseout
    via addEventListener; mouseenter / mouseleave /
    .onmouseover / .onmouseout / onmouseover= still absent).
  - Output-shape: <style> rules list gains .hover-status;
    <body> shape gains item 2 <p id="hover-status">; script
    section gains the M4.20 listener block; the legend
    and script items renumber 3 → 4 and 4 → 5.
  - "Deliberately do NOT do" frame extends to M4.20.

docs/milestone-4-checkpoint.md (INLINE refresh per
feedback_checkpoint_drift)
  - Sub-milestones list adds M4.20.
  - HTML wrapper shape: <style> rules list gains
    .hover-status; <body> gains <p id="hover-status">;
    <script> gains mouseover/mouseout listeners.
  - <style> selector count 19 → 20.
  - Interactivity surface section adds a hover-status bullet.
  - HOVER + TOOLTIPS deferred bucket rewrites: hover-status
    bar SHIPPED; pair-hover, position-aware tooltip, SVG
    <title>, hover-driven panel preview, mouseenter/leave
    still deferred.
  - "Refreshed at" stamp extends to M4.20.

tests/systems/svg_export_test.cpp
  - Retunes the M4.19 "no JS hover handler" test:
    mouseover + mouseout via addEventListener now
    legitimately appear; the test asserts the narrower
    still-deferred surface (mouseenter / mouseleave /
    .onmouseover / .onmouseout / onmouseover= /
    onmouseout=) is absent; the <head> <title> +
    "no <title> in body" assertions preserved verbatim.
  - +7 new M4.20 doctest cases (placement / CSS / listener
    wiring / XSS-safe / no-network / no-detail-panel-touch
    / standalone provinces.svg has none of it).

README.md / docs/README.md / rfc/README.md   M4.20 entry, M4
                                             still in progress
docs/m4-20-tooltip-skeleton.md               this file
```

## 2. Why status-text bar (not SVG `<title>`, not floating tooltip)

The reviewer's spec for M4.20 was explicit:

> M4.20 tooltip skeleton ... 不要用 SVG `<title>` child；建議
> 做純 HTML details-adjacent tooltip / status text，或先做
> tooltip design decision，再開 PR.

**SVG `<title>` rejected because:**

- `<title>` is the SVG-native tooltip mechanism and is also
  what browsers expose as the **accessible name** when no
  `aria-label` / `aria-labelledby` is present. With both
  `<title>` and `aria-label` (M4.17), some browsers prefer
  one over the other — the consistency between sighted +
  screen-reader users would degrade.
- The reviewer flagged this explicitly.

**Floating / position-aware tooltip rejected because:**

- Requires JS positioning logic (read cursor coords,
  compute popup position with viewport-edge avoidance).
- Requires more CSS (absolute positioning, z-index, arrow
  pseudo-elements, etc.).
- "Skeleton" implies minimal — a fixed-position bar is
  enough to convey "this marker has hover info".

**Status text bar chosen because:**

- One small HTML element (`<p>`) + one CSS rule + ~10
  lines of JS. Minimal.
- Sits in the document flow above the details panel —
  visually adjacent ("details-adjacent" per the reviewer's
  wording) without floating layout.
- Browser status bar idiom: most users know "the bar
  shows info about what you're hovering". Self-explanatory.
- Reuses existing M4.8/M4.13 `data-*` attrs — no new
  attribute, no new state field, no schema growth.

## 3. The composed label

```js
hoverStatus.textContent = owner
  ? (name + " (" + owner + ")")
  : name;
```

where `name = el.getAttribute("data-name")` and
`owner = el.getAttribute("data-owner-name")`.

The format mirrors the M4.17 `aria-label` shape (`"<name>,
<owner-name>"`) but uses parentheses instead of a comma so
sighted users get a slightly different visual register
(parens read as "secondary info"). Both surfaces still
agree on **what** the marker represents — same `name`,
same `owner-name` — they just present it slightly
differently for the two modalities.

When `owner` is empty (invalid owner index — see the
M4.13 defensive fallback), the label degrades to just
`name`. Same fallback shape as the M4.17 aria-label.

`textContent` (NOT `innerHTML`) keeps the write XSS-safe:
even if a future renderer regression emitted an unescaped
`data-name` attribute, the JS write would still treat the
value as plain text and not re-interpret HTML.

## 4. Why mouseover/mouseout, not mouseenter/mouseleave

Three subtle differences:

- **`mouseover` / `mouseout` bubble**; `mouseenter` /
  `mouseleave` do not. M4 markers don't have nested
  children that would benefit from the non-bubble
  semantics, so the simpler bubbling-pair is fine.
- **`mouseover` fires every time the pointer enters a
  child element**; `mouseenter` fires only when entering
  the element itself. Again, M4 markers have no children,
  so the behaviour is identical.
- **`mouseover` is the older, broader-support pair.**
  Both are widely supported in modern browsers (M4.16
  `:focus-visible` already requires modern), but
  `mouseover` carries less subtlety.

Choosing the bubbling pair also keeps the M4.20 surface
intentionally minimal: a future M4.x sub-milestone that
wants enter/leave semantics (e.g. to count hover events,
or to track total hover dwell time) can layer
`mouseenter`/`mouseleave` on top without breaking
`mouseover`/`mouseout` clients.

## 5. What M4.20 does NOT do

```text
no SVG <title> child element on markers (would compete
   with the M4.17 aria-label as the accessible name)
no position-aware tooltip near cursor (the M4.20 bar is
   fixed-position above the details panel)
no pair-hover (hovering a circle does NOT toggle a CSS
   class on its sibling text — would need JS classList
   manipulation; M4.20 only writes to the hover-status bar)
no mouseenter / mouseleave (M4.20 uses the bubbling
   mouseover / mouseout pair)
no element.onmouseover / .onmouseout property assignment
no inline onmouseover= / onmouseout= attributes
no hover delay (the bar updates immediately on mouseover
   and clears immediately on mouseout)
no hover-driven detail-panel preview (click and hover
   are independent surfaces; the M4.20 mouseover listener
   does NOT call showDetails or selectProvince and does
   NOT touch the details element)
no animation / transition on the bar update
no aria-live region on the hover-status bar (broader
   ARIA still deferred from M4.17)
no innerHTML / outerHTML / document.write / eval /
   Function / insertAdjacentHTML (M4.10 XSS-safe contract
   preserved)
no fetch / XHR / localStorage / sessionStorage / cookie /
   history / window.location / navigator
no inline event attributes (onclick= / onkeydown= /
   onmouseover= / ...)
no per-element inline style="..."
no <meta name="viewport">
no save schema bump (still v12 — hover-status reads
   render-time attrs, no new persistent state)
no new state field
no new artefact (still 10)
no new fixture
no new InterestGroupKind / PlayerCommandKind
no runner CLI flag
no neighbour / adjacency edges
no terrain / resources / population overlays
no change to provinces.svg bytes (hover-status surface
   is HTML-only)
no M4 close-out
no "M4 closed" wording
```

`provinces.svg` bytes UNCHANGED from M4.19 — the
hover-status surface lives entirely in the HTML wrapper.
`map.html` bytes did change (one new HTML element, one
new CSS rule, two new listener types per province in the
script).

## 6. Test coverage

The M4.19 "no JS hover handler" test retunes:

- Now asserts `addEventListener("mouseover")` and
  `addEventListener("mouseout")` ARE present.
- Still asserts the narrower still-deferred surface is
  absent: `"mouseenter"` / `"mouseleave"` (event names),
  `.onmouseover` / `.onmouseout` (property assignment),
  `onmouseover=` / `onmouseout=` (inline attribute form).
- Preserves the M4.19 fix: `<head>` `<title>Leviathan
  Map</title>` is the only `<title>` in the document; no
  `<title>` inside `<body>`.

Seven new M4.20 cases pin:

- The `<p id="hover-status" class="hover-status">&nbsp;</p>`
  element appears between `</svg>` and `<div id="details">`.
- The `.hover-status` CSS rule appears with `max-width`,
  `min-height: 1em` (layout-jump prevention), and
  `font-style: italic` (transient-UI marker).
- The script wires `addEventListener("mouseover")` +
  `addEventListener("mouseout")`, looks up
  `getElementById("hover-status")`, writes via
  `hoverStatus.textContent`, and reads via
  `getAttribute("data-name")` + `getAttribute("data-owner-name")`.
- The handler is XSS-safe: no `innerHTML` / `outerHTML` /
  `document.write` / `eval` / `Function` /
  `insertAdjacentHTML`.
- The handler doesn't mutate state or call out: no
  `fetch` / `XMLHttpRequest` / `localStorage` /
  `sessionStorage` / `document.cookie` /
  `history.pushState` / `window.location` / `navigator`.
- The mouseover handler does NOT call `showDetails` /
  `selectProvince` / touch `details.` / use `.classList`
  on hovered elements — hover and click are independent
  surfaces.
- Standalone `provinces.svg` has no `hover-status` /
  `.hover-status` / `mouseover` / `mouseout` /
  `textContent`.

Integration tests A/B/C/D/E/F all stay green unchanged.
M4.10/M4.12/M4.15/M4.16/M4.17/M4.19 unit tests all stay
green — additive surface only.

Local verification used `./build/bin/Debug/leviathan_tests.exe`
directly (per `feedback_ctest_masks_doctest`): 886 cases,
61651 assertions, 0 failed.

## 7. Cross-references

- [`m4-19-hover-affordance-skeleton.md`](m4-19-hover-affordance-skeleton.md)
  — the M4.19 `:hover` CSS that M4.20 builds on. M4.19
  was the visual cue ("this marker reacts to me"); M4.20
  is the textual cue ("here's what it is"). Both surfaces
  coexist — the user gets both the grey stroke (M4.19)
  AND the status-bar text (M4.20) on hover.
- [`m4-17-aria-labels-skeleton.md`](m4-17-aria-labels-skeleton.md)
  — the M4.17 `aria-label` that M4.20's `data-owner-name`
  fallback semantics mirror. Same composed shape (just
  parentheses instead of a comma for sighted/visual users).
- [`m4-13-details-owner-name-polish.md`](m4-13-details-owner-name-polish.md)
  — the M4.13 widening that gave `<circle>` + `<text>`
  the `data-owner-name` attribute M4.20 reads.
- [`m4-10-clickable-ui-skeleton.md`](m4-10-clickable-ui-skeleton.md)
  — the M4.10 click handler that M4.20's mouseover/mouseout
  layer alongside. Same per-element loop, same XSS-safe
  DOM-API discipline, asymmetric one-inline-script
  invariant carries over.
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4 status snapshot; **inline-refreshed in this
  PR** per the `feedback_checkpoint_drift` rule.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  M3 invariants. M4.20 preserves all of them.

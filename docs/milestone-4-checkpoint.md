# Milestone 4 Checkpoint

Status: in progress

Refreshed at **M4.18** (companion notes for
`feature/rfc090-m4-18-accessibility-checkpoint-refresh`).
Originally written at M4.9 (M4.2–M4.8 SVG / HTML DOM
contract); refreshed at M4.14 to cover M4.10–M4.13 (first
JavaScript, decoupled labels, transient `.selected`, fifth
`data-owner-name`); refreshed again at M4.18 to cover
M4.15 (keyboard focus), M4.16 (visible focus ring), and
M4.17 (ARIA labels).

M4.18 is a checkpoint refresh, **not an exit report**. M4
remains in progress: this file is updated so the next
future M4.x sub-milestone has one canonical snapshot to
read. No new system, formula, artefact, save-schema bump,
runner CLI flag, gameplay branch, or new interactivity.
Renderer bytes are byte-identical with M4.17 — M4.18 only
adds tests + docs.

The companion exit report (`docs/milestone-4-result.md`) is
deliberately not written yet. The close-out lands when the
reviewer decides M4 is done, not at any checkpoint.

This file mirrors `docs/milestone-3-checkpoint.md`'s role:
a single-page snapshot of M4 state as of M4.18 that future
sub-milestones can read for "what does the contract look
like right now?" without piecing together seventeen
per-sub-milestone notes.

## 1. M4 sub-milestones shipped

```text
M4.1   ProvinceNode data layer (save v11 → v12)
M4.2   first SVG renderer + provinces.svg as 9th artefact
M4.3   per-owner palette (kOwnerPalette + color_for_owner)
M4.4   per-node <text> label
M4.5   HTML wrapper + map.html as 10th artefact
M4.6   minimal viewer CSS (body / svg / svg text)
M4.7   static legend (<ul class="legend">)
M4.8   widened identity surface (4 data-* on <circle> AND <text>)
M4.9   DOM contract checkpoint (original M4.2–M4.8 snapshot)
M4.10  first inline <script> in map.html — stateless click
       handler + <div id="details"> panel; asymmetric JS
       boundary (provinces.svg stays script-free)
M4.11  details panel <dt> labels decoupled from raw data-* keys
M4.12  transient .selected class + circle.selected /
       text.selected CSS + selectProvince(el) helper
M4.13  widened identity surface (5 data-* — added data-owner-name)
M4.14  DOM contract checkpoint refresh (covered M4.10–M4.13)
M4.15  keyboard focus skeleton — tabindex="0" on circle+text
       + keydown Enter/Space listener (no ARIA polish yet)
M4.16  focus-visible styling skeleton — :focus-visible CSS
       rings (#1976d2 blue) so M4.15 focus is visible
M4.17  ARIA labels skeleton — role="button" + aria-label=
       "<name>, <owner_name>" on circle+text (narrowly
       reverses the M4.15/M4.16 "no ARIA" non-goal)
M4.18  accessibility checkpoint refresh (this PR)
```

## 2. Current artefact contract

End-of-run produces ten files. M3 close shipped eight; M4
added two (M4.2 and M4.5):

```text
save.json                                  (M0.8,  required)
events.jsonl                               (M0.6,  required)
summary.csv                                (M0.10, opt-in)
countries.csv                              (M1.14, opt-in)
factions.csv                               (M1.16, opt-in)
interest_groups.csv                        (M3.5,  unconditional)
interest_group_country_feedback.csv        (M3.6,  unconditional)
interest_group_authority_pressure.csv      (M3.6,  unconditional)
provinces.svg                              (M4.2,  unconditional)
map.html                                   (M4.5,  unconditional)
```

`provinces.svg` and `map.html` share the same `<svg>` body
via the `render_svg_root` helper (M4.5 refactor); the
difference is only the wrapper (XML prolog + standalone SVG
vs HTML5 doc with `<head>` + `<body>` + details panel +
legend + click-handler script).

**Important asymmetry, established at M4.10:** the
provinces.svg / map.html bodies are byte-identical for the
SVG, but the HTML wrapper carries additional surfaces
(`<style>` block, `<div id="details">`, `<ul class="legend">`,
inline `<script>`) that `provinces.svg` deliberately never
gets. `render_svg_root` is the **shared** source of truth
for the SVG body; the HTML wrapper material is appended
only inside `render_map_html`.

## 3. SVG body shape (shared between provinces.svg and map.html)

```text
<svg xmlns="http://www.w3.org/2000/svg"
     viewBox="0 0 1000 1000"
     width="1000" height="1000">

  for each ProvinceNode p in state.provinces (vector order):
    <circle
      cx="<p.x * 1000, fixed 2dp>"
      cy="<p.y * 1000, fixed 2dp>"
      r="8"
      fill="<color_for_owner(p.owner)>"
      tabindex="0"                                          // M4.15
      role="button"                                         // M4.17
      aria-label="<xml_attr_escape(aria_label_raw)>"        // M4.17
      data-id="<xml_attr_escape(p.id_code)>"
      data-owner="<p.owner.value()>"
      data-owner-code="<xml_attr_escape(owner_code)>"
      data-owner-name="<xml_attr_escape(owner_name)>"       // M4.13
      data-name="<xml_attr_escape(p.name)>"/>
    <text
      x="<cx>"
      y="<cy + kLabelYOffset>"            // kLabelYOffset = 22.0
      text-anchor="middle"
      tabindex="0"                                          // M4.15
      role="button"                                         // M4.17
      aria-label="<xml_attr_escape(aria_label_raw)>"        // M4.17
      data-id="<xml_attr_escape(p.id_code)>"
      data-owner="<p.owner.value()>"
      data-owner-code="<xml_attr_escape(owner_code)>"
      data-owner-name="<xml_attr_escape(owner_name)>"       // M4.13
      data-name="<xml_attr_escape(p.name)>"
    ><xml_text_escape(p.name)></text>

</svg>
```

where:

- `color_for_owner` returns
  `kOwnerPalette[owner.value() % kOwnerPaletteSize]` for
  valid owners and `kOwnerFallbackFill` (`#888888`) for
  negative owners.
- `owner_code`, `owner_name` resolve from
  `state.countries[owner.value()]` when the owner index is
  valid; **both fall back to `""`** under a single shared
  bounds check (they cannot disagree about validity).
- `aria_label_raw` composes at render time:
  `owner_name.empty() ? p.name : (p.name + ", " + owner_name)`.
  The whole composed string is escaped via
  `xml_attr_escape` as a single value.
- Insertion order follows `state.provinces` (no sort).
- LF line terminators throughout; fixed 2-space indent.
- `data-*` attribute values use `xml_attr_escape` (`& < >
  " '` → entities); the `<text>` body uses
  `xml_text_escape` (`& < >` only).
- `tabindex="0"` and `role="button"` are fixed literals,
  not escape sites.
- Empty `state.provinces` produces a header-only SVG —
  the `<svg>` element is still written so the artefact
  contract is consistent.

The identity surface widened twice: M4.8 took the surface
from two attrs (`data-id`, `data-owner`) to four
(`data-owner-code`, `data-name`); M4.13 widened it to five
(`data-owner-name`). The interactivity / a11y surface
widened three more times: M4.15 added `tabindex="0"`; M4.17
added `role="button"` + `aria-label`. Each widening is
additive — no removed or renamed attribute.

## 4. HTML wrapper shape (map.html)

```text
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Leviathan Map</title>
  <style>
  body      { margin: 0; padding: 20px; background-color: #f0f0f0; }
  svg       { display: block; margin: 0 auto;
              border: 1px solid #888;
              background-color: #ffffff; }
  svg text  { font-family: sans-serif; }
  .legend   { list-style: none; padding: 0;
              margin: 20px auto; max-width: 1000px;
              font-family: sans-serif; }
  .legend li      { display: flex; align-items: center;
                    margin: 4px 0; }
  .legend .swatch { width: 16px; height: 16px;
                    margin-right: 8px; flex-shrink: 0; }
  .details        { max-width: 1000px; margin: 20px auto;        // M4.10
                    padding: 12px; background-color: #ffffff;
                    border: 1px solid #888; font-family: sans-serif; }
  .details dl     { margin: 0; }
  .details dt     { font-weight: bold; margin-top: 4px; }
  .details dd     { margin: 0 0 4px 16px; }
  .details-empty  { margin: 0; color: #666; }
  svg circle[data-id], svg text[data-id]                          // M4.10
                  { cursor: pointer; }
  svg circle.selected { stroke: #000000; stroke-width: 3; }       // M4.12
  svg text.selected   { font-weight: bold; }                      // M4.12
  svg circle:focus { outline: none; }                             // M4.16
  svg circle:focus-visible { outline: none;                       // M4.16
                              stroke: #1976d2;
                              stroke-width: 4; }
  svg text:focus { outline: none; }                               // M4.16
  svg text:focus-visible { outline: 2px solid #1976d2;            // M4.16
                            outline-offset: 2px; }
  </style>
</head>
<body>
  <svg ...>...</svg>            // same body as provinces.svg
                                // (M4.15+M4.17 tabindex/role/
                                // aria-label included), no XML prolog
  <div id="details" class="details">                              // M4.10
    <p class="details-empty">Click a province to see its details.</p>
  </div>
  <ul class="legend">
    for each CountryState c in state.countries (vector order):
      <li data-owner="<i>">
        <svg class="swatch" viewBox="0 0 16 16" width="16" height="16">
          <circle cx="8" cy="8" r="8"
                  fill="<color_for_owner(CountryId{i})>"/>
        </svg>
        <xml_text_escape(c.id_code)> &mdash;
        <xml_text_escape(c.name)>
      </li>
  </ul>
  <script>                                                        // M4.10
  (function() {
    var details = document.getElementById("details");
    if (!details) { return; }
    var fields = [                                                // M4.11+M4.13
      { attr: "data-id",         label: "Province ID"   },
      { attr: "data-owner",      label: "Owner Index"   },
      { attr: "data-owner-code", label: "Owner Code"    },
      { attr: "data-owner-name", label: "Owner Name"    },         // M4.13
      { attr: "data-name",       label: "Province Name" }
    ];
    var nodes = document.querySelectorAll(
      "svg circle[data-id], svg text[data-id]");                  // M4.10
    function showDetails(el) {                                    // M4.10+M4.11
      // clear existing dl, build fresh <dl><dt>label</dt>
      // <dd>el.getAttribute(attr)</dd></dl> via createElement +
      // textContent — XSS-safe by construction.
    }
    function selectProvince(el) {                                 // M4.12
      // querySelectorAll(".selected") + classList.remove on all;
      // then for each n in nodes: if n.getAttribute("data-id") ===
      //   el.getAttribute("data-id") then n.classList.add("selected").
    }
    for (var j = 0; j < nodes.length; j++) {                      // M4.10+M4.15
      (function(el) {
        function activate() { selectProvince(el); showDetails(el); }
        el.addEventListener("click", activate);
        el.addEventListener("keydown", function(ev) {             // M4.15
          if (ev.key === "Enter" || ev.key === " ") {
            ev.preventDefault();
            activate();
          }
        });
      })(nodes[j]);
    }
  })();
  </script>
</body>
</html>
```

where:

- `<style>` block now carries **17 selectors** (M4.6 three
  + M4.7 three + M4.10 six + M4.12 two + M4.16 four; was
  13 at M4.14). All are layout / state rules — no
  animations, no transitions, no media queries, no
  `@import`, no `@font-face`.
- The inline SVG body is byte-identical to
  `provinces.svg` minus the XML prolog (M4.15 / M4.17
  attributes are included in the shared body, so both
  files carry them).
- The `<div id="details">` placeholder appears
  **between** the SVG and the legend (M4.10).
- The `<ul class="legend">` appears next. One
  `<li data-owner="N">` per country in vector order;
  empty `state.countries` produces an empty `<ul>`.
- Per-row swatch is inline SVG (not an HTML element with
  `background-color`) so no element ever carries inline
  `style="..."`. Legend swatch `<circle>` elements
  deliberately carry **no** `tabindex` / `role` /
  `aria-label` — they stay decorative (M4.15 / M4.17
  intent).
- The inline `<script>` IIFE sits at the **end of
  `<body>`**, after the legend. Exactly **one** script
  (no `src=`, no `type=`). The per-element listener
  loop wires both `click` and `keydown` via a shared
  `activate()` closure so the two input modalities
  cannot drift in effect.
- Selection state lives entirely on the DOM (`class=
  "selected"`); never persisted across reloads.
- Focus state lives entirely in the browser; the M4.16
  `:focus-visible` rules make keyboard focus visible
  without affecting mouse-click focus.

## 5. The DOM identity + interactivity contract (load-bearing for future UI)

```text
Province identity (M4.8 + M4.13):
  - data-id          = ProvinceNode::id_code
  - data-owner       = owner.value() (raw int as string)
  - data-owner-code  = state.countries[owner].id_code (or "")
  - data-owner-name  = state.countries[owner].name     (or "")    // M4.13
  - data-name        = ProvinceNode::name

  Present on BOTH <circle> AND <text> for every node.
  XML-attribute-escaped at every site.
  data-owner-code and data-owner-name share a single
  bounds check (they cannot disagree about validity).

Country identity (legend, map.html only):
  - <li data-owner="N">  where N == vector index in state.countries
  - <li> body text       contains the country's id_code

Coordinate space:
  - viewBox 0 0 1000 1000
  - cx = node.x * 1000   (std::fixed setprecision(2))
  - cy = node.y * 1000
  - text x = cx
  - text y = cy + 22.0   (kLabelYOffset)

Owner colour:
  - <circle fill> matches kOwnerPalette[owner.value() %
    kOwnerPaletteSize] for valid owners, kOwnerFallbackFill
    (#888888) for invalid.
  - Legend swatches use the same lookup.

Interactivity surface (map.html only, M4.10–M4.16):
  - <div id="details">  the read-only panel the click +
                        keydown handler repaints on each
                        activation
  - .selected           transient class added to the
                        clicked / activated province's
                        <circle> AND <text> (pair via
                        shared data-id); cleared on every
                        activation
  - selectProvince(el)  helper that walks the pre-
                        collected nodes NodeList — does
                        NOT build CSS selectors at runtime
                        (no attribute-value escape risk)
  - showDetails(el)     helper that reads each fields[i]
                        .attr via getAttribute and writes
                        fields[i].label + the value into
                        the panel via createElement +
                        textContent (XSS-safe)
  - activate()          per-element closure shared between
                        click and keydown so the effect
                        cannot drift between input modes  // M4.15
  - fields array        five entries since M4.13

Accessibility surface (M4.15–M4.17):
  - tabindex="0"        on every <circle> + <text>; legend
                        swatch <circle>s stay tabindex-free
  - keydown listener    Enter or " " (Space) on a focused
                        marker calls activate(); Space gets
                        event.preventDefault() to suppress
                        page-scroll
  - :focus-visible CSS  blue (#1976d2) ring/outline shown
                        for keyboard-driven focus only;
                        mouse clicks paint only the M4.12
                        black .selected stroke (no ring)
  - role="button"       on every <circle> + <text> (matches
                        the click + Enter/Space activation
                        model exactly)
  - aria-label          "<name>, <owner_name>" when owner
                        resolves; "<name>" alone when owner
                        is invalid (no trailing comma).
                        Whole composed string XML-escaped
                        as a single value.
  - Legend swatches     carry NO tabindex / role / aria-label
                        — they stay decorative
```

## 6. Invariants future M4.x sub-milestones must preserve

```text
both artefacts stay deterministic (same state → same bytes)
both artefacts stay valid (parseable SVG / HTML5)
no removal or rename of an existing data-* attribute
   (additive growth only — M4.13 added data-owner-name;
    the four M4.8 keys data-id / data-owner / data-owner-code
    / data-name are NOT renamed)
provinces.svg and map.html stay in sync on the SVG body
   (render_svg_root is the single source of truth)
the per-element no-inline-style invariant continues to hold
   (svg fill via presentation attribute is fine;
    inline style="..." on individual elements is not)
the legend keeps 1:1 correspondence with state.countries
the artefact set stays at 10 (any 11th needs its own
   sub-milestone with the cadence / determinism /
   pre-end_tick contracts documented)
save format stays v12 (any new persistent state bumps)
   — data-owner-name (M4.13) and aria-label (M4.17) are
   DERIVED from existing fields at render time, not new
   fields on ProvinceNode, so they did NOT bump the schema
RunnerOptions::provinces_svg_path / RunnerOptions::map_html_path
   stay opt-in path overrides; no CLI flag is wired
provinces.svg stays fully inert wrt scripts (no <script>,
   no <style>, no font-family) — the M4.10 asymmetric
   script invariant. SVG-level interactivity attributes
   (tabindex, role, aria-label) DO appear in
   provinces.svg via the shared SVG body; some standalone
   SVG consumers may ignore them, which is the consumer's
   call, not the renderer's.
map.html carries EXACTLY ONE inline <script> (no src=, no
   type=, no second script) — also M4.10
the click + keydown handlers stay XSS-safe (createElement +
   textContent only; no innerHTML / outerHTML /
   document.write / eval / Function)
the handlers stay read-only (no fetch / XMLHttpRequest
   / localStorage / sessionStorage / cookie / history.pushState
   / window.location / navigator)
selection state stays purely DOM-level (no className
   string concatenation; no setAttribute("class", ...);
   no persistence across reloads)
the click + keydown listeners share a per-element
   activate() closure so input modes cannot drift  (M4.15)
the :focus-visible rules stay (NOT bare :focus) so the
   keyboard-focus ring does not collide with the M4.12
   .selected stroke on mouse clicks  (M4.16)
the ARIA surface stays narrow — role="button" and
   aria-label only on <circle>+<text>; legend swatches
   stay decorative. Broader ARIA (aria-selected /
   aria-current / aria-pressed / aria-live /
   aria-describedby / aria-labelledby) is still deferred
   to a dedicated future A11Y sub-milestone  (M4.17)
no <link>, no inline event attributes (onclick= / onkeydown=
   / ...) on either artefact
no <meta name="viewport">, no CSS animations / transitions
   / media queries / @import / @font-face on either artefact
```

## 7. Deferred items

These are intentionally not in M4 yet. M4.18 lists them so a
later sub-milestone has one canonical reference for what is
explicitly out of scope.

```text
HOVER + TOOLTIPS
  hover state (no CSS :hover rules; no JS mouseover)
  tooltips (the M4.10 details panel is click/keydown-only)

BROADER ARIA (the narrower ARIA surface — role=button +
              aria-label — shipped at M4.17)
  aria-selected on the M4.12 .selected markers
  aria-current on the focused marker
  aria-pressed if a future selection model wants
    toggle-button semantics
  aria-live region on the details panel for click-update
    announcements
  aria-describedby / aria-labelledby for indirection
  any role other than "button" on the markers
  <title> / <desc> SVG-native tooltip child elements

KEYBOARD POLISH (the focus + Enter/Space surface shipped
                 at M4.15+M4.16; broader keyboard still
                 deferred)
  arrow-key navigation between markers
  Escape to clear the details panel
  Tab navigation within the details panel content
  Keyboard shortcut to focus the panel
  skip-link / landmark navigation

PERSISTENT SELECTION
  selection persistence across reloads (the M4.12
    .selected class is transient; no localStorage /
    sessionStorage / cookie / URL fragment)
  multi-select / shift-click / right-click / context menu
  selection-driven URL fragment (read on load, not write)

DOM EXTENSIONS
  neighbour adjacency edges (SVG <line> / <polyline>)
  terrain / resources / population overlays
  SVG-side controller-vs-owner distinction
  richer node fields on ProvinceNode (population, terrain,
    garrison, ...)
  multi-province countries with explicit grouping
  unowned provinces (current contract assumes owner
    resolves to a valid CountryId; empty data-owner-code /
    data-owner-name / aria-label-without-comma is a
    defensive fallback for hand-built bad states, not a
    modelled "neutral" status)

VISUAL POLISH
  <meta name="viewport"> + media queries for responsive sizing
  CSS animations / transitions on the .selected highlight
    or the M4.16 focus ring
  font-family / font-size on the SVG <text> elements themselves
    (M4.4 contract preserved; only CSS selectors set fonts)

INFRASTRUCTURE
  atomic end_tick writes (deferred since M2.9)
  runner CLI flag for either artefact
  M4 close-out / exit report
```

## 8. Integration test coverage at the M4.18 checkpoint

`tests/integration/m4_dom_contract_test.cpp` (6 cases):

1. **Uniform identity surface across both artefacts.** For
   every canonical province (berlin / paris / tokyo), each
   of the **five** `data-*` attribute substrings appears at
   least twice (once on `<circle>`, once on `<text>`) in
   both `provinces.svg` and `map.html`. M4.13 grew this
   from four attrs to five.
2. **Legend rows correspond 1:1 with `state.countries`.**
   `map.html` carries one `<li data-owner="N">` per
   canonical country, and each row's body contains the
   country's `id_code`. `provinces.svg` has no legend.
3. **Asymmetric no-stray-interactivity invariant.**
   `provinces.svg` stays fully inert wrt scripts (no
   `<script>`, no `<style>`, no `font-family`).
   `map.html` carries EXACTLY ONE inline `<script>` block
   (no `src=`, no `type=`) — the M4.10 click+keydown
   handler. Both artefacts still have NO `<link>`, NO
   inline event attributes, and NO per-element
   `style="..."`. (Split into the asymmetric per-artefact
   shape at M4.10.)
4. **(M4.14) Click-handler fields contract.** The
   canonical `map.html` script carries exactly the
   M4.13-era five-entry fields list — each of the five
   attribute names and each of the five labels appears
   verbatim inside the inline script. Catches accidental
   shrinkage back to the four-entry form or label drift.
5. **(M4.15) Keyboard focus end-to-end.** Every canonical
   province `<circle>` + `<text>` carries `tabindex="0"`
   in BOTH artefacts (6 occurrences in each); the legend
   swatch `<circle>` is NOT focusable; map.html wires the
   `keydown` listener with Enter / Space / preventDefault.
6. **(M4.18) Accessibility surface end-to-end.** New in
   M4.18: every canonical province `<circle>` + `<text>`
   carries `role="button"` + `aria-label` with the
   canonical composed value (e.g. "Berlin, Germany",
   "Paris, France", "Tokyo, Japan") in BOTH
   `provinces.svg` and `map.html`; the legend swatch
   carries neither; the M4.16 `:focus-visible` rules
   appear in the `map.html` `<style>` block but NOT in
   `provinces.svg`.

Per-element / per-rule pinning lives in
`tests/systems/svg_export_test.cpp` and the M4.x sections
of `tests/systems/runner_test.cpp`; the integration tests
above are the end-to-end check against the canonical
fixture.

## 9. What M4.18 does NOT do

```text
no new system
no new formula
no new artefact (still 10)
no save schema bump (still v12)
no new state field
no new InterestGroupKind / PlayerCommandKind
no new feature surface (M4.18 is docs + 1 integration test only)
no rename of any data-* attribute
no change to the SVG body / click handler / details panel
   / .selected CSS / focus-visible CSS / role / aria-label
   bytes (M4.15–M4.17 shapes byte-identical)
no <meta name="viewport">
no CSS animations / transitions / media queries / @import
no neighbour / adjacency edges
no terrain / resources / population overlays
no events / AI / command integration
no hover state / tooltip
no broader ARIA (aria-selected / aria-current /
   aria-pressed / aria-live / aria-describedby /
   aria-labelledby) — still deferred
no keyboard polish beyond M4.15 (no arrow-key nav,
   no Escape-to-clear, no Tab-within-panel)
no selection persistence across reloads
no runner CLI flag
no atomic end_tick writes
no M4 close-out
no docs/milestone-4-result.md
no "M4 closed" wording
no change to provinces.svg or map.html bytes
   (M4.18 only adds tests + docs; the renderer is
   byte-identical with M4.17)
```

# Milestone 4 Result

**Status: closed.**

M4 set out to give Project Leviathan its first
player-visible surface: a typed map-node data layer, a
deterministic SVG renderer feeding a browser-friendly HTML
viewer, an interactivity layer with click / keyboard /
hover affordances, an accessibility surface, and a small
responsive polish — all shipped as an additive sequence
of skeleton sub-milestones, none of which touches the
gameplay simulation under it. Twenty-three sub-milestones
delivered that.

## 1. What M4 shipped

| Sub-milestone | Title | Highlights |
|---------------|-------|------------|
| **M4.1** | ProvinceNode data layer | New `core::ProvinceNode { id_code, name, owner, x, y }` replacing the M0 stub. `GameState::provinces` becomes a typed vector; loaded via a new scenario-loader `provinces` array. Save format v11 → v12 (provinces array required at save layer). Canonical fixture `data/provinces/1930_core_nodes.json` ships GER/FRA/JPN nodes. Data-only; no system reads. |
| **M4.2** | SVG exporter skeleton | First renderer. New `leviathan::systems::svg_export` module: `render_provinces(state) → string` + `write_provinces(state, path)`. `end_tick` writes `provinces.svg` UNCONDITIONALLY as the **9th artefact**. Deterministic byte-stable output (LF, `setprecision(2)`, insertion order). `RunnerOptions::provinces_svg_path` optional override. XML attribute escaping caught + fixed in review. |
| **M4.3** | SVG owner-color skeleton | Per-owner palette. New `kOwnerPalette` (10-entry `constexpr std::array<string_view, 10>`), `kOwnerPaletteSize`, `kOwnerFallbackFill = "#888888"`, `color_for_owner(CountryId)`. Modulo wrap; negative-owner fallback. GER/FRA/JPN map to slots 0/1/2 (steel blue / indian red / goldenrod). Save format unchanged. |
| **M4.4** | SVG labels skeleton | One `<text>` per `<circle>`. New `kLabelYOffset = 22.0`. New `xml_text_escape` (anonymous namespace; `& < >` only — distinct from M4.2 `xml_attr_escape`). Interleaved per-node emission preserved. No `font-family` / `font-size` / `fill` on `<text>` themselves (typography deferred). |
| **M4.5** | HTML viewer skeleton | Wraps the SVG body in a minimal HTML5 document. New `render_map_html(state) → string` + `write_map_html(state, path)`. Internal `render_svg_root` helper shared by both paths so `render_provinces` stays byte-identical. `map.html` written UNCONDITIONALLY as the **10th artefact**. `RunnerOptions::map_html_path` optional override. No CSS, no `<script>`, no `<link>` at this stage. |
| **M4.6** | HTML viewer minimal CSS skeleton | Smallest possible `<style>` block: three selectors (`body` page bg + padding; `svg` centred white card with grey border; `svg text` sans-serif). `provinces.svg` byte-output UNCHANGED — CSS lives in the HTML wrapper only. M4.5 "no JS / no `<script>` / no `<link>`" preserved. |
| **M4.7** | HTML legend skeleton | Static `<ul class="legend">` after the inline SVG. One `<li data-owner="N">` per `state.countries[i]` with a tiny 16×16 inline SVG swatch (`fill="..."` from `color_for_owner`) + `"<id_code> &mdash; <name>"`. Inline-SVG swatch (not HTML `background-color`) preserves the "no inline `style="..."` per element" rule. `provinces.svg` byte-output UNCHANGED. |
| **M4.8** | Widened identity surface (4 data-* attrs) | Every `<circle>` and every `<text>` carries the same four `data-*` attrs: `data-id`, `data-owner`, `data-owner-code`, `data-name`. Uniform identity so a future clickable UI can address either element without DOM-walking siblings. First M4.x sub-milestone since M4.4 that deliberately edits the standalone SVG body — additive only. |
| **M4.9** | DOM contract checkpoint | Three integration tests in `tests/integration/m4_dom_contract_test.cpp` (uniform identity surface; legend rows 1:1 with `state.countries`; no-interactivity invariant). New `docs/milestone-4-checkpoint.md` snapshot. Zero new behaviour. **M4 stays in progress** — pattern mirrors M3.7. |
| **M4.10** | HTML clickable UI skeleton (first JavaScript) | Single inline `<script>` IIFE at end of `<body>`. One `click` listener per `svg circle[data-id], svg text[data-id]`. New `<div id="details">` panel populated via `createElement` + `textContent` (XSS-safe — no `innerHTML` / `outerHTML` / `eval`). No `fetch` / `XMLHttpRequest` / `localStorage` / `sessionStorage` / `history.pushState` / `window.location` / `navigator`. **Asymmetric JS boundary established**: `provinces.svg` stays fully script-free; `map.html` carries exactly ONE inline `<script>` (no `src=`, no `type=`). |
| **M4.11** | Details panel labels polish | `<dt>` labels decoupled from raw `data-*` attribute names. `var keys = [...]` → `var fields = [{attr, label}, ...]`. Renders `Province ID` / `Owner Index` / `Owner Code` / `Province Name`. `getAttribute` still uses raw M4.8 keys — DOM contract unchanged; only the labels rendered into `<dt>` cells change. |
| **M4.12** | Transient selection skeleton | Two new CSS rules (`svg circle.selected { stroke: #000000; stroke-width: 3; }` + `svg text.selected { font-weight: bold; }`). New `selectProvince(el)` helper called from the click listener: clears all `.selected` via `classList.remove` then re-adds to every node sharing the clicked element's `data-id`. **Selection is purely DOM-level** — never written into `GameState`, never persisted. |
| **M4.13** | Identity surface widened to 5 attrs | Fifth `data-owner-name` attribute on every `<circle>` and `<text>`, resolved from `state.countries[owner.value()].name` (or `""` when invalid — same defensive fallback as `data-owner-code`). Shared single bounds check. XML-attribute-escaped. M4.11 fields array grows 4 → 5 entries with new `Owner Name` row. **Save format STAYS v12** — derived from `state.countries`, not a new `ProvinceNode` field. |
| **M4.14** | DOM contract checkpoint refresh | Refreshes the M4.9 checkpoint to cover M4.10–M4.13 (first JS + asymmetric one-script invariant; decoupled labels; transient `.selected`; fifth `data-owner-name` attr). Adds one new integration assertion (test D) pinning the canonical `map.html` script's 5-entry fields list end-to-end. Zero new behaviour. |
| **M4.15** | Keyboard focus accessibility skeleton | `tabindex="0"` on every `<circle>` and `<text>` (rendered in `render_svg_root`, so `provinces.svg` picks it up too). Inline `<script>` adds a `keydown` listener alongside `click` so Enter / Space activate the same `selectProvince + showDetails` pair (with `event.preventDefault()` on Space). Click + keydown share a per-element `activate()` closure. Legend swatch `<circle>`s lack `data-id` so stay non-focusable. NO ARIA polish (deferred). |
| **M4.16** | Focus-visible styling skeleton | Makes M4.15's keyboard focus VISIBLE. Four new CSS rules: bare `:focus { outline: none; }` suppressors + `:focus-visible { stroke: #1976d2; stroke-width: 4; }` on circle + `outline: 2px solid #1976d2; outline-offset: 2px` on text. Uses `:focus-visible` (NOT bare `:focus`) so mouse clicks don't trip the ring — keeps M4.12 black `.selected` stroke visually distinct from M4.16 blue keyboard-focus indicator. |
| **M4.17** | ARIA labels skeleton | `role="button"` + `aria-label="<name>, <owner_name>"` on every `<circle>` and `<text>`. Composed at render time (full form for valid owner; `<name>` alone for invalid — same bounds check as `data-owner-code` / `data-owner-name`); XML-attribute-escaped as a single value. **Narrowly reverses the M4.15/M4.16 "no ARIA" non-goal**; broader ARIA (`aria-selected`, `aria-current`, `aria-pressed`, `aria-live`, `aria-describedby`, `aria-labelledby`) stays deferred. |
| **M4.18** | Accessibility checkpoint refresh | Refreshes the M4.14 checkpoint to cover M4.15–M4.17 (keyboard focus; focus-visible CSS; ARIA labels). Adds integration test F end-to-end on the canonical scenario (`role="button"` count; composed `aria-label` matches; legend swatches stay decorative; `:focus-visible` CSS in `map.html` but NOT in `provinces.svg`; still-deferred ARIA absent). Zero new behaviour. |
| **M4.19** | Hover affordance skeleton | Two `:hover` CSS rules: `svg circle:hover { stroke: #666666; stroke-width: 2; }` + `svg text:hover { text-decoration: underline; }`. Placed BEFORE `.selected` / `:focus-visible` in CSS source order so state rules override hover when both apply. Three state colours stay separable (hover grey / selected black / focused blue). Pure CSS — no JS hover handler, no tooltip, no SVG `<title>` child. |
| **M4.20** | Hover tooltip skeleton | New `<p id="hover-status">` text bar between SVG and details panel. Inline `<script>` registers `mouseover` + `mouseout` listeners alongside click + keydown; mouseover composes `"<name> (<owner-name>)"` via `getAttribute` on M4.8/M4.13 attrs and writes via `textContent`. **Hover and click are independent surfaces** — mouseover handler never touches the details panel / `.selected`. No SVG `<title>` child (would compete with M4.17 `aria-label`). |
| **M4.21** | Responsive viewport skeleton | `<meta name="viewport" content="width=device-width, initial-scale=1">` in `<head>` + one `@media (max-width: 1040px) { svg { width: 100%; height: auto; } }` rule. The 1040px threshold = 1000 (SVG width) + 2*20 (body padding). Above 1040px the M4.6 desktop rule (`margin: 0 auto`) wins; at and below the @media rule scales the SVG to fit the column (the existing `viewBox` preserves the aspect ratio). Pure CSS — no JS responsive surface. |
| **M4.22** | Close-out readiness checkpoint | Refreshes `docs/milestone-4-checkpoint.md` with a new section 9 "Close-out readiness assessment" categorising remaining deferred items (A defer-to-M5+ gameplay-domain; B post-M4 follow-up viewer polish; C not-needed-for-close nice-to-haves). New integration test G "M4 viewer contract complete" touches every M4.x surface marker on the canonical scenario in one runner-driven check. Fixes the PR #87 reviewer flag about the 1040px math wording (only the doc text drifted; implementation was always correct). Zero new behaviour. **Verdict**: M4 is structurally ready for close-out. |
| **M4.23** | M4 exit / close-out | This sub-milestone. New `docs/milestone-4-result.md` (this file). READMEs flipped to "M4 closed". `docs/milestone-4-checkpoint.md` annotated as historical. No code, no formula, no fixture, no test change. **M4 closes here.** |

## 2. Final M4 dataflow

```text
GameState::provinces (typed M4.1 ProvinceNode vector)
state.countries (existing M1 vector)
   -> render_svg_root(state) -> shared SVG body
        cx / cy / r / fill                              (M4.2 + M4.3)
        <text> labels                                   (M4.4)
        5 data-* attrs on <circle> AND <text>           (M4.8 + M4.13)
        tabindex="0" on <circle> AND <text>             (M4.15)
        role="button" + aria-label on <circle> AND <text> (M4.17)

shared SVG body
   -> render_provinces(state)
   -> provinces.svg                                     (M4.2 unconditional;
                                                         9th artefact)

shared SVG body
   -> render_map_html(state)
        <head>: <meta charset> + <meta viewport>        (M4.21)
                + <title> + <style> block:
                  body + svg + svg text                 (M4.6)
                  .legend + .legend li + .swatch        (M4.7)
                  .details + .details-empty + cursor    (M4.10)
                  .hover-status                         (M4.20)
                  svg circle[data-id]:hover             (M4.19)
                  svg circle.selected / text.selected   (M4.12)
                  svg circle:focus + :focus-visible     (M4.16)
                  svg text:focus + :focus-visible       (M4.16)
                  @media (max-width: 1040px) { svg ... }  (M4.21)
        <body>:
          inline <svg>...</svg> (from render_svg_root)
          <p id="hover-status">                         (M4.20)
          <div id="details">                            (M4.10)
          <ul class="legend">                           (M4.7)
          <script> IIFE (the ONLY inline script):       (M4.10–M4.20)
            click + keydown + mouseover + mouseout
            via a shared per-element activate() closure
            for click / keydown; separate handlers for
            mouseover / mouseout (hover-status only,
            never touching the details panel)
   -> map.html                                          (M4.5 unconditional;
                                                         10th artefact)
```

The two artefacts share the SVG body byte-for-byte; the
HTML wrapper carries everything else. Asymmetric JS
boundary (M4.10) holds: `provinces.svg` stays fully
script-free / style-free; `map.html` carries EXACTLY ONE
inline `<script>` (no `src=`, no `type=`).

## 3. Final artefact contract

M4 closes with ten runner artefacts:

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

`end_tick` writes the ten files **sequentially, not
transactionally** — a mid-`end_tick` I/O failure can leave
a partial set on disk. The pre-`end_tick` no-artefact
contract from M2.9 (failures before `end_tick` is reached
write zero files) continues to hold for the ninth and tenth
files automatically because `end_tick` is still the only
function that touches disk. Atomic-end_tick remains a
deferred item.

## 4. Save schema

```text
save format remains v12
```

M4 bumped the schema exactly once (v11 → v12 at M4.1, to
make the `provinces` block a required root-level array
with the typed `ProvinceNode` shape). Every subsequent M4
sub-milestone (M4.2 – M4.22) was deliberately
schema-neutral; M4.23 is doc-only. v12 is the new floor;
the next persistent-state addition will bump it under the
M0.8 strict-equality + version-history rule. Note that
M4.13's `data-owner-name` and M4.17's `aria-label` are
derived from `state.countries[owner].name` at render time
— neither added a new field to `ProvinceNode`, so neither
required a schema bump.

## 5. Architectural invariants every future milestone must preserve

These are the rules M4 added on top of the M0 / M1 / M2 /
M3 invariants. Future milestones must not silently break
them.

- **`render_svg_root` is the single source of truth for
  the SVG body.** Both `provinces.svg` and `map.html`
  emit the same byte stream there. Any new SVG-body
  attribute must go through `render_svg_root` so both
  artefacts pick it up uniformly.
- **The asymmetric JS boundary holds.** `provinces.svg`
  stays fully inert: no `<script>`, no `<style>`, no
  `font-family`. `map.html` carries EXACTLY ONE inline
  `<script>` (no `src=`, no `type=`, no second script).
  Any future viewer interactivity goes into that same
  inline IIFE.
- **The XSS-safe DOM API contract.** The inline `<script>`
  reads via `getAttribute` and writes via
  `createElement` + `textContent` + `classList`. It does
  NOT use `innerHTML` / `outerHTML` / `document.write` /
  `eval` / `Function` / `insertAdjacentHTML` /
  `className` string concatenation / `setAttribute("class", ...)`.
- **The no-storage / no-network / no-navigation contract.**
  The viewer script does NOT call `fetch` /
  `XMLHttpRequest` / `localStorage` / `sessionStorage` /
  `document.cookie` / `history.pushState` /
  `history.replaceState` / `window.location` /
  `navigator.*`. Selection / hover / focus are purely
  DOM-level (lost on reload).
- **The 5-attribute identity surface.** Every `<circle>`
  and every `<text>` carries `data-id`, `data-owner`,
  `data-owner-code`, `data-owner-name`, `data-name`
  identically. `data-owner-code` and `data-owner-name`
  share a single bounds check so they cannot disagree
  about validity. All five values are
  XML-attribute-escaped via the M4.2 helper. Growth is
  additive only — the M4.8 four keys + the M4.13 fifth
  key are NOT renamed.
- **The accessibility surface.** Every `<circle>` and
  every `<text>` carries `tabindex="0"` + `role="button"`
  + `aria-label`. Legend swatch `<circle>`s deliberately
  carry none of these (they're decorative — emitted in
  `render_map_html`, not `render_svg_root`). The
  `aria-label` value is the composed `"<name>, <owner_name>"`
  (or just `<name>` when invalid).
- **The interactivity surface lives in one inline IIFE.**
  Click + keydown share a per-element `activate()`
  closure so input modalities cannot drift. Mouseover +
  mouseout target the hover-status bar only; they do NOT
  touch the M4.10 details panel or the M4.12 `.selected`
  class. Click and hover are independent surfaces.
- **The transient-state contract.** `.selected` (M4.12)
  and keyboard focus (M4.15/M4.16) and hover (M4.19/M4.20)
  are purely DOM-level — not persisted across reload, not
  written into `GameState`, not surfaced in any save /
  scenario / runner artefact.
- **CSS source order matters for state rules.** `:hover`
  (M4.19) sits BEFORE `.selected` (M4.12) and
  `:focus-visible` (M4.16) in source order so the equal-
  specificity state rules override hover when both apply.
- **`:focus-visible`, NOT bare `:focus`, for the
  keyboard-focus ring** (M4.16) — keeps mouse-click
  focus from collide with the M4.12 `.selected` stroke.
  Two paired bare-`:focus { outline: none; }` rules
  neutralise the browser default outline.
- **No SVG `<title>` child element on the markers** —
  would compete with the M4.17 `aria-label` as the
  accessible name. The HTML `<head>` `<title>Leviathan
  Map</title>` is preserved as the only `<title>` in
  the document.
- **The responsive contract is narrow.** Exactly ONE
  `<meta name="viewport">` and ONE `@media` block ship
  (M4.21). Broader responsive (mobile-only layouts,
  breakpoint cascade, container queries, `prefers-*`
  features, responsive font sizing, JS responsive
  surface — `matchMedia` / `ResizeObserver`) stays
  deferred.
- **Byte-identical determinism.** Two runs with the
  same options produce byte-identical save.json /
  events.jsonl / summary.csv / countries.csv /
  factions.csv / interest_groups.csv / the two M3.6
  trace CSVs / provinces.svg / map.html. The M1.17 /
  M2.22 / M3.7 byte-identical determinism contracts
  extend through all M4 artefacts by construction
  (`render_svg_root` is pure and total; `render_map_html`
  composes deterministic strings).
- **The artefact set is ten files.** Adding an eleventh
  requires its own sub-milestone with the cadence /
  determinism / pre-`end_tick` contracts documented
  alongside (per `milestone-3-result.md` §5 and the
  M4.2 / M4.5 precedents).
- **Save format v12 is the new floor.** Future
  persistent state bumps it under M0.8's strict-equality
  rule. M4.13's `data-owner-name` and M4.17's
  `aria-label` are derived (not stored), so they did
  NOT bump the schema — the same approach should be
  preferred for any future render-time-derived viewer
  surface.

## 6. Deferred items

These are intentionally not in M4. They are listed so a
future sub-milestone or post-M4 milestone has one
canonical reference for what M4 explicitly did not ship.

### Category A — defer to M5+ (gameplay-domain)

- Neighbour adjacency edges (SVG `<line>` / `<polyline>`).
- Terrain / resources / population overlays.
- Ownership-dynamics layer (provinces are static at M4
  close; the renderer reads `owner` and never writes it).
- SVG-side controller-vs-owner distinction.
- Multi-province countries with explicit grouping.
- Unowned provinces (the M4 contract assumes
  `owner.value()` resolves into `state.countries`;
  empty `data-owner-code` / `data-owner-name` is a
  defensive fallback for hand-built bad states, not a
  modelled "neutral" status).
- Event triggers / AI / command integration with the
  viewer.
- Runner CLI flag for either artefact (only
  `RunnerOptions::provinces_svg_path` /
  `RunnerOptions::map_html_path` programmatic
  overrides exist).
- Atomic `end_tick` writes (temp-file + rename) —
  deferred since M2.9.

### Category B — recommended post-M4 viewer-polish follow-up

- Broader ARIA: `aria-selected` on the M4.12
  `.selected` markers; `aria-current` on the focused
  marker; `aria-pressed` if a future selection model
  wants toggle-button semantics; `aria-live` region on
  the details panel / hover-status bar for click-update
  announcements; `aria-describedby` / `aria-labelledby`
  for indirection.
- Pair-hover (hovering a circle also highlights its
  sibling text via JS `mouseover` / `mouseout` +
  `classList` toggling on the sibling sharing `data-id`).
- Position-aware floating tooltip near cursor (the
  M4.20 hover-status bar is fixed-position above the
  details panel, not cursor-following).
- Selection persistence across reload (URL fragment
  read on load, NOT write; no `localStorage`).
- Keyboard polish beyond M4.15: arrow-key navigation
  between markers; Escape-to-clear the details panel;
  Tab-within-panel; keyboard shortcut to focus the
  panel; skip-link / landmark navigation.
- Mobile-only layout rules beyond the M4.21 SVG-scale
  rule (breakpoint cascade; dedicated mobile layouts
  for legend / details / hover-status).
- Dark-mode variant via `prefers-color-scheme`.
- `prefers-reduced-motion` respect (would matter once
  any animation / transition is added).
- CSS animations / transitions on `.selected` / focus /
  hover state changes.

### Category C — not needed for close (no concrete consumer)

- Container queries (`@container`).
- `@supports` feature queries.
- Responsive font sizing (`clamp()` / `vw` / `vh` units).
- Font-family / font-size on the SVG `<text>` elements
  themselves (M4.4 contract preserved; only CSS
  selectors set fonts).
- JS responsive surface (`matchMedia` / `ResizeObserver`
  / `window.innerWidth` reads / `"resize"` listener).
- Hover delay / hover-driven detail-panel preview.
- `mouseenter` / `mouseleave` instead of `mouseover` /
  `mouseout` (no behavioural advantage given the
  marker shape).

## 7. Recommended next milestone candidates

Next milestone direction should be chosen explicitly by
the reviewer.

Candidates:

- **RFC-090 M5** — event engine. Picks up the
  "interest-group thresholds trigger events" item the
  M3 result deferred, and is the natural neighbour to
  the M3 reaction loop on the gameplay side. The M4
  viewer has the DOM surface (data-* attrs +
  hover-status bar) to display future event-driven
  changes without further viewer work in M5 itself.
- **Post-M4 viewer-polish follow-up** *if* the
  reviewer wants to land one or more Category B items
  before moving to M5. Pair-hover, broader ARIA
  (`aria-selected` + `aria-live`), and selection
  persistence are the three most user-visible. Could
  be one PR each, or batched if scoped carefully.
- **Pick up gameplay-domain Category A items** —
  neighbour adjacency, terrain, ownership dynamics.
  These overlap with M5 / future RFC-090 milestones;
  scope them under the appropriate RFC-090 §M5 or
  §M6 numbering rather than as M4 follow-ups.

M4.23 deliberately does **not** open or claim any of
the above. No "M5 in progress" wording lands in this
PR; the next milestone starts when the reviewer says
so. The 2026-05-17 force-reset lesson (don't invent
milestone numbers; don't pre-open the next milestone
in a close-out PR) holds: M5 starts in its own
deliberate first sub-milestone PR.

**M4 closes here.**

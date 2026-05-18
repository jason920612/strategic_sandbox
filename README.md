# Project Leviathan

一款以 1930–2000 全球政治沙盒為背景的即時暫停制國家領導人模擬遊戲。玩家不直接微操軍隊，而是透過法律、行政命令、預算、任命、外交與戰略目標影響國家。國內派系、官僚、軍方、情報部門、地方利益集團、技術菁英、媒體、學生、宗教、工會、農民等會依照自身利益自動反應，讓世界局勢自然演化、混沌且不可完全預測。

> Code name: **Project Leviathan**. Final title TBD — see `rfc/RFC-000-overview.md`.

## Status

- Phase: **Milestone 4 — SVG map + UI (CLOSED, RFC-090
  §M4).** M0 / M1 / M2 / M3 / M4 all closed. M4 delivered
  in 23 sub-milestones (M4.1–M4.23) a deterministic
  10-artefact viewer stack consisting of: typed
  `ProvinceNode` data layer (save v11→v12 at M4.1); SVG
  renderer + `provinces.svg` as 9th artefact (M4.2); HTML
  viewer wrapper + `map.html` as 10th artefact (M4.5); a
  shared `render_svg_root` helper between the two; a
  five-attribute DOM identity surface on every `<circle>`
  + `<text>` (M4.8 + M4.13); a single-inline-`<script>`
  interactivity layer with click + Enter/Space + hover
  listeners (M4.10/M4.15/M4.20); a `<div id="details">`
  panel + `<p id="hover-status">` text bar; a transient
  `.selected` highlight (M4.12); CSS `:focus-visible` +
  `:hover` rings (M4.16/M4.19); a `tabindex="0"` +
  `role="button"` + composed `aria-label` accessibility
  surface (M4.15/M4.17); a `<meta name="viewport">` + one
  responsive `@media (max-width: 1040px)` rule (M4.21).
  Asymmetric JS boundary established at M4.10 holds end
  to end: `provinces.svg` stays fully inert (no
  `<script>` / `<style>` / `font-family`); `map.html`
  carries EXACTLY ONE inline `<script>` (no `src=`, no
  `type=`). The XSS-safe DOM API contract holds:
  `getAttribute` to read, `createElement` + `textContent`
  + `classList` to write; never `innerHTML` / `eval` /
  `fetch` / storage / navigation. Selection / hover /
  focus are purely DOM-level (lost on reload). See
  `docs/milestone-4-result.md` for the **M4 exit report**
  (full ledger, dataflow, contract, invariants,
  deferred items, neutral next-milestone candidates),
  `docs/milestone-3-result.md` / `docs/milestone-2-result.md`
  / `docs/milestone-1-result.md` for prior exit reports,
  and `docs/milestone-4-checkpoint.md` (now annotated
  as historical) for the in-progress snapshots that
  preceded the close-out.
- Latest shipped sub-milestone: **M4.23 — M4 exit /
  close-out.** Docs-only PR mirroring M1.17 / M2.22 /
  M3.9 in shape. Publishes
  `docs/milestone-4-result.md` (the M4 exit report —
  seven sections: M4.1–M4.23 ledger, final dataflow,
  10-artefact contract, save-format v12 floor,
  architectural invariants every future milestone must
  preserve, deferred items categorised A/B/C, neutral
  next-milestone candidates). Annotates
  `docs/milestone-4-checkpoint.md` as historical with a
  top-of-file pointer to the exit report. Flips this
  README + `docs/README.md` + `rfc/README.md` to "M4
  closed". No code, no formula, no fixture, no test
  change (892 doctest cases / 61742 assertions identical
  with M4.22). `provinces.svg` + `map.html` bytes
  byte-identical with M4.22. **M4 closes here.** **No
  M5 in progress** — M5 starts in its own deliberate
  first sub-milestone PR when the reviewer decides;
  M4.23 makes no claim about which milestone is next.
- Previously shipped: **M4.22 — close-out
  readiness checkpoint.** Mirrors M3.7's role for the M3
  reaction loop: docs + 1 integration test, no renderer
  behaviour change. After M4.21 the M4 viewer stack is
  structurally complete; M4.22 formally assesses
  readiness, locks the current contract with one
  consolidated end-to-end integration test (test G "M4
  viewer contract complete"), fixes a small math wording
  the PR #87 reviewer flagged, and leaves the M4 exit
  report (`docs/milestone-4-result.md`) for a deliberate
  follow-up PR once the reviewer gives the "do M4
  close-out" green light. **`docs/milestone-4-checkpoint.md`
  gains a new section 9 "Close-out readiness assessment"**
  with four subsections: (9.1) one-line summary of what
  M4 shipped — a deterministic 10-artefact viewer stack
  with 5-attr DOM identity surface, click+keydown+hover
  listeners, transient `.selected`/`:focus-visible`/`:hover`
  CSS rings, `role="button"` + `aria-label`,
  hover-status text bar, viewport meta + one responsive
  `@media` rule; (9.2) still-deferred items categorised:
  Category A defer-to-M5+ gameplay-domain (neighbour
  adjacency, terrain, ownership dynamics, event
  integration), Category B recommended post-M4 follow-up
  polish (broader ARIA — `aria-selected`/`aria-current`/
  `aria-pressed`/`aria-live`/`aria-describedby`, pair-hover,
  position-aware tooltip, selection persistence, keyboard
  polish, mobile-only layouts, dark-mode, CSS
  animations), Category C not-needed-for-close
  nice-to-haves (container queries, `@supports`,
  responsive font sizing, JS responsive surface, hover
  delay); (9.3) verdict — **M4 is structurally ready
  for close-out**; (9.4) the PR #87 wording fix. New
  integration test G touches every M4.x surface marker
  on the canonical scenario in one runner-driven check
  (viewBox, circle, text, 5 data-* attrs, tabindex, role,
  aria-label, details panel, legend, click+keydown+mouseover+mouseout
  listeners, hover-status, `.selected` CSS,
  `:focus-visible`, `:hover`, fields-array labels,
  viewport meta, `@media` block) AND pins that
  `provinces.svg` carries NONE of the HTML-wrapper
  surfaces AND pins all 7 unconditional artefacts present
  AND `save_version: 12`. **Math wording fix** per PR
  #87 reviewer flag: 1040px threshold = `1000 (SVG) + 2
  * 20 (body padding) = 1040` (the SVG's 1px border
  lives INSIDE the padded column, not adding to layout
  width); the implementation always used 1040 correctly,
  only the explanatory text in svg_export.cpp /
  svg_export.hpp / READMEs / m4-21 design note had
  drifted to `1000 + 20*2 + 1*2 = 1042`. Fixed across
  all sites; m4-21 design note got a short "Fixed in
  M4.22" acknowledgement. **M4 remains in progress at
  the end of this PR** — no `docs/milestone-4-result.md`,
  no "M4 closed" wording. The reviewer's next decision
  per the checkpoint section 9.6 is one of: (1) M4
  close-out (publish exit report + flip READMEs), (2)
  one more polish PR from category B, or (3) stop M4
  and move to M5. M4.22 does NOT pick. Artefact set
  unchanged (still 10). Save format unchanged (still
  v12). `provinces.svg` and `map.html` bytes BOTH
  byte-identical with M4.21 — the svg_export.cpp change
  is comment-only, not behavioural. **1 new doctest
  case (892 total, 61742 assertions; verified via direct
  `leviathan_tests.exe` run** per the
  `feedback_ctest_masks_doctest` rule). **No new system,
  no new formula, no new artefact, no new state field /
  fixture / `InterestGroupKind` / `PlayerCommandKind`,
  no save schema bump, no new feature surface (M4.22 is
  docs + 1 integration test + 1 math-wording fix), no
  rename of any data-* attribute, no renderer behaviour
  change, no broader ARIA / pair-hover / position-aware
  tooltip / keyboard polish / selection persistence /
  hover delay / dark-mode (all explicitly deferred per
  checkpoint section 9.2), no change to `provinces.svg`
  or `map.html` bytes, no M4 close-out, no
  `docs/milestone-4-result.md`, no "M4 closed" wording.**
- Previously shipped: **M4.21 — responsive
  viewport skeleton.** Makes `map.html` render usably on
  narrow / mobile screens. Two small additions: (a)
  `<meta name="viewport" content="width=device-width,
  initial-scale=1">` in `<head>` (mobile browsers lay
  the page out at device-width instead of the default
  desktop-emulated ~980px; `initial-scale=1` disables
  auto-zoom-out on first paint); (b) one `@media
  (max-width: 1040px)` rule in the `<style>` block that
  scales the SVG to `width: 100%; max-width: 100%;
  height: auto` so it fits the column without
  horizontal scroll (the existing `viewBox` preserves
  the aspect ratio for free). The 1040px threshold is
  `1000 (SVG width) + 20*2 (body padding) + 1*2
  (border)`. Above 1040px the original M4.6 `svg {
  ...; margin: 0 auto; ...}` rule centres the
  fixed-size SVG (desktop layout preserved); at and
  below 1040px the @media rule activates. Legend /
  details panel / hover-status bar all inherit the
  column width via their existing `max-width: 1000px;
  margin: 0 auto` — no per-element mobile rule needed.
  **Narrowly reverses the M4.5–M4.20 "no `<meta
  viewport>`, no media queries" non-goal** — only ONE
  viewport meta and ONE @media block ship. Broader
  responsive surface (mobile-only layouts, breakpoint
  cascade, container queries, `prefers-color-scheme` /
  `prefers-reduced-motion`, responsive font sizing,
  JS responsive surface — `matchMedia` /
  `ResizeObserver` / `window.innerWidth` / `"resize"`)
  all stay deferred. **Pure CSS** — no JS resize
  listener, no `matchMedia`, no `ResizeObserver`. No
  external CSS / font / `<link>`. M4.10/M4.11/M4.12/
  M4.13/M4.15/M4.16/M4.17/M4.19/M4.20 invariants all
  carry over unchanged (additive only). Per the
  `feedback_checkpoint_drift` rule,
  `docs/milestone-4-checkpoint.md` is refreshed
  **inline** in this PR (`<head>` shape adds viewport
  meta; `<style>` block adds @media rule; selector-
  count wording becomes "20 plain selectors plus one
  @media block"; VISUAL POLISH deferred bucket
  rewrites with M4.21 shipped; invariants section
  rewrites "no <meta viewport>, no media queries" rule
  to "exactly one viewport meta, exactly one @media
  block"). **M4 remains in progress** — no
  `docs/milestone-4-result.md`; M4.21 is one more
  skeleton sub-milestone, not an exit. Artefact set
  unchanged (still 10). Save format unchanged (still
  v12). `provinces.svg` bytes UNCHANGED from M4.20
  (viewport + @media live in `render_map_html` only);
  `map.html` bytes did change (one new meta + one new
  @media block). **5 new doctest cases (891 total,
  61678 assertions; verified via direct
  `leviathan_tests.exe` run** per the
  `feedback_ctest_masks_doctest` rule). **No second
  `<meta name="viewport">`, no second @media block, no
  `@container` / container queries, no `@supports`, no
  `min-width:` media queries (only `max-width`), no
  `orientation:` queries, no `prefers-color-scheme`,
  no `prefers-reduced-motion`, no responsive font
  sizing (`clamp()`, `vw` / `vh` units), no JS
  responsive surface, no mobile-only layout rules for
  legend / details panel / hover-status bar (they
  inherit the column via existing max-width +
  centring), no fluid font / font-size CSS, no CSS
  animations / transitions, no `@import` /
  `@font-face`, no `<link>` / external CSS / font, no
  inline event attributes, no per-element inline
  `style="..."`, no save schema bump, no new state
  field / artefact / fixture / `InterestGroupKind` /
  `PlayerCommandKind`, no rename of the M4.8 / M4.13
  data-* keys, no second `<script>`, no `<script
  src=>`, no `<script type=>`, no `fetch` / XHR /
  storage / history / navigation APIs, no `innerHTML`
  / `outerHTML` / `document.write` / `eval` /
  `Function` / `insertAdjacentHTML`, no broader ARIA
  (still deferred), no state mutation, no commands,
  no AI, no events, no selection persistence, no
  adjacency / terrain / overlays, no runner CLI flag,
  no change to `provinces.svg` bytes, no M4
  close-out, no `docs/milestone-4-result.md`, no "M4
  closed" wording.**
- Previously shipped: **M4.20 — hover tooltip
  skeleton.** Adds a small **hover-status text bar**
  (`<p id="hover-status" class="hover-status">`) between
  the inline SVG and the M4.10 details panel. The inline
  `<script>` registers `mouseover` + `mouseout`
  listeners on every province `<circle>` + `<text>`
  (same per-element loop as the M4.10 click + M4.15
  keydown listeners). On `mouseover` the bar's
  `textContent` is updated to `"<name> (<owner-name>)"`
  (or just `<name>` when owner-name is empty — same
  defensive fallback as the M4.17 aria-label); on
  `mouseout` it's cleared. Reads via `getAttribute` on
  the existing M4.8/M4.13 `data-name` + `data-owner-name`
  attributes (no new attribute, no schema growth);
  writes via `textContent` only (XSS-safe — the M4.10
  no-`innerHTML` discipline carries over). One new CSS
  rule `.hover-status` provides minimal styling
  (centred, italic muted text with `min-height: 1em` to
  prevent layout jump on first hover). **Tooltip
  design choice**: status-text bar chosen over SVG
  `<title>` child (which would compete with the M4.17
  `aria-label` as the accessible name — reviewer flagged
  this) and over a position-aware floating tooltip
  (more scope than a skeleton needs). The bar sits in
  document flow above the details panel, browser
  status-bar idiom. **Narrowly reverses the M4.19 "no
  JS hover handler" non-goal** — only `mouseover` +
  `mouseout` via `addEventListener` ship; `mouseenter`
  / `mouseleave` / `.onmouseover` / `.onmouseout` /
  inline `onmouseover=` / `onmouseout=` stay absent.
  Hover and click are **independent surfaces** — the
  M4.20 mouseover handler does NOT call `showDetails` /
  `selectProvince`, never touches the details panel,
  never mutates `.classList` on the hovered element.
  No SVG `<title>` child anywhere. No `innerHTML` /
  `outerHTML` / `document.write` / `eval` / `Function`
  / `insertAdjacentHTML`. No fetch / XHR / storage /
  history / navigation APIs. M4.10's XSS-safe DOM API,
  no-network discipline, asymmetric one-inline-script
  invariant, M4.12 `.selected`, M4.13 five-attr DOM
  contract, M4.15 `tabindex` + keydown handler, M4.16
  `:focus-visible` rings, M4.17 `role="button"` +
  `aria-label`, M4.19 `:hover` CSS all carry over
  unchanged (additive only). Per the
  `feedback_checkpoint_drift` rule,
  `docs/milestone-4-checkpoint.md` is refreshed
  **inline** in this PR (selector count 19 → 20;
  HTML body shape gains item 2 `<p id="hover-status">`;
  script section gains the M4.20 listener block;
  Interactivity surface section gains a hover-status
  bullet; HOVER+TOOLTIPS deferred bucket rewrites with
  M4.20 shipped). **M4 remains in progress** — no
  `docs/milestone-4-result.md`; M4.20 is one more
  skeleton sub-milestone, not an exit. Artefact set
  unchanged (still 10). Save format unchanged (still
  v12) — hover-status reads render-time attrs, no new
  persistent state. `provinces.svg` bytes UNCHANGED
  from M4.19 (hover-status is HTML-only); `map.html`
  bytes did change (new element + new CSS rule + new
  listener types in the script). **7 new doctest cases
  + 1 retuned (886 total, 61651 assertions; verified
  via direct `leviathan_tests.exe` run per
  `feedback_ctest_masks_doctest`).** **No SVG `<title>`
  child, no position-aware tooltip, no pair-hover, no
  `mouseenter`/`mouseleave`, no hover delay, no
  hover-driven panel preview, no `aria-live` on the
  bar, no animation / transition, no broader ARIA, no
  state mutation, no commands, no AI, no events
  emitted by hover, no selection persistence, no
  keyboard shortcut for the bar, no save schema bump,
  no new state field / artefact / fixture /
  `InterestGroupKind` / `PlayerCommandKind`, no rename
  of the M4.8 / M4.13 data-* keys, no second
  `<script>`, no `<script src=>` / `<script type=>`,
  no `<link>`, no external CSS / font / `<iframe>` /
  `<img>`, no `fetch` / XHR / storage / history /
  navigation APIs, no `innerHTML` / `outerHTML` /
  `document.write` / `eval` / `Function` /
  `insertAdjacentHTML`, no inline event attributes, no
  per-element inline `style="..."`, no `<meta
  name="viewport">`, no CSS animations / transitions
  / media queries / `@import` / `@font-face`, no
  neighbour / adjacency edges, no terrain / resources
  / population overlays, no runner CLI flag, no change
  to `provinces.svg` bytes, no M4 close-out, no
  `docs/milestone-4-result.md`, no "M4 closed"
  wording.**
- Previously shipped: **M4.19 — hover
  affordance skeleton.** Adds a mouse-hover visual cue
  so users can see "this marker reacts to me" before
  they click. Pure CSS — no JavaScript change, no
  markup change beyond the `<style>` block. Two new CSS
  rules in the M4.6 `<style>` block:
  `svg circle:hover { stroke: #666666; stroke-width: 2; }`
  + `svg text:hover { text-decoration: underline; }`.
  Placed AFTER the M4.10 `cursor: pointer` rule and
  BEFORE the M4.12 `.selected` / M4.16 `:focus-visible`
  rules so those state rules (equal specificity, later
  in source order) override hover on the same element
  when both apply. Three state colours stay distinct:
  hover grey (`#666666`, 2px) / selected black
  (`#000000`, 3px) / focused blue (`#1976d2`, 4px); the
  thicker stroke wins when states stack. Text uses
  underline (different mechanism from M4.12's
  `font-weight: bold` and M4.16's `outline`) so layered
  text states stay readable. **Pure CSS — no JS hover
  handler, no `mouseover`/`mouseout` listener, no
  tooltip, no SVG `<title>` child element** (a `<title>`
  child would compete with the M4.17 `aria-label` as
  the accessible name). Pair-hover (hover a circle →
  also highlight its sibling text) deferred — would
  need JS. Per the new feedback rule, the M4
  checkpoint doc (`docs/milestone-4-checkpoint.md`) is
  refreshed **inline** in this PR (selector count 17 →
  19; HTML wrapper block gains the two hover rules;
  interactivity surface section gets a `:hover` CSS
  bullet; HOVER+TOOLTIPS deferred bucket rewrites to
  "basic :hover CSS shipped at M4.19; richer hover
  behaviour still deferred"). M4.10's XSS-safe DOM API,
  no-network discipline, asymmetric one-inline-script
  invariant, M4.12 `.selected`, M4.13 five-attr DOM
  contract, M4.15 `tabindex` + keydown handler, M4.16
  `:focus-visible` rings, M4.17 `role="button"` +
  `aria-label` all carry over unchanged (additive only).
  **M4 remains in progress** — no
  `docs/milestone-4-result.md`; M4.19 is one more
  skeleton sub-milestone, not an exit. Artefact set
  unchanged (still 10). Save format unchanged (still
  v12). `provinces.svg` bytes UNCHANGED from M4.17 (CSS
  is HTML-only); `map.html` bytes did change (two new
  CSS rules). 5 new doctest cases (885 total). **No JS
  hover handler, no pair-hover, no tooltip, no `<title>`
  child element, no hover-driven detail-panel preview /
  hover delay, no animation / transition on the hover
  state, no broader ARIA (still deferred from M4.17),
  no keyboard polish beyond M4.15, no selection
  persistence, no save schema bump, no new state field
  / artefact / fixture / `InterestGroupKind` /
  `PlayerCommandKind`, no rename of the M4.8 / M4.13
  data-* keys, no second `<script>`, no `<script src=>`,
  no `<script type=>`, no `<link>`, no external CSS /
  font / `<iframe>` / `<img>`, no `fetch` / XHR /
  storage / history / navigation APIs, no `innerHTML` /
  `outerHTML` / `document.write` / `eval` / `Function`,
  no inline event attributes, no per-element inline
  `style="..."`, no `<meta name="viewport">`, no CSS
  animations / transitions / media queries / `@import`
  / `@font-face`, no neighbour / adjacency edges, no
  terrain / resources / population overlays, no runner
  CLI flag, no change to `provinces.svg` bytes, no M4
  close-out, no `docs/milestone-4-result.md`, no "M4
  closed" wording.**
- Previously shipped: **M4.18 — accessibility
  checkpoint refresh.** Mirrors M4.14's role for the M4
  DOM contract: zero new behaviour, just a refreshed
  status snapshot + one new integration assertion.
  Refreshes `docs/milestone-4-checkpoint.md` from its
  M4.14 scope (M4.2–M4.13) to cover the three a11y
  surfaces that landed in M4.15–M4.17: keyboard focus
  (`tabindex="0"` + keydown Enter/Space listener),
  focus-visible CSS (`#1976d2` blue rings via
  `:focus-visible`), and ARIA labels (`role="button"` +
  `aria-label="<name>, <owner_name>"`). The refreshed
  checkpoint now enumerates 17 `<style>` selectors (was
  13 at M4.14; M4.16 added four `:focus`/`:focus-visible`
  rules), a new "Accessibility surface (M4.15–M4.17)"
  section listing tabindex / keydown / focus-visible /
  role=button / aria-label / decorative-legend-swatch
  invariant, and a reworked deferred-items list:
  KEYBOARD+FOCUS surface SHIPPED; BROADER ARIA
  (`aria-selected`, `aria-current`, `aria-pressed`,
  `aria-live`, `aria-describedby`, `aria-labelledby`)
  still deferred; KEYBOARD POLISH (arrow-key nav,
  Escape-to-clear, Tab-within-panel) still deferred.
  Adds **one** new integration assertion in
  `tests/integration/m4_dom_contract_test.cpp` (test F)
  that pins the M4.15–M4.17 a11y surface end-to-end on
  the canonical scenario: 6 `role="button"` occurrences
  per artefact (3 provinces × circle+text); 2
  `aria-label="<name>, <owner_name>"` per province per
  artefact with the canonical composed values; all 3
  legend swatches carry NO `role` / `aria-label` /
  `tabindex` (decorative invariant); `:focus-visible` +
  `#1976d2` CSS in `map.html` but NOT in
  `provinces.svg`; the still-deferred ARIA surface
  stays absent in both artefacts. End-to-end mirror of
  the M4.15–M4.17 svg_export_test unit cases through
  the runner / canonical fixture path. **M4 remains in
  progress** — no `docs/milestone-4-result.md`; M4.18
  is a checkpoint refresh, not an exit. **Renderer
  bytes byte-identical with M4.17** — only tests + docs
  ship. Artefact set unchanged (still 10). Save format
  unchanged (still v12). M1.17 / M2.22 / M3.7
  byte-identical determinism contracts continue to pass.
  2 new doctest cases (880 total: 1 integration test F
  with multiple sub-checks). **No new system / formula /
  artefact / state field / fixture, no save schema
  bump, no new `InterestGroupKind` / `PlayerCommandKind`,
  no new feature surface (M4.18 is docs + 1 integration
  test), no rename of any data-* attribute, no change
  to render_svg_root / render_map_html bytes, no
  broader ARIA, no keyboard polish beyond M4.15, no
  `<meta name="viewport">`, no CSS animations /
  transitions / media queries, no adjacency / terrain /
  overlays, no events / AI / commands, no hover /
  tooltip, no selection persistence, no runner CLI
  flag, no atomic `end_tick` writes, no M4 close-out,
  no `docs/milestone-4-result.md`, no "M4 closed"
  wording, no change to `provinces.svg` or `map.html`
  bytes.**
- Previously shipped: **M4.17 — ARIA labels
  skeleton.** Makes the M4.15-focusable / M4.10-clickable
  province markers **screen-reader-readable**. Every
  `<circle>` and every `<text>` in the SVG body now
  carries `role="button"` (matches the click + Enter/Space
  activation model) + `aria-label="<name>, <owner_name>"`
  (gives the otherwise-nameless `<circle>` a readable
  name; gives `<text>` a consistent name so the
  announcement is the same regardless of which sibling
  has focus). The aria-label is composed at render time:
  `<name>, <owner_name>` when the owner index resolves
  via the same single bounds check used by
  `data-owner-code` / `data-owner-name`; `<name>` alone
  (no trailing comma) when the owner is invalid. The
  whole composed string is XML-attribute-escaped via the
  M4.2 helper, so a name with `& < > " '` cannot break
  the attribute syntax. Lives in `render_svg_root` so
  the standalone `provinces.svg` picks up both attrs
  too. The M4.7 legend swatch `<circle>` elements in
  `map.html` are emitted separately (inside
  `render_map_html`, not in `render_svg_root`) and
  deliberately carry neither `role` nor `aria-label` —
  they stay decorative. **This intentionally reverses
  the M4.15/M4.16 "no ARIA" non-goal** in a narrowly-
  scoped way: only `role="button"` + `aria-label` ship;
  the broader ARIA surface (`aria-selected`,
  `aria-current`, `aria-pressed`, `aria-live`,
  `aria-describedby`, `aria-labelledby`) stays deferred
  to a future dedicated A11Y sub-milestone. The
  M4.15/M4.16 unit tests retune accordingly: the
  over-broad "role= / aria-label= absent" checks become
  "narrower-ARIA-surface still absent" checks. M4.10's
  XSS-safe DOM API, no-network discipline, asymmetric
  one-inline-script invariant, M4.12 `.selected`,
  M4.13 five-attr DOM contract, M4.15 `tabindex` +
  keydown handler, and M4.16 `:focus-visible` rings all
  carry over unchanged (additive only). `role="button"`
  + `aria-label` chosen specifically: `role="link"` was
  rejected (the handler doesn't navigate); `role="option"`
  was rejected (would require `role="listbox"` on the
  SVG, arrow-key navigation, `aria-activedescendant` —
  out of scope); no role was rejected (focused markers
  would announce as plain graphics). aria-label values
  match what the M4.10/M4.11 details panel renders for
  the `Province Name` / `Owner Name` rows, so sighted
  users see the same identity screen-reader users hear.
  **M4 remains in progress** — no
  `docs/milestone-4-result.md`; M4.17 is one more
  skeleton sub-milestone, not an exit. Artefact set
  unchanged (still 10). Save format unchanged (still
  v12) — aria-label is composed from existing
  `ProvinceNode` + `state.countries` fields, not a new
  persistent state field. `provinces.svg` bytes DID
  change (two new attributes on every `<circle>` +
  `<text>` — additive only); `map.html` bytes did
  change (same SVG body). 7 new doctest cases (878
  total). **No `aria-selected=`, no `aria-current=`, no
  `aria-pressed=`, no `aria-live=`, no
  `aria-describedby=`, no `aria-labelledby=`, no
  `role=` other than `"button"`, no `<title>` /
  `<desc>` child elements on the markers, no state
  mutation, no commands, no AI, no events, no selection
  persistence, no tooltip, no hover, no animation, no
  keyboard shortcut for the panel, no save schema bump,
  no new state field / artefact / fixture /
  `InterestGroupKind` / `PlayerCommandKind`, no rename
  of the M4.8 / M4.13 data-* keys, no second
  `<script>`, no `<script src=>`, no `<script type=>`,
  no `<link>`, no external CSS / font / `<iframe>` /
  `<img>`, no `fetch` / XHR / storage / history /
  navigation APIs, no `innerHTML` / `outerHTML` /
  `document.write` / `eval` / `Function`, no inline
  event attributes, no per-element inline `style="..."`,
  no `<meta name="viewport">`, no CSS animations /
  transitions / media queries / `@import` / `@font-face`,
  no neighbour / adjacency edges, no terrain / resources
  / population overlays, no runner CLI flag, no M4
  close-out, no `docs/milestone-4-result.md`, no "M4
  closed" wording.**
- Previously shipped: **M4.16 — focus-visible
  styling skeleton.** Makes M4.15's keyboard focus
  VISIBLE. Pure CSS — no JavaScript change, no markup
  change beyond the `<style>` block. Four new CSS rules
  in the M4.6 `<style>` block:
  `svg circle:focus { outline: none; }` +
  `svg circle:focus-visible { outline: none; stroke:
  #1976d2; stroke-width: 4; }` +
  `svg text:focus { outline: none; }` +
  `svg text:focus-visible { outline: 2px solid #1976d2;
  outline-offset: 2px; }`. The bare-`:focus` rules
  suppress the browser's default focus outline so the
  `:focus-visible` styling wins. Uses `:focus-visible`
  (NOT bare `:focus`) so the ring appears for
  keyboard-triggered focus only and NOT for mouse
  clicks — that keeps the M4.12 `.selected` highlight
  (set by click/activate, black stroke) visually
  distinct from the M4.16 keyboard-focus indicator
  (blue stroke / blue outline). Colour `#1976d2`
  chosen to contrast with the M4.3 owner palette and
  with the M4.12 `.selected` `#000000` stroke. Circle
  uses stroke-based ring (matches the shape outline);
  text uses CSS outline + outline-offset (rectangular
  ring around the text bounding box). M4.10's XSS-safe
  DOM API, no-network discipline, asymmetric
  one-inline-script invariant, M4.12 `.selected`
  surface, M4.13 five-attr DOM contract, M4.15
  `tabindex="0"` + keydown handler all carry over
  unchanged (additive only). **Explicit non-goal: still
  NO ARIA polish** — no `role=`, `aria-label=`,
  `aria-selected=`, etc. That lands in a future
  dedicated A11Y sub-milestone. **M4 remains in
  progress** — no `docs/milestone-4-result.md`; M4.16
  is one more skeleton sub-milestone, not an exit.
  Artefact set unchanged (still 10). Save format
  unchanged (still v12). `provinces.svg` bytes
  UNCHANGED from M4.15 — the focus CSS lives entirely
  in the HTML wrapper's `<style>` block. `map.html`
  bytes did change (four new CSS rules). 5 new doctest
  cases (871 total). **No state mutation, no commands,
  no AI, no events, no selection persistence, no
  tooltip, no hover, no animation / transition on the
  ring, no `:focus-visible` polyfill (modern browsers
  only; old browsers fall back to no ring with no
  regression), no save schema bump, no new state field
  / artefact / fixture / `InterestGroupKind` /
  `PlayerCommandKind`, no rename of the M4.8 / M4.13
  data-* keys, no second `<script>`, no `<script
  src=>`, no `<script type=>`, no `<link>`, no
  external CSS / font / `<iframe>` / `<img>`, no
  `fetch` / XHR / storage / history / navigation APIs,
  no `innerHTML` / `outerHTML` / `document.write` /
  `eval` / `Function`, no inline event attributes, no
  per-element inline `style="..."`, no `<meta
  name="viewport">`, no CSS animations / transitions
  / media queries / `@import` / `@font-face`, no
  neighbour / adjacency edges, no terrain / resources
  / population overlays, no runner CLI flag, no change
  to `provinces.svg` bytes, no M4 close-out, no
  `docs/milestone-4-result.md`, no "M4 closed"
  wording.**
- Previously shipped: **M4.15 — keyboard focus
  accessibility skeleton.** First keyboard-input surface
  for the M4 viewer. Every `<circle>` and every `<text>`
  in the SVG body now carries `tabindex="0"` (rendered in
  `render_svg_root`, so the standalone `provinces.svg`
  picks it up too); the inline `<script>` in `map.html`
  registers a `keydown` listener alongside the existing
  `click` listener so pressing **Enter** or **Space**
  while focused on a province fires the same
  `selectProvince + showDetails` pair the click runs. The
  keydown handler calls `event.preventDefault()` for
  Space to suppress the browser's default page-scroll.
  The click and keydown handlers share a per-element
  `activate()` closure so the effect cannot drift between
  modalities. The M4.7 legend swatch `<circle>` elements
  in `map.html` are emitted separately inside
  `render_map_html` (not in `render_svg_root`) and lack
  `tabindex`, so they stay out of the tab order — the
  existing M4.10 selector `svg circle[data-id], svg
  text[data-id]` already skipped them anyway. **This is
  a skeleton, NOT a full ARIA polish.** Explicit
  non-goals enforced by tests: no `role=`, no
  `aria-label=`, no `aria-selected=`, no `aria-current=`,
  no `aria-pressed=`, no `:focus` / `:focus-visible` CSS
  (the browser's default focus ring is what users see),
  no `tabindex` values other than `"0"` (no
  programmatic-only focus; no manual ordering), no
  keyboard shortcut for the panel, no skip-link, no focus
  management between renders. Those land in a future
  dedicated A11Y sub-milestone. M4.10's XSS-safe DOM API
  (`createElement` + `textContent` only; no `innerHTML` /
  `outerHTML` / `document.write` / `eval` / `Function`),
  no-network discipline (no `fetch` / `XMLHttpRequest`),
  asymmetric one-inline-script invariant, M4.12 transient
  `.selected` surface, and the M4.8 + M4.13 five-attr DOM
  contract all carry over unchanged (additive only). **M4
  remains in progress** — no `docs/milestone-4-result.md`;
  M4.15 is one more skeleton sub-milestone, not an exit.
  Artefact set unchanged (still 10). Save format
  unchanged (still v12) — `tabindex` is render-time only,
  not a new field on `ProvinceNode`. `provinces.svg`
  bytes DID change (new `tabindex="0"` on every
  `<circle>` + `<text>` — additive only); `map.html`
  bytes did change (same SVG body + the refactored
  listener loop + new keydown wiring in the script). 9
  new doctest cases (866 total: 6 svg_export + 3 in the
  standalone-SVG / integration / etc. cluster). **No
  state mutation, no commands, no AI, no events emitted
  by keyboard activation, no selection persistence, no
  multi-select / shift-Enter / right-click, no hover, no
  tooltip, no animation, no save schema bump, no new
  state field, no new artefact, no new fixture, no new
  `InterestGroupKind` / `PlayerCommandKind`, no rename
  of the M4.8 / M4.13 data-* keys, no second `<script>`,
  no `<script src=>`, no `<script type=>`, no `<link>`,
  no external CSS / font / `<iframe>` / `<img>`, no
  `fetch` / XHR / storage / history / navigation APIs,
  no `innerHTML` / `outerHTML` / `document.write` /
  `eval` / `Function`, no inline event attributes
  (`onkeydown=` / ...; the M4.15 keydown wiring uses
  `addEventListener` exclusively), no per-element inline
  `style="..."`, no `<meta name="viewport">`, no CSS
  animations / transitions / media queries / `@import`
  / `@font-face`, no neighbour / adjacency edges, no
  terrain / resources / population overlays, no runner
  CLI flag, no M4 close-out, no `docs/milestone-4-result.md`,
  no "M4 closed" wording.**
- Previously shipped: **M4.14 — DOM contract
  checkpoint refresh.** Mirrors M4.9's role for the M4
  reaction loop: zero new behaviour, just a refreshed
  status snapshot and one new integration assertion.
  Refreshes `docs/milestone-4-checkpoint.md` from its
  original M4.2–M4.8 scope to cover the four surfaces
  that landed in M4.10–M4.13: first inline `<script>` in
  `map.html` (asymmetric JS boundary —
  `provinces.svg` stays script-free); details panel
  `<dt>` labels decoupled from raw `data-*` keys
  (`Province ID` / `Owner Index` / `Owner Code` /
  `Province Name` / new `Owner Name` at M4.13); transient
  `.selected` class + `circle.selected` / `text.selected`
  CSS + `selectProvince(el)` helper (purely DOM-level —
  no `localStorage` / `sessionStorage` / cookie / URL
  fragment); fifth `data-owner-name` attribute on every
  `<circle>` and `<text>` derived from
  `state.countries[owner].name`. The checkpoint doc now
  enumerates 13 CSS selectors in the `<style>` block (was
  6 at M4.9), an interactivity-surface section listing
  `<div id="details">` / `.selected` / `selectProvince`
  / `showDetails` / the 5-entry `fields` array, and a
  reworked deferred-items list bucketed into HOVER+TOOLTIPS,
  KEYBOARD+A11Y, PERSISTENT SELECTION, DOM EXTENSIONS,
  VISUAL POLISH, INFRASTRUCTURE. Adds **one** new
  integration assertion in
  `tests/integration/m4_dom_contract_test.cpp` (test D)
  that pins the M4.13-era five-entry fields list inside
  the canonical `map.html` script — both the five raw
  `data-*` attribute names and the five human-readable
  labels appear verbatim; `provinces.svg` carries none of
  the JS-literal forms. Test D is the end-to-end mirror
  of the M4.11/M4.13 svg_export_test unit cases —
  catches accidental shrinkage back to a four-entry
  fields array or label drift through the actual runner
  path. **M4 remains in progress** — no
  `docs/milestone-4-result.md`; M4.14 is a checkpoint
  refresh, not an exit. **Renderer bytes byte-identical
  with M4.13** — only tests + docs ship. Artefact set
  unchanged (still 10). Save format unchanged (still
  v12). M1.17 / M2.22 / M3.7 byte-identical determinism
  contracts continue to pass. 1 new doctest case (857
  total). **No new system, no new formula, no new
  artefact, no save schema bump, no new state field, no
  new fixture, no new `InterestGroupKind` /
  `PlayerCommandKind`, no new feature surface (M4.14 is
  docs + 1 integration test), no rename of any data-*
  attribute, no change to the click handler / details
  panel / `.selected` CSS / fields array bytes, no
  `<meta name="viewport">`, no CSS animations /
  transitions / media queries / `@import` / `@font-face`,
  no neighbour / adjacency edges, no terrain / resources
  / population overlays, no events / AI / command
  integration, no hover state / tooltips / keyboard
  navigation / `aria-*` polish, no selection persistence
  across reloads, no runner CLI flag, no atomic
  `end_tick` writes, no M4 close-out, no
  `docs/milestone-4-result.md`, no "M4 closed" wording,
  no change to `provinces.svg` or `map.html` bytes.**
- Previously shipped: **M4.13 — details panel
  owner-name polish.** Widens the M4.8 identity surface by
  one attribute and the M4.11 details-panel `fields`
  array by one row. Every `<circle>` and every `<text>`
  in the SVG body now also carries `data-owner-name`,
  resolved from `state.countries[owner.value()].name` (or
  `""` when the owner index is invalid — same defensive
  fallback as the M4.8 `data-owner-code`). A **single
  bounds check** covers both lookups so they cannot
  disagree about validity. The new value is
  XML-attribute-escaped via the M4.2 helper. The M4.11
  `fields` array grows from four to five entries — the
  new row is `{ attr: "data-owner-name", label: "Owner
  Name" }`, and the details panel now renders five dt/dd
  pairs (`Province ID` / `Owner Index` / `Owner Code` /
  `Owner Name` / `Province Name`). **Save format stays
  v12** — `data-owner-name` is **derived** from
  `state.countries` at render time, not a new field on
  `ProvinceNode`, so the save schema does not grow.
  Future M4 hover / tooltip / clickable-UI sub-milestones
  reading the data-* surface get the country name for
  free via `getAttribute("data-owner-name")` instead of
  having to DOM-walk the legend or build a JS country
  lookup table inside the inline script. M4.10's XSS-safe
  DOM API, no-network discipline, asymmetric one-
  inline-script invariant, M4.12's transient `.selected`
  surface, and the M4.8 four-attr DOM contract all carry
  over unchanged (the M4.8 keys are NOT renamed; M4.13
  is purely additive). **M4 remains in progress** — no
  `docs/milestone-4-result.md`; M4.13 is one more
  additive widening, not an exit. Artefact set unchanged
  (still 10). Save format unchanged (still v12).
  `provinces.svg` bytes DID change (the new attribute on
  every `<circle>` + `<text>` — additive only; no
  removed attributes, no rendered-pixel movement);
  `map.html` bytes did change (same SVG body + the new
  fifth `fields` entry). M1.17 / M2.22 / M3.7
  byte-identical determinism contracts continue to pass
  by construction. 8 new doctest cases (856 total). **No
  new field on `ProvinceNode`, no save schema bump, no
  new state field, no new artefact, no new fixture, no
  new `InterestGroupKind` / `PlayerCommandKind`, no
  rename of the M4.8 data-* keys, no state mutation, no
  commands, no AI, no events, no selection persistence,
  no multi-select / right-click, no hover state, no
  tooltips, no keyboard navigation / focus ring /
  `aria-*` polish, no animation, no second `<script>`,
  no `<script src=>`, no `<script type=>`, no `<link>`,
  no external CSS / font / `<iframe>` / `<img>`, no
  `fetch` / XHR / storage / history / navigation APIs,
  no `innerHTML` / `outerHTML` / `document.write` /
  `eval` / `Function`, no inline event attributes, no
  per-element inline `style="..."`, no `<meta
  name="viewport">`, no CSS animations / transitions /
  media queries / `@import` / `@font-face`, no neighbour
  / adjacency edges, no terrain / resources / population
  overlays, no runner CLI flag, no M4 close-out, no
  `docs/milestone-4-result.md`, no "M4 closed" wording.**
- Previously shipped: **M4.12 — clickable UI
  selected-state CSS skeleton.** Layers a transient
  selection highlight on top of the M4.10 click handler /
  M4.11 details labels. Two new CSS rules in the M4.6
  `<style>` block — `svg circle.selected { stroke:
  #000000; stroke-width: 3; }` and `svg text.selected {
  font-weight: bold; }`. The click handler now also calls
  a `selectProvince(el)` helper that uses
  `classList.remove("selected")` on every prior
  `.selected` node and `classList.add("selected")` on
  every node sharing the clicked element's `data-id`
  (clicking either the `<circle>` or the `<text>`
  highlights the whole province pair — fulfils the M4.8
  design intent that "a future clickable UI can address
  either element uniformly"). The selection logic
  deliberately walks the pre-collected `nodes` NodeList
  and compares `data-id` strings, so the attribute value
  never re-enters a CSS-selector parser at runtime. The
  initial render carries **NO `class="selected"`**
  anywhere; the class only appears at click time.
  **Selection is purely DOM-level**: never written into
  `GameState`, never persisted across reloads, no
  `localStorage` / `sessionStorage` / cookie / URL
  fragment — a page reload always starts with nothing
  selected. M4.10's XSS-safe DOM API (`createElement` +
  `textContent` only; no `innerHTML` / `outerHTML` /
  `document.write` / `eval` / `Function`), no-network
  discipline (no `fetch` / `XMLHttpRequest`), and
  asymmetric "exactly one inline `<script>` in `map.html`;
  `provinces.svg` stays script-free" invariant all carry
  over unchanged. The M4.8 `data-*` DOM contract on
  `<circle>` / `<text>` is **NOT renamed**. **M4 remains
  in progress** — no `docs/milestone-4-result.md`; M4.12
  is the first selection-surface skeleton, not an exit.
  Artefact set unchanged (still 10). Save format unchanged
  (still v12). `provinces.svg` bytes unchanged from M4.8;
  `map.html` bytes did change (two new CSS rules + new
  `selectProvince` helper + extended click listener). 5
  new doctest cases (848 total). **No state mutation, no
  commands, no AI, no events emitted by the selection, no
  selection persistence, no multi-select / shift-click /
  right-click / context menu, no hover state, no tooltips,
  no keyboard navigation / focus ring / `aria-*` polish,
  no animation / transition on the highlight, no save
  schema bump, no new state field, no new artefact, no
  new fixture, no new `InterestGroupKind` /
  `PlayerCommandKind`, no rename of the M4.8 `data-*` DOM
  contract keys, no second `<script>`, no `<script src=>`,
  no `<script type=>`, no `<link>`, no external CSS / font
  / `<iframe>` / `<img>`, no `fetch` / XHR / storage /
  history / navigation APIs, no `innerHTML` / `outerHTML`
  / `document.write` / `eval` / `Function`, no
  `className` string concatenation, no `setAttribute(
  "class", ...)`, no inline event attributes, no
  per-element inline `style="..."`, no `<meta
  name="viewport">`, no CSS animations / transitions /
  media queries / `@import` / `@font-face`, no neighbour
  / adjacency edges, no terrain / resources / population
  overlays, no runner CLI flag, no change to
  `provinces.svg` bytes, no M4 close-out, no
  `docs/milestone-4-result.md`, no "M4 closed" wording.**
- Previously shipped: **M4.11 — clickable UI
  details labels polish.** Pure UX polish on the M4.10
  click handler. The `<dt>` labels rendered into the
  `<div id="details">` panel are decoupled from the raw
  `data-*` attribute names: `getAttribute` still reads the
  M4.8 DOM contract keys (`data-id` / `data-owner` /
  `data-owner-code` / `data-name` — **NOT renamed**; the
  `<circle>` / `<text>` surface is byte-identical with
  M4.10), but each `<dt>` now renders a fixed
  human-readable label (`Province ID` / `Owner Index` /
  `Owner Code` / `Province Name`). The renderer change is
  one JS literal: `var keys = [...]` →
  `var fields = [{attr, label}, ...]`, with
  `dt.textContent = f.label` and
  `dd.textContent = el.getAttribute(f.attr) || ""`. M4.10's
  XSS-safe DOM API (`createElement` + `textContent` only,
  no `innerHTML` / `outerHTML` / `document.write` / `eval`
  / `Function`), no-storage / no-network discipline (no
  `fetch` / `XMLHttpRequest` / `localStorage` /
  `sessionStorage` / `history.pushState` / `window.location`
  / `navigator`), and asymmetric "exactly one inline
  `<script>` in `map.html`; `provinces.svg` stays
  script-free" invariant all carry over unchanged. **M4
  remains in progress** — no `docs/milestone-4-result.md`;
  M4.11 is a UX polish, not an exit. Artefact set unchanged
  (still 10). Save format unchanged (still v12).
  `provinces.svg` bytes unchanged from M4.8; `map.html`
  bytes did change (four new label strings + new `fields`
  array structure inside the inline script). 4 new doctest
  cases (843 total). **No state mutation, no commands, no
  AI, no events emitted by the click, no selection
  persistence, no multi-select / right-click, no hover
  state, no tooltips, no keyboard navigation / focus ring
  / `aria-*` polish, no animation, no save schema bump, no
  new state field, no new artefact, no new fixture, no new
  `InterestGroupKind` / `PlayerCommandKind`, no rename of
  the M4.8 `data-*` DOM contract keys, no second
  `<script>`, no `<script src=>`, no `<script type=>`, no
  `<link>`, no external CSS / font / `<iframe>` / `<img>`,
  no `fetch` / XHR / storage / history / navigation APIs,
  no `innerHTML` / `outerHTML` / `document.write` / `eval`
  / `Function`, no inline event attributes, no per-element
  inline `style="..."`, no `<meta name="viewport">`, no
  CSS animations / transitions / media queries / `@import`
  / `@font-face`, no neighbour / adjacency edges, no
  terrain / resources / population overlays, no runner CLI
  flag, no change to `provinces.svg` bytes, no M4
  close-out, no `docs/milestone-4-result.md`, no "M4
  closed" wording.**
- Previously shipped: **M4.10 — HTML clickable UI
  skeleton.** First JavaScript in `map.html`. A single
  inline `<script>` IIFE at the end of `<body>` attaches one
  `click` listener per `svg circle[data-id], svg
  text[data-id]` element; the listener reads the four M4.8
  `data-*` attributes off the clicked element via
  `getAttribute` and renders them as a `<dl>` inside a new
  `<div id="details">` placeholder that sits between the
  inline SVG and the M4.7 legend. The placeholder starts
  with `<p class="details-empty">Click a province to see
  its details.</p>`; the first click replaces the
  placeholder text with the `<dl>`, subsequent clicks
  replace the previous `<dl>`. The handler is **stateless +
  XSS-safe**: it uses `createElement` + `textContent` only
  (no `innerHTML` / `outerHTML` / `document.write` / `eval`
  / `Function`), and never calls `fetch` / `XMLHttpRequest`
  / `localStorage` / `sessionStorage` / `history.pushState`
  / `window.location` / `navigator`. The selector
  deliberately skips the M4.7 legend swatch `<circle>`
  elements (which have no `data-id`), keeping the legend
  non-clickable. Four new CSS rules (`.details` + dl/dt/dd
  + `.details-empty` + `svg circle[data-id], svg
  text[data-id] { cursor: pointer; }`) live in the M4.6
  `<style>` block; **no per-element inline `style="..."`**
  — the M4.6 single-CSS-surface contract holds. The
  JavaScript boundary is now **asymmetric**:
  `provinces.svg` stays fully script-free / inert;
  `map.html` carries EXACTLY ONE inline script (no `src=`,
  no `type=`). The M4.9 integration test C splits to
  enforce the new asymmetric invariant; M4.5 / M4.6 "no
  `<script>`" unit test retunes accordingly. **M4 remains
  in progress** — no `docs/milestone-4-result.md` is
  written; M4.10 is one more skeleton sub-milestone, not
  an exit report. Artefact set unchanged (still 10). Save
  format unchanged (still v12). `provinces.svg` bytes
  unchanged from M4.8; `map.html` bytes did change (new
  CSS + placeholder + script). 8 new doctest cases (839
  total: 7 in `svg_export_test`, 1 in `runner_test`).
  **M4 in progress.** **No state mutation from the viewer,
  no commands, no AI, no events emitted by the click, no
  selection persistence, no multi-select / shift-click /
  right-click, no hover state, no tooltips, no keyboard
  navigation / focus ring / `aria-*` polish, no animation,
  no save schema bump (still v12), no new state field, no
  new artefact (still 10), no new fixture, no new
  `InterestGroupKind` / `PlayerCommandKind`, no second
  `<script>`, no `<script src=>`, no `<script
  type="module">`, no `<link>`, no external CSS / font /
  `<iframe>` / `<img>`, no `fetch` / `XMLHttpRequest` /
  `localStorage` / `sessionStorage` / `history.pushState`
  / `window.location` / `navigator`, no `innerHTML` /
  `outerHTML` / `document.write` / `eval` / `Function`,
  no inline event attributes (`onclick=` / ...), no
  per-element inline `style="..."`, no `<meta
  name="viewport">`, no CSS animations / transitions /
  media queries / `@import` / `@font-face`, no neighbour
  / adjacency edges, no terrain / resources / population
  overlays, no runner CLI flag, no atomic `end_tick`
  writes, no change to `provinces.svg` bytes, no M4
  close-out, no `docs/milestone-4-result.md`, no "M4
  closed" wording.**
- Previously shipped: **M4.9 — HTML DOM contract
  checkpoint.** Mirrors M3.7's role for the M3 reaction
  loop: zero new behaviour, just three new integration
  tests (`tests/integration/m4_dom_contract_test.cpp`) and
  a single-page snapshot (`docs/milestone-4-checkpoint.md`)
  that formally pin the M4.2–M4.8 SVG / HTML DOM contract.
  The three end-to-end cases: (1) every canonical province
  surfaces all four `data-*` attributes on **both**
  `<circle>` and `<text>` in **both** `provinces.svg` and
  `map.html`; (2) the legend has one `<li data-owner="N">`
  per `state.countries[i]` and each row carries its
  country's `id_code`; (3) the no-interactivity invariant
  holds — no `<script>`, no `<link>`, no inline event
  attributes, no per-element `style="..."`, and
  `provinces.svg` additionally has no `<style>` block at
  all. (M4.10 reframes invariant 3 as asymmetric:
  `provinces.svg` stays script-free; `map.html` now
  carries exactly one inline script.) Renderer bytes
  unchanged from M4.8 — only tests + docs ship in M4.9.
  3 new doctest cases (830 total at M4.9).
- Previously shipped: **M4.8 — HTML static
  province data attributes skeleton.** Widens the identity
  surface inside the SVG body: every `<circle>` and every
  `<text>` now carries the same four read-only `data-*`
  attributes (`data-id`, `data-owner`, `data-owner-code`,
  `data-name`) so a future clickable UI / DOM script can
  address either element uniformly without DOM-walking
  siblings. `data-owner-code` resolves to
  `state.countries[owner.value()].id_code` when the index
  is valid, or the empty string otherwise (defensive
  fallback for hand-built states; save / scenario layers
  reject invalid owners at load time). `data-name` mirrors
  the `<text>` body content but exposes it as a uniform
  attribute for programmatic lookup. All four data-* values
  are XML-attribute-escaped via the M4.2 helper. **This is
  the first M4.x sub-milestone since M4.4 that deliberately
  edits the standalone SVG body** — the change is purely
  additive (no removed attributes, no rendered-pixel
  movement); SVG-to-PNG pipelines and vector tools see no
  visual difference. Both `provinces.svg` and `map.html`
  pick up the new attributes for free since they share the
  `render_svg_root` helper. All M4.5 / M4.6 / M4.7 nots
  preserved: no JavaScript, no `<script>`, no `<link>`, no
  inline event attributes, no inline `style="..."` per
  element, no `<meta name="viewport">`, no CSS animations /
  transitions / media queries / `@import` / `@font-face`,
  no click handlers, no clickable UI, no hover state, no
  tooltips, no state mutation. Artefact set unchanged
  (still 10). Save format unchanged (still v12). M1.17 /
  M2.22 / M3.7 byte-identical determinism contracts
  continue to pass by construction. 8 new doctest cases (7
  svg_export + 1 runner; M4.4 empty-name test retuned to
  anchor on the new stable surface; 827 total). **M4 in
  progress.** **No JavaScript, no click handlers, no event
  handlers / hover state / tooltips, no state mutation
  (data-* are read-only viewer surface), no `<script>` / no
  `<link>` / no inline event attributes / no inline
  `style="..."` per element, no `<meta name="viewport">`,
  no CSS animations / transitions / media queries /
  `@import` / `@font-face`, no new artefact (still 10), no
  save schema bump (still v12), no new state field, no new
  `InterestGroupKind` / `PlayerCommandKind`, no runner CLI
  flag, no events / AI / command integration, no
  ownership-dynamics layer, no neighbour / adjacency edges,
  no terrain / resources / population overlays, no new
  gameplay, no atomic `end_tick` writes, no per-province
  colour override, no new `<circle>` / `<text>` presentation
  attributes (the change is data-* only; `cx` / `cy` / `r`
  / `fill` / `x` / `y` / `text-anchor` are unchanged).**
- Previously shipped: **M4.7 — HTML legend
  skeleton.** Adds a static `<ul class="legend">` to
  `map.html` immediately after the inline SVG so a viewer
  can decode which palette colour belongs to which country.
  One `<li data-owner="N">` per `state.countries[i]`, in
  vector order; content is `<svg class="swatch"
  viewBox="0 0 16 16">...<circle ... fill="<palette>"/>
  </svg>` + `"id_code &mdash; name"` text. The per-row
  swatch is a tiny inline SVG (not an HTML element with
  `background-color`), so the M4.6 "no inline `style="..."`
  on individual elements" constraint stays unbroken — the
  `fill` attribute on `<circle>` is an SVG presentation
  attribute, not an HTML inline style. Three new CSS rules
  join the M4.6 block: `.legend { list-style: none;
  padding: 0; margin: 20px auto; max-width: 1000px;
  font-family: sans-serif; }`, `.legend li { display: flex;
  align-items: center; margin: 4px 0; }`, `.legend .swatch
  { width: 16px; height: 16px; margin-right: 8px;
  flex-shrink: 0; }`. Text content XML-text-escaped via the
  M4.4 `xml_text_escape` helper. Empty `state.countries`
  produces an empty `<ul>` (always-present-file contract
  preserved). **`provinces.svg` byte output unchanged** —
  the legend lives only in the HTML wrapper; the standalone
  SVG path stays CSS-free, legend-free for downstream
  consumers (SVG-to-PNG pipelines, vector tools). All M4.5
  / M4.6 nots preserved: no JavaScript, no `<script>`, no
  `<link>`, no inline event attributes, no inline `style="..."`
  per element, no `<meta name="viewport">`, no CSS
  animations / transitions / media queries / `@import` /
  `@font-face`. M4.4 `<text>` font-family / font-size
  contract on the elements themselves preserved. Artefact
  set unchanged (still 10). Save format unchanged (still
  v12). M1.17 / M2.22 / M3.7 byte-identical determinism
  contracts continue to pass by construction. 9 new doctest
  cases (8 svg_export + 1 runner; 819 total). **M4 in
  progress.** **No JavaScript / `<script>`, no `<link>`
  external stylesheet, no inline event attributes, no
  inline `style="..."`, no `<meta name="viewport">`, no CSS
  animations / transitions / media queries / `@import` /
  `@font-face`, no click handlers, no clickable UI, no
  hover state, no tooltips, no state mutation from the
  viewer, no font-family / font-size on the SVG `<text>`
  elements themselves, no ownership dynamics, no neighbour
  / adjacency edges, no terrain / resources / population
  overlays, no events, no AI, no command integration, no
  new `PlayerCommandKind`, no runner CLI flag, no new
  artefact (still 10), no save schema bump (still v12), no
  new state field, no new gameplay, no atomic `end_tick`
  writes, no change to `provinces.svg` bytes.**
- Previously shipped: **M4.6 — HTML viewer
  minimal CSS skeleton.** Adds the smallest possible
  inline `<style>` block to the M4.5 HTML wrapper — three
  CSS selectors that centre the SVG card on a neutral page,
  give it a thin border so the white-fill body pops, and
  switch the labels to a sans-serif font so they're more
  readable than the browser's serif default for SVG
  `<text>`. Selectors: `body { margin: 0; padding: 20px;
  background-color: #f0f0f0; }`, `svg { display: block;
  margin: 0 auto; border: 1px solid #888; background-color:
  #ffffff; }`, `svg text { font-family: sans-serif; }`. The
  `<style>` block sits at 2-space indent inside `<head>`
  alongside the existing `<meta>` and `<title>`. `provinces.svg`
  byte output is **unchanged** — the CSS lives only in the
  HTML wrapper; the standalone-SVG path stays CSS-free for
  downstream consumers (SVG-to-PNG pipelines, vector tools).
  All M4.5 nots preserved: no JavaScript, no `<script>`, no
  `<link>` to external stylesheet, no inline event
  attributes (`onclick` / `onmouseover` / `onload` / ...),
  no `<meta name="viewport">`, no inline `style="..."` on
  individual elements. M4.4 `<text>` font-family / font-size
  contract preserved on the elements themselves — only the
  CSS selector `svg text` sets the font, which applies only
  to the HTML viewer. M1.17 / M2.22 / M3.7 byte-identical
  determinism contracts continue to pass by construction.
  Artefact set unchanged (still 10). Save format unchanged
  (still v12). 6 new doctest cases (5 svg_export + 1 runner;
  the M4.5 "no `<style>`" test was retuned to drop its
  `<style>` assertion and add an inline-`style=` check;
  810 total). **M4 in progress.** **No JavaScript / `<script>`,
  no `<link>` external stylesheet, no inline event
  attributes, no inline `style="..."`, no `<meta name="viewport">`,
  no per-province / per-element CSS override, no CSS
  animations / transitions / media queries / `@import` /
  `@font-face`, no click handlers, no clickable UI, no
  hover state, no tooltips, no state mutation from the
  viewer, no legend / colour key, no font-family /
  font-size on the `<text>` elements themselves, no
  ownership dynamics, no neighbour / adjacency edges, no
  terrain / resources / population overlays, no events, no
  AI, no command integration, no new `PlayerCommandKind`,
  no runner CLI flag, no new artefact (still 10), no save
  schema bump (still v12), no new state field, no new
  gameplay, no atomic `end_tick` writes, no change to
  `provinces.svg` bytes.**
- Previously shipped: **M4.5 — HTML viewer
  skeleton.** Wraps the existing SVG body in a minimal
  HTML5 document so the map opens cleanly in a browser
  without the raw-XML chrome standalone `.svg` files
  attract. New public functions on
  `leviathan::systems::svg_export`:
  `render_map_html(state) → std::string` (pure transform)
  and `write_map_html(state, path) → Result<bool>` (render +
  file write). Internal refactor extracted a shared
  `render_svg_root` helper so `render_provinces` continues
  to emit exactly the same bytes (verified by existing M4.x
  tests staying green without modification). HTML shape:
  `<!DOCTYPE html>` + `<html lang="en">` + minimal `<head>`
  (`<meta charset="UTF-8">` + `<title>Leviathan Map</title>`)
  + `<body>` containing the inline `<svg>` body (no XML
  prolog — that line is invalid inside HTML). No CSS / no
  JavaScript / no `<style>` / no `<script>` / no `<link>` /
  no inline event handlers — M4.5 ships the minimum
  wrapper. Inline (not external-reference) embedding chosen
  so the file is self-contained: no `file://` vs `http://`
  CORS pitfalls, can be emailed / shared standalone. `end_tick`
  writes `map.html` UNCONDITIONALLY as the **10th artefact**
  (mirrors the M3.5 / M3.6 / M4.2 unconditional-artefact
  pattern; satisfies the M3-exit-report §5 "growing the set
  needs its own sub-milestone with the contracts documented"
  rule for the second time). `RunnerOptions::map_html_path`
  optional override (no CLI flag); default
  `<output_dir>/map.html`; `RunOutcome::map_html_path` carries
  the resolved path. M2.9 pre-`end_tick` no-artefact contract
  extends automatically; M3.6 mid-`end_tick` non-transactional
  caveat extends similarly (still a deferred item). M1.17 /
  M2.22 / M3.7 byte-identical determinism contracts extended
  from 9 to 10 artefacts. Save format unchanged (still v12).
  `provinces.svg` byte output unchanged from M4.4. 12 new
  doctest cases (7 svg_export + 5 runner; 804 total).
  **M4 in progress.** **No click handlers, no clickable UI,
  no event handlers, no hover state, no tooltips, no state
  mutation from the viewer, no legend / colour key, no CSS
  / JavaScript / `<style>` / `<script>` / `<link>` / inline
  event attributes, no `<meta name="viewport">`, no
  font-family / font-size on `<text>`, no ownership
  dynamics, no neighbour / adjacency edges, no terrain /
  resources / population overlays, no events, no AI, no
  command integration, no new `PlayerCommandKind`, no
  runner CLI flag, no save schema bump (still v12), no
  new state field, no new gameplay, no atomic `end_tick`
  writes.**
- Previously shipped: **M4.4 — SVG labels
  skeleton.** Adds one `<text>` label per `<circle>` in
  `provinces.svg`. Each label is positioned at `(cx, cy +
  kLabelYOffset)` with `text-anchor="middle"`, content set
  to the XML-text-escaped `ProvinceNode::name`. New public
  constant `kLabelYOffset = 22.0` on
  `leviathan::systems::svg_export`. New `xml_text_escape`
  helper in the anonymous namespace escapes only `& < >`
  per the XML 1.0 §2.4 text-content rules; the M4.2
  `xml_attr_escape` (which also handles `" '`) continues to
  cover `data-id` unchanged. `<circle>` and `<text>` are
  interleaved per node so each node's two elements stay
  grouped in the byte stream. No font-family / font-size /
  fill on `<text>` — the SVG consumer's default applies
  (M4.4 ships minimum labels; typography is a future
  presentation sub-milestone). Empty `name` still emits an
  empty-bodied `<text>` so the renderer is total under
  hand-built states (the save / scenario layers reject empty
  names in production paths). Every other SVG byte —
  viewBox, circle attributes, owner-driven palette,
  data-id (XML-escaped) / data-owner identity, insertion
  order, fixed-precision coords, LF terminators,
  header-only-on-empty — is byte-identical with M4.3.
  Artefact set unchanged (still 9). Save format unchanged
  (still v12). M1.17 / M2.22 / M3.7 byte-identical
  determinism contracts still pass by construction (same
  state → same labels → same bytes). 8 new doctest cases
  (7 svg_export + 1 runner; 792 total). **M4 in progress.**
  **No HTML viewer, no clickable UI, no event handlers, no
  hover state / tooltips, no legend / colour key, no
  font-family / font-size / fill on `<text>`, no label
  collision detection, no per-province label override, no
  rich text / multi-line labels, no ownership dynamics, no
  neighbour / adjacency edges, no terrain / resources /
  population overlays, no events, no AI, no command
  integration, no new `PlayerCommandKind`, no runner CLI
  flag, no new artefact (still 9), no save schema bump
  (still v12), no new state field, no new gameplay, no
  atomic `end_tick` writes.**
- Previously shipped: **M4.3 — SVG owner-color
  skeleton.** Replaces M4.2's hardcoded `fill="black"` with
  a deterministic per-owner palette lookup. New public
  symbols on `leviathan::systems::svg_export`:
  `kOwnerPalette` (10-entry `constexpr std::array<string_view,
  10>` of hex-RGB strings), `kOwnerPaletteSize`,
  `kOwnerFallbackFill` (`#888888`), and
  `color_for_owner(CountryId) → string_view`. The palette is
  indexed by `owner.value() % kOwnerPaletteSize` (modulo
  wraps; future-growth-by-appending preserves existing
  owner→colour mappings); negative owner returns the
  defensive fallback so the renderer is total under
  hand-built states even though the save / scenario layer
  rejects invalid owners. Canonical owners GER / FRA / JPN
  map to entries 0 / 1 / 2 (steel blue / indian red /
  goldenrod). Every other SVG attribute — viewBox, circle
  radius, `data-id` (still XML-escaped per M4.2 review fix),
  `data-owner`, insertion order, fixed-precision coords,
  LF terminators, header-only-on-empty — is byte-identical
  with M4.2. Artefact set unchanged (still 9; `provinces.svg`
  remains the only SVG). Save format unchanged (still v12).
  M1.17 / M2.22 / M3.7 byte-identical determinism contracts
  still pass by construction (same state → same colours →
  same bytes). 7 new doctest cases (6 svg_export + 1 runner;
  784 total). **M4 in progress.** **No HTML viewer, no
  clickable UI, no event handlers, no hover state / tooltips,
  no labels / text elements, no legend / colour key, no
  per-province colour override, no ownership dynamics, no
  neighbour / adjacency edges, no terrain / resources /
  population overlays, no events, no AI, no command
  integration, no new `PlayerCommandKind`, no runner CLI
  flag, no new artefact (still 9), no save schema bump
  (still v12), no new state field, no new gameplay, no
  atomic `end_tick` writes.**
- Previously shipped: **M4.2 — SVG exporter
  skeleton.** First renderer that turns the M4.1
  `ProvinceNode` data layer into pixels. New
  `leviathan::systems::svg_export` module with two free
  functions: `render_provinces(state) → std::string` (pure
  transform) and `write_provinces(state, path) → Result<bool>`
  (render + file write). Output is a deterministic SVG
  document with `viewBox="0 0 1000 1000"`, one `<circle>` per
  province at `cx = node.x * 1000`, `cy = node.y * 1000`,
  `r=8`, `fill="black"`, plus `data-id` / `data-owner`
  identity attributes; insertion order preserved (no sort);
  LF line terminators; `std::fixed` + `setprecision(2)` so
  output is byte-stable across platforms; empty
  `state.provinces` produces a header-only `<svg>`. `end_tick`
  writes `provinces.svg` **unconditionally** as the **9th
  artefact** (M3 / M4.1's 8-artefact invariant grew by one —
  per the M3.9 exit report §5 a 9th artefact requires its own
  sub-milestone with the contracts documented; M4.2 is that
  sub-milestone). `RunnerOptions::provinces_svg_path` is an
  optional override defaulting to
  `<output_dir>/provinces.svg`; **no CLI flag**.
  `RunOutcome::provinces_svg_path` carries the resolved path.
  M2.9 pre-`end_tick` no-artefact contract extends
  automatically; M3.6 mid-`end_tick` non-transactional caveat
  extends similarly (still a deferred item). Integration
  tests m1 / m2 / m3 byte-identical determinism contracts
  extended from 8 to 9 artefacts. Branch name carries
  explicit `rfc090-` prefix to disambiguate from the
  2026-05-17 rolled-back invented-M4.X governance work
  (lesson recorded in `docs/milestone-3-result.md` §7).
  12 new doctest cases (8 svg_export + 5 runner; one of the
  runner cases already counted toward the SVG writer's
  determinism evidence; 776 total). **M4 in progress.**
  **No HTML viewer, no clickable UI, no event handlers, no
  hover state, no map colours, no per-country palette, no
  ownership dynamics, no neighbour / adjacency edges, no
  controller-vs-owner split, no terrain / resources /
  population overlays, no labels / text elements, no events,
  no AI, no command integration, no new `PlayerCommandKind`,
  no runner CLI flag, no save schema bump (still v12), no
  new state field, no new gameplay, no atomic `end_tick`
  writes.**
- Previously shipped: **M4.1 — SVG map data
  skeleton.** Fleshes out what used to be the M0
  `ProvinceState{id, owner}` stub into a typed
  `core::ProvinceNode { id_code, name, owner, x, y }` with
  normalised `[0, 1]` map coordinates. `GameState::provinces`
  is now a typed vector that no system reads yet —
  M4.1 is **data only**; the future SVG exporter / UI
  consumes it. **Save format bumped v11 → v12**: the
  `provinces` array is required at the save layer (empty
  allowed), every entry validated (non-empty id_code + name,
  owner resolves into `state.countries`, x / y finite in
  `[0, 1]`, duplicate id_code rejected); v11 saves rejected
  loudly. Scenario loader gains an **optional** root-level
  `provinces` array of file paths pointing at per-file
  province manifests (`{ "provinces": [ {id, name, owner,
  x, y}, ... ] }`); manifests authored before M4.1 stay
  valid (missing key parses as empty); cross-file id_code
  uniqueness enforced. New canonical fixture
  `data/provinces/1930_core_nodes.json` ships three nodes
  (`berlin` / `paris` / `tokyo` owned by GER / FRA / JPN),
  wired into both canonical scenario manifests.
  `ScenarioLoadOutcome` gains `provinces_loaded`.
  `diagnostics::compare_states` now walks the provinces
  vector (size + per-field paths). 19 new doctest cases
  (8 save_system + 8 scenario_loader + 3 diagnostics; 764
  total). **M4 in progress.** **No SVG exporter, no HTML
  viewer, no clickable UI, no province rendering, no map
  colours, no ownership dynamics, no neighbour adjacency,
  no terrain / resources / population, no war / fronts /
  movement, no events, no AI, no command integration, no
  new `PlayerCommandKind`, no runner CLI flag, no new
  artefact (still 8), no CSV for provinces, no changes to
  M3 formulas, no changes to M2 command gates, no
  diplomacy, no M5 event-engine work.**
- Previously shipped: **M3.9 — M3 close-out.**
  Doc-only PR that publishes `docs/milestone-3-result.md`
  (M3 exit report), annotates
  `docs/milestone-3-checkpoint.md` as historical, and flips
  the three READMEs to "M3 closed". No new system, no new
  formula, no new artefact (still 8), no save schema bump
  (still v11), no new state field, no new
  `InterestGroupKind`, no new fixture, no new test, no
  `PlayerCommandKind`, no event, no log, no AI / UI / REPL
  / CLI surface, no command-gate change, no runner CLI
  flag, no atomic `end_tick` writes. **M3 closes here.**
  M3.7 / M3.8 integration tests continue to cover the
  loop / 8-artefact / canonical-data-row paths; no
  additional test was needed for the close-out itself.
  745 doctest cases unchanged.
- Previously shipped: **M3.8 — canonical scenario
  interest-group fixtures.** Data-only PR that adds one
  Bureaucracy interest group per canonical country
  (`ger_bureaucracy` / `fra_bureaucracy` / `jpn_bureaucracy`,
  each with `influence=0.55, loyalty=0.50, radicalism=0.10`)
  to `data/scenarios/1930_minimal.json` and
  `data/scenarios/1930_with_start_policies.json`. Up through
  M3.7 the M3 CSVs (`interest_groups.csv` /
  `interest_group_country_feedback.csv` /
  `interest_group_authority_pressure.csv`) were header-only on
  canonical-scenario runs; M3.8 takes the canonical path off
  the header-only branch so the three M3 CSVs now carry real
  data rows (9 / 3 / 3 rows in the 31-day canonical run,
  pinned by a new `runner_test` case). Bureaucracy was the
  only `InterestGroupKind` chosen so the fixture exercises
  all three reverse-direction systems (M3.2 react / M3.3
  country_feedback / M3.4 authority_pressure — the last
  reads only Bureaucracy-kind groups) without introducing
  any unimplemented gameplay. The canonical
  `scenario_loader` test gains six new assertions pinning
  the 3-group shape; the M1 / M2 integration tests'
  byte-identical determinism contracts are unchanged in
  shape — only the "canonical scenarios author zero
  interest groups" explanatory comments needed a refresh.
  **M3 remains in progress** — no
  `docs/milestone-3-result.md`, no "M3 closed" wording, no
  M4. **No new system, no new formula, no new artefact
  (still 8), no save schema bump (still v11), no loader
  semantic change, no auto-generation of interest groups,
  no new `InterestGroupKind`, no Military / Workers /
  Media / etc. groups yet, no `military_pressure` /
  `intelligence_pressure` / `media_pressure`, no event
  triggers, no command-gate diagnostic surface, no
  command-gate formula change, no AI / UI / REPL / CLI, no
  new `PlayerCommandKind`, no runner CLI flag, no atomic
  `end_tick` writes, no M3 close-out, no M4 / post-M3
  governance follow-up.** 1 new doctest case + 6 new
  asserts on an existing case (745 total).
- Previously shipped: **M3.7 — M3 reaction-loop
  integration checkpoint.** Pins the M3.1–M3.6 reaction loop
  at the seam between M3 and any future milestone via three
  new integration tests
  (`tests/integration/m3_end_to_end_test.cpp`) plus a short
  checkpoint doc (`docs/milestone-3-checkpoint.md`). The
  integration tests cover (1) one-month
  `monthly::tick_all_countries` driving every M3 leg
  (M3.2 react + M3.3 country_feedback + M3.4 authority_pressure)
  in a single call against a hand-built state, asserting each
  reverse-direction counter, every mutable field changed, and
  each trace vector got one row; (2) `runner::run_state`
  emitting the full 8-artefact set with actual M3 data rows
  (canonical M1.17 / M2 integration runs only ever exercised
  the header-only path because canonical scenarios author zero
  interest groups); (3) two byte-for-byte identical hand-built
  states producing byte-identical 8-artefact output (M1.17 /
  M2.22's determinism contract but with M3 mutation actually
  on the wire). The checkpoint doc records the current
  dataflow, the 8 artefacts, the invariants future
  sub-milestones must preserve, and the deferred items
  (events, AI, UI / REPL / CLI surfaces, atomic `end_tick`
  writes, M3 close-out, etc.) that intentionally did not ship
  yet. **M3 remains in progress** — no `docs/milestone-3-result.md`
  is written; M3.7 is a checkpoint, not an exit report. No new
  system, no new formula, no new artefact, no save schema
  bump (still v11), no new state field, no new
  `InterestGroupKind`, no new `PlayerCommandKind`, no events,
  no logs from interest groups, no AI / UI / REPL / CLI
  surface, no command gate formula change, no command-gate
  diagnostic surface, no runner CLI flag, no atomic
  `end_tick` write, no M4 / post-M3 governance follow-up.
- Previously shipped: **M3.6 — InterestGroup feedback outcome
  diagnostics / CSV trace surface.** Outcome-
  trace complement to M3.5's state surface. Two new
  unconditional CSVs join the artefact set:
  `interest_group_country_feedback.csv` (M3.3 outcome trace)
  and `interest_group_authority_pressure.csv` (M3.4 outcome
  trace). Ten columns each — `date`, `country_id`,
  `country_id_code`, `matched_groups`, `weight_sum`, the
  influence-weighted aggregate (`weighted_radicalism` /
  `weighted_bureaucracy_loyalty`), `target_*`, plus the
  `before` / `after` / `delta` triple for the single mutated
  field. Cadence is "one row per real mutation" — skipped
  countries produce no row; preflight failure produces no
  partial rows. New
  `interest_group::CountryFeedbackTraceRow` +
  `AuthorityPressureTraceRow` POD types;
  `country_feedback` / `authority_pressure` gain an optional
  `std::vector<...>* trace_out = nullptr` parameter (default-
  null = byte-identical with M3.3 / M3.4 baseline).
  `MonthlyOutcome` surfaces both trace vectors;
  `tick_all_countries` passes them through; `TickController`
  drains them in `step_one_day`. `end_tick` writes the two
  new CSVs unconditionally after `interest_groups.csv`,
  growing the artefact set 6 → 8. New diagnostics writers
  `write_country_feedback_csv_header / _row` +
  `write_authority_pressure_csv_header / _row` mirror the
  M3.5 csv_escape + scientific-precision conventions.
  `RunnerOptions` gains two optional path overrides; **no CLI
  flag**. `RunOutcome` gains two paths + two row counters.
  `main()` prints both. **Save format unchanged (still
  v11)**; only runtime artefact output grew. M1.17 / M2.22
  byte-identical determinism contracts extend from 6 → 8
  artefacts (canonical scenarios author zero interest groups
  so the three M3 files are all header-only). **M2.9
  pre-`end_tick` no-artefact contract** automatically
  extends to the 7th and 8th files because `end_tick` is
  still the only function that writes. 24 new doctest cases.
  No new gameplay, no new `PlayerCommandKind`, no new
  `InterestGroupKind` variants, no formula change to
  M3.2 / M3.3 / M3.4, no per-tick state delta CSV, no M3.2
  `react` per-mutation trace, no events / AI / UI / REPL,
  no new CLI flag, no command-gate integration, no atomic
  `end_tick` writes, no `--target-date` interaction beyond
  the existing replay flow.
- Previously shipped: **M3.5 — InterestGroup reaction
  diagnostics / CSV surface.** First M3 observability
  artefact. The runner now writes `interest_groups.csv`
  unconditionally on every run, on the same snapshot cadence
  as the existing CSVs (start + each `month_changed` + final
  post-sanity). Nine fixed columns:
  `date,id_code,name,kind,country_id,country_id_code,influence,
  loyalty,radicalism`. Vector-order preserved. Drive-by
  extracted the `InterestGroupKind` ↔ string mapping into
  shared `core/interest_group_kind.{hpp,cpp}`. **Save format
  unchanged (still v11).** Determinism contract grew 5 → 6
  artefacts. 24 doctest cases. See
  `docs/m3-5-interest-group-csv-surface.md` for the full
  design note.
- Previously shipped: **M3.4 — InterestGroup-derived
  authority pressure skeleton.** Opens the second reverse-
  direction channel: interest groups press not only on
  `country.stability` (M3.3) but also on
  `country.government_authority.bureaucratic_compliance` —
  the M2.18 `EnactPolicy` gate input. Bureaucracy-kind
  influence-weighted loyalty drives the formula at rate
  0.01, completing the rate ladder mood (0.05) → stability
  (0.02) → authority (0.01). **No save schema bump** (still
  v11). 19 doctest cases. See `docs/m3-4-interest-group-
  authority-pressure.md` for the full design note.
- Previously shipped: **M3.3 — InterestGroup country feedback
  skeleton.** Closes the M3 reaction loop: interest
  groups now push back on country state. Extends the M3.2
  `interest_group_system` module with a new constant
  `kInterestGroupCountryFeedbackRate = 0.02`,
  `CountryFeedbackOutcome { int countries_updated }`, and
  `country_feedback(state)` free function. For each country,
  computes an **influence-weighted radicalism**
  (`sum(group.influence * group.radicalism) / sum(group.influence)`
  across matching groups with `influence > 0`) and drifts
  `country.stability` toward `1.0 - weighted_radicalism` at
  rate 0.02, then clamps to `[0, 1]`. Countries with no
  matching groups or zero total influence are skipped (no
  mutation, not counted). The single output field is
  `country.stability`; `legitimacy`, `government_authority`,
  `corruption`, `central_control`, and `administrative_efficiency`
  are all untouched. The single input aggregate is
  influence-weighted radicalism; `loyalty` does not feed this
  step. Strict preflight: every `group.country`,
  `group.influence`, `group.radicalism`, and
  `country.stability` validated for finite + `[0, 1]` BEFORE
  any country mutates (atomicity across the list — a single
  NaN would otherwise poison `country.stability`). Wired into
  the monthly pipeline as the FINAL step of
  `tick_all_countries`, after M3.2's `react`, so it reads the
  just-updated `radicalism`. `MonthlyOutcome` gains
  `int interest_group_countries_updated`. The slower 0.02 rate
  (vs M3.2's 0.05) damps the closed loop. **No save schema
  bump** (still v11); existing M1 / M2 / M3.1 / M3.2 callers
  see byte-identical pipeline behaviour because canonical
  scenarios author zero interest groups. 14 new doctest cases.
  No events / AI / UI / command integration / new mutation
  targets / per-kind formulas / RNG / coup / strike / civil
  war / cross-border.
- Previously shipped: **M3.2 — InterestGroupReactionSystem
  skeleton.** First M3 system to **mutate** the M3.1 data
  layer. New module
  `leviathan::systems::interest_group` with constant
  `kInterestGroupReactionRate = 0.05`, `ReactionOutcome { int
  groups_updated }`, and `react(state)` free function. The
  reaction is a linear-toward-equilibrium drift driven by a
  single input (`country.stability`):
  `loyalty += (country.stability - loyalty) * 0.05` and
  `radicalism += ((1.0 - country.stability) - radicalism) * 0.05`,
  both clamped to `[0, 1]`. `influence`, `kind`, `country`,
  `id_code`, and `name` are untouched. `react` is pure (no logs,
  no RNG, no time advancement, no country / faction / policy
  mutation, no event emission) and preflight-validates every
  `group.country` against `state.countries` so a single bad
  entry leaves all other groups byte-identical (atomicity
  across the list). Wired into the monthly pipeline as the
  FINAL step of `tick_all_countries`, after every per-country
  `faction::react → stability::tick → economy::tick` finishes;
  the global step reads each country's post-tick stability.
  `MonthlyOutcome` gains `int interest_groups_updated` matching
  `ReactionOutcome::groups_updated`. **No save schema bump**
  (still v11); existing M1 / M2 callers that don't author
  interest groups see byte-identical pipeline behaviour. No
  events / AI / UI / command integration / country aggregate
  effects / influence drift / per-kind formulas / RNG / strikes
  / protests / coups / cross-border behaviour. 13 new doctest
  cases.
- Previously shipped: **M3.1 — InterestGroupState / political
  actors skeleton.** Opens M3 with a stripped-down data layer
  for political interest groups. New
  `core::InterestGroupKind` enum (10 variants: `Bureaucracy`,
  `Military`, `Workers`, `Farmers`, `Religious`, `Media`,
  `Students`, `LocalElites`, `Business`, `Technocrats`). New
  `core::InterestGroupState` POD with `id_code`, `name`, `kind`,
  `country` (numeric handle), and three `[0, 1]` behavioural
  ratios (`influence`, `loyalty` defaulting to 0.5;
  `radicalism` defaulting to 0.0). New
  `GameState::interest_groups` root-level vector so future
  cross-country interactions compose naturally; each entry's
  `country` field points back to the country it belongs to.
  **Save format bumped v10 → v11** with the block REQUIRED at
  the save layer (empty array allowed) and validated entry by
  entry — non-empty id_code + name, known kind string, country
  index resolving into `state.countries`, three ratios in
  `[0, 1]` via `require_ratio`, duplicate `id_code` rejected.
  `scenario_loader` accepts an optional `interest_groups`
  array in the manifest; missing key → empty vector;
  present-but-malformed rejected with `interest_groups[N]` and
  the offending field name in the error. Existing scenarios
  (`1930_minimal.json`, `1930_with_start_policies.json`) load
  unchanged. `diagnostics::compare_states` walks the array
  field-by-field under `interest_groups[N].*`. **Data only**:
  no M1 / M2 system reads or writes the new fields; M1
  monthly pipeline and M2 command path are byte-identical.
  Future M3 sub-milestones (reactions, command-resistance
  contributions, event triggers, AI) will consume the data.
  20 new doctest cases. No new gameplay; no new CLI flag; no
  new PlayerCommandKind; no new CSV column; no `state.logs`
  entry; no replay primitive change; no M1 / M2 system
  change.
- Previously shipped: **M2.22 — M2 exit / integration tests
  (closed M2).** Three end-to-end integration tests
  in `tests/integration/m2_end_to_end_test.cpp` pinning the
  M2 player-operation surface at the seam to M3+:
  (1) command script + replay + verify equivalence — drives
  `apply_command_script` on a source state, replays the resulting
  `applied_commands` log via `replay_with_time` into a target
  state, and asserts `diagnostics::compare_states` reports zero
  mismatches across every gameplay-relevant field;
  (2) order-execution gate atomicity across kinds — a mixed
  script (military AdjustBudget + EnactPolicy + welfare
  AdjustBudget) against a low-bureaucratic-compliance country
  with high military loyalty lands only the military entry,
  rejects the EnactPolicy at the gate, and leaves the welfare
  AdjustBudget unreached, with state / queue / applied_commands
  all consistent with per-command atomicity;
  (3) 5-artefact byte-identical determinism with M2 commands —
  M1.17's determinism contract (save.json + events.jsonl +
  summary.csv + countries.csv + factions.csv) extended through
  a 31-day run that applies commands at day 0 via
  `apply_command_script`. New `docs/milestone-2-result.md`
  exit report lists every sub-milestone, the architectural
  invariants future milestones must preserve, and the deferred
  items (Delayed / Distorted outcomes, scheduler, RNG-based
  resistance, attempted-command log, CLI script flag,
  runner-level rejection surface, expanded authority fields,
  authority drift, faction reactions, etc.) that consciously
  did not ship in M2. **M2 closes here.** Save format stays
  v10; no library behaviour change; no new gameplay; no save
  schema change. **Future player-operation work moves to M3+
  or to separate future milestones.**
- Previously shipped (M2 highlights): **M2.21** command script
  driver helper. **M2.20** command rejection reporting
  (`RejectionRecord` + `try_apply_pending`). **M2.18 / M2.19**
  EnactPolicy + AdjustBudget execution gates. **M2.17**
  OrderExecutionSystem skeleton. **M2.16**
  `GovernmentAuthorityState` (save schema v9 → v10). **M2.14**
  Replay target-date CLI. **M2.9** Replay CLI error-path
  hardening. **M2.13** Verify tolerance CLI. **M2.8 / M2.11 /
  M2.12** `--replay` / `--verify` / `--verify-strict` CLI
  family.
- Next milestone direction: **TBD — awaits explicit
  reviewer direction.** Per `milestone-4-result.md` §7,
  candidates include (a) **RFC-090 M5** (event engine —
  natural neighbour to the M3 reaction loop and would
  unlock the "interest-group thresholds trigger events"
  deferred item from M3); (b) **post-M4 viewer-polish
  follow-up** sourced from `milestone-4-result.md` §6
  category B (broader ARIA, pair-hover, position-aware
  tooltip, selection persistence, keyboard polish,
  mobile-only layouts, dark-mode); (c) **gameplay-domain
  category A items** (neighbour adjacency, terrain,
  ownership dynamics) under RFC-090 §M5 or §M6 numbering.
  M4.23 deliberately does NOT open or claim any of
  these. M5 starts in its own deliberate first
  sub-milestone PR when the reviewer says so.
  Historical context (M4.1–M4.23):
  M4.1–M4.4 shipped the SVG data → pixels pipeline; M4.5
  shipped the HTML viewer wrapper; M4.6 the minimal CSS;
  M4.7 the legend; M4.8 widened the SVG identity surface
  (four data-* attrs); M4.9 pinned the DOM contract via
  integration tests + checkpoint doc; M4.10 added the
  first JavaScript — a stateless click-handler details
  panel; M4.11 polished the panel's `<dt>` labels to
  fixed human-readable strings; M4.12 added a transient
  `.selected` class + CSS highlight on the clicked
  province pair; M4.13 widened the identity surface to
  five data-* attrs (added `data-owner-name`) and grew
  the details panel by one row; M4.14 refreshed the
  checkpoint doc to cover M4.10–M4.13 and added one new
  integration assertion; M4.15 added `tabindex="0"` on
  every `<circle>` + `<text>` and a keydown
  Enter/Space activation listener (keyboard-focus
  skeleton; no ARIA polish); M4.16 added the
  `:focus-visible` CSS rings so M4.15's keyboard focus
  is visible (blue ring distinct from M4.12 black
  `.selected` stroke); M4.17 added `role="button"` +
  `aria-label="<name>, <owner_name>"` so screen-reader
  users get an interactive-control announcement with a
  readable name; M4.19 added the `:hover` CSS rules so
  mouse users see a grey-stroke / underline affordance
  before they click; M4.20 added the `<p id="hover-status">`
  text bar that updates on `mouseover` with the composed
  `"<name> (<owner-name>)"` label; M4.21 added a viewport
  meta + one responsive `@media (max-width: 1040px)` rule
  so `map.html` renders usably on narrow / mobile screens;
  M4.18 refreshed the checkpoint doc to
  cover M4.15–M4.17 and added one new integration
  assertion; M4.22 added a close-out readiness
  assessment + consolidated integration test G "M4
  viewer contract complete" + fixed the PR #87 reviewer
  flag about the 1040px math wording; M4.23 published
  the M4 exit report (`docs/milestone-4-result.md`),
  annotated `docs/milestone-4-checkpoint.md` as
  historical, and flipped the three READMEs to "M4
  closed". M4 closes here.
- M0 closed. M1 closed. M2 closed. **M3 closed** with M3.1 +
  M3.2 + M3.3 + M3.4 + M3.5 + M3.6 + M3.7 + M3.8 + M3.9
  shipped. **M4 closed** with M4.1 + M4.2 + M4.3 +
  M4.4 + M4.5 + M4.6 + M4.7 + M4.8 + M4.9 + M4.10 + M4.11 + M4.12 + M4.13 + M4.14 + M4.15 + M4.16 + M4.17 + M4.18 + M4.19 + M4.20 + M4.21 + M4.22 + M4.23 shipped. See
  `docs/milestone-0-result.md`, `docs/milestone-1-result.md`,
  `docs/milestone-2-result.md`, and `docs/milestone-3-result.md`
  for the exit reports, `docs/milestone-4-checkpoint.md`
  for the M4-in-progress snapshot, and
  `rfc/RFC-090-roadmap.md` for the full milestone map.

`GameState` is a passive container. Systems shipped in M0:
`leviathan::systems::time` (date advance + boundary detection);
`leviathan::systems::random` (deterministic splitmix64 RNG, no
`<random>`); `leviathan::systems::logging` (explicit-only logging
with byte-stable JSONL); `leviathan::systems::data_loader` (JSON
config + country parsers via nlohmann/json);
`leviathan::systems::save_system` (JSON round-trip with `save_version`
/ `rng_algorithm_version` gates); `leviathan::systems::runner` (CLI
`leviathan --days N [--config ...] [--seed ...] [--output ...]
[--summary-csv ...]`); `leviathan::systems::diagnostics`
(observation-only `snapshot()` + summary CSV + `sanity_check()`).
Two runs with the same options produce byte-identical save, log,
and summary-CSV files. M0 closes with a full end-to-end integration
test (`tests/integration/m0_end_to_end_test.cpp`) that loads three
country JSON files, ticks 365 days, saves, loads back, and verifies
the round-trip.

**Milestone 1** (single-country internal politics prototype,
RFC-090 §M1) is complete; **Milestone 2** (player-operation
prototype, RFC-090 §M2) is also complete with M2.1–M2.22
merged; **Milestone 3** (internal politics / interest-group
reaction layer, RFC-090 §M3) is complete with M3.1 + M3.2
+ M3.3 + M3.4 + M3.5 + M3.6 + M3.7 + M3.8 + M3.9 shipped;
**Milestone 4** (SVG map + UI, RFC-090 §M4) is complete
with M4.1 + M4.2 + M4.3 + M4.4 + M4.5 + M4.6 + M4.7 + M4.8 + M4.9 + M4.10 + M4.11 + M4.12 + M4.13 + M4.14 + M4.15 + M4.16 + M4.17 + M4.18 + M4.19 + M4.20 + M4.21 + M4.22 + M4.23 shipped. Seventy sub-milestones shipped:
M1.1 CountryState fields; M1.2 FactionState; M1.3 BudgetState
(seven categories, no sum-to-1 enforcement); M1.4 PolicyData +
PolicyEffect; M1.5 PolicySystem `apply_policy_effects` (first real
gameplay effect, atomic via pre-flight); M1.6 FactionSystem `react`
(linear-toward-equilibrium loyalty / support drift); M1.7
StabilitySystem `tick` (first country-side dynamic, stripped-down
RFC-080 §5); M1.8 EconomySystem `tick` (RFC-080 §3 tax revenue,
expenditure = `gdp × sum_budget × 0.20`, stripped-down RFC-080 §4
GDP growth); M1.9 MonthlyPipeline `tick_country` /
`tick_all_countries` (first composition sub-milestone with canonical
order `faction::react → stability::tick → economy::tick`); M1.10
runner monthly pipeline wiring (every `month_changed` invokes
`monthly::tick_all_countries`; `run_state(state, opts)` exposed for
test injection); M1.11 scenario loader (`--scenario PATH` flag +
`scenario_loader::load_into_state` compose the M0.7 / M1.1 / M1.2
/ M1.4 parsers into a manifest-driven loader; `leviathan --days
365 --scenario data/scenarios/1930_minimal.json` produces a
non-empty world end-to-end without test-only injection); M1.12
economy → stability coupling (new `CountryState::last_gdp_growth_rate`
field, `economy::tick` writes it, `stability::tick` reads it as the
RFC-080 §5 `EconomicGrowth` term with `kEconomicGrowthWeight = 2.0`;
monthly pipeline order unchanged, intentional one-month lag;
save format bumped v5 → v6, first M1 save-schema bump); M1.13
scenario starting policies (manifest gains optional
`starting_policies` array of `{policy, actor}` id_code pairs;
loader applies each via `policy::apply_policy_effects` exactly
once at day 0, with the new fixture
`data/scenarios/1930_with_start_policies.json` enacting
`raise_taxes` + `increase_military_budget` on GER); M1.14
Diagnostics surfaces `last_gdp_growth_rate` — new
`CountrySummaryRow` + `country_snapshot` + per-country CSV writers,
plus opt-in `--countries-csv PATH` runner flag emitting 8 columns
per country per snapshot point (existing `--summary-csv` byte-for-
byte unchanged, M0.10 determinism contract preserved); **M1.15
Policy duration tracking — new `ActivePolicy{policy_id_code,
expires_on}` core type and `CountryState::active_policies` vector;
every successful `policy::apply_policy_effects` records one entry
with `expires_on = current_date + duration_days`; pre-flight
failure appends nothing. Save format bumped v6 → v7 (v6 saves
rejected loudly, v7 country without `active_policies` rejected).
Tracking-only: no expiration sweep, no revert, no scheduler.
`apply_policy_effects` also enforces a runtime cap on
`duration_days` (`kMaxTrackedPolicyDurationDays = 36500`, ~100
years) and rejects negatives, because `GameDate::advance_days` is
a per-day loop; **M1.16 Faction-level diagnostics CSV — new
`FactionSummaryRow` + `faction_snapshot` + per-faction CSV
writers, plus opt-in `--factions-csv PATH` runner flag emitting
9 columns per faction per snapshot point. Existing summary CSV
and per-country CSV both byte-for-byte unchanged (M0.10 + M1.14
contracts preserved). No save-format bump (still v7); **M1.17 M1
exit / integration tests — new
`tests/integration/m1_end_to_end_test.cpp` (1-year scenario run +
10-year soak run + 5-artefact byte-identical determinism), new
`docs/milestone-1-result.md` exit report, drive-by `main()`
milestone-label cleanup. No new system, no new flag, no save
schema change. M1 closes here; **M2.1 Player country selection —
new `GameState::player_country` (`CountryId`, default invalid),
new `--player COUNTRY_IDCODE` runner flag resolved after scenario
load. Save format bumped v7 → v8 (`"player_country"` required at
root; v7 rejected loudly). M1 systems unchanged: no behaviour
branches on `player_country` yet. Opens RFC-090 §M2; **M2.2 Pause
/ resume / step primitives — new `runner::TickController` runtime
struct (lives outside `GameState`) plus three free functions:
`begin_tick` / `step_one_day` / `end_tick`. `run_state` is
rewritten as a thin composition over them; M1.17's 5-artefact
byte-identical determinism contract is preserved (two new
equivalence tests pin it). Misuse rejected. No save format change
(still v8); no new CLI flag; no new logs. Drive-by: 2 regression
tests pinning that bad `--player` writes no on-disk artefacts;
**M2.3 Player command queue — new `core::PlayerCommand` (kind +
`policy_id_code`; M2.3 ships `EnactPolicy`) and new
`systems::commands::{CommandQueue, apply_pending}` module. Queue
is driver-owned (not in `GameState` or `TickController`).
`apply_pending` requires `state.player_country` to index into
`state.countries`, drains in insertion order, dispatches each
`EnactPolicy` through `policy::apply_policy_effects` (reusing
M1.5 atomicity + M1.15 active_policies tracking + duration cap).
Non-atomic across the list; first failure stops with failed cmd
at head. No save format change (still v8); no new flag / log / M1
system change; **M2.4 Player command log — new
`core::AppliedPlayerCommand{applied_on, command}` type and new
`GameState::applied_commands` vector. `commands::apply_pending`
appends one log entry per successful per-command dispatch (after
the M1.5 / M1.15 mutations land), so per-command atomicity covers
the log too; failed commands stay in the queue and do NOT log.
`applied_on` captures `state.current_date` at apply time. **Save
format bumped v8 → v9** with `"applied_commands"` as a required
root-level array; v8 saves rejected loudly; malformed entries
all rejected with `applied_commands[N]` in the error. Foundation
for future deterministic replay (RFC-050 §8). No M1 system
behaviour change; **M2.5 AdjustBudget player command — extends
`PlayerCommandKind` + `PlayerCommand` (new `budget_category` +
`budget_delta` fields). `apply_pending` gains a new switch arm
that validates the 7-category whitelist + finite delta, then
applies `budget.<category> += delta` and clamps to `[0, 1]`.
Per-command atomicity (M2.3) and log-on-success (M2.4) shared
unchanged. Save kind mapping grows to handle `"AdjustBudget"`
with per-kind JSON shape. No save format bump (still v9). Drive-
by: `player_command_kind_to_string` fallback now returns
`"UnknownPlayerCommandKind"` sentinel rather than a real kind
string, addressing the PR #32 reviewer nit; **M2.6 Replay applied
command log prototype — new `systems::commands::replay(state,
log)` free function. For each log entry forces
`state.current_date = applied_on`, builds a 1-element
`CommandQueue`, calls `apply_pending` (reusing M2.3 dispatch +
M2.4 log append + M1.5/M1.15 effect machinery unchanged).
Preconditions: `player_country` valid + `applied_commands`
empty. Atomicity across the log mirrors M2.3 mid-list-failure:
failed entry reported with `replay[N]: ...` in the error,
prior entries stay applied + logged, subsequent entries skipped.
Prototype limits pinned by tests: no time-system advancement
between commands, `current_date` ends at last entry, scenario
must be pre-loaded. No save format change; **M2.7 Replay with
time-system advancement — new
`systems::commands::replay_with_time(state, opts, ctrl, log)`
free function lifting M2.6's "no time advance" limit. For each
log entry, advances via M2.2 `step_one_day` until
`current_date == applied_on` (so M1.10 monthly pipeline runs on
boundaries naturally), then dispatches via 1-element queue +
`apply_pending`. Preconditions add `ctrl.started && !ctrl.ended`
and monotonic non-decreasing dates (addresses PR #34 nit).
Killer equivalence test pins that replay reproduces the original
simulation's state byte-for-byte (current_date, days_stepped,
monthly_ticks, log entries, command effects, and monthly-
pipeline-mutated fields). No save format change; **M2.8 Replay
CLI harness — `--replay PATH` runner flag wires M2.7 into the
CLI. `run()` branches: with `--replay` set (requires
`--scenario`), loads the save at PATH, optionally inherits
`player_country` from the loaded save when `--player` is unset,
runs `begin_tick → replay_with_time → end_tick`, and populates
a new `RunOutcome::replay_commands_replayed` field. `main()`
prints two extra summary lines. The CLI does NOT auto-compare —
user diffs source vs target save files. No save format change;
**M2.14 Replay target-date CLI — new `--target-date YYYY-MM-DD`
runner flag (requires `--replay`). Combines two effects: log
truncation (entries with `applied_on > target_date` are skipped
before `replay_with_time` runs) + post-replay time-system
extension (`step_one_day` loop until `current_date == target_date`,
so the M1.10 monthly pipeline fires on every month boundary
crossed). Parsed via `core::GameDate::parse` (rejects malformed
dates at parse time). Scenario-start precondition checked in
`run()` before any tick — pre-`end_tick` failure under the M2.9
contract, so bad target_date writes no artefacts. `main()` prints
`Target date: <value>` in the replay block when set.
`replay_with_time` and `step_one_day` semantics are unchanged;
M2.14 is glue. No save format change;
**M4.23 M4 exit / close-out — docs-only PR mirroring
M1.17 / M2.22 / M3.9 in shape. Publishes
`docs/milestone-4-result.md` (the M4 exit report, 7
sections: M4.1–M4.23 ledger / final dataflow /
10-artefact contract / save-format v12 floor /
architectural invariants every future milestone must
preserve / deferred items categorised A/B/C / neutral
next-milestone candidates). Annotates
`docs/milestone-4-checkpoint.md` as historical with a
top-of-file pointer to the exit report; keeps the
checkpoint body verbatim for archaeology. Flips this
README + `docs/README.md` + `rfc/README.md` to "M4
closed". **No code, no formula, no fixture, no test
change** (892 doctest cases / 61742 assertions
identical with M4.22; `provinces.svg` + `map.html`
bytes byte-identical with M4.22). The 2026-05-17
force-reset lesson (see `milestone-3-result.md` §7)
is the documented reason for "don't write the exit
report until the milestone is actually exiting" — a
previous attempt at M3.7+ drifted into premature
close-out + invented M4.X numbers + a 9th artefact
and was force-reset; the recovery pattern was a
dedicated final close-out PR that does nothing else.
M1.17 / M2.22 / M3.9 all followed that pattern;
M4.23 follows it for M4. **M4 closes here.** **No
"M5 in progress" wording** lands in this PR; M5 starts
in its own deliberate first sub-milestone PR when the
reviewer says so; M4.23 makes no claim about which
milestone is next. **No new system, no new formula,
no new artefact (still 10), no save schema bump (still
v12), no new state field / fixture /
`InterestGroupKind` / `PlayerCommandKind`, no renderer
behaviour change, no rename of any data-* attribute,
no change to `provinces.svg` or `map.html` bytes, no
new test (close-out PR is docs-only — M4.22 already
added integration test G as the consolidated "M4
viewer contract complete" pin).**;
**M4.22 close-out readiness checkpoint — mirrors M3.7's
role for the M3 reaction loop: docs + 1 integration
test, no renderer behaviour change. After M4.21 the M4
viewer stack is structurally complete; M4.22 formally
assesses readiness, locks the current contract with one
consolidated end-to-end integration test (test G "M4
viewer contract complete"), and fixes the PR #87
reviewer flag about the 1040px math wording (the
implementation always used 1040 = 1000 + 2*20; only the
doc text drifted to "1000 + 20*2 + 1*2 = 1042"; fixed
across svg_export.cpp, svg_export.hpp, READMEs, m4-21
design note). `docs/milestone-4-checkpoint.md` gains a
new section 9 "Close-out readiness assessment" with
four subsections (9.1 one-line summary of what M4
shipped; 9.2 still-deferred items categorised — A
defer-to-M5+ gameplay-domain, B recommended post-M4
follow-up polish, C not-needed-for-close nice-to-haves;
9.3 verdict — M4 is structurally ready; 9.4 the PR #87
wording fix). New integration test G touches every M4.x
surface marker (viewBox, circle, text, 5 data-* attrs,
tabindex, role, aria-label, details panel, legend,
click+keydown+mouseover+mouseout listeners,
hover-status, .selected CSS, :focus-visible, :hover,
fields-array labels, viewport meta, @media block) and
pins provinces.svg carries NONE of the HTML-wrapper
surfaces + all 7 unconditional artefacts present +
save_version: 12. **M4 remains in progress at end of
this PR** — no `docs/milestone-4-result.md`, no "M4
closed" wording. **The reviewer's next decision is one
of: (1) M4 close-out PR (publish exit report + flip
READMEs), (2) one more polish PR from category B
(broader ARIA, pair-hover, position-aware tooltip,
keyboard polish, selection persistence, mobile-only
layouts, dark-mode, CSS animations), or (3) stop M4
and move to M5. M4.22 does NOT pick.** Renderer bytes
byte-identical with M4.21 — the svg_export.cpp change
is comment-only. **Artefact set unchanged (still 10);
save format unchanged (still v12);** M1.17 / M2.22 /
M3.7 byte-identical determinism contracts continue to
pass. **1 new doctest case (892 total, 61742
assertions; verified via direct `leviathan_tests.exe`
run** per the `feedback_ctest_masks_doctest` rule).
**No new system / formula / artefact / state field /
fixture / `InterestGroupKind` / `PlayerCommandKind`,
no save schema bump, no new feature surface, no
rename of any data-* attribute, no renderer
behaviour change, no broader ARIA / pair-hover /
position-aware tooltip / keyboard polish / selection
persistence / hover delay / dark-mode (all
explicitly deferred per checkpoint section 9.2), no
change to `provinces.svg` or `map.html` bytes, no M4
close-out, no `docs/milestone-4-result.md`, no "M4
closed" wording.**;
**M4.21 responsive viewport skeleton — makes `map.html`
render usably on narrow / mobile screens. Two small
additions: (a) `<meta name="viewport"
content="width=device-width, initial-scale=1">` in
`<head>` (right after `<meta charset>`, before `<title>`)
— mobile browsers lay out at the device's actual width
instead of the default ~980px desktop emulation;
`initial-scale=1` disables auto-zoom-out; (b) one
`@media (max-width: 1040px)` rule in the `<style>`
block scaling the SVG to `width: 100%; max-width: 100%;
height: auto` so it fits the column without horizontal
scroll. The 1040px threshold is `1000 (SVG width) + 2 *
20 (body padding) = 1040px` (the SVG's 1px border lives inside the padded column, not adding to its layout width). Above 1040px the
M4.6 desktop rule (`margin: 0 auto`) wins; at and below
the @media rule wins. The existing `viewBox` preserves
the SVG aspect ratio for free under the percentage
width. Legend / details panel / hover-status bar inherit
the column via their existing `max-width: 1000px;
margin: 0 auto` — no per-element mobile rule. **Narrowly
reverses the M4.5–M4.20 "no `<meta viewport>`, no media
queries" non-goal** — only ONE viewport meta and ONE
@media block ship. Broader responsive surface (mobile-
only layouts, breakpoint cascade, container queries,
`prefers-*` features, responsive font sizing, JS
responsive surface — `matchMedia` / `ResizeObserver` /
`window.innerWidth` / `"resize"`) all stay deferred.
**Pure CSS** — no JS resize listener. Per
`feedback_checkpoint_drift`, `docs/milestone-4-checkpoint.md`
refreshed **inline** in this PR (`<head>` shape adds
viewport meta; `<style>` block adds @media rule;
selector-count wording becomes "20 plain selectors plus
one @media block"; VISUAL POLISH deferred bucket
rewrites; invariants section rewrites "no <meta
viewport>, no media queries" rule). M4.10/M4.11/M4.12/
M4.13/M4.15/M4.16/M4.17/M4.19/M4.20 invariants all
carry over unchanged (additive only). **M4 remains in
progress.** **Artefact set unchanged (still 10); save
format unchanged (still v12);** M1.17 / M2.22 / M3.7
byte-identical determinism contracts continue to pass.
`provinces.svg` bytes UNCHANGED from M4.20 (viewport +
@media live in `render_map_html` only); `map.html`
bytes did change (one new meta + one new @media block).
**5 new doctest cases (891 total, 61678 assertions;
verified via direct `leviathan_tests.exe` run** per the
`feedback_ctest_masks_doctest` rule). **No second
viewport meta / @media block, no `@container` /
`@supports`, no `min-width:` queries (only `max-width`),
no `orientation:` / `prefers-color-scheme` /
`prefers-reduced-motion`, no responsive font sizing
(`clamp()` / `vw` / `vh`), no JS responsive surface, no
mobile-only layout rules, no fluid font CSS, no CSS
animations / transitions, no `@import` / `@font-face`,
no `<link>` / external CSS / font, no inline event
attributes, no per-element inline `style="..."`, no
save schema bump, no new state field / artefact /
fixture / `InterestGroupKind` / `PlayerCommandKind`,
no rename of the M4.8 / M4.13 data-* keys, no second
`<script>`, no `<script src=>` / `<script type=>`, no
`fetch` / XHR / storage / history / navigation APIs,
no `innerHTML` / `outerHTML` / `document.write` /
`eval` / `Function` / `insertAdjacentHTML`, no broader
ARIA (still deferred), no state mutation, no commands,
no AI, no events, no selection persistence, no
adjacency / terrain / overlays, no runner CLI flag, no
change to `provinces.svg` bytes, no M4 close-out, no
`docs/milestone-4-result.md`, no "M4 closed" wording.**;
**M4.20 hover tooltip skeleton — adds a `<p
id="hover-status" class="hover-status">` text bar
between the inline SVG and the M4.10 details panel.
Inline `<script>` registers `mouseover` + `mouseout`
listeners on every province marker (same per-element
loop as click + keydown) that update the bar's
`textContent` to `"<name> (<owner-name>)"` (or
`<name>` alone when owner-name is empty) on mouseover
and clear it on mouseout. Reads via `getAttribute` on
M4.8/M4.13 `data-name` + `data-owner-name` (no new
attribute); writes via `textContent` only (XSS-safe).
One new `.hover-status` CSS rule. Tooltip design:
status-bar chosen over SVG `<title>` (would compete
with M4.17 aria-label as accessible name — reviewer
flagged) and over position-aware floating tooltip
(more scope than a skeleton). Narrowly reverses M4.19's
"no JS hover handler" non-goal — only `mouseover` +
`mouseout` via `addEventListener` ship; `mouseenter`
/ `mouseleave` / `.onmouseover` / `.onmouseout` /
inline `onmouseover=` / `onmouseout=` stay absent.
Hover and click are **independent surfaces** — the
mouseover handler does NOT call `showDetails` /
`selectProvince` / touch `details.` / use `.classList`
on hovered elements. No SVG `<title>` child anywhere
(M4.5 head `<title>` preserved as the only one in the
doc). Per `feedback_checkpoint_drift`,
`docs/milestone-4-checkpoint.md` refreshed **inline**
in this PR (selector count 19 → 20; HTML body shape
gains item 2 `<p id="hover-status">`; script section
gains the M4.20 listener block; HOVER+TOOLTIPS
deferred bucket rewrites). M4.10's XSS-safe DOM API,
no-network discipline, asymmetric one-inline-script
invariant, M4.12 `.selected`, M4.13 five-attr DOM
contract, M4.15 `tabindex` + keydown, M4.16
`:focus-visible`, M4.17 role/aria-label, M4.19
`:hover` CSS all carry over unchanged. **M4 remains
in progress.** **Artefact set unchanged (still 10);
save format unchanged (still v12);** M1.17 / M2.22 /
M3.7 byte-identical determinism contracts continue to
pass. `provinces.svg` bytes UNCHANGED from M4.19;
`map.html` bytes did change (new element + new CSS
rule + new listener types in script). 7 new + 1
retuned doctest cases (886 total, 61651 assertions;
**verified via direct `leviathan_tests.exe` run** per
the `feedback_ctest_masks_doctest` rule — ctest can
silently mask doctest CHECK failures). **No SVG
`<title>` child, no position-aware tooltip, no
pair-hover, no `mouseenter`/`mouseleave`, no hover
delay, no hover-driven panel preview, no `aria-live`
on the bar, no animation / transition, no broader
ARIA, no state mutation, no commands, no AI, no
events emitted by hover, no selection persistence, no
keyboard shortcut for the bar, no save schema bump,
no new state field / artefact / fixture /
`InterestGroupKind` / `PlayerCommandKind`, no rename
of the M4.8 / M4.13 data-* keys, no second
`<script>`, no `<script src=>` / `<script type=>`,
no `<link>`, no external CSS / font / `<iframe>` /
`<img>`, no `fetch` / XHR / storage / history /
navigation APIs, no `innerHTML` / `outerHTML` /
`document.write` / `eval` / `Function` /
`insertAdjacentHTML`, no inline event attributes, no
per-element inline `style="..."`, no `<meta
name="viewport">`, no CSS animations / transitions /
media queries / `@import` / `@font-face`, no
adjacency / terrain / overlays, no runner CLI flag,
no change to `provinces.svg` bytes, no M4 close-out,
no `docs/milestone-4-result.md`, no "M4 closed"
wording.**;
**M4.19 hover affordance skeleton — adds a mouse-hover
visual cue so users can see "this marker reacts to me"
before they click. Pure CSS — no JavaScript change, no
markup change beyond the `<style>` block. Two new CSS
rules in the M4.6 `<style>` block: `svg circle:hover {
stroke: #666666; stroke-width: 2; }` + `svg text:hover {
text-decoration: underline; }`. Placed AFTER the M4.10
`cursor: pointer` rule and BEFORE M4.12 `.selected` /
M4.16 `:focus-visible` so those state rules (equal
specificity, later in source order) win on the same
element. Three state colours stay distinct: hover grey
(`#666666`, 2px) / selected black (`#000000`, 3px) /
focused blue (`#1976d2`, 4px); the thicker stroke wins
when states stack. Text uses underline (different from
M4.12's `font-weight: bold` and M4.16's `outline`) so
layered text states stay readable. **Pure CSS — no JS
hover handler, no `mouseover`/`mouseout` listener, no
tooltip, no SVG `<title>` child element** (a `<title>`
child would compete with the M4.17 `aria-label` as the
accessible name). Pair-hover deferred (would need JS).
Per the new feedback rule,
`docs/milestone-4-checkpoint.md` is refreshed **inline**
in this PR (selector count 17 → 19; interactivity
surface section gets a `:hover` CSS bullet;
HOVER+TOOLTIPS deferred bucket rewrites). M4.10's
XSS-safe DOM API, no-network discipline, asymmetric
one-inline-script invariant, M4.12 `.selected`, M4.13
five-attr DOM contract, M4.15 `tabindex` + keydown
handler, M4.16 `:focus-visible` rings, M4.17
`role="button"` + `aria-label` all carry over unchanged
(additive only). **M4 remains in progress.** **Artefact
set unchanged (still 10); save format unchanged (still
v12);** M1.17 / M2.22 / M3.7 byte-identical determinism
contracts continue to pass. `provinces.svg` bytes
UNCHANGED from M4.17 (CSS is HTML-only); `map.html`
bytes did change (two new CSS rules). 5 new doctest
cases (885 total). **No JS hover handler, no
pair-hover, no tooltip, no `<title>` child, no
hover-driven preview / delay, no animation /
transition, no broader ARIA, no keyboard polish beyond
M4.15, no selection persistence, no save schema bump,
no new state field / artefact / fixture /
`InterestGroupKind` / `PlayerCommandKind`, no rename
of the M4.8 / M4.13 data-* keys, no second `<script>`,
no `<script src=>` / `<script type=>`, no `<link>`, no
external CSS / font / `<iframe>` / `<img>`, no `fetch`
/ XHR / storage / history / navigation APIs, no
`innerHTML` / `outerHTML` / `document.write` / `eval`
/ `Function`, no inline event attributes, no
per-element inline `style="..."`, no `<meta
name="viewport">`, no CSS animations / transitions /
media queries / `@import` / `@font-face`, no adjacency
/ terrain / overlays, no runner CLI flag, no change to
`provinces.svg` bytes, no M4 close-out, no
`docs/milestone-4-result.md`, no "M4 closed" wording.**;
**M4.18 accessibility checkpoint refresh — mirrors
M4.14's role for the M4 DOM contract: zero new
behaviour, just a refreshed status snapshot + one new
integration assertion. Refreshes
`docs/milestone-4-checkpoint.md` from its M4.14 scope
(M4.2–M4.13) to cover the three a11y surfaces shipped
in M4.15–M4.17: keyboard focus (`tabindex="0"` +
keydown Enter/Space listener), focus-visible CSS
(`#1976d2` blue rings via `:focus-visible`), and ARIA
labels (`role="button"` + `aria-label="<name>,
<owner_name>"`). The refreshed checkpoint now
enumerates 17 `<style>` selectors (was 13 at M4.14;
M4.16 added four `:focus`/`:focus-visible` rules), a
new "Accessibility surface (M4.15–M4.17)" section, and
a rebucketed deferred-items list (KEYBOARD+FOCUS
SHIPPED; BROADER ARIA + KEYBOARD POLISH still
deferred). Adds **one** new integration assertion
(`tests/integration/m4_dom_contract_test.cpp` test F):
the canonical scenario carries 6 `role="button"`
occurrences per artefact, 2 `aria-label="<name>,
<owner_name>"` per province per artefact, all 3
legend swatches carry NO `role` / `aria-label` /
`tabindex` (decorative invariant), M4.16
`:focus-visible` + `#1976d2` CSS appears in `map.html`
but NOT in `provinces.svg`, and the still-deferred
ARIA surface (aria-selected / aria-current /
aria-pressed / aria-live / aria-describedby /
aria-labelledby) stays absent end-to-end. End-to-end
mirror of the M4.15–M4.17 svg_export_test unit cases
through the runner / canonical fixture path.
**M4 remains in progress** — no
`docs/milestone-4-result.md`; M4.18 is a checkpoint
refresh, not an exit. Renderer bytes byte-identical
with M4.17 — only tests + docs ship. **Artefact set
unchanged (still 10); save format unchanged (still
v12);** M1.17 / M2.22 / M3.7 byte-identical
determinism contracts continue to pass. 2 new doctest
cases (880 total: 1 integration test F with multiple
sub-checks). **No new system / formula / artefact /
state field / fixture, no save schema bump, no new
`InterestGroupKind` / `PlayerCommandKind`, no new
feature surface, no rename of any data-* attribute,
no change to render_svg_root / render_map_html bytes,
no broader ARIA, no keyboard polish beyond M4.15, no
`<meta name="viewport">`, no CSS animations /
transitions / media queries, no adjacency / terrain /
overlays, no events / AI / commands, no hover /
tooltip, no selection persistence, no runner CLI
flag, no atomic `end_tick` writes, no M4 close-out,
no `docs/milestone-4-result.md`, no "M4 closed"
wording, no change to `provinces.svg` or `map.html`
bytes.**;
**M4.17 ARIA labels skeleton — makes the
M4.15-focusable / M4.10-clickable province markers
screen-reader-readable. Every `<circle>` and `<text>`
now carries `role="button"` + `aria-label="<name>,
<owner_name>"`. Label composed at render time: full
form `<name>, <owner_name>` when owner resolves;
fallback `<name>` (no trailing comma) when owner is
invalid. The composed string is XML-attribute-escaped
as a single value via the M4.2 helper. Same single
bounds check as `data-owner-code` / `data-owner-name`
gates the fallback. Both attrs go on both elements
(M4.8/M4.13 uniform-identity-surface pattern). Lives
in `render_svg_root` so `provinces.svg` picks up both
attrs too. Legend swatch `<circle>` elements stay
decorative (no `role`, no `aria-label`). **This
narrowly REVERSES the M4.15/M4.16 "no ARIA"
non-goal** — only `role="button"` + `aria-label` ship.
The broader still-deferred ARIA surface
(`aria-selected`, `aria-current`, `aria-pressed`,
`aria-live`, `aria-describedby`, `aria-labelledby`)
lands in a future dedicated A11Y sub-milestone.
M4.15/M4.16 unit tests retuned: the over-broad
`role=`/`aria-label=` absence checks become
narrower-ARIA-surface absence checks. `role="button"`
chosen specifically: matches the click + Enter/Space
activation model. `role="link"` rejected (the handler
doesn't navigate); `role="option"` rejected (would
need listbox + arrow-nav, out of scope); no role
rejected (would announce as plain graphic). aria-label
matches what the M4.10/M4.11 details panel renders
for `Province Name` / `Owner Name`, so sighted +
screen-reader users see / hear the same identity.
M4.10's XSS-safe DOM API, no-network discipline,
asymmetric one-inline-script invariant, M4.12
`.selected` surface, M4.13 five-attr DOM contract,
M4.15 `tabindex` + keydown handler, M4.16
`:focus-visible` rings all carry over unchanged.
**M4 remains in progress.** **Artefact set unchanged
(still 10); save format unchanged (still v12)** —
aria-label is composed from existing `ProvinceNode` +
`state.countries` fields, not a new persistent state
field. M1.17 / M2.22 / M3.7 byte-identical
determinism contracts continue to pass.
`provinces.svg` bytes DID change (two new attrs on
every `<circle>` + `<text>` — additive only);
`map.html` bytes did change (same SVG body). 7 new
doctest cases (878 total). **No `aria-selected`, no
`aria-current`, no `aria-pressed`, no `aria-live`, no
`aria-describedby`, no `aria-labelledby`, no `role`
other than `"button"`, no `<title>` / `<desc>` child
elements on markers, no state mutation, no commands,
no AI, no events, no selection persistence, no
tooltip, no hover, no animation, no keyboard shortcut
for the panel, no save schema bump, no new state
field / artefact / fixture / `InterestGroupKind` /
`PlayerCommandKind`, no rename of the M4.8 / M4.13
data-* keys, no second `<script>`, no `<script src=>`,
no `<script type=>`, no `<link>`, no external CSS /
font / `<iframe>` / `<img>`, no `fetch` / XHR /
storage / history / navigation APIs, no `innerHTML`
/ `outerHTML` / `document.write` / `eval` /
`Function`, no inline event attributes, no per-element
inline `style="..."`, no `<meta name="viewport">`, no
CSS animations / transitions / media queries /
`@import` / `@font-face`, no neighbour / adjacency /
terrain / overlays, no runner CLI flag, no M4
close-out, no `docs/milestone-4-result.md`, no "M4
closed" wording.**;
**M4.16 focus-visible styling skeleton — makes M4.15's
keyboard focus VISIBLE. Pure CSS — no JavaScript change,
no markup change beyond the `<style>` block. Four new CSS
rules in the M4.6 `<style>` block:
`svg circle:focus { outline: none; }` +
`svg circle:focus-visible { outline: none; stroke:
#1976d2; stroke-width: 4; }` +
`svg text:focus { outline: none; }` +
`svg text:focus-visible { outline: 2px solid #1976d2;
outline-offset: 2px; }`. The bare-`:focus` rules
suppress the browser's default focus outline; the
`:focus-visible` rings provide the M4.16 keyboard-focus
styling. Uses `:focus-visible` (NOT bare `:focus`) so
the ring appears for keyboard-triggered focus only —
mouse clicks paint only the M4.12 `.selected` black
stroke, keyboard Tab paints only the M4.16 `#1976d2`
ring, keyboard activate (Enter/Space) paints both
(correct: "this is selected" + "this is still focused").
Colour `#1976d2` chosen to contrast with the M4.3
owner palette and the M4.12 `#000000` `.selected`
stroke. Circle uses stroke-based ring (matches shape
outline); text uses CSS outline + outline-offset
(rectangular ring around bounding box). M4.10's
XSS-safe DOM API, no-network discipline, asymmetric
one-inline-script invariant, M4.12 `.selected`,
M4.13 five-attr DOM contract, M4.15 `tabindex` +
keydown handler all carry over unchanged. **Still NO
ARIA polish** — that lands in a future dedicated A11Y
sub-milestone. **M4 remains in progress.** **Artefact
set unchanged (still 10); save format unchanged (still
v12);** M1.17 / M2.22 / M3.7 byte-identical determinism
contracts continue to pass. `provinces.svg` bytes
UNCHANGED from M4.15 (focus CSS is HTML-only);
`map.html` bytes did change (four new CSS rules). 5
new doctest cases (871 total). **No state mutation, no
commands, no AI, no events, no selection persistence,
no tooltip, no hover, no animation / transition, no
`:focus-visible` polyfill, no save schema bump, no new
state field / artefact / fixture / `InterestGroupKind`
/ `PlayerCommandKind`, no rename of the M4.8 / M4.13
data-* keys, no second `<script>`, no `<script src=>`
/ `<script type=>`, no `<link>`, no external CSS /
font / `<iframe>` / `<img>`, no `fetch` / XHR /
storage / history / navigation APIs, no `innerHTML` /
`outerHTML` / `document.write` / `eval` / `Function`,
no inline event attributes, no per-element inline
`style="..."`, no `<meta name="viewport">`, no CSS
animations / transitions / media queries / `@import`
/ `@font-face`, no adjacency / terrain / overlays, no
runner CLI flag, no change to `provinces.svg` bytes,
no M4 close-out, no `docs/milestone-4-result.md`, no
"M4 closed" wording.**;
**M4.15 keyboard focus accessibility skeleton — first
keyboard-input surface for the M4 viewer. Every `<circle>`
and every `<text>` in the SVG body now carries
`tabindex="0"` (rendered in `render_svg_root`, so the
standalone `provinces.svg` picks it up too); the inline
`<script>` in `map.html` registers a `keydown` listener
alongside the existing `click` listener so pressing Enter
or Space while focused on a province fires the same
`selectProvince + showDetails` pair the click runs. The
keydown handler calls `event.preventDefault()` for Space
to suppress the browser's default page-scroll. The click
and keydown handlers share a per-element `activate()`
closure so the effect cannot drift between modalities.
The M4.7 legend swatch `<circle>` elements in `map.html`
are emitted separately (inside `render_map_html`, not in
`render_svg_root`) and lack `tabindex`, so they stay out
of the tab order — the M4.10 selector
`svg circle[data-id], svg text[data-id]` already skipped
them anyway. **Explicit non-goal: NO ARIA polish** — no
`role=`, `aria-label=`, `aria-selected=`, `aria-current=`,
`aria-pressed=`, no `:focus`/`:focus-visible` CSS, no
`tabindex` values other than `"0"`, no keyboard shortcut
for the panel, no focus management between renders. That
all lands in a future dedicated A11Y sub-milestone. M4.10's
XSS-safe DOM API, no-network discipline, asymmetric
one-inline-script invariant, M4.12's transient `.selected`
surface, and the M4.8 + M4.13 five-attr DOM contract all
carry over unchanged. **M4 remains in progress.**
**Artefact set unchanged (still 10); save format unchanged
(still v12)** — `tabindex` is render-time only, not a new
field on `ProvinceNode`. M1.17 / M2.22 / M3.7
byte-identical determinism contracts continue to pass.
`provinces.svg` bytes DID change (new `tabindex="0"` on
every `<circle>` + `<text>` — additive only); `map.html`
bytes did change (same SVG body + refactored listener
loop + new keydown wiring in the script). 9 new doctest
cases (866 total: 6 svg_export + 1 integration test E + 2
standalone-SVG / cross-checks). **No state mutation, no
commands, no AI, no events emitted by keyboard
activation, no selection persistence, no multi-select /
shift-Enter / right-click, no hover, no tooltip, no
animation, no save schema bump, no new state field /
artefact / fixture / `InterestGroupKind` /
`PlayerCommandKind`, no rename of the M4.8 / M4.13
data-* keys, no second `<script>`, no `<script src=>` /
`<script type=>`, no `<link>`, no external CSS / font /
`<iframe>` / `<img>`, no `fetch` / XHR / storage /
history / navigation APIs, no `innerHTML` / `outerHTML`
/ `document.write` / `eval` / `Function`, no inline
event attributes (`onkeydown=` / ...), no per-element
inline `style="..."`, no `<meta name="viewport">`, no
CSS animations / transitions / media queries / `@import`
/ `@font-face`, no adjacency / terrain / overlays, no
runner CLI flag, no M4 close-out, no
`docs/milestone-4-result.md`, no "M4 closed" wording.**;
**M4.14 DOM contract checkpoint refresh — mirrors M4.9's
role: zero new behaviour, just a refreshed status snapshot
+ one new integration assertion.
`docs/milestone-4-checkpoint.md` was originally written at
M4.9 to pin the M4.2–M4.8 SVG / HTML DOM contract. M4.14
refreshes it to cover what landed in M4.10 (first inline
`<script>` in `map.html` + asymmetric JS boundary), M4.11
(decoupled `<dt>` labels — `Province ID` / `Owner Index`
/ `Owner Code` / `Province Name` / `Owner Name` at M4.13),
M4.12 (transient `.selected` class + `circle.selected` /
`text.selected` CSS + `selectProvince(el)` helper —
purely DOM-level, no persistence), and M4.13 (fifth
`data-owner-name` attribute on every `<circle>` and
`<text>`, derived from `state.countries[owner].name`).
The refreshed checkpoint enumerates 13 CSS selectors in
the `<style>` block (was 6 at M4.9), an
interactivity-surface section listing `<div id="details">`
/ `.selected` / `selectProvince` / `showDetails` / the
5-entry `fields` array, and a reworked deferred-items
list bucketed into HOVER+TOOLTIPS / KEYBOARD+A11Y /
PERSISTENT SELECTION / DOM EXTENSIONS / VISUAL POLISH /
INFRASTRUCTURE. Adds **one** new integration assertion
(`tests/integration/m4_dom_contract_test.cpp` test D):
the canonical `map.html` script carries the M4.13-era
five-entry fields list — both the five raw `data-*`
attribute names and the five human-readable labels
appear verbatim inside the inline `<script>`;
`provinces.svg` carries none of the JS-literal forms.
End-to-end mirror of the M4.11/M4.13 svg_export_test
unit cases through the actual runner / canonical fixture
path. **M4 remains in progress** — no
`docs/milestone-4-result.md`; M4.14 is a checkpoint
refresh, not an exit. Renderer bytes byte-identical with
M4.13 — only tests + docs ship. **Artefact set unchanged
(still 10); save format unchanged (still v12);** M1.17 /
M2.22 / M3.7 byte-identical determinism contracts
continue to pass. 1 new doctest case (857 total). **No
new system / formula / artefact / state field /
fixture, no save schema bump, no new
`InterestGroupKind` / `PlayerCommandKind`, no new
feature surface, no rename of any data-* attribute, no
change to the click handler / details panel / `.selected`
CSS / fields array bytes, no `<meta name="viewport">`,
no CSS animations / transitions / media queries, no
adjacency / terrain / overlays, no events / AI /
commands, no hover / tooltip / keyboard nav / `aria-*`
polish, no selection persistence, no runner CLI flag, no
atomic `end_tick` writes, no M4 close-out, no
`docs/milestone-4-result.md`, no "M4 closed" wording, no
change to `provinces.svg` or `map.html` bytes.**;
**M4.13 details panel owner-name polish — widens the M4.8
identity surface by one attribute and the M4.11
details-panel `fields` array by one row. Every `<circle>`
and every `<text>` in the SVG body now also carries
`data-owner-name`, resolved from
`state.countries[owner.value()].name` (or `""` when the
owner index is invalid — same defensive fallback as M4.8
`data-owner-code`). A single bounds check covers both
lookups so they cannot disagree about validity; the value
is XML-attribute-escaped via the M4.2 helper. The M4.11
`fields` array grows from four to five entries — new row
`{ attr: "data-owner-name", label: "Owner Name" }` — so
the details panel now renders five dt/dd pairs. **Save
format stays v12** — `data-owner-name` is derived from
`state.countries` at render time, not a new field on
`ProvinceNode`. M4.10's XSS-safe DOM API, no-network
discipline, asymmetric one-inline-script invariant,
M4.12's transient `.selected` surface, and the M4.8 keys
themselves all carry over unchanged (additive only — no
rename). **M4 remains in progress.** **Artefact set
unchanged (still 10); save format unchanged (still v12);**
M1.17 / M2.22 / M3.7 byte-identical determinism contracts
continue to pass. `provinces.svg` bytes DID change (the
new attribute on every `<circle>` + `<text>` — additive
only); `map.html` bytes did change (same SVG body + the
new fifth `fields` entry). 8 new doctest cases (856
total). **No new field on `ProvinceNode`, no save schema
bump, no new state field / artefact / fixture /
`InterestGroupKind` / `PlayerCommandKind`, no rename of
the M4.8 data-* keys, no state mutation, no commands, no
AI, no events, no selection persistence, no multi-select
/ right-click, no hover, no tooltip, no keyboard nav /
`aria-*` polish, no animation, no second `<script>`, no
`<script src=>` / `<script type=>`, no `<link>`, no
external CSS / font / `<iframe>` / `<img>`, no `fetch` /
XHR / storage / history / navigation APIs, no `innerHTML`
/ `outerHTML` / `document.write` / `eval` / `Function`,
no `className` string concatenation, no `setAttribute(
"class", ...)`, no inline event attributes, no
per-element inline `style="..."`, no `<meta
name="viewport">`, no CSS animations / transitions /
media queries / `@import` / `@font-face`, no neighbour /
adjacency / terrain / overlays, no runner CLI flag, no
M4 close-out, no `docs/milestone-4-result.md`, no "M4
closed" wording.**;
**M4.12 clickable UI selected-state CSS skeleton — layers a
transient selection highlight on top of the M4.10/M4.11
click handler. Two new CSS rules in the M4.6 `<style>`
block: `svg circle.selected { stroke: #000000;
stroke-width: 3; }` and `svg text.selected { font-weight:
bold; }`. The click handler now also calls a
`selectProvince(el)` helper that clears every prior
`.selected` via `classList.remove("selected")` and adds
`.selected` via `classList.add("selected")` to every node
sharing the clicked element's `data-id` (clicking either
the `<circle>` or the `<text>` lights up the whole
province pair — fulfils the M4.8 design intent that "a
future clickable UI can address either element
uniformly"). Walks the pre-collected `nodes` NodeList
and compares `data-id` strings rather than re-entering a
CSS-selector parser at runtime. Initial render carries
**NO `class="selected"`** anywhere; the class only appears
at click time. **Selection is purely DOM-level**: never
written into `GameState`, never persisted across reloads,
no `localStorage` / `sessionStorage` / cookie / URL
fragment — a page reload always starts with nothing
selected. M4.10's XSS-safe DOM API, no-network discipline,
and asymmetric one-inline-script invariant all carry over
unchanged. The M4.8 `data-*` DOM contract on `<circle>` /
`<text>` is NOT renamed. **M4 remains in progress.**
**Artefact set unchanged (still 10); save format unchanged
(still v12);** M1.17 / M2.22 / M3.7 byte-identical
determinism contracts continue to pass. `provinces.svg`
bytes unchanged from M4.8; `map.html` bytes did change
(two new CSS rules + new helper + extended listener). 5
new doctest cases (848 total). **No state mutation, no
commands, no AI, no events emitted by the selection, no
selection persistence, no multi-select / right-click, no
hover, no tooltip, no keyboard nav / `aria-*` polish, no
animation, no save schema bump, no new state field /
artefact / fixture / `InterestGroupKind` /
`PlayerCommandKind`, no rename of the M4.8 data-* DOM
contract keys, no second `<script>`, no `<script src=>` /
`<script type=>`, no `<link>`, no external CSS / font /
`<iframe>` / `<img>`, no `fetch` / XHR / storage / history
/ navigation APIs, no `innerHTML` / `outerHTML` /
`document.write` / `eval` / `Function`, no `className`
string concatenation, no `setAttribute("class", ...)`, no
inline event attributes, no per-element inline
`style="..."`, no `<meta name="viewport">`, no CSS
animations / transitions / media queries / `@import` /
`@font-face`, no neighbour / adjacency / terrain /
overlays, no runner CLI flag, no change to `provinces.svg`
bytes, no M4 close-out, no `docs/milestone-4-result.md`,
no "M4 closed" wording.**;
**M4.11 clickable UI details labels polish — pure UX polish
on the M4.10 click handler. The `<dt>` labels rendered into
the `<div id="details">` panel are decoupled from the raw
`data-*` attribute names: `getAttribute` still reads the
M4.8 DOM contract keys (`data-id` / `data-owner` /
`data-owner-code` / `data-name` — NOT renamed; the
`<circle>` / `<text>` surface is byte-identical with M4.10),
but each `<dt>` renders a fixed human-readable label
(`Province ID` / `Owner Index` / `Owner Code` /
`Province Name`). One JS literal change: `var keys = [...]`
→ `var fields = [{attr, label}, ...]`. M4.10's XSS-safe DOM
API, no-storage / no-network discipline, and asymmetric
"exactly one inline `<script>` in `map.html`;
`provinces.svg` stays script-free" invariant all carry over
unchanged. **M4 remains in progress.** **Artefact set
unchanged (still 10); save format unchanged (still v12);**
M1.17 / M2.22 / M3.7 byte-identical determinism contracts
continue to pass. `provinces.svg` bytes unchanged from M4.8;
`map.html` bytes did change (four new label strings). 4 new
doctest cases (843 total). **No rename of the M4.8 data-*
DOM contract keys, no state mutation, no commands, no AI,
no events emitted by the click, no selection persistence,
no multi-select / right-click, no hover, no tooltip, no
keyboard nav / `aria-*` polish, no animation, no save
schema bump, no new state field / artefact / fixture /
`InterestGroupKind` / `PlayerCommandKind`, no second
`<script>`, no `<script src=>` / `<script type=>`, no
`<link>`, no external CSS / font / `<iframe>` / `<img>`,
no `fetch` / XHR / storage / history / navigation APIs,
no `innerHTML` / `outerHTML` / `document.write` / `eval`
/ `Function`, no inline event attributes, no per-element
inline `style="..."`, no `<meta name="viewport">`, no
CSS animations / transitions / media queries / `@import`
/ `@font-face`, no neighbour / adjacency / terrain /
overlays, no runner CLI flag, no change to `provinces.svg`
bytes, no M4 close-out, no `docs/milestone-4-result.md`,
no "M4 closed" wording.**;
**M4.10 HTML clickable UI skeleton — first JavaScript in
`map.html`. A single inline `<script>` IIFE at the end of
`<body>` attaches one `click` listener per `svg
circle[data-id], svg text[data-id]` element; the listener
reads the four M4.8 `data-*` attributes off the clicked
element via `getAttribute` and renders them as a `<dl>`
inside a new `<div id="details">` placeholder that sits
between the inline SVG and the M4.7 legend. Placeholder
starts with `<p class="details-empty">Click a province to
see its details.</p>`; the first click replaces the
placeholder, subsequent clicks replace the previous `<dl>`.
The handler is **stateless + XSS-safe**: it uses
`createElement` + `textContent` only (no `innerHTML` /
`outerHTML` / `document.write` / `eval` / `Function`),
and never calls `fetch` / `XMLHttpRequest` / `localStorage`
/ `sessionStorage` / `history.pushState` / `window.location`
/ `navigator`. The selector deliberately skips the M4.7
legend swatch `<circle>` elements (which have no `data-id`),
keeping the legend non-clickable. Four new CSS rules
(`.details` + dl/dt/dd + `.details-empty` + `svg
circle[data-id], svg text[data-id] { cursor: pointer; }`)
live in the M4.6 `<style>` block; **no per-element inline
`style="..."`** — M4.6 single-CSS-surface contract holds.
The JavaScript boundary is now **asymmetric**:
`provinces.svg` stays fully script-free; `map.html`
carries EXACTLY ONE inline script (no `src=`, no `type=`).
M4.9's integration test C splits to enforce the new
asymmetric invariant; M4.5/M4.6 "no `<script>`" unit test
retunes. **M4 remains in progress** — no
`docs/milestone-4-result.md`; M4.10 is one more skeleton
sub-milestone, not an exit. `provinces.svg` bytes
unchanged from M4.8; `map.html` bytes did change (new CSS
+ placeholder + script). **Artefact set unchanged (still
10); save format unchanged (still v12);** M1.17 / M2.22 /
M3.7 byte-identical determinism contracts continue to
pass. 8 new doctest cases (839 total: 7 svg_export + 1
runner). **No state mutation, no commands, no AI, no
events emitted by the click, no selection persistence, no
multi-select / right-click, no hover state, no tooltips,
no keyboard navigation / focus ring / `aria-*` polish, no
animation, no save schema bump, no new state field, no
new artefact, no new fixture, no new `InterestGroupKind`
/ `PlayerCommandKind`, no second `<script>`, no `<script
src=>`, no `<script type=>`, no `<link>`, no external CSS
/ font / `<iframe>` / `<img>`, no `fetch` /
`XMLHttpRequest` / `localStorage` / `sessionStorage` /
`history.pushState` / `window.location` / `navigator`, no
`innerHTML` / `outerHTML` / `document.write` / `eval` /
`Function`, no inline event attributes, no per-element
inline `style="..."`, no `<meta name="viewport">`, no CSS
animations / transitions / media queries / `@import` /
`@font-face`, no neighbour / adjacency edges, no terrain
/ resources / population overlays, no runner CLI flag, no
change to `provinces.svg` bytes, no M4 close-out, no
`docs/milestone-4-result.md`, no "M4 closed" wording.**;
**M4.9 HTML DOM contract checkpoint — mirrors M3.7's role
for the M3 reaction loop: zero new behaviour, just three
new integration tests (`tests/integration/m4_dom_contract_test.cpp`)
and a single-page snapshot (`docs/milestone-4-checkpoint.md`)
that formally pin the M4.2–M4.8 SVG / HTML DOM contract.
The three end-to-end cases: (1) every canonical province
surfaces all four `data-*` attributes on **both**
`<circle>` and `<text>` in **both** `provinces.svg` and
`map.html`; (2) the legend has one `<li data-owner="N">`
per `state.countries[i]` and each row carries its
country's `id_code`; (3) the no-interactivity invariant
holds — no `<script>`, no `<link>`, no inline event
attributes, no per-element `style="..."`, and
`provinces.svg` additionally has no `<style>` block at
all. The checkpoint doc lists every contract point
(artefact set, SVG body shape, HTML wrapper shape, the
identity surface DOM lookups, the invariants future M4.x
sub-milestones must preserve, the deferred items) in one
place. **M4 remains in progress** — no
`docs/milestone-4-result.md`; M4.9 is a checkpoint, not
an exit report. Renderer bytes unchanged from M4.8 — only
tests + docs ship. **Artefact set unchanged (still 10);
save format unchanged (still v12);** M1.17 / M2.22 / M3.7
byte-identical determinism contracts continue to pass.
3 new doctest cases (830 total). **M4 in progress.** **No
new system, no new formula, no new artefact, no save
schema bump, no new state field, no new fixture, no new
`InterestGroupKind` / `PlayerCommandKind`, no JavaScript,
no `<script>` / `<link>` / inline event attributes /
per-element inline `style="..."`, no
`<meta name="viewport">`, no CSS animations /
transitions / media queries / `@import` / `@font-face`,
no click handlers, no clickable UI, no hover state, no
tooltips, no state mutation, no neighbour / adjacency
edges, no terrain / resources / population overlays, no
events, no AI, no command integration, no runner CLI
flag, no atomic `end_tick` writes, no M4 close-out, no
`docs/milestone-4-result.md`, no "M4 closed" wording, no
change to `provinces.svg` or `map.html` bytes.**;
**M4.8 HTML static province data attributes skeleton —
widens the identity surface inside the SVG body: every
`<circle>` and every `<text>` now carries the same four
read-only `data-*` attributes (`data-id`, `data-owner`,
`data-owner-code`, `data-name`) so a future clickable UI /
DOM script can address either element uniformly without
DOM-walking siblings. `data-owner-code` resolves to
`state.countries[owner.value()].id_code` when valid, or `""`
otherwise (defensive fallback for hand-built states; save /
scenario layers reject invalid owners at load time).
`data-name` mirrors the `<text>` body content for uniform
programmatic lookup. All four data-* values
XML-attribute-escaped via the M4.2 helper. **First M4.x
sub-milestone since M4.4 that deliberately edits the
standalone SVG body** — the change is purely additive (no
removed attributes, no rendered-pixel movement); SVG-to-PNG
pipelines and vector tools see no visual difference. Both
`provinces.svg` and `map.html` pick up the new attributes
because they share the `render_svg_root` helper. All M4.5 /
M4.6 / M4.7 nots preserved: no JavaScript, no `<script>`,
no `<link>`, no inline event attributes, no inline
`style="..."` per element, no `<meta name="viewport">`, no
CSS animations / transitions / media queries / `@import` /
`@font-face`, no click handlers, no clickable UI, no hover
state, no tooltips, no state mutation. **Artefact set
unchanged (still 10); save format unchanged (still v12);**
M1.17 / M2.22 / M3.7 byte-identical determinism contracts
continue to pass by construction. 8 new doctest cases (7
svg_export + 1 runner; M4.4 empty-name test retuned to
anchor on the new stable surface; 827 total). **M4 in
progress.** **No JavaScript, no click handlers, no event
handlers / hover state / tooltips, no state mutation
(data-* are read-only), no `<script>` / no `<link>` / no
inline event attributes / no inline `style="..."` per
element, no `<meta name="viewport">`, no CSS animations /
transitions / media queries / `@import` / `@font-face`, no
new artefact (still 10), no save schema bump (still v12),
no new state field, no new `InterestGroupKind` /
`PlayerCommandKind`, no runner CLI flag, no events / AI /
command integration, no ownership-dynamics layer, no
neighbour / adjacency edges, no terrain / resources /
population overlays, no new gameplay, no atomic `end_tick`
writes, no per-province colour override, no new `<circle>`
/ `<text>` presentation attributes (the change is data-*
only; cx / cy / r / fill / x / y / text-anchor are
unchanged).**;
**M4.7 HTML legend skeleton — adds a static
`<ul class="legend">` to `map.html` immediately after the
inline SVG so a viewer can decode which palette colour
belongs to which country. One `<li data-owner="N">` per
`state.countries[i]`, in vector order; row content is a
tiny 16x16 inline SVG swatch (coloured via
`color_for_owner(CountryId{i})`) followed by `"id_code
&mdash; name"` text. Per-row swatch is inline SVG (not an
HTML `<span>` with `background-color`) so the M4.6 "no
inline `style="..."` per element" constraint stays
unbroken — `fill` on `<circle>` is an SVG presentation
attribute, not an HTML inline style. Three new CSS rules
(`.legend`, `.legend li`, `.legend .swatch`) join the M4.6
block for layout (list-style removal, centred `max-width:
1000px`, flex layout for swatch + text, fixed swatch size).
Legend text XML-text-escaped via the M4.4 helper. Empty
`state.countries` produces an empty `<ul>` (always-present
file contract preserved). **`provinces.svg` byte output
unchanged** — legend lives only in HTML wrapper;
standalone-SVG path stays CSS-free / legend-free for
downstream consumers. All M4.5 / M4.6 nots preserved: no
JavaScript, no `<script>`, no `<link>`, no inline event
attributes, no inline `style="..."` per element, no
`<meta name="viewport">`, no CSS animations / transitions
/ media queries / `@import` / `@font-face`. **Artefact set
unchanged (still 10); save format unchanged (still v12);**
M1.17 / M2.22 / M3.7 byte-identical determinism contracts
continue to pass by construction. 9 new doctest cases (8
svg_export + 1 runner; 819 total). **M4 in progress.**
**No JavaScript / `<script>`, no `<link>` external
stylesheet, no inline event attributes, no inline
`style="..."`, no `<meta name="viewport">`, no CSS
animations / transitions / media queries / `@import` /
`@font-face`, no click handlers, no clickable UI, no
hover state, no tooltips, no state mutation from the
viewer, no font-family / font-size on the SVG `<text>`
elements themselves, no ownership dynamics, no neighbour /
adjacency edges, no terrain / resources / population
overlays, no events, no AI, no command integration, no new
`PlayerCommandKind`, no runner CLI flag, no save schema
bump, no new state field, no new gameplay, no atomic
`end_tick` writes, no change to `provinces.svg` bytes.**;
**M4.6 HTML viewer minimal CSS skeleton — adds the smallest
possible inline `<style>` block to the M4.5 HTML wrapper.
Three CSS selectors: `body { margin: 0; padding: 20px;
background-color: #f0f0f0; }` (zero browser margin + small
breathing room + neutral grey page bg so the white SVG card
pops), `svg { display: block; margin: 0 auto; border: 1px
solid #888; background-color: #ffffff; }` (centre + bordered
"card"), `svg text { font-family: sans-serif; }` (fix the
browser's serif default for SVG `<text>`, which renders
small labels poorly). `<style>` sits at 2-space indent
inside `<head>` alongside `<meta>` + `<title>`.
**`provinces.svg` byte output unchanged** — CSS lives only
in the HTML wrapper; the standalone-SVG path stays CSS-free
for downstream consumers (SVG-to-PNG pipelines, vector
tools). All M4.5 nots preserved: no JavaScript, no
`<script>`, no `<link>`, no inline event attributes, no
`<meta name="viewport">`, no inline `style="..."` on
individual elements. M4.4 `<text>` font-family / font-size
contract preserved on the elements themselves; only the CSS
selector `svg text` sets the font and applies only to the
HTML viewer. **Artefact set unchanged (still 10); save
format unchanged (still v12);** M1.17 / M2.22 / M3.7
byte-identical determinism contracts continue to pass by
construction. 6 new doctest cases (5 svg_export + 1 runner;
M4.5 "no `<style>`" test retuned to drop its `<style>`
assertion + add an inline-`style=` check; 810 total).
**M4 in progress.** **No JavaScript / `<script>`, no
`<link>` external stylesheet, no inline event attributes,
no inline `style="..."`, no `<meta name="viewport">`, no
per-province / per-element CSS override, no CSS animations
/ transitions / media queries / `@import` / `@font-face`,
no click handlers, no clickable UI, no hover state, no
tooltips, no state mutation from the viewer, no legend /
colour key, no font-family / font-size on the `<text>`
elements themselves, no ownership dynamics, no neighbour /
adjacency edges, no terrain / resources / population
overlays, no events, no AI, no command integration, no
new `PlayerCommandKind`, no runner CLI flag, no save
schema bump, no new state field, no new gameplay, no
atomic `end_tick` writes, no change to `provinces.svg`
bytes.**;
**M4.5 HTML viewer skeleton — wraps the existing SVG body in
a minimal HTML5 document so the map opens cleanly in a
browser without the raw-XML chrome standalone `.svg` files
attract. New public functions on
`leviathan::systems::svg_export`:
`render_map_html(state) → std::string` and
`write_map_html(state, path) → Result<bool>`. Internal
refactor extracted a shared `render_svg_root` helper so
`render_provinces` continues to emit exactly the same bytes
(existing M4.x tests stay green without modification). HTML
shape: `<!DOCTYPE html>` + `<html lang="en">` + minimal
`<head>` (`<meta charset="UTF-8">` + `<title>Leviathan
Map</title>`) + `<body>` with inline `<svg>` body (no XML
prolog — invalid inside HTML). No CSS / no JavaScript / no
`<style>` / no `<script>` / no `<link>` / no inline event
handlers. Inline (not external-reference) embedding so the
file is self-contained — no `file://` vs `http://` CORS
pitfalls. `end_tick` writes `map.html` UNCONDITIONALLY as
the **10th artefact** (mirrors M3.5 / M3.6 / M4.2 pattern;
satisfies the M3-exit-report §5 "growing the set needs its
own sub-milestone" rule for the second time).
`RunnerOptions::map_html_path` optional override (no CLI
flag); default `<output_dir>/map.html`;
`RunOutcome::map_html_path` carries resolution. M2.9
pre-`end_tick` no-artefact contract extends automatically;
M3.6 mid-`end_tick` non-transactional caveat extends
similarly. M1.17 / M2.22 / M3.7 byte-identical determinism
contracts extended from 9 to 10 artefacts. **Artefact set
now 10; save format unchanged (still v12); `provinces.svg`
bytes unchanged from M4.4.** 12 new doctest cases (7
svg_export + 5 runner; 804 total). **M4 in progress.** **No
click handlers, no clickable UI, no event handlers, no
hover state, no tooltips, no state mutation from the
viewer, no legend / colour key, no CSS / JavaScript /
`<style>` / `<script>` / `<link>` / inline event
attributes, no `<meta name="viewport">`, no font-family /
font-size on `<text>`, no ownership dynamics, no neighbour /
adjacency edges, no terrain / resources / population
overlays, no events, no AI, no command integration, no new
`PlayerCommandKind`, no runner CLI flag, no save schema
bump, no new state field, no new gameplay, no atomic
`end_tick` writes.**;
**M4.4 SVG labels skeleton — adds one `<text>` per `<circle>`
in `provinces.svg`. Each label positioned at `(cx, cy +
kLabelYOffset)` with `text-anchor="middle"`, content set to
the XML-text-escaped `ProvinceNode::name`. New public
constant `kLabelYOffset = 22.0`; new `xml_text_escape` helper
(escapes `& < >` per XML 1.0 §2.4 text-content rules; sibling
to the M4.2 `xml_attr_escape` which also handles `" '` for
attribute contexts). `<circle>` and `<text>` interleaved per
node (one of each, in `state.provinces` order). No
`font-family` / `font-size` / `fill` on `<text>` — SVG
consumer default applies; typography deferred. Empty `name`
still emits an empty-bodied `<text>` so renderer is total
under hand-built states. Every other SVG byte (viewBox,
circle attributes, owner-driven palette, `data-id`
(XML-escaped) / `data-owner` identity, insertion order,
fixed-precision coords, LF terminators, header-only-on-empty)
byte-identical with M4.3. **Artefact set unchanged (still 9);
save format unchanged (still v12)**; M1.17 / M2.22 / M3.7
byte-identical determinism contracts still pass by
construction. 8 new doctest cases (7 svg_export + 1 runner;
792 total). **M4 in progress.** **No HTML viewer, no
clickable UI, no event handlers, no hover state / tooltips,
no legend / colour key, no font-family / font-size / fill on
`<text>`, no label collision detection, no per-province
label override, no rich text / multi-line labels, no
ownership dynamics, no neighbour / adjacency edges, no
terrain / resources / population overlays, no events, no AI,
no command integration, no new `PlayerCommandKind`, no
runner CLI flag, no new artefact (still 9), no save schema
bump (still v12), no new state field, no new gameplay, no
atomic `end_tick` writes.**;
**M4.3 SVG owner-color skeleton — replaces M4.2's hardcoded
`fill="black"` with a deterministic per-owner palette
lookup. New public symbols on
`leviathan::systems::svg_export`: `kOwnerPalette` (10-entry
`constexpr std::array<string_view, 10>` of hex-RGB strings),
`kOwnerPaletteSize`, `kOwnerFallbackFill` (`#888888`), and
`color_for_owner(CountryId) → string_view`. Palette indexed
by `owner.value() % kOwnerPaletteSize` (modulo wraps; future
growth-by-appending preserves existing owner→colour mappings);
negative owner returns the defensive fallback so the renderer
is total under hand-built states even though the save /
scenario layer rejects invalid owners. Canonical owners GER /
FRA / JPN map to entries 0 / 1 / 2 (steel blue / indian red /
goldenrod). Every other SVG attribute — viewBox, circle
radius, `data-id` (still XML-escaped per M4.2 review fix),
`data-owner`, insertion order, fixed-precision coords, LF
terminators, header-only-on-empty — is byte-identical with
M4.2. Artefact set unchanged (still 9). Save format unchanged
(still v12). M1.17 / M2.22 / M3.7 byte-identical determinism
contracts still pass by construction (same state → same
colours → same bytes). 7 new doctest cases (6 svg_export +
1 runner; 784 total). **M4 in progress.** **No HTML viewer,
no clickable UI, no event handlers, no hover state /
tooltips, no labels / text elements, no legend / colour
key, no per-province colour override, no ownership dynamics,
no neighbour / adjacency edges, no terrain / resources /
population overlays, no events, no AI, no command
integration, no new `PlayerCommandKind`, no runner CLI flag,
no new artefact (still 9), no save schema bump (still v12),
no new state field, no new gameplay, no atomic `end_tick`
writes.**;
**M4.2 SVG exporter skeleton — first renderer that turns the
M4.1 `ProvinceNode` data layer into pixels. New
`leviathan::systems::svg_export` module with two free
functions: `render_provinces(state) → std::string` (pure
transform) and `write_provinces(state, path) → Result<bool>`
(render + file write). Output is a deterministic SVG document
with `viewBox="0 0 1000 1000"`, one `<circle>` per province
at `cx = node.x * 1000`, `cy = node.y * 1000`, `r=8`,
`fill="black"`, plus `data-id` / `data-owner` identity
attributes; insertion order preserved; LF terminators;
`std::fixed` + `setprecision(2)` for byte-stable coords;
empty `state.provinces` produces a header-only `<svg>`.
`end_tick` writes `provinces.svg` UNCONDITIONALLY as the
**9th artefact**, mirroring the M3.5 / M3.6
unconditional-artefact shape; per the M3.9 exit report §5,
adding a 9th artefact requires its own sub-milestone with
the contracts documented — M4.2 is that sub-milestone.
`RunnerOptions::provinces_svg_path` optional override (no
CLI flag); default `<output_dir>/provinces.svg`;
`RunOutcome::provinces_svg_path` carries resolution.
M2.9 pre-`end_tick` no-artefact contract extends
automatically; M3.6 mid-`end_tick` non-transactional caveat
extends similarly. Integration tests m1 / m2 / m3
byte-identical determinism contracts extended from 8 to 9
artefacts. Branch name carries explicit `rfc090-` prefix to
disambiguate from the rolled-back invented-M4.X work.
12 new doctest cases (8 svg_export + 5 runner; one of the
runner cases doubles as determinism evidence; 776 total).
**M4 in progress.** **No HTML viewer, no clickable UI, no
event handlers, no hover state / tooltips, no map colours,
no per-country palette, no ownership-driven colour mapping,
no ownership dynamics, no neighbour / adjacency edges, no
controller-vs-owner split, no terrain / resources /
population overlays, no labels / text elements, no events,
no AI, no command integration, no new `PlayerCommandKind`,
no runner CLI flag, no save schema bump (still v12), no
new state field, no new gameplay, no atomic `end_tick`
writes.**;
**M4.1 SVG map data skeleton — opens RFC-090 §M4 (SVG map +
UI). Replaces the dead M0 `ProvinceState{id, owner}` stub with
a typed `core::ProvinceNode { id_code, name, owner, x, y }`
where `x` / `y` are normalised `[0, 1]` map coordinates;
`GameState::provinces` is now a typed vector but no system
reads it yet — M4.1 is data only, the future SVG exporter / UI
consumes it. **Save format bumped v11 → v12**: the `provinces`
array is REQUIRED at the save layer (empty allowed) with every
entry validated (non-empty id_code + name, owner resolving
into `state.countries`, x / y finite in `[0, 1]`, duplicate
id_code rejected); v11 saves rejected loudly. Scenario loader
gains an OPTIONAL root-level `provinces` array of file paths
pointing at per-file province manifests (`{ "provinces": [
{id, name, owner, x, y}, ... ] }`); manifests authored before
M4.1 stay valid (missing key parses as empty); cross-file
id_code uniqueness enforced. New canonical fixture
`data/provinces/1930_core_nodes.json` ships three nodes
(berlin / paris / tokyo owned by GER / FRA / JPN), wired into
both canonical scenario manifests. `ScenarioLoadOutcome` gains
`provinces_loaded`. `diagnostics::compare_states` walks the
provinces vector (size + per-field paths `provinces[N].
{id_code,name,owner,x,y}`). 19 new doctest cases
(8 save_system + 8 scenario_loader + 3 diagnostics; 764 total).
**M4 in progress.** **No SVG exporter, no HTML viewer, no
clickable UI, no province rendering, no map colours, no
ownership dynamics, no neighbour adjacency, no terrain /
resources / population, no war / fronts / movement, no events,
no AI, no command integration, no new `PlayerCommandKind`,
no runner CLI flag, no new artefact (still 8), no CSV for
provinces, no changes to M3 formulas, no changes to M2
command gates, no diplomacy, no M5 event-engine work.**;
**M3.9 M3 close-out — doc-only PR that publishes
`docs/milestone-3-result.md` (the M3 exit report:
nine-row sub-milestone ledger, final dataflow, eight-artefact
contract, save-format v11 floor, architectural invariants
every future milestone must preserve, deferred items, and
neutral next-milestone candidates). Annotates
`docs/milestone-3-checkpoint.md` with a "historical
checkpoint" top note pointing to the exit report; keeps the
rest of the checkpoint doc for archaeology. Flips all three
READMEs to "M3 closed" / "Latest shipped: M3.9" / "Next
milestone: TBD — requires explicit reviewer direction". **M3
closes here.** **No new system, no new formula, no new
artefact (still 8), no save schema bump (still v11), no
new state field, no new `InterestGroupKind`, no new fixture,
no new test, no `PlayerCommandKind`, no event, no log, no
AI / UI / REPL / CLI surface, no command-gate change, no
runner CLI flag, no atomic `end_tick` writes, no M4, no
post-M3 governance follow-up wording.** Tests unchanged
(745 doctest cases); M3.7's integration tests and M3.8's
canonical-data-row test already cover the loop, 8-artefact,
and canonical-data-row paths. **Future player-operation or
gameplay work moves to a separate future milestone** (M4 /
M5 / non-RFC-numbered follow-up — reviewer chooses);
**M3.9 makes no claim about which.**
**M3.8 canonical scenario interest-group fixtures — adds one
Bureaucracy interest group per canonical country
(`ger_bureaucracy` / `fra_bureaucracy` / `jpn_bureaucracy`,
each `influence=0.55, loyalty=0.50, radicalism=0.10`) to
`data/scenarios/1930_minimal.json` and
`data/scenarios/1930_with_start_policies.json`. Up through
M3.7 the three M3 CSVs (`interest_groups.csv` /
`interest_group_country_feedback.csv` /
`interest_group_authority_pressure.csv`) were header-only on
canonical-scenario runs; M3.8 takes the canonical path off the
header-only branch so a 31-day canonical run now produces
9 / 3 / 3 rows respectively. Bureaucracy is the only kind
chosen because the M3.4 `authority_pressure` step reads only
Bureaucracy-kind groups, so a single Bureaucracy group per
country exercises all three reverse-direction systems without
introducing any unimplemented gameplay. Canonical scenario
loader test gains 6 new asserts pinning the 3-group shape;
M1.17 / M2.22 byte-identical determinism contracts unchanged
in shape (only the "canonical scenarios author zero interest
groups" comments needed a refresh). **No new system, no new
formula, no new artefact (still 8), no save schema bump
(still v11), no loader semantic change, no auto-generation,
no new `InterestGroupKind`, no Military / Workers / etc.
groups yet, no `military_pressure` / `intelligence_pressure`
/ `media_pressure`, no event triggers, no command-gate
diagnostic surface, no command-gate formula change, no AI /
UI / REPL / CLI, no new `PlayerCommandKind`, no runner CLI
flag, no atomic `end_tick` writes, no M3 close-out, no M4.**
1 new doctest case + 6 new asserts on an existing case (745
total);
**M3.7 M3 reaction-loop integration checkpoint — pins the
M3.1–M3.6 closed loop at the seam between M3 and any future
milestone. Adds `tests/integration/m3_end_to_end_test.cpp`
with three cases: (1) one-month `monthly::tick_all_countries`
fires M3.2 react + M3.3 country_feedback + M3.4
authority_pressure in a single call against a hand-built
one-country / one-Bureaucracy-group state, asserting every
reverse-direction counter, every mutable field actually
changed, and each trace vector got one row whose post-mutation
value matches the live state field; (2) `runner::run_state`
emits all eight artefacts with actual M3 data rows
(`interest_groups.csv` with three snapshot rows, both M3.6
trace CSVs with one row each); (3) two byte-for-byte identical
hand-built states through `run_state` produce byte-identical
eight artefacts (M1.17 / M2.22 determinism contract extended
to the M3-mutation path, not just the canonical-scenario
header-only path). Adds `docs/milestone-3-checkpoint.md`
documenting the current dataflow, the eight artefacts, the
invariants future sub-milestones must preserve, and the
deferred items (events, AI, UI / REPL / CLI surfaces, atomic
`end_tick` writes, M3 close-out, etc.) that intentionally
did not ship yet. **M3 remains in progress** — no exit
report, no "M3 closed" wording, no M4. **No new system, no
new formula, no new artefact, no save schema bump (still
v11), no new state field, no new `InterestGroupKind`, no new
`PlayerCommandKind`, no events, no logs from interest
groups, no AI / UI / REPL / CLI surface, no command gate
formula change, no command-gate diagnostic surface, no
runner CLI flag, no atomic `end_tick` writes.** 3 new
integration doctest cases (744 total);
**M3.6 InterestGroup feedback outcome diagnostics / CSV trace
surface — outcome-trace complement to M3.5's state surface.
Two new unconditional CSVs:
`interest_group_country_feedback.csv` (M3.3 outcomes) and
`interest_group_authority_pressure.csv` (M3.4 outcomes), ten
columns each. Cadence: one row per real per-country
mutation. New `interest_group::CountryFeedbackTraceRow` +
`AuthorityPressureTraceRow` POD types; `country_feedback` /
`authority_pressure` gain optional
`std::vector<...>* trace_out = nullptr` arg (default-null =
M3.3 / M3.4 baseline). `MonthlyOutcome` surfaces the trace
vectors; `TickController` drains them in `step_one_day`. New
diagnostics writers
`write_country_feedback_csv_header / _row` +
`write_authority_pressure_csv_header / _row` reuse the M3.5
`csv_escape` / scientific-precision conventions.
`RunnerOptions` gains two optional path overrides; **no CLI
flag**. `RunOutcome` gains two paths + two row counters.
**No save schema bump (still v11), no formula change to M3.2
/ M3.3 / M3.4, no new gameplay.** Determinism contract grows
6 → 8 byte-identical artefacts; M2.9 pre-`end_tick`
no-artefact contract automatically extends to the 7th and
8th files. 24 new doctest cases. No events / AI / UI / REPL /
new `PlayerCommandKind` / new `InterestGroupKind` variants /
M3.2 `react` per-mutation trace / per-tick state delta CSV /
command-gate integration / atomic `end_tick` writes /
`--target-date` behaviour change;
**M3.5 InterestGroup reaction diagnostics / CSV surface —
first M3 observability artefact. `interest_groups.csv`
written unconditionally by `end_tick` alongside `save.json`
/ `events.jsonl` / the three opt-in CSVs, on the same
snapshot cadence (start + each `month_changed` + final
post-sanity). Nine fixed columns:
`date,id_code,name,kind,country_id,country_id_code,
influence,loyalty,radicalism`. Vector-order preserved (no
sorting). New `diagnostics::InterestGroupSummaryRow` +
`interest_group_snapshot` + `write_interest_group_csv_*`
helpers + tiny `csv_escape` (RFC 4180). Invalid
`group.country` rejected loudly at snapshot time. Empty
`state.interest_groups` still produces a header-only file.
**No `--interest-groups-csv` CLI flag** — the
`RunnerOptions::interest_groups_csv_path` programmatic
override defaults to `<output_dir>/interest_groups.csv`.
`RunOutcome` gains `interest_groups_csv_path` +
`interest_groups_csv_rows`. Drive-by extracted the
`InterestGroupKind` ↔ string mapping (previously duplicated
in `save_system.cpp` + `scenario_loader.cpp`) into shared
`core/interest_group_kind.{hpp,cpp}` — one source of truth
for save / scenario / diagnostics. **No save schema bump
(still v11)**; M1.17 / M2.22 byte-identical determinism
contracts extend 5 → 6 artefacts; M2.9 pre-`end_tick`
no-artefact contract automatically extends to the sixth
file. 24 new doctest cases. No new gameplay / events / AI /
UI / REPL / CLI flag / new `PlayerCommandKind` / new
`InterestGroupKind` variants / formula change / per-tick
delta CSV / formula-trace CSV / command-gate integration /
atomic `end_tick` writes;
**M3.4 InterestGroup-derived authority pressure skeleton —
opens the second reverse-direction channel: interest groups
press on `country.government_authority.bureaucratic_compliance`.
Extends `interest_group_system` with
`kInterestGroupAuthorityPressureRate = 0.01`,
`AuthorityPressureOutcome { countries_updated }`, and
`authority_pressure(state)` free function. For each country,
computes influence-weighted loyalty over **Bureaucracy-kind**
groups only and drifts `bureaucratic_compliance` toward that
target at rate 0.01, clamping `[0, 1]`. Countries with no
Bureaucracy groups or zero total Bureaucracy influence are
skipped. Mutation surface restricted to `bureaucratic_compliance`:
the other three authority sub-fields, plus country stability /
legitimacy / corruption, are byte-identical. Strict preflight
on inputs actually read (group.country / influence / loyalty /
country.bureaucratic_compliance, all finite + `[0, 1]`). Wired
into `tick_all_countries` as the THIRD global step, AFTER
M3.3's `country_feedback`, completing the rate ladder mood
(0.05) → stability (0.02) → authority (0.01). `MonthlyOutcome`
gains `int interest_group_authority_countries_updated`. 19
new doctest cases. **No save schema bump** (still v11). The
M2.18 EnactPolicy gate is now a downstream consumer of the
loop but M3.4 does NOT change the gate formula;
**M3.3 InterestGroup country feedback skeleton — closes the
M3 reaction loop: interest groups push back on country state.
Extends the M3.2 `interest_group_system` module with constant
`kInterestGroupCountryFeedbackRate = 0.02`,
`CountryFeedbackOutcome { int countries_updated }`, and
`country_feedback(state)` free function. Per country computes
influence-weighted `radicalism` aggregate
(`sum(g.influence * g.radicalism) / sum(g.influence)` over
groups with `influence > 0`) and drifts `country.stability`
toward `1.0 - weighted_radicalism` at rate 0.02, clamping to
`[0, 1]`. Countries with no matching groups or zero total
influence skipped. Only `country.stability` mutates;
`legitimacy` / `government_authority` / `corruption` /
`central_control` / `administrative_efficiency` all untouched.
Only influence-weighted radicalism feeds the formula — loyalty
is not consulted. Strict preflight validates every group +
country before any mutation (atomicity guards against NaN
poisoning). Wired into the monthly pipeline as the FINAL step
of `tick_all_countries`, after M3.2's `react`, so it reads
just-updated radicalism. `MonthlyOutcome` gains
`int interest_group_countries_updated`. Slower 0.02 rate (vs
M3.2's 0.05) damps the closed loop. No save schema bump (still
v11). 14 new doctest cases. Drive-by: M3.2's monthly-pipeline
integration test's exact-arithmetic assertion (which would
have duplicated the M3.3 formula) demoted to a directional
check; the new M3.3 monthly-pipeline test pins the layered
behaviour. Existing M1 / M2 fixtures unaffected — canonical
scenarios author zero interest groups so M3.3 finds no
matching groups per country and skips all of them;
**M3.2 InterestGroupReactionSystem skeleton — first M3 system
that mutates the M3.1 data layer. New module
`leviathan::systems::interest_group` with
`kInterestGroupReactionRate = 0.05`, `ReactionOutcome { int
groups_updated }`, and `react(state)`. Reaction is a linear-
toward-equilibrium drift on two fields driven by a single
input (`country.stability`): `loyalty += (country.stability -
loyalty) * 0.05` and `radicalism += ((1.0 - country.stability)
- radicalism) * 0.05`, both clamped to `[0, 1]`. `influence`,
`kind`, `country`, `id_code`, `name` untouched. `react` is
pure (no logs / RNG / time / country / faction / policy
mutation / events) and preflight-validates every
`group.country` before mutating any group (atomicity). Wired
into the monthly pipeline as the FINAL step of
`tick_all_countries` (after every per-country faction →
stability → economy). `MonthlyOutcome` gains `int
interest_groups_updated`. No save schema bump (still v11);
existing M1/M2 callers without interest groups see byte-
identical pipeline behaviour. 13 new doctest cases. No
events / AI / UI / command integration / country aggregate
effects / influence drift / per-kind formulas / RNG / strikes
/ protests / coups / cross-border;
**M3.1 InterestGroupState / political actors skeleton — opens
M3. New `core::InterestGroupKind` enum (10 variants:
Bureaucracy / Military / Workers / Farmers / Religious / Media
/ Students / LocalElites / Business / Technocrats). New
`core::InterestGroupState` POD with id_code / name / kind /
country (CountryId handle) / influence / loyalty (both default
0.5) / radicalism (default 0.0). New `GameState::interest_groups`
root-level vector (cross-country interactions can compose
naturally; each entry's `country` field points back to the
country). **Save format bumped v10 → v11** with the array
REQUIRED at the save layer (empty allowed) and every entry
strictly validated: non-empty id_code + name, known kind string,
country index resolving into `state.countries`, three ratios in
`[0, 1]` via `require_ratio`, duplicate id_code rejected.
`scenario_loader` accepts an OPTIONAL `interest_groups` block
in scenario JSON; missing → empty vector; present-but-malformed
rejected with `interest_groups[N]` path in error.
`diagnostics::compare_states` walks the array under
`interest_groups[N].*`. **Data only** — no M1 / M2 system reads
or writes the new fields. Future M3 sub-milestones layer
reactions / command-resistance / events / AI on top. No new
CLI flag; no new PlayerCommandKind; no new CSV column; no
`state.logs` entry; no replay primitive change; no M1 / M2
system change;
**M2.22 M2 exit / integration tests — closes M2. New
`tests/integration/m2_end_to_end_test.cpp` ships 3 end-to-end
tests pinning the player-operation surface: command script +
replay + `compare_states` equivalence, order-execution gate
atomicity across `EnactPolicy` and `AdjustBudget`, and 5-artefact
byte-identical determinism with M2 commands applied at day 0
through `apply_command_script`. New
`docs/milestone-2-result.md` exit report lists M2.1–M2.21
ledger, the architectural invariants every M3+ milestone must
preserve, and the explicitly-deferred items (Delayed /
Distorted outcomes, scheduler, RNG-based resistance, attempted-
command log, CLI script flag, runner-level rejection surface,
expanded authority fields, authority drift, faction reactions,
multi-country interaction, weighted formulas, …). README +
docs README + rfc README + memory all marked M2 closed. No new
gameplay, no save format change (still v10), no runner / CLI /
replay primitive / DataLoader / M1 system change, no new
`PlayerCommandKind`, no new CSV column. **M2 closes here.**
**M2.21 Command script driver helper — adds the library-only
free function
`commands::apply_command_script(state, vector<PlayerCommand>)`
on top of M2.20's `try_apply_pending`. Takes a one-shot script,
builds a local `CommandQueue`, dispatches through
`try_apply_pending`. Outcome reuses M2.20's
`ApplyWithReportOutcome` (no parallel struct). Routing inherited:
full drain → success + nullopt rejection; gate rejection →
success + populated record; non-execution failure (precondition
/ NaN delta / unknown policy / unknown category) → failure. Input
script vector not mutated. Library-only — no runner / RunOutcome
/ `main()` / CLI / replay / save schema change. Saves three
boilerplate lines at every REPL / scripted-test / agent-driver
call site;
**M2.20 Command rejection reporting — makes M2.18 / M2.19
order-execution rejections observable as structured data without
changing `apply_pending` semantics. New POD
`commands::RejectionRecord { kind, policy_id_code,
budget_category, compliance, threshold, resistance }`. New
wrapper `commands::ApplyWithReportOutcome { apply, rejection }`.
New free function `commands::try_apply_pending(state, queue)`
drains the queue exactly like `apply_pending` (same precondition,
same atomicity) but surfaces an order-execution rejection as
`Result::success` carrying the populated record. Non-execution
errors (precondition / NaN delta / unknown policy / unknown
category) still return `Result::failure`. Internal refactor
extracts `dispatch_one` in `commands.cpp`'s anonymous namespace
shared by both functions; `apply_pending`'s legacy rejection
error string is byte-identical via a `format_rejection_message`
helper. M2.18 / M2.19 tests pass unchanged. Drive-by:
refreshed stale `order_execution.cpp` comment that still said
"only EnactPolicy is evaluated in this PR" before M2.19 added
the AdjustBudget arm. No save format change (still v10); no
`apply_pending` signature change; no persistent attempted-
command log; no `state.logs` entry; no `RunOutcome` rejection
surface (M2.21 candidate); no DataLoader / replay primitive /
runner / CLI / M1 system change;
**M2.19 AdjustBudget execution gate — extends M2.18's
command-rejection shape to `AdjustBudget` with a category-aware
single-input gate. New constant
`kAdjustBudgetComplianceThreshold = 0.3` (matches `EnactPolicy`).
The `AdjustBudget` arm in `evaluate()` selects an authority input
by `command.budget_category`: `"military"` ⇒ `military_loyalty`,
every other category ⇒ `bureaucratic_compliance`. Then
`resistance = 1.0 - selected` and `status = (selected >= 0.3) ?
Accepted : Rejected`. `commands::apply_pending` gains a
pre-flight gate block structurally identical to M2.18's; rejected
`AdjustBudget` commands short-circuit with an error naming
`order_execution`, `rejected`, `AdjustBudget`, the offending
`budget_category`, selected compliance, and threshold. M2.3 / M2.4
mid-list-failure atomicity preserved. **No save format change**
(still v10); no `Delayed` / `Distorted` outcomes; no per-category
routing beyond the `military` branch; no weighted formula; no
scheduler; no RNG; no `state.logs` entry; no `RunOutcome`
rejection counter (M2.20 candidate); no DataLoader / policy /
replay primitive / runner / M1 system change;
**M2.18 EnactPolicy execution gate — first M2 sub-milestone where
a player command can be **rejected**. `order_execution` grows the
constant `kEnactPolicyComplianceThreshold = 0.3`, a `Rejected`
variant on `ExecutionStatus`, and a `resistance` field on
`OrderExecutionOutcome` (`1.0 - bureaucratic_compliance` for
`EnactPolicy`; `0.0` for kinds without a gate). `evaluate` now
branches on `command.kind`: `EnactPolicy` is `Accepted` when
`bureaucratic_compliance >= 0.3` and `Rejected` otherwise;
`AdjustBudget` stays unconditionally `Accepted`.
`commands::apply_pending` calls `evaluate` BEFORE the M2.3 policy
lookup for `EnactPolicy` commands; on `Rejected` it returns
`Result::failure` whose error names `order_execution`, `rejected`,
and the policy id_code. The rejected command stays at the head of
the queue and is NOT appended to `state.applied_commands`
(mirrors M2.3 / M2.4 mid-list-failure atomicity). Threshold 0.3
chosen so canonical default-0.5 scenarios accept unchanged. No
save format change (still v10); no `AdjustBudget` gate; no
`Delayed` / `Distorted` outcomes; no scheduler; no RNG; no
`state.logs` entry on rejection; no `RunOutcome` field counting
rejected commands; no DataLoader / policy effect / replay /
runner / M1 system change;
**M2.17 OrderExecutionSystem skeleton — first M2 system that reads
the M2.16 `government_authority` block. New module
`leviathan::systems::order_execution` with `OrderExecutionInputs`
(4-field snapshot of the actor country's authority ratios,
defaults 0.5), `ExecutionStatus` enum (only `Accepted` shipped;
`Rejected` / `Delayed` / `Distorted` reserved by name), and
`OrderExecutionOutcome { status, inputs }`. New free function
`evaluate(state, command) → Result<OrderExecutionOutcome>` mirrors
the M2.3 `apply_pending` preconditions (valid `player_country`
indexing into countries), snapshots authority into the outcome,
and always returns `Accepted`. **No caller wires the function in
yet** — `commands::apply_pending` is byte-identical with M2.5 /
M2.16. **No `resistance` field** in the outcome (deferred to the
same PR that introduces the formula). Pure read: no state
mutation, no logs, no RNG. CMake wires the new
`order_execution.cpp` into `leviathan_systems` and the new
`order_execution_test.cpp` into `leviathan_tests`. No save format
change;
**M2.16 GovernmentAuthorityState — first M2 gameplay-state
extension. New `core::GovernmentAuthorityState` POD with four
`[0, 1]` ratio fields defaulting to `0.5`
(`bureaucratic_compliance`, `military_loyalty`,
`intelligence_capability`, `media_control`) added to
`CountryState` as the `government_authority` field. **Save format
bumped v9 → v10**: block REQUIRED at save layer (`require_ratio`
per sub-field); DataLoader keeps it OPTIONAL in country JSON
(missing → defaults; present → validated). `diagnostics::compare_states`
extended to walk the four sub-fields under the
`countries[N].government_authority.*` JSON path. Drive-by: every
`"save_version": 9` JSON literal in tests bumped to `10`; every
existing v10 hand-built country JSON gained the new block.
**Data-only** — zero M1 systems read or write the new fields; M1
monthly pipeline and M2 command path are byte-identical. Deferred
from RFC-020 §3 with explicit documentation: `local_control`,
`legal_mandate`, `leader_prestige`, `party_organization`. **No
new gameplay logic, no new policy effect target, no new
`PlayerCommandKind`, no new CSV column, no scenario fixture
changes, no `state.logs` entry.**
**M2.9 Replay CLI error-path hardening — doc + tests PR with no
library behaviour change. Cements the **pre-`end_tick`** contract:
a `--replay` failure that occurs before `end_tick` is reached
(`save_system::load` failure, missing `--scenario`, `begin_tick`
rejection, `replay_with_time` failure on out-of-order log /
unknown policy id_code / malformed budget command / monthly
pipeline failure) writes ZERO output artefacts because `end_tick`
is the only function that touches disk. Failures INSIDE `end_tick`
itself are explicitly NOT covered — its five writes (save → log
→ summary CSV → countries CSV → factions CSV) are sequential and
non-transactional, so a mid-`end_tick` I/O failure can leave a
partial set on disk; atomic temp-file + rename is a deliberate
non-goal of this PR. `runner::run`'s doc comment gains an
explicit "M2.9 contract" block with that scope split; 3
regression tests pin missing source / out-of-order log / unknown
policy id_code with all five artefact paths wired and checked
absent. No save format change;
**M2.10 State comparison API — new
`systems::diagnostics::compare_states(a, b, opts)` free function
returning a list of `StateMismatch` entries (field_path +
detail). Walks gameplay-relevant fields field-by-field in
canonical order with floating-point tolerance (default 1e-9).
Library-only for now; consumers include replay-equivalence
tests and a future `--verify` CLI flag. Deliberately skips rng,
logs, policies, provinces, events, simulation_config — each
documented with rationale. No save format change; **M2.11
Replay verify CLI — new `--verify` boolean runner flag wires
M2.10 `compare_states` into the M2.8 `--replay` flow. After
`end_tick`, the runner compares the replayed state against the
loaded source save and populates
`RunOutcome::verify_mismatches`. `main()` prints
`Verify mismatches: N` plus one bullet per mismatch.
Informational only (exit code stays 0; strict mode deferred).
Reuses the already-loaded source save. No save format change;
**M2.12 Replay strict mode — new `--verify-strict` boolean
runner flag (requires `--verify`) makes `main()` exit
`EXIT_FAILURE` when M2.11 reports any mismatches. Full mismatch
list still printed first so CI logs capture every divergence.
`run()` semantics unchanged — strict is a `main()`-level exit
policy (library / CLI separation stays clean). Flag-chain
`--verify-strict → --verify → --replay` each with loud rejection
on missing dependency. No save format change; **M2.13 Verify
tolerance CLI — new `--verify-tolerance FLOAT` runner flag
(requires `--verify`) overrides M2.10's default 1e-9 tolerance
when calling `compare_states`. Parses via a new
`parse_nonneg_double` helper that rejects empty / garbage /
non-finite / negative inputs. Plumbed into `run()`'s replay
branch via `diagnostics::CompareOptions`. `main()` prints the
active tolerance when set. Completes the M2 replay-CLI family
(--replay / --verify / --verify-strict / --verify-tolerance).
No save format change.**

## Repository layout

```text
.
├── CMakeLists.txt        Top-level build
├── README.md             This file
├── rfc/                  Design RFCs — read these before changing scope
├── include/leviathan/    Public headers (core/ + systems/)
├── src/                  Simulation core + executable entry point
├── tests/                Unit / integration tests (doctest, per-module)
├── data/                 Game data (JSON): config/simulation.json,
│                         countries/{germany,france,japan}.json (M0.7-M1.3),
│                         factions/ger_*.json (M1.2),
│                         policies/*.json (M1.4),
│                         scenarios/1930_minimal.json (M1.11),
│                         scenarios/1930_with_start_policies.json (M1.13)
├── tools/                Dev / debug tools, currently empty
└── docs/                 Per-milestone design notes (m0-N-*.md) +
                          pr-drafts/ (PR write-ups)
```

## Requirements

- A C++17 compiler:
  - MSVC 19.20+ (Visual Studio 2019 or 2022), or
  - GCC 9+, or
  - Clang 10+
- CMake **3.16 or newer**
- A build tool that CMake can drive (Ninja, Make, MSBuild, Xcode, ...)
- Network access on the **first** configure: the build fetches
  [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 (used by
  the data loader), and the test suite fetches
  [doctest](https://github.com/doctest/doctest) v2.4.11. Both go through
  CMake `FetchContent` (shallow clones, pinned tags). Subsequent
  configures reuse the cached clones in `build/_deps/`.

## Build

From the repo root:

```bash
# Configure (single-config generators)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --config Debug
```

On Windows with Visual Studio, the equivalent is:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

The main executable is produced at `build/bin/leviathan` (`leviathan.exe`
on Windows).

## Run a simulation (M0.9 headless runner)

```bash
# Show usage
./build/bin/Debug/leviathan --help

# 10-day smoke run (everything else falls back to defaults)
./build/bin/Debug/leviathan --days 10

# M1.11 - load the canonical 1930 world so the monthly pipeline
# actually has countries to tick:
./build/bin/Debug/leviathan \
    --days 365 \
    --scenario data/scenarios/1930_minimal.json \
    --seed 12345

# Pinned configuration matching the M0.9 spec example
./build/bin/Debug/leviathan \
    --config data/config/simulation.json \
    --days 365 \
    --seed 12345 \
    --output out/ \
    --save out/save.json \
    --log out/events.jsonl \
    --summary-csv out/summary.csv

# M1.14 - per-country diagnostic CSV: one row per country per snapshot
# point with gdp / stability / last_gdp_growth_rate etc., inspectable
# without round-tripping the save.
./build/bin/Debug/leviathan \
    --days 365 \
    --scenario data/scenarios/1930_minimal.json \
    --countries-csv out/countries.csv

# M1.16 - per-faction diagnostic CSV: one row per faction per snapshot
# point with support / influence / radicalism / loyalty / resources.
# Can be combined with --countries-csv and --summary-csv.
./build/bin/Debug/leviathan \
    --days 365 \
    --scenario data/scenarios/1930_minimal.json \
    --countries-csv out/countries.csv \
    --factions-csv  out/factions.csv

# M1.17 - 10-year soak (RFC-090 §1.17 acceptance criterion).
# Loads the canonical day-0-policies scenario and ticks 3652 days
# (1930-01-01 -> 1940-01-01). 120 monthly pipeline runs.
./build/bin/Debug/leviathan \
    --days     3652 \
    --seed     12345 \
    --scenario data/scenarios/1930_with_start_policies.json

# M2.1 - select a player country. Resolution runs after --scenario
# loads, so --player only makes sense when a scenario is loaded.
# The id_code must match a country in the manifest; bad id_codes
# fail loudly before any tick happens.
./build/bin/Debug/leviathan \
    --days     365 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --player   GER

# M2.8 - replay a previously-saved command log onto a fresh state.
# --replay requires --scenario (for the fresh baseline). When
# --player is unset, player_country is auto-inherited from the
# loaded save. The runner writes the replayed state to save.json
# under --output; diff against the source to verify equivalence.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/source.json \
    --output   replay_out/

# M2.11 - same as above, but auto-compare the replayed state
# against the source via diagnostics::compare_states. Mismatches
# are printed to stdout; exit code stays 0 regardless.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/source.json \
    --verify \
    --output   replay_out/

# M2.12 - same as above but make the process exit EXIT_FAILURE
# on any mismatch. The full mismatch list still prints first so
# CI logs see the divergence; useful as a build/replay regression
# gate.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/golden.json \
    --verify --verify-strict \
    --output   replay_out/

# M2.13 - loosen the verify tolerance for cumulative drift in
# long simulations. Default is 1e-9; pass any finite non-negative
# double via --verify-tolerance to override.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/golden.json \
    --verify --verify-strict --verify-tolerance 1e-6 \
    --output   replay_out/

# M2.14 - replay only up to a chosen date and save there. Entries
# with applied_on > target_date are skipped; after the truncated
# replay, the time system is advanced day-by-day until current_date
# equals target_date so the resulting save reflects exactly that
# day. Requires --replay.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/source.json \
    --target-date 1930-06-15 \
    --output   replay_out/
```

Required flag: `--days`. Everything else has a default
(`--config data/config/simulation.json`, `--output out/`, `--seed`
falls back to the value in the config file, `--save` and `--log` live
under `--output`).

Outputs (paths can be overridden):

| File | Format | Source |
|------|--------|--------|
| `out/save.json`     | Pretty-printed save (M0.8)            | Round-trippable with `save_system::load` |
| `out/events.jsonl`  | One JSON object per line              | M0.6 logging exporter |
| `out/summary.csv`   | Per-snapshot CSV (M0.10, optional)    | `--summary-csv PATH` ; columns: `date,country_count,log_count,seed` |
| stdout              | Plain-text run summary                | start / end dates, log count, sanity-issue count, output paths |

**Determinism**: the same `--config` + `--days` + `--seed` produces
byte-identical `save.json`, `events.jsonl`, **and `summary.csv`**.
Pinned by tests in `tests/systems/runner_test.cpp`
(`byte-identical`, `summary-csv same-seed`).

The runner ticks an empty world (no countries, factions, policies)
in M0.9. Loading countries from a directory will land in a later PR.

## Test

```bash
ctest --test-dir build --output-on-failure
```

For multi-config generators (Visual Studio, Xcode):

```bash
ctest --test-dir build -C Debug --output-on-failure
```

As of M3.4 there are **693 doctest cases**. M0 contributed 179;
M1.1 added 9; M1.2 added 17; M1.3 added 9; M1.4 added 17; M1.5
added 24; M1.6 added 17; M1.7 added 16; M1.8 added 19; M1.9 added
11; M1.10 added 9; M1.11 added 25; M1.12 added 15; M1.13 added 15;
M1.14 added 17; M1.15 added 15; M1.16 added 18; M1.17 added 3
end-to-end integration tests; M2.1 added 17; M2.2 added 10; M2.3
added 8; M2.4 added 13; M2.5 added 11; M2.6 added 9; M2.7 added
10; M2.8 added 7; M2.10 added 12; M2.11 added 5; M2.12 added 5;
M2.13 added 8; M2.9 added 3; M2.14 added 9; M2.16 added 13;
M2.17 added 10; M2.18 added 10; M2.19 added 11; M2.20 added 10;
M2.21 added 8; M2.22 added 3 end-to-end integration tests;
M3.1 added 20 across the v10 → v11 save-format bump; M3.2 added
13 across the new `interest_group_system` module; M3.3 added 14
across the new `country_feedback` API; M3.4 adds 19 across the
new `authority_pressure` API: 17 unit tests (empty-state success
/ country-with-no-Bureaucracy-group skipped / high-loyalty drifts
compliance up / low-loyalty drifts compliance down /
influence-weighted aggregate over two Bureaucracy groups /
non-Bureaucracy groups ignored / zero-influence groups ignored
/ multi-country independent / interest groups never mutated /
other three authority sub-fields byte-identical / stability /
legitimacy / corruption byte-identical / invalid-group-country
preflight atomicity / NaN loyalty preflight / out-of-range
influence preflight / NaN bureaucratic_compliance preflight /
clamp safety) and 2 monthly-pipeline tests (tick_all_countries
runs M3.2 → M3.3 → M3.4 in that order with all four counters
set and an order-pin via re-running `authority_pressure` on the
resulting state; non-Bureaucracy interest group still succeeds
with `interest_group_authority_countries_updated == 0`).
Previously M3.3 added 14 across the new `country_feedback` API: 12 unit tests
(empty-state success / country-with-no-groups skipped / high-
radicalism lowers stability / low-radicalism raises / influence-
weighted aggregate over two groups / zero-influence groups
skipped / multi-country independent / interest groups never
mutated / invalid-group-country preflight atomicity / NaN
radicalism preflight / out-of-range influence preflight / NaN
country.stability preflight / clamp safety) and 2 monthly-
pipeline tests (tick_all_countries runs M3.2 then M3.3 in that
order, with both counters set and an order-pin via re-running
feedback on the resulting state; empty-interest_groups still
succeeds with `interest_group_countries_updated == 0`).
Previously M3.2 adds 13 across the new `interest_group_system`
module: 11 unit tests
covering empty / high-stability / low-stability drift /
multi-country routing / influence + identity fields untouched /
clamp safety / preflight-atomicity-on-invalid-country / sentinel
country handling, plus 2 monthly-pipeline integration tests
pinning that `tick_all_countries` runs the new react step after
the per-country tick and that empty `interest_groups` flows
unchanged. M3.1 adds 20 across the v10 → v11 save-format bump
and the new
`InterestGroupState` plumbing: 2 game_state baselines (empty
default vector + default POD defaults 0.5/0.5/0.0), 9 save_system
(serialize emits empty array; round-trip preserves arbitrary
values; old v10 save rejected; v11 missing block rejected; v11
wrong type rejected; unknown kind rejected; country OOB
rejected; out-of-range ratio rejected; duplicate id_code
rejected), 8 scenario_loader (absent → empty; wrong type
rejected; missing field rejected; ratio out of range rejected;
duplicate id_code rejected; happy-path load with two groups;
unknown country id_code rejected; unknown kind rejected), and
2 diagnostics compare_states tests (size mismatch + per-field
mismatch paths). Previously M2.22 added 3 end-to-end integration
tests in
`m2_end_to_end_test.cpp` closing M2:
(1) command script + replay reproduces source via
`compare_states` (zero mismatches across every gameplay-
relevant field after replay_with_time on the source's
applied_commands);
(2) gate atomicity across `EnactPolicy` and `AdjustBudget`
(low bureaucratic_compliance + high military_loyalty: military
adjustment lands, EnactPolicy rejected, trailing welfare
AdjustBudget unreached; state / queue / applied_commands all
consistent with per-command atomicity);
(3) 5-artefact byte-identical determinism with M2 commands
(same script + same setup produces matching save.json /
events.jsonl / summary.csv / countries.csv / factions.csv
across two independent temp dirs, extending M1.17's
determinism contract through the command path). Previously
M2.21 adds 8 covering the new
`commands::apply_command_script` helper: empty script success;
full success applies both `EnactPolicy("raise_taxes")` and
`AdjustBudget("military", +0.02)` in order and logs both;
EnactPolicy rejected at compliance 0.1 surfaces structured
`RejectionRecord{kind, policy_id_code, compliance=0.1,
threshold=0.3}`; AdjustBudget(military) rejected at
military_loyalty 0.05 records `military_loyalty` as compliance
(M2.19 selected-input contract); mid-script rejection preserves
prior `AdjustBudget(military)` apply+log while trailing
`AdjustBudget(welfare)` does NOT run (helper does not surface
remaining commands); unknown policy id_code still returns
`Result::failure`; invalid `player_country` returns failure
naming `player_country`; input vector survives the call
unmutated element-by-element. Previously M2.20 added 10
covering the new `try_apply_pending` structured-rejection
surface: full drain returns success + nullopt rejection; EnactPolicy
rejection populates `RejectionRecord{kind, policy_id_code,
compliance=0.1, threshold=0.3, resistance=0.9}` with full
atomicity asserted (tax burden / queue head / applied_commands
all unchanged); AdjustBudget(military) rejection records
`military_loyalty` as `compliance` (not bureaucratic);
AdjustBudget(welfare) rejection records `bureaucratic_compliance`
even with high military_loyalty (selected-input contract);
unknown policy / unknown budget_category / non-finite delta all
still return `Result::failure`; invalid `player_country` returns
failure that names `try_apply_pending`; mid-list rejection
preserves a prior successful `AdjustBudget(military)` and leaves
the rejected `EnactPolicy` at the queue head with the trailing
command still behind it; `apply_pending` rejection still returns
`Result::failure` (backward-compat regression). Previously M2.19
adds 11 covering the new
category-aware AdjustBudget gate: 7 in `order_execution_test.cpp`
(military category at threshold 0.3 accepts with resistance 0.7;
0.299 rejects; military category ignores high bureaucratic
compliance when military_loyalty is low; non-military categories
— iterates over administration/education/welfare/intelligence/
infrastructure/industry — reject when bureaucratic_compliance <
0.3 regardless of military_loyalty; non-military accepts at high
bureaucratic_compliance with low military_loyalty; default 0.5
authority accepts both military and non-military categories;
rejected path is non-mutating) and 4 in `commands_test.cpp`
(AdjustBudget(military) rejected when military_loyalty < 0.3 with
full error contents; AdjustBudget(welfare) rejected when
bureaucratic_compliance < 0.3 even with high military_loyalty;
AdjustBudget(military) still accepted at military_loyalty 0.8
when bureaucratic_compliance 0.05; mid-list rejection with prior
AdjustBudget(military) applied and trailing EnactPolicy still
queued). Drive-by: M2.18's "EnactPolicy and AdjustBudget identical
inputs" assertion updated (adjust.resistance now reflects
military_loyalty, not 0.0); M2.18's "AdjustBudget bypasses" /
"unaffected" tests refreshed to reflect the M2.19 routing.
Previously M2.18 added 10 covering the new EnactPolicy gate:
6 in `order_execution_test.cpp` (compliance at threshold 0.3
accepts with resistance 0.7; compliance 0.299 rejects;
`resistance == 1.0 - compliance` across spot-check inputs;
default 0.5 compliance still accepts; `AdjustBudget` bypasses the
gate at compliance 0.01; rejected path leaves state byte-
identical) and 4 in `commands_test.cpp` (default 0.5 EnactPolicy
still drains the queue and logs; compliance < 0.3 rejected with
`order_execution` + `rejected` + policy id_code in the error,
state unchanged, queue head intact, applied_commands untouched;
mid-list rejection stops with prior AdjustBudget already applied
and logged while trailing EnactPolicy stays queued;
`AdjustBudget` unaffected by 0.05 compliance). Drive-by updates:
"default OrderExecutionOutcome" now also pins `resistance == 0.0`
and the "EnactPolicy and AdjustBudget" kind-comparison test
splits into "same inputs, different resistance". Previously
M2.17 added 10 covering the new `order_execution::evaluate`
skeleton (3 precondition cases — no player_country, out of range,
empty countries; 3 success-path cases — Accepted returned, inputs
mirror the actor's authority block one-for-one, function reads
the *selected* country rather than `countries[0]`; 3
non-mutation / determinism / kind-independence cases — state is
byte-identical after the call, `EnactPolicy` and `AdjustBudget`
produce identical outcomes since the skeleton ignores
`command.kind`, repeated calls return identical outcomes; 1
default-construction baseline pinning all four `inputs` ratios at
0.5 and `status == Accepted`). Previously M2.16 added 13 across
the v10 save-format bump and the new
`GovernmentAuthorityState` plumbing: 1 game_state baseline (default
0.5 across all four sub-fields), 5 data_loader (missing block →
defaults; present → loaded; wrong-type rejected; missing sub-field
rejected; out-of-range rejected), 6 save_system (serialize emits
the block; round-trip preserves arbitrary values; v9 save rejected
loudly; v10 country missing block rejected; v10 block missing
sub-key rejected; v10 out-of-range value rejected), and 1
diagnostics compare_states test pinning the per-sub-field paths
under `countries[0].government_authority.*`. Previously M2.14
added 9 covering `--target-date`:
5 parse cases (plumbed; default nullopt; missing value rejected;
malformed date `"1930-13-01"` rejected with flag name + value in
error; without `--replay` rejected with both flag names in error)
and 4 run cases (target past log advances time system and saves
at target; target equal to last entry replays full log with no
extra step; target earlier than a log entry truncates the log to
entries with `applied_on <= target_date`; target before scenario
start rejected with `--target-date` + the bad date +
"scenario start" in error, and `check_no_artifacts` confirms the
M2.9 pre-`end_tick` no-artefact contract). The dated-log helper
`build_source_with_dated_log` hand-splices `AppliedPlayerCommand`
entries at chosen monotonic dates so truncation can be exercised
without going through `apply_pending`. Previously M2.9 added 3
covering the no-artefact contract under `--replay`: missing
source file fails with "--replay" in error and all five artefact
paths absent; out-of-order log fails with "out-of-order" in error
and all five paths absent; unknown policy id_code fails with
`"no_such_policy_id_code"` in error and all five paths absent.
Each test wires save / events / summary CSV / countries CSV /
factions CSV inside a `TempDir` and uses a shared
`wire_all_artifacts` + `check_no_artifacts` helper. Previously
M2.13 added 8 covering `--verify-tolerance`: parse plumbed with
value preserved; default nullopt; missing value rejected;
non-numeric (`"abc"`) rejected with "floating-point" in error;
negative rejected with `">= 0"` in error; without `--verify`
rejected with both flag names; end-to-end: loose tolerance
(`1e-2`) absorbs a `1e-3` `gdp` tweak with no mismatch on that
path; tight tolerance (`1e-6`) catches the same tweak.
Previously M2.12 added 5 covering `--verify-strict`: parse plumbed (with `--verify`),
defaults false, `--verify-strict` without `--verify` rejected
with both flag names; run with strict + matching source succeeds
empty mismatches; run with strict + tweaked source still
succeeds at `run()` level (exit code is `main()`'s concern) but
reports mismatches. Previously M2.11 added 5 covering the
`--verify` CLI flag: parse_args plumbed (with `--replay`),
defaults false, `--verify` without `--replay` rejected with both
flag names in the error; full-replay with matching source
reports zero mismatches; full-replay with manually tweaked
source (`countries[0].legal_tax_burden = 0.99`) detects the
divergence at the documented path. Previously M2.10 added 12
covering the new `compare_states` API: two empty match; identical seeded match;
`current_date` diff with both date strings in detail;
`player_country` diff; country count → `countries.size()`
mismatch; gdp diff on country[0] → `countries[0].gdp` path;
tolerance — within (silent) and outside (reported);
`active_policies` size diff caught with the array path;
`applied_commands` size diff caught; multiple mismatches
collected in canonical order; custom `CompareOptions` tolerance
respected. Previously M2.8 added 7 covering the `--replay` CLI: parse_args plumbed,
missing value rejected, default unset; run --replay without
--scenario rejected; run --replay with single EnactPolicy
reproduces source's `legal_tax_burden` + log; --player auto-
inherits from loaded save when unset; --replay of an empty-log
save replays zero commands. Previously M2.7 added 10 covering
"M2.7: replay_with_time": empty log no-op,
command at start_date (0 advance), command 5 days later (5
advance), command past month boundary (45 days + 1 monthly_tick),
multiple commands at different dates, out-of-order rejected,
ctrl not started rejected, no player_country rejected, non-empty
`applied_commands` rejected, **full equivalence vs original
simulation** (drive source via step + apply, replay onto fresh
target, verify every observable matches including gdp /
stability / last_gdp_growth_rate). Previously M2.6 added 9 in
`tests/systems/commands_test.cpp`: empty log replays cleanly;
single EnactPolicy / single AdjustBudget / mixed-kind log all
replay state + re-log correctly; replayed log mirrors source
byte-equivalent across two dates; `current_date` forced to last
entry's `applied_on` (prototype limit pinned); unknown
policy_id_code mid-list stops with `replay[N]` in the error;
no `player_country` rejects before any replay; non-empty
`applied_commands` rejects the precondition. Previously M2.5
added 11 covering the AdjustBudget kind: 8 commands_test cases (delta mutates target field; negative
delta shrinks; overshoot clamps to 1.0; undershoot clamps to 0.0;
unknown `budget_category` rejected with no mutation; non-finite
`budget_delta` rejected; AdjustBudget log entry carries correct
category + delta; mixed-kind queue applies both in insertion order
with both kinds logged) and 3 save_system_test cases (AdjustBudget
log entry round-trips category + delta; v9 entry missing
`budget_category` rejected; v9 entry missing `budget_delta`
rejected). Previously M2.4 added 13 covering player command log: 5
commands_test cases (successful enact appends one entry; multiple
successes append in insertion order; failed command does NOT log
(per-command atomicity); `applied_on` captures `state.current_date`
at apply time, distinct dates pinned across two `apply_pending`
calls; precondition failure leaves log untouched); 8 save_system
cases (rejects an old v8 save loudly; serialize emits
`"applied_commands": []`; populated round-trip preserves dates +
policy_id_code; v9 missing `applied_commands` rejected; entry
with malformed `applied_on` `"1930-02-30"` rejected; entry with
unknown `kind` `"SomethingBogus"` rejected; entry missing
`policy_id_code` rejected; entry missing `command` sub-object
rejected). Plus an extension of the `game_state_test` baseline
check to assert `applied_commands.empty()`. Save schema is now
v9. Previously M2.3 added 8 in
`tests/systems/commands_test.cpp`: empty queue is a no-op success;
single `EnactPolicy` drains the queue and applies the policy;
successful enact chains into `active_policies` (M1.15 integration:
`expires_on = 1930-03-02` for canonical `raise_taxes` 60-day enact
from `1930-01-01`); multiple commands apply in insertion order;
no `player_country` selected rejected with queue untouched;
`player_country` out-of-range rejected; unknown `policy_id_code`
mid-queue stops at failed cmd (first cmd applied, failed stays at
head, trailing cmd still queued); policy with bad effect target
stops via M1.5 pre-flight rejection. Previously M2.2 added 10
covering pause / resume / step primitives: 6 misuse + counter
cases (`begin_tick` double-begin rejected; `step_one_day` before
begin / after end rejected; `end_tick` before begin / double end
rejected; controller counters `start_date / days_stepped /
monthly_ticks / started / ended` reflect actual lifecycle); 2
equivalence cases (`begin/step×N/end` byte-identical to
`run_state(days=N)` across save + events + all 3 CSVs;
`begin/step×15/step×16/end` byte-identical to
`run_state(days=31)`); 2 drive-by regression cases from the PR
#29 nit (bad `--player` empty-world / unknown id_code both leave
no `save.json` / `events.jsonl` on disk). Previously M2.1 added
17 covering player country selection: 9 save_system cases (rejects an old v7 save loudly,
serialize emits `"player_country": -1`, save+load round-trips
both `-1` and a valid index, v8 missing `player_country` rejected,
non-integer rejected, `< -1` rejected, out-of-range rejected,
above `INT_MAX` rejected); 8 runner cases (`--player` plumbed /
value-missing / default-unset; on empty world rejected with
id_code in message; unknown id_code rejected; `--player GER` →
`player_country: 0`; `--player FRA` → `player_country: 1`; no
`--player` → `player_country: -1`). M1.17 added the 3
end-to-end integration tests in `tests/integration/m1_end_to_end_test.cpp`:
(1) 1-year run that loads the canonical day-0-policies scenario via
the runner, asserts the `active_policies` round-trip (`expires_on
= 1930-03-02` for `raise_taxes`, `1930-01-31` for
`increase_military_budget`), checks `monthly_ticks == 12` and CSV
row counts (1+12+1 snapshot points × N entities); (2) 10-year soak
run pinning RFC-090 §1.17 (3652 days, 120 monthly pipelines, zero
sanity issues, every country's gdp / stability / legitimacy /
last_gdp_growth_rate finite and clamped); (3) 5-artefact
byte-identical determinism check (save / events / summary CSV /
countries CSV / factions CSV) over the 1-year scenario. M1.16
previously added 18 covering the **per-faction CSV**: 9 diagnostics cases (faction_snapshot reads
every field, invalid id rejected with bad index in message,
default-id rejected, empty state rejects any index, header
byte-exact, row well-formed with 8 commas / 9 columns, negative
resources survives format, byte-identical for same row twice,
snapshot doesn't mutate state); 9 runner cases (`--factions-csv`
plumbed/value-missing/default-unset, no-flag no file, empty state
header-only, canonical scenario emits `9 rows = 3 factions × 3
snapshots` for a 31-day run, byte-identical determinism, summary
CSV unchanged when `--factions-csv` is added (M0.10 contract
regression), countries CSV unchanged when `--factions-csv` is
added (M1.14 contract regression)). Save schema remains v7 (no
new persistent state). Each `TEST_CASE` is registered with CTest
individually, so e.g. `ctest -R "faction_snapshot|factions-csv"`
runs just the M1.16 cases.

Previously: M1.15 added 15 covering the **policy duration
tracking** save state plus the new runtime cap on `duration_days`:
9 policy_system cases (successful enact appends one `ActivePolicy`,
pre-flight failure appends nothing, `duration_days == 0` same-day
expiry, multiple enacts preserve insertion order, faction-zero-
match enactment still records, only the actor's list grows,
negative `duration_days` rejected, `duration_days >
kMaxTrackedPolicyDurationDays` rejected, the cap boundary value
itself is accepted); 6 save_system cases (rejects an old v6 save
loudly, serialize emits `"active_policies": []`, save+load
round-trips populated entries with their `expires_on` dates, v7
country missing `active_policies` rejected, entry missing
`policy_id_code` rejected, entry with malformed `expires_on`
rejected); plus an extension to the M1.13 day-0-enactment scenario
test that pins `expires_on = 1930-03-02` for the canonical
`raise_taxes` (60-day) enactment.
Save schema is now v7. Each `TEST_CASE` is registered with CTest
individually, so e.g. `ctest -R "M1.15"` runs just the M1.15 cases.

## Build options

| Option                   | Default                       | Purpose                              |
|--------------------------|-------------------------------|--------------------------------------|
| `LEVIATHAN_BUILD_TESTS`  | `ON` when top-level, else `OFF` | Build and register the test suite. |

Override with `-DLEVIATHAN_BUILD_TESTS=OFF` if you only want the binary.

## Contributing / roadmap

Read the RFCs in order before changing scope:

1. `rfc/README.md`
2. `rfc/RFC-000-overview.md`
3. `rfc/RFC-001-development-contract.md`
4. `rfc/RFC-010-prototype-v0_1.md`
5. `rfc/RFC-060-technical-architecture.md`
6. `rfc/RFC-070-data-formats.md`
7. `rfc/RFC-080-research-formulas.md`
8. `rfc/RFC-090-roadmap.md`

Each Milestone 0 sub-milestone (M0.1 – M0.11) ships as its own PR to
`main`. Do not bundle multiple sub-milestones in one PR.

Per-milestone design notes (locked-in schemas, error formats,
architectural rules) live under `docs/m0-N-*.md`; PR write-ups live
under `docs/pr-drafts/`.

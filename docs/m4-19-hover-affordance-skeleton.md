# M4.19 - hover affordance skeleton

Companion notes for `feature/rfc090-m4-19-hover-affordance-skeleton`.

M4.19 adds a mouse-hover visual cue so users can see "this
marker reacts to me" before they click. Pure CSS — no
JavaScript change, no markup change beyond the `<style>`
block.

## 1. Scope

What ships:

```text
src/leviathan/systems/svg_export.cpp
  - render_map_html <style>: +2 CSS rules
      svg circle:hover { stroke: #666666; stroke-width: 2; }
      svg text:hover   { text-decoration: underline; }
    Placed AFTER the M4.10 cursor:pointer rule and BEFORE
    the M4.12 .selected rules so that .selected and
    :focus-visible (equal specificity, later in source
    order) win on the same element when both apply.
include/leviathan/systems/svg_export.hpp
  - Intro paragraph mentions M4.19 hover affordance.
  - Output-shape <style> rules list gains the two new rules
    with M4.19 annotation.
  - "What ... deliberately do NOT do" heading extends to
    M4.19.
docs/milestone-4-checkpoint.md
  - INLINE refresh per the new
    feedback_checkpoint_drift rule: <style> selector
    count 17 → 19; HTML wrapper shape block lists the
    two new hover rules; Interactivity surface section
    adds a :hover CSS bullet; HOVER + TOOLTIPS deferred
    bucket rewrites to "basic :hover CSS shipped at
    M4.19; richer hover behaviour still deferred".
tests/systems/svg_export_test.cpp                  +5 doctest cases
README.md / docs/README.md / rfc/README.md         M4.19 entry,
                                                   M4 still in
                                                   progress
docs/m4-19-hover-affordance-skeleton.md            this file
```

## 2. Why these two rules specifically

**Circle: grey 2px stroke.**

```css
svg circle:hover { stroke: #666666; stroke-width: 2; }
```

- `#666666` is medium grey, deliberately distinct from
  the M4.12 black `#000000` `.selected` stroke and the
  M4.16 blue `#1976d2` `:focus-visible` ring. Three
  states stay separable: hover (grey), selected (black),
  focused (blue).
- `stroke-width: 2` is thinner than both M4.12 (3px) and
  M4.16 (4px), so when states stack (hover + selected,
  or hover + focused), the thicker state visually wins.

**Text: underline.**

```css
svg text:hover { text-decoration: underline; }
```

- Different mechanism from M4.12 `.selected`
  (`font-weight: bold`) and M4.16 `:focus-visible`
  (`outline`). Layered text states stay readable: bold +
  underlined for selected-and-hovered; outlined + bold +
  underlined for selected-and-focused-and-hovered.
- `text-decoration: underline` is a standard, widely
  supported affordance for "this text is interactive"
  (mirrors browser link behaviour).

## 3. CSS source order matters

CSS specificity for the three state-rules is **equal**
(each is `element + 1 pseudo-class or class`):

- `svg circle:hover`         — 0,0,1,1
- `svg circle.selected`      — 0,0,1,1
- `svg circle:focus-visible` — 0,0,1,1

Because specificity is equal, the **last-declared rule
wins** when multiple apply. The M4.19 rules MUST appear
**before** `.selected` and `:focus-visible` so those
state-rules override hover when both apply (e.g. when the
user has both clicked AND is still hovering). The test
`render_map_html: M4.19 :hover rules appear BEFORE
.selected and :focus-visible in CSS source order` pins
this.

## 4. Why pure CSS, not JS pair-hover

A "richer" hover would highlight the matching `<text>`
when the user hovers a `<circle>` (and vice versa). That
would require JS:

```js
// Hypothetical pair-hover handler — NOT shipped in M4.19
el.addEventListener("mouseover", function() {
  // find all nodes sharing this data-id, add .hover-pair
});
el.addEventListener("mouseout", function() {
  // remove .hover-pair from all nodes
});
```

This is deferred for two reasons:

1. **Pure CSS keeps the M4.19 surface minimal.** Adding
   mouseover/mouseout listeners would grow the inline
   `<script>` and add a new event-listener type — a
   non-trivial expansion. Deferring keeps M4.19 small.
2. **CSS-only hover is good enough as an affordance.**
   The browser's cursor:pointer (M4.10) already tells
   the user "this is clickable"; the M4.19 hover adds a
   visual cue on the specific element under the cursor.
   Pair-hover is polish, not foundational.

A future M4.x sub-milestone can layer JS pair-hover on
top of M4.19's CSS without changing the CSS rules.

## 5. What M4.19 does NOT do

```text
no JS hover handler (no addEventListener("mouseover" /
   "mouseout" / "mouseenter" / "mouseleave"); no
   element.onmouseover / .onmouseout assignment)
no pair-hover (hovering a circle does NOT also highlight
   its matching <text>, and vice versa)
no tooltip (no SVG <title> child element on the markers;
   no CSS-only tooltip via :hover ::after)
no hover-driven detail-panel preview / hover delay
no animation / transition on the hover state
no broader ARIA (still deferred from M4.17)
no keyboard polish beyond M4.15
no selection persistence across reloads
no second <script>, no <script src=>, no <script type=>
no <link>, no external CSS, no external font
no fetch / XHR / storage / history / navigation APIs
no innerHTML / outerHTML / document.write / eval / Function
no inline event attributes
no per-element inline style="..." (the hover CSS lives
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
no change to provinces.svg bytes (hover CSS is HTML-only)
no M4 close-out
no "M4 closed" wording
```

`provinces.svg` bytes UNCHANGED from M4.17 — the hover CSS
lives entirely in the HTML wrapper's `<style>` block.
`map.html` bytes did change (two new CSS rules).

## 6. Test coverage

Unit (`tests/systems/svg_export_test.cpp`):

- `render_map_html: M4.19 <style> block carries the two
  hover rules` — pins both CSS literals verbatim.
- `render_map_html: M4.19 hover colour distinct from M4.12
  .selected and M4.16 :focus-visible` — pins all three
  state stroke literals (`#666666` / `#000000` / `#1976d2`)
  appear and are distinct.
- `render_map_html: M4.19 :hover rules appear BEFORE
  .selected and :focus-visible in CSS source order` —
  pins the ordering invariant that lets equal-specificity
  state rules override hover correctly.
- `render_map_html: M4.19 hover is pure CSS — no JS hover
  handler, no tooltip, no <title> child` — explicit
  absence of `"mouseover"` / `"mouseout"` / `"mouseenter"`
  / `"mouseleave"` event names, `.onmouseover` /
  `.onmouseout` property assignments, and SVG `<title>`
  child elements on the markers. (The `<head>` `<title>`
  remains, as expected.)
- `render_provinces (standalone SVG) does NOT include the
  M4.19 :hover rules` — standalone SVG path stays free of
  `:hover`, `text-decoration`, `#666666`.

The M4.9 / M4.14 / M4.18 integration tests A/B/C/D/E/F
all stay green unchanged: none assert hover absence; all
the existing surfaces are unchanged. M4.10–M4.17 unit
tests stay green — additive CSS only.

## 7. Cross-references

- [`m4-16-focus-visible-skeleton.md`](m4-16-focus-visible-skeleton.md)
  — the M4.16 `:focus-visible` styling M4.19 stacks
  alongside. Same `<style>` block; specificity / source
  ordering carefully managed so the three states
  (hover, selected, focused) stay separable.
- [`m4-12-selected-state-css-skeleton.md`](m4-12-selected-state-css-skeleton.md)
  — the M4.12 `.selected` rules M4.19 visually
  distinguishes from. Three colour literals (`#666666`
  hover / `#000000` selected / `#1976d2` focus) chosen
  to stay readable.
- [`m4-10-clickable-ui-skeleton.md`](m4-10-clickable-ui-skeleton.md)
  — the M4.10 `cursor: pointer` rule is the first
  hover-related affordance; M4.19 extends with visual
  state on the element being hovered.
- [`m4-17-aria-labels-skeleton.md`](m4-17-aria-labels-skeleton.md)
  — the M4.17 `aria-label` is the accessible name. The
  M4.19 non-goal "no SVG `<title>` child" exists
  specifically because `<title>` would compete with
  `aria-label` as the accessible name (browsers may
  prefer one over the other).
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4 status snapshot; **inline-refreshed in this
  PR** per the new
  [feedback_checkpoint_drift](../memory/feedback_checkpoint_drift.md)
  rule.
- [`m4-18-accessibility-checkpoint-refresh.md`](m4-18-accessibility-checkpoint-refresh.md)
  — the M4.18 batched-refresh PR. M4.19 is the first
  surface change AFTER the checkpoint-drift rule, so
  the checkpoint refresh happens inline here instead of
  waiting for the next batched refresh.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  M3 invariants. M4.19 preserves all of them.

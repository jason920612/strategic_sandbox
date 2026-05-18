# M4.21 - responsive viewport skeleton

Companion notes for `feature/rfc090-m4-21-responsive-viewport-skeleton`.

M4.21 makes `map.html` render usably on narrow / mobile
screens. Two additions, both small:

- `<meta name="viewport" content="width=device-width,
  initial-scale=1">` in `<head>` — tells mobile browsers
  to use the device's actual width as the viewport
  instead of the default ~980px desktop emulation, and
  to start at zoom level 1 (no auto-shrink-to-fit).
- One `@media (max-width: 1040px)` rule in the `<style>`
  block — scales the SVG to `width: 100%; height: auto`
  so it fits the column without horizontal scroll. The
  existing `viewBox="0 0 1000 1000"` preserves the
  aspect ratio for free.

That's the entire skeleton. Narrowly reverses the
M4.5–M4.20 "no `<meta viewport>`, no media queries"
non-goal; broader responsive work stays deferred.

## 1. Scope

What ships:

```text
src/leviathan/systems/svg_export.cpp
  - render_map_html <head>: +1 <meta name="viewport">
    tag right after <meta charset> and before <title>.
  - render_map_html <style>: +1 @media block at the end
    of the existing rules:
      @media (max-width: 1040px) {
        svg { width: 100%; max-width: 100%; height: auto; }
      }

include/leviathan/systems/svg_export.hpp
  - Intro paragraph mentions M4.21 narrowly reverses
    the M4.5–M4.20 "no <meta viewport>, no media queries"
    non-goal.
  - Output-shape <head> description gains the viewport
    meta; <style> rules list gains the @media block.
  - "Deliberately do NOT do" frame extends to M4.21.

tests/systems/svg_export_test.cpp                  +5 doctest cases

docs/milestone-4-checkpoint.md (INLINE refresh per
feedback_checkpoint_drift)
  - Sub-milestones list adds M4.21.
  - <head> shape adds the viewport meta.
  - <style> block adds the @media rule.
  - Selector-count wording: "20 plain selectors plus one
    @media block" (was "20 selectors" at M4.20).
  - Invariants section rewrites the "no <meta viewport>,
    no media queries" rule to "exactly one viewport meta,
    exactly one @media block; broader responsive surface
    deferred".
  - Deferred items VISUAL POLISH bucket rewrites: viewport
    + one @media SHIPPED; broader responsive work
    (mobile-only layouts, breakpoint cascade, container
    queries, prefers-* media features, responsive font
    sizing, JS resize listener) still deferred.
  - "Refreshed at" stamp extends to M4.21.

README.md / docs/README.md / rfc/README.md         M4.21 entry,
                                                   M4 still in
                                                   progress
docs/m4-21-responsive-viewport-skeleton.md         this file
```

## 2. Why these specific values

**`<meta name="viewport" content="width=device-width, initial-scale=1">`** — the standard modern viewport
declaration. `width=device-width` makes the layout
viewport match the device's actual width (so 360px-wide
phones layout at 360px CSS pixels, not at the default
desktop-emulated ~980px). `initial-scale=1` makes the
browser start at zoom level 1 instead of auto-shrinking
to fit. Together these are the universally-recommended
default for modern web apps. No `maximum-scale` /
`user-scalable=no` — those harm a11y and we're not
trying to lock zoom.

**`@media (max-width: 1040px)`** — the threshold is
chosen to match `1000 (SVG width) + 2 * 20px (body
padding) = 1040px`. The SVG's 1px border lives INSIDE
the padded column, so it doesn't add to the layout
width. Below the threshold the fixed-width SVG would
either be cropped (overflow hidden) or force horizontal
scroll. At and below 1040px the rule activates and the
SVG scales to `width: 100%`. Above 1040px the original
`svg { ... margin: 0 auto; ... }` rule (M4.6) centres
the fixed-size SVG, which is the desktop layout we
want preserved.

(Fixed in M4.22: an earlier draft of this note wrote
`1000 + 20*2 + 1*2 = 1042`, double-counting the border.
The implementation always used 1040 correctly; only the
math wording drifted.)

**`width: 100%; max-width: 100%; height: auto`** — the
SVG's `width="1000"` / `height="1000"` attributes from
`render_svg_root` are sized in pixels, but the existing
`viewBox="0 0 1000 1000"` declares the coordinate
system. With `width: 100%; height: auto`, the rendered
SVG width becomes whatever the column allows, and the
height auto-computes from the viewBox aspect ratio (1:1
square). All M4.x positioning (circle cx/cy, text x/y,
focus ring stroke-width, etc.) is in viewBox coordinates,
so the entire visual surface scales together — no broken
proportions, no text-too-small, no clipped markers.

## 3. Why not a broader responsive overhaul

The reviewer's spec was explicit:

> 只加 `<meta name="viewport">` 和非常小的 responsive
> CSS，讓 `map.html` 在窄螢幕上更可讀；不要做 new
> artifact、schema、commands、state mutation、events、AI、
> gameplay、external CSS、media-query 大重排或
> mobile-specific JS.

The minimal addition addresses the **single biggest
usability problem on narrow screens** (the SVG being
larger than the viewport) without committing to a full
responsive redesign. Things deliberately deferred:

- **Mobile-only layouts** — legend / details panel /
  hover-status bar all keep their desktop arrangement
  (vertical stack with max-width: 1000px centred). On
  narrow screens they naturally stack — they already
  use `max-width` and `margin: 0 auto`, so they wrap
  to the column without needing dedicated mobile rules.
- **Breakpoint cascade** — only one threshold at 1040px.
  No `(max-width: 768px)` / `(max-width: 480px)` / etc.
  Future M4.x can add more thresholds if specific
  surfaces need finer control.
- **Container queries** (`@container`) — needs an
  explicit `container-type` declaration on the column
  parent, which would add another CSS rule. Beyond the
  skeleton scope.
- **`prefers-color-scheme` / `prefers-reduced-motion`**
  — would imply a dark-mode variant or animation
  controls. Out of scope; no dark mode and no
  animations exist yet.
- **Responsive font sizing** (`clamp()`, `vw` units) —
  font sizes are inherited from `body { font-family:
  sans-serif }` and the OS default; that scales fine
  on most devices without per-element fluid sizing.
- **JS responsive surface** (`matchMedia`,
  `ResizeObserver`, `window.innerWidth`, `"resize"`
  listeners) — the M4.21 surface is **pure CSS**, no
  JS responsive logic at all. Click + keydown + hover
  listeners already in place; no need to add a resize
  listener for the viewport scaling to work.

## 4. What M4.21 does NOT do

```text
no second <meta name="viewport"> (only ONE viewport tag)
no second @media block (only ONE media query); no
   breakpoint cascade
no @container / container queries
no @supports feature queries
no min-width: media queries (only max-width)
no orientation media queries
no prefers-color-scheme dark-mode variant
no prefers-reduced-motion
no responsive font sizing (no clamp() / vw / vh units)
no JS responsive surface (no matchMedia / ResizeObserver
   / window.innerWidth / window.innerHeight reads /
   "resize" event listener / .onresize property)
no mobile-only layout rules for the legend / details
   panel / hover-status bar — they wrap naturally via
   their existing max-width + margin: 0 auto
no fluid font / font-size CSS
no CSS animations / transitions
no @import / @font-face
no <link>, no external CSS, no external font
no inline event attributes
no per-element inline style="..."
no save schema bump (still v12)
no new state field
no new artefact (still 10)
no new fixture
no new InterestGroupKind / PlayerCommandKind
no rename of the M4.8 / M4.13 data-* keys
no second <script>, no <script src=>, no <script type=>
no fetch / XHR / storage / history / navigation APIs
no innerHTML / outerHTML / document.write / eval /
   Function / insertAdjacentHTML
no broader ARIA (still deferred from M4.17)
no state mutation, no commands, no events, no AI
no selection persistence
no neighbour / adjacency edges
no terrain / resources / population overlays
no runner CLI flag
no change to provinces.svg bytes (viewport + @media live
   in render_map_html only)
no M4 close-out
no "M4 closed" wording
```

`provinces.svg` bytes UNCHANGED from M4.20 — the
viewport tag and the @media rule both live in
`render_map_html`. `map.html` bytes did change (one new
meta tag, one new @media block).

## 5. Test coverage

Unit (`tests/systems/svg_export_test.cpp`):

- `render_map_html: M4.21 emits <meta name="viewport">
  with width=device-width + initial-scale=1` — pins the
  exact attribute literal AND its position in `<head>`
  (after `<meta charset>`, before `<title>`).
- `render_map_html: M4.21 <style> block carries the
  @media (max-width: 1040px) responsive rule` — pins
  the breakpoint literal AND the rule body
  (`svg { width: 100%; max-width: 100%; height: auto; }`).
- `render_map_html: M4.21 carries only ONE @media rule
  and ONE viewport tag (no breakpoint cascade)` — count
  `@media` == 1, count `<meta name="viewport"` == 1;
  explicit absence of `@container`, `@supports`,
  `min-width:`, `orientation:`,
  `prefers-color-scheme`, `prefers-reduced-motion`.
- `render_map_html: M4.21 responsive surface is pure
  CSS — no JS resize / matchMedia / ResizeObserver` —
  explicit absence of `"resize"`, `matchMedia`,
  `ResizeObserver`, `onresize`, `window.innerWidth`,
  `window.innerHeight`.
- `render_provinces (standalone SVG) does NOT include
  the M4.21 viewport / media query` — standalone SVG
  carries no `viewport` / `device-width` /
  `initial-scale` / `@media` / `max-width: 1040px`.

The M4.9 / M4.14 / M4.18 integration tests A/B/C/D/E/F
all stay green unchanged — none assert viewport /
media-query absence. M4.10–M4.17 / M4.19 / M4.20 unit
tests stay green — additive surface only.

Verified directly via `./build/bin/Debug/leviathan_tests.exe`
per `feedback_ctest_masks_doctest`: 891 cases, 61678
assertions, 0 failed.

## 6. Cross-references

- [`m4-20-tooltip-skeleton.md`](m4-20-tooltip-skeleton.md)
  — the M4.20 hover-status bar that M4.21 lays out
  alongside the rest of the column. On narrow screens
  the bar inherits the column width via its existing
  `max-width: 1000px; margin: 8px auto` — no M4.21-
  specific rule needed.
- [`m4-19-hover-affordance-skeleton.md`](m4-19-hover-affordance-skeleton.md)
  — the M4.19 :hover CSS. The M4.21 @media block sits
  AFTER M4.19's :hover rules in source order; @media
  rules don't compete with state-rules so ordering
  here is informational only.
- [`m4-16-focus-visible-skeleton.md`](m4-16-focus-visible-skeleton.md)
  — the M4.16 :focus-visible CSS. Same source-order
  rationale.
- [`m4-6-html-viewer-css-skeleton.md`](m4-6-html-viewer-css-skeleton.md)
  — the M4.6 single-CSS-surface rule (no per-element
  inline `style="..."`). M4.21 obeys it: the new @media
  rule lives in the `<style>` block.
- [`m4-5-html-viewer-skeleton.md`](m4-5-html-viewer-skeleton.md)
  — the M4.5 contract that requires `<head>` `<title>`.
  M4.21 adds the viewport meta tag adjacent to the
  existing `<meta charset>` without disturbing the
  M4.5 head shape.
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4 status snapshot; **inline-refreshed in this
  PR** per the `feedback_checkpoint_drift` rule.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  M3 invariants. M4.21 preserves all of them.

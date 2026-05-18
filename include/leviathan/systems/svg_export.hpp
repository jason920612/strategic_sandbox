// SvgExport - minimal deterministic SVG / HTML renderer for
// the M4 map data layer.
//
// M4.2 (M4.x history) shipped the first renderer that turns
// `state.provinces` into a visible deterministic SVG artefact.
// M4.3 layered a deterministic per-owner colour onto the
// existing circles using a fixed 10-entry palette. M4.4 added
// a `<text>` label per node immediately after the matching
// `<circle>`. M4.5 added a minimal HTML5 wrapper around the
// same SVG body so `provinces.svg` is also reachable as
// `map.html` for browser-friendly viewing. M4.6 added the
// smallest possible `<style>` block (three selectors). M4.7
// added a static `<ul class="legend">` after the inline SVG
// (three more CSS selectors). M4.8 widened the identity
// surface (the four `data-*` attributes on both `<circle>`
// and `<text>`). M4.9 was a checkpoint (zero behaviour
// change; three new integration tests + status snapshot).
// M4.10 was the **first M4.x to add JavaScript to map.html**:
// a single stateless inline `<script>` at end of body wires a
// click handler that copies the four data-* values off the
// clicked `<circle>` / `<text>` into a read-only
// `<div id="details">` panel sitting between the SVG and the
// legend. The script uses `addEventListener` +
// `createElement` + `textContent` — never `innerHTML`,
// `eval`, `document.write`, `fetch`, `XMLHttpRequest`, or
// `localStorage` — so attribute values cannot inject markup
// and the viewer remains read-only. `provinces.svg` stays
// fully inert (no `<script>`, no `<style>`, no
// `font-family`). M4.11 (this revision) is a UX polish on
// the M4.10 click handler: the `<dt>` labels rendered into
// the details panel are decoupled from the raw `data-*`
// attribute names. `getAttribute` still reads the M4.8 DOM
// contract keys (`data-id` / `data-owner` / `data-owner-code`
// / `data-name` — those are NOT renamed; the `<circle>` /
// `<text>` surface is byte-identical with M4.10), but each
// `<dt>` now renders a fixed human-readable label
// (`Province ID` / `Owner Index` / `Owner Code` /
// `Province Name`). M4.12 (this revision) layers a
// transient selection highlight on top of the M4.10/M4.11
// click handler: two new CSS rules — `svg circle.selected
// { stroke: #000000; stroke-width: 3; }` and `svg
// text.selected { font-weight: bold; }` — live in the
// M4.6 `<style>` block, and the click handler now also
// calls a `selectProvince` helper that clears all
// `.selected` classes and re-adds `.selected` to every
// node (circle + text) sharing the clicked element's
// data-id. Selection is **purely DOM-level**: never
// written into `GameState`, never persisted across
// reloads, no `localStorage` / `sessionStorage` / cookie /
// URL fragment. M4.13 (this revision) widens the M4.8
// identity surface by one attribute: every `<circle>`
// and every `<text>` now also carries `data-owner-name`,
// resolved from `state.countries[owner.value()].name`
// (or `""` when the owner is invalid — same defensive
// fallback as `data-owner-code`). The M4.11 details
// panel's fields array grows from four entries to five
// — the new row renders as `Owner Name`. No new field
// on `ProvinceNode`; `data-owner-name` is derived from
// `state.countries` at render time, so the save format
// stays v12. M4.15 (this revision) makes the province
// markers reachable from the keyboard. Every `<circle>`
// and every `<text>` now carries `tabindex="0"` (emitted
// in `render_svg_root`, so the standalone provinces.svg
// picks it up too); the inline `<script>` registers a
// `keydown` listener alongside the existing `click`
// listener so pressing Enter or Space while focused on a
// province marker fires the same `selectProvince +
// showDetails` pair the click would. The keydown handler
// calls `event.preventDefault()` for Space to suppress
// the default page-scroll. M4.7 legend swatch `<circle>`
// elements are emitted separately (inside
// render_map_html, not render_svg_root) and lack
// `data-id`, so they stay non-focusable and the
// selector keeps skipping them. NO ARIA polish (no
// `role=` / `aria-label=` / `aria-selected=` /
// `aria-current=`) — those are deferred to a future
// dedicated A11Y sub-milestone. M4.16 (this revision)
// makes M4.15's keyboard focus VISIBLE. Two new CSS
// rules in the M4.6 `<style>` block use the
// `:focus-visible` pseudo-class (NOT bare `:focus`) so
// the highlight appears for keyboard-triggered focus
// only and NOT for mouse clicks — that keeps the
// M4.12 `.selected` highlight (set by activate())
// visually distinct from the M4.16 keyboard-focus
// ring. The circle uses a blue stroke ring; the text
// uses a blue CSS `outline` with `outline-offset`. The
// colour (#1976d2) is chosen to contrast with the M4.3
// owner palette and with the M4.12 black `.selected`
// stroke so the two states don't collide visually.
// Two paired bare-`:focus` rules with `outline: none`
// neutralise the browser's default focus outline so
// the M4.16 styling wins. M4.17 (this revision) makes
// the province markers screen-reader-readable. Every
// `<circle>` and every `<text>` now carries
// `role="button"` + `aria-label="<name>, <owner-name>"`
// (composed before XML-escape, so a single
// `xml_attr_escape` call covers it). When the owner
// index is invalid the label falls back to `<name>`
// alone. The label values match what the M4.10/M4.11
// details panel renders, so a screen-reader user hears
// "Berlin, Germany — button" on focus and gets the
// same node identity sighted users see in the panel.
// This intentionally REVERSES the M4.15/M4.16 "no
// ARIA" non-goal; the still-deferred surface is
// narrower (no `aria-selected` / `aria-current` /
// `aria-pressed` / `aria-live` / `aria-describedby`
// / `aria-labelledby`). M4.19 (this revision) adds a
// mouse-hover affordance. Two new CSS rules in the
// M4.6 `<style>` block: `svg circle:hover { stroke:
// #666666; stroke-width: 2; }` and `svg text:hover
// { text-decoration: underline; }`. The grey 2px
// stroke is visually distinct from the M4.12 black
// 3px `.selected` stroke and the M4.16 blue 4px
// `:focus-visible` ring; text-decoration: underline is
// a different mechanism from `.selected`'s
// `font-weight: bold` and `:focus-visible`'s
// `outline`. The rules sit BEFORE `.selected` /
// `:focus-visible` in CSS source order so those (equal
// specificity, later) win when both apply.
// M4.19 itself is pure CSS — no JS hover handler.
// M4.20 (this revision) adds a **hover-status text bar**
// (`<p id="hover-status" class="hover-status">`) between
// the SVG and the M4.10 details panel; the inline
// `<script>` registers `mouseover` / `mouseout`
// listeners on every `<circle>` + `<text>` (in the same
// per-element loop as the M4.10 click + M4.15 keydown
// listeners) that update `hoverStatus.textContent` to
// the composed string `"<name> (<owner-name>)"` (just
// `<name>` if the owner is missing) and clear it on
// mouseout. Reads the existing M4.8 / M4.13 `data-name`
// and `data-owner-name` attributes via `getAttribute`;
// writes only via `textContent` (XSS-safe — the M4.10
// no-`innerHTML` discipline carries over). **No SVG
// `<title>` child element** anywhere — that would
// compete with the M4.17 `aria-label` as the accessible
// name. M4.20 narrowly reverses the M4.19 "no JS hover
// handler" non-goal in the same shape M4.17 reversed
// the M4.15/M4.16 "no ARIA" non-goal: only `mouseover`
// + `mouseout` ship; `mouseenter` / `mouseleave` /
// `.onmouseover` / `.onmouseout` (inline attribute
// form) stay absent. The hover-status bar is a separate
// element from the M4.10 details panel — hover never
// mutates the panel's content, so the click semantics
// stay clean. M4.21 (this revision) makes `map.html`
// render usably on narrow / mobile screens. Two
// additions: (a) `<meta name="viewport"
// content="width=device-width, initial-scale=1">` in
// `<head>` so mobile browsers lay the page out at the
// device's actual width instead of the default
// desktop-emulation viewport; (b) one small `@media
// (max-width: 1040px)` rule in the `<style>` block
// that scales the SVG to `width: 100%; height: auto`
// so it fits the column without horizontal scroll (the
// existing `viewBox` preserves the aspect ratio). The
// 1040px threshold is the SVG's 1000px content width
// plus 2 * 20px body padding (the SVG's 1px border
// lives INSIDE that padded column, not adding to its
// layout width). **Narrowly reverses the M4.5–M4.20
// "no `<meta name=viewport>`, no media queries"
// non-goal** ── only the one viewport tag and the one
// media query ship; broader responsive work (mobile-
// specific layouts, breakpoint cascade, container
// queries, responsive font sizing, JS resize listener,
// viewport-aware repositioning of details/hover-status
// / legend) stays deferred. No state field, no new
// artefact, no save-schema bump. Future M4
// sub-milestones (pair-hover, live-region
// announcements, position-aware tooltip near cursor,
// persistent selection state, neighbour-adjacency
// lines, terrain, broader responsive layout, etc.)
// will extend the renderer further.
//
// What M4.10 / M4.11 / M4.12 / M4.13 / M4.15 / M4.16 / M4.17 / M4.19 / M4.20 / M4.21 deliberately do NOT do:
//   * No clickable UI / event handlers / hover state.
//   * No tooltips.
//   * No state mutation from the viewer. `map.html` is a
//     READ-ONLY render of `state.provinces` +
//     `state.countries`. The M4.10 click handler reads
//     attributes off the clicked element and writes them
//     into the DOM via `textContent`; it never mutates the
//     underlying `GameState`, never calls `fetch` /
//     `XMLHttpRequest` / `localStorage` / `sessionStorage`
//     / `history.pushState` / `window.location`.
//   * No CSS animations / transitions / media queries /
//     `@import` / `@font-face` — the stylesheet is plain
//     layout only.
//   * **JavaScript boundary is asymmetric since M4.10.**
//     `provinces.svg` stays fully script-free.
//     `map.html` carries EXACTLY ONE inline `<script>`
//     block — the M4.10 click handler. The script is
//     inline only (no `src=`, no `type=`), uses
//     `addEventListener` + `createElement` + `textContent`
//     (no `innerHTML` / `outerHTML` / `document.write` /
//     `eval` / `Function`), and scopes its selector to
//     `svg circle[data-id], svg text[data-id]` so the
//     legend's data-id-less swatch <circle> elements stay
//     non-clickable.
//   * No `<link>` to an external stylesheet (M4.5 constraint
//     preserved; the `<style>` block is inline only).
//   * No inline event attributes (`onclick=` / `onmouseover=`
//     / `onload=` / ...) — M4.5 constraint preserved. The
//     M4.10 handler uses `addEventListener`, not inline
//     attribute syntax.
//   * No inline `style="..."` attributes on individual
//     elements — the `<style>` block is the single CSS
//     surface (M4.6 constraint preserved).
//   * No `<meta name="viewport">` — responsive sizing stays
//     a future sub-milestone.
//   * No per-province colour override.
//   * No ownership-dynamics layer (provinces are static; the
//     renderer reads `owner` and never writes it).
//   * No neighbour / adjacency edges.
//   * No terrain / resources / population overlays.
//   * No events / AI / command integration.
//   * No CLI flag — both artefacts are unconditional, in the
//     same shape as `interest_groups.csv` (M3.5), with
//     `RunnerOptions::provinces_svg_path` (M4.2) and
//     `RunnerOptions::map_html_path` (M4.5) as optional
//     programmatic overrides.
//   * No new save-format field (the renderer reads existing
//     `state.provinces` + `state.countries`; save format
//     stays v12).
//   * `provinces.svg` bytes DID change at M4.8 (added
//     `data-owner-code` / `data-name` to every `<circle>`
//     and the four data-* attributes to every `<text>`)
//     and again at M4.13 (added `data-owner-name` to both).
//     Each such change is **additive only** (no removed
//     attributes, no rendered-pixel movement); SVG-to-PNG
//     pipelines and vector tools see identical visuals.
//     M4.5 / M4.6 / M4.7 / M4.10 / M4.11 / M4.12's "no
//     change to provinces.svg" guarantee resumes between
//     each additive widening; the byte-identical
//     determinism contract (same state → same bytes) is
//     unchanged.
//   * No font-family / font-size / fill on `<text>` elements
//     themselves (M4.4 contract preserved) — the only font
//     change is the M4.6 CSS `svg text { font-family:
//     sans-serif; }` rule plus the M4.7 `legend { ... }`
//     rules, both of which only affect the HTML viewer.
//
// Output shape (M4.5):
//   * `provinces.svg`:
//       - `<?xml version="1.0" encoding="UTF-8"?>` prolog.
//       - SVG 1.1 root with viewBox `0 0 1000 1000`.
//       - For each ProvinceNode, two paired elements emitted
//         in `state.provinces` order:
//           1. `<circle cx=... cy=... r="8" fill=...
//              tabindex="0" role="button" aria-label=...
//              data-id=... data-owner=... data-owner-code=...
//              data-owner-name=... data-name=.../>`
//              (M4.2 + M4.3 + M4.8 + M4.13 + M4.15 + M4.17
//              shape).
//           2. `<text x=... y=... text-anchor="middle"
//              tabindex="0" role="button" aria-label=...
//              data-id=... data-owner=... data-owner-code=...
//              data-owner-name=... data-name=...
//              >NAME</text>` with x = cx, y =
//              cy + kLabelYOffset, and NAME the XML-text-
//              escaped `ProvinceNode::name` (M4.4 + M4.8 +
//              M4.13 + M4.15 + M4.17 shape).
//         M4.8 widened the identity surface uniformly so
//         both `<circle>` and `<text>` carry the same
//         `data-*` attributes (`data-id`, `data-owner`,
//         `data-owner-code`, `data-name`) — a future
//         clickable UI / DOM script can address either
//         element without DOM-walking siblings. **M4.13
//         widens the surface by one more attribute**
//         (`data-owner-name`), keeping the same parity
//         between circle + text. `data-owner-code` and
//         `data-owner-name` both resolve to the matching
//         `state.countries[owner.value()]` field (`id_code`
//         and `name` respectively) when the owner index is
//         valid, or the empty string otherwise
//         (defense-in-depth for hand-built states; save /
//         scenario layers reject invalid owners at load
//         time). The single bounds check covers both
//         lookups so they cannot disagree about validity.
//         All five data-* values are XML-attribute-escaped
//         (M4.2 helper). **M4.15** adds `tabindex="0"` to
//         both `<circle>` and `<text>` so keyboard users
//         can Tab through the province markers; the
//         attribute is a fixed literal, not an
//         attribute-escape site. Legend swatch `<circle>`
//         elements in `map.html` are emitted separately
//         (inside `render_map_html`) and lack `tabindex`,
//         so they stay out of the tab order.
//         **M4.17** adds `role="button"` + `aria-label`
//         to both `<circle>` and `<text>`. `role="button"`
//         is a fixed literal; `aria-label` is composed at
//         render time as `<name>` (when owner is invalid)
//         or `<name>, <owner-name>` (when owner resolves),
//         then XML-attribute-escaped as a single value via
//         the M4.2 helper so a name containing `& < > " '`
//         cannot break the attribute syntax. Legend swatch
//         `<circle>` elements do NOT carry `role` /
//         `aria-label` — they stay decorative.
//   * `map.html` (M4.5 new; M4.6 adds CSS; M4.7 adds legend;
//     M4.10 adds clickable details panel + click handler):
//       - `<!DOCTYPE html>` + `<html lang="en">` + minimal
//         `<head>` (`<meta charset="UTF-8">` + `<meta
//         name="viewport" content="width=device-width,
//         initial-scale=1">` (M4.21) + `<title>` +
//         `<style>` block).
//       - The `<style>` block carries the M4.6 / M4.7 rules
//         plus four M4.10 rules:
//         `body { margin: 0; padding: 20px;
//                 background-color: #f0f0f0; }`           (M4.6)
//         `svg  { display: block; margin: 0 auto;
//                 border: 1px solid #888;
//                 background-color: #ffffff; }`           (M4.6)
//         `svg text { font-family: sans-serif; }`         (M4.6)
//         `.legend { list-style: none; padding: 0;
//                    margin: 20px auto; max-width: 1000px;
//                    font-family: sans-serif; }`          (M4.7)
//         `.legend li { display: flex; align-items: center;
//                       margin: 4px 0; }`                 (M4.7)
//         `.legend .swatch { width: 16px; height: 16px;
//                            margin-right: 8px;
//                            flex-shrink: 0; }`           (M4.7)
//         `.details { margin: 20px auto; max-width: 1000px;
//                     font-family: sans-serif; }`         (M4.10)
//         `.details dl { margin: 0; }`                    (M4.10)
//         `.details dt { font-weight: bold; }`            (M4.10)
//         `.details dd { margin: 0 0 8px 0; }`            (M4.10)
//         `.details-empty { color: #666; font-style: italic; }`
//                                                         (M4.10)
//         `.hover-status { max-width: 1000px;
//             margin: 8px auto; min-height: 1em;
//             font-family: sans-serif; color: #666;
//             font-style: italic; text-align: center; }`  (M4.20)
//         `svg circle[data-id], svg text[data-id]
//            { cursor: pointer; }`                        (M4.10)
//         `svg circle:hover
//            { stroke: #666666; stroke-width: 2; }`       (M4.19)
//         `svg text:hover
//            { text-decoration: underline; }`             (M4.19)
//         `svg circle.selected
//            { stroke: #000000; stroke-width: 3; }`       (M4.12)
//         `svg text.selected { font-weight: bold; }`      (M4.12)
//         `svg circle:focus { outline: none; }`           (M4.16)
//         `svg circle:focus-visible
//            { outline: none; stroke: #1976d2;
//              stroke-width: 4; }`                        (M4.16)
//         `svg text:focus { outline: none; }`             (M4.16)
//         `svg text:focus-visible
//            { outline: 2px solid #1976d2;
//              outline-offset: 2px; }`                    (M4.16)
//         `@media (max-width: 1040px) {
//             svg { width: 100%; max-width: 100%;
//                   height: auto; } }`                    (M4.21)
//       - `<body>` contains, in order:
//           1. The **exact same** `<svg>...</svg>` body as
//              `provinces.svg`, but WITHOUT the XML prolog
//              (which is invalid inside HTML).
//           2. (M4.20) A `<p id="hover-status"
//              class="hover-status">&nbsp;</p>` text bar.
//              Initial body is a non-breaking space so the
//              line takes layout space without text. The
//              inline `<script>` updates its `textContent`
//              on `mouseover` of a province marker to
//              `"<name> (<owner-name>)"` (or just
//              `<name>` when owner-name is empty) and
//              clears it back to `""` on `mouseout`. Reads
//              only via `getAttribute`; writes only via
//              `textContent`. Never touches the M4.10
//              details panel — hover and click are
//              independent UI surfaces.
//           3. (M4.10) A `<div id="details" class="details">`
//              placeholder, initially containing a single
//              `<p class="details-empty">` instructing the
//              viewer to click a province. The M4.10 click
//              handler replaces the div's contents with a
//              `<dl>` whose `<dd>` cells carry the clicked
//              element's `data-id` / `data-owner` /
//              `data-owner-code` / (M4.13) `data-owner-name`
//              / `data-name` values respectively, read via
//              `getAttribute` and written via
//              `createElement` + `textContent` (XSS-safe).
//              M4.11: the `<dt>` labels are decoupled from
//              the raw attribute names and render the fixed
//              human-readable strings `Province ID` /
//              `Owner Index` / `Owner Code` /
//              `Owner Name` (M4.13) / `Province Name`.
//              `getAttribute` still uses
//              the raw data-* keys (the M4.8 DOM contract
//              is unchanged); only the labels rendered into
//              `<dt>` cells change.
//           4. (M4.7) A `<ul class="legend">`. One `<li
//              data-owner="N">` per entry in
//              `state.countries`, in vector order. Each
//              `<li>` carries a tiny 16x16 inline SVG
//              swatch (`<svg class="swatch"
//              viewBox="0 0 16 16" width="16" height="16">
//              <circle cx="8" cy="8" r="8" fill="..."/>
//              </svg>`) coloured by
//              `color_for_owner(CountryId{i})` and the
//              text `"<id_code> &mdash; <name>"` (both
//              XML-text-escaped). Empty `state.countries`
//              produces an empty `<ul>` (always-present
//              file contract preserved). The legend
//              swatch `<circle>` elements carry NO
//              `data-id` and therefore stay non-clickable
//              under the M4.10 selector.
//           5. (M4.10) A single inline `<script>` IIFE at
//              the very end of `<body>`. The script attaches
//              one `click` listener per
//              `svg circle[data-id], svg text[data-id]`
//              element. It uses `getAttribute` + `createElement`
//              + `textContent` only — no `innerHTML` /
//              `outerHTML` / `document.write` / `eval` /
//              `Function`, no `fetch` / `XMLHttpRequest`, no
//              `localStorage` / `sessionStorage`, no
//              `history.pushState`, no `window.location`
//              navigation. The script never mutates the
//              underlying `GameState`; it only repaints the
//              `<div id="details">` placeholder. M4.12: the
//              click listener also calls a `selectProvince`
//              helper that uses `classList.remove("selected")`
//              on every prior `.selected` node and
//              `classList.add("selected")` on every node
//              sharing the clicked element's `data-id` (so
//              clicking either the circle or the text
//              highlights the whole province pair). The
//              initial render carries NO `class="selected"`
//              anywhere; selection is purely DOM-level and
//              lost on reload. **M4.15**: the same per-node
//              loop now also registers a `keydown` listener.
//              When `event.key === "Enter"` or
//              `event.key === " "`, it calls
//              `event.preventDefault()` (suppresses Space-
//              scroll) and runs the same `selectProvince +
//              showDetails` pair the `click` listener runs.
//              The two listeners share a per-element
//              `activate()` closure so the effect cannot
//              drift between input modalities.
//              **M4.20**: the same loop ALSO registers
//              `mouseover` + `mouseout` listeners that
//              update the M4.20 `<p id="hover-status">`
//              text bar via `textContent` (composed string
//              `"<name> (<owner-name>)"` from
//              `getAttribute("data-name")` +
//              `getAttribute("data-owner-name")`). Hover
//              never mutates the M4.10 details panel —
//              hover and click are independent surfaces.
//              `mouseenter` / `mouseleave` /
//              `.onmouseover` / `.onmouseout` (inline
//              attribute form) deliberately NOT used.
//   * `<circle>` and `<text>` are interleaved (one of each per
//     node, in that order) — keeps each node's elements
//     grouped in the byte stream and matches the human
//     mental model "a province is a node + a label".
//   * Insertion order follows `state.provinces` (no sorting).
//   * LF line terminators, fixed two-space indent. Coordinates
//     emitted via `std::fixed` + `setprecision(2)` so output
//     is byte-stable across platforms.
//   * `data-id` is XML-attribute-escaped (M4.2 review fix —
//     escapes `& < > " '`). Label content uses the strict-XML
//     text-content escape (`& < >` only) since `" '` are legal
//     inside text content; both helpers live in the .cpp
//     anonymous namespace.
//   * Empty `state.provinces` produces a header-only SVG (and
//     therefore a header-only-SVG-inside-the-HTML wrapper) —
//     both files are still written so the artefact contract
//     is consistent (always-present files).

#ifndef LEVIATHAN_SYSTEMS_SVG_EXPORT_HPP
#define LEVIATHAN_SYSTEMS_SVG_EXPORT_HPP

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::svg_export {

// SVG coordinate range. Normalised `[0, 1]` x / y on
// `ProvinceNode` map to `[0, kSvgCoordScale]` in the output.
inline constexpr double kSvgCoordScale = 1000.0;

// M4.4: vertical offset (in SVG coordinate units) from a
// node's circle centre to its label's baseline. With the M4.2
// circle radius of 8 and the default SVG `<text>` font (16px
// sans-serif on most renderers), 22 puts the label baseline
// roughly 14 units below the circle's bottom edge — visually
// clear of the node without overlapping the next row of
// nodes at the canonical scenario's density. Exposed publicly
// so tests can compute the expected y position without
// hard-coding the constant at every assertion site.
inline constexpr double kLabelYOffset = 22.0;

// M4.3 deterministic per-owner palette. Fixed 10-entry table
// of hex-RGB colours chosen to be visually distinct under
// default `circle` rendering. A future sub-milestone can grow
// the table or swap the strategy (e.g. hash-based) — until
// then this mapping is load-bearing for the SVG byte-stable
// determinism contract. Entries must NOT be reordered or
// changed without a coordinated test / fixture / golden-file
// update; appending new entries at the end is safe (existing
// owner indices keep their colour as long as the table grows
// monotonically).
inline constexpr std::array<std::string_view, 10> kOwnerPalette = {
    "#4682b4",   // 0  steel blue   (canonical GER)
    "#cd5c5c",   // 1  indian red   (canonical FRA)
    "#daa520",   // 2  goldenrod    (canonical JPN)
    "#2e8b57",   // 3  sea green
    "#9370db",   // 4  medium purple
    "#ff8c00",   // 5  dark orange
    "#008080",   // 6  teal
    "#ff1493",   // 7  deep pink
    "#6b8e23",   // 8  olive drab
    "#6a5acd",   // 9  slate blue
};

inline constexpr std::size_t kOwnerPaletteSize = kOwnerPalette.size();

// Defensive fallback for an invalid (`CountryId::invalid()` /
// negative) owner. The save layer rejects such entries at load
// time so canonical pipelines never hit this branch; the
// fallback exists so the renderer can never emit malformed
// (missing-attribute or out-of-range-index) SVG even on
// hand-constructed states.
inline constexpr std::string_view kOwnerFallbackFill = "#888888";

// Return the deterministic fill colour for an owner CountryId.
// `owner.value() < 0` (invalid sentinel) returns the fallback;
// any non-negative value returns
// `kOwnerPalette[value % kOwnerPaletteSize]`.
//
// Exposed publicly so callers / tests can compute the
// expected colour for a given owner without re-deriving the
// modulo + table lookup at every test site.
std::string_view color_for_owner(core::CountryId owner);

// Pure render: turn `state.provinces` into the SVG document
// described in the file header. Never fails; always returns a
// valid SVG string (header-only when `state.provinces` is
// empty).
//
// The returned string ends with a single trailing newline and
// uses LF line terminators throughout.
std::string render_provinces(const core::GameState& state);

// Render + write to disk. Returns failure on any filesystem
// error (with the path in the message). The intermediate
// string is exactly what `render_provinces(state)` would
// produce, so the on-disk contents are byte-stable across
// repeat calls with the same state.
//
// Parent directories are created as needed (mirrors the
// existing `save_system::save` and CSV-writer behaviour).
core::Result<bool> write_provinces(const core::GameState& state,
                                   const std::filesystem::path& path);

// M4.5: pure render of a minimal HTML5 wrapper around the
// inline SVG body. The `<body>` contains exactly the same
// `<svg>...</svg>` element `render_provinces` emits, but
// WITHOUT the leading `<?xml ...?>` prolog (which is invalid
// inside an HTML document). No CSS, no JavaScript, no
// `<style>` / `<script>` / `<link>` / inline event
// attributes — M4.5 ships the minimum viewer that opens
// `state.provinces` in a browser without the raw-XML
// chrome standalone `.svg` files attract.
//
// Never fails; always returns a valid HTML string. The
// returned string ends with a single trailing newline and
// uses LF line terminators throughout.
//
// Same-state byte-stability holds for this output too (the
// underlying `render_svg_root` is deterministic and the HTML
// wrapper is a fixed string).
std::string render_map_html(const core::GameState& state);

// Render + write to disk. Returns failure on any filesystem
// error (with the path in the message). Parent directories
// are created as needed. The on-disk contents are exactly
// what `render_map_html(state)` would produce.
core::Result<bool> write_map_html(const core::GameState& state,
                                  const std::filesystem::path& path);

}  // namespace leviathan::systems::svg_export

#endif  // LEVIATHAN_SYSTEMS_SVG_EXPORT_HPP

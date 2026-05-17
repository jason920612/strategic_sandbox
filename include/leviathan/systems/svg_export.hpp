// SvgExport - minimal deterministic SVG / HTML renderer for
// the M4 map data layer.
//
// M4.2 (M4.x history) shipped the first renderer that turns
// `state.provinces` into a visible deterministic SVG artefact.
// M4.3 layered a deterministic per-owner colour onto the
// existing circles using a fixed 10-entry palette indexed by
// `owner.value() % kOwnerPaletteSize`. M4.4 added a `<text>`
// label per node immediately after the matching `<circle>`.
// M4.5 added a minimal HTML5 wrapper around the same SVG body
// so `provinces.svg` is also reachable as `map.html` for
// browser-friendly viewing. M4.6 added the smallest possible
// `<style>` block to the M4.5 wrapper (three selectors).
// M4.7 added a static `<ul class="legend">` after the inline
// SVG (three more CSS selectors for layout). M4.8 (this
// revision) widens the identity surface inside the SVG body:
// every `<circle>` and every `<text>` now carries the same
// four read-only `data-*` attributes (`data-id`,
// `data-owner`, `data-owner-code`, `data-name`) so a future
// clickable UI / DOM script can address either element
// uniformly without DOM-walking sibling nodes. This is the
// first M4.x sub-milestone since M4.4 that intentionally
// changes the bytes of `provinces.svg` — the change is purely
// additive (new attributes on existing elements; no rendered
// pixels move), so downstream consumers (SVG-to-PNG
// pipelines, vector tools) see no visual difference. Future
// M4 sub-milestones (clickable UI, hover state,
// neighbour-adjacency lines, terrain, etc.) will extend the
// renderer further.
//
// What M4.8 deliberately does NOT do:
//   * No clickable UI / event handlers / hover state.
//   * No tooltips.
//   * No state mutation from the viewer (`map.html` is a
//     read-only render of `state.provinces` + `state.countries`).
//   * No CSS animations / transitions / media queries /
//     `@import` / `@font-face` — the M4.7 stylesheet is
//     plain layout only.
//   * No JavaScript / `<script>` (M4.5 constraint preserved).
//   * No `<link>` to an external stylesheet (M4.5 constraint
//     preserved; the `<style>` block is inline only).
//   * No inline event attributes (`onclick` / `onmouseover` /
//     `onload` / ...) — M4.5 constraint preserved.
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
//   * `provinces.svg` bytes DID change at M4.8 — the new
//     `data-owner-code` / `data-name` attributes appear on
//     every `<circle>` and the four data-* attributes appear
//     on every `<text>`. This is the first M4.x sub-milestone
//     since M4.4 that deliberately edits the standalone SVG
//     body. The change is **additive only** (no removed
//     attributes, no rendered-pixel movement); SVG-to-PNG
//     pipelines and vector tools see identical visuals.
//     M4.5 / M4.6 / M4.7's "no change to provinces.svg"
//     guarantee no longer applies; the byte-identical
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
//           1. `<circle cx=... cy=... r="8" fill=... data-id=...
//              data-owner=... data-owner-code=... data-name=.../>`
//              (M4.2 + M4.3 + M4.8 shape).
//           2. `<text x=... y=... text-anchor="middle"
//              data-id=... data-owner=... data-owner-code=...
//              data-name=...>NAME</text>` with x = cx, y =
//              cy + kLabelYOffset, and NAME the XML-text-
//              escaped `ProvinceNode::name` (M4.4 + M4.8
//              shape).
//         M4.8 widens the identity surface uniformly: both
//         `<circle>` and `<text>` carry the same four
//         `data-*` attributes (`data-id`, `data-owner`,
//         `data-owner-code`, `data-name`) so a future
//         clickable UI / DOM script can address either
//         element without DOM-walking siblings.
//         `data-owner-code` resolves to
//         `state.countries[owner.value()].id_code` when the
//         owner index is valid, or the empty string
//         otherwise (defense-in-depth for hand-built states;
//         save / scenario layers reject invalid owners at
//         load time). All four data-* values are
//         XML-attribute-escaped (M4.2 helper).
//   * `map.html` (M4.5 new; M4.6 adds CSS; M4.7 adds legend):
//       - `<!DOCTYPE html>` + `<html lang="en">` + minimal
//         `<head>` (`<meta charset="UTF-8">` + `<title>` +
//         `<style>` block).
//       - The `<style>` block carries six rules:
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
//       - `<body>` contains:
//           1. The **exact same** `<svg>...</svg>` body as
//              `provinces.svg`, but WITHOUT the XML prolog
//              (which is invalid inside HTML).
//           2. (M4.7) A `<ul class="legend">` immediately
//              after the inline SVG. One `<li
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
//              file contract preserved).
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

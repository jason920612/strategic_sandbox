// SvgExport - minimal deterministic SVG renderer for the M4 map
// data layer.
//
// M4.2 is the next step after M4.1's typed ProvinceNode data
// layer: it turns the data into a visible, deterministic SVG
// artefact that future M4 sub-milestones (HTML viewer,
// clickable map, colour-by-owner, neighbour-adjacency lines,
// terrain, etc.) will extend.
//
// What M4.2 deliberately does NOT do:
//   * No HTML viewer / wrapper.
//   * No clickable UI / event handlers / hover state.
//   * No map colours (every node renders with the same default
//     fill — no per-country variation, no ownership-driven
//     palette).
//   * No ownership-dynamics layer (provinces are static).
//   * No neighbour / adjacency edges.
//   * No terrain / resources / population overlays.
//   * No events / AI / command integration.
//   * No CLI flag — the new artefact is unconditional in the
//     same shape as `interest_groups.csv` (M3.5), with an
//     optional programmatic path override on `RunnerOptions`.
//   * No new save-format field (the renderer reads existing
//     `state.provinces`).
//
// Output shape:
//   * SVG 1.1 root with viewBox `0 0 1000 1000`.
//   * One `<circle>` per ProvinceNode at
//     `cx = round(node.x * 1000, 2)`, `cy = round(node.y * 1000, 2)`,
//     `r = 8`, fixed black fill.
//   * Each `<circle>` carries `data-id="<id_code>"` and
//     `data-owner="<int>"` so a future renderer / tester can
//     identify nodes without parsing presentation values.
//   * Insertion order follows `state.provinces` (no sorting).
//   * Two whitespace-significant rules: LF line terminators,
//     fixed two-space indent. Coordinates emitted via
//     `std::fixed` + `setprecision(2)` so output is
//     byte-stable across platforms.
//   * Empty `state.provinces` produces a header-only SVG —
//     the `<svg>` element is still written so the artefact
//     contract is consistent (always-present file).

#ifndef LEVIATHAN_SYSTEMS_SVG_EXPORT_HPP
#define LEVIATHAN_SYSTEMS_SVG_EXPORT_HPP

#include <filesystem>
#include <string>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::svg_export {

// SVG coordinate range. Normalised `[0, 1]` x / y on
// `ProvinceNode` map to `[0, kSvgCoordScale]` in the output.
inline constexpr double kSvgCoordScale = 1000.0;

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

}  // namespace leviathan::systems::svg_export

#endif  // LEVIATHAN_SYSTEMS_SVG_EXPORT_HPP

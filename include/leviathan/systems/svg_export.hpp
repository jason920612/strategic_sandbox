// SvgExport - minimal deterministic SVG renderer for the M4 map
// data layer.
//
// M4.2 (predecessor) shipped the first renderer that turns
// `state.provinces` into a visible deterministic SVG artefact.
// M4.3 (this revision) layers a deterministic per-owner colour
// onto the existing circles using a fixed 10-entry palette
// indexed by `owner.value() % kOwnerPaletteSize`. Nothing else
// about the SVG shape changes — same viewBox, same circles,
// same `data-id` / `data-owner` attributes, same coord
// precision. Future M4 sub-milestones (HTML viewer, clickable
// map, neighbour-adjacency lines, terrain, labels / legend,
// etc.) will extend the renderer further.
//
// What M4.3 deliberately does NOT do:
//   * No HTML viewer / wrapper.
//   * No clickable UI / event handlers / hover state.
//   * No legend / colour key inside the SVG.
//   * No labels / text elements.
//   * No per-province colour override (the palette is owner-
//     keyed only; future per-province overlays land in their
//     own sub-milestone).
//   * No ownership-dynamics layer (provinces are static; the
//     renderer reads `owner` and never writes it).
//   * No neighbour / adjacency edges.
//   * No terrain / resources / population overlays.
//   * No events / AI / command integration.
//   * No CLI flag — the artefact is still unconditional in the
//     same shape as `interest_groups.csv` (M3.5), with the
//     existing `RunnerOptions::provinces_svg_path` optional
//     override carried forward unchanged.
//   * No new save-format field (the renderer reads existing
//     `state.provinces`; save format stays v12).
//   * No change to the M4.2 artefact set count (still 9).
//
// Output shape (M4.3):
//   * SVG 1.1 root with viewBox `0 0 1000 1000`.
//   * One `<circle>` per ProvinceNode at
//     `cx = round(node.x * 1000, 2)`, `cy = round(node.y * 1000, 2)`,
//     `r = 8`, `fill` selected by owner via
//     `kOwnerPalette[node.owner.value() % kOwnerPaletteSize]`.
//     Invalid owners (negative index — should never happen at
//     the save layer, which rejects them) get a defensive
//     fallback fill of `#888888`.
//   * Each `<circle>` carries `data-id="<id_code>"` and
//     `data-owner="<int>"` so a future renderer / tester can
//     identify nodes without parsing presentation values.
//   * Insertion order follows `state.provinces` (no sorting).
//   * LF line terminators, fixed two-space indent. Coordinates
//     emitted via `std::fixed` + `setprecision(2)` so output
//     is byte-stable across platforms.
//   * `data-id` is XML-attribute-escaped (M4.2 review fix).
//   * Empty `state.provinces` produces a header-only SVG —
//     the `<svg>` element is still written so the artefact
//     contract is consistent (always-present file).

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

}  // namespace leviathan::systems::svg_export

#endif  // LEVIATHAN_SYSTEMS_SVG_EXPORT_HPP

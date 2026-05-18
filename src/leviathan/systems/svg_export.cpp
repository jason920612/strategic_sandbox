#include "leviathan/systems/svg_export.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace leviathan::systems::svg_export {

namespace {

// Emit a coord as a fixed two-decimal-place string. Byte-stable
// across platforms (`std::fixed` + `setprecision(2)`).
std::string fmt_coord(double v) {
    std::ostringstream oss;
    oss << std::fixed;
    oss.precision(2);
    oss << v;
    return oss.str();
}

// XML attribute-value escaping. `ProvinceNode::id_code` only
// guarantees non-empty string; the M3.1 / M4.1 save + scenario
// loader do NOT restrict the character set, so the renderer
// MUST escape `&`, `<`, `>`, `"`, `'` to keep the output valid
// XML and prevent attribute injection (an id_code containing
// `"` would otherwise close the attribute early). Numeric fields
// (`data-owner` from `CountryId::value()`, the coord doubles)
// are emitted via integer / fixed-precision formatters and
// don't need escaping.
std::string xml_attr_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += ch;       break;
        }
    }
    return out;
}

// XML text-content escaping. M4.4 emits `<text>` elements
// whose body is the XML-escaped `ProvinceNode::name`. Per the
// XML 1.0 spec §2.4, text content only requires `&`, `<`, and
// (technically only when followed by `>`) `>` to be escaped;
// `"` and `'` are legal as literals inside text content. We
// keep the helper separate from `xml_attr_escape` so the
// escape sets are explicit at each call site — the attribute
// helper is a superset and would also work, but the strict
// text-content helper produces shorter output and matches
// standard XML library conventions.
std::string xml_text_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;";  break;
            case '>': out += "&gt;";  break;
            default:  out += ch;      break;
        }
    }
    return out;
}

// M4.5: render just the `<svg>...</svg>` element body of the
// scene — no XML prolog, no HTML wrapper. Shared by:
//
//   * `render_provinces` (M4.2 standalone-SVG path), which
//     prepends the `<?xml ...?>` declaration so the output is
//     a self-contained SVG document on disk.
//   * `render_map_html` (M4.5 HTML wrapper), which inlines
//     this body inside an HTML5 `<body>` element — the XML
//     declaration is invalid inside an HTML document, so the
//     helper must NOT emit it.
//
// The byte stream produced here is byte-identical regardless
// of caller (same `state` → same bytes), which is what keeps
// the M1.17 / M2.22 / M3.7 byte-identical determinism
// contracts valid for both `provinces.svg` and `map.html`.
std::string render_svg_root(const core::GameState& state) {
    std::ostringstream out;

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\""
           " viewBox=\"0 0 1000 1000\""
           " width=\"1000\" height=\"1000\">\n";

    // One <circle> + one <text> per node, in vector order.
    // Fill is selected by owner via `color_for_owner` (M4.3);
    // the <text> (M4.4) is anchored at (cx, cy + kLabelYOffset)
    // with `text-anchor="middle"` and the XML-text-escaped
    // `ProvinceNode::name` as content. M4.8 widens the
    // identity surface on both elements: every <circle> and
    // every <text> now carries the same four `data-*`
    // attributes (`data-id`, `data-owner`, `data-owner-code`,
    // `data-name`) so a future clickable UI / DOM script can
    // address either element uniformly. The pair stays
    // interleaved (circle then text per node) so each node's
    // elements stay grouped in the byte stream.
    for (const auto& p : state.provinces) {
        const double cx_val = p.x * kSvgCoordScale;
        const double cy_val = p.y * kSvgCoordScale;
        const std::string cx = fmt_coord(cx_val);
        const std::string cy = fmt_coord(cy_val);
        const std::string ty = fmt_coord(cy_val + kLabelYOffset);
        const std::string_view fill = color_for_owner(p.owner);

        // M4.8 owner-code lookup: resolve to state.countries
        // entry; emit empty when the owner is invalid or out
        // of range. The save / scenario layers reject such
        // entries at load time, so this only matters for
        // hand-built states. Mirrors the defensive fallback
        // strategy used by `color_for_owner`.
        std::string owner_code;
        const auto owner_v = p.owner.value();
        if (owner_v >= 0 &&
            static_cast<std::size_t>(owner_v) < state.countries.size()) {
            owner_code = state.countries[static_cast<std::size_t>(owner_v)].id_code;
        }
        const std::string id_attr =
            xml_attr_escape(p.id_code);
        const std::string name_attr =
            xml_attr_escape(p.name);
        const std::string owner_code_attr =
            xml_attr_escape(owner_code);
        const std::string owner_int_attr =
            std::to_string(owner_v);

        out << "  <circle"
               " cx=\"" << cx << "\""
               " cy=\"" << cy << "\""
               " r=\"8\""
               " fill=\"" << fill << "\""
               " data-id=\"" << id_attr << "\""
               " data-owner=\"" << owner_int_attr << "\""
               " data-owner-code=\"" << owner_code_attr << "\""
               " data-name=\"" << name_attr << "\""
               "/>\n";
        out << "  <text"
               " x=\"" << cx << "\""
               " y=\"" << ty << "\""
               " text-anchor=\"middle\""
               " data-id=\"" << id_attr << "\""
               " data-owner=\"" << owner_int_attr << "\""
               " data-owner-code=\"" << owner_code_attr << "\""
               " data-name=\"" << name_attr << "\""
               ">" << xml_text_escape(p.name) << "</text>\n";
    }

    out << "</svg>\n";
    return out.str();
}

// Write `body` to `path`. Creates parent directories as
// needed. Used by both `write_provinces` (M4.2) and
// `write_map_html` (M4.5) so the file-creation contract is
// identical for both artefacts.
core::Result<bool> write_string_to(const std::string& body,
                                   const std::filesystem::path& path) {
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return core::Result<bool>::failure(
                path.string() +
                ": create_directories failed: " + ec.message());
        }
    }

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        return core::Result<bool>::failure(
            path.string() + ": cannot open file for writing");
    }
    f.write(body.data(), static_cast<std::streamsize>(body.size()));
    if (!f.good()) {
        return core::Result<bool>::failure(
            path.string() + ": write failed");
    }
    return core::Result<bool>::success(true);
}

}  // namespace

std::string_view color_for_owner(core::CountryId owner) {
    const auto v = owner.value();
    if (v < 0) {
        return kOwnerFallbackFill;
    }
    // The modulo wraps the index back into [0, kOwnerPaletteSize)
    // so very large scenarios reuse colours rather than over-run
    // the table. Two countries with `owner.value() % size`
    // congruent will collide on colour; future sub-milestones
    // can extend the table or swap strategies if the collision
    // becomes a real-world UX problem.
    const auto idx =
        static_cast<std::size_t>(v) % kOwnerPaletteSize;
    return kOwnerPalette[idx];
}

std::string render_provinces(const core::GameState& state) {
    // Standalone SVG document: XML prolog + <svg> root. The
    // prolog is invalid inside an HTML document, which is
    // why render_map_html calls render_svg_root directly
    // instead of stripping the prolog from render_provinces's
    // output.
    return std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n") +
           render_svg_root(state);
}

core::Result<bool> write_provinces(const core::GameState& state,
                                   const std::filesystem::path& path) {
    return write_string_to(render_provinces(state), path);
}

std::string render_map_html(const core::GameState& state) {
    // Minimal HTML5 wrapper around the inline SVG body. M4.5
    // shipped the wrapper with NO CSS / NO JS / NO <style> /
    // NO <script> / NO <link> / NO inline event attributes.
    // M4.6 added the smallest possible `<style>` block (3
    // rules). M4.7 added a static `<ul class="legend">`
    // after the inline SVG (3 more CSS rules). M4.8 widened
    // the SVG identity surface (data-* on both <circle> and
    // <text>). M4.9 was a checkpoint (zero behaviour change).
    // M4.10 (this revision) is the **first M4.x to add
    // JavaScript** to map.html: a single stateless inline
    // `<script>` at end of body wires a click handler that
    // copies the four data-* values of the clicked
    // <circle> / <text> into a read-only "details" panel
    // sitting between the SVG and the legend. provinces.svg
    // stays JS-free / script-free. Map writes are still
    // forbidden — the handler is read-only: it reads
    // attributes and writes them into the DOM via
    // textContent (never innerHTML / never eval). The M4.5
    // "no <link>", "no inline event attributes", "no
    // per-element style=" rules still hold for both
    // artefacts. Full ruleset documented in
    // `svg_export.hpp` and pinned by
    // tests/systems/svg_export_test.cpp.
    std::ostringstream out;
    out << "<!DOCTYPE html>\n";
    out << "<html lang=\"en\">\n";
    out << "<head>\n";
    out << "  <meta charset=\"UTF-8\">\n";
    out << "  <title>Leviathan Map</title>\n";
    out << "  <style>\n";
    out << "  body { margin: 0; padding: 20px;"
           " background-color: #f0f0f0; }\n";
    out << "  svg { display: block; margin: 0 auto;"
           " border: 1px solid #888;"
           " background-color: #ffffff; }\n";
    out << "  svg text { font-family: sans-serif; }\n";
    out << "  .legend { list-style: none; padding: 0;"
           " margin: 20px auto; max-width: 1000px;"
           " font-family: sans-serif; }\n";
    out << "  .legend li { display: flex; align-items: center;"
           " margin: 4px 0; }\n";
    out << "  .legend .swatch { width: 16px; height: 16px;"
           " margin-right: 8px; flex-shrink: 0; }\n";
    // M4.10 details panel layout. Sits between SVG and
    // legend; same max-width as both so the column feels
    // aligned. Cursor: pointer on the data-id-bearing
    // <circle> / <text> tells the user the markers are
    // clickable (no :hover state, no animations).
    out << "  .details { max-width: 1000px;"
           " margin: 20px auto; padding: 12px;"
           " background-color: #ffffff;"
           " border: 1px solid #888;"
           " font-family: sans-serif; }\n";
    out << "  .details dl { margin: 0; }\n";
    out << "  .details dt { font-weight: bold;"
           " margin-top: 4px; }\n";
    out << "  .details dd { margin: 0 0 4px 16px; }\n";
    out << "  .details-empty { margin: 0; color: #666; }\n";
    out << "  svg circle[data-id], svg text[data-id]"
           " { cursor: pointer; }\n";
    // M4.12 selection highlight. The click handler adds the
    // `.selected` class to both the clicked element and its
    // sibling sharing the same data-id, so the whole province
    // pair lights up regardless of whether the user clicked
    // the circle or the label. Black stroke on the circle is
    // visible against every M4.3 palette colour; bold on the
    // text matches a familiar "this row is selected" idiom.
    // No transitions / animations — same constraint as the
    // M4.6 / M4.10 stylesheet.
    out << "  svg circle.selected"
           " { stroke: #000000; stroke-width: 3; }\n";
    out << "  svg text.selected { font-weight: bold; }\n";
    out << "  </style>\n";
    out << "</head>\n";
    out << "<body>\n";
    out << render_svg_root(state);

    // M4.10 details panel placeholder. Empty initial state
    // tells the user the panel exists and what to do.
    out << "<div id=\"details\" class=\"details\">\n";
    out << "  <p class=\"details-empty\">"
           "Click a province to see its details."
           "</p>\n";
    out << "</div>\n";

    // M4.7 legend. One <li> per country, in vector order.
    // Each <li> carries a tiny 16x16 inline SVG swatch
    // coloured by `color_for_owner(CountryId{i})` so the
    // legend matches what render_svg_root paints on each
    // node. Empty state.countries → empty <ul> (kept for
    // consistency with the always-present-file contract;
    // mirrors the empty-state header-only <svg> behaviour).
    out << "<ul class=\"legend\">\n";
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const auto& c = state.countries[i];
        const core::CountryId owner{static_cast<int>(i)};
        const std::string_view fill = color_for_owner(owner);
        out << "  <li data-owner=\"" << static_cast<int>(i) << "\">"
               "<svg class=\"swatch\" viewBox=\"0 0 16 16\""
               " width=\"16\" height=\"16\">"
               "<circle cx=\"8\" cy=\"8\" r=\"8\" fill=\""
            << fill
            << "\"/></svg>"
            << xml_text_escape(c.id_code) << " &mdash; "
            << xml_text_escape(c.name)
            << "</li>\n";
    }
    out << "</ul>\n";

    // M4.10 click-handler script. Inline IIFE; runs after
    // the DOM is fully parsed (the script sits at end of
    // body). Reads four data-* attributes off the clicked
    // <circle> / <text> via getAttribute, writes them into
    // the details panel via document.createElement +
    // textContent so an attribute value containing literal
    // HTML cannot inject markup. The selector specifically
    // requires `[data-id]` so the legend's swatch <circle>
    // elements (which lack data-id) stay non-clickable.
    //
    // M4.11: the <dt> labels are decoupled from the raw
    // data-* attribute names. `getAttribute` still queries
    // the M4.8 DOM contract keys (`data-id` etc.) — those
    // are NOT renamed — but the rendered <dt> body shows a
    // fixed human-readable label so the panel reads as
    // English-language UX instead of leaking the
    // implementation key.
    //
    // M4.12: the click handler also toggles a transient
    // `.selected` class via classList. Clicking either the
    // <circle> or the <text> of a province lights up BOTH
    // elements (the M4.8 widened data-id surface makes the
    // pair lookup a one-line filter). Selection state is
    // purely DOM-level — never written back into GameState,
    // never persisted to localStorage / sessionStorage /
    // history / URL fragment, lost on reload by design.
    out << "<script>\n";
    out << "(function() {\n";
    out << "  var details = document.getElementById(\"details\");\n";
    out << "  if (!details) { return; }\n";
    out << "  var fields = [\n";
    out << "    { attr: \"data-id\",         label: \"Province ID\"   },\n";
    out << "    { attr: \"data-owner\",      label: \"Owner Index\"   },\n";
    out << "    { attr: \"data-owner-code\", label: \"Owner Code\"    },\n";
    out << "    { attr: \"data-name\",       label: \"Province Name\" }\n";
    out << "  ];\n";
    out << "  var nodes = document.querySelectorAll(\n";
    out << "    \"svg circle[data-id], svg text[data-id]\");\n";
    out << "  function showDetails(el) {\n";
    out << "    while (details.firstChild) {\n";
    out << "      details.removeChild(details.firstChild);\n";
    out << "    }\n";
    out << "    var dl = document.createElement(\"dl\");\n";
    out << "    for (var i = 0; i < fields.length; i++) {\n";
    out << "      var f = fields[i];\n";
    out << "      var dt = document.createElement(\"dt\");\n";
    out << "      dt.textContent = f.label;\n";
    out << "      var dd = document.createElement(\"dd\");\n";
    out << "      dd.textContent = el.getAttribute(f.attr) || \"\";\n";
    out << "      dl.appendChild(dt);\n";
    out << "      dl.appendChild(dd);\n";
    out << "    }\n";
    out << "    details.appendChild(dl);\n";
    out << "  }\n";
    out << "  function selectProvince(el) {\n";
    out << "    var id = el.getAttribute(\"data-id\");\n";
    out << "    if (!id) { return; }\n";
    out << "    var prev = document.querySelectorAll(\".selected\");\n";
    out << "    for (var p = 0; p < prev.length; p++) {\n";
    out << "      prev[p].classList.remove(\"selected\");\n";
    out << "    }\n";
    out << "    for (var q = 0; q < nodes.length; q++) {\n";
    out << "      if (nodes[q].getAttribute(\"data-id\") === id) {\n";
    out << "        nodes[q].classList.add(\"selected\");\n";
    out << "      }\n";
    out << "    }\n";
    out << "  }\n";
    out << "  for (var j = 0; j < nodes.length; j++) {\n";
    out << "    var el = nodes[j];\n";
    out << "    el.addEventListener(\"click\","
           " (function(e) { return function() {"
           " selectProvince(e); showDetails(e); }; })(el));\n";
    out << "  }\n";
    out << "})();\n";
    out << "</script>\n";

    out << "</body>\n";
    out << "</html>\n";
    return out.str();
}

core::Result<bool> write_map_html(const core::GameState& state,
                                  const std::filesystem::path& path) {
    return write_string_to(render_map_html(state), path);
}

}  // namespace leviathan::systems::svg_export

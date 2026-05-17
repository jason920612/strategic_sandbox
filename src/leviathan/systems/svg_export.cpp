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
    // `ProvinceNode::name` as content. The pair is interleaved
    // (circle then text per node) so each node's elements stay
    // grouped in the byte stream.
    for (const auto& p : state.provinces) {
        const double cx_val = p.x * kSvgCoordScale;
        const double cy_val = p.y * kSvgCoordScale;
        const std::string cx = fmt_coord(cx_val);
        const std::string cy = fmt_coord(cy_val);
        const std::string ty = fmt_coord(cy_val + kLabelYOffset);
        const std::string_view fill = color_for_owner(p.owner);
        out << "  <circle"
               " cx=\"" << cx << "\""
               " cy=\"" << cy << "\""
               " r=\"8\""
               " fill=\"" << fill << "\""
               " data-id=\"" << xml_attr_escape(p.id_code) << "\""
               " data-owner=\"" << p.owner.value() << "\""
               "/>\n";
        out << "  <text"
               " x=\"" << cx << "\""
               " y=\"" << ty << "\""
               " text-anchor=\"middle\""
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
    // M4.6 adds the smallest possible `<style>` block — three
    // selectors that centre the SVG, give it a card-like
    // border on a neutral page background, and pick a
    // sans-serif font for the labels so they're more
    // readable than the browser's serif default. All other
    // M4.5 nots still hold: no <script>, no <link>, no
    // JavaScript, no inline event attributes, no
    // <meta name="viewport">. The full ruleset is documented
    // in `svg_export.hpp` and pinned by
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
    out << "  </style>\n";
    out << "</head>\n";
    out << "<body>\n";
    out << render_svg_root(state);
    out << "</body>\n";
    out << "</html>\n";
    return out.str();
}

core::Result<bool> write_map_html(const core::GameState& state,
                                  const std::filesystem::path& path) {
    return write_string_to(render_map_html(state), path);
}

}  // namespace leviathan::systems::svg_export

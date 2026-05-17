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
    std::ostringstream out;

    // Header. Always written, even when state.provinces is
    // empty, so the artefact-on-disk contract is consistent.
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
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

core::Result<bool> write_provinces(const core::GameState& state,
                                   const std::filesystem::path& path) {
    const std::string body = render_provinces(state);

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

}  // namespace leviathan::systems::svg_export

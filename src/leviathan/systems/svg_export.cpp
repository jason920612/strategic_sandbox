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

}  // namespace

std::string render_provinces(const core::GameState& state) {
    std::ostringstream out;

    // Header. Always written, even when state.provinces is
    // empty, so the artefact-on-disk contract is consistent.
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\""
           " viewBox=\"0 0 1000 1000\""
           " width=\"1000\" height=\"1000\">\n";

    // One <circle> per node, in vector order.
    for (const auto& p : state.provinces) {
        const std::string cx = fmt_coord(p.x * kSvgCoordScale);
        const std::string cy = fmt_coord(p.y * kSvgCoordScale);
        out << "  <circle"
               " cx=\"" << cx << "\""
               " cy=\"" << cy << "\""
               " r=\"8\""
               " fill=\"black\""
               " data-id=\"" << xml_attr_escape(p.id_code) << "\""
               " data-owner=\"" << p.owner.value() << "\""
               "/>\n";
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

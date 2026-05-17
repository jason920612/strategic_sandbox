#include "leviathan/systems/svg_export.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
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
               " data-id=\"" << p.id_code << "\""
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

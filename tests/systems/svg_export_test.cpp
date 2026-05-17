// M4.2 SvgExport unit tests.
//
// render_provinces is the pure render path (no I/O); write_provinces
// is the render + write path. Both are deterministic — same state →
// byte-identical output every call.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/svg_export.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::GameState;
using leviathan::core::ProvinceNode;
namespace svg = leviathan::systems::svg_export;

namespace {

struct TempDir {
    fs::path path;
    explicit TempDir(std::string name)
        : path(fs::temp_directory_path() / name) {
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

ProvinceNode node(const std::string& id_code, int owner_idx,
                  double x, double y, const std::string& name = "Name") {
    ProvinceNode p;
    p.id_code = id_code;
    p.name    = name;
    p.owner   = CountryId{owner_idx};
    p.x       = x;
    p.y       = y;
    return p;
}

}  // namespace

// ---------------------------------------------------------------------
// render_provinces: shape
// ---------------------------------------------------------------------

TEST_CASE("render_provinces: empty state produces header-only SVG") {
    GameState state;
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("<?xml")                    != std::string::npos);
    CHECK(svg_text.find("<svg ")                    != std::string::npos);
    CHECK(svg_text.find("viewBox=\"0 0 1000 1000\"") != std::string::npos);
    CHECK(svg_text.find("</svg>")                   != std::string::npos);
    CHECK(svg_text.find("<circle")                  == std::string::npos);
    // Stable LF + trailing newline.
    CHECK(svg_text.back() == '\n');
    CHECK(svg_text.find('\r') == std::string::npos);
}

TEST_CASE("render_provinces: one node produces one circle with stable coords") {
    GameState state;
    state.provinces.push_back(node("berlin", 0, 0.52, 0.44, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("<circle")           != std::string::npos);
    CHECK(svg_text.find("cx=\"520.00\"")     != std::string::npos);
    CHECK(svg_text.find("cy=\"440.00\"")     != std::string::npos);
    CHECK(svg_text.find("data-id=\"berlin\"")     != std::string::npos);
    CHECK(svg_text.find("data-owner=\"0\"")        != std::string::npos);
}

TEST_CASE("render_provinces: insertion order is preserved (no sort)") {
    GameState state;
    state.provinces.push_back(node("tokyo",   2, 0.83, 0.55));
    state.provinces.push_back(node("berlin",  0, 0.52, 0.44));
    state.provinces.push_back(node("paris",   1, 0.47, 0.48));
    const std::string svg_text = svg::render_provinces(state);
    const auto tokyo_at  = svg_text.find("tokyo");
    const auto berlin_at = svg_text.find("berlin");
    const auto paris_at  = svg_text.find("paris");
    REQUIRE(tokyo_at  != std::string::npos);
    REQUIRE(berlin_at != std::string::npos);
    REQUIRE(paris_at  != std::string::npos);
    CHECK(tokyo_at  <  berlin_at);
    CHECK(berlin_at <  paris_at);
}

TEST_CASE("render_provinces: deterministic — two calls produce byte-identical output") {
    GameState state;
    state.provinces.push_back(node("berlin", 0, 0.52, 0.44));
    state.provinces.push_back(node("paris",  1, 0.47, 0.48));
    const std::string a = svg::render_provinces(state);
    const std::string b = svg::render_provinces(state);
    CHECK(a == b);
}

// ---------------------------------------------------------------------
// write_provinces: round-trip
// ---------------------------------------------------------------------

TEST_CASE("write_provinces: writes file matching render_provinces output byte-for-byte") {
    TempDir td("leviathan_svg_export_roundtrip");
    GameState state;
    state.provinces.push_back(node("berlin", 0, 0.52, 0.44));
    state.provinces.push_back(node("tokyo",  2, 0.83, 0.55));
    const auto path = td.path / "out.svg";
    REQUIRE(svg::write_provinces(state, path).ok());
    REQUIRE(fs::exists(path));
    const std::string on_disk = read_file(path);
    const std::string in_mem  = svg::render_provinces(state);
    CHECK(on_disk == in_mem);
}

TEST_CASE("write_provinces: creates parent directories") {
    TempDir td("leviathan_svg_export_mkdir");
    GameState state;
    const auto nested = td.path / "a" / "b" / "c" / "out.svg";
    REQUIRE(svg::write_provinces(state, nested).ok());
    CHECK(fs::exists(nested));
}

TEST_CASE("write_provinces: empty state still writes a valid SVG file") {
    TempDir td("leviathan_svg_export_empty");
    GameState state;  // no provinces
    const auto path = td.path / "empty.svg";
    REQUIRE(svg::write_provinces(state, path).ok());
    REQUIRE(fs::exists(path));
    const std::string body = read_file(path);
    CHECK(body.find("<svg ") != std::string::npos);
    CHECK(body.find("</svg>") != std::string::npos);
    CHECK(body.find("<circle") == std::string::npos);
}

TEST_CASE("render_provinces: escapes XML attribute values") {
    // ProvinceNode::id_code is only required to be non-empty by the
    // M3.1 / M4.1 save + scenario loader; nothing restricts the
    // character set. An id_code containing `"` would otherwise close
    // the attribute early and inject arbitrary content into the
    // rendered SVG. All five XML attribute-value metacharacters
    // (& < > " ') must be escaped to their named entities.
    GameState state;
    state.provinces.push_back(
        node("a&b\"c<d>e'f", 0, 0.5, 0.5, "Trouble"));
    const std::string svg_text = svg::render_provinces(state);

    // Every metacharacter is replaced by its entity form.
    CHECK(svg_text.find("a&amp;b&quot;c&lt;d&gt;e&apos;f")
          != std::string::npos);

    // The raw unescaped sequence MUST NOT appear anywhere in the
    // output (would mean we emitted literal closing-quote / tag
    // characters in the attribute body).
    CHECK(svg_text.find("a&b\"c<d>e'f") == std::string::npos);
    // Specifically the early-close attack form (a literal
    // double-quote inside the attribute) MUST NOT appear.
    CHECK(svg_text.find("data-id=\"a&b\"")
          == std::string::npos);
}

// ---------------------------------------------------------------------
// M4.3 - per-owner palette
// ---------------------------------------------------------------------

TEST_CASE("color_for_owner: invalid owner returns the fallback fill") {
    CHECK(svg::color_for_owner(CountryId::invalid()) ==
          svg::kOwnerFallbackFill);
}

TEST_CASE("color_for_owner: indexes the palette directly for small owners") {
    // Each owner.value() less than kOwnerPaletteSize maps to
    // the table entry with the same index.
    for (std::size_t i = 0; i < svg::kOwnerPaletteSize; ++i) {
        CHECK(svg::color_for_owner(CountryId{static_cast<int>(i)}) ==
              svg::kOwnerPalette[i]);
    }
}

TEST_CASE("color_for_owner: wraps via modulo when owner.value() >= palette size") {
    // owner 0 and owner kOwnerPaletteSize share an entry; the
    // wrap is documented as load-bearing in svg_export.hpp.
    CHECK(svg::color_for_owner(CountryId{0}) ==
          svg::color_for_owner(
              CountryId{static_cast<int>(svg::kOwnerPaletteSize)}));
    // 2*size + 3 maps to the same entry as 3.
    CHECK(svg::color_for_owner(CountryId{3}) ==
          svg::color_for_owner(
              CountryId{static_cast<int>(
                  2 * svg::kOwnerPaletteSize + 3)}));
}

TEST_CASE("render_provinces: per-owner fill colour appears on the circle") {
    GameState state;
    // Three owners exercising different palette entries.
    state.provinces.push_back(node("ger_capital", 0, 0.5, 0.5));
    state.provinces.push_back(node("fra_capital", 1, 0.6, 0.6));
    state.provinces.push_back(node("jpn_capital", 2, 0.7, 0.7));
    const std::string svg_text = svg::render_provinces(state);

    // Each owner's palette colour appears at least once.
    CHECK(svg_text.find(std::string("fill=\"") +
                        std::string(svg::kOwnerPalette[0]) + "\"")
          != std::string::npos);
    CHECK(svg_text.find(std::string("fill=\"") +
                        std::string(svg::kOwnerPalette[1]) + "\"")
          != std::string::npos);
    CHECK(svg_text.find(std::string("fill=\"") +
                        std::string(svg::kOwnerPalette[2]) + "\"")
          != std::string::npos);

    // M4.2's hardcoded black is GONE — the new mapping replaced it.
    CHECK(svg_text.find("fill=\"black\"") == std::string::npos);
}

TEST_CASE("render_provinces: same owner across multiple nodes gets the same colour") {
    GameState state;
    state.provinces.push_back(node("ger_a", 0, 0.10, 0.10));
    state.provinces.push_back(node("ger_b", 0, 0.20, 0.20));
    state.provinces.push_back(node("ger_c", 0, 0.30, 0.30));
    const std::string svg_text = svg::render_provinces(state);

    // The expected per-owner colour appears for each node.
    const std::string fill_attr =
        std::string("fill=\"") +
        std::string(svg::kOwnerPalette[0]) + "\"";
    std::size_t hits = 0;
    std::size_t pos  = 0;
    while ((pos = svg_text.find(fill_attr, pos)) != std::string::npos) {
        ++hits;
        pos += fill_attr.size();
    }
    CHECK(hits == 3u);
}

TEST_CASE("render_provinces: invalid owner falls back to the defensive fill") {
    GameState state;
    ProvinceNode p;
    p.id_code = "ghost";
    p.name    = "Ghost";
    p.owner   = CountryId::invalid();  // -1
    p.x       = 0.5;
    p.y       = 0.5;
    state.provinces.push_back(p);
    const std::string svg_text = svg::render_provinces(state);

    CHECK(svg_text.find(std::string("fill=\"") +
                        std::string(svg::kOwnerFallbackFill) + "\"")
          != std::string::npos);
    // And the invalid owner index still appears in data-owner.
    CHECK(svg_text.find("data-owner=\"-1\"") != std::string::npos);
}

// ---------------------------------------------------------------------
// M4.4 - <text> labels per node
// ---------------------------------------------------------------------

TEST_CASE("render_provinces: empty state emits no <text> elements") {
    GameState state;
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("<text") == std::string::npos);
}

TEST_CASE("render_provinces: one node emits one <text> with the node's name") {
    GameState state;
    state.provinces.push_back(node("berlin", 0, 0.52, 0.44, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find(">Berlin</text>") != std::string::npos);
    // text-anchor centres the label on the node horizontally.
    CHECK(svg_text.find("text-anchor=\"middle\"") != std::string::npos);
}

TEST_CASE("render_provinces: <text> is positioned at (cx, cy + kLabelYOffset)") {
    GameState state;
    // x=0.50 / y=0.40 → cx=500.00 / cy=400.00 → ty=422.00.
    state.provinces.push_back(node("a", 0, 0.50, 0.40, "A"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("x=\"500.00\" y=\"422.00\"") != std::string::npos);
    // Sanity: kLabelYOffset is 22 exactly (no surprise rounding).
    CHECK(svg::kLabelYOffset == doctest::Approx(22.0));
}

TEST_CASE("render_provinces: <circle> immediately precedes its matching <text>") {
    GameState state;
    state.provinces.push_back(node("a", 0, 0.10, 0.10, "Alpha"));
    state.provinces.push_back(node("b", 0, 0.20, 0.20, "Beta"));
    const std::string svg_text = svg::render_provinces(state);

    // Find the absolute order of each marker in the string.
    const auto circle_a = svg_text.find("data-id=\"a\"");
    const auto text_a   = svg_text.find(">Alpha</text>");
    const auto circle_b = svg_text.find("data-id=\"b\"");
    const auto text_b   = svg_text.find(">Beta</text>");
    REQUIRE(circle_a != std::string::npos);
    REQUIRE(text_a   != std::string::npos);
    REQUIRE(circle_b != std::string::npos);
    REQUIRE(text_b   != std::string::npos);

    // Interleaved per node, in vector order:
    //   circle_a < text_a < circle_b < text_b
    CHECK(circle_a < text_a);
    CHECK(text_a   < circle_b);
    CHECK(circle_b < text_b);
}

TEST_CASE("render_provinces: <text> content escapes & < > but leaves \" and ' literal") {
    // XML text-content rules: only & < > require escaping;
    // " and ' are legal as literals inside text content.
    GameState state;
    state.provinces.push_back(node("x", 0, 0.5, 0.5, "A&B<C>D\"E'F"));
    const std::string svg_text = svg::render_provinces(state);

    // The escaped form appears.
    CHECK(svg_text.find(">A&amp;B&lt;C&gt;D\"E'F</text>")
          != std::string::npos);
    // The raw unescaped form does NOT appear inside a text body —
    // an unescaped `<` would otherwise look like a start tag.
    CHECK(svg_text.find(">A&B<C>D\"E'F</text>") == std::string::npos);
}

TEST_CASE("render_provinces: empty name still emits a (visually empty) <text> element") {
    // ProvinceNode::name is rejected as empty by the save /
    // scenario layers, but a hand-built state in a unit test can
    // construct one. The renderer stays total: one <text> per
    // node, with empty body.
    GameState state;
    ProvinceNode p;
    p.id_code = "ghost";
    p.name    = "";  // empty on purpose
    p.owner   = CountryId{0};
    p.x       = 0.5;
    p.y       = 0.5;
    state.provinces.push_back(p);
    const std::string svg_text = svg::render_provinces(state);

    // The empty <text> body appears as "><...></text>" form.
    CHECK(svg_text.find("text-anchor=\"middle\"></text>") !=
          std::string::npos);
}

TEST_CASE("render_provinces: label rendering is deterministic across repeat calls") {
    GameState state;
    state.provinces.push_back(node("a", 0, 0.10, 0.10, "Alpha"));
    state.provinces.push_back(node("b", 1, 0.20, 0.20, "Beta&Gamma"));
    state.provinces.push_back(node("c", 2, 0.30, 0.30, "Carat<>"));
    CHECK(svg::render_provinces(state) == svg::render_provinces(state));
}

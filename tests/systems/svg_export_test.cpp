// SvgExport unit tests (M4.2 ... M4.7).
//
// render_provinces is the pure SVG render path (no I/O);
// write_provinces is the render + write path. render_map_html /
// write_map_html are their HTML-wrapped siblings (M4.5 onwards).
// All four are deterministic — same state → byte-identical output
// every call.

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
using leviathan::core::CountryState;
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

CountryState country(int idx, const std::string& id_code,
                     const std::string& name) {
    CountryState c;
    c.id      = CountryId{idx};
    c.id_code = id_code;
    c.name    = name;
    return c;
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

    // The empty <text> body is observable as `data-name=""` on
    // the <text> element (M4.8 widened the identity surface so
    // both <circle> and <text> now carry data-name explicitly),
    // and as an immediate `></text>` close-tag with no body
    // characters between them. Use the data-name="" attribute as
    // the stable anchor since M4.4's "text-anchor=\"middle\">"
    // is no longer immediately followed by `</text>` after the
    // M4.8 attribute additions.
    CHECK(svg_text.find(" data-name=\"\"")     != std::string::npos);
    CHECK(svg_text.find("\"></text>")          != std::string::npos);
    // No literal "ghost" appears as a <text> body — only as
    // attribute values (data-id="ghost").
    CHECK(svg_text.find(">ghost</text>") == std::string::npos);
}

TEST_CASE("render_provinces: label rendering is deterministic across repeat calls") {
    GameState state;
    state.provinces.push_back(node("a", 0, 0.10, 0.10, "Alpha"));
    state.provinces.push_back(node("b", 1, 0.20, 0.20, "Beta&Gamma"));
    state.provinces.push_back(node("c", 2, 0.30, 0.30, "Carat<>"));
    CHECK(svg::render_provinces(state) == svg::render_provinces(state));
}

// ---------------------------------------------------------------------
// M4.5 - HTML viewer wrapping the inline SVG
// ---------------------------------------------------------------------

TEST_CASE("render_map_html: empty state still produces a valid HTML document") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("<!DOCTYPE html>")      != std::string::npos);
    CHECK(html.find("<html lang=\"en\">")    != std::string::npos);
    CHECK(html.find("<meta charset=\"UTF-8\">") != std::string::npos);
    CHECK(html.find("<title>Leviathan Map</title>") != std::string::npos);
    CHECK(html.find("<body>")               != std::string::npos);
    CHECK(html.find("<svg ")                != std::string::npos);
    CHECK(html.find("</svg>")               != std::string::npos);
    CHECK(html.find("</body>")              != std::string::npos);
    CHECK(html.find("</html>")              != std::string::npos);
    // No circles for empty state.
    CHECK(html.find("<circle")              == std::string::npos);
    // Stable LF + trailing newline; no CR (matches the SVG file).
    CHECK(html.back() == '\n');
    CHECK(html.find('\r') == std::string::npos);
}

TEST_CASE("render_map_html: does NOT emit an XML prolog (invalid inside HTML)") {
    // The XML declaration `<?xml ...?>` is only valid for
    // standalone SVG documents. Inside an HTML5 document it would
    // be a parse error for strict consumers. Verify the wrapper
    // strips / never emits it.
    GameState state;
    state.provinces.push_back(node("a", 0, 0.5, 0.5));
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("<?xml") == std::string::npos);
}

TEST_CASE("render_map_html: inlines the same <svg> body as render_provinces") {
    // Whatever bytes `render_provinces` emits for the <svg>
    // root, `render_map_html` must contain the same bytes. The
    // wrapper is responsible only for the surrounding HTML.
    GameState state;
    state.provinces.push_back(node("berlin", 0, 0.52, 0.44, "Berlin"));
    state.provinces.push_back(node("paris",  1, 0.47, 0.48, "Paris"));

    const std::string svg_only = svg::render_provinces(state);
    // Strip the XML prolog from svg_only (everything from the
    // first `<svg` onward is the body the HTML inlines).
    const auto svg_start = svg_only.find("<svg ");
    REQUIRE(svg_start != std::string::npos);
    const std::string svg_body = svg_only.substr(svg_start);

    const std::string html = svg::render_map_html(state);
    CHECK(html.find(svg_body) != std::string::npos);
}

TEST_CASE("render_map_html: no <link>, no inline event attributes, no per-element style") {
    // M4.5 shipped the wrapper with no CSS or JS at all. M4.6
    // added a single `<style>` block (separately tested
    // below). M4.10 added a single inline `<script>` block
    // (separately tested in the M4.10 section). Every other
    // M4.5 "no" still holds: no external stylesheet link, no
    // inline event handlers, no per-element inline style.
    GameState state;
    state.provinces.push_back(node("a", 0, 0.5, 0.5, "Alpha"));
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("<link")   == std::string::npos);
    // Common inline event attributes — none should ever appear.
    // The M4.10 click handler uses addEventListener, not
    // onclick="..." inline attributes.
    CHECK(html.find("onclick=")     == std::string::npos);
    CHECK(html.find("onmouseover=") == std::string::npos);
    CHECK(html.find("onload=")      == std::string::npos);
    // No per-element inline style attributes either — the M4.6
    // `<style>` block is the single CSS surface.
    CHECK(html.find(" style=\"") == std::string::npos);
}

TEST_CASE("render_map_html: deterministic across repeat calls") {
    GameState state;
    state.provinces.push_back(node("a", 0, 0.10, 0.10, "Alpha"));
    state.provinces.push_back(node("b", 1, 0.20, 0.20, "Beta&Gamma"));
    state.provinces.push_back(node("c", 2, 0.30, 0.30, "Carat<>"));
    CHECK(svg::render_map_html(state) == svg::render_map_html(state));
}

TEST_CASE("write_map_html: writes file matching render_map_html byte-for-byte") {
    TempDir td("leviathan_svg_export_html_roundtrip");
    GameState state;
    state.provinces.push_back(node("berlin", 0, 0.52, 0.44, "Berlin"));
    state.provinces.push_back(node("tokyo",  2, 0.83, 0.55, "Tokyo"));
    const auto path = td.path / "out.html";
    REQUIRE(svg::write_map_html(state, path).ok());
    REQUIRE(fs::exists(path));
    const std::string on_disk = read_file(path);
    const std::string in_mem  = svg::render_map_html(state);
    CHECK(on_disk == in_mem);
}

TEST_CASE("write_map_html: creates parent directories") {
    TempDir td("leviathan_svg_export_html_mkdir");
    GameState state;
    const auto nested = td.path / "deep" / "nested" / "map.html";
    REQUIRE(svg::write_map_html(state, nested).ok());
    CHECK(fs::exists(nested));
}

// ---------------------------------------------------------------------
// M4.6 - minimal CSS inside the HTML wrapper
// ---------------------------------------------------------------------

TEST_CASE("render_map_html: emits a <style> block in <head>") {
    // M4.6 adds the smallest possible inline CSS. The block
    // lives in <head> alongside <meta> and <title>.
    GameState state;
    const std::string html = svg::render_map_html(state);
    const auto style_at = html.find("<style>");
    const auto head_at  = html.find("</head>");
    REQUIRE(style_at != std::string::npos);
    REQUIRE(head_at  != std::string::npos);
    // <style> is inside <head> (closes before </head>).
    CHECK(style_at < head_at);
    CHECK(html.find("</style>") != std::string::npos);
}

TEST_CASE("render_map_html: body rule centres + backgrounds the page") {
    // The `body` selector zeroes the browser margin, adds
    // padding for breathing room, and gives the page a
    // neutral grey background so the white SVG card pops.
    GameState state;
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("body {") != std::string::npos);
    CHECK(html.find("margin: 0")              != std::string::npos);
    CHECK(html.find("padding: 20px")          != std::string::npos);
    CHECK(html.find("background-color: #f0f0f0") != std::string::npos);
}

TEST_CASE("render_map_html: svg rule centres the SVG with a border") {
    // The `svg` selector turns the SVG into a centred,
    // bordered "card" on the page.
    GameState state;
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("svg {") != std::string::npos);
    CHECK(html.find("display: block")            != std::string::npos);
    CHECK(html.find("margin: 0 auto")            != std::string::npos);
    CHECK(html.find("border: 1px solid #888")    != std::string::npos);
    CHECK(html.find("background-color: #ffffff") != std::string::npos);
}

TEST_CASE("render_map_html: svg text rule uses sans-serif for label readability") {
    // SVG `<text>` elements deliberately don't carry their
    // own font (M4.4 / M4.5 contract preserved). The M4.6
    // CSS provides the default, fixing the browser's serif
    // fallback which renders small labels poorly.
    GameState state;
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("svg text {") != std::string::npos);
    CHECK(html.find("font-family: sans-serif") != std::string::npos);
}

TEST_CASE("render_provinces (standalone SVG) does NOT include the M4.6 CSS") {
    // The CSS lives in the HTML wrapper only. A standalone
    // SVG file consumed by, e.g., an SVG-to-PNG pipeline
    // must remain free of HTML-only constructs.
    GameState state;
    state.provinces.push_back(node("a", 0, 0.5, 0.5, "Alpha"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("<style")            == std::string::npos);
    CHECK(svg_text.find("font-family")       == std::string::npos);
    CHECK(svg_text.find("background-color")  == std::string::npos);
}

// ---------------------------------------------------------------------
// M4.7 - static legend in the HTML viewer
// ---------------------------------------------------------------------

TEST_CASE("render_map_html: emits a <ul class=\"legend\"> after the inline SVG") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    const std::string html = svg::render_map_html(state);

    const auto svg_close = html.find("</svg>");
    const auto ul_open   = html.find("<ul class=\"legend\">");
    const auto ul_close  = html.find("</ul>");
    REQUIRE(svg_close != std::string::npos);
    REQUIRE(ul_open   != std::string::npos);
    REQUIRE(ul_close  != std::string::npos);

    // <ul> appears AFTER the inline SVG and before </body>.
    CHECK(svg_close < ul_open);
    CHECK(ul_open   < ul_close);
    CHECK(ul_close  < html.find("</body>"));
}

TEST_CASE("render_map_html: empty state.countries → empty <ul class=\"legend\">") {
    // Always-present-file contract: the legend element exists
    // even when there's nothing to enumerate. Mirrors the
    // empty-state header-only <svg> behaviour.
    GameState state;
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("<ul class=\"legend\">") != std::string::npos);
    CHECK(html.find("</ul>")                 != std::string::npos);
    CHECK(html.find("<li ")                  == std::string::npos);
}

TEST_CASE("render_map_html: one country → one <li> with id_code + name + swatch") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    const std::string html = svg::render_map_html(state);

    // The <li> carries the canonical id_code + name text and a
    // matching-coloured swatch.
    CHECK(html.find("data-owner=\"0\"") != std::string::npos);
    CHECK(html.find("GER &mdash; Germany") != std::string::npos);
    const std::string expected_swatch_fill =
        std::string("fill=\"") +
        std::string(svg::kOwnerPalette[0]) + "\"";
    CHECK(html.find(expected_swatch_fill) != std::string::npos);
    // Swatch is a 16x16 inline SVG, not the main 1000x1000 one.
    CHECK(html.find("class=\"swatch\"") != std::string::npos);
    CHECK(html.find("viewBox=\"0 0 16 16\"") != std::string::npos);
}

TEST_CASE("render_map_html: three countries → three <li>s in vector order") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.countries.push_back(country(1, "FRA", "France"));
    state.countries.push_back(country(2, "JPN", "Japan"));
    const std::string html = svg::render_map_html(state);

    const auto ger_at = html.find("GER &mdash; Germany");
    const auto fra_at = html.find("FRA &mdash; France");
    const auto jpn_at = html.find("JPN &mdash; Japan");
    REQUIRE(ger_at != std::string::npos);
    REQUIRE(fra_at != std::string::npos);
    REQUIRE(jpn_at != std::string::npos);
    // Vector order preserved.
    CHECK(ger_at < fra_at);
    CHECK(fra_at < jpn_at);

    // Each owner's palette colour appears at least once
    // (swatches).
    for (int i = 0; i < 3; ++i) {
        const std::string fill =
            std::string("fill=\"") +
            std::string(svg::kOwnerPalette[static_cast<std::size_t>(i)]) +
            "\"";
        CHECK(html.find(fill) != std::string::npos);
    }
}

TEST_CASE("render_map_html: legend text content is XML-text-escaped") {
    // id_code / name are only required to be non-empty by the
    // save layer; the renderer must escape `&`, `<`, `>` so
    // strange country fixtures can't break the document.
    GameState state;
    state.countries.push_back(country(0, "X&Y", "A<B>C"));
    const std::string html = svg::render_map_html(state);

    CHECK(html.find(">X&amp;Y &mdash; A&lt;B&gt;C</li>")
          != std::string::npos);
    // Raw unescaped form must not appear inside a <li> body.
    CHECK(html.find(">X&Y &mdash; A<B>C</li>") == std::string::npos);
}

TEST_CASE("render_map_html: <style> block carries the M4.7 legend CSS rules") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    CHECK(html.find(".legend {")          != std::string::npos);
    CHECK(html.find("list-style: none")   != std::string::npos);
    CHECK(html.find(".legend li {")       != std::string::npos);
    CHECK(html.find("display: flex")      != std::string::npos);
    CHECK(html.find(".legend .swatch {")  != std::string::npos);
    CHECK(html.find("width: 16px")        != std::string::npos);
}

TEST_CASE("render_map_html: legend is deterministic across repeat calls") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.countries.push_back(country(1, "FRA", "France & Co"));
    CHECK(svg::render_map_html(state) == svg::render_map_html(state));
}

TEST_CASE("render_provinces (standalone SVG) does NOT include the M4.7 legend") {
    // Legend lives only in the HTML wrapper. Standalone SVG
    // path stays legend-free / CSS-free for downstream
    // consumers.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("a", 0, 0.5, 0.5, "Alpha"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("<ul")     == std::string::npos);
    CHECK(svg_text.find("legend")  == std::string::npos);
    CHECK(svg_text.find("&mdash;") == std::string::npos);
}

// ---------------------------------------------------------------------
// M4.8 - widened identity surface (data-* attributes on circle + text)
// ---------------------------------------------------------------------

TEST_CASE("render_provinces: <circle> carries M4.8 data-name and data-owner-code attrs") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("data-name=\"Berlin\"")    != std::string::npos);
    CHECK(svg_text.find("data-owner-code=\"GER\"") != std::string::npos);
}

TEST_CASE("render_provinces: <text> carries the same four data-* attrs as <circle>") {
    // M4.8 makes the identity surface uniform across the
    // sibling <circle>/<text> pair. Both should carry the same
    // four data-* attributes for a given node.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);

    // Locate the <text> opening tag, then check each attribute
    // appears inside it.
    const auto text_open = svg_text.find("<text ");
    const auto text_close_of_open = svg_text.find(">", text_open);
    REQUIRE(text_open           != std::string::npos);
    REQUIRE(text_close_of_open  != std::string::npos);
    const std::string text_tag =
        svg_text.substr(text_open, text_close_of_open - text_open + 1);

    CHECK(text_tag.find("data-id=\"berlin\"")        != std::string::npos);
    CHECK(text_tag.find("data-owner=\"0\"")          != std::string::npos);
    CHECK(text_tag.find("data-owner-code=\"GER\"")   != std::string::npos);
    CHECK(text_tag.find("data-name=\"Berlin\"")      != std::string::npos);
}

TEST_CASE("render_provinces: data-* attributes appear twice per node (circle + text)") {
    // Each canonical attribute value should show up at least
    // twice for a single node (once on the <circle>, once on
    // the <text>).
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);

    auto count_occurrences = [&](std::string_view needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = svg_text.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };

    CHECK(count_occurrences("data-id=\"berlin\"")        == 2u);
    CHECK(count_occurrences("data-owner=\"0\"")          == 2u);
    CHECK(count_occurrences("data-owner-code=\"GER\"")   == 2u);
    CHECK(count_occurrences("data-owner-name=\"Germany\"") == 2u);
    CHECK(count_occurrences("data-name=\"Berlin\"")      == 2u);
}

TEST_CASE("render_provinces: data-name + data-owner-code are XML-attribute-escaped") {
    // Same escape contract as data-id (M4.2 review fix). Use
    // a country id_code and a province name with the five
    // XML metacharacters; assert both attributes carry the
    // escaped form, never the raw form.
    GameState state;
    state.countries.push_back(country(0, "X&Y\"Z<W>", "Country"));
    state.provinces.push_back(node("p", 0, 0.5, 0.5, "A&B<C>\"D'E"));
    const std::string svg_text = svg::render_provinces(state);

    CHECK(svg_text.find("data-name=\"A&amp;B&lt;C&gt;&quot;D&apos;E\"")
          != std::string::npos);
    CHECK(svg_text.find("data-owner-code=\"X&amp;Y&quot;Z&lt;W&gt;\"")
          != std::string::npos);

    // The raw form must NOT appear as a data-* attribute body.
    CHECK(svg_text.find("data-name=\"A&B<")        == std::string::npos);
    CHECK(svg_text.find("data-owner-code=\"X&Y")   == std::string::npos);
}

TEST_CASE("render_provinces: invalid owner → empty data-owner-code (defensive)") {
    // Hand-built state with invalid CountryId (default -1).
    // The save / scenario layers reject this in production but
    // the renderer stays total.
    GameState state;
    ProvinceNode p;
    p.id_code = "ghost";
    p.name    = "Ghost";
    p.owner   = CountryId::invalid();  // -1
    p.x       = 0.5;
    p.y       = 0.5;
    state.provinces.push_back(p);
    const std::string svg_text = svg::render_provinces(state);

    // The attribute is still emitted (uniform identity surface)
    // but with an empty value.
    CHECK(svg_text.find("data-owner-code=\"\"") != std::string::npos);
    // data-owner still carries the raw integer ("-1").
    CHECK(svg_text.find("data-owner=\"-1\"")    != std::string::npos);
    // data-name still carries the name.
    CHECK(svg_text.find("data-name=\"Ghost\"")  != std::string::npos);
}

TEST_CASE("render_provinces: out-of-range owner → empty data-owner-code (defensive)") {
    // Owner index 5 with only 1 country loaded — out of range.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("orphan", 5, 0.5, 0.5, "Orphan"));
    const std::string svg_text = svg::render_provinces(state);

    CHECK(svg_text.find("data-owner=\"5\"")     != std::string::npos);
    CHECK(svg_text.find("data-owner-code=\"\"") != std::string::npos);
}

TEST_CASE("render_provinces: data-owner-code matches state.countries[owner].id_code") {
    // Three nodes owned by three different countries; each
    // node's data-owner-code matches the corresponding
    // country's id_code, not the country's name.
    GameState state;
    state.countries.push_back(country(0, "AAA", "Alpha Land"));
    state.countries.push_back(country(1, "BBB", "Bravo Land"));
    state.countries.push_back(country(2, "CCC", "Charlie Land"));
    state.provinces.push_back(node("p0", 0, 0.1, 0.1, "Node0"));
    state.provinces.push_back(node("p1", 1, 0.2, 0.2, "Node1"));
    state.provinces.push_back(node("p2", 2, 0.3, 0.3, "Node2"));
    const std::string svg_text = svg::render_provinces(state);

    CHECK(svg_text.find("data-owner-code=\"AAA\"") != std::string::npos);
    CHECK(svg_text.find("data-owner-code=\"BBB\"") != std::string::npos);
    CHECK(svg_text.find("data-owner-code=\"CCC\"") != std::string::npos);
    // Country `name` should NOT appear inside data-owner-code.
    CHECK(svg_text.find("data-owner-code=\"Alpha Land\"") ==
          std::string::npos);
}

// ---------------------------------------------------------------------
// M4.10 - clickable UI skeleton (first JS in map.html)
// ---------------------------------------------------------------------

TEST_CASE("render_map_html: emits <div id=\"details\"> placeholder between SVG and legend") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    const auto svg_close = html.find("</svg>");
    const auto details_at = html.find("<div id=\"details\"");
    const auto ul_open    = html.find("<ul class=\"legend\">");
    REQUIRE(svg_close != std::string::npos);
    REQUIRE(details_at != std::string::npos);
    REQUIRE(ul_open    != std::string::npos);
    // Order: SVG → details panel → legend.
    CHECK(svg_close < details_at);
    CHECK(details_at < ul_open);
    // Empty-state placeholder text.
    CHECK(html.find("Click a province to see its details.")
          != std::string::npos);
    CHECK(html.find("class=\"details-empty\"")
          != std::string::npos);
}

TEST_CASE("render_map_html: emits exactly one inline <script> at end of body") {
    GameState state;
    const std::string html = svg::render_map_html(state);

    // Count <script> openings.
    auto count = [&](const std::string& needle) {
        std::size_t c = 0;
        std::size_t pos = 0;
        while ((pos = html.find(needle, pos)) != std::string::npos) {
            ++c;
            pos += needle.size();
        }
        return c;
    };
    CHECK(count("<script")  == 1u);
    CHECK(count("</script") == 1u);

    // Script must be inline (no src=, no type=) — keeps the
    // file self-contained, no CORS pitfalls.
    CHECK(html.find("<script src=")  == std::string::npos);
    CHECK(html.find("<script type=") == std::string::npos);

    // Script sits at end of body (after the legend's </ul>).
    const auto ul_close = html.rfind("</ul>");
    const auto script_at = html.find("<script");
    REQUIRE(ul_close  != std::string::npos);
    REQUIRE(script_at != std::string::npos);
    CHECK(ul_close < script_at);
}

TEST_CASE("render_map_html: click handler uses XSS-safe DOM API (no innerHTML / no eval / no document.write)") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    // The M4.10 handler uses textContent + createElement —
    // strings read from attributes can never be re-interpreted
    // as markup. None of the standard XSS escape hatches
    // should appear.
    CHECK(html.find("innerHTML")     == std::string::npos);
    CHECK(html.find("outerHTML")     == std::string::npos);
    CHECK(html.find("document.write") == std::string::npos);
    CHECK(html.find("eval(")         == std::string::npos);
    CHECK(html.find("Function(")     == std::string::npos);
    // The safe API surface IS present.
    CHECK(html.find("textContent")   != std::string::npos);
    CHECK(html.find("createElement") != std::string::npos);
    CHECK(html.find("addEventListener") != std::string::npos);
}

TEST_CASE("render_map_html: click handler does NOT mutate state or call out (no fetch / no XHR / no storage)") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    // Read-only viewer scope — the handler must not touch
    // any persistence / network API.
    CHECK(html.find("fetch(")                  == std::string::npos);
    CHECK(html.find("XMLHttpRequest")          == std::string::npos);
    CHECK(html.find("localStorage")            == std::string::npos);
    CHECK(html.find("sessionStorage")          == std::string::npos);
    CHECK(html.find("history.pushState")       == std::string::npos);
    CHECK(html.find("window.location")         == std::string::npos);
    CHECK(html.find("navigator.")              == std::string::npos);
}

TEST_CASE("render_map_html: click handler scopes to data-id-bearing circles / texts (legend swatches excluded)") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    // The handler queries `svg circle[data-id], svg text[data-id]`
    // so the legend's swatch <circle> elements (which lack
    // data-id) cannot fire the click handler. Pin the exact
    // selector string.
    CHECK(html.find("svg circle[data-id], svg text[data-id]")
          != std::string::npos);
}

TEST_CASE("render_map_html: <style> block carries the M4.10 details + cursor rules") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    CHECK(html.find(".details {")           != std::string::npos);
    CHECK(html.find(".details dl {")        != std::string::npos);
    CHECK(html.find(".details dt {")        != std::string::npos);
    CHECK(html.find(".details dd {")        != std::string::npos);
    CHECK(html.find(".details-empty {")     != std::string::npos);
    CHECK(html.find("svg circle[data-id], svg text[data-id] { cursor: pointer; }")
          != std::string::npos);
}

TEST_CASE("render_provinces (standalone SVG) does NOT include the M4.10 script or details panel") {
    // Standalone SVG path stays inert: no <script>, no
    // <div id="details">, no .details CSS.
    GameState state;
    state.provinces.push_back(node("a", 0, 0.5, 0.5, "Alpha"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("<script")              == std::string::npos);
    CHECK(svg_text.find("</script")             == std::string::npos);
    CHECK(svg_text.find("<div id=\"details\"")  == std::string::npos);
    CHECK(svg_text.find(".details")             == std::string::npos);
    CHECK(svg_text.find("addEventListener")     == std::string::npos);
}

// =====================================================================
// M4.11 — details labels polish
// =====================================================================
// The M4.10 click handler used the raw data-* attribute name
// as both the <dt> label and the getAttribute() lookup key.
// M4.11 decouples the two: getAttribute() still reads the M4.8
// DOM contract keys (`data-id`, `data-owner`,
// `data-owner-code`, `data-name` — those are NOT renamed; the
// <circle> / <text> surface is byte-identical with M4.10), but
// the rendered <dt> body shows a fixed human-readable label
// (`Province ID`, `Owner Index`, `Owner Code`, `Province
// Name`). Pure UX polish; no commands / state mutation /
// schema bump / artefact change.

TEST_CASE("render_map_html: M4.11 click handler emits the four human-readable <dt> labels") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    // All four labels appear as JavaScript string literals
    // inside the inline script. textContent guarantees they
    // render verbatim as the <dt> body at click time.
    CHECK(html.find("\"Province ID\"")   != std::string::npos);
    CHECK(html.find("\"Owner Index\"")   != std::string::npos);
    CHECK(html.find("\"Owner Code\"")    != std::string::npos);
    CHECK(html.find("\"Province Name\"") != std::string::npos);
}

TEST_CASE("render_map_html: M4.11 still uses raw data-* attr names for getAttribute (M4.8 DOM contract unchanged)") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    // The handler still has to look up the M4.8 attributes
    // — only the <dt> labels are remapped. The four data-*
    // keys must still appear as JS string literals (they go
    // into the `attr:` field of each `fields[]` entry, which
    // the loop passes to el.getAttribute(f.attr)).
    CHECK(html.find("\"data-id\"")         != std::string::npos);
    CHECK(html.find("\"data-owner\"")      != std::string::npos);
    CHECK(html.find("\"data-owner-code\"") != std::string::npos);
    CHECK(html.find("\"data-name\"")       != std::string::npos);
    // getAttribute is still the read path (textContent is
    // still the write path — XSS-safe API surface from
    // M4.10 preserved).
    CHECK(html.find("getAttribute(") != std::string::npos);
    CHECK(html.find("textContent")   != std::string::npos);
}

TEST_CASE("render_map_html: M4.11 changes the labels rendered into <dt>, NOT the <circle> / <text> attributes themselves") {
    // The M4.8 DOM identity surface lives on the SVG
    // elements: every <circle> and every <text> still
    // carries `data-id` / `data-owner` / `data-owner-code` /
    // `data-name` literally. A future clickable-UI consumer
    // that greps for these attribute names must still find
    // them on the elements — M4.11 only retouches the
    // panel's display labels.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string html = svg::render_map_html(state);
    // <circle> attribute surface (raw key, unchanged).
    CHECK(html.find(" data-id=\"berlin\"")        != std::string::npos);
    CHECK(html.find(" data-owner=\"0\"")          != std::string::npos);
    CHECK(html.find(" data-owner-code=\"GER\"")   != std::string::npos);
    CHECK(html.find(" data-name=\"Berlin\"")      != std::string::npos);
}

TEST_CASE("render_provinces (standalone SVG) does NOT carry the M4.11 details labels") {
    // The details panel + handler are HTML-wrapper-only.
    // The standalone SVG must NOT pick up the new labels
    // even though they are pure string literals.
    GameState state;
    state.provinces.push_back(node("a", 0, 0.5, 0.5, "Alpha"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("Province ID")   == std::string::npos);
    CHECK(svg_text.find("Owner Index")   == std::string::npos);
    CHECK(svg_text.find("Owner Code")    == std::string::npos);
    CHECK(svg_text.find("Province Name") == std::string::npos);
}

// =====================================================================
// M4.12 — clickable UI selected-state CSS skeleton
// =====================================================================
// M4.12 layers a transient selection highlight on top of the
// M4.10/M4.11 click handler:
//   * Two new CSS rules in the M4.6 <style> block:
//       svg circle.selected { stroke: #000000; stroke-width: 3; }
//       svg text.selected   { font-weight: bold; }
//   * The click handler now also calls a `selectProvince`
//     helper that clears all `.selected` classes and re-adds
//     `.selected` to every node (circle + text) sharing the
//     clicked element's data-id.
// Selection is **purely DOM-level**: never written into
// GameState; never persisted across reloads; no
// localStorage / sessionStorage / cookie / URL fragment.
// `class="selected"` does NOT appear in the initial render.

TEST_CASE("render_map_html: M4.12 <style> block carries circle.selected + text.selected rules") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("svg circle.selected { stroke: #000000; stroke-width: 3; }")
          != std::string::npos);
    CHECK(html.find("svg text.selected { font-weight: bold; }")
          != std::string::npos);
}

TEST_CASE("render_map_html: M4.12 click handler uses classList.add/remove on \"selected\"") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    // classList API is the safe class manipulation path —
    // it cannot inject markup the way assigning to .className
    // with concatenated strings could.
    CHECK(html.find("classList.add(\"selected\")")    != std::string::npos);
    CHECK(html.find("classList.remove(\"selected\")") != std::string::npos);
    // Helper function present.
    CHECK(html.find("selectProvince") != std::string::npos);
    // The handler still queries the previously-selected
    // nodes so the highlight clears between clicks.
    CHECK(html.find("\".selected\"") != std::string::npos);
}

TEST_CASE("render_map_html: M4.12 initial render has NO class=\"selected\" anywhere") {
    // At first paint, nothing is selected. The handler only
    // adds the class on click — there should be no element
    // with class="selected" in the static HTML, on either a
    // populated state or an empty one.
    {
        GameState state;
        state.countries.push_back(country(0, "GER", "Germany"));
        state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
        const std::string html = svg::render_map_html(state);
        CHECK(html.find("class=\"selected\"")  == std::string::npos);
        CHECK(html.find(" selected\"")          == std::string::npos);
        CHECK(html.find("=\"selected ")         == std::string::npos);
    }
    {
        GameState state;
        const std::string html = svg::render_map_html(state);
        CHECK(html.find("class=\"selected\"") == std::string::npos);
    }
}

TEST_CASE("render_map_html: M4.12 selection is DOM-only — no storage / persistence / state mutation surface") {
    // Reinforces the M4.10 no-persistence discipline against
    // the M4.12 new code path. Selection must not leak into
    // localStorage / sessionStorage / cookie / history /
    // location, and must not introduce any new XSS escape
    // hatches via `className` string concatenation or
    // setAttribute("class", ...).
    GameState state;
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("localStorage")          == std::string::npos);
    CHECK(html.find("sessionStorage")        == std::string::npos);
    CHECK(html.find("document.cookie")       == std::string::npos);
    CHECK(html.find("history.pushState")     == std::string::npos);
    CHECK(html.find("history.replaceState")  == std::string::npos);
    CHECK(html.find("window.location")       == std::string::npos);
    CHECK(html.find(".className")            == std::string::npos);
    CHECK(html.find("setAttribute(\"class\"") == std::string::npos);
}

TEST_CASE("render_provinces (standalone SVG) does NOT include the M4.12 selection CSS or .selected handling") {
    // M4.12, like M4.10/M4.11, lives entirely in the HTML
    // wrapper. The standalone SVG must stay free of the
    // selection CSS and any selection logic.
    GameState state;
    state.provinces.push_back(node("a", 0, 0.5, 0.5, "Alpha"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("circle.selected")    == std::string::npos);
    CHECK(svg_text.find("text.selected")      == std::string::npos);
    CHECK(svg_text.find(".selected")          == std::string::npos);
    CHECK(svg_text.find("selectProvince")     == std::string::npos);
    CHECK(svg_text.find("classList")          == std::string::npos);
    // Initial-paint invariant also holds.
    CHECK(svg_text.find("class=\"selected\"") == std::string::npos);
}

// =====================================================================
// M4.13 — details panel owner-name polish (widens the M4.8 surface)
// =====================================================================
// M4.13 adds a fifth `data-*` attribute to every <circle>
// and every <text> — `data-owner-name`, resolved from
// `state.countries[owner.value()].name` (or `""` when the
// owner index is invalid; same defensive fallback as the
// M4.8 `data-owner-code`). The M4.11 `fields` array in the
// click handler grows from four entries to five so the
// details panel renders a new `Owner Name` row.
//
// Save format stays v12: `data-owner-name` is **derived**
// from `state.countries` at render time, not a new field
// on `ProvinceNode`.

TEST_CASE("render_provinces: <circle> + <text> carry M4.13 data-owner-name with the owner's name") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);

    // Both <circle> and <text> carry the new attribute.
    auto count_occurrences = [&](std::string_view needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = svg_text.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    CHECK(count_occurrences("data-owner-name=\"Germany\"") == 2u);
}

TEST_CASE("render_provinces: M4.13 data-owner-name appears inside the <text> opening tag") {
    // Mirror the M4.8 "<text> carries the same four attrs"
    // check — now five attrs.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);

    const auto text_open = svg_text.find("<text ");
    const auto text_close_of_open = svg_text.find(">", text_open);
    REQUIRE(text_open           != std::string::npos);
    REQUIRE(text_close_of_open  != std::string::npos);
    const std::string text_tag =
        svg_text.substr(text_open, text_close_of_open - text_open + 1);

    CHECK(text_tag.find("data-owner-name=\"Germany\"") != std::string::npos);
}

TEST_CASE("render_provinces: M4.13 data-owner-name is XML-attribute-escaped") {
    // Country name with the five XML metacharacters. The
    // M4.2 xml_attr_escape helper covers all of them.
    GameState state;
    state.countries.push_back(country(0, "X", "A&B<C>\"D'E"));
    state.provinces.push_back(node("p", 0, 0.5, 0.5, "Node"));
    const std::string svg_text = svg::render_provinces(state);

    CHECK(svg_text.find("data-owner-name=\"A&amp;B&lt;C&gt;&quot;D&apos;E\"")
          != std::string::npos);
    // Raw form must NOT appear as a data-owner-name attribute body.
    CHECK(svg_text.find("data-owner-name=\"A&B<") == std::string::npos);
}

TEST_CASE("render_provinces: M4.13 invalid owner → empty data-owner-name (defensive)") {
    // Same hand-built state shape as the M4.8 invalid-owner
    // test. The attribute is still emitted (uniform identity
    // surface) but with an empty value.
    GameState state;
    ProvinceNode p;
    p.id_code = "ghost";
    p.name    = "Ghost";
    p.owner   = CountryId::invalid();  // -1
    p.x       = 0.5;
    p.y       = 0.5;
    state.provinces.push_back(p);
    const std::string svg_text = svg::render_provinces(state);

    CHECK(svg_text.find("data-owner-name=\"\"") != std::string::npos);
    // And data-owner-code stays empty for the same reason
    // (single bounds check covers both lookups).
    CHECK(svg_text.find("data-owner-code=\"\"") != std::string::npos);
}

TEST_CASE("render_provinces: M4.13 out-of-range owner → empty data-owner-name (defensive)") {
    // Owner index 5 with only 1 country loaded.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("orphan", 5, 0.5, 0.5, "Orphan"));
    const std::string svg_text = svg::render_provinces(state);

    CHECK(svg_text.find("data-owner=\"5\"")          != std::string::npos);
    CHECK(svg_text.find("data-owner-name=\"\"")      != std::string::npos);
    CHECK(svg_text.find("data-owner-name=\"Germany\"") == std::string::npos);
}

TEST_CASE("render_provinces: M4.13 data-owner-name matches state.countries[owner].name across multiple countries") {
    // Three nodes owned by three different countries. Each
    // data-owner-name resolves to the right country's name,
    // not its id_code.
    GameState state;
    state.countries.push_back(country(0, "AAA", "Alpha Land"));
    state.countries.push_back(country(1, "BBB", "Bravo Land"));
    state.countries.push_back(country(2, "CCC", "Charlie Land"));
    state.provinces.push_back(node("p0", 0, 0.1, 0.1, "Node0"));
    state.provinces.push_back(node("p1", 1, 0.2, 0.2, "Node1"));
    state.provinces.push_back(node("p2", 2, 0.3, 0.3, "Node2"));
    const std::string svg_text = svg::render_provinces(state);

    CHECK(svg_text.find("data-owner-name=\"Alpha Land\"")   != std::string::npos);
    CHECK(svg_text.find("data-owner-name=\"Bravo Land\"")   != std::string::npos);
    CHECK(svg_text.find("data-owner-name=\"Charlie Land\"") != std::string::npos);
    // Country id_code should NOT appear inside data-owner-name.
    CHECK(svg_text.find("data-owner-name=\"AAA\"") == std::string::npos);
}

TEST_CASE("render_map_html: M4.13 details panel fields array carries the fifth Owner Name entry") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    // The M4.11 fields array grows from 4 to 5 entries.
    CHECK(html.find("\"data-owner-name\"") != std::string::npos);
    CHECK(html.find("\"Owner Name\"")      != std::string::npos);
    // The other four M4.11 labels are still present.
    CHECK(html.find("\"Province ID\"")     != std::string::npos);
    CHECK(html.find("\"Owner Index\"")     != std::string::npos);
    CHECK(html.find("\"Owner Code\"")      != std::string::npos);
    CHECK(html.find("\"Province Name\"")   != std::string::npos);
}

TEST_CASE("render_map_html: M4.13 propagates data-owner-name through the inline SVG body") {
    // map.html shares the same SVG body as provinces.svg, so
    // the M4.13 attribute reaches the HTML wrapper for free.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("data-owner-name=\"Germany\"") != std::string::npos);
}

// =====================================================================
// M4.15 — keyboard focus accessibility skeleton
// =====================================================================
// M4.15 makes the province markers reachable from the
// keyboard. Two surfaces:
//   * SVG body: every <circle> and every <text> in
//     `render_svg_root` carries `tabindex="0"` so the markers
//     enter the tab order. Legend swatch <circle> elements
//     (emitted in `render_map_html` separately) lack
//     `tabindex` and stay out of the tab order.
//   * inline <script>: alongside the existing `click`
//     listener, every focusable node also gets a `keydown`
//     listener. Enter or Space fires the same
//     `selectProvince + showDetails` pair the click runs;
//     Space additionally `preventDefault()`s to suppress
//     page-scroll. The two listeners share a per-element
//     `activate()` closure so the effect cannot drift
//     between input modalities.
//
// Out of scope (deliberate non-goals):
//   * No ARIA polish (no role= / aria-label= / aria-selected=
//     / aria-current=). Save that for a dedicated A11Y PR.
//   * No persistent focus state across reload.
//   * No tooltip / hover / mouseover.

TEST_CASE("render_provinces: M4.15 every <circle> + <text> carries tabindex=\"0\"") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);

    // Both elements carry the same tabindex form.
    auto count_occurrences = [&](std::string_view needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = svg_text.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    CHECK(count_occurrences("tabindex=\"0\"") == 2u);
}

TEST_CASE("render_provinces: M4.15 tabindex appears inside the <circle> AND <text> opening tags") {
    // Locate each opening tag and assert tabindex appears
    // inside it (not, say, accidentally hoisted into the
    // <svg> root or appended outside the element).
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);

    const auto circle_open = svg_text.find("<circle ");
    const auto circle_close = svg_text.find("/>", circle_open);
    REQUIRE(circle_open  != std::string::npos);
    REQUIRE(circle_close != std::string::npos);
    const std::string circle_tag =
        svg_text.substr(circle_open, circle_close - circle_open + 2);
    CHECK(circle_tag.find("tabindex=\"0\"") != std::string::npos);

    const auto text_open = svg_text.find("<text ");
    const auto text_close = svg_text.find(">", text_open);
    REQUIRE(text_open  != std::string::npos);
    REQUIRE(text_close != std::string::npos);
    const std::string text_tag =
        svg_text.substr(text_open, text_close - text_open + 1);
    CHECK(text_tag.find("tabindex=\"0\"") != std::string::npos);
}

TEST_CASE("render_map_html: M4.15 legend swatch <circle> elements do NOT carry tabindex") {
    // Legend swatches are emitted in render_map_html (NOT
    // render_svg_root) and have no data-id; the M4.15 spec
    // explicitly keeps them out of the tab order. Build a
    // state with one province + one country so we have both
    // a province circle (tabindex="0", inside the inline SVG
    // body) and a legend swatch circle (no tabindex, inside
    // the <li>).
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string html = svg::render_map_html(state);

    // Locate the legend swatch's <svg class="swatch"> ...
    // </svg> block and assert tabindex is absent inside it.
    const auto swatch_open = html.find("<svg class=\"swatch\"");
    const auto swatch_close = html.find("</svg>", swatch_open);
    REQUIRE(swatch_open  != std::string::npos);
    REQUIRE(swatch_close != std::string::npos);
    const std::string swatch =
        html.substr(swatch_open, swatch_close - swatch_open + 6);
    CHECK(swatch.find("tabindex") == std::string::npos);
}

TEST_CASE("render_map_html: M4.15 inline <script> wires a keydown listener with Enter + Space + preventDefault") {
    GameState state;
    const std::string html = svg::render_map_html(state);

    // Both listener types are now present.
    CHECK(html.find("addEventListener(\"click\"")    != std::string::npos);
    CHECK(html.find("addEventListener(\"keydown\"")  != std::string::npos);
    // The key check covers Enter + Space (the literal " "
    // form, not the Space-key alias).
    CHECK(html.find("\"Enter\"") != std::string::npos);
    CHECK(html.find("\" \"")     != std::string::npos);
    // Space-scroll suppression.
    CHECK(html.find("preventDefault") != std::string::npos);
    // Shared activation helper between click + keydown so
    // the effect cannot drift between modalities.
    CHECK(html.find("function activate()") != std::string::npos);
}

TEST_CASE("render_map_html: M4.15 still-deferred ARIA non-goals (M4.17 added role=button + aria-label only)") {
    // M4.17 reversed two non-goals of this skeleton:
    // role=button and aria-label now legitimately appear on
    // every <circle> + <text>. The narrower ARIA surface
    // still NOT shipped:
    //   aria-selected / aria-current / aria-pressed /
    //   aria-live / aria-describedby / aria-labelledby
    // and tabindex stays at "0" only.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string html = svg::render_map_html(state);

    CHECK(html.find("aria-selected=")     == std::string::npos);
    CHECK(html.find("aria-current=")      == std::string::npos);
    CHECK(html.find("aria-pressed=")      == std::string::npos);
    CHECK(html.find("aria-live=")         == std::string::npos);
    CHECK(html.find("aria-describedby=")  == std::string::npos);
    CHECK(html.find("aria-labelledby=")   == std::string::npos);
    // No tabindex="-1" or other non-zero forms — only
    // tabindex="0" (the "include in default tab order" form).
    CHECK(html.find("tabindex=\"-1\"") == std::string::npos);
    CHECK(html.find("tabindex=\"1\"")  == std::string::npos);
}

TEST_CASE("render_provinces (standalone SVG) DOES carry tabindex (same SVG body as map.html)") {
    // The M4.15 tabindex lives in render_svg_root, so the
    // standalone SVG path picks it up automatically. This
    // is the intended scope: a standalone SVG opened by
    // itself can still be keyboard-navigated where the
    // viewer supports it (some browsers; vector tools may
    // ignore tabindex on SVG content — that's the consumer's
    // call, not the renderer's).
    GameState state;
    state.provinces.push_back(node("a", 0, 0.5, 0.5, "Alpha"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find("tabindex=\"0\"") != std::string::npos);
    // But the standalone SVG STILL has no script / no
    // keydown wiring — the listener lives in the HTML
    // wrapper only.
    CHECK(svg_text.find("<script")        == std::string::npos);
    CHECK(svg_text.find("addEventListener") == std::string::npos);
    CHECK(svg_text.find("keydown")        == std::string::npos);
}

// =====================================================================
// M4.16 — focus-visible styling skeleton
// =====================================================================
// M4.16 layers a focus-visible CSS ring on top of M4.15's
// keyboard-focus surface so users can SEE which province
// marker is currently focused. Two ring rules + two paired
// bare-:focus rules that neutralise the browser's default
// outline:
//
//   svg circle:focus { outline: none; }
//   svg circle:focus-visible {
//     outline: none; stroke: #1976d2; stroke-width: 4;
//   }
//   svg text:focus { outline: none; }
//   svg text:focus-visible {
//     outline: 2px solid #1976d2; outline-offset: 2px;
//   }
//
// :focus-visible (not bare :focus) means the ring shows for
// keyboard-triggered focus only, not for mouse clicks —
// that keeps the M4.12 .selected highlight (set by the
// click/activate handler) visually distinct from the M4.16
// keyboard-focus indicator.

TEST_CASE("render_map_html: M4.16 <style> block carries the four focus-related rules") {
    GameState state;
    const std::string html = svg::render_map_html(state);

    // The two bare-:focus suppressors (neutralise default ring).
    CHECK(html.find("svg circle:focus { outline: none; }") != std::string::npos);
    CHECK(html.find("svg text:focus { outline: none; }")   != std::string::npos);

    // The two :focus-visible rings.
    CHECK(html.find("svg circle:focus-visible") != std::string::npos);
    CHECK(html.find("stroke: #1976d2; stroke-width: 4;") != std::string::npos);
    CHECK(html.find("svg text:focus-visible") != std::string::npos);
    CHECK(html.find("outline: 2px solid #1976d2;") != std::string::npos);
    CHECK(html.find("outline-offset: 2px;") != std::string::npos);
}

TEST_CASE("render_map_html: M4.16 uses :focus-visible (NOT bare :focus) for the rings") {
    // The behavioural distinction matters: bare :focus would
    // make the ring appear after a mouse click too, which
    // would collide with the M4.12 .selected stroke. The
    // :focus-visible rules MUST be present; the bare :focus
    // rules may only appear as the no-op `outline: none;`
    // suppressors. Pin both directions.
    GameState state;
    const std::string html = svg::render_map_html(state);

    CHECK(html.find(":focus-visible") != std::string::npos);
    // The ring colour / weight literals must NOT also appear
    // attached to a bare `:focus {` rule (which would mean
    // the click-state-via-:focus regression).
    CHECK(html.find("svg circle:focus { stroke") == std::string::npos);
    CHECK(html.find("svg text:focus { outline: 2px") == std::string::npos);
}

TEST_CASE("render_map_html: M4.16 focus ring colour does NOT collide with M4.12 .selected stroke") {
    // M4.12 .selected uses #000000; M4.16 focus uses #1976d2.
    // The two states must remain visually separable so a
    // user can tell "I clicked this" from "I tabbed here".
    GameState state;
    const std::string html = svg::render_map_html(state);

    // .selected stroke is still black (M4.12 contract intact).
    CHECK(html.find("svg circle.selected { stroke: #000000;") != std::string::npos);
    // Focus stroke is the M4.16 blue.
    CHECK(html.find("stroke: #1976d2;") != std::string::npos);
    // The two literals are distinct.
    CHECK(std::string("#000000") != std::string("#1976d2"));
}

TEST_CASE("render_map_html: M4.16 still-deferred ARIA non-goals (after M4.17, role=button + aria-label legitimately appear)") {
    // M4.17 reversed two non-goals of this CSS skeleton:
    // role=button and aria-label now legitimately appear on
    // every <circle> + <text>. The still-deferred narrower
    // ARIA surface stays absent.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string html = svg::render_map_html(state);

    CHECK(html.find("aria-selected=")    == std::string::npos);
    CHECK(html.find("aria-current=")     == std::string::npos);
    CHECK(html.find("aria-pressed=")     == std::string::npos);
    CHECK(html.find("aria-live=")        == std::string::npos);
    CHECK(html.find("aria-describedby=") == std::string::npos);
    CHECK(html.find("aria-labelledby=")  == std::string::npos);
}

TEST_CASE("render_provinces (standalone SVG) does NOT include the M4.16 focus-visible rules") {
    // The focus CSS lives in render_map_html only. The
    // standalone SVG path carries none of it.
    GameState state;
    state.provinces.push_back(node("a", 0, 0.5, 0.5, "Alpha"));
    const std::string svg_text = svg::render_provinces(state);

    CHECK(svg_text.find(":focus-visible") == std::string::npos);
    CHECK(svg_text.find(":focus")         == std::string::npos);
    CHECK(svg_text.find("#1976d2")        == std::string::npos);
    CHECK(svg_text.find("outline-offset") == std::string::npos);
}

// =====================================================================
// M4.17 — ARIA labels skeleton (minimal screen-reader surface)
// =====================================================================
// M4.17 reverses the explicit M4.15/M4.16 "no ARIA" non-
// goal: every <circle> and every <text> now carries
// `role="button"` (matches the M4.10 click + M4.15
// keyboard activation) and `aria-label="..."` (the
// otherwise-nameless <circle> gets a readable name; <text>
// gets a consistent name so screen-reader announcements
// don't differ by which sibling has focus). Composed at
// render time: `<name>, <owner_name>` when the owner
// resolves; `<name>` alone when the owner is invalid.
// Both attributes go on both circle and text — same
// pattern as the M4.8 + M4.13 data-* widenings. The
// values are XML-attribute-escaped via the M4.2 helper.
//
// Still deferred (narrower ARIA surface): aria-selected,
// aria-current, aria-pressed, aria-live, aria-describedby,
// aria-labelledby. Those land in a future A11Y PR.

TEST_CASE("render_provinces: M4.17 <circle> + <text> carry role=\"button\"") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);

    // Both elements carry the same role.
    auto count_occurrences = [&](std::string_view needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = svg_text.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    CHECK(count_occurrences("role=\"button\"") == 2u);
}

TEST_CASE("render_provinces: M4.17 aria-label is composed as \"<name>, <owner_name>\" for valid owner") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);

    auto count_occurrences = [&](std::string_view needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = svg_text.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    // Both circle + text get the same label.
    CHECK(count_occurrences("aria-label=\"Berlin, Germany\"") == 2u);
}

TEST_CASE("render_provinces: M4.17 aria-label inside the <text> opening tag (uniform identity)") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string svg_text = svg::render_provinces(state);

    const auto text_open = svg_text.find("<text ");
    const auto text_close = svg_text.find(">", text_open);
    REQUIRE(text_open  != std::string::npos);
    REQUIRE(text_close != std::string::npos);
    const std::string text_tag =
        svg_text.substr(text_open, text_close - text_open + 1);
    CHECK(text_tag.find("role=\"button\"")               != std::string::npos);
    CHECK(text_tag.find("aria-label=\"Berlin, Germany\"") != std::string::npos);
}

TEST_CASE("render_provinces: M4.17 aria-label falls back to <name> alone when owner is invalid") {
    // Hand-built state with CountryId::invalid() (default -1).
    // Save/scenario layers reject this in production but the
    // renderer stays total.
    GameState state;
    ProvinceNode p;
    p.id_code = "ghost";
    p.name    = "Ghost";
    p.owner   = CountryId::invalid();  // -1
    p.x       = 0.5;
    p.y       = 0.5;
    state.provinces.push_back(p);
    const std::string svg_text = svg::render_provinces(state);

    // Label is the name alone (no comma + owner suffix).
    CHECK(svg_text.find("aria-label=\"Ghost\"") != std::string::npos);
    // The composed form must NOT appear.
    CHECK(svg_text.find("aria-label=\"Ghost, ") == std::string::npos);
}

TEST_CASE("render_provinces: M4.17 aria-label is XML-attribute-escaped") {
    // Province name + country name with the five XML
    // metacharacters. The composed label "<name>, <owner>"
    // is escaped as a single value.
    GameState state;
    state.countries.push_back(country(0, "X", "X&Y\"Z"));
    state.provinces.push_back(node("p", 0, 0.5, 0.5, "A&B<C>"));
    const std::string svg_text = svg::render_provinces(state);

    // Pre-escape form is `A&B<C>, X&Y"Z`; expected post-escape:
    // `A&amp;B&lt;C&gt;, X&amp;Y&quot;Z`.
    CHECK(svg_text.find(
        "aria-label=\"A&amp;B&lt;C&gt;, X&amp;Y&quot;Z\"")
        != std::string::npos);
    // The raw form must NOT appear as an aria-label body.
    CHECK(svg_text.find("aria-label=\"A&B<")  == std::string::npos);
}

TEST_CASE("render_map_html: M4.17 legend swatch <circle> elements do NOT carry role or aria-label") {
    // Legend swatches are decorative and emitted in
    // render_map_html separately. They don't take focus, they
    // don't activate anything, and they don't get an ARIA name.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string html = svg::render_map_html(state);

    // Locate the legend swatch block and assert role / aria-label absent inside it.
    const auto swatch_open = html.find("<svg class=\"swatch\"");
    const auto swatch_close = html.find("</svg>", swatch_open);
    REQUIRE(swatch_open  != std::string::npos);
    REQUIRE(swatch_close != std::string::npos);
    const std::string swatch =
        html.substr(swatch_open, swatch_close - swatch_open + 6);
    CHECK(swatch.find("role=")       == std::string::npos);
    CHECK(swatch.find("aria-label=") == std::string::npos);
}

TEST_CASE("render_map_html: M4.17 propagates role + aria-label through the inline SVG body") {
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string html = svg::render_map_html(state);
    // Same shared SVG body — both attrs reach map.html.
    CHECK(html.find("role=\"button\"")                   != std::string::npos);
    CHECK(html.find("aria-label=\"Berlin, Germany\"")    != std::string::npos);
}

// =====================================================================
// M4.19 — hover affordance skeleton
// =====================================================================
// M4.19 adds two new :hover CSS rules in the M4.6 <style>
// block:
//   svg circle:hover { stroke: #666666; stroke-width: 2; }
//   svg text:hover   { text-decoration: underline; }
// Pure CSS — no JS hover handler, no mouseover / mouseout
// listener, no tooltip, no SVG <title> child element.
// The rules sit BEFORE .selected and :focus-visible in
// CSS source order, so those (equal specificity) win on
// the same element when both apply: hover never collides
// with selection or focus state.

TEST_CASE("render_map_html: M4.19 <style> block carries the two hover rules") {
    GameState state;
    const std::string html = svg::render_map_html(state);
    CHECK(html.find("svg circle:hover { stroke: #666666; stroke-width: 2; }")
          != std::string::npos);
    CHECK(html.find("svg text:hover { text-decoration: underline; }")
          != std::string::npos);
}

TEST_CASE("render_map_html: M4.19 hover colour distinct from M4.12 .selected and M4.16 :focus-visible") {
    // Three states must stay visually separable so a user
    // can tell hover (grey) from selected (black) from
    // focused (blue).
    GameState state;
    const std::string html = svg::render_map_html(state);

    // Hover: grey, 2px.
    CHECK(html.find("svg circle:hover { stroke: #666666; stroke-width: 2; }")
          != std::string::npos);
    // Selected: black, 3px.
    CHECK(html.find("svg circle.selected { stroke: #000000; stroke-width: 3; }")
          != std::string::npos);
    // Focus-visible: blue, 4px.
    CHECK(html.find("stroke: #1976d2; stroke-width: 4;")
          != std::string::npos);
    // All three literals are distinct (sanity).
    CHECK(std::string("#666666") != std::string("#000000"));
    CHECK(std::string("#666666") != std::string("#1976d2"));
}

TEST_CASE("render_map_html: M4.19 :hover rules appear BEFORE .selected and :focus-visible in CSS source order") {
    // CSS specificity is equal for `circle:hover` /
    // `circle.selected` / `circle:focus-visible` (one
    // pseudo-class or class each). Last-rule-wins, so the
    // M4.19 :hover must come first if .selected and
    // :focus-visible are supposed to override it on the
    // same element. Pin the source ordering.
    GameState state;
    const std::string html = svg::render_map_html(state);

    const auto hover_at    = html.find("svg circle:hover {");
    const auto selected_at = html.find("svg circle.selected {");
    const auto focus_at    = html.find("svg circle:focus-visible");
    REQUIRE(hover_at    != std::string::npos);
    REQUIRE(selected_at != std::string::npos);
    REQUIRE(focus_at    != std::string::npos);
    CHECK(hover_at < selected_at);
    CHECK(hover_at < focus_at);
}

TEST_CASE("render_map_html: M4.19 hover is pure CSS — no JS hover handler, no SVG <title> tooltip child") {
    // Pin the explicit non-goals of this skeleton:
    //   - no addEventListener("mouseover" / "mouseout") in
    //     the inline script
    //   - no element.onmouseover / .onmouseout assignment
    //   - no SVG <title> child element on the markers (the
    //     SVG-native tooltip mechanism — would compete with
    //     the M4.17 aria-label as the accessible name).
    // The HTML <head> <title>Leviathan Map</title> is the
    // page title and is required by the M4.5 contract;
    // M4.19 must not regress that, and must not add any
    // additional <title> tag inside <body>.
    GameState state;
    state.countries.push_back(country(0, "GER", "Germany"));
    state.provinces.push_back(node("berlin", 0, 0.5, 0.5, "Berlin"));
    const std::string html = svg::render_map_html(state);

    // No JS hover listener of any flavour.
    CHECK(html.find("\"mouseover\"")  == std::string::npos);
    CHECK(html.find("\"mouseout\"")   == std::string::npos);
    CHECK(html.find("\"mouseenter\"") == std::string::npos);
    CHECK(html.find("\"mouseleave\"") == std::string::npos);
    CHECK(html.find(".onmouseover")   == std::string::npos);
    CHECK(html.find(".onmouseout")    == std::string::npos);

    // The HTML <head> <title> is preserved (M4.5 contract)
    // and is the ONLY <title> tag in the document — no SVG
    // <title> child anywhere.
    auto count_occurrences = [&](std::string_view needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = html.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    CHECK(html.find("<title>Leviathan Map</title>") != std::string::npos);
    CHECK(count_occurrences("<title>")  == 1u);
    CHECK(count_occurrences("</title>") == 1u);

    // <body>...</body> carries NO <title> tag — that would
    // be a child of an SVG element (the only legal location
    // for <title> outside <head>), and is the M4.19 "no SVG
    // tooltip" non-goal we explicitly enforce.
    const auto body_open  = html.find("<body>");
    const auto body_close = html.find("</body>");
    REQUIRE(body_open  != std::string::npos);
    REQUIRE(body_close != std::string::npos);
    const std::string body =
        html.substr(body_open, body_close - body_open);
    CHECK(body.find("<title>")  == std::string::npos);
    CHECK(body.find("</title>") == std::string::npos);
}

TEST_CASE("render_provinces (standalone SVG) does NOT include the M4.19 :hover rules") {
    // CSS lives in the HTML wrapper only — standalone SVG
    // path carries no hover styling.
    GameState state;
    state.provinces.push_back(node("a", 0, 0.5, 0.5, "Alpha"));
    const std::string svg_text = svg::render_provinces(state);
    CHECK(svg_text.find(":hover")            == std::string::npos);
    CHECK(svg_text.find("text-decoration")   == std::string::npos);
    CHECK(svg_text.find("#666666")           == std::string::npos);
}

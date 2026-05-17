// M4.9 SVG / HTML DOM contract checkpoint.
//
// M4 has shipped, in sequence:
//
//   M4.1 typed `ProvinceNode` data layer
//   M4.2 first SVG renderer (`provinces.svg`)
//   M4.3 per-owner palette
//   M4.4 per-node `<text>` label
//   M4.5 minimal HTML wrapper (`map.html`)
//   M4.6 minimal viewer CSS
//   M4.7 static legend
//   M4.8 widened `data-*` identity surface on <circle> and <text>
//
// M4.9 is a checkpoint, not a feature. It does NOT add a new
// system, formula, artefact, save schema bump, gameplay
// branch, runner CLI flag, or any interactivity; it only
// pins the M4.2–M4.8 SVG / HTML DOM contract at the seam
// between M4 and any future M4.x (clickable UI, hover,
// tooltips, adjacency, terrain). The contract spec lives in
// `docs/milestone-4-checkpoint.md`; this file pins three
// coarse end-to-end properties via the canonical scenario:
//
//   A. Uniform identity surface across both artefacts.
//      For every canonical province, both `<circle>` and
//      `<text>` carry the same four `data-*` attributes
//      (`data-id`, `data-owner`, `data-owner-code`,
//      `data-name`), and the same attribute values appear in
//      `provinces.svg` and in `map.html`.
//
//   B. Legend rows correspond 1:1 with `state.countries`.
//      `map.html` carries exactly one `<li data-owner="N">`
//      per canonical country, with each country's `id_code`
//      appearing inside its `<li>` body.
//
//   C. No-interactivity invariant.
//      Both `provinces.svg` and `map.html` are inert
//      documents — no `<script>`, no `<link>`, no inline
//      event attributes, no inline `style="..."` per
//      element. This is what makes M4.x usable today (the
//      file opens in a browser without any runtime) AND
//      what a future M4.x clickable-UI sub-milestone has
//      to consciously break.
//
// Unit-level pinning of each contract clause is already in
// `tests/systems/svg_export_test.cpp` and the M4.x sections
// of `tests/systems/runner_test.cpp`. This integration test
// adds the end-to-end check against the canonical fixture so
// a future renderer change that quietly breaks the surface
// trips this gate as well.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/runner.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::GameState;
namespace rn = leviathan::systems::runner;

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

#ifdef LEVIATHAN_TEST_DATA_DIR
const char* kCanonicalConfig =
    LEVIATHAN_TEST_DATA_DIR "/config/simulation.json";
const char* kCanonicalScenario =
    LEVIATHAN_TEST_DATA_DIR "/scenarios/1930_minimal.json";
#endif

// Canonical fixture facts (pinned by the M3.8 fixture +
// M4.1 fixture). Three nodes, three countries, owner index
// matches country vector order.
struct CanonicalProvince {
    const char* id;     // ProvinceNode::id_code
    const char* name;   // ProvinceNode::name
    int         owner;  // CountryId index
    const char* owner_code;  // owning country's id_code
};

constexpr CanonicalProvince kCanonicalProvinces[] = {
    {"berlin", "Berlin", 0, "GER"},
    {"paris",  "Paris",  1, "FRA"},
    {"tokyo",  "Tokyo",  2, "JPN"},
};

}  // namespace

#ifdef LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// A. Uniform identity surface across both artefacts
// =====================================================================
TEST_CASE("M4 DOM contract: every canonical province surfaces all four data-* attrs on <circle> AND <text> in both provinces.svg AND map.html") {
    TempDir td("leviathan_m4_dom_identity");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    REQUIRE(rn::run(opts).ok());

    const std::string svg  = read_file(td.path / "provinces.svg");
    const std::string html = read_file(td.path / "map.html");

    auto check_body = [&](const std::string& body,
                          const std::string& body_label) {
        for (const auto& p : kCanonicalProvinces) {
            INFO("artefact = " << body_label
                 << " ; province = " << p.id);
            const std::string id_attr =
                std::string("data-id=\"") + p.id + "\"";
            const std::string owner_attr =
                std::string("data-owner=\"") +
                std::to_string(p.owner) + "\"";
            const std::string owner_code_attr =
                std::string("data-owner-code=\"") + p.owner_code + "\"";
            const std::string name_attr =
                std::string("data-name=\"") + p.name + "\"";

            // M4.8: each of the four data-* attribute substrings
            // must appear at least twice — once on <circle>,
            // once on <text>.
            auto count = [&](const std::string& needle) {
                std::size_t c = 0;
                std::size_t pos = 0;
                while ((pos = body.find(needle, pos)) !=
                       std::string::npos) {
                    ++c;
                    pos += needle.size();
                }
                return c;
            };
            CHECK(count(id_attr)         >= 2u);
            CHECK(count(owner_attr)      >= 2u);
            CHECK(count(owner_code_attr) >= 2u);
            CHECK(count(name_attr)       >= 2u);
        }
    };
    check_body(svg,  "provinces.svg");
    check_body(html, "map.html");
}

// =====================================================================
// B. Legend rows correspond 1:1 with state.countries
// =====================================================================
TEST_CASE("M4 DOM contract: map.html legend has one <li data-owner=N> per canonical country") {
    TempDir td("leviathan_m4_dom_legend_per_country");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    REQUIRE(rn::run(opts).ok());

    const std::string html = read_file(td.path / "map.html");

    // Canonical scenario has three countries: GER (0), FRA (1),
    // JPN (2). One <li data-owner="N"> per country.
    struct CanonicalCountry { int idx; const char* id_code; };
    constexpr CanonicalCountry kCountries[] = {
        {0, "GER"}, {1, "FRA"}, {2, "JPN"},
    };

    for (const auto& c : kCountries) {
        INFO("country = " << c.id_code);
        const std::string li_attr =
            std::string("<li data-owner=\"") +
            std::to_string(c.idx) + "\"";
        const auto li_at = html.find(li_attr);
        REQUIRE(li_at != std::string::npos);
        // The country's id_code appears AFTER the <li> opener
        // and BEFORE the matching </li>.
        const auto li_close = html.find("</li>", li_at);
        REQUIRE(li_close != std::string::npos);
        const std::string row =
            html.substr(li_at, li_close - li_at);
        CHECK(row.find(c.id_code) != std::string::npos);
    }

    // provinces.svg has no legend.
    const std::string svg = read_file(td.path / "provinces.svg");
    CHECK(svg.find("<li ")            == std::string::npos);
    CHECK(svg.find("class=\"legend\"") == std::string::npos);
}

// =====================================================================
// C. No-interactivity invariant
// =====================================================================
TEST_CASE("M4 DOM contract: both artefacts are inert (no <script>, no <link>, no inline event attrs, no per-element inline style)") {
    TempDir td("leviathan_m4_dom_no_interactivity");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    REQUIRE(rn::run(opts).ok());

    const std::string svg  = read_file(td.path / "provinces.svg");
    const std::string html = read_file(td.path / "map.html");

    auto check_body = [&](const std::string& body,
                          const std::string& body_label) {
        INFO("artefact = " << body_label);
        // No JavaScript.
        CHECK(body.find("<script")  == std::string::npos);
        CHECK(body.find("</script") == std::string::npos);
        // No external resource links.
        CHECK(body.find("<link")    == std::string::npos);
        // No inline event attributes (the common ones; a future
        // clickable UI sub-milestone must consciously add one).
        CHECK(body.find("onclick")     == std::string::npos);
        CHECK(body.find("onmouseover") == std::string::npos);
        CHECK(body.find("onload")      == std::string::npos);
        CHECK(body.find("onmousedown") == std::string::npos);
        CHECK(body.find("onkeydown")   == std::string::npos);
        // No per-element inline `style="..."`. The M4.6 +
        // M4.7 CSS lives entirely in the <style> block;
        // individual <circle> / <text> / <li> / swatch SVG
        // elements never carry a style attribute.
        CHECK(body.find(" style=\"") == std::string::npos);
    };
    check_body(svg,  "provinces.svg");
    check_body(html, "map.html");

    // provinces.svg additionally has NO <style> block (CSS
    // is HTML-wrapper-only).
    CHECK(svg.find("<style")    == std::string::npos);
    CHECK(svg.find("font-family") == std::string::npos);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

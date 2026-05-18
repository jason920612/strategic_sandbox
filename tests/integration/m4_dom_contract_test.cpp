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
//      `<text>` carry the same `data-*` attributes
//      (`data-id`, `data-owner`, `data-owner-code`,
//      `data-owner-name` (M4.13), `data-name`), and the
//      same attribute values appear in `provinces.svg` and
//      in `map.html`. The set was four at M4.8 and grew to
//      five at M4.13 (`data-owner-name` derived from
//      `state.countries[owner].name`).
//
//   B. Legend rows correspond 1:1 with `state.countries`.
//      `map.html` carries exactly one `<li data-owner="N">`
//      per canonical country, with each country's `id_code`
//      appearing inside its `<li>` body.
//
//   C. No-stray-interactivity invariant (asymmetric since
//      M4.10). `provinces.svg` stays fully inert (no
//      `<script>`, no `<style>`, no `font-family`).
//      `map.html` carries EXACTLY ONE inline `<script>`
//      block — the M4.10 click handler — and it must be
//      inline (no `src=`, no `type=`). Both artefacts
//      still have NO `<link>`, NO inline event attributes
//      (`onclick=` / etc.), and NO per-element
//      `style="..."`. A future M4.x sub-milestone that
//      adds a second script, an external src, or any other
//      interactivity surface trips this gate.
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
// matches country vector order. owner_name added in M4.13.
struct CanonicalProvince {
    const char* id;          // ProvinceNode::id_code
    const char* name;        // ProvinceNode::name
    int         owner;       // CountryId index
    const char* owner_code;  // owning country's id_code
    const char* owner_name;  // owning country's name (M4.13)
};

constexpr CanonicalProvince kCanonicalProvinces[] = {
    {"berlin", "Berlin", 0, "GER", "Germany"},
    {"paris",  "Paris",  1, "FRA", "France"},
    {"tokyo",  "Tokyo",  2, "JPN", "Japan"},
};

}  // namespace

#ifdef LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// A. Uniform identity surface across both artefacts
// =====================================================================
TEST_CASE("M4 DOM contract: every canonical province surfaces all five data-* attrs on <circle> AND <text> in both provinces.svg AND map.html") {
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
            const std::string owner_name_attr =
                std::string("data-owner-name=\"") + p.owner_name + "\"";
            const std::string name_attr =
                std::string("data-name=\"") + p.name + "\"";

            // M4.8 widened the identity surface to four data-*
            // attributes; M4.13 widens it again to five. Each
            // attribute substring must appear at least twice —
            // once on <circle>, once on <text>.
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
            CHECK(count(owner_name_attr) >= 2u);
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
// C. No-stray-interactivity invariant
//    (asymmetric since M4.10: map.html now carries one inline
//    <script> for the click-handler; provinces.svg stays
//    fully inert.)
// =====================================================================
TEST_CASE("M4 DOM contract: provinces.svg stays fully inert; map.html carries only the M4.10 click-handler script") {
    TempDir td("leviathan_m4_dom_interactivity_boundary");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    REQUIRE(rn::run(opts).ok());

    const std::string svg  = read_file(td.path / "provinces.svg");
    const std::string html = read_file(td.path / "map.html");

    // Common invariants (still hold for BOTH artefacts).
    auto check_no_link_no_inline_event_no_style = [&](
            const std::string& body, const std::string& body_label) {
        INFO("artefact = " << body_label);
        // No external resource links.
        CHECK(body.find("<link")    == std::string::npos);
        // No inline event attributes (the common ones). M4.10's
        // click handler uses addEventListener, not onclick="..."
        // inline attributes.
        CHECK(body.find("onclick=")     == std::string::npos);
        CHECK(body.find("onmouseover=") == std::string::npos);
        CHECK(body.find("onload=")      == std::string::npos);
        CHECK(body.find("onmousedown=") == std::string::npos);
        CHECK(body.find("onkeydown=")   == std::string::npos);
        // No per-element inline `style="..."`. The M4.6 +
        // M4.7 CSS lives entirely in the <style> block;
        // individual <circle> / <text> / <li> / swatch SVG
        // elements never carry a style attribute.
        CHECK(body.find(" style=\"") == std::string::npos);
    };
    check_no_link_no_inline_event_no_style(svg,  "provinces.svg");
    check_no_link_no_inline_event_no_style(html, "map.html");

    // provinces.svg-specific invariants:
    //   * NO <script> at all (interactivity is HTML-wrapper-only).
    //   * NO <style> at all (CSS is HTML-wrapper-only).
    //   * NO font-family (typography deferred).
    CHECK(svg.find("<script")    == std::string::npos);
    CHECK(svg.find("</script")   == std::string::npos);
    CHECK(svg.find("<style")     == std::string::npos);
    CHECK(svg.find("font-family") == std::string::npos);

    // map.html-specific invariant (M4.10):
    //   * EXACTLY ONE <script> ... </script> pair (the
    //     click handler IIFE). Future M4.x sub-milestones
    //     that grow the handler must keep it a single
    //     inline block — no <script src="...">, no <script
    //     type="module">, no second <script>.
    auto count_occurrences = [&](const std::string& body,
                                 const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = body.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    CHECK(count_occurrences(html, "<script")  == 1u);
    CHECK(count_occurrences(html, "</script") == 1u);
    // The single script is INLINE (no `src=` attribute).
    CHECK(html.find("<script src=") == std::string::npos);
    CHECK(html.find("<script type=") == std::string::npos);
}

// =====================================================================
// D. (M4.14) Click-handler fields contract
//
//    Pins the M4.13-era five-entry fields list inside the
//    inline <script>. Each of the five data-* attribute
//    names AND each of the five human-readable labels must
//    appear inside the click-handler script in the
//    canonical map.html. Catches accidental shrinkage back
//    to the M4.10/M4.11 four-entry form, label drift (e.g.
//    "Owner Name" reverting to raw "data-owner-name"), or a
//    future refactor that quietly drops a row.
//
//    This integration check is the end-to-end mirror of the
//    M4.11/M4.13 svg_export_test unit cases — it confirms
//    the labels reach the actual canonical artefact via the
//    runner, not just via direct render_map_html invocation.
// =====================================================================
TEST_CASE("M4 DOM contract: map.html click-handler script carries the M4.13 five-entry fields list with the canonical labels") {
    TempDir td("leviathan_m4_dom_fields_contract");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    REQUIRE(rn::run(opts).ok());

    const std::string html = read_file(td.path / "map.html");

    // Five attribute names (raw data-* keys, NOT renamed
    // since M4.8 / M4.13).
    CHECK(html.find("\"data-id\"")         != std::string::npos);
    CHECK(html.find("\"data-owner\"")      != std::string::npos);
    CHECK(html.find("\"data-owner-code\"") != std::string::npos);
    CHECK(html.find("\"data-owner-name\"") != std::string::npos);
    CHECK(html.find("\"data-name\"")       != std::string::npos);

    // Five human-readable labels (M4.11 introduced four;
    // M4.13 added Owner Name).
    CHECK(html.find("\"Province ID\"")   != std::string::npos);
    CHECK(html.find("\"Owner Index\"")   != std::string::npos);
    CHECK(html.find("\"Owner Code\"")    != std::string::npos);
    CHECK(html.find("\"Owner Name\"")    != std::string::npos);
    CHECK(html.find("\"Province Name\"") != std::string::npos);

    // provinces.svg carries none of the click handler — the
    // fields list lives inside the inline <script>, which
    // is map.html-only. The JS-literal forms (the attribute
    // name and the label surrounded by matching quotes)
    // must NOT appear; the bare SVG attribute form
    // `data-owner-name="..."` still does, as M4.13 pinned.
    const std::string svg = read_file(td.path / "provinces.svg");
    CHECK(svg.find("\"Owner Name\"")        == std::string::npos);
    CHECK(svg.find("\"Province ID\"")       == std::string::npos);
    CHECK(svg.find("\"data-owner-name\"")   == std::string::npos);
    CHECK(svg.find("\"data-id\"")           == std::string::npos);
}

// =====================================================================
// E. (M4.15) Keyboard focus end-to-end
//
//    Every canonical province <circle> + <text> carries
//    tabindex="0" in BOTH provinces.svg and map.html (the
//    M4.15 widening lives in render_svg_root, which both
//    artefacts share). The legend swatch <circle> in
//    map.html does NOT carry tabindex (it's emitted
//    separately in render_map_html and lacks data-id).
//    map.html's inline <script> wires keydown alongside
//    click.
// =====================================================================
TEST_CASE("M4 DOM contract: M4.15 tabindex=\"0\" on every canonical province circle+text in both artefacts; legend swatch is NOT focusable; keydown listener wired in map.html") {
    TempDir td("leviathan_m4_dom_keyboard_focus");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    REQUIRE(rn::run(opts).ok());

    const std::string svg  = read_file(td.path / "provinces.svg");
    const std::string html = read_file(td.path / "map.html");

    auto count_occurrences = [&](const std::string& body,
                                 const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = body.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };

    // Canonical scenario has three provinces. Each renders
    // one <circle> + one <text>, each carrying tabindex="0",
    // so the SVG body contributes 6 occurrences. The legend
    // swatch <circle> elements (3 of them in map.html) do
    // NOT carry tabindex.
    CHECK(count_occurrences(svg,  "tabindex=\"0\"") == 6u);
    CHECK(count_occurrences(html, "tabindex=\"0\"") == 6u);

    // map.html's inline <script> wires keydown + the key
    // checks + preventDefault. The handler exists, but the
    // standalone provinces.svg carries none of it.
    CHECK(html.find("addEventListener(\"keydown\"") != std::string::npos);
    CHECK(html.find("\"Enter\"")        != std::string::npos);
    CHECK(html.find("preventDefault")   != std::string::npos);
    CHECK(svg.find("addEventListener")  == std::string::npos);
    CHECK(svg.find("keydown")           == std::string::npos);
}

// =====================================================================
// F. (M4.18) Accessibility surface end-to-end
//
//    Every canonical province <circle> + <text> carries
//    role="button" + aria-label with the canonical
//    composed value in BOTH provinces.svg and map.html
//    (M4.17 attrs live in render_svg_root). Legend swatch
//    <circle> carries neither (M4.17 intent — decorative).
//    map.html's <style> block carries the M4.16
//    :focus-visible CSS rules; provinces.svg has neither
//    the CSS nor the <style> block (asymmetric CSS surface
//    established at M4.10).
// =====================================================================
TEST_CASE("M4 DOM contract: M4.17 role=button + aria-label end-to-end; M4.16 focus-visible CSS asymmetric") {
    TempDir td("leviathan_m4_dom_a11y_surface");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    REQUIRE(rn::run(opts).ok());

    const std::string svg  = read_file(td.path / "provinces.svg");
    const std::string html = read_file(td.path / "map.html");

    auto count_occurrences = [&](const std::string& body,
                                 const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = body.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };

    // Canonical scenario has three provinces × (circle + text)
    // = 6 role="button" occurrences in each artefact.
    CHECK(count_occurrences(svg,  "role=\"button\"") == 6u);
    CHECK(count_occurrences(html, "role=\"button\"") == 6u);

    // Each canonical province produces one composed aria-label
    // shared by its <circle> and <text>: 2 occurrences per
    // province per artefact.
    for (const auto& p : kCanonicalProvinces) {
        INFO("province = " << p.id);
        const std::string aria_label_attr =
            std::string("aria-label=\"") + p.name + ", " + p.owner_name + "\"";
        CHECK(count_occurrences(svg,  aria_label_attr) == 2u);
        CHECK(count_occurrences(html, aria_label_attr) == 2u);
    }

    // Legend swatch <circle> elements stay decorative — they
    // are emitted in render_map_html (NOT in render_svg_root),
    // and live inside <svg class="swatch">. Locate each swatch
    // and assert role / aria-label absent inside its span.
    std::size_t scan_at = 0;
    std::size_t swatches_checked = 0;
    while (true) {
        const auto open = html.find("<svg class=\"swatch\"", scan_at);
        if (open == std::string::npos) { break; }
        const auto close = html.find("</svg>", open);
        REQUIRE(close != std::string::npos);
        const std::string swatch =
            html.substr(open, close - open + 6);
        CHECK(swatch.find("role=")       == std::string::npos);
        CHECK(swatch.find("aria-label=") == std::string::npos);
        CHECK(swatch.find("tabindex=")   == std::string::npos);
        scan_at = close + 6;
        ++swatches_checked;
    }
    // Three canonical countries → three legend swatches.
    CHECK(swatches_checked == 3u);

    // M4.16 :focus-visible CSS lives in map.html's <style>
    // block; provinces.svg carries none of it.
    CHECK(html.find(":focus-visible")              != std::string::npos);
    CHECK(html.find("svg circle:focus-visible")    != std::string::npos);
    CHECK(html.find("svg text:focus-visible")      != std::string::npos);
    CHECK(html.find("#1976d2")                     != std::string::npos);
    CHECK(svg.find(":focus-visible")               == std::string::npos);
    CHECK(svg.find(":focus")                       == std::string::npos);
    CHECK(svg.find("#1976d2")                      == std::string::npos);

    // Still-deferred ARIA surface (M4.17 explicit non-goals)
    // stays absent end-to-end in both artefacts.
    for (const std::string& body : {svg, html}) {
        CHECK(body.find("aria-selected=")    == std::string::npos);
        CHECK(body.find("aria-current=")     == std::string::npos);
        CHECK(body.find("aria-pressed=")     == std::string::npos);
        CHECK(body.find("aria-live=")        == std::string::npos);
        CHECK(body.find("aria-describedby=") == std::string::npos);
        CHECK(body.find("aria-labelledby=")  == std::string::npos);
    }
}

// =====================================================================
// G. (M4.22) "M4 viewer contract complete" — single end-to-end
//    catch-all that touches every M4.x surface marker on the
//    canonical scenario.
//
//    Tests A–F already pin individual surfaces; test G is the
//    "did anything regress holistically?" check. If a future PR
//    accidentally drops one of the M4.x surfaces from the
//    canonical artefact path (e.g. by short-circuiting
//    render_map_html under some condition), this catch-all
//    fires.
//
//    M4.22 is a close-out readiness checkpoint — this test
//    documents the M4 viewer contract end-to-end as of the
//    readiness moment, so future regressions show up
//    immediately rather than at M4 close-out time.
// =====================================================================
TEST_CASE("M4 DOM contract: canonical map.html carries every M4.x surface marker; provinces.svg carries the SVG-body subset") {
    TempDir td("leviathan_m4_dom_contract_complete");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.days          = 1;
    opts.output_dir    = td.path;
    opts.scenario_path = kCanonicalScenario;
    REQUIRE(rn::run(opts).ok());

    const std::string svg  = read_file(td.path / "provinces.svg");
    const std::string html = read_file(td.path / "map.html");

    // -----------------------------------------------------------------
    // SVG body surfaces (live in render_svg_root → BOTH artefacts).
    // -----------------------------------------------------------------
    for (const std::string& body : {svg, html}) {
        // M4.2 / M4.3: viewBox + circle / fill.
        CHECK(body.find("viewBox=\"0 0 1000 1000\"") != std::string::npos);
        CHECK(body.find("<circle ") != std::string::npos);
        // M4.4: <text> labels.
        CHECK(body.find("<text ") != std::string::npos);
        // M4.8 + M4.13: five-attr identity surface.
        CHECK(body.find("data-id=")         != std::string::npos);
        CHECK(body.find("data-owner=")      != std::string::npos);
        CHECK(body.find("data-owner-code=") != std::string::npos);
        CHECK(body.find("data-owner-name=") != std::string::npos);
        CHECK(body.find("data-name=")       != std::string::npos);
        // M4.15: tabindex on every marker.
        CHECK(body.find("tabindex=\"0\"") != std::string::npos);
        // M4.17: role + aria-label.
        CHECK(body.find("role=\"button\"") != std::string::npos);
        CHECK(body.find("aria-label=")     != std::string::npos);
    }

    // -----------------------------------------------------------------
    // HTML-wrapper-only surfaces (live in render_map_html).
    // -----------------------------------------------------------------
    // M4.5 / M4.6 / M4.10: <!DOCTYPE html> + <style> + details panel.
    CHECK(html.find("<!DOCTYPE html>")            != std::string::npos);
    CHECK(html.find("<style>")                    != std::string::npos);
    CHECK(html.find("<div id=\"details\"")        != std::string::npos);
    // M4.7: legend.
    CHECK(html.find("<ul class=\"legend\">")      != std::string::npos);
    // M4.10 click handler infrastructure.
    CHECK(html.find("addEventListener(\"click\"") != std::string::npos);
    // M4.11 + M4.13 five-entry fields array.
    CHECK(html.find("\"Province ID\"")    != std::string::npos);
    CHECK(html.find("\"Owner Name\"")     != std::string::npos);
    CHECK(html.find("\"Province Name\"")  != std::string::npos);
    // M4.12 .selected CSS.
    CHECK(html.find("svg circle.selected") != std::string::npos);
    CHECK(html.find("svg text.selected")   != std::string::npos);
    // M4.15 keydown listener.
    CHECK(html.find("addEventListener(\"keydown\"") != std::string::npos);
    // M4.16 :focus-visible CSS.
    CHECK(html.find(":focus-visible")              != std::string::npos);
    CHECK(html.find("#1976d2")                     != std::string::npos);
    // M4.19 :hover CSS.
    CHECK(html.find("svg circle:hover")  != std::string::npos);
    CHECK(html.find("svg text:hover")    != std::string::npos);
    // M4.20 hover-status surface.
    CHECK(html.find("<p id=\"hover-status\"")        != std::string::npos);
    CHECK(html.find(".hover-status")                 != std::string::npos);
    CHECK(html.find("addEventListener(\"mouseover\"") != std::string::npos);
    CHECK(html.find("addEventListener(\"mouseout\"")  != std::string::npos);
    // M4.21 responsive surface.
    CHECK(html.find("<meta name=\"viewport\"") != std::string::npos);
    CHECK(html.find("@media (max-width: 1040px)") != std::string::npos);

    // -----------------------------------------------------------------
    // Bytes provinces.svg deliberately does NOT carry (HTML-only
    // surfaces). One catch-all check that no HTML-wrapper surface
    // leaked into the standalone SVG.
    // -----------------------------------------------------------------
    CHECK(svg.find("<!DOCTYPE")        == std::string::npos);
    CHECK(svg.find("<style>")          == std::string::npos);
    CHECK(svg.find("<script")          == std::string::npos);
    CHECK(svg.find("<div id=\"details\"") == std::string::npos);
    CHECK(svg.find("<p id=\"hover-status\"") == std::string::npos);
    CHECK(svg.find("<ul class=\"legend\">") == std::string::npos);
    CHECK(svg.find("@media")           == std::string::npos);
    CHECK(svg.find("<meta")            == std::string::npos);
    CHECK(svg.find(":focus-visible")   == std::string::npos);
    CHECK(svg.find(":hover")           == std::string::npos);
    CHECK(svg.find(".selected")        == std::string::npos);
    CHECK(svg.find("addEventListener") == std::string::npos);

    // -----------------------------------------------------------------
    // Save format + artefact contract pinned at the readiness moment.
    // -----------------------------------------------------------------
    CHECK(fs::exists(td.path / "save.json"));
    CHECK(fs::exists(td.path / "events.jsonl"));
    CHECK(fs::exists(td.path / "interest_groups.csv"));
    CHECK(fs::exists(td.path / "interest_group_country_feedback.csv"));
    CHECK(fs::exists(td.path / "interest_group_authority_pressure.csv"));
    CHECK(fs::exists(td.path / "provinces.svg"));
    CHECK(fs::exists(td.path / "map.html"));
    // save.json carries `"save_version": 13` (M5.1 schema bump;
    // M4 closed at v12, M5.1 bumped v12 -> v13 for the typed
    // EventDefinition block).
    const std::string save_json = read_file(td.path / "save.json");
    CHECK(save_json.find("\"save_version\": 13") != std::string::npos);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

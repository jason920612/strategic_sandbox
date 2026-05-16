#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/data_loader.hpp"

using leviathan::core::CountryState;
using leviathan::core::GameDate;
using leviathan::core::SimulationConfig;
namespace dl = leviathan::systems::data_loader;

// ---------------------------------------------------------------------
// SimulationConfig - happy paths
// ---------------------------------------------------------------------

TEST_CASE("parse_simulation_config: minimal valid input") {
    const auto r = dl::parse_simulation_config(R"({
        "simulation": { "start_date": "1930-01-01" }
    })");
    REQUIRE(r.ok());
    CHECK(r.value().start_date == GameDate(1930, 1, 1));
    // Optional fields keep their struct defaults.
    CHECK(r.value().end_date   == GameDate(2000, 12, 31));
    CHECK(r.value().seed       == 0u);
    CHECK(r.value().daily_tick);
}

TEST_CASE("parse_simulation_config: every field set explicitly") {
    const auto r = dl::parse_simulation_config(R"({
        "simulation": {
            "start_date": "1936-07-17",
            "end_date":   "1945-09-02",
            "seed":       42,
            "daily_tick": false
        }
    })");
    REQUIRE(r.ok());
    CHECK(r.value().start_date == GameDate(1936, 7, 17));
    CHECK(r.value().end_date   == GameDate(1945, 9, 2));
    CHECK(r.value().seed       == 42u);
    CHECK_FALSE(r.value().daily_tick);
}

// ---------------------------------------------------------------------
// SimulationConfig - error paths
// ---------------------------------------------------------------------

TEST_CASE("parse_simulation_config: malformed JSON is rejected") {
    const auto r = dl::parse_simulation_config(R"({not really json)");
    REQUIRE(r.failed());
    CHECK(r.error().find("JSON parse error") != std::string::npos);
}

TEST_CASE("parse_simulation_config: empty input is rejected") {
    const auto r = dl::parse_simulation_config("");
    REQUIRE(r.failed());
}

TEST_CASE("parse_simulation_config: missing start_date names the field") {
    const auto r = dl::parse_simulation_config(R"({"simulation": {}})");
    REQUIRE(r.failed());
    CHECK(r.error().find("simulation.start_date") != std::string::npos);
    CHECK(r.error().find("missing") != std::string::npos);
}

TEST_CASE("parse_simulation_config: start_date wrong type names the field") {
    const auto r = dl::parse_simulation_config(R"({
        "simulation": { "start_date": 1930 }
    })");
    REQUIRE(r.failed());
    CHECK(r.error().find("simulation.start_date") != std::string::npos);
    CHECK(r.error().find("string") != std::string::npos);
}

TEST_CASE("parse_simulation_config: start_date invalid calendar date") {
    const auto r = dl::parse_simulation_config(R"({
        "simulation": { "start_date": "1930-02-30" }
    })");
    REQUIRE(r.failed());
    CHECK(r.error().find("simulation.start_date") != std::string::npos);
    CHECK(r.error().find("Gregorian") != std::string::npos);
    // The raw bad value must be present in the message so a human can
    // locate the typo in their JSON.
    CHECK(r.error().find("1930-02-30") != std::string::npos);
}

TEST_CASE("parse_simulation_config: seed wrong type") {
    const auto r = dl::parse_simulation_config(R"({
        "simulation": { "start_date": "1930-01-01", "seed": "abc" }
    })");
    REQUIRE(r.failed());
    CHECK(r.error().find("simulation.seed") != std::string::npos);
}

TEST_CASE("parse_simulation_config: accepts uint64 max seed") {
    // SimulationConfig::seed is uint64_t; the loader must not silently
    // truncate values in the upper half of the range. Regression for
    // PR #7 review feedback.
    const auto r = dl::parse_simulation_config(R"({
        "simulation": {
            "start_date": "1930-01-01",
            "seed": 18446744073709551615
        }
    })");
    REQUIRE(r.ok());
    CHECK(r.value().seed == std::numeric_limits<std::uint64_t>::max());
}

TEST_CASE("parse_simulation_config: accepts a seed above INT64_MAX") {
    // 2^63 sits just past the signed-int64 boundary. A loader that
    // routes everything through int64_t would either truncate or reject
    // this value; the correct behaviour is to accept it.
    const auto r = dl::parse_simulation_config(R"({
        "simulation": {
            "start_date": "1930-01-01",
            "seed": 9223372036854775808
        }
    })");
    REQUIRE(r.ok());
    CHECK(r.value().seed == (static_cast<std::uint64_t>(1) << 63));
}

TEST_CASE("parse_simulation_config: negative seed rejected") {
    const auto r = dl::parse_simulation_config(R"({
        "simulation": { "start_date": "1930-01-01", "seed": -1 }
    })");
    REQUIRE(r.failed());
    CHECK(r.error().find("seed") != std::string::npos);
    CHECK(r.error().find("negative") != std::string::npos);
}

TEST_CASE("parse_simulation_config: daily_tick wrong type") {
    const auto r = dl::parse_simulation_config(R"({
        "simulation": {
            "start_date": "1930-01-01",
            "daily_tick": "yes"
        }
    })");
    REQUIRE(r.failed());
    CHECK(r.error().find("daily_tick") != std::string::npos);
    CHECK(r.error().find("boolean") != std::string::npos);
}

TEST_CASE("parse_simulation_config: source_label appears in error messages") {
    const auto r = dl::parse_simulation_config(
        R"({"simulation": {}})", "my-config.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("my-config.json") != std::string::npos);
}

// ---------------------------------------------------------------------
// Country - happy paths
// ---------------------------------------------------------------------

namespace {
// Canonical full-shape country JSON used as the baseline for parse_country
// tests. Individual tests can edit one field via search-and-replace before
// passing it to parse_country.
const std::string kCanonicalCountryJson = R"({
    "id":                        "GER",
    "name":                      "Germany",
    "display_name":              "Federal Republic of Germany",
    "initial_gdp":               100.0,
    "initial_stability":         0.55,
    "legal_tax_burden":          0.20,
    "fiscal_capacity":           0.50,
    "administrative_efficiency": 0.55,
    "central_control":           0.60,
    "corruption":                0.25,
    "legitimacy":                0.55,
    "military_power":            0.50,
    "threat_perception":         0.30,
    "budget": {
        "administration":  0.25,
        "military":        0.35,
        "education":       0.10,
        "welfare":         0.10,
        "intelligence":    0.05,
        "infrastructure":  0.10,
        "industry":        0.05
    }
})";

std::string replace_first(std::string s, std::string_view needle,
                          std::string_view replacement) {
    const std::size_t pos = s.find(needle);
    if (pos == std::string::npos) return s;
    s.replace(pos, needle.size(), replacement);
    return s;
}
}  // namespace

TEST_CASE("parse_country: full RFC-070 + M1.1 shape") {
    const auto r = dl::parse_country(kCanonicalCountryJson);
    REQUIRE(r.ok());
    const auto& c = r.value();
    CHECK(c.id_code      == "GER");
    CHECK(c.name         == "Germany");
    CHECK(c.display_name == "Federal Republic of Germany");
    // Initial-* fields are loaded into runtime gdp / stability.
    CHECK(c.gdp                       == doctest::Approx(100.0));
    CHECK(c.stability                 == doctest::Approx(0.55));
    // Runtime-only fields start at 0; not read from the config JSON.
    CHECK(c.tax_revenue               == doctest::Approx(0.0));
    CHECK(c.budget_balance            == doctest::Approx(0.0));
    // All ratio fields land directly.
    CHECK(c.legal_tax_burden          == doctest::Approx(0.20));
    CHECK(c.fiscal_capacity           == doctest::Approx(0.50));
    CHECK(c.administrative_efficiency == doctest::Approx(0.55));
    CHECK(c.central_control           == doctest::Approx(0.60));
    CHECK(c.corruption                == doctest::Approx(0.25));
    CHECK(c.legitimacy                == doctest::Approx(0.55));
    CHECK(c.military_power            == doctest::Approx(0.50));
    CHECK(c.threat_perception         == doctest::Approx(0.30));
    // M1.3 budget block.
    CHECK(c.budget.administration  == doctest::Approx(0.25));
    CHECK(c.budget.military        == doctest::Approx(0.35));
    CHECK(c.budget.education       == doctest::Approx(0.10));
    CHECK(c.budget.welfare         == doctest::Approx(0.10));
    CHECK(c.budget.intelligence    == doctest::Approx(0.05));
    CHECK(c.budget.infrastructure  == doctest::Approx(0.10));
    CHECK(c.budget.industry        == doctest::Approx(0.05));
    // The numeric id stays at its invalid default - assignment is the
    // caller's responsibility.
    CHECK_FALSE(c.id.valid());
}

TEST_CASE("parse_country: display_name defaults to name when omitted") {
    // Drop the display_name line from the canonical fixture entirely.
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"display_name\":              \"Federal Republic of Germany\",\n",
        "");
    const auto r = dl::parse_country(text);
    REQUIRE(r.ok());
    CHECK(r.value().display_name == "Germany");
}

// ---------------------------------------------------------------------
// Country - error paths
// ---------------------------------------------------------------------

TEST_CASE("parse_country: missing id field") {
    const std::string text = replace_first(
        kCanonicalCountryJson, "\"id\":                        \"GER\",\n", "");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("missing") != std::string::npos);
    CHECK(r.error().find("'id'")    != std::string::npos);
}

TEST_CASE("parse_country: initial_gdp wrong type") {
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"initial_gdp\":               100.0,",
        "\"initial_gdp\":               \"lots\",");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("initial_gdp") != std::string::npos);
    CHECK(r.error().find("number")      != std::string::npos);
}

TEST_CASE("parse_country: NaN literal makes the document malformed") {
    // Strict JSON does not permit NaN / Infinity literals, so nlohmann
    // rejects the whole document at parse time - the loader sees a
    // "malformed JSON" error, not a "non-finite number" error.
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"initial_gdp\":               100.0,",
        "\"initial_gdp\":               NaN,");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("JSON parse error") != std::string::npos);
}

TEST_CASE("parse_country: source label appears in error message") {
    const auto r = dl::parse_country(R"({})", "data/countries/broken.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("data/countries/broken.json") != std::string::npos);
}

// ---------------------------------------------------------------------
// Country - M1.1 new field requirements
// ---------------------------------------------------------------------

TEST_CASE("parse_country: missing legal_tax_burden is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson, "\"legal_tax_burden\":          0.20,\n", "");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("missing")            != std::string::npos);
    CHECK(r.error().find("'legal_tax_burden'") != std::string::npos);
}

TEST_CASE("parse_country: missing administrative_efficiency is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson, "\"administrative_efficiency\": 0.55,\n", "");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'administrative_efficiency'") != std::string::npos);
}

TEST_CASE("parse_country: missing legitimacy is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson, "\"legitimacy\":                0.55,\n", "");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'legitimacy'") != std::string::npos);
}

TEST_CASE("parse_country: negative initial_gdp is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"initial_gdp\":               100.0,",
        "\"initial_gdp\":               -1.0,");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("initial_gdp") != std::string::npos);
    CHECK(r.error().find(">= 0")        != std::string::npos);
}

TEST_CASE("parse_country: ratio above 1.0 is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"corruption\":                0.25,",
        "\"corruption\":                1.5,");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("corruption")   != std::string::npos);
    CHECK(r.error().find("[0, 1]")       != std::string::npos);
}

TEST_CASE("parse_country: ratio below 0.0 is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"central_control\":           0.60,",
        "\"central_control\":           -0.1,");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("central_control") != std::string::npos);
    CHECK(r.error().find("[0, 1]")          != std::string::npos);
}

// ---------------------------------------------------------------------
// Country - M1.3 budget block
// ---------------------------------------------------------------------

TEST_CASE("parse_country: missing budget block is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson,
        ",\n    \"budget\": {\n"
        "        \"administration\":  0.25,\n"
        "        \"military\":        0.35,\n"
        "        \"education\":       0.10,\n"
        "        \"welfare\":         0.10,\n"
        "        \"intelligence\":    0.05,\n"
        "        \"infrastructure\":  0.10,\n"
        "        \"industry\":        0.05\n"
        "    }",
        "");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("missing")  != std::string::npos);
    CHECK(r.error().find("'budget'") != std::string::npos);
}

TEST_CASE("parse_country: budget with wrong type is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"budget\": {\n"
        "        \"administration\":  0.25,\n"
        "        \"military\":        0.35,\n"
        "        \"education\":       0.10,\n"
        "        \"welfare\":         0.10,\n"
        "        \"intelligence\":    0.05,\n"
        "        \"infrastructure\":  0.10,\n"
        "        \"industry\":        0.05\n"
        "    }",
        "\"budget\": 42");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("budget") != std::string::npos);
    CHECK(r.error().find("JSON object") != std::string::npos);
}

TEST_CASE("parse_country: missing budget.military is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"military\":        0.35,\n        ", "");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    // Error must point at the budget sub-context so the user knows
    // it's not the top-level "military_power" field.
    CHECK(r.error().find("budget")   != std::string::npos);
    CHECK(r.error().find("military") != std::string::npos);
}

TEST_CASE("parse_country: budget category above 1.0 is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"administration\":  0.25,",
        "\"administration\":  1.5,");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("budget")          != std::string::npos);
    CHECK(r.error().find("administration")  != std::string::npos);
    CHECK(r.error().find("[0, 1]")          != std::string::npos);
}

TEST_CASE("parse_country: budget category below 0.0 is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"welfare\":         0.10,",
        "\"welfare\":         -0.1,");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("welfare") != std::string::npos);
    CHECK(r.error().find("[0, 1]")  != std::string::npos);
}

TEST_CASE("parse_country: budget that sums to less than 1.0 is accepted") {
    // The M1.3 loader does NOT enforce sum = 1; that's an
    // economy-tick concern. Authors can under-allocate the budget.
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"administration\":  0.25,",
        "\"administration\":  0.00,");
    const auto r = dl::parse_country(text);
    REQUIRE(r.ok());
    CHECK(r.value().budget.administration == doctest::Approx(0.0));
}

TEST_CASE("parse_country: initial_stability above 1.0 is rejected") {
    const std::string text = replace_first(
        kCanonicalCountryJson,
        "\"initial_stability\":         0.55,",
        "\"initial_stability\":         1.5,");
    const auto r = dl::parse_country(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("initial_stability") != std::string::npos);
    CHECK(r.error().find("[0, 1]")            != std::string::npos);
}

// ---------------------------------------------------------------------
// File-based loading
// ---------------------------------------------------------------------

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("load_simulation_config: canonical data/config/simulation.json") {
    namespace fs = std::filesystem;
    const fs::path p = fs::path(LEVIATHAN_TEST_DATA_DIR) / "config" / "simulation.json";

    const auto r = dl::load_simulation_config(p);
    // If this fails, doctest's REQUIRE prints the file and line; the
    // contents of r.error() can be inspected by re-running just this
    // case under a debugger or with -s.
    REQUIRE(r.ok());
    CHECK(r.value().start_date == GameDate(1930, 1, 1));
    CHECK(r.value().end_date   == GameDate(2000, 12, 31));
    CHECK(r.value().seed       == 19300101u);
    CHECK(r.value().daily_tick);
}

TEST_CASE("load_country: canonical data/countries/germany.json") {
    namespace fs = std::filesystem;
    const fs::path p = fs::path(LEVIATHAN_TEST_DATA_DIR) / "countries" / "germany.json";

    const auto r = dl::load_country(p);
    REQUIRE(r.ok());
    const auto& c = r.value();
    CHECK(c.id_code      == "GER");
    CHECK(c.name         == "Germany");
    CHECK(c.display_name == "Germany");
    CHECK(c.gdp                       == doctest::Approx(100.0));
    CHECK(c.stability                 == doctest::Approx(0.55));
    CHECK(c.legal_tax_burden          == doctest::Approx(0.20));
    CHECK(c.fiscal_capacity           == doctest::Approx(0.50));
    CHECK(c.administrative_efficiency == doctest::Approx(0.55));
    CHECK(c.central_control           == doctest::Approx(0.60));
    CHECK(c.corruption                == doctest::Approx(0.25));
    CHECK(c.legitimacy                == doctest::Approx(0.55));
    CHECK(c.military_power            == doctest::Approx(0.50));
    CHECK(c.threat_perception         == doctest::Approx(0.30));
    // M1.3 budget block (sums to exactly 1.0 for Germany).
    CHECK(c.budget.administration  == doctest::Approx(0.25));
    CHECK(c.budget.military        == doctest::Approx(0.35));
    CHECK(c.budget.education       == doctest::Approx(0.10));
    CHECK(c.budget.welfare         == doctest::Approx(0.10));
    CHECK(c.budget.intelligence    == doctest::Approx(0.05));
    CHECK(c.budget.infrastructure  == doctest::Approx(0.10));
    CHECK(c.budget.industry        == doctest::Approx(0.05));
}

#endif  // LEVIATHAN_TEST_DATA_DIR

// ---------------------------------------------------------------------
// Faction (M1.2)
// ---------------------------------------------------------------------

namespace {
const std::string kCanonicalFactionJson = R"({
    "id":         "GER_military",
    "country":    "GER",
    "type":       "military",
    "name":       "Reichswehr",
    "support":    0.45,
    "influence":  0.70,
    "radicalism": 0.30,
    "loyalty":    0.55,
    "resources":  1.20,
    "preferred_policies": [
        "increase_military_budget",
        "press_censorship"
    ]
})";
}  // namespace

TEST_CASE("parse_faction: full M1.2 shape") {
    const auto r = dl::parse_faction(kCanonicalFactionJson);
    REQUIRE(r.ok());
    const auto& f = r.value();
    CHECK(f.id_code         == "GER_military");
    CHECK(f.country_id_code == "GER");
    CHECK(f.type            == "military");
    CHECK(f.name            == "Reichswehr");
    CHECK(f.support    == doctest::Approx(0.45));
    CHECK(f.influence  == doctest::Approx(0.70));
    CHECK(f.radicalism == doctest::Approx(0.30));
    CHECK(f.loyalty    == doctest::Approx(0.55));
    CHECK(f.resources  == doctest::Approx(1.20));
    REQUIRE(f.preferred_policies.size() == 2);
    CHECK(f.preferred_policies[0] == "increase_military_budget");
    CHECK(f.preferred_policies[1] == "press_censorship");
    // Numeric ids stay at invalid defaults - caller-assigned.
    CHECK_FALSE(f.id.valid());
    CHECK_FALSE(f.country.valid());
}

TEST_CASE("parse_faction: empty preferred_policies array is allowed") {
    const std::string text = replace_first(
        kCanonicalFactionJson,
        "\"preferred_policies\": [\n"
        "        \"increase_military_budget\",\n"
        "        \"press_censorship\"\n"
        "    ]",
        "\"preferred_policies\": []");
    const auto r = dl::parse_faction(text);
    REQUIRE(r.ok());
    CHECK(r.value().preferred_policies.empty());
}

TEST_CASE("parse_faction: missing country is rejected") {
    const std::string text = replace_first(
        kCanonicalFactionJson, "\"country\":    \"GER\",\n    ", "");
    const auto r = dl::parse_faction(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("missing")  != std::string::npos);
    CHECK(r.error().find("'country'") != std::string::npos);
}

TEST_CASE("parse_faction: missing type is rejected") {
    const std::string text = replace_first(
        kCanonicalFactionJson, "\"type\":       \"military\",\n    ", "");
    const auto r = dl::parse_faction(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'type'") != std::string::npos);
}

TEST_CASE("parse_faction: missing preferred_policies is rejected") {
    const std::string text = replace_first(
        kCanonicalFactionJson,
        ",\n    \"preferred_policies\": [\n"
        "        \"increase_military_budget\",\n"
        "        \"press_censorship\"\n"
        "    ]",
        "");
    const auto r = dl::parse_faction(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("preferred_policies") != std::string::npos);
}

TEST_CASE("parse_faction: preferred_policies wrong type is rejected") {
    const std::string text = replace_first(
        kCanonicalFactionJson,
        "\"preferred_policies\": [\n"
        "        \"increase_military_budget\",\n"
        "        \"press_censorship\"\n"
        "    ]",
        "\"preferred_policies\": \"none\"");
    const auto r = dl::parse_faction(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("preferred_policies") != std::string::npos);
    CHECK(r.error().find("array") != std::string::npos);
}

TEST_CASE("parse_faction: non-string entry in preferred_policies is rejected") {
    const std::string text = replace_first(
        kCanonicalFactionJson,
        "\"increase_military_budget\",\n"
        "        \"press_censorship\"",
        "\"increase_military_budget\",\n"
        "        42");
    const auto r = dl::parse_faction(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("preferred_policies[1]") != std::string::npos);
}

TEST_CASE("parse_faction: support above 1.0 is rejected") {
    const std::string text = replace_first(
        kCanonicalFactionJson, "\"support\":    0.45,", "\"support\":    1.5,");
    const auto r = dl::parse_faction(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("support") != std::string::npos);
    CHECK(r.error().find("[0, 1]")  != std::string::npos);
}

TEST_CASE("parse_faction: radicalism below 0.0 is rejected") {
    const std::string text = replace_first(
        kCanonicalFactionJson,
        "\"radicalism\": 0.30,",
        "\"radicalism\": -0.1,");
    const auto r = dl::parse_faction(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("radicalism") != std::string::npos);
    CHECK(r.error().find("[0, 1]")     != std::string::npos);
}

TEST_CASE("parse_faction: negative resources is rejected") {
    const std::string text = replace_first(
        kCanonicalFactionJson,
        "\"resources\":  1.20,",
        "\"resources\":  -0.1,");
    const auto r = dl::parse_faction(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("resources") != std::string::npos);
    CHECK(r.error().find(">= 0")      != std::string::npos);
}

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("load_faction: canonical data/factions/ger_military.json") {
    namespace fs = std::filesystem;
    const fs::path p = fs::path(LEVIATHAN_TEST_DATA_DIR) /
                       "factions" / "ger_military.json";

    const auto r = dl::load_faction(p);
    REQUIRE(r.ok());
    const auto& f = r.value();
    CHECK(f.id_code         == "GER_military");
    CHECK(f.country_id_code == "GER");
    CHECK(f.type            == "military");
    CHECK(f.name            == "Reichswehr");
    CHECK(f.support    == doctest::Approx(0.45));
    CHECK(f.influence  == doctest::Approx(0.70));
    CHECK(f.radicalism == doctest::Approx(0.30));
    CHECK(f.loyalty    == doctest::Approx(0.55));
    CHECK(f.resources  == doctest::Approx(1.20));
    CHECK(f.preferred_policies.size() == 2);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

TEST_CASE("load_faction: missing file path is named in the error") {
    const auto r = dl::load_faction("does-not-exist/faction.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("does-not-exist/faction.json") != std::string::npos);
}

// ---------------------------------------------------------------------
// Policy (M1.4)
// ---------------------------------------------------------------------

namespace {
const std::string kCanonicalPolicyJson = R"({
    "id":            "increase_military_budget",
    "name":          "Increase Military Budget",
    "category":      "budget",
    "duration_days": 30,
    "admin_cost":    0.10,
    "effects": [
        { "target": "country.military_power",   "op": "add", "value":  0.03 },
        { "target": "faction:military.support", "op": "add", "value":  0.08 },
        { "target": "faction:workers.support",  "op": "add", "value": -0.03 }
    ]
})";
}  // namespace

TEST_CASE("parse_policy: full M1.4 shape") {
    const auto r = dl::parse_policy(kCanonicalPolicyJson);
    REQUIRE(r.ok());
    const auto& p = r.value();
    CHECK(p.id_code       == "increase_military_budget");
    CHECK(p.name          == "Increase Military Budget");
    CHECK(p.category      == "budget");
    CHECK(p.duration_days == 30);
    CHECK(p.admin_cost    == doctest::Approx(0.10));
    REQUIRE(p.effects.size() == 3);
    CHECK(p.effects[0].target == "country.military_power");
    CHECK(p.effects[0].op     == "add");
    CHECK(p.effects[0].value  == doctest::Approx(0.03));
    CHECK(p.effects[2].target == "faction:workers.support");
    CHECK(p.effects[2].value  == doctest::Approx(-0.03));
    CHECK_FALSE(p.id.valid());
}

TEST_CASE("parse_policy: empty effects array is allowed") {
    const std::string text = replace_first(
        kCanonicalPolicyJson,
        "\"effects\": [\n"
        "        { \"target\": \"country.military_power\",   \"op\": \"add\", \"value\":  0.03 },\n"
        "        { \"target\": \"faction:military.support\", \"op\": \"add\", \"value\":  0.08 },\n"
        "        { \"target\": \"faction:workers.support\",  \"op\": \"add\", \"value\": -0.03 }\n"
        "    ]",
        "\"effects\": []");
    const auto r = dl::parse_policy(text);
    REQUIRE(r.ok());
    CHECK(r.value().effects.empty());
}

TEST_CASE("parse_policy: missing id is rejected") {
    const std::string text = replace_first(
        kCanonicalPolicyJson,
        "\"id\":            \"increase_military_budget\",\n    ", "");
    const auto r = dl::parse_policy(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'id'") != std::string::npos);
}

TEST_CASE("parse_policy: missing category is rejected") {
    const std::string text = replace_first(
        kCanonicalPolicyJson, "\"category\":      \"budget\",\n    ", "");
    const auto r = dl::parse_policy(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'category'") != std::string::npos);
}

TEST_CASE("parse_policy: missing effects is rejected") {
    const std::string text = replace_first(
        kCanonicalPolicyJson,
        ",\n    \"effects\": [\n"
        "        { \"target\": \"country.military_power\",   \"op\": \"add\", \"value\":  0.03 },\n"
        "        { \"target\": \"faction:military.support\", \"op\": \"add\", \"value\":  0.08 },\n"
        "        { \"target\": \"faction:workers.support\",  \"op\": \"add\", \"value\": -0.03 }\n"
        "    ]",
        "");
    const auto r = dl::parse_policy(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("effects") != std::string::npos);
}

TEST_CASE("parse_policy: effects wrong type rejected") {
    const std::string text = replace_first(
        kCanonicalPolicyJson,
        "\"effects\": [\n"
        "        { \"target\": \"country.military_power\",   \"op\": \"add\", \"value\":  0.03 },\n"
        "        { \"target\": \"faction:military.support\", \"op\": \"add\", \"value\":  0.08 },\n"
        "        { \"target\": \"faction:workers.support\",  \"op\": \"add\", \"value\": -0.03 }\n"
        "    ]",
        "\"effects\": \"none\"");
    const auto r = dl::parse_policy(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("effects") != std::string::npos);
    CHECK(r.error().find("array")   != std::string::npos);
}

TEST_CASE("parse_policy: effect with missing target is rejected") {
    const std::string text = replace_first(
        kCanonicalPolicyJson,
        "{ \"target\": \"country.military_power\",   \"op\": \"add\", \"value\":  0.03 }",
        "{ \"op\": \"add\", \"value\":  0.03 }");
    const auto r = dl::parse_policy(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("effects[0]") != std::string::npos);
    CHECK(r.error().find("'target'")   != std::string::npos);
}

TEST_CASE("parse_policy: effect with wrong-type value rejected") {
    const std::string text = replace_first(
        kCanonicalPolicyJson,
        "{ \"target\": \"country.military_power\",   \"op\": \"add\", \"value\":  0.03 }",
        "{ \"target\": \"country.military_power\", \"op\": \"add\", \"value\": \"big\" }");
    const auto r = dl::parse_policy(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("effects[0]") != std::string::npos);
    CHECK(r.error().find("value")      != std::string::npos);
}

TEST_CASE("parse_policy: negative duration_days rejected") {
    const std::string text = replace_first(
        kCanonicalPolicyJson,
        "\"duration_days\": 30,",
        "\"duration_days\": -5,");
    const auto r = dl::parse_policy(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("duration_days") != std::string::npos);
}

TEST_CASE("parse_policy: admin_cost out of range rejected") {
    const std::string text = replace_first(
        kCanonicalPolicyJson,
        "\"admin_cost\":    0.10,",
        "\"admin_cost\":    1.5,");
    const auto r = dl::parse_policy(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("admin_cost") != std::string::npos);
    CHECK(r.error().find("[0, 1]")     != std::string::npos);
}

#ifdef LEVIATHAN_TEST_DATA_DIR

TEST_CASE("load_policy: canonical data/policies/increase_military_budget.json") {
    namespace fs = std::filesystem;
    const fs::path p = fs::path(LEVIATHAN_TEST_DATA_DIR) /
                       "policies" / "increase_military_budget.json";
    const auto r = dl::load_policy(p);
    REQUIRE(r.ok());
    CHECK(r.value().id_code == "increase_military_budget");
    CHECK(r.value().category == "budget");
    CHECK(r.value().duration_days == 30);
    CHECK(r.value().effects.size() == 3);
}

TEST_CASE("load_policy: canonical data/policies/administrative_reform.json") {
    namespace fs = std::filesystem;
    const fs::path p = fs::path(LEVIATHAN_TEST_DATA_DIR) /
                       "policies" / "administrative_reform.json";
    const auto r = dl::load_policy(p);
    REQUIRE(r.ok());
    CHECK(r.value().duration_days == 180);
    CHECK(r.value().effects.size() == 4);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

TEST_CASE("load_policy: missing file path is named in the error") {
    const auto r = dl::load_policy("does-not-exist/policy.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("does-not-exist/policy.json") != std::string::npos);
}

TEST_CASE("load_simulation_config: missing file names the path") {
    const auto r = dl::load_simulation_config("does-not-exist/sim.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("does-not-exist/sim.json") != std::string::npos);
    CHECK(r.error().find("cannot open") != std::string::npos);
}

TEST_CASE("load_country: missing file names the path") {
    const auto r = dl::load_country("does-not-exist/ger.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("does-not-exist/ger.json") != std::string::npos);
}

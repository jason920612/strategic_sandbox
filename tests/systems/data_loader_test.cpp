#include <doctest/doctest.h>

#include <filesystem>
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

TEST_CASE("parse_country: full RFC-070 shape") {
    const auto r = dl::parse_country(R"({
        "id":                "GER",
        "name":              "Germany",
        "display_name":      "Federal Republic of Germany",
        "initial_gdp":       100.0,
        "initial_stability": 0.55
    })");
    REQUIRE(r.ok());
    const auto& c = r.value();
    CHECK(c.id_code           == "GER");
    CHECK(c.name              == "Germany");
    CHECK(c.display_name      == "Federal Republic of Germany");
    CHECK(c.initial_gdp       == doctest::Approx(100.0));
    CHECK(c.initial_stability == doctest::Approx(0.55));
    // The numeric id stays at its invalid default - assignment is the
    // caller's responsibility.
    CHECK_FALSE(c.id.valid());
}

TEST_CASE("parse_country: display_name defaults to name when omitted") {
    const auto r = dl::parse_country(R"({
        "id":   "FRA",
        "name": "France",
        "initial_gdp":       80.0,
        "initial_stability": 0.60
    })");
    REQUIRE(r.ok());
    CHECK(r.value().display_name == "France");
}

// ---------------------------------------------------------------------
// Country - error paths
// ---------------------------------------------------------------------

TEST_CASE("parse_country: missing id field") {
    const auto r = dl::parse_country(R"({
        "name": "Germany",
        "initial_gdp":       100.0,
        "initial_stability": 0.55
    })");
    REQUIRE(r.failed());
    CHECK(r.error().find("missing") != std::string::npos);
    // The path is just "id" for the top-level field.
    CHECK(r.error().find("'id'") != std::string::npos);
}

TEST_CASE("parse_country: initial_gdp wrong type") {
    const auto r = dl::parse_country(R"({
        "id":   "GER",
        "name": "Germany",
        "initial_gdp":       "lots",
        "initial_stability": 0.55
    })");
    REQUIRE(r.failed());
    CHECK(r.error().find("initial_gdp") != std::string::npos);
    CHECK(r.error().find("number") != std::string::npos);
}

TEST_CASE("parse_country: non-finite numeric rejected") {
    // JSON does not natively support NaN/Inf; nlohmann parses such
    // tokens as null, which fails the number type-check.
    const auto r = dl::parse_country(R"({
        "id":   "GER",
        "name": "Germany",
        "initial_gdp":       NaN,
        "initial_stability": 0.55
    })");
    REQUIRE(r.failed());
}

TEST_CASE("parse_country: source label appears in error message") {
    const auto r = dl::parse_country(R"({})", "data/countries/broken.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("data/countries/broken.json") != std::string::npos);
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
    CHECK(r.value().id_code      == "GER");
    CHECK(r.value().name         == "Germany");
    CHECK(r.value().display_name == "Germany");
    CHECK(r.value().initial_gdp       == doctest::Approx(100.0));
    CHECK(r.value().initial_stability == doctest::Approx(0.55));
}

#endif  // LEVIATHAN_TEST_DATA_DIR

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

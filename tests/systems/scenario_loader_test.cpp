#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/scenario_loader.hpp"

namespace fs  = std::filesystem;
namespace sl  = leviathan::systems::scenario_loader;
using leviathan::core::CountryId;
using leviathan::core::FactionId;
using leviathan::core::GameState;
using leviathan::core::PolicyId;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------

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

void write_file(const fs::path& p, const std::string& body) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(body.data(), static_cast<std::streamsize>(body.size()));
}

// Minimal valid country JSON with the supplied id_code.
std::string country_json(const std::string& id_code,
                         const std::string& name = "Country") {
    return R"({
  "id": ")" + id_code + R"(",
  "name": ")" + name + R"(",
  "initial_gdp": 100.0,
  "initial_stability": 0.5,
  "legal_tax_burden": 0.2,
  "fiscal_capacity": 0.5,
  "administrative_efficiency": 0.5,
  "central_control": 0.5,
  "corruption": 0.2,
  "legitimacy": 0.5,
  "military_power": 0.5,
  "threat_perception": 0.3,
  "budget": {
    "administration": 0.2, "military": 0.2, "education": 0.1,
    "welfare": 0.1, "intelligence": 0.1, "infrastructure": 0.2,
    "industry": 0.1
  }
})";
}

std::string faction_json(const std::string& id_code,
                         const std::string& country_id_code) {
    return R"({
  "id": ")" + id_code + R"(",
  "country": ")" + country_id_code + R"(",
  "type": "bureaucracy",
  "name": "Faction",
  "support":    0.4,
  "influence":  0.5,
  "radicalism": 0.3,
  "loyalty":    0.5,
  "resources":  0.0,
  "preferred_policies": []
})";
}

std::string policy_json(const std::string& id_code) {
    return R"({
  "id": ")" + id_code + R"(",
  "name": "Policy",
  "category": "budget",
  "duration_days": 0,
  "admin_cost": 0.0,
  "effects": []
})";
}

#ifdef LEVIATHAN_TEST_DATA_DIR
const fs::path kCanonicalScenario =
    fs::path(LEVIATHAN_TEST_DATA_DIR) / "scenarios" / "1930_minimal.json";
#endif

}  // namespace

// =====================================================================
// parse_manifest - happy path
// =====================================================================

TEST_CASE("parse_manifest: happy path returns all three arrays") {
    const std::string text = R"({
  "scenario": {
    "countries": ["countries/a.json"],
    "factions":  ["factions/b.json"],
    "policies":  ["policies/c.json", "policies/d.json"]
  }
})";
    const auto r = sl::parse_manifest(text);
    REQUIRE(r.ok());
    const auto& m = r.value();
    REQUIRE(m.countries.size() == 1u);
    CHECK(m.countries[0] == fs::path("countries/a.json"));
    REQUIRE(m.factions.size() == 1u);
    CHECK(m.factions[0] == fs::path("factions/b.json"));
    REQUIRE(m.policies.size() == 2u);
    CHECK(m.policies[1] == fs::path("policies/d.json"));
}

TEST_CASE("parse_manifest: empty arrays are allowed") {
    const auto r = sl::parse_manifest(R"({"scenario":{"countries":[],"factions":[],"policies":[]}})");
    REQUIRE(r.ok());
    CHECK(r.value().countries.empty());
    CHECK(r.value().factions.empty());
    CHECK(r.value().policies.empty());
}

// =====================================================================
// parse_manifest - error paths
// =====================================================================

TEST_CASE("parse_manifest: malformed JSON is rejected") {
    const auto r = sl::parse_manifest("{ this is not json", "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("JSON parse error") != std::string::npos);
}

TEST_CASE("parse_manifest: top-level non-object is rejected") {
    const auto r = sl::parse_manifest("[1,2,3]", "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("not an object") != std::string::npos);
}

TEST_CASE("parse_manifest: missing scenario.countries is rejected") {
    const auto r = sl::parse_manifest(
        R"({"scenario":{"factions":[],"policies":[]}})", "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("countries") != std::string::npos);
}

TEST_CASE("parse_manifest: countries with wrong type is rejected") {
    const auto r = sl::parse_manifest(
        R"({"scenario":{"countries":"not-an-array","factions":[],"policies":[]}})",
        "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("countries") != std::string::npos);
}

TEST_CASE("parse_manifest: non-string array element is rejected") {
    const auto r = sl::parse_manifest(
        R"({"scenario":{"countries":["a.json", 42],"factions":[],"policies":[]}})",
        "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("countries[1]") != std::string::npos);
}

TEST_CASE("parse_manifest: missing scenario wrapper is rejected") {
    const auto r = sl::parse_manifest(
        R"({"countries":[],"factions":[],"policies":[]})", "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("scenario") != std::string::npos);
}

// =====================================================================
// load_into_state - happy path with synthetic temp dir
// =====================================================================

TEST_CASE("load_into_state: end-to-end happy path with synthetic fixtures") {
    TempDir td("scen_loader_e2e");
    // Layout: <td>/data/{countries,factions,policies}/*.json
    //         <td>/data/scenarios/manifest.json
    // Paths in manifest are relative to <td>/data/.
    write_file(td.path / "data" / "countries" / "ger.json", country_json("GER", "Germany"));
    write_file(td.path / "data" / "countries" / "fra.json", country_json("FRA", "France"));
    write_file(td.path / "data" / "factions" / "ger_mil.json", faction_json("GER_mil", "GER"));
    write_file(td.path / "data" / "factions" / "fra_mil.json", faction_json("FRA_mil", "FRA"));
    write_file(td.path / "data" / "policies" / "raise_tax.json", policy_json("raise_tax"));

    const std::string manifest = R"({
  "scenario": {
    "countries": ["countries/ger.json", "countries/fra.json"],
    "factions":  ["factions/ger_mil.json", "factions/fra_mil.json"],
    "policies":  ["policies/raise_tax.json"]
  }
})";
    const auto manifest_path = td.path / "data" / "scenarios" / "test.json";
    write_file(manifest_path, manifest);

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.ok());
    CHECK(r.value().countries_loaded == 2);
    CHECK(r.value().factions_loaded  == 2);
    CHECK(r.value().policies_loaded  == 1);

    REQUIRE(state.countries.size() == 2u);
    CHECK(state.countries[0].id_code == "GER");
    CHECK(state.countries[0].id.value() == 0);
    CHECK(state.countries[1].id_code == "FRA");
    CHECK(state.countries[1].id.value() == 1);

    REQUIRE(state.factions.size() == 2u);
    CHECK(state.factions[0].id_code == "GER_mil");
    CHECK(state.factions[0].id.value() == 0);
    CHECK(state.factions[0].country.value() == 0);          // resolves to GER
    CHECK(state.factions[1].id_code == "FRA_mil");
    CHECK(state.factions[1].id.value() == 1);
    CHECK(state.factions[1].country.value() == 1);          // resolves to FRA

    REQUIRE(state.policies.size() == 1u);
    CHECK(state.policies[0].id_code == "raise_tax");
    CHECK(state.policies[0].id.value() == 0);
}

// =====================================================================
// load_into_state - validation
// =====================================================================

TEST_CASE("load_into_state: duplicate country id_code is rejected") {
    TempDir td("scen_loader_dup_country");
    // Both files declare id="GER" inside their JSON, despite different filenames.
    write_file(td.path / "data" / "countries" / "a.json", country_json("GER"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("GER"));
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path, R"({"scenario":{"countries":["countries/a.json","countries/b.json"],"factions":[],"policies":[]}})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("duplicate country") != std::string::npos);
    CHECK(r.error().find("GER") != std::string::npos);
}

TEST_CASE("load_into_state: faction referencing missing country is rejected") {
    TempDir td("scen_loader_missing_country");
    write_file(td.path / "data" / "countries" / "ger.json", country_json("GER"));
    write_file(td.path / "data" / "factions" / "x.json", faction_json("X", "USA"));  // USA not loaded
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               R"({"scenario":{"countries":["countries/ger.json"],"factions":["factions/x.json"],"policies":[]}})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("missing country") != std::string::npos);
    CHECK(r.error().find("USA") != std::string::npos);
}

TEST_CASE("load_into_state: duplicate faction id_code is rejected") {
    TempDir td("scen_loader_dup_faction");
    write_file(td.path / "data" / "countries" / "ger.json", country_json("GER"));
    write_file(td.path / "data" / "factions" / "a.json", faction_json("F1", "GER"));
    write_file(td.path / "data" / "factions" / "b.json", faction_json("F1", "GER"));
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               R"({"scenario":{"countries":["countries/ger.json"],"factions":["factions/a.json","factions/b.json"],"policies":[]}})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("duplicate faction") != std::string::npos);
}

TEST_CASE("load_into_state: duplicate policy id_code is rejected") {
    TempDir td("scen_loader_dup_policy");
    write_file(td.path / "data" / "policies" / "a.json", policy_json("P1"));
    write_file(td.path / "data" / "policies" / "b.json", policy_json("P1"));
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               R"({"scenario":{"countries":[],"factions":[],"policies":["policies/a.json","policies/b.json"]}})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("duplicate policy") != std::string::npos);
}

TEST_CASE("load_into_state: pre-populated state is rejected") {
    TempDir td("scen_loader_prepop");
    write_file(td.path / "data" / "countries" / "ger.json", country_json("GER"));
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               R"({"scenario":{"countries":["countries/ger.json"],"factions":[],"policies":[]}})");

    GameState state;
    // Seed an existing country - mimics a caller that already ran the
    // loader once or otherwise mutated state.
    leviathan::core::CountryState pre;
    pre.id_code = "PRE";
    state.countries.push_back(pre);

    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("requires an empty GameState") != std::string::npos);
}

TEST_CASE("load_into_state: missing manifest file is reported with path") {
    GameState state;
    const auto r = sl::load_into_state(state, "no/such/manifest.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("no/such/manifest.json") != std::string::npos);
}

TEST_CASE("load_into_state: missing country fixture is reported with its path") {
    TempDir td("scen_loader_missing_country_file");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               R"({"scenario":{"countries":["countries/does_not_exist.json"],"factions":[],"policies":[]}})");
    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("does_not_exist.json") != std::string::npos);
}

// =====================================================================
// load_into_state - canonical fixture
// =====================================================================

#ifdef LEVIATHAN_TEST_DATA_DIR
TEST_CASE("load_into_state: canonical data/scenarios/1930_minimal.json loads cleanly") {
    GameState state;
    const auto r = sl::load_into_state(state, kCanonicalScenario);
    REQUIRE(r.ok());
    CHECK(r.value().countries_loaded == 3);   // Germany, France, Japan
    CHECK(r.value().factions_loaded  == 3);   // GER_military, GER_workers, GER_bureaucracy
    CHECK(r.value().policies_loaded  == 10);  // all canonical fixtures

    // Verify ID assignment order:
    CHECK(state.countries[0].id_code == "GER");
    CHECK(state.countries[1].id_code == "FRA");
    CHECK(state.countries[2].id_code == "JPN");

    // Every GER faction should resolve to CountryId{0}.
    for (const auto& f : state.factions) {
        CHECK(f.country_id_code == "GER");
        CHECK(f.country.value() == 0);
    }
}
#endif

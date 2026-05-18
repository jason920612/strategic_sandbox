#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/scenario_loader.hpp"

namespace fs  = std::filesystem;
namespace sl  = leviathan::systems::scenario_loader;
using leviathan::core::CountryId;
using leviathan::core::FactionId;
using leviathan::core::GameDate;
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
    CHECK(r.error().find("countries") != std::string::npos);
}

// =====================================================================
// Issue #110 §4: empty-state preflight covers ALL 7 loader-populated
// containers (countries / provinces / factions / policies / events /
// interest_groups / relationships), not just the original three.
// =====================================================================

namespace {

// Build a minimal valid manifest that the loader can otherwise load
// cleanly — the only failure mode under test in this block is the
// caller pre-populating one of the GameState containers.
fs::path write_minimal_manifest(const TempDir& td) {
    write_file(td.path / "data" / "countries" / "ger.json", country_json("GER"));
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               R"({"scenario":{"countries":["countries/ger.json"],"factions":[],"policies":[]}})");
    return manifest_path;
}

}  // namespace

TEST_CASE("Issue #110: pre-populated state.provinces is rejected") {
    TempDir td("scen_loader_prepop_provinces");
    const auto manifest_path = write_minimal_manifest(td);

    GameState state;
    leviathan::core::ProvinceNode node;
    node.id_code = "STALE";
    state.provinces.push_back(node);

    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("requires an empty GameState") != std::string::npos);
    CHECK(r.error().find("provinces") != std::string::npos);
}

TEST_CASE("Issue #110: pre-populated state.events is rejected") {
    TempDir td("scen_loader_prepop_events");
    const auto manifest_path = write_minimal_manifest(td);

    GameState state;
    leviathan::core::EventDefinition def;
    def.id_code = "stale_event";
    def.name    = "Stale Event";
    def.visible_report = "x";
    def.true_cause     = "x";
    state.events.push_back(def);

    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("requires an empty GameState") != std::string::npos);
    CHECK(r.error().find("events") != std::string::npos);
}

TEST_CASE("Issue #110: pre-populated state.interest_groups is rejected") {
    TempDir td("scen_loader_prepop_igs");
    const auto manifest_path = write_minimal_manifest(td);

    GameState state;
    leviathan::core::InterestGroupState ig;
    ig.id_code = "stale_ig";
    ig.country = CountryId{0};
    state.interest_groups.push_back(ig);

    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("requires an empty GameState") != std::string::npos);
    CHECK(r.error().find("interest_groups") != std::string::npos);
}

TEST_CASE("Issue #110: pre-populated state.relationships is rejected") {
    TempDir td("scen_loader_prepop_relationships");
    const auto manifest_path = write_minimal_manifest(td);

    GameState state;
    leviathan::core::CountryRelation rel;
    rel.from         = CountryId{0};
    rel.to           = CountryId{1};
    rel.relationship = -0.5;
    rel.threat       = 0.5;
    state.relationships.push_back(rel);

    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("requires an empty GameState") != std::string::npos);
    CHECK(r.error().find("relationships") != std::string::npos);
}

TEST_CASE("Issue #110: error message names every non-empty container "
          "when multiple are pre-populated") {
    TempDir td("scen_loader_prepop_multi");
    const auto manifest_path = write_minimal_manifest(td);

    GameState state;
    leviathan::core::CountryRelation rel;
    rel.from = CountryId{0};
    rel.to   = CountryId{1};
    state.relationships.push_back(rel);
    leviathan::core::InterestGroupState ig;
    ig.id_code = "stale_ig";
    state.interest_groups.push_back(ig);

    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups") != std::string::npos);
    CHECK(r.error().find("relationships")   != std::string::npos);
}

TEST_CASE("Issue #110: calling load_into_state twice on the same state fails "
          "(regression for silent-append behaviour issue #110 §4 flagged)") {
    TempDir td("scen_loader_double_load");
    const auto manifest_path = write_minimal_manifest(td);

    GameState state;
    REQUIRE(sl::load_into_state(state, manifest_path).ok());
    // Second call must fail — preflight now catches all 7 containers,
    // including the one (countries) populated by the first call.
    const auto r2 = sl::load_into_state(state, manifest_path);
    REQUIRE(r2.failed());
    CHECK(r2.error().find("requires an empty GameState") != std::string::npos);
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
    // M1.13: canonical fixture has NO starting_policies, so the
    // counter stays at 0 and the existing manifest is still valid.
    CHECK(r.value().starting_policies_applied == 0);

    // Verify ID assignment order:
    CHECK(state.countries[0].id_code == "GER");
    CHECK(state.countries[1].id_code == "FRA");
    CHECK(state.countries[2].id_code == "JPN");

    // Every GER faction should resolve to CountryId{0}.
    for (const auto& f : state.factions) {
        CHECK(f.country_id_code == "GER");
        CHECK(f.country.value() == 0);
    }

    // M3.8: canonical scenario now authors one Bureaucracy interest
    // group per country (GER / FRA / JPN). Numeric values are pinned
    // here so the manifest can't silently drift; cross-country
    // ordering matches the country load order.
    REQUIRE(state.interest_groups.size() == 3u);
    CHECK(state.interest_groups[0].id_code        == "ger_bureaucracy");
    CHECK(state.interest_groups[0].kind           ==
          leviathan::core::InterestGroupKind::Bureaucracy);
    CHECK(state.interest_groups[0].country.value() == 0);   // GER
    CHECK(state.interest_groups[0].influence       == doctest::Approx(0.55));
    CHECK(state.interest_groups[0].loyalty         == doctest::Approx(0.50));
    CHECK(state.interest_groups[0].radicalism      == doctest::Approx(0.10));
    CHECK(state.interest_groups[1].id_code        == "fra_bureaucracy");
    CHECK(state.interest_groups[1].kind           ==
          leviathan::core::InterestGroupKind::Bureaucracy);
    CHECK(state.interest_groups[1].country.value() == 1);   // FRA
    CHECK(state.interest_groups[2].id_code        == "jpn_bureaucracy");
    CHECK(state.interest_groups[2].kind           ==
          leviathan::core::InterestGroupKind::Bureaucracy);
    CHECK(state.interest_groups[2].country.value() == 2);   // JPN

    // M4.1: canonical scenario now authors one ProvinceNode per
    // canonical country via the shared `provinces/1930_core_nodes.json`
    // file. Pin the 3-node shape so a future manifest edit can't
    // silently drift, and pin GER's coordinates so the on-disk
    // fixture is locked in by tests.
    CHECK(r.value().provinces_loaded == 3);
    REQUIRE(state.provinces.size()   == 3u);
    CHECK(state.provinces[0].id_code       == "berlin");
    CHECK(state.provinces[0].name          == "Berlin");
    CHECK(state.provinces[0].owner.value() == 0);   // GER
    CHECK(state.provinces[0].x             == doctest::Approx(0.52));
    CHECK(state.provinces[0].y             == doctest::Approx(0.44));
    CHECK(state.provinces[1].id_code       == "paris");
    CHECK(state.provinces[1].owner.value() == 1);   // FRA
    CHECK(state.provinces[2].id_code       == "tokyo");
    CHECK(state.provinces[2].owner.value() == 2);   // JPN
}
#endif

// =====================================================================
// M1.13: starting_policies parse
// =====================================================================

TEST_CASE("parse_manifest: starting_policies absent parses as empty (M1.11 back-compat)") {
    const auto r = sl::parse_manifest(
        R"({"scenario":{"countries":[],"factions":[],"policies":[]}})");
    REQUIRE(r.ok());
    CHECK(r.value().starting_policies.empty());
}

TEST_CASE("parse_manifest: starting_policies happy path") {
    const std::string text = R"({
        "scenario": {
            "countries": [], "factions": [], "policies": [],
            "starting_policies": [
                { "policy": "raise_taxes",    "actor": "GER" },
                { "policy": "expand_welfare", "actor": "FRA" }
            ]
        }
    })";
    const auto r = sl::parse_manifest(text);
    REQUIRE(r.ok());
    REQUIRE(r.value().starting_policies.size() == 2u);
    CHECK(r.value().starting_policies[0].policy_id_code == "raise_taxes");
    CHECK(r.value().starting_policies[0].actor_id_code  == "GER");
    CHECK(r.value().starting_policies[1].policy_id_code == "expand_welfare");
    CHECK(r.value().starting_policies[1].actor_id_code  == "FRA");
}

TEST_CASE("parse_manifest: starting_policies not an array rejected") {
    const auto r = sl::parse_manifest(
        R"({"scenario":{"countries":[],"factions":[],"policies":[],"starting_policies":"bad"}})",
        "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("starting_policies") != std::string::npos);
    CHECK(r.error().find("not an array")      != std::string::npos);
}

TEST_CASE("parse_manifest: starting_policies entry not an object rejected") {
    const auto r = sl::parse_manifest(
        R"({"scenario":{"countries":[],"factions":[],"policies":[],"starting_policies":["just a string"]}})",
        "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("starting_policies[0]") != std::string::npos);
    CHECK(r.error().find("not an object")        != std::string::npos);
}

TEST_CASE("parse_manifest: starting_policies entry missing 'policy' rejected") {
    const auto r = sl::parse_manifest(
        R"({"scenario":{"countries":[],"factions":[],"policies":[],"starting_policies":[{"actor":"GER"}]}})",
        "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("starting_policies[0].policy") != std::string::npos);
}

TEST_CASE("parse_manifest: starting_policies entry missing 'actor' rejected") {
    const auto r = sl::parse_manifest(
        R"({"scenario":{"countries":[],"factions":[],"policies":[],"starting_policies":[{"policy":"raise_taxes"}]}})",
        "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("starting_policies[0].actor") != std::string::npos);
}

TEST_CASE("parse_manifest: starting_policies entry 'policy' wrong type rejected") {
    const auto r = sl::parse_manifest(
        R"({"scenario":{"countries":[],"factions":[],"policies":[],"starting_policies":[{"policy":42,"actor":"GER"}]}})",
        "<test>");
    REQUIRE(r.failed());
    CHECK(r.error().find("starting_policies[0].policy") != std::string::npos);
}

// =====================================================================
// M1.13: starting_policies apply
// =====================================================================

TEST_CASE("load_into_state: starting_policies applies day-0 enactment") {
    // Write a synthetic scenario where raise_taxes is enacted on
    // GER at day 0. raise_taxes adds 0.05 to country.legal_tax_burden,
    // so GER's tax burden should change from its initial value.
    TempDir td("scen_loader_m113_day0");
    write_file(td.path / "data" / "countries" / "ger.json", country_json("GER", "Germany"));
    write_file(td.path / "data" / "policies" / "raise_taxes.json",
               R"({
  "id": "raise_taxes",
  "name": "Raise Taxes",
  "category": "tax",
  "duration_days": 60,
  "admin_cost": 0.12,
  "effects": [
    { "target": "country.legal_tax_burden", "op": "add", "value": 0.05 }
  ]
})");
    const std::string manifest = R"({
  "scenario": {
    "countries": ["countries/ger.json"],
    "factions":  [],
    "policies":  ["policies/raise_taxes.json"],
    "starting_policies": [
      { "policy": "raise_taxes", "actor": "GER" }
    ]
  }
})";
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path, manifest);

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.ok());
    CHECK(r.value().starting_policies_applied == 1);

    // country_json sets legal_tax_burden = 0.20. Day-0 raise_taxes
    // adds 0.05; final value should be 0.25.
    REQUIRE(state.countries.size() == 1u);
    CHECK(state.countries[0].legal_tax_burden == doctest::Approx(0.25));

    // M1.15: every successful apply_policy_effects (including the
    // day-0 path) appends to active_policies. GameState defaults to
    // 1930-01-01; raise_taxes has duration_days=60, so expires_on
    // must be 1930-03-02.
    REQUIRE(state.countries[0].active_policies.size() == 1u);
    CHECK(state.countries[0].active_policies[0].policy_id_code == "raise_taxes");
    CHECK(state.countries[0].active_policies[0].expires_on
          == GameDate(1930, 3, 2));
}

TEST_CASE("load_into_state: starting_policies unknown policy id_code rejected") {
    TempDir td("scen_loader_m113_unknown_policy");
    write_file(td.path / "data" / "countries" / "ger.json", country_json("GER", "Germany"));
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": ["countries/ger.json"],
    "factions":  [],
    "policies":  [],
    "starting_policies": [
      { "policy": "this_policy_was_not_loaded", "actor": "GER" }
    ]
  }
})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("unknown policy id_code")             != std::string::npos);
    CHECK(r.error().find("this_policy_was_not_loaded")         != std::string::npos);
}

TEST_CASE("load_into_state: starting_policies unknown actor id_code rejected") {
    TempDir td("scen_loader_m113_unknown_actor");
    write_file(td.path / "data" / "countries" / "ger.json", country_json("GER", "Germany"));
    write_file(td.path / "data" / "policies" / "raise_taxes.json",
               policy_json("raise_taxes"));
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": ["countries/ger.json"],
    "factions":  [],
    "policies":  ["policies/raise_taxes.json"],
    "starting_policies": [
      { "policy": "raise_taxes", "actor": "USA" }
    ]
  }
})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("unknown actor country id_code") != std::string::npos);
    CHECK(r.error().find("USA")                            != std::string::npos);
}

TEST_CASE("load_into_state: starting_policies with invalid target propagates apply error") {
    TempDir td("scen_loader_m113_bad_target");
    write_file(td.path / "data" / "countries" / "ger.json", country_json("GER", "Germany"));
    // Policy whose effect references a CountryState field that
    // doesn't exist. PolicySystem (M1.5) will reject this on apply.
    write_file(td.path / "data" / "policies" / "broken.json",
               R"({
  "id": "broken",
  "name": "Broken",
  "category": "test",
  "duration_days": 0,
  "admin_cost": 0.0,
  "effects": [
    { "target": "country.no_such_field", "op": "add", "value": 0.01 }
  ]
})");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": ["countries/ger.json"],
    "factions":  [],
    "policies":  ["policies/broken.json"],
    "starting_policies": [
      { "policy": "broken", "actor": "GER" }
    ]
  }
})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("apply_policy_effects") != std::string::npos);
}

TEST_CASE("load_into_state: starting_policies multiple entries apply in order") {
    // Both raise_taxes and a second policy apply, each contributing
    // to legal_tax_burden so we can see ordered accumulation.
    TempDir td("scen_loader_m113_multi");
    write_file(td.path / "data" / "countries" / "ger.json", country_json("GER", "Germany"));
    write_file(td.path / "data" / "policies" / "tax_up.json",
               R"({
  "id": "tax_up",
  "name": "Tax Up",
  "category": "tax",
  "duration_days": 0,
  "admin_cost": 0.0,
  "effects": [
    { "target": "country.legal_tax_burden", "op": "add", "value": 0.05 }
  ]
})");
    write_file(td.path / "data" / "policies" / "tax_up2.json",
               R"({
  "id": "tax_up2",
  "name": "Tax Up 2",
  "category": "tax",
  "duration_days": 0,
  "admin_cost": 0.0,
  "effects": [
    { "target": "country.legal_tax_burden", "op": "add", "value": 0.10 }
  ]
})");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": ["countries/ger.json"],
    "factions":  [],
    "policies":  ["policies/tax_up.json", "policies/tax_up2.json"],
    "starting_policies": [
      { "policy": "tax_up",  "actor": "GER" },
      { "policy": "tax_up2", "actor": "GER" }
    ]
  }
})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.ok());
    CHECK(r.value().starting_policies_applied == 2);
    // 0.20 (initial) + 0.05 (tax_up) + 0.10 (tax_up2) = 0.35.
    CHECK(state.countries[0].legal_tax_burden == doctest::Approx(0.35));
}

// ---------------------------------------------------------------------
// M3.1 - interest_groups in the scenario manifest
// ---------------------------------------------------------------------

TEST_CASE("parse_manifest: interest_groups absent parses as empty") {
    const auto r = sl::parse_manifest(
        R"({ "scenario": { "countries": [], "factions": [], "policies": [] } })");
    REQUIRE(r.ok());
    CHECK(r.value().interest_groups.empty());
}

TEST_CASE("parse_manifest: interest_groups not an array rejected") {
    const auto r = sl::parse_manifest(R"({
  "scenario": { "countries": [], "factions": [], "policies": [],
                 "interest_groups": "bogus" }
})");
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups") != std::string::npos);
}

TEST_CASE("parse_manifest: interest_groups entry missing field rejected") {
    const auto r = sl::parse_manifest(R"({
  "scenario": { "countries": [], "factions": [], "policies": [],
                 "interest_groups": [
                   { "id_code": "x", "kind": "Bureaucracy",
                     "country": "GER", "influence": 0.5,
                     "loyalty": 0.5, "radicalism": 0.0 }
                 ] }
})");
    REQUIRE(r.failed());
    CHECK(r.error().find("name") != std::string::npos);
}

TEST_CASE("parse_manifest: interest_groups ratio out of range rejected") {
    const auto r = sl::parse_manifest(R"({
  "scenario": { "countries": [], "factions": [], "policies": [],
                 "interest_groups": [
                   { "id_code": "x", "name": "X",
                     "kind": "Bureaucracy", "country": "GER",
                     "influence": 1.5, "loyalty": 0.5,
                     "radicalism": 0.0 }
                 ] }
})");
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups") != std::string::npos);
    CHECK(r.error().find("influence")       != std::string::npos);
}

TEST_CASE("parse_manifest: interest_groups duplicate id_code rejected") {
    const auto r = sl::parse_manifest(R"({
  "scenario": { "countries": [], "factions": [], "policies": [],
                 "interest_groups": [
                   { "id_code": "dup", "name": "A",
                     "kind": "Bureaucracy", "country": "GER",
                     "influence": 0.5, "loyalty": 0.5,
                     "radicalism": 0.0 },
                   { "id_code": "dup", "name": "B",
                     "kind": "Military", "country": "GER",
                     "influence": 0.5, "loyalty": 0.5,
                     "radicalism": 0.0 }
                 ] }
})");
    REQUIRE(r.failed());
    CHECK(r.error().find("dup")       != std::string::npos);
    CHECK(r.error().find("duplicate") != std::string::npos);
}

TEST_CASE("load_into_state: interest_groups happy path") {
    TempDir td("scen_loader_m31_igs_happy");
    write_file(td.path / "data" / "countries" / "ger.json",
               country_json("GER", "Germany"));
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": ["countries/ger.json"],
    "factions":  [],
    "policies":  [],
    "interest_groups": [
      { "id_code": "ger_bureaucracy", "name": "German Bureaucracy",
        "kind": "Bureaucracy", "country": "GER",
        "influence": 0.60, "loyalty": 0.55, "radicalism": 0.05 },
      { "id_code": "ger_military",    "name": "German Military",
        "kind": "Military",    "country": "GER",
        "influence": 0.40, "loyalty": 0.70, "radicalism": 0.15 }
    ]
  }
})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.ok());
    REQUIRE(state.interest_groups.size() == 2u);
    CHECK(state.interest_groups[0].id_code    == "ger_bureaucracy");
    CHECK(state.interest_groups[0].kind       ==
          leviathan::core::InterestGroupKind::Bureaucracy);
    CHECK(state.interest_groups[0].country.value() == 0);
    CHECK(state.interest_groups[0].influence  == doctest::Approx(0.60));
    CHECK(state.interest_groups[1].id_code    == "ger_military");
    CHECK(state.interest_groups[1].kind       ==
          leviathan::core::InterestGroupKind::Military);
    CHECK(state.interest_groups[1].loyalty    == doctest::Approx(0.70));
}

TEST_CASE("load_into_state: interest_groups unknown country rejected") {
    TempDir td("scen_loader_m31_igs_unknown_country");
    write_file(td.path / "data" / "countries" / "ger.json",
               country_json("GER", "Germany"));
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": ["countries/ger.json"],
    "factions":  [],
    "policies":  [],
    "interest_groups": [
      { "id_code": "x", "name": "X", "kind": "Bureaucracy",
        "country": "XYZ",
        "influence": 0.5, "loyalty": 0.5, "radicalism": 0.0 }
    ]
  }
})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups[0]") != std::string::npos);
    CHECK(r.error().find("XYZ")                != std::string::npos);
}

TEST_CASE("load_into_state: interest_groups unknown kind rejected") {
    TempDir td("scen_loader_m31_igs_unknown_kind");
    write_file(td.path / "data" / "countries" / "ger.json",
               country_json("GER", "Germany"));
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": ["countries/ger.json"],
    "factions":  [],
    "policies":  [],
    "interest_groups": [
      { "id_code": "x", "name": "X", "kind": "FloatingMasons",
        "country": "GER",
        "influence": 0.5, "loyalty": 0.5, "radicalism": 0.0 }
    ]
  }
})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups[0]") != std::string::npos);
    CHECK(r.error().find("FloatingMasons")     != std::string::npos);
}

// =====================================================================
// M4.1 - provinces in the scenario manifest
// =====================================================================

TEST_CASE("parse_manifest: provinces absent parses as empty") {
    const auto r = sl::parse_manifest(
        R"({ "scenario": { "countries": [], "factions": [], "policies": [] } })");
    REQUIRE(r.ok());
    CHECK(r.value().provinces.empty());
}

TEST_CASE("parse_manifest: provinces not an array rejected") {
    const auto r = sl::parse_manifest(R"({
  "scenario": { "countries": [], "factions": [], "policies": [],
                 "provinces": "bogus" }
})");
    REQUIRE(r.failed());
    CHECK(r.error().find("provinces") != std::string::npos);
}

TEST_CASE("parse_manifest: provinces non-string entry rejected") {
    const auto r = sl::parse_manifest(R"({
  "scenario": { "countries": [], "factions": [], "policies": [],
                 "provinces": [ 42 ] }
})");
    REQUIRE(r.failed());
    CHECK(r.error().find("provinces[0]") != std::string::npos);
}

TEST_CASE("load_into_state: provinces happy path") {
    TempDir td("scen_loader_m41_provinces_happy");
    write_file(td.path / "data" / "countries" / "ger.json",
               country_json("GER", "Germany"));
    write_file(td.path / "data" / "provinces" / "core.json", R"({
  "provinces": [
    { "id": "berlin", "name": "Berlin",
      "owner": "GER", "x": 0.5, "y": 0.4 }
  ]
})");
    const auto manifest_path = td.path / "data" / "scenarios" / "s.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": [ "countries/ger.json" ],
    "factions":  [],
    "policies":  [],
    "provinces": [ "provinces/core.json" ]
  }
})");
    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.ok());
    CHECK(r.value().provinces_loaded == 1);
    REQUIRE(state.provinces.size() == 1u);
    CHECK(state.provinces[0].id_code        == "berlin");
    CHECK(state.provinces[0].name           == "Berlin");
    CHECK(state.provinces[0].owner.value()  == 0);   // GER index
    CHECK(state.provinces[0].x              == doctest::Approx(0.5));
    CHECK(state.provinces[0].y              == doctest::Approx(0.4));
}

TEST_CASE("load_into_state: provinces unknown owner rejected") {
    TempDir td("scen_loader_m41_provinces_unknown_owner");
    write_file(td.path / "data" / "countries" / "ger.json",
               country_json("GER", "Germany"));
    write_file(td.path / "data" / "provinces" / "core.json", R"({
  "provinces": [
    { "id": "x", "name": "X", "owner": "XYZ",
      "x": 0.5, "y": 0.4 }
  ]
})");
    const auto manifest_path = td.path / "data" / "scenarios" / "s.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": [ "countries/ger.json" ],
    "factions":  [],
    "policies":  [],
    "provinces": [ "provinces/core.json" ]
  }
})");
    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("provinces[0]") != std::string::npos);
    CHECK(r.error().find("XYZ")          != std::string::npos);
}

TEST_CASE("load_into_state: provinces x/y out of range rejected") {
    TempDir td("scen_loader_m41_provinces_xy_oor");
    write_file(td.path / "data" / "countries" / "ger.json",
               country_json("GER", "Germany"));
    write_file(td.path / "data" / "provinces" / "core.json", R"({
  "provinces": [
    { "id": "x", "name": "X", "owner": "GER",
      "x": 1.5, "y": 0.4 }
  ]
})");
    const auto manifest_path = td.path / "data" / "scenarios" / "s.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": [ "countries/ger.json" ],
    "factions":  [],
    "policies":  [],
    "provinces": [ "provinces/core.json" ]
  }
})");
    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("provinces[0]") != std::string::npos);
    CHECK(r.error().find("x")            != std::string::npos);
}

TEST_CASE("load_into_state: duplicate province id across files rejected") {
    TempDir td("scen_loader_m41_provinces_dup_across");
    write_file(td.path / "data" / "countries" / "ger.json",
               country_json("GER", "Germany"));
    write_file(td.path / "data" / "provinces" / "a.json", R"({
  "provinces": [
    { "id": "dup", "name": "A", "owner": "GER",
      "x": 0.1, "y": 0.1 }
  ]
})");
    write_file(td.path / "data" / "provinces" / "b.json", R"({
  "provinces": [
    { "id": "dup", "name": "B", "owner": "GER",
      "x": 0.2, "y": 0.2 }
  ]
})");
    const auto manifest_path = td.path / "data" / "scenarios" / "s.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": [ "countries/ger.json" ],
    "factions":  [],
    "policies":  [],
    "provinces": [ "provinces/a.json", "provinces/b.json" ]
  }
})");
    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("duplicate") != std::string::npos);
    CHECK(r.error().find("dup")       != std::string::npos);
}

TEST_CASE("load_into_state: province file missing provinces array rejected") {
    TempDir td("scen_loader_m41_provinces_missing_array");
    write_file(td.path / "data" / "countries" / "ger.json",
               country_json("GER", "Germany"));
    write_file(td.path / "data" / "provinces" / "core.json",
               R"({ "not_provinces": [] })");
    const auto manifest_path = td.path / "data" / "scenarios" / "s.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": [ "countries/ger.json" ],
    "factions":  [],
    "policies":  [],
    "provinces": [ "provinces/core.json" ]
  }
})");
    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("provinces") != std::string::npos);
}

// =====================================================================
// M5.1 - EventDefinition trigger/effect schema foundation
// =====================================================================
// Schema-only milestone: events are loaded, validated, stored.
// No trigger evaluator, no firing, no effects application, no
// monthly integration, no events.jsonl changes. Tests pin the
// loader contract end-to-end via temp-dir scenarios.

namespace {

std::string make_two_country_manifest_with_events(
        const std::string& events_relpath) {
    return R"({
  "scenario": {
    "countries": [ "countries/a.json", "countries/b.json" ],
    "factions":  [],
    "policies":  [],
    "events":    [ ")" + events_relpath + R"(" ]
  }
})";
}

std::string canonical_two_event_file() {
    return R"({
  "events": [
    {
      "id": "low_stability_unrest",
      "name": "Low Stability Unrest",
      "description": "Low stability creates unrest pressure.",
      "visible_report": "Reports indicate growing unrest among the population; stability appears to be slipping.",
      "true_cause": "The country's actual stability has fallen below the unrest threshold.",
      "triggers": [
        { "target": "country.stability", "op": "lt", "value": 0.30 }
      ],
      "effects": [
        { "target": "country.stability", "op": "add", "value": -0.02 }
      ]
    },
    {
      "id": "radical_interest_group_warning",
      "name": "Radical Interest Group Warning",
      "description": "A highly radical interest group signals political risk.",
      "visible_report": "Intelligence reports a sharp uptick in radical rhetoric from one of the country's interest groups.",
      "true_cause": "An interest group's actual radicalism has crossed the warning threshold.",
      "triggers": [
        { "target": "interest_group.radicalism", "op": "gt", "value": 0.75 }
      ],
      "effects": [
        { "target": "country.legitimacy", "op": "add", "value": -0.01 }
      ]
    }
  ]
})";
}

}  // namespace

TEST_CASE("M5.1 parse_manifest: missing 'events' key is allowed (pre-M5 manifests stay valid)") {
    const std::string text = R"({
  "scenario": {
    "countries": ["countries/a.json"],
    "factions":  [],
    "policies":  []
  }
})";
    const auto r = sl::parse_manifest(text);
    REQUIRE(r);
    CHECK(r.value().events.empty());
}

TEST_CASE("M5.1 parse_manifest: 'events' wrong type is rejected") {
    const std::string text = R"({
  "scenario": {
    "countries": [], "factions": [], "policies": [],
    "events": "not an array"
  }
})";
    const auto r = sl::parse_manifest(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'scenario.events' is not an array")
          != std::string::npos);
}

TEST_CASE("M5.1 parse_manifest: 'events' non-string element rejected") {
    const std::string text = R"({
  "scenario": {
    "countries": [], "factions": [], "policies": [],
    "events": [ 42 ]
  }
})";
    const auto r = sl::parse_manifest(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'scenario.events[0]' is not a string")
          != std::string::npos);
}

TEST_CASE("M5.1 load_into_state: canonical-style scenario loads exactly 2 event definitions") {
    TempDir td("leviathan_m5_load_canonical_events");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "core.json", canonical_two_event_file());
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/core.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r);
    CHECK(r.value().events_loaded == 2);
    REQUIRE(state.events.size() == 2);

    {
        const auto& ev = state.events[0];
        CHECK(ev.id_code == "low_stability_unrest");
        CHECK(ev.name    == "Low Stability Unrest");
        CHECK(ev.description ==
              "Low stability creates unrest pressure.");
        REQUIRE(ev.triggers.size() == 1u);
        CHECK(ev.triggers[0].target == "country.stability");
        CHECK(ev.triggers[0].op     == "lt");
        CHECK(ev.triggers[0].value  == doctest::Approx(0.30));
        REQUIRE(ev.effects.size() == 1u);
        CHECK(ev.effects[0].target == "country.stability");
        CHECK(ev.effects[0].op     == "add");
        CHECK(ev.effects[0].value  == doctest::Approx(-0.02));
    }
    {
        const auto& ev = state.events[1];
        CHECK(ev.id_code == "radical_interest_group_warning");
        CHECK(ev.name    == "Radical Interest Group Warning");
        REQUIRE(ev.triggers.size() == 1u);
        CHECK(ev.triggers[0].target == "interest_group.radicalism");
        CHECK(ev.triggers[0].op     == "gt");
        CHECK(ev.triggers[0].value  == doctest::Approx(0.75));
        REQUIRE(ev.effects.size() == 1u);
        CHECK(ev.effects[0].target == "country.legitimacy");
        CHECK(ev.effects[0].op     == "add");
        CHECK(ev.effects[0].value  == doctest::Approx(-0.01));
    }
}

TEST_CASE("M5.1 load_into_state: event file missing 'events' array rejected") {
    TempDir td("leviathan_m5_event_file_no_array");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({"not_events": []})");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("'events' missing or not an array")
          != std::string::npos);
}

TEST_CASE("M5.1 load_into_state: event entry missing 'id' rejected") {
    TempDir td("leviathan_m5_event_missing_id");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "name": "X", "description": "",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("'events[0]'.id missing")
          != std::string::npos);
}

TEST_CASE("M5.1 load_into_state: empty 'triggers' rejected") {
    TempDir td("leviathan_m5_event_empty_triggers");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "", "visible_report": "test report", "true_cause": "test cause",
            "triggers": [],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("triggers must be non-empty")
          != std::string::npos);
}

TEST_CASE("M5.1 load_into_state: trigger target not in allowlist rejected") {
    TempDir td("leviathan_m5_bad_trigger_target");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "", "visible_report": "test report", "true_cause": "test cause",
            "triggers": [
              { "target": "country.gdp", "op": "lt", "value": 100.0 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("'country.gdp' is not in the M5.1 allowlist")
          != std::string::npos);
}

TEST_CASE("M5.1 load_into_state: trigger op not in allowlist rejected") {
    TempDir td("leviathan_m5_bad_trigger_op");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "", "visible_report": "test report", "true_cause": "test cause",
            "triggers": [
              { "target": "country.stability", "op": "eq", "value": 0.5 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("'eq' is not in the M5.1 allowlist")
          != std::string::npos);
}

TEST_CASE("M5.1 load_into_state: trigger value wrong-type rejected") {
    TempDir td("leviathan_m5_bad_trigger_value");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "", "visible_report": "test report", "true_cause": "test cause",
            "triggers": [
              { "target": "country.stability", "op": "lt",
                "value": "not a number" }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find(".value missing or not a number")
          != std::string::npos);
}

TEST_CASE("M5.1 load_into_state: effect missing 'op' rejected (matches policy-effect style)") {
    TempDir td("leviathan_m5_bad_effect_op");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "", "visible_report": "test report", "true_cause": "test cause",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [
              { "target": "country.stability", "value": 0.1 }
            ] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find(".op missing or not a string")
          != std::string::npos);
}

TEST_CASE("M5.1 load_into_state: empty 'effects' is allowed (warning-only events)") {
    TempDir td("leviathan_m5_empty_effects_ok");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "warning.json", R"({
        "events": [
          { "id": "warn", "name": "Warning Event", "description": "", "visible_report": "warn report", "true_cause": "warn cause",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.20 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/warning.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r);
    REQUIRE(state.events.size() == 1u);
    CHECK(state.events[0].id_code == "warn");
    CHECK(state.events[0].effects.empty());
}

TEST_CASE("M5.1 load_into_state: duplicate event id across files is rejected") {
    TempDir td("leviathan_m5_dup_event_id");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "one.json", R"({
        "events": [
          { "id": "shared", "name": "First", "description": "", "visible_report": "shared report 1", "true_cause": "shared 1",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ]
    })");
    write_file(td.path / "data" / "events" / "two.json", R"({
        "events": [
          { "id": "shared", "name": "Second", "description": "", "visible_report": "shared report 2", "true_cause": "shared 2",
            "triggers": [
              { "target": "country.stability", "op": "gt", "value": 0.8 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path, R"({
  "scenario": {
    "countries": [ "countries/a.json", "countries/b.json" ],
    "factions":  [],
    "policies":  [],
    "events":    [ "events/one.json", "events/two.json" ]
  }
})");

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("'shared' is a duplicate")
          != std::string::npos);
}

TEST_CASE("M5.1 regression: loading events does NOT mutate countries or interest_groups") {
    TempDir td("leviathan_m5_no_mutate");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA", "Alpha"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB", "Bravo"));
    write_file(td.path / "data" / "events" / "core.json", canonical_two_event_file());
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/core.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r);
    REQUIRE(state.countries.size() == 2u);
    CHECK(state.countries[0].id_code   == "AAA");
    CHECK(state.countries[0].stability == doctest::Approx(0.5));
    CHECK(state.countries[0].legitimacy == doctest::Approx(0.5));
    CHECK(state.countries[1].id_code   == "BBB");
    CHECK(state.countries[1].stability == doctest::Approx(0.5));
    CHECK(state.interest_groups.empty());
}

// =====================================================================
// M6.1 - EventDefinition.true_cause (RFC-090 §6.1) loader
// =====================================================================

TEST_CASE("M6.1 load_into_state: canonical-shape events carry their authored true_cause") {
    TempDir td("leviathan_m61_canonical_true_cause");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "core.json",
               canonical_two_event_file());
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/core.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r);
    REQUIRE(state.events.size() == 2u);
    CHECK(state.events[0].true_cause ==
          "The country's actual stability has fallen below the unrest threshold.");
    CHECK(state.events[1].true_cause ==
          "An interest group's actual radicalism has crossed the warning threshold.");
}

TEST_CASE("M6.1 load_into_state: event missing 'true_cause' rejected") {
    TempDir td("leviathan_m61_missing_true_cause");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "",
            "visible_report": "valid",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("true_cause missing or not a string")
          != std::string::npos);
}

TEST_CASE("M6.1 load_into_state: event true_cause wrong type rejected") {
    TempDir td("leviathan_m61_wrong_type_true_cause");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "",
            "visible_report": "valid",
            "true_cause": 42,
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("true_cause missing or not a string")
          != std::string::npos);
}

TEST_CASE("M6.1 load_into_state: event true_cause empty rejected") {
    TempDir td("leviathan_m61_empty_true_cause");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "",
            "visible_report": "valid",
            "true_cause": "",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("true_cause must be non-empty")
          != std::string::npos);
}

// =====================================================================
// M6.2 - EventDefinition.visible_report (RFC-090 §6.2) loader
// =====================================================================

TEST_CASE("M6.2 load_into_state: canonical-shape events carry their authored visible_report") {
    TempDir td("leviathan_m62_canonical_visible_report");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "core.json",
               canonical_two_event_file());
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/core.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r);
    REQUIRE(state.events.size() == 2u);
    CHECK(state.events[0].visible_report ==
          "Reports indicate growing unrest among the population; stability appears to be slipping.");
    CHECK(state.events[1].visible_report ==
          "Intelligence reports a sharp uptick in radical rhetoric from one of the country's interest groups.");
}

TEST_CASE("M6.2 load_into_state: event missing 'visible_report' rejected") {
    TempDir td("leviathan_m62_missing_visible_report");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "",
            "true_cause": "valid",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("visible_report missing or not a string")
          != std::string::npos);
}

TEST_CASE("M6.2 load_into_state: event visible_report wrong type rejected") {
    TempDir td("leviathan_m62_wrong_type_visible_report");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "",
            "visible_report": 42,
            "true_cause": "valid",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("visible_report missing or not a string")
          != std::string::npos);
}

TEST_CASE("M6.2 load_into_state: event visible_report empty rejected") {
    TempDir td("leviathan_m62_empty_visible_report");
    write_file(td.path / "data" / "countries" / "a.json", country_json("AAA"));
    write_file(td.path / "data" / "countries" / "b.json", country_json("BBB"));
    write_file(td.path / "data" / "events" / "bad.json", R"({
        "events": [
          { "id": "x", "name": "X", "description": "",
            "visible_report": "",
            "true_cause": "valid",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ]
    })");
    const auto manifest_path = td.path / "data" / "scenarios" / "m.json";
    write_file(manifest_path,
               make_two_country_manifest_with_events("events/bad.json"));

    GameState state;
    const auto r = sl::load_into_state(state, manifest_path);
    REQUIRE(r.failed());
    CHECK(r.error().find("visible_report must be non-empty")
          != std::string::npos);
}

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/systems/logging_system.hpp"
#include "leviathan/systems/save_system.hpp"
#include "leviathan/systems/time_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::LogSeverity;
namespace lg = leviathan::systems::logging;
namespace lt = leviathan::systems::time;
namespace ss = leviathan::systems::save_system;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------

namespace {

GameState build_seeded_state() {
    GameState state;
    state.current_date = GameDate(1930, 1, 1);
    state.rng.seed     = 19300101u;
    state.rng.counter  = 0u;

    // -- countries -----------------------------------------------------
    CountryState germany;
    germany.id           = CountryId{0};
    germany.id_code      = "GER";
    germany.name         = "Germany";
    germany.display_name = "Germany";
    germany.gdp                       = 100.0;
    germany.tax_revenue               = 18.5;   // non-zero so round-trip is meaningful
    germany.budget_balance            = -3.2;
    germany.legal_tax_burden          = 0.20;
    germany.fiscal_capacity           = 0.50;
    germany.administrative_efficiency = 0.55;
    germany.central_control           = 0.60;
    germany.corruption                = 0.25;
    germany.stability                 = 0.55;
    germany.legitimacy                = 0.55;
    germany.military_power            = 0.50;
    germany.threat_perception         = 0.30;
    state.countries.push_back(std::move(germany));

    CountryState france;
    france.id           = CountryId{1};
    france.id_code      = "FRA";
    france.name         = "France";
    france.display_name = "French Republic";
    france.gdp                       = 80.0;
    france.tax_revenue               = 16.2;
    france.budget_balance            = 1.1;
    france.legal_tax_burden          = 0.22;
    france.fiscal_capacity           = 0.55;
    france.administrative_efficiency = 0.60;
    france.central_control           = 0.65;
    france.corruption                = 0.20;
    france.stability                 = 0.60;
    france.legitimacy                = 0.65;
    france.military_power            = 0.55;
    france.threat_perception         = 0.40;
    state.countries.push_back(std::move(france));

    // -- factions ------------------------------------------------------
    leviathan::core::FactionState gm;
    gm.id              = leviathan::core::FactionId{0};
    gm.country         = CountryId{0};   // links to Germany above
    gm.id_code         = "GER_military";
    gm.country_id_code = "GER";
    gm.name            = "Reichswehr";
    gm.type            = "military";
    gm.support         = 0.45;
    gm.influence       = 0.70;
    gm.radicalism      = 0.30;
    gm.loyalty         = 0.55;
    gm.resources       = 1.20;
    gm.preferred_policies = {"increase_military_budget", "press_censorship"};
    state.factions.push_back(std::move(gm));

    leviathan::core::FactionState fb;
    fb.id              = leviathan::core::FactionId{1};
    fb.country         = CountryId{0};
    fb.id_code         = "GER_bureaucracy";
    fb.country_id_code = "GER";
    fb.name            = "Reichsbeamtenschaft";
    fb.type            = "bureaucracy";
    fb.support         = 0.55;
    fb.influence       = 0.60;
    fb.radicalism      = 0.15;
    fb.loyalty         = 0.70;
    fb.resources       = 0.80;
    fb.preferred_policies = {"administrative_reform"};
    state.factions.push_back(std::move(fb));

    return state;
}

struct TempFile {
    std::filesystem::path path;
    explicit TempFile(std::string name)
        : path(std::filesystem::temp_directory_path() / name) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

}  // namespace

// ---------------------------------------------------------------------
// serialize: always-succeeds smoke
// ---------------------------------------------------------------------

TEST_CASE("serialize: empty GameState produces a well-formed JSON object") {
    GameState state;
    const std::string text = ss::serialize(state);
    CHECK(text.front() == '{');
    CHECK(text.find("\"save_version\": 3")          != std::string::npos);
    CHECK(text.find("\"rng_algorithm_version\": 1") != std::string::npos);
    CHECK(text.find("\"current_date\": \"1930-01-01\"") != std::string::npos);
    // Reserved entity-container keys exist even if empty so a future
    // M1.2+ save can populate them without bumping save_version.
    CHECK(text.find("\"provinces\":") != std::string::npos);
    CHECK(text.find("\"factions\":")  != std::string::npos);
    CHECK(text.find("\"policies\":")  != std::string::npos);
    CHECK(text.find("\"events\":")    != std::string::npos);
}

TEST_CASE("serialize: country, faction, and log entries appear in the output") {
    GameState state = build_seeded_state();
    lg::log_info(state, "test", "main", "hello",
                 {{"k1", "v1"}, {"k2", "v2"}});
    const std::string text = ss::serialize(state);

    // Countries
    CHECK(text.find("\"id_code\": \"GER\"")     != std::string::npos);
    CHECK(text.find("\"id_code\": \"FRA\"")     != std::string::npos);
    CHECK(text.find("\"gdp\": 100.0")           != std::string::npos);
    CHECK(text.find("\"corruption\": 0.25")     != std::string::npos);
    CHECK(text.find("\"military_power\": 0.5")  != std::string::npos);

    // Factions (M1.2)
    CHECK(text.find("\"id_code\": \"GER_military\"")    != std::string::npos);
    CHECK(text.find("\"id_code\": \"GER_bureaucracy\"") != std::string::npos);
    CHECK(text.find("\"type\": \"military\"")           != std::string::npos);
    CHECK(text.find("\"radicalism\": 0.3")              != std::string::npos);
    CHECK(text.find("\"increase_military_budget\"")     != std::string::npos);

    // Log
    CHECK(text.find("\"message\": \"hello\"")  != std::string::npos);
    CHECK(text.find("\"k1\": \"v1\"")          != std::string::npos);
    CHECK(text.find("\"k2\": \"v2\"")          != std::string::npos);
}

// ---------------------------------------------------------------------
// round-trip (in-memory)
// ---------------------------------------------------------------------

TEST_CASE("round-trip: empty GameState is byte-stable in its fields") {
    GameState before;
    const auto r = ss::deserialize(ss::serialize(before));
    REQUIRE(r.ok());
    const GameState& after = r.value();
    CHECK(after.current_date == before.current_date);
    CHECK(after.rng.seed     == before.rng.seed);
    CHECK(after.rng.counter  == before.rng.counter);
    CHECK(after.countries.empty());
    CHECK(after.logs.empty());
}

TEST_CASE("round-trip: every documented field survives") {
    GameState before = build_seeded_state();
    // Advance a few days and capture the rng counter so the test
    // proves counter survives the round trip (not just seed).
    lt::advance_days(before, 5);
    before.rng.counter = 13u;

    lg::log_info (before, "lifecycle", "main", "start");
    lg::log_warn (before, "config",    "main", "fallback",
                  {{"reason", "missing"}});
    lg::log_error(before, "io",        "main", "boom!",
                  {{"path", "data/x.json"}});

    const auto r = ss::deserialize(ss::serialize(before));
    REQUIRE(r.ok());
    const GameState& after = r.value();

    CHECK(after.current_date == before.current_date);
    CHECK(after.rng.seed     == before.rng.seed);
    CHECK(after.rng.counter  == before.rng.counter);

    REQUIRE(after.countries.size() == before.countries.size());
    for (std::size_t i = 0; i < before.countries.size(); ++i) {
        const auto& b = before.countries[i];
        const auto& a = after.countries[i];
        CHECK(a.id.value()        == b.id.value());
        CHECK(a.id_code           == b.id_code);
        CHECK(a.name              == b.name);
        CHECK(a.display_name      == b.display_name);
        CHECK(a.gdp                       == doctest::Approx(b.gdp));
        CHECK(a.tax_revenue               == doctest::Approx(b.tax_revenue));
        CHECK(a.budget_balance            == doctest::Approx(b.budget_balance));
        CHECK(a.legal_tax_burden          == doctest::Approx(b.legal_tax_burden));
        CHECK(a.fiscal_capacity           == doctest::Approx(b.fiscal_capacity));
        CHECK(a.administrative_efficiency == doctest::Approx(b.administrative_efficiency));
        CHECK(a.central_control           == doctest::Approx(b.central_control));
        CHECK(a.corruption                == doctest::Approx(b.corruption));
        CHECK(a.stability                 == doctest::Approx(b.stability));
        CHECK(a.legitimacy                == doctest::Approx(b.legitimacy));
        CHECK(a.military_power            == doctest::Approx(b.military_power));
        CHECK(a.threat_perception         == doctest::Approx(b.threat_perception));
    }

    REQUIRE(after.factions.size() == before.factions.size());
    for (std::size_t i = 0; i < before.factions.size(); ++i) {
        const auto& b = before.factions[i];
        const auto& a = after.factions[i];
        CHECK(a.id.value()       == b.id.value());
        CHECK(a.country.value()  == b.country.value());
        CHECK(a.id_code          == b.id_code);
        CHECK(a.country_id_code  == b.country_id_code);
        CHECK(a.name             == b.name);
        CHECK(a.type             == b.type);
        CHECK(a.support    == doctest::Approx(b.support));
        CHECK(a.influence  == doctest::Approx(b.influence));
        CHECK(a.radicalism == doctest::Approx(b.radicalism));
        CHECK(a.loyalty    == doctest::Approx(b.loyalty));
        CHECK(a.resources  == doctest::Approx(b.resources));
        REQUIRE(a.preferred_policies.size() == b.preferred_policies.size());
        for (std::size_t k = 0; k < b.preferred_policies.size(); ++k) {
            CHECK(a.preferred_policies[k] == b.preferred_policies[k]);
        }
    }

    REQUIRE(after.logs.size() == before.logs.size());
    for (std::size_t i = 0; i < before.logs.size(); ++i) {
        CHECK(after.logs[i].date     == before.logs[i].date);
        CHECK(after.logs[i].category == before.logs[i].category);
        CHECK(after.logs[i].severity == before.logs[i].severity);
        CHECK(after.logs[i].source   == before.logs[i].source);
        CHECK(after.logs[i].message  == before.logs[i].message);
        REQUIRE(after.logs[i].metadata.size() == before.logs[i].metadata.size());
        for (std::size_t k = 0; k < before.logs[i].metadata.size(); ++k) {
            CHECK(after.logs[i].metadata[k].first  == before.logs[i].metadata[k].first);
            CHECK(after.logs[i].metadata[k].second == before.logs[i].metadata[k].second);
        }
    }
}

TEST_CASE("round-trip: RNG counter at uint64 boundary survives") {
    GameState before;
    before.rng.seed    = std::uint64_t{1};
    before.rng.counter = std::numeric_limits<std::uint64_t>::max() - 1;

    const auto r = ss::deserialize(ss::serialize(before));
    REQUIRE(r.ok());
    CHECK(r.value().rng.counter == before.rng.counter);
}

TEST_CASE("round-trip: log metadata insertion order is preserved") {
    GameState before;
    before.current_date = GameDate(1945, 5, 8);
    lg::log_info(before, "test", "main", "ordered",
                 {{"z", "Z"}, {"a", "A"}, {"m", "M"}});
    const auto r = ss::deserialize(ss::serialize(before));
    REQUIRE(r.ok());
    REQUIRE(r.value().logs.size() == 1);
    const auto& md = r.value().logs.front().metadata;
    REQUIRE(md.size() == 3);
    CHECK(md[0].first == "z");
    CHECK(md[1].first == "a");
    CHECK(md[2].first == "m");
}

// ---------------------------------------------------------------------
// version policy
// ---------------------------------------------------------------------

// Shared helper: a full-shape country JSON object as a string. Tests can
// embed this in a countries-array literal or mutate one field for the
// negative-test variants.
namespace {
const std::string kFullCountryJsonObject = R"(
    { "id": 0, "id_code": "GER", "name": "Germany", "display_name": "Germany",
      "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
      "legal_tax_burden": 0.20, "fiscal_capacity": 0.50,
      "administrative_efficiency": 0.55, "central_control": 0.60,
      "corruption": 0.25, "stability": 0.55, "legitimacy": 0.55,
      "military_power": 0.50, "threat_perception": 0.30 })";
}  // namespace

TEST_CASE("deserialize: rejects an unknown save_version") {
    // 99 is well past every supported version. The current valid
    // version (kSaveFormatVersion) is now 3 as of M1.2.
    const std::string text = R"({
        "save_version": 99,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text, "fake.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 99") != std::string::npos);
    CHECK(r.error().find("supports 3") != std::string::npos);
    CHECK(r.error().find("fake.json")  != std::string::npos);
}

TEST_CASE("deserialize: an old v1 save is rejected loudly") {
    // M1.1 bumped kSaveFormatVersion to 2; a save created under v1
    // must NOT load silently (its country shape is incompatible).
    const std::string text = R"({
        "save_version": 1,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 1") != std::string::npos);
    CHECK(r.error().find("supports 3") != std::string::npos);
}

TEST_CASE("deserialize: an old v2 save is rejected loudly") {
    // M1.2 bumped kSaveFormatVersion 2 -> 3. v2 saves had reserved-empty
    // factions arrays; loading a v3-shaped save with an M1.1 binary
    // would have silently lost factions, so we gate strictly rather
    // than rely on the reserved-empty forward-compat note from M0.8.
    const std::string text = R"({
        "save_version": 2,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 2") != std::string::npos);
    CHECK(r.error().find("supports 3") != std::string::npos);
}

TEST_CASE("deserialize: rejects an unknown rng_algorithm_version") {
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 99,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported rng_algorithm_version 99") != std::string::npos);
    CHECK(r.error().find("supports 1") != std::string::npos);
}

TEST_CASE("deserialize: missing save_version is rejected") {
    const std::string text = R"({
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0}
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("missing required field 'save_version'") != std::string::npos);
}

TEST_CASE("deserialize: malformed JSON is rejected") {
    const auto r = ss::deserialize("{this is not json", "bad.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("JSON parse error") != std::string::npos);
    CHECK(r.error().find("bad.json")         != std::string::npos);
}

TEST_CASE("deserialize: top-level non-object is rejected") {
    const auto r = ss::deserialize("[1, 2, 3]");
    REQUIRE(r.failed());
    CHECK(r.error().find("top-level JSON value is not an object") != std::string::npos);
}

TEST_CASE("deserialize: invalid Gregorian current_date is rejected") {
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 1,
        "current_date": "1930-02-30",
        "rng": {"seed": 0, "counter": 0}
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("not a real Gregorian date") != std::string::npos);
    CHECK(r.error().find("1930-02-30") != std::string::npos);
}

TEST_CASE("deserialize: country id above CountryId range is rejected") {
    // CountryId is currently backed by int, so 2^31 must be refused
    // rather than silently truncated. Regression for PR #8 review.
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 2147483648, "id_code": "BIG", "name": "Bad",
              "display_name": "Bad",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5 }
        ]
    })";
    const auto r = ss::deserialize(text, "bad-save.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("countries[0]") != std::string::npos);
    CHECK(r.error().find("id")           != std::string::npos);
    CHECK(r.error().find("out of range") != std::string::npos);
}

TEST_CASE("deserialize: country id at exactly INT_MAX is accepted") {
    // Boundary case: INT_MAX (2^31 - 1) is the largest representable
    // value and should round-trip without complaint.
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 2147483647, "id_code": "MAX", "name": "Max",
              "display_name": "Max",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.ok());
    REQUIRE(r.value().countries.size() == 1);
    CHECK(r.value().countries.front().id.value() == 2147483647);
}

TEST_CASE("deserialize: country with wrong type names its index") {
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": "lots", "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5 }
        ]
    })";
    const auto r = ss::deserialize(text, "bad.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("countries[0]") != std::string::npos);
    CHECK(r.error().find("gdp")          != std::string::npos);
}

TEST_CASE("deserialize: country missing a M1.1 required field is rejected") {
    // Drop "legal_tax_burden" from an otherwise-valid country. The
    // loader must reject it rather than silently default to 0.
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "fiscal_capacity": 0.5,
              "administrative_efficiency": 0.5, "central_control": 0.5,
              "corruption": 0.2, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("countries[0]")     != std::string::npos);
    CHECK(r.error().find("legal_tax_burden") != std::string::npos);
}

TEST_CASE("deserialize: faction with wrong type names its index") {
    // M1.2 regression: corrupted faction must be reported with the
    // factions[N] context, not the generic field error.
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "factions": [
            { "id": 0, "country": 0, "id_code": "GER_military",
              "country_id_code": "GER", "name": "Reichswehr",
              "type": "military",
              "support": "lots", "influence": 0.5, "radicalism": 0.2,
              "loyalty": 0.5, "resources": 1.0,
              "preferred_policies": [] }
        ]
    })";
    const auto r = ss::deserialize(text, "bad.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("factions[0]") != std::string::npos);
    CHECK(r.error().find("support")     != std::string::npos);
}

TEST_CASE("deserialize: faction missing preferred_policies is rejected") {
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "factions": [
            { "id": 0, "country": 0, "id_code": "GER_military",
              "country_id_code": "GER", "name": "Reichswehr",
              "type": "military",
              "support": 0.5, "influence": 0.5, "radicalism": 0.2,
              "loyalty": 0.5, "resources": 1.0 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("factions[0]")        != std::string::npos);
    CHECK(r.error().find("preferred_policies") != std::string::npos);
}

TEST_CASE("deserialize: faction id above FactionId range is rejected") {
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "factions": [
            { "id": 2147483648, "country": 0, "id_code": "BIG",
              "country_id_code": "GER", "name": "Bad", "type": "x",
              "support": 0.5, "influence": 0.5, "radicalism": 0.2,
              "loyalty": 0.5, "resources": 1.0,
              "preferred_policies": [] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("factions[0]") != std::string::npos);
    CHECK(r.error().find("out of range") != std::string::npos);
}

TEST_CASE("deserialize: unknown severity in log is rejected") {
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "logs": [
            { "date": "1930-01-01", "category": "x",
              "severity": "panic", "source": "s", "message": "m" }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("logs[0]") != std::string::npos);
    CHECK(r.error().find("panic")   != std::string::npos);
}

// ---------------------------------------------------------------------
// file I/O round-trip
// ---------------------------------------------------------------------

TEST_CASE("save + load: file round-trip preserves the state") {
    TempFile tmp("leviathan_test_save_roundtrip.json");

    GameState before = build_seeded_state();
    lt::advance_days(before, 7);
    before.rng.counter = 3u;
    lg::log_info(before, "test", "main", "marker");

    const auto save_r = ss::save(before, tmp.path);
    REQUIRE(save_r.ok());
    REQUIRE(std::filesystem::exists(tmp.path));

    const auto load_r = ss::load(tmp.path);
    REQUIRE(load_r.ok());
    const GameState& after = load_r.value();
    CHECK(after.current_date == before.current_date);
    CHECK(after.rng.seed     == before.rng.seed);
    CHECK(after.rng.counter  == before.rng.counter);
    CHECK(after.countries.size() == before.countries.size());
    CHECK(after.logs.size()      == before.logs.size());
}

TEST_CASE("save: creates parent directories if needed") {
    namespace fs = std::filesystem;
    const fs::path base = fs::temp_directory_path() /
                          "leviathan_test_save_parents" /
                          "nested" /
                          "save.json";
    // Clean up the whole base directory before and after.
    std::error_code ec;
    fs::remove_all(fs::temp_directory_path() / "leviathan_test_save_parents", ec);

    GameState state;
    const auto r = ss::save(state, base);
    REQUIRE(r.ok());
    CHECK(fs::exists(base));

    fs::remove_all(fs::temp_directory_path() / "leviathan_test_save_parents", ec);
}

TEST_CASE("load: missing file path is named in the error") {
    const auto r = ss::load("does-not-exist/sav.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("does-not-exist/sav.json") != std::string::npos);
    CHECK(r.error().find("cannot open")             != std::string::npos);
}

TEST_CASE("save + load: empty entity containers round-trip cleanly") {
    TempFile tmp("leviathan_test_save_empty.json");
    GameState before;
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    CHECK(r.value().countries.empty());
    CHECK(r.value().provinces.empty());
    CHECK(r.value().factions.empty());
    CHECK(r.value().policies.empty());
    CHECK(r.value().events.empty());
    CHECK(r.value().logs.empty());
}

TEST_CASE("save + load: factions round-trip via file") {
    // M1.2 end-to-end: serialise a state with factions, write to disk,
    // read back, verify the factions vector survives.
    TempFile tmp("leviathan_test_save_factions.json");
    GameState before = build_seeded_state();

    REQUIRE(ss::save(before, tmp.path).ok());
    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    REQUIRE(r.value().factions.size() == 2);
    CHECK(r.value().factions[0].id_code == "GER_military");
    CHECK(r.value().factions[1].id_code == "GER_bureaucracy");
    CHECK(r.value().factions[0].type    == "military");
    CHECK(r.value().factions[1].type    == "bureaucracy");
    CHECK(r.value().factions[0].preferred_policies.size() == 2);
}

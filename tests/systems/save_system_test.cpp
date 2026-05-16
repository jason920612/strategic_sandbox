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

    CountryState germany;
    germany.id                 = CountryId{0};
    germany.id_code            = "GER";
    germany.name               = "Germany";
    germany.display_name       = "Germany";
    germany.initial_gdp        = 100.0;
    germany.initial_stability  = 0.55;
    state.countries.push_back(std::move(germany));

    CountryState france;
    france.id                 = CountryId{1};
    france.id_code            = "FRA";
    france.name               = "France";
    france.display_name       = "French Republic";
    france.initial_gdp        = 80.0;
    france.initial_stability  = 0.60;
    state.countries.push_back(std::move(france));

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
    CHECK(text.find("\"save_version\": 1")          != std::string::npos);
    CHECK(text.find("\"rng_algorithm_version\": 1") != std::string::npos);
    CHECK(text.find("\"current_date\": \"1930-01-01\"") != std::string::npos);
    // Reserved entity-container keys exist even if empty so a future
    // M1 save can populate them without bumping save_version.
    CHECK(text.find("\"provinces\":") != std::string::npos);
    CHECK(text.find("\"factions\":")  != std::string::npos);
    CHECK(text.find("\"policies\":")  != std::string::npos);
    CHECK(text.find("\"events\":")    != std::string::npos);
}

TEST_CASE("serialize: country and log entries appear in the output") {
    GameState state = build_seeded_state();
    lg::log_info(state, "test", "main", "hello",
                 {{"k1", "v1"}, {"k2", "v2"}});
    const std::string text = ss::serialize(state);

    CHECK(text.find("\"id_code\": \"GER\"")  != std::string::npos);
    CHECK(text.find("\"id_code\": \"FRA\"")  != std::string::npos);
    CHECK(text.find("\"initial_gdp\": 100.0") != std::string::npos);
    CHECK(text.find("\"message\": \"hello\"") != std::string::npos);
    CHECK(text.find("\"k1\": \"v1\"")        != std::string::npos);
    CHECK(text.find("\"k2\": \"v2\"")        != std::string::npos);
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
        CHECK(after.countries[i].id.value()        == before.countries[i].id.value());
        CHECK(after.countries[i].id_code           == before.countries[i].id_code);
        CHECK(after.countries[i].name              == before.countries[i].name);
        CHECK(after.countries[i].display_name      == before.countries[i].display_name);
        CHECK(after.countries[i].initial_gdp       == doctest::Approx(before.countries[i].initial_gdp));
        CHECK(after.countries[i].initial_stability == doctest::Approx(before.countries[i].initial_stability));
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

TEST_CASE("deserialize: rejects an unknown save_version") {
    const std::string text = R"({
        "save_version": 2,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text, "fake.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 2") != std::string::npos);
    CHECK(r.error().find("supports 1") != std::string::npos);
    CHECK(r.error().find("fake.json")  != std::string::npos);
}

TEST_CASE("deserialize: rejects an unknown rng_algorithm_version") {
    const std::string text = R"({
        "save_version": 1,
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
        "save_version": 1,
        "rng_algorithm_version": 1,
        "current_date": "1930-02-30",
        "rng": {"seed": 0, "counter": 0}
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("not a real Gregorian date") != std::string::npos);
    CHECK(r.error().find("1930-02-30") != std::string::npos);
}

TEST_CASE("deserialize: country with wrong type names its index") {
    const std::string text = R"({
        "save_version": 1,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "initial_gdp": "lots", "initial_stability": 0.5 }
        ]
    })";
    const auto r = ss::deserialize(text, "bad.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("countries[0]") != std::string::npos);
    CHECK(r.error().find("initial_gdp")  != std::string::npos);
}

TEST_CASE("deserialize: unknown severity in log is rejected") {
    const std::string text = R"({
        "save_version": 1,
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

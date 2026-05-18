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
    germany.budget.administration     = 0.25;
    germany.budget.military           = 0.35;
    germany.budget.education          = 0.10;
    germany.budget.welfare            = 0.10;
    germany.budget.intelligence       = 0.05;
    germany.budget.infrastructure     = 0.10;
    germany.budget.industry           = 0.05;
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
    france.budget.administration     = 0.25;
    france.budget.military           = 0.30;
    france.budget.education          = 0.15;
    france.budget.welfare            = 0.15;
    france.budget.intelligence       = 0.04;
    france.budget.infrastructure     = 0.06;
    france.budget.industry           = 0.05;
    state.countries.push_back(std::move(france));

    // -- policies (M1.4) -----------------------------------------------
    leviathan::core::PolicyData pm;
    pm.id            = leviathan::core::PolicyId{0};
    pm.id_code       = "increase_military_budget";
    pm.name          = "Increase Military Budget";
    pm.category      = "budget";
    pm.duration_days = 30;
    pm.admin_cost    = 0.10;
    pm.effects = {
        {"country.military_power",   "add",  0.03},
        {"faction:military.support", "add",  0.08},
        {"faction:workers.support",  "add", -0.03},
    };
    state.policies.push_back(std::move(pm));

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
    CHECK(text.find("\"save_version\": 16")         != std::string::npos);
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
    // Budget block (M1.3)
    CHECK(text.find("\"budget\":")              != std::string::npos);
    CHECK(text.find("\"administration\": 0.25") != std::string::npos);

    // Policies (M1.4)
    CHECK(text.find("\"id_code\": \"increase_military_budget\"") != std::string::npos);
    CHECK(text.find("\"category\": \"budget\"")                  != std::string::npos);
    CHECK(text.find("\"duration_days\": 30")                     != std::string::npos);
    CHECK(text.find("\"target\": \"country.military_power\"")    != std::string::npos);

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
        CHECK(a.last_gdp_growth_rate      == doctest::Approx(b.last_gdp_growth_rate));
        CHECK(a.budget.administration     == doctest::Approx(b.budget.administration));
        CHECK(a.budget.military           == doctest::Approx(b.budget.military));
        CHECK(a.budget.education          == doctest::Approx(b.budget.education));
        CHECK(a.budget.welfare            == doctest::Approx(b.budget.welfare));
        CHECK(a.budget.intelligence       == doctest::Approx(b.budget.intelligence));
        CHECK(a.budget.infrastructure     == doctest::Approx(b.budget.infrastructure));
        CHECK(a.budget.industry           == doctest::Approx(b.budget.industry));
    }

    REQUIRE(after.policies.size() == before.policies.size());
    for (std::size_t i = 0; i < before.policies.size(); ++i) {
        const auto& b = before.policies[i];
        const auto& a = after.policies[i];
        CHECK(a.id.value()    == b.id.value());
        CHECK(a.id_code       == b.id_code);
        CHECK(a.name          == b.name);
        CHECK(a.category      == b.category);
        CHECK(a.duration_days == b.duration_days);
        CHECK(a.admin_cost    == doctest::Approx(b.admin_cost));
        REQUIRE(a.effects.size() == b.effects.size());
        for (std::size_t k = 0; k < b.effects.size(); ++k) {
            CHECK(a.effects[k].target == b.effects[k].target);
            CHECK(a.effects[k].op     == b.effects[k].op);
            CHECK(a.effects[k].value  == doctest::Approx(b.effects[k].value));
        }
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

TEST_CASE("deserialize: rejects an unknown save_version (v99)") {
    // 99 is well past every supported version. The current valid
    // version (kSaveFormatVersion) is now 6 as of M1.12.
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
    CHECK(r.error().find("supports 16") != std::string::npos);
    CHECK(r.error().find("fake.json")  != std::string::npos);
}

TEST_CASE("deserialize: an old v3 save is rejected loudly") {
    // M1.3 bumped kSaveFormatVersion 3 -> 4. v3 saves had no budget
    // block on countries; loading one with M1.3 would leave each
    // country's budget zeroed and produce confusing failures later.
    const std::string text = R"({
        "save_version": 3,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 3") != std::string::npos);
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("deserialize: an old v4 save is rejected loudly") {
    // M1.4 bumped kSaveFormatVersion 4 -> 5. v4 saves had empty
    // policies arrays; an M1.4 binary that silently accepted them
    // would lose any in-flight policy state. Strict gating
    // eliminates the ambiguity (same rule as v2/v3 rejections).
    const std::string text = R"({
        "save_version": 4,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 4") != std::string::npos);
    CHECK(r.error().find("supports 16") != std::string::npos);
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
    CHECK(r.error().find("supports 16") != std::string::npos);
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
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("deserialize: an old v5 save is rejected loudly") {
    // M1.12 bumped kSaveFormatVersion 5 -> 6. v5 saves predate the
    // new CountryState::last_gdp_growth_rate field; loading a v5 save
    // with an M1.12 binary would reload with an UNINITIALISED runtime
    // growth rate, which would silently change the next monthly tick.
    // Gate strictly rather than tolerate it.
    const std::string text = R"({
        "save_version": 5,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 5") != std::string::npos);
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("deserialize: an old v6 save is rejected loudly") {
    // M1.15 bumped kSaveFormatVersion 6 -> 7. v6 saves predate the new
    // CountryState::active_policies field; loading a v6 save with an
    // M1.15+ binary would lose every day-0 enactment recorded by
    // scenario_loader. Strict version gating is the same rule we used
    // for v1..v5 rejections.
    const std::string text = R"({
        "save_version": 6,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 6") != std::string::npos);
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("deserialize: an old v7 save is rejected loudly") {
    // M2.1 bumped kSaveFormatVersion 7 -> 8. v7 saves predate the new
    // GameState::player_country field; loading a v7 save with an M2.1+
    // binary would silently default the player selection to invalid,
    // which is exactly the kind of silent state loss the strict
    // version gate exists to prevent.
    const std::string text = R"({
        "save_version": 7,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 7") != std::string::npos);
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("deserialize: an old v8 save is rejected loudly") {
    // M2.4 bumped kSaveFormatVersion 8 -> 9. v8 saves predate the new
    // GameState::applied_commands replay log; loading a v8 save with
    // an M2.4+ binary would silently drop every entry the command
    // queue recorded.
    const std::string text = R"({
        "save_version": 8,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 8") != std::string::npos);
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("deserialize: an old v9 save is rejected loudly") {
    // M2.16 bumped kSaveFormatVersion 9 -> 10. v9 saves predate the
    // new CountryState::government_authority block; loading one
    // with an M2.16+ binary would silently default every country
    // to neutral 0.5 authority across the board, dropping whatever
    // the user originally authored.
    const std::string text = R"({
        "save_version": 9,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 9") != std::string::npos);
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("deserialize: an old v10 save is rejected loudly") {
    // M3.1 bumped kSaveFormatVersion 10 -> 11. v10 saves predate
    // the new root-level GameState::interest_groups array; loading
    // one with an M3.1+ binary would silently default to an empty
    // interest-groups list, dropping whatever the scenario / save
    // originally authored.
    const std::string text = R"({
        "save_version": 10,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 10") != std::string::npos);
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("deserialize: an old v11 save is rejected loudly (M4.1 bumped to v12)") {
    // M4.1 bumped kSaveFormatVersion 11 -> 12. v11 saves predate
    // the typed ProvinceNode array; loading one with an M4.1+
    // binary would either drop the user's map nodes or fabricate
    // blank ones, so we reject loudly rather than tolerate the
    // shape mismatch. M5.1 (v13) carries the same strict gate —
    // the rejection message now says "supports 16" instead of
    // "supports 12", but the rejection itself still fires.
    const std::string text = R"({
        "save_version": 11,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "events": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 11") != std::string::npos);
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("deserialize: an old v12 save is rejected loudly (M5.1 bumped to v13)") {
    // M5.1 bumped kSaveFormatVersion 12 -> 13. v12 saves predate
    // the typed EventDefinition array (M0 stub only emitted an
    // empty array regardless of state); loading one with an
    // M5.1+ binary would silently drop any event definitions
    // the user authored under M5+. We bump strictly. The fixture
    // below is shaped like a valid v12 save (interest_groups +
    // provinces both empty arrays, events absent because v12
    // serialized it as empty but the loader ignored it
    // entirely) — under v13 the version gate trips first, so
    // the rejection is reproducible without a fully-populated
    // post-v12 save.
    const std::string text = R"({
        "save_version": 12,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("unsupported save_version 12") != std::string::npos);
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("deserialize: v11 country missing last_gdp_growth_rate is rejected") {
    // M1.12 made last_gdp_growth_rate a required country field; the
    // field survived the M1.15 v6->v7 and M2.1 v7->v8 bumps. A v8
    // save that drops it must still be rejected loudly so the
    // runtime stability term cannot silently default to 0 on reload.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.20, "fiscal_capacity": 0.50,
              "administrative_efficiency": 0.55, "central_control": 0.60,
              "corruption": 0.25, "stability": 0.55, "legitimacy": 0.55,
              "military_power": 0.50, "threat_perception": 0.30,
              "budget": {
                "administration": 0.25, "military": 0.35, "education": 0.10,
                "welfare": 0.10, "intelligence": 0.05, "infrastructure": 0.10,
                "industry": 0.05
              } }
        ]
    })";
    const auto r = ss::deserialize(text, "no-growth.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("countries[0]")          != std::string::npos);
    CHECK(r.error().find("last_gdp_growth_rate")  != std::string::npos);
}

TEST_CASE("save + load: last_gdp_growth_rate survives the round trip") {
    // Set a non-zero value, round-trip, check we read back exactly.
    TempFile tmp("leviathan_test_save_growth.json");
    GameState before = build_seeded_state();
    before.countries[0].last_gdp_growth_rate =  0.0035;
    before.countries[1].last_gdp_growth_rate = -0.0042;
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    REQUIRE(r.value().countries.size() == 2);
    CHECK(r.value().countries[0].last_gdp_growth_rate == doctest::Approx( 0.0035));
    CHECK(r.value().countries[1].last_gdp_growth_rate == doctest::Approx(-0.0042));
}

TEST_CASE("serialize: country emits an active_policies array (M1.15)") {
    // The field is always written, even when empty, so v7 round-trip
    // of a freshly-built state succeeds without manual seeding.
    GameState state = build_seeded_state();
    const std::string text = ss::serialize(state);
    CHECK(text.find("\"active_policies\": []") != std::string::npos);
}

TEST_CASE("save + load: active_policies survives the round trip") {
    TempFile tmp("leviathan_test_save_active_policies.json");
    GameState before = build_seeded_state();
    before.countries[0].active_policies.push_back(
        {"raise_taxes", leviathan::core::GameDate(1930, 3, 2)});
    before.countries[0].active_policies.push_back(
        {"increase_military_budget", leviathan::core::GameDate(1930, 1, 31)});
    before.countries[1].active_policies.push_back(
        {"expand_welfare", leviathan::core::GameDate(1930, 4, 30)});
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    REQUIRE(r.value().countries.size() == 2);
    const auto& a = r.value().countries[0].active_policies;
    REQUIRE(a.size() == 2);
    CHECK(a[0].policy_id_code == "raise_taxes");
    CHECK(a[0].expires_on     == leviathan::core::GameDate(1930, 3, 2));
    CHECK(a[1].policy_id_code == "increase_military_budget");
    CHECK(a[1].expires_on     == leviathan::core::GameDate(1930, 1, 31));
    const auto& b = r.value().countries[1].active_policies;
    REQUIRE(b.size() == 1);
    CHECK(b[0].policy_id_code == "expand_welfare");
    CHECK(b[0].expires_on     == leviathan::core::GameDate(1930, 4, 30));
}

TEST_CASE("deserialize: v11 country missing active_policies is rejected") {
    // M1.15 made active_policies a required country field; loading a
    // hand-written v8 save without it must fail rather than default to
    // an empty list (which would silently drop scenario day-0 records).
    // M2.16: government_authority is required AND comes before
    // active_policies in load order, so we include it here to make
    // sure the active_policies-missing check is what trips.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.20, "fiscal_capacity": 0.50,
              "administrative_efficiency": 0.55, "central_control": 0.60,
              "corruption": 0.25, "stability": 0.55, "legitimacy": 0.55,
              "military_power": 0.50, "threat_perception": 0.30,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.25, "military": 0.35, "education": 0.10,
                "welfare": 0.10, "intelligence": 0.05, "infrastructure": 0.10,
                "industry": 0.05
              },
              "government_authority": {
                "bureaucratic_compliance": 0.5, "military_loyalty": 0.5,
                "intelligence_capability": 0.5, "media_control": 0.5
              } }
        ]
    })";
    const auto r = ss::deserialize(text, "no-active.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("countries[0]")    != std::string::npos);
    CHECK(r.error().find("active_policies") != std::string::npos);
}

TEST_CASE("deserialize: active_policies entry missing policy_id_code is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.20, "fiscal_capacity": 0.50,
              "administrative_efficiency": 0.55, "central_control": 0.60,
              "corruption": 0.25, "stability": 0.55, "legitimacy": 0.55,
              "military_power": 0.50, "threat_perception": 0.30,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.25, "military": 0.35, "education": 0.10,
                "welfare": 0.10, "intelligence": 0.05, "infrastructure": 0.10,
                "industry": 0.05
              },
              "government_authority": {
                "bureaucratic_compliance": 0.5, "military_loyalty": 0.5,
                "intelligence_capability": 0.5, "media_control": 0.5
              },
              "active_policies": [
                { "expires_on": "1930-02-01" }
              ] }
        ]
    })";
    const auto r = ss::deserialize(text, "bad-active.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("active_policies[0]") != std::string::npos);
    CHECK(r.error().find("policy_id_code")     != std::string::npos);
}

TEST_CASE("deserialize: active_policies entry with malformed expires_on is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.20, "fiscal_capacity": 0.50,
              "administrative_efficiency": 0.55, "central_control": 0.60,
              "corruption": 0.25, "stability": 0.55, "legitimacy": 0.55,
              "military_power": 0.50, "threat_perception": 0.30,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.25, "military": 0.35, "education": 0.10,
                "welfare": 0.10, "intelligence": 0.05, "infrastructure": 0.10,
                "industry": 0.05
              },
              "government_authority": {
                "bureaucratic_compliance": 0.5, "military_loyalty": 0.5,
                "intelligence_capability": 0.5, "media_control": 0.5
              },
              "active_policies": [
                { "policy_id_code": "raise_taxes",
                  "expires_on": "1930-02-30" }
              ] }
        ]
    })";
    const auto r = ss::deserialize(text, "bad-date.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("active_policies[0]") != std::string::npos);
    CHECK(r.error().find("expires_on")         != std::string::npos);
    CHECK(r.error().find("1930-02-30")         != std::string::npos);
}

// ---------------------------------------------------------------------
// M2.1 - player_country round-trip and validation
// ---------------------------------------------------------------------

TEST_CASE("serialize: emits player_country at the root (M2.1)") {
    // Default is invalid() = -1. Serialize must always emit the
    // field so the v8 strict-required loader works against our own
    // output.
    GameState state;
    const std::string text = ss::serialize(state);
    CHECK(text.find("\"player_country\": -1") != std::string::npos);
}

TEST_CASE("save + load: player_country = -1 (default) survives the round trip") {
    TempFile tmp("leviathan_test_save_player_default.json");
    GameState before = build_seeded_state();
    // build_seeded_state leaves player_country at the default invalid().
    REQUIRE(before.player_country == CountryId::invalid());
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    CHECK(r.value().player_country == CountryId::invalid());
    CHECK(r.value().player_country.value() == -1);
}

TEST_CASE("save + load: player_country = a valid index round-trips") {
    TempFile tmp("leviathan_test_save_player_valid.json");
    GameState before = build_seeded_state();
    // build_seeded_state has 2 countries [GER, FRA]; pick FRA.
    before.player_country = CountryId{1};
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    CHECK(r.value().player_country == CountryId{1});
}

TEST_CASE("deserialize: v11 missing player_country is rejected") {
    // M2.1 made player_country a required root-level field.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text, "no-player.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
}

TEST_CASE("deserialize: v11 player_country non-integer is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": "GER",
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
    CHECK(r.error().find("integer")        != std::string::npos);
}

TEST_CASE("deserialize: v11 player_country < -1 is rejected") {
    // -1 is the only valid negative; anything more negative is bogus.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -7,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
    CHECK(r.error().find(">= -1")          != std::string::npos);
}

TEST_CASE("deserialize: v11 player_country index out of range is rejected") {
    // Empty countries[] -> any non-invalid index is out of range.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": 0,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
    CHECK(r.error().find("out of range")   != std::string::npos);
}

TEST_CASE("deserialize: v11 player_country above CountryId int range is rejected") {
    // 2^31 must be refused rather than silently truncated, same rule
    // as country `id` (see existing tests). With no countries loaded
    // this also doubles as an out-of-range check, but we test the
    // int-range message specifically here.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": 2147483648,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
    CHECK(r.error().find("CountryId")      != std::string::npos);
}

// ---------------------------------------------------------------------
// M2.4 - applied_commands round-trip and validation
// ---------------------------------------------------------------------

TEST_CASE("serialize: emits applied_commands at the root (M2.4)") {
    // Default is empty. Serialize must always emit the field so the
    // v9 strict-required loader works against our own output.
    GameState state;
    const std::string text = ss::serialize(state);
    CHECK(text.find("\"applied_commands\": []") != std::string::npos);
}

TEST_CASE("save + load: populated applied_commands round-trips") {
    TempFile tmp("leviathan_test_save_applied_commands.json");
    GameState before = build_seeded_state();
    leviathan::core::AppliedPlayerCommand a;
    a.applied_on            = GameDate(1930, 2, 15);
    a.command.kind          = leviathan::core::PlayerCommandKind::EnactPolicy;
    a.command.policy_id_code = "raise_taxes";
    before.applied_commands.push_back(a);
    leviathan::core::AppliedPlayerCommand b;
    b.applied_on            = GameDate(1930, 4, 1);
    b.command.kind          = leviathan::core::PlayerCommandKind::EnactPolicy;
    b.command.policy_id_code = "increase_education";
    before.applied_commands.push_back(b);
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    const auto& after = r.value().applied_commands;
    REQUIRE(after.size() == 2u);
    CHECK(after[0].applied_on            == GameDate(1930, 2, 15));
    CHECK(after[0].command.policy_id_code == "raise_taxes");
    CHECK(after[1].applied_on            == GameDate(1930, 4, 1));
    CHECK(after[1].command.policy_id_code == "increase_education");
}

TEST_CASE("deserialize: v11 missing applied_commands is rejected") {
    // M2.4 made applied_commands a required root-level field.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": []
    })";
    const auto r = ss::deserialize(text, "no-log.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("applied_commands") != std::string::npos);
}

TEST_CASE("deserialize: v11 applied_commands entry with malformed applied_on is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [
            { "applied_on": "1930-02-30",
              "command": { "kind": "EnactPolicy", "policy_id_code": "raise_taxes" } }
        ]
    })";
    const auto r = ss::deserialize(text, "bad-date.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("applied_commands[0]") != std::string::npos);
    CHECK(r.error().find("applied_on")          != std::string::npos);
    CHECK(r.error().find("1930-02-30")          != std::string::npos);
}

TEST_CASE("deserialize: v11 applied_commands entry with unknown kind is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [
            { "applied_on": "1930-02-15",
              "command": { "kind": "SomethingBogus", "policy_id_code": "x" } }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("applied_commands[0]")  != std::string::npos);
    CHECK(r.error().find("SomethingBogus")       != std::string::npos);
    CHECK(r.error().find("unknown player command kind") != std::string::npos);
}

TEST_CASE("deserialize: v11 applied_commands entry missing policy_id_code is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [
            { "applied_on": "1930-02-15",
              "command": { "kind": "EnactPolicy" } }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("applied_commands[0]") != std::string::npos);
    CHECK(r.error().find("policy_id_code")      != std::string::npos);
}

TEST_CASE("save + load: AdjustBudget applied_commands entry round-trips (M2.5)") {
    TempFile tmp("leviathan_test_save_adjust_budget_log.json");
    GameState before = build_seeded_state();
    leviathan::core::AppliedPlayerCommand a;
    a.applied_on = GameDate(1930, 5, 1);
    a.command.kind            = leviathan::core::PlayerCommandKind::AdjustBudget;
    a.command.budget_category = "military";
    a.command.budget_delta    = 0.04;
    before.applied_commands.push_back(a);
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    REQUIRE(r.value().applied_commands.size() == 1u);
    const auto& entry = r.value().applied_commands[0];
    CHECK(entry.applied_on            == GameDate(1930, 5, 1));
    CHECK(entry.command.kind          ==
          leviathan::core::PlayerCommandKind::AdjustBudget);
    CHECK(entry.command.budget_category == "military");
    CHECK(entry.command.budget_delta    == doctest::Approx(0.04));
}

TEST_CASE("deserialize: v11 AdjustBudget entry missing budget_category is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [
            { "applied_on": "1930-05-01",
              "command": { "kind": "AdjustBudget", "budget_delta": 0.04 } }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("applied_commands[0]") != std::string::npos);
    CHECK(r.error().find("budget_category")     != std::string::npos);
}

TEST_CASE("deserialize: v11 AdjustBudget entry missing budget_delta is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [
            { "applied_on": "1930-05-01",
              "command": { "kind": "AdjustBudget", "budget_category": "military" } }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("applied_commands[0]") != std::string::npos);
    CHECK(r.error().find("budget_delta")        != std::string::npos);
}

TEST_CASE("deserialize: v11 applied_commands entry missing command sub-object is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [
            { "applied_on": "1930-02-15" }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("applied_commands[0]") != std::string::npos);
    CHECK(r.error().find("'command'")           != std::string::npos);
}

TEST_CASE("deserialize: rejects an unknown rng_algorithm_version") {
    const std::string text = R"({
        "save_version": 16,
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
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
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
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 2147483648, "id_code": "BIG", "name": "Bad",
              "display_name": "Bad",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.2, "military": 0.2, "education": 0.1,
                "welfare": 0.1, "intelligence": 0.1, "infrastructure": 0.1,
                "industry": 0.1
              } }
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
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 2147483647, "id_code": "MAX", "name": "Max",
              "display_name": "Max",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.2, "military": 0.2, "education": 0.1,
                "welfare": 0.1, "intelligence": 0.1, "infrastructure": 0.1,
                "industry": 0.1
              },
              "government_authority": {
                "bureaucratic_compliance": 0.5, "military_loyalty": 0.5,
                "intelligence_capability": 0.5, "media_control": 0.5
              },
              "active_policies": [] }
        ],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.ok());
    REQUIRE(r.value().countries.size() == 1);
    CHECK(r.value().countries.front().id.value() == 2147483647);
}

TEST_CASE("deserialize: country with wrong type names its index") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": "lots", "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.2, "military": 0.2, "education": 0.1,
                "welfare": 0.1, "intelligence": 0.1, "infrastructure": 0.1,
                "industry": 0.1
              } }
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
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "fiscal_capacity": 0.5,
              "administrative_efficiency": 0.5, "central_control": 0.5,
              "corruption": 0.2, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.2, "military": 0.2, "education": 0.1,
                "welfare": 0.1, "intelligence": 0.1, "infrastructure": 0.1,
                "industry": 0.1
              } }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("countries[0]")     != std::string::npos);
    CHECK(r.error().find("legal_tax_burden") != std::string::npos);
}

TEST_CASE("save + load: policies round-trip via file") {
    TempFile tmp("leviathan_test_save_policies.json");
    GameState before = build_seeded_state();
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    REQUIRE(r.value().policies.size() == 1);
    const auto& p = r.value().policies.front();
    CHECK(p.id_code      == "increase_military_budget");
    CHECK(p.duration_days == 30);
    REQUIRE(p.effects.size() == 3);
    CHECK(p.effects[0].target == "country.military_power");
    CHECK(p.effects[2].value  == doctest::Approx(-0.03));
}

TEST_CASE("deserialize: policy missing effects is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "policies": [
            { "id": 0, "id_code": "p", "name": "P", "category": "budget",
              "duration_days": 30, "admin_cost": 0.1 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("policies[0]") != std::string::npos);
    CHECK(r.error().find("effects")     != std::string::npos);
}

TEST_CASE("deserialize: policy effect missing target is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "policies": [
            { "id": 0, "id_code": "p", "name": "P", "category": "budget",
              "duration_days": 30, "admin_cost": 0.1,
              "effects": [ { "op": "add", "value": 0.1 } ] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("policies[0]") != std::string::npos);
    CHECK(r.error().find("effects[0]")  != std::string::npos);
    CHECK(r.error().find("target")      != std::string::npos);
}

TEST_CASE("deserialize: faction with wrong type names its index") {
    // M1.2 regression: corrupted faction must be reported with the
    // factions[N] context, not the generic field error.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
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
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
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
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
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
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
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

TEST_CASE("save + load: budget round-trips via file") {
    // M1.3 end-to-end: budget categories survive the disk round trip.
    TempFile tmp("leviathan_test_save_budget.json");
    GameState before = build_seeded_state();
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    REQUIRE(r.value().countries.size() == 2);
    const auto& after_ger = r.value().countries[0];
    CHECK(after_ger.budget.administration == doctest::Approx(0.25));
    CHECK(after_ger.budget.military       == doctest::Approx(0.35));
    CHECK(after_ger.budget.industry       == doctest::Approx(0.05));
    const auto& after_fra = r.value().countries[1];
    CHECK(after_fra.budget.education      == doctest::Approx(0.15));
    CHECK(after_fra.budget.welfare        == doctest::Approx(0.15));
}

TEST_CASE("deserialize: country missing budget block is rejected") {
    // Country shape that has every M1.1/M1.2 numeric field but no
    // budget object - should fail with the budget-context error.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.5,
              "administrative_efficiency": 0.5, "central_control": 0.5,
              "corruption": 0.2, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.3,
              "last_gdp_growth_rate": 0.0 }
        ]
    })";
    const auto r = ss::deserialize(text, "no-budget.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("countries[0]") != std::string::npos);
    CHECK(r.error().find("budget")       != std::string::npos);
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

// ---------------------------------------------------------------------
// M2.16 - GovernmentAuthorityState save/load
// ---------------------------------------------------------------------

TEST_CASE("serialize: country emits a government_authority block (M2.16)") {
    GameState state = build_seeded_state();
    const std::string text = ss::serialize(state);
    CHECK(text.find("\"government_authority\":")     != std::string::npos);
    CHECK(text.find("\"bureaucratic_compliance\":")  != std::string::npos);
    CHECK(text.find("\"military_loyalty\":")         != std::string::npos);
    CHECK(text.find("\"intelligence_capability\":")  != std::string::npos);
    CHECK(text.find("\"media_control\":")            != std::string::npos);
}

TEST_CASE("save + load: government_authority survives the round trip") {
    TempFile tmp("leviathan_test_save_authority.json");
    GameState before = build_seeded_state();
    before.countries[0].government_authority.bureaucratic_compliance = 0.72;
    before.countries[0].government_authority.military_loyalty        = 0.81;
    before.countries[0].government_authority.intelligence_capability = 0.43;
    before.countries[0].government_authority.media_control           = 0.15;
    before.countries[1].government_authority.bureaucratic_compliance = 0.30;
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    REQUIRE(r.value().countries.size() == 2);
    const auto& ger = r.value().countries[0].government_authority;
    CHECK(ger.bureaucratic_compliance == doctest::Approx(0.72));
    CHECK(ger.military_loyalty        == doctest::Approx(0.81));
    CHECK(ger.intelligence_capability == doctest::Approx(0.43));
    CHECK(ger.media_control           == doctest::Approx(0.15));
    const auto& fra = r.value().countries[1].government_authority;
    CHECK(fra.bureaucratic_compliance == doctest::Approx(0.30));
    // Untouched sub-fields keep the default 0.5.
    CHECK(fra.military_loyalty        == doctest::Approx(0.50));
}

TEST_CASE("deserialize: v11 country missing government_authority is rejected") {
    // The block is required at the save layer. A v10 country without
    // it would otherwise default to all-0.5 authority, masking
    // hand-edited / corrupted saves.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.20, "fiscal_capacity": 0.50,
              "administrative_efficiency": 0.55, "central_control": 0.60,
              "corruption": 0.25, "stability": 0.55, "legitimacy": 0.55,
              "military_power": 0.50, "threat_perception": 0.30,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.25, "military": 0.35, "education": 0.10,
                "welfare": 0.10, "intelligence": 0.05, "infrastructure": 0.10,
                "industry": 0.05
              },
              "active_policies": [] }
        ],
        "applied_commands": []
    })";
    const auto r = ss::deserialize(text, "no-authority.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("countries[0]")         != std::string::npos);
    CHECK(r.error().find("government_authority") != std::string::npos);
}

TEST_CASE("deserialize: v11 government_authority missing a sub-key is rejected") {
    // bureaucratic_compliance omitted; require_ratio must name the
    // sub-key and the government_authority context.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.20, "fiscal_capacity": 0.50,
              "administrative_efficiency": 0.55, "central_control": 0.60,
              "corruption": 0.25, "stability": 0.55, "legitimacy": 0.55,
              "military_power": 0.50, "threat_perception": 0.30,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.25, "military": 0.35, "education": 0.10,
                "welfare": 0.10, "intelligence": 0.05, "infrastructure": 0.10,
                "industry": 0.05
              },
              "government_authority": {
                "military_loyalty": 0.5,
                "intelligence_capability": 0.5,
                "media_control": 0.5
              },
              "active_policies": [] }
        ],
        "applied_commands": []
    })";
    const auto r = ss::deserialize(text, "missing-sub.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("government_authority")     != std::string::npos);
    CHECK(r.error().find("bureaucratic_compliance")  != std::string::npos);
}

TEST_CASE("deserialize: v11 government_authority out-of-range value is rejected") {
    // media_control = 1.5: outside [0, 1]. require_ratio must reject.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "player_country": -1,
        "current_date": "1930-01-01",
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 100.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.20, "fiscal_capacity": 0.50,
              "administrative_efficiency": 0.55, "central_control": 0.60,
              "corruption": 0.25, "stability": 0.55, "legitimacy": 0.55,
              "military_power": 0.50, "threat_perception": 0.30,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.25, "military": 0.35, "education": 0.10,
                "welfare": 0.10, "intelligence": 0.05, "infrastructure": 0.10,
                "industry": 0.05
              },
              "government_authority": {
                "bureaucratic_compliance": 0.5,
                "military_loyalty": 0.5,
                "intelligence_capability": 0.5,
                "media_control": 1.5
              },
              "active_policies": [] }
        ],
        "applied_commands": []
    })";
    const auto r = ss::deserialize(text, "oob.json");
    REQUIRE(r.failed());
    CHECK(r.error().find("government_authority") != std::string::npos);
    CHECK(r.error().find("media_control")        != std::string::npos);
}

// ---------------------------------------------------------------------
// M3.1 - InterestGroupState save/load
// ---------------------------------------------------------------------

TEST_CASE("serialize: root emits an interest_groups array (M3.1)") {
    GameState state;  // empty state has interest_groups empty
    const std::string text = ss::serialize(state);
    CHECK(text.find("\"interest_groups\": []") != std::string::npos);
}

TEST_CASE("save + load: interest_groups round-trip survives the disk write") {
    TempFile tmp("leviathan_test_save_interest_groups.json");
    GameState before = build_seeded_state();
    leviathan::core::InterestGroupState g;
    g.id_code    = "ger_bureaucracy";
    g.name       = "German Bureaucracy";
    g.kind       = leviathan::core::InterestGroupKind::Bureaucracy;
    g.country    = leviathan::core::CountryId{0};   // GER from seeded state.
    g.influence  = 0.62;
    g.loyalty    = 0.41;
    g.radicalism = 0.18;
    before.interest_groups.push_back(g);

    REQUIRE(ss::save(before, tmp.path).ok());
    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    REQUIRE(r.value().interest_groups.size() == 1u);
    const auto& after = r.value().interest_groups[0];
    CHECK(after.id_code    == "ger_bureaucracy");
    CHECK(after.name       == "German Bureaucracy");
    CHECK(after.kind       == leviathan::core::InterestGroupKind::Bureaucracy);
    CHECK(after.country.value() == 0);
    CHECK(after.influence  == doctest::Approx(0.62));
    CHECK(after.loyalty    == doctest::Approx(0.41));
    CHECK(after.radicalism == doctest::Approx(0.18));
}

TEST_CASE("deserialize: v11 missing interest_groups is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups") != std::string::npos);
}

TEST_CASE("deserialize: v11 interest_groups wrong type is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": 0
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups") != std::string::npos);
    CHECK(r.error().find("JSON array")      != std::string::npos);
}

TEST_CASE("deserialize: v11 interest_groups entry unknown kind is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [
            { "id_code": "x", "name": "X", "kind": "FloatingMasons",
              "country": 0,
              "influence": 0.5, "loyalty": 0.5, "radicalism": 0.0 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups[0]") != std::string::npos);
    CHECK(r.error().find("FloatingMasons")     != std::string::npos);
}

TEST_CASE("deserialize: v11 interest_groups country index out of range is rejected") {
    // Empty countries[]; any non-negative country is OOB.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [
            { "id_code": "x", "name": "X", "kind": "Bureaucracy",
              "country": 7,
              "influence": 0.5, "loyalty": 0.5, "radicalism": 0.0 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups[0]") != std::string::npos);
    CHECK(r.error().find("country")            != std::string::npos);
}

TEST_CASE("deserialize: v11 interest_groups out-of-range ratio is rejected") {
    // Earlier version of this test used `country: -1`, which the
    // save loader rejects FIRST via `require_u64`'s non-negative
    // check — the ratio assertion never ran. Provide a single
    // valid `countries[0]` and use `country: 0` so the failure
    // really comes from `influence: 1.5` tripping `require_ratio`.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.2, "military": 0.2, "education": 0.1,
                "welfare": 0.1, "intelligence": 0.1, "infrastructure": 0.1,
                "industry": 0.1
              },
              "government_authority": {
                "bureaucratic_compliance": 0.5, "military_loyalty": 0.5,
                "intelligence_capability": 0.5, "media_control": 0.5
              },
              "active_policies": [] }
        ],
        "logs": [],
        "applied_commands": [],
        "interest_groups": [
            { "id_code": "x", "name": "X", "kind": "Bureaucracy",
              "country": 0,
              "influence": 1.5, "loyalty": 0.5, "radicalism": 0.0 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups[0]") != std::string::npos);
    CHECK(r.error().find("influence")           != std::string::npos);
}

TEST_CASE("deserialize: v11 duplicate interest_groups id_code is rejected") {
    // Single loaded country so country: 0 resolves; both entries
    // share the same id_code, which must trip the duplicate check.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.2, "military": 0.2, "education": 0.1,
                "welfare": 0.1, "intelligence": 0.1, "infrastructure": 0.1,
                "industry": 0.1
              },
              "government_authority": {
                "bureaucratic_compliance": 0.5, "military_loyalty": 0.5,
                "intelligence_capability": 0.5, "media_control": 0.5
              },
              "active_policies": [] }
        ],
        "logs": [],
        "applied_commands": [],
        "interest_groups": [
            { "id_code": "dup", "name": "A", "kind": "Bureaucracy",
              "country": 0,
              "influence": 0.5, "loyalty": 0.5, "radicalism": 0.0 },
            { "id_code": "dup", "name": "B", "kind": "Military",
              "country": 0,
              "influence": 0.5, "loyalty": 0.5, "radicalism": 0.0 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("interest_groups[1]") != std::string::npos);
    CHECK(r.error().find("duplicate")          != std::string::npos);
}

// ---------------------------------------------------------------------
// M4.1 - root-level provinces (typed ProvinceNode array, save v12)
// ---------------------------------------------------------------------

TEST_CASE("serialize: root emits a provinces array (M4.1)") {
    GameState state;  // empty state has provinces empty
    const std::string text = ss::serialize(state);
    CHECK(text.find("\"provinces\": []") != std::string::npos);
}

TEST_CASE("save + load: provinces round-trip survives the disk write") {
    TempFile tmp("leviathan_test_save_provinces.json");
    GameState before = build_seeded_state();
    leviathan::core::ProvinceNode p;
    p.id_code = "berlin";
    p.name    = "Berlin";
    p.owner   = leviathan::core::CountryId{0};
    p.x       = 0.52;
    p.y       = 0.44;
    before.provinces.push_back(std::move(p));
    REQUIRE(ss::save(before, tmp.path).ok());

    const auto r = ss::load(tmp.path);
    REQUIRE(r.ok());
    REQUIRE(r.value().provinces.size() == 1u);
    const auto& after = r.value().provinces[0];
    CHECK(after.id_code        == "berlin");
    CHECK(after.name           == "Berlin");
    CHECK(after.owner.value()  == 0);
    CHECK(after.x              == doctest::Approx(0.52));
    CHECK(after.y              == doctest::Approx(0.44));
}

TEST_CASE("deserialize: v12 missing provinces is rejected") {
    // v12 makes the provinces array a required root field — silently
    // defaulting it to empty would either drop user-authored map
    // nodes or fabricate blanks. Reject loudly.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "events": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("provinces") != std::string::npos);
}

TEST_CASE("deserialize: v12 provinces wrong type is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": "bogus"
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("provinces") != std::string::npos);
}

TEST_CASE("deserialize: v12 provinces owner index out of range is rejected") {
    // Single loaded country at index 0 but the province says owner: 5.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.2, "military": 0.2, "education": 0.1,
                "welfare": 0.1, "intelligence": 0.1, "infrastructure": 0.1,
                "industry": 0.1
              },
              "government_authority": {
                "bureaucratic_compliance": 0.5, "military_loyalty": 0.5,
                "intelligence_capability": 0.5, "media_control": 0.5
              },
              "active_policies": [] }
        ],
        "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [
            { "id_code": "berlin", "name": "Berlin",
              "owner": 5, "x": 0.5, "y": 0.5 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("provinces[0]") != std::string::npos);
    CHECK(r.error().find("owner")        != std::string::npos);
}

TEST_CASE("deserialize: v12 provinces x out of range is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.2, "military": 0.2, "education": 0.1,
                "welfare": 0.1, "intelligence": 0.1, "infrastructure": 0.1,
                "industry": 0.1
              },
              "government_authority": {
                "bureaucratic_compliance": 0.5, "military_loyalty": 0.5,
                "intelligence_capability": 0.5, "media_control": 0.5
              },
              "active_policies": [] }
        ],
        "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [
            { "id_code": "berlin", "name": "Berlin",
              "owner": 0, "x": 1.5, "y": 0.5 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("provinces[0]") != std::string::npos);
    CHECK(r.error().find("'x'")          != std::string::npos);
}

TEST_CASE("deserialize: v12 duplicate provinces id_code is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [
            { "id": 0, "id_code": "GER", "name": "Germany",
              "display_name": "Germany",
              "gdp": 1.0, "tax_revenue": 0.0, "budget_balance": 0.0,
              "legal_tax_burden": 0.1, "fiscal_capacity": 0.1,
              "administrative_efficiency": 0.1, "central_control": 0.1,
              "corruption": 0.1, "stability": 0.5, "legitimacy": 0.5,
              "military_power": 0.5, "threat_perception": 0.5,
              "last_gdp_growth_rate": 0.0,
              "budget": {
                "administration": 0.2, "military": 0.2, "education": 0.1,
                "welfare": 0.1, "intelligence": 0.1, "infrastructure": 0.1,
                "industry": 0.1
              },
              "government_authority": {
                "bureaucratic_compliance": 0.5, "military_loyalty": 0.5,
                "intelligence_capability": 0.5, "media_control": 0.5
              },
              "active_policies": [] }
        ],
        "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [
            { "id_code": "dup", "name": "A",
              "owner": 0, "x": 0.1, "y": 0.1 },
            { "id_code": "dup", "name": "B",
              "owner": 0, "x": 0.2, "y": 0.2 }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("provinces[1]") != std::string::npos);
    CHECK(r.error().find("duplicate")    != std::string::npos);
}

// =====================================================================
// M5.1 - EventDefinition trigger/effect schema foundation (save v13)
// =====================================================================

TEST_CASE("M5.1 serialize: empty state.events emits 'events': [] at root") {
    GameState state;
    const std::string text = ss::serialize(state);
    CHECK(text.find("\"events\": []") != std::string::npos);
}

TEST_CASE("M5.1 serialize + deserialize: round-trip preserves canonical events shape") {
    GameState state;
    {
        leviathan::core::EventDefinition ev;
        ev.id_code        = "low_stability_unrest";
        ev.name           = "Low Stability Unrest";
        ev.description    = "Low stability creates unrest pressure.";
        ev.visible_report = "Reports indicate growing unrest.";
        ev.true_cause     = "The country's actual stability has fallen below the unrest threshold.";
        leviathan::core::EventTrigger t;
        t.target = "country.stability";
        t.op     = "lt";
        t.value  = 0.30;
        ev.triggers.push_back(t);
        leviathan::core::PolicyEffect e;
        e.target = "country.stability";
        e.op     = "add";
        e.value  = -0.02;
        ev.effects.push_back(e);
        state.events.push_back(std::move(ev));
    }
    const std::string text = ss::serialize(state);
    const auto r = ss::deserialize(text);
    REQUIRE(r);
    const auto& loaded = r.value();
    REQUIRE(loaded.events.size() == 1u);
    CHECK(loaded.events[0].id_code  == "low_stability_unrest");
    CHECK(loaded.events[0].name     == "Low Stability Unrest");
    CHECK(loaded.events[0].description ==
          "Low stability creates unrest pressure.");
    REQUIRE(loaded.events[0].triggers.size() == 1u);
    CHECK(loaded.events[0].triggers[0].target == "country.stability");
    CHECK(loaded.events[0].triggers[0].op     == "lt");
    CHECK(loaded.events[0].triggers[0].value  == doctest::Approx(0.30));
    REQUIRE(loaded.events[0].effects.size() == 1u);
    CHECK(loaded.events[0].effects[0].target == "country.stability");
    CHECK(loaded.events[0].effects[0].op     == "add");
    CHECK(loaded.events[0].effects[0].value  == doctest::Approx(-0.02));
}

TEST_CASE("M5.1 deserialize: v13 save missing 'events' key is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("missing required field 'events'")
          != std::string::npos);
}

TEST_CASE("M5.1 deserialize: 'events' wrong-type rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": "bogus"
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'events' has wrong type") != std::string::npos);
}

TEST_CASE("M5.1 deserialize: trigger target not in allowlist rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [
          { "id_code": "x", "name": "X", "description": "", "visible_report": "test report", "true_cause": "test cause",
            "triggers": [
              { "target": "country.gdp", "op": "lt", "value": 100.0 }
            ],
            "effects": [] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'country.gdp' is not in the M5.1 allowlist")
          != std::string::npos);
}

TEST_CASE("M5.1 deserialize: trigger op not in allowlist rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [
          { "id_code": "x", "name": "X", "description": "", "visible_report": "test report", "true_cause": "test cause",
            "triggers": [
              { "target": "country.stability", "op": "eq", "value": 0.5 }
            ],
            "effects": [] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'eq' is not in the M5.1 allowlist")
          != std::string::npos);
}

TEST_CASE("M5.1 deserialize: empty triggers rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [
          { "id_code": "x", "name": "X", "description": "", "visible_report": "test report", "true_cause": "test cause",
            "triggers": [],
            "effects": [] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'triggers' must be non-empty")
          != std::string::npos);
}

TEST_CASE("M5.1 deserialize: duplicate event id_code rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [
          { "id_code": "dup", "name": "First", "description": "", "visible_report": "dup report 1", "true_cause": "dup test 1",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ], "effects": [] },
          { "id_code": "dup", "name": "Second", "description": "", "visible_report": "dup report 2", "true_cause": "dup test 2",
            "triggers": [
              { "target": "country.stability", "op": "gt", "value": 0.8 }
            ], "effects": [] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("duplicate 'id_code' 'dup'")
          != std::string::npos);
}

// =====================================================================
// M5.4 - EventInstance / event_history data skeleton (save v14)
// =====================================================================

TEST_CASE("M5.4 serialize: empty state.event_history emits 'event_history': [] at root") {
    GameState state;
    const std::string text = ss::serialize(state);
    CHECK(text.find("\"event_history\": []") != std::string::npos);
}

TEST_CASE("M5.4 serialize + deserialize: round-trip preserves a canonical history entry") {
    GameState state;
    {
        leviathan::core::EventInstance inst;
        inst.event_id_code = "low_stability_unrest";
        inst.fired_on      = GameDate(1930, 3, 15);
        leviathan::core::EventInstanceActor a;
        a.kind            = "country";
        a.id_code         = "GER";
        a.country_id_code = "GER";
        a.index           = 0;
        inst.actors.push_back(a);
        state.event_history.push_back(std::move(inst));
    }
    const std::string text = ss::serialize(state);
    const auto r = ss::deserialize(text);
    REQUIRE(r);
    const auto& loaded = r.value();
    REQUIRE(loaded.event_history.size() == 1u);
    CHECK(loaded.event_history[0].event_id_code == "low_stability_unrest");
    CHECK(loaded.event_history[0].fired_on      == GameDate(1930, 3, 15));
    REQUIRE(loaded.event_history[0].actors.size() == 1u);
    CHECK(loaded.event_history[0].actors[0].kind            == "country");
    CHECK(loaded.event_history[0].actors[0].id_code         == "GER");
    CHECK(loaded.event_history[0].actors[0].country_id_code == "GER");
    CHECK(loaded.event_history[0].actors[0].index           == 0u);
}

TEST_CASE("M5.4 serialize + deserialize: cross-scope actor entry (country + interest_group) round-trips") {
    GameState state;
    {
        leviathan::core::EventInstance inst;
        inst.event_id_code = "compound";
        inst.fired_on      = GameDate(1930, 6, 1);
        leviathan::core::EventInstanceActor a1;
        a1.kind            = "country";
        a1.id_code         = "GER";
        a1.country_id_code = "GER";
        a1.index           = 0;
        leviathan::core::EventInstanceActor a2;
        a2.kind            = "interest_group";
        a2.id_code         = "fra_workers";
        a2.country_id_code = "FRA";
        a2.index           = 2;
        inst.actors.push_back(a1);
        inst.actors.push_back(a2);
        state.event_history.push_back(std::move(inst));
    }
    const std::string text = ss::serialize(state);
    const auto r = ss::deserialize(text);
    REQUIRE(r);
    const auto& loaded = r.value();
    REQUIRE(loaded.event_history.size() == 1u);
    REQUIRE(loaded.event_history[0].actors.size() == 2u);
    CHECK(loaded.event_history[0].actors[0].kind == "country");
    CHECK(loaded.event_history[0].actors[1].kind == "interest_group");
    CHECK(loaded.event_history[0].actors[1].country_id_code == "FRA");
    CHECK(loaded.event_history[0].actors[1].index           == 2u);
}

TEST_CASE("M5.4 deserialize: an old v13 save is rejected loudly (M5.4 bumped to v14)") {
    const std::string text = R"({
        "save_version": 13,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("M5.4 deserialize: v14 save missing 'event_history' key is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("missing required field 'event_history'")
          != std::string::npos);
}

TEST_CASE("M5.4 deserialize: 'event_history' wrong-type rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": "bogus"
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'event_history' has wrong type")
          != std::string::npos);
}

TEST_CASE("M5.4 deserialize: event_history entry missing 'event_id_code' rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": [
          { "fired_on": "1930-03-15", "actors": [] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("event_history[0]") != std::string::npos);
    CHECK(r.error().find("'event_id_code'")  != std::string::npos);
}

TEST_CASE("M5.4 deserialize: bad fired_on date rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": [
          { "event_id_code": "x", "fired_on": "1930-13-99", "actors": [] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'fired_on'") != std::string::npos);
}

TEST_CASE("M5.4 deserialize: actor kind not in allowlist rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": [
          { "event_id_code": "x", "fired_on": "1930-03-15",
            "actors": [
              { "kind": "faction", "id_code": "GER_mil",
                "country_id_code": "GER", "index": 0 }
            ] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'faction' is not in the M5.4 allowlist")
          != std::string::npos);
}

TEST_CASE("M5.4 deserialize: actor missing country_id_code rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": [
          { "event_id_code": "x", "fired_on": "1930-03-15",
            "actors": [
              { "kind": "country", "id_code": "GER", "index": 0 }
            ] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'country_id_code'") != std::string::npos);
}

TEST_CASE("M5.4 deserialize: 'actors' missing/wrong-type rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": [
          { "event_id_code": "x", "fired_on": "1930-03-15",
            "actors": "not an array" }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'actors'") != std::string::npos);
}

TEST_CASE("M5.4 deserialize: actor with empty id_code rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": [
          { "event_id_code": "x", "fired_on": "1930-03-15",
            "actors": [
              { "kind": "country", "id_code": "",
                "country_id_code": "GER", "index": 0 }
            ] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'id_code' must be non-empty") != std::string::npos);
}

TEST_CASE("M5.4 deserialize: M5.4 does NOT cross-check event_id_code against state.events") {
    // A history entry referencing an unknown event_id_code must
    // load fine — M5.4 deliberately allows this so a save can be
    // reloaded into a different scenario manifest later. Pinning
    // the contract.
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": [
          { "event_id_code": "ghost_event_no_definition",
            "fired_on": "1930-03-15", "actors": [] }
        ]
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r);
    REQUIRE(r.value().event_history.size() == 1u);
    CHECK(r.value().event_history[0].event_id_code == "ghost_event_no_definition");
}

// =====================================================================
// M6.1 - EventDefinition.true_cause (RFC-090 §6.1) save v15
// =====================================================================

TEST_CASE("M6.1 serialize: every events[] entry serializes its true_cause field") {
    GameState state;
    {
        leviathan::core::EventDefinition ev;
        ev.id_code        = "low_stability_unrest";
        ev.name           = "Low Stability Unrest";
        ev.description    = "Low stability creates unrest pressure.";
        ev.visible_report = "Reports indicate growing unrest.";
        ev.true_cause     = "The country's actual stability has fallen below the unrest threshold.";
        leviathan::core::EventTrigger t;
        t.target = "country.stability";
        t.op     = "lt";
        t.value  = 0.30;
        ev.triggers.push_back(t);
        state.events.push_back(std::move(ev));
    }
    const std::string text = ss::serialize(state);
    // Both the field name and the value land in the JSON.
    CHECK(text.find("\"true_cause\":") != std::string::npos);
    CHECK(text.find("The country's actual stability has fallen below the unrest threshold.")
          != std::string::npos);
}

TEST_CASE("M6.1 round-trip: canonical event preserves true_cause") {
    GameState state;
    {
        leviathan::core::EventDefinition ev;
        ev.id_code        = "low_stability_unrest";
        ev.name           = "Low Stability Unrest";
        ev.description    = "Low stability creates unrest pressure.";
        ev.visible_report = "Reports indicate growing unrest.";
        ev.true_cause     = "Stability fell below the unrest threshold.";
        leviathan::core::EventTrigger t;
        t.target = "country.stability";
        t.op     = "lt";
        t.value  = 0.30;
        ev.triggers.push_back(t);
        state.events.push_back(std::move(ev));
    }
    const std::string text = ss::serialize(state);
    const auto r = ss::deserialize(text);
    REQUIRE(r);
    REQUIRE(r.value().events.size() == 1u);
    CHECK(r.value().events[0].true_cause ==
          "Stability fell below the unrest threshold.");
}

TEST_CASE("M6.1 deserialize: a v14 save (no true_cause) is rejected loudly (supports 16)") {
    const std::string text = R"({
        "save_version": 14,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("M6.1 deserialize: save with event missing true_cause is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [
          { "id_code": "x", "name": "X", "description": "",
            "visible_report": "valid report so visible_report check passes; the M6.1 true_cause check below is the one we want to fail",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ],
        "event_history": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'true_cause' missing or not a string")
          != std::string::npos);
}

TEST_CASE("M6.1 deserialize: v15 save with non-string true_cause is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [
          { "id_code": "x", "name": "X", "description": "",
            "visible_report": "valid",
            "true_cause": 42,
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ],
        "event_history": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'true_cause' missing or not a string")
          != std::string::npos);
}

TEST_CASE("M6.1 deserialize: v15 save with empty true_cause is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [
          { "id_code": "x", "name": "X", "description": "",
            "visible_report": "valid",
            "true_cause": "",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ],
        "event_history": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'true_cause' must be non-empty")
          != std::string::npos);
}

// =====================================================================
// M6.2 - EventDefinition.visible_report (RFC-090 §6.2) save v16
// =====================================================================

TEST_CASE("M6.2 serialize: every events[] entry serializes its visible_report field") {
    GameState state;
    {
        leviathan::core::EventDefinition ev;
        ev.id_code        = "x";
        ev.name           = "X";
        ev.description    = "";
        ev.visible_report = "Reports indicate growing unrest.";
        ev.true_cause     = "Stability fell.";
        leviathan::core::EventTrigger t;
        t.target = "country.stability";
        t.op     = "lt";
        t.value  = 0.30;
        ev.triggers.push_back(t);
        state.events.push_back(std::move(ev));
    }
    const std::string text = ss::serialize(state);
    CHECK(text.find("\"visible_report\":") != std::string::npos);
    CHECK(text.find("Reports indicate growing unrest.") != std::string::npos);
}

TEST_CASE("M6.2 round-trip: canonical event preserves visible_report") {
    GameState state;
    {
        leviathan::core::EventDefinition ev;
        ev.id_code        = "low_stability_unrest";
        ev.name           = "Low Stability Unrest";
        ev.description    = "Low stability creates unrest pressure.";
        ev.visible_report = "Reports indicate growing unrest among the population.";
        ev.true_cause     = "Stability fell below the unrest threshold.";
        leviathan::core::EventTrigger t;
        t.target = "country.stability";
        t.op     = "lt";
        t.value  = 0.30;
        ev.triggers.push_back(t);
        state.events.push_back(std::move(ev));
    }
    const std::string text = ss::serialize(state);
    const auto r = ss::deserialize(text);
    REQUIRE(r);
    REQUIRE(r.value().events.size() == 1u);
    CHECK(r.value().events[0].visible_report ==
          "Reports indicate growing unrest among the population.");
}

TEST_CASE("M6.2 deserialize: a v15 save (no visible_report) is rejected loudly (supports 16)") {
    const std::string text = R"({
        "save_version": 15,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [],
        "event_history": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("supports 16") != std::string::npos);
}

TEST_CASE("M6.2 deserialize: save with event missing visible_report is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [
          { "id_code": "x", "name": "X", "description": "",
            "true_cause": "valid",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ],
        "event_history": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'visible_report' missing or not a string")
          != std::string::npos);
}

TEST_CASE("M6.2 deserialize: save with non-string visible_report is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [
          { "id_code": "x", "name": "X", "description": "",
            "visible_report": 42,
            "true_cause": "valid",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ],
        "event_history": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'visible_report' missing or not a string")
          != std::string::npos);
}

TEST_CASE("M6.2 deserialize: save with empty visible_report is rejected") {
    const std::string text = R"({
        "save_version": 16,
        "rng_algorithm_version": 1,
        "current_date": "1930-01-01",
        "player_country": -1,
        "rng": {"seed": 0, "counter": 0},
        "countries": [], "logs": [],
        "applied_commands": [],
        "interest_groups": [],
        "provinces": [],
        "events": [
          { "id_code": "x", "name": "X", "description": "",
            "visible_report": "",
            "true_cause": "valid",
            "triggers": [
              { "target": "country.stability", "op": "lt", "value": 0.5 }
            ],
            "effects": [] }
        ],
        "event_history": []
    })";
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("'visible_report' must be non-empty")
          != std::string::npos);
}

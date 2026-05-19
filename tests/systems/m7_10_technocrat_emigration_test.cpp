// M7.10 technocrat_emigration canonical event unit test
// (RFC-090 §7.10 `加入技術菁英外逃事件`).

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/event_evaluator.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventTrigger;
using leviathan::core::GameState;
using leviathan::core::InterestGroupKind;
using leviathan::core::InterestGroupState;
namespace ee = leviathan::systems::event_evaluator;

namespace {
std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
}  // namespace

#ifdef LEVIATHAN_TEST_DATA_DIR
TEST_CASE("M7.10 technocrat_emigration: event exists with the documented schema") {
    const fs::path p = fs::path{LEVIATHAN_TEST_DATA_DIR} /
                       "events" / "1930_rfc_extended_events.json";
    const std::string text = read_file(p);
    REQUIRE(!text.empty());
    CHECK(text.find("\"id\": \"technocrat_emigration\"") != std::string::npos);
    CHECK(text.find("\"category\": \"economic\"")        != std::string::npos);
}
#endif

TEST_CASE("M7.10 technocrat_emigration: compound trigger fires") {
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "Germany";
    ger.legitimacy = 0.35;
    s.countries.push_back(ger);
    InterestGroupState tech;
    tech.id_code    = "ger_tech";
    tech.name       = "Technocrats";
    tech.kind       = InterestGroupKind::Technocrats;
    tech.country    = CountryId{0};
    tech.radicalism = 0.70;
    s.interest_groups.push_back(tech);

    EventDefinition def;
    def.id_code        = "technocrat_emigration";
    def.name           = "x";
    def.visible_report = "x";
    def.true_cause     = "x";
    def.category       = "economic";
    def.triggers.push_back(EventTrigger{"country.legitimacy",        "lt", 0.40});
    def.triggers.push_back(EventTrigger{"interest_group.radicalism", "gt", 0.60});
    s.events.push_back(std::move(def));

    REQUIRE(ee::match_events(s).size() == 1u);
}

TEST_CASE("M7.10 technocrat_emigration: no fire when legitimacy healthy") {
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "Germany";
    ger.legitimacy = 0.85;
    s.countries.push_back(ger);
    InterestGroupState tech;
    tech.id_code    = "ger_tech";
    tech.name       = "x";
    tech.kind       = InterestGroupKind::Technocrats;
    tech.country    = CountryId{0};
    tech.radicalism = 0.80;
    s.interest_groups.push_back(tech);

    EventDefinition def;
    def.id_code        = "technocrat_emigration";
    def.name           = "x";
    def.visible_report = "x";
    def.true_cause     = "x";
    def.category       = "economic";
    def.triggers.push_back(EventTrigger{"country.legitimacy",        "lt", 0.40});
    def.triggers.push_back(EventTrigger{"interest_group.radicalism", "gt", 0.60});
    s.events.push_back(std::move(def));

    CHECK(ee::match_events(s).empty());
}

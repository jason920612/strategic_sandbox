// M7.7 intelligence_agency_expansion canonical event unit test
// (RFC-090 §7.7 `加入情報部門擴權`).

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
TEST_CASE("M7.7 intelligence_agency_expansion: event exists with the documented schema") {
    const fs::path p = fs::path{LEVIATHAN_TEST_DATA_DIR} /
                       "events" / "1930_rfc_extended_events.json";
    const std::string text = read_file(p);
    REQUIRE(!text.empty());
    CHECK(text.find("\"id\": \"intelligence_agency_expansion\"") != std::string::npos);
    CHECK(text.find("\"category\": \"intelligence\"")            != std::string::npos);
    CHECK(text.find("country.stability")                         != std::string::npos);
    CHECK(text.find("country.government_authority.bureaucratic_compliance")
                                                                 != std::string::npos);
}
#endif

TEST_CASE("M7.7 intelligence_agency_expansion: fires only when both halves satisfy") {
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "Germany";
    ger.stability = 0.30;   // below threshold
    ger.government_authority.bureaucratic_compliance = 0.40;  // below threshold
    s.countries.push_back(ger);

    EventDefinition def;
    def.id_code        = "intelligence_agency_expansion";
    def.name           = "x";
    def.visible_report = "x";
    def.true_cause     = "x";
    def.category       = "intelligence";
    def.triggers.push_back(EventTrigger{"country.stability", "lt", 0.35});
    def.triggers.push_back(EventTrigger{
        "country.government_authority.bureaucratic_compliance",
        "lt", 0.50});
    s.events.push_back(std::move(def));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    CHECK(matches[0].event_id_code == "intelligence_agency_expansion");
}

TEST_CASE("M7.7 intelligence_agency_expansion: no fire when bureaucratic compliance is healthy") {
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "Germany";
    ger.stability = 0.30;
    ger.government_authority.bureaucratic_compliance = 0.80;
    s.countries.push_back(ger);

    EventDefinition def;
    def.id_code        = "intelligence_agency_expansion";
    def.name           = "x";
    def.visible_report = "x";
    def.true_cause     = "x";
    def.category       = "intelligence";
    def.triggers.push_back(EventTrigger{"country.stability", "lt", 0.35});
    def.triggers.push_back(EventTrigger{
        "country.government_authority.bureaucratic_compliance",
        "lt", 0.50});
    s.events.push_back(std::move(def));

    const auto matches = ee::match_events(s);
    CHECK(matches.empty());
}

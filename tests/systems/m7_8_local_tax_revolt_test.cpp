// M7.8 local_tax_revolt canonical event unit test
// (RFC-090 §7.8 `加入地方抗稅`).

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
TEST_CASE("M7.8 local_tax_revolt: event exists with the documented schema") {
    const fs::path p = fs::path{LEVIATHAN_TEST_DATA_DIR} /
                       "events" / "1930_rfc_extended_events.json";
    const std::string text = read_file(p);
    REQUIRE(!text.empty());
    CHECK(text.find("\"id\": \"local_tax_revolt\"") != std::string::npos);
    CHECK(text.find("\"category\": \"fiscal\"")     != std::string::npos);
}
#endif

TEST_CASE("M7.8 local_tax_revolt: fires only when both stability + bureaucratic compliance slip") {
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "Germany";
    ger.stability = 0.35;
    ger.government_authority.bureaucratic_compliance = 0.30;
    s.countries.push_back(ger);

    EventDefinition def;
    def.id_code        = "local_tax_revolt";
    def.name           = "x";
    def.visible_report = "x";
    def.true_cause     = "x";
    def.category       = "fiscal";
    def.triggers.push_back(EventTrigger{"country.stability", "lt", 0.40});
    def.triggers.push_back(EventTrigger{
        "country.government_authority.bureaucratic_compliance",
        "lt", 0.40});
    s.events.push_back(std::move(def));

    REQUIRE(ee::match_events(s).size() == 1u);
}

TEST_CASE("M7.8 local_tax_revolt: no fire when stability healthy") {
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "Germany";
    ger.stability = 0.70;
    ger.government_authority.bureaucratic_compliance = 0.30;
    s.countries.push_back(ger);

    EventDefinition def;
    def.id_code        = "local_tax_revolt";
    def.name           = "x";
    def.visible_report = "x";
    def.true_cause     = "x";
    def.category       = "fiscal";
    def.triggers.push_back(EventTrigger{"country.stability", "lt", 0.40});
    def.triggers.push_back(EventTrigger{
        "country.government_authority.bureaucratic_compliance",
        "lt", 0.40});
    s.events.push_back(std::move(def));

    CHECK(ee::match_events(s).empty());
}

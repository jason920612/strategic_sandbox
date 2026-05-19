// M7.6 media_scandal canonical event unit test
// (RFC-090 §7.6 `加入媒體醜聞`).

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
TEST_CASE("M7.6 media_scandal: event exists in 1930_rfc_extended_events.json with the documented schema") {
    const fs::path p = fs::path{LEVIATHAN_TEST_DATA_DIR} /
                       "events" / "1930_rfc_extended_events.json";
    const std::string text = read_file(p);
    REQUIRE(!text.empty());
    CHECK(text.find("\"id\": \"media_scandal\"")     != std::string::npos);
    CHECK(text.find("\"name\": \"Media Scandal\"")   != std::string::npos);
    CHECK(text.find("\"category\": \"media\"")       != std::string::npos);
    CHECK(text.find("country.legitimacy")            != std::string::npos);
    CHECK(text.find("interest_group.radicalism")     != std::string::npos);
}
#endif

TEST_CASE("M7.6 media_scandal: compound trigger fires only when both halves satisfy") {
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "Germany";
    ger.legitimacy = 0.40;
    s.countries.push_back(ger);

    InterestGroupState media;
    media.id_code    = "ger_media";
    media.name       = "Press";
    media.kind       = InterestGroupKind::Media;
    media.country    = CountryId{0};
    media.radicalism = 0.75;
    s.interest_groups.push_back(media);

    EventDefinition def;
    def.id_code        = "media_scandal";
    def.name           = "x";
    def.visible_report = "x";
    def.true_cause     = "x";
    def.category       = "media";
    def.triggers.push_back(EventTrigger{"country.legitimacy",       "lt", 0.45});
    def.triggers.push_back(EventTrigger{"interest_group.radicalism","gt", 0.70});
    s.events.push_back(std::move(def));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    CHECK(matches[0].event_id_code == "media_scandal");
}

TEST_CASE("M7.6 media_scandal: no fire when legitimacy is healthy even if radicalism high") {
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "Germany";
    ger.legitimacy = 0.80;
    s.countries.push_back(ger);
    InterestGroupState media;
    media.id_code    = "ger_media";
    media.name       = "x";
    media.kind       = InterestGroupKind::Media;
    media.country    = CountryId{0};
    media.radicalism = 0.90;
    s.interest_groups.push_back(media);

    EventDefinition def;
    def.id_code        = "media_scandal";
    def.name           = "x";
    def.visible_report = "x";
    def.true_cause     = "x";
    def.category       = "media";
    def.triggers.push_back(EventTrigger{"country.legitimacy",       "lt", 0.45});
    def.triggers.push_back(EventTrigger{"interest_group.radicalism","gt", 0.70});
    s.events.push_back(std::move(def));

    const auto matches = ee::match_events(s);
    CHECK(matches.empty());
}

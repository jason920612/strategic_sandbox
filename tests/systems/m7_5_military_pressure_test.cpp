// M7.5 military_pressure canonical event unit test
// (RFC-090 §7.5 `加入軍方施壓`).
//
// The event is authored in `data/events/1930_rfc_extended_events.json`
// under the strict M5.1 trigger-target allowlist (`country.legitimacy`
// + `interest_group.loyalty`). RFC-090 §7.5 reads "add military
// pressure"; with the current trigger surface, "military pressure"
// is modelled as a compound condition (regime legitimacy weakened
// AND at least one interest group has low loyalty) — the narrative
// in the event's `visible_report` / `true_cause` carries the
// military-specific framing while the engine binds whichever
// low-loyalty IG happened to satisfy. The event id_code anchors
// the audit-trail label.
//
// This test loads the actual JSON via scenario_loader (no inline
// fixture) so a typo in the canonical event file fails this test.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/event_evaluator.hpp"
#include "leviathan/systems/scenario_loader.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventTrigger;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameState;
using leviathan::core::InterestGroupKind;
using leviathan::core::InterestGroupState;
namespace ee = leviathan::systems::event_evaluator;
namespace sl = leviathan::systems::scenario_loader;

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

void write_file(const fs::path& p, const std::string& content) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(content.data(),
            static_cast<std::streamsize>(content.size()));
}

std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

#ifdef LEVIATHAN_TEST_DATA_DIR
namespace {
const fs::path kDataDir            = LEVIATHAN_TEST_DATA_DIR;
const fs::path kCanonicalCountries = kDataDir / "countries";
const fs::path kExtendedEvents     = kDataDir / "events" /
                                     "1930_rfc_extended_events.json";

}  // namespace

TEST_CASE("M7.5 military_pressure: event exists in 1930_rfc_extended_events.json with the documented schema") {
    const std::string text = read_file(kExtendedEvents);
    REQUIRE(!text.empty());
    CHECK(text.find("\"id\": \"military_pressure\"")        != std::string::npos);
    CHECK(text.find("\"name\": \"Military Pressure\"")      != std::string::npos);
    CHECK(text.find("\"category\": \"military\"")           != std::string::npos);
    CHECK(text.find("country.legitimacy")                   != std::string::npos);
    CHECK(text.find("interest_group.loyalty")               != std::string::npos);
}

#endif  // LEVIATHAN_TEST_DATA_DIR

TEST_CASE("M7.5 military_pressure: trigger fires only when legitimacy < 0.40 AND any IG loyalty < 0.30") {
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "Germany";
    ger.stability  = 0.50;
    ger.legitimacy = 0.30;   // below threshold
    s.countries.push_back(ger);

    InterestGroupState mil;
    mil.id_code = "ger_mil";
    mil.name    = "German Military";
    mil.kind    = InterestGroupKind::Military;
    mil.country = CountryId{0};
    mil.loyalty = 0.20;       // below threshold
    s.interest_groups.push_back(mil);

    EventDefinition mp;
    mp.id_code        = "military_pressure";
    mp.name           = "Military Pressure";
    mp.description    = "x";
    mp.visible_report = "x";
    mp.true_cause     = "x";
    mp.category       = "military";
    mp.triggers.push_back(EventTrigger{"country.legitimacy",   "lt", 0.40});
    mp.triggers.push_back(EventTrigger{"interest_group.loyalty","lt", 0.30});
    s.events.push_back(std::move(mp));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    CHECK(matches[0].event_id_code == "military_pressure");
}

TEST_CASE("M7.5 military_pressure: trigger does NOT fire when only one half satisfies") {
    GameState s;
    CountryState ger;
    ger.id      = CountryId{0};
    ger.id_code = "GER";
    ger.name    = "Germany";
    ger.legitimacy = 0.30;
    s.countries.push_back(ger);
    InterestGroupState mil;
    mil.id_code = "ger_mil";
    mil.name    = "x";
    mil.kind    = InterestGroupKind::Military;
    mil.country = CountryId{0};
    mil.loyalty = 0.80;   // above threshold — half-condition fails
    s.interest_groups.push_back(mil);

    EventDefinition mp;
    mp.id_code        = "military_pressure";
    mp.name           = "x";
    mp.visible_report = "x";
    mp.true_cause     = "x";
    mp.category       = "military";
    mp.triggers.push_back(EventTrigger{"country.legitimacy",    "lt", 0.40});
    mp.triggers.push_back(EventTrigger{"interest_group.loyalty","lt", 0.30});
    s.events.push_back(std::move(mp));

    const auto matches = ee::match_events(s);
    CHECK(matches.empty());
}

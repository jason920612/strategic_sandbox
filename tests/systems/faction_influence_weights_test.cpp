// M7.3 faction.influence trigger / weight-modifier unit tests
// (RFC-090 §7.3 `加入派系影響力權重`, RFC-020 §6).
//
// Pins the new field end-to-end:
//   * scenario_loader / save_system accept `faction.influence`
//     in EventTrigger.target.
//   * event_evaluator binds Faction actors for
//     `faction.influence` triggers (same shape as M7.2's
//     `faction.radicalism` path).
//   * event_evaluator's rank_weighted_events accepts
//     `faction.influence` in WeightModifier targets and raises
//     the event's firing weight when an influential faction is
//     present.
//   * Per-country scoping (issue #112) holds for the new
//     trigger / modifier target.

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/event_evaluator.hpp"
#include "leviathan/systems/event_firer.hpp"
#include "leviathan/systems/save_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventTrigger;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::PolicyEffect;
using leviathan::core::WeightModifier;
namespace ee = leviathan::systems::event_evaluator;
namespace ef = leviathan::systems::event_firer;
namespace ss = leviathan::systems::save_system;

namespace {

CountryState make_country(int id, const std::string& code) {
    CountryState c;
    c.id      = CountryId{id};
    c.id_code = code;
    c.name    = code;
    c.stability  = 0.55;
    c.legitimacy = 0.55;
    return c;
}

FactionState make_faction(int id, int country_id,
                          const std::string& id_code,
                          const std::string& country_id_code,
                          const std::string& type,
                          double influence,
                          double radicalism = 0.30,
                          double loyalty    = 0.50) {
    FactionState f;
    f.id              = FactionId{id};
    f.country         = CountryId{country_id};
    f.id_code         = id_code;
    f.country_id_code = country_id_code;
    f.name            = id_code;
    f.type            = type;
    f.support         = 0.5;
    f.influence       = influence;
    f.radicalism      = radicalism;
    f.loyalty         = loyalty;
    f.resources       = 0.0;
    return f;
}

EventTrigger make_trigger(const std::string& target,
                          const std::string& op,
                          double value) {
    EventTrigger t;
    t.target = target;
    t.op     = op;
    t.value  = value;
    return t;
}

EventDefinition make_event(const std::string& id_code,
                           std::vector<EventTrigger> triggers) {
    EventDefinition d;
    d.id_code        = id_code;
    d.name           = id_code;
    d.visible_report = "vp";
    d.true_cause     = "tc";
    d.category       = "political";
    d.triggers       = std::move(triggers);
    return d;
}

}  // namespace

// =====================================================================
// faction.influence as a trigger target
// =====================================================================

TEST_CASE("M7.3 trigger_matches: faction.influence gt threshold returns true when any faction exceeds") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.30));
    s.factions.push_back(make_faction(1, 0, "X_w", "X", "workers",  0.85));

    const auto trig = make_trigger("faction.influence", "gt", 0.80);
    CHECK(ee::trigger_matches(s, trig) == true);
}

TEST_CASE("M7.3 trigger_matches: faction.influence below threshold returns false") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.30));
    s.factions.push_back(make_faction(1, 0, "X_w", "X", "workers",  0.50));

    const auto trig = make_trigger("faction.influence", "gt", 0.80);
    CHECK(ee::trigger_matches(s, trig) == false);
}

TEST_CASE("M7.3 trigger_actor: faction.influence binds to Faction-kind actor") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.85));

    const auto trig = make_trigger("faction.influence", "gt", 0.80);
    const auto actor = ee::trigger_actor(s, trig);
    REQUIRE(actor.has_value());
    CHECK(actor->kind == ee::TriggerActorKind::Faction);
    CHECK(actor->id_code == "X_m");
}

// =====================================================================
// Per-country scoping for faction.influence (issue #112 preserved)
// =====================================================================

TEST_CASE("M7.3 match_events_for_country: faction.influence binds within named country only") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.countries.push_back(make_country(1, "Y"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.85));
    s.factions.push_back(make_faction(1, 1, "Y_m", "Y", "military", 0.20));
    s.events.push_back(make_event("infl_pressure",
        { make_trigger("faction.influence", "gt", 0.80) }));

    const auto for_x = ee::match_events_for_country(s, CountryId{0});
    REQUIRE(for_x.size() == 1u);
    CHECK(for_x[0].triggers[0].actor.id_code == "X_m");

    const auto for_y = ee::match_events_for_country(s, CountryId{1});
    CHECK(for_y.empty());  // Y's faction is below threshold
}

// =====================================================================
// faction.influence as a WeightModifier target
// =====================================================================

TEST_CASE("M7.3 rank_weighted_events: faction.influence weight modifier raises weight when present") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military",
                                       /*influence*/0.85,
                                       /*radicalism*/0.90));

    EventDefinition def = make_event("crisis",
        { make_trigger("faction.radicalism", "gt", 0.85) });
    WeightModifier wm;
    wm.target       = "faction.influence";
    wm.op           = "gt";
    wm.value        = 0.60;
    wm.weight_delta = 0.5;
    def.weight_modifiers.push_back(wm);
    s.events.push_back(std::move(def));

    const auto r = ee::rank_weighted_events(s);
    REQUIRE(r);
    REQUIRE(r.value().size() == 1u);
    // base weight 1.0 + modifier delta 0.5 because the
    // faction's influence is above 0.60.
    CHECK(r.value()[0].weight == doctest::Approx(1.5));
}

TEST_CASE("M7.3 rank_weighted_events: faction.influence weight modifier leaves weight at base when faction not influential") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military",
                                       /*influence*/0.30));

    EventDefinition def = make_event("crisis",
        { make_trigger("country.stability", "lt", 0.99) });
    WeightModifier wm;
    wm.target       = "faction.influence";
    wm.op           = "gt";
    wm.value        = 0.60;
    wm.weight_delta = 0.5;
    def.weight_modifiers.push_back(wm);
    s.events.push_back(std::move(def));

    const auto r = ee::rank_weighted_events(s);
    REQUIRE(r);
    REQUIRE(r.value().size() == 1u);
    CHECK(r.value()[0].weight == doctest::Approx(1.0));   // base only
}

// =====================================================================
// Save round-trip: faction.influence trigger / modifier survive
// =====================================================================

TEST_CASE("M7.3 save round-trip: faction.influence trigger + weight_modifier survive") {
    GameState before;
    before.countries.push_back(make_country(0, "X"));
    before.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.85));

    EventDefinition def = make_event("infl",
        { make_trigger("faction.influence", "gt", 0.80) });
    WeightModifier wm;
    wm.target       = "faction.influence";
    wm.op           = "gt";
    wm.value        = 0.60;
    wm.weight_delta = 0.5;
    def.weight_modifiers.push_back(wm);
    before.events.push_back(std::move(def));

    const std::string text = ss::serialize(before);
    CHECK(text.find("\"target\": \"faction.influence\"") != std::string::npos);

    const auto r = ss::deserialize(text);
    REQUIRE(r);
    REQUIRE(r.value().events.size() == 1u);
    REQUIRE(r.value().events[0].triggers.size() == 1u);
    CHECK(r.value().events[0].triggers[0].target == "faction.influence");
    REQUIRE(r.value().events[0].weight_modifiers.size() == 1u);
    CHECK(r.value().events[0].weight_modifiers[0].target == "faction.influence");
}

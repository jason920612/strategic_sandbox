// M7.2 faction-radicalism-event unit tests (RFC-090 §7.2,
// RFC-020 §7 / §6).
//
// Pins the new trigger surface end-to-end:
//   * scenario_loader accepts `faction.radicalism` as a trigger
//     target.
//   * event_evaluator binds Faction actors when the trigger
//     target is `faction.radicalism` (both global ANY-satisfies
//     and per-country scopes).
//   * event_evaluator's rank_weighted_events accepts
//     `faction.radicalism` in WeightModifier targets.
//   * event_firer's `to_actor` emits "faction" as the
//     EventInstanceActor.kind string with the owning country
//     resolved by FactionState.country handle.
//   * save_system event_history allowlist accepts "faction" as
//     a valid actor kind.

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
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
                          double radicalism,
                          double loyalty = 0.5) {
    FactionState f;
    f.id              = FactionId{id};
    f.country         = CountryId{country_id};
    f.id_code         = id_code;
    f.country_id_code = country_id_code;
    f.name            = id_code;
    f.type            = type;
    f.support         = 0.5;
    f.influence       = 0.5;
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
// event_evaluator: trigger_matches + trigger_actor on faction.radicalism
// =====================================================================

TEST_CASE("M7.2 trigger_matches: faction.radicalism gt threshold returns true when any faction exceeds") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.40));
    s.factions.push_back(make_faction(1, 0, "X_w", "X", "workers",  0.90));

    const auto trig = make_trigger("faction.radicalism", "gt", 0.85);
    CHECK(ee::trigger_matches(s, trig) == true);
}

TEST_CASE("M7.2 trigger_matches: faction.radicalism gt threshold returns false when no faction qualifies") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.40));
    s.factions.push_back(make_faction(1, 0, "X_w", "X", "workers",  0.50));

    const auto trig = make_trigger("faction.radicalism", "gt", 0.85);
    CHECK(ee::trigger_matches(s, trig) == false);
}

TEST_CASE("M7.2 trigger_matches: empty state.factions evaluates as false") {
    GameState s;
    const auto trig = make_trigger("faction.radicalism", "gt", 0.50);
    CHECK(ee::trigger_matches(s, trig) == false);
}

TEST_CASE("M7.2 trigger_actor: first-faction-in-vector-order wins") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_first",  "X", "military", 0.90));
    s.factions.push_back(make_faction(1, 0, "X_second", "X", "workers",  0.95));

    const auto trig = make_trigger("faction.radicalism", "gt", 0.85);
    const auto actor = ee::trigger_actor(s, trig);
    REQUIRE(actor.has_value());
    CHECK(actor->kind == ee::TriggerActorKind::Faction);
    CHECK(actor->id_code == "X_first");
    CHECK(actor->index == 0u);
    CHECK(actor->country.value() == 0);
}

TEST_CASE("M7.2 trigger_actor: returns nullopt when no faction satisfies") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.40));

    const auto trig = make_trigger("faction.radicalism", "gt", 0.85);
    const auto actor = ee::trigger_actor(s, trig);
    CHECK(!actor.has_value());
}

// =====================================================================
// event_evaluator: per-country scoping
// =====================================================================

TEST_CASE("M7.2 match_events_for_country: faction.* binds within named country only") {
    // Two countries, one each with a high-radicalism faction.
    // The event's faction.radicalism trigger should bind to EACH
    // country's own faction when evaluated for that country —
    // not leak across.
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.countries.push_back(make_country(1, "Y"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.95));
    s.factions.push_back(make_faction(1, 1, "Y_m", "Y", "military", 0.95));
    s.events.push_back(make_event("frc",
        { make_trigger("faction.radicalism", "gt", 0.85) }));

    const auto for_x = ee::match_events_for_country(s, CountryId{0});
    REQUIRE(for_x.size() == 1u);
    REQUIRE(for_x[0].triggers.size() == 1u);
    CHECK(for_x[0].triggers[0].actor.kind == ee::TriggerActorKind::Faction);
    CHECK(for_x[0].triggers[0].actor.id_code == "X_m");

    const auto for_y = ee::match_events_for_country(s, CountryId{1});
    REQUIRE(for_y.size() == 1u);
    REQUIRE(for_y[0].triggers.size() == 1u);
    CHECK(for_y[0].triggers[0].actor.id_code == "Y_m");
}

TEST_CASE("M7.2 match_events_for_country: a country with no qualifying faction does NOT match") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.countries.push_back(make_country(1, "Y"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.95));
    s.factions.push_back(make_faction(1, 1, "Y_m", "Y", "military", 0.40));   // calm
    s.events.push_back(make_event("frc",
        { make_trigger("faction.radicalism", "gt", 0.85) }));

    // Y has no faction over the threshold, so no match for Y
    // even though X qualifies.
    const auto for_y = ee::match_events_for_country(s, CountryId{1});
    CHECK(for_y.empty());
}

// =====================================================================
// event_evaluator: rank_weighted_events accepts faction.radicalism
// =====================================================================

TEST_CASE("M7.2 rank_weighted_events: faction.radicalism modifier is accepted") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.90));

    EventDefinition def = make_event("frc",
        { make_trigger("country.stability", "lt", 0.99) });
    WeightModifier wm;
    wm.target       = "faction.radicalism";
    wm.op           = "gt";
    wm.value        = 0.85;
    wm.weight_delta = 0.5;
    def.weight_modifiers.push_back(wm);
    s.events.push_back(std::move(def));

    const auto r = ee::rank_weighted_events(s);
    REQUIRE(r);
    REQUIRE(r.value().size() == 1u);
    // base weight 1.0 + modifier delta 0.5 because the
    // faction is above 0.85.
    CHECK(r.value()[0].weight == doctest::Approx(1.5));
}

// =====================================================================
// event_firer: to_actor emits "faction" kind string
// =====================================================================

TEST_CASE("M7.2 record_match: Faction trigger actor lands as EventInstanceActor kind=faction") {
    GameState s;
    s.countries.push_back(make_country(0, "X"));
    s.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.95));
    s.events.push_back(make_event("frc",
        { make_trigger("faction.radicalism", "gt", 0.85) }));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    REQUIRE(ef::record_match(s, matches[0], GameDate(1930, 4, 1)));

    REQUIRE(s.event_history.size() == 1u);
    const auto& inst = s.event_history[0];
    REQUIRE(inst.actors.size() == 1u);
    CHECK(inst.actors[0].kind            == "faction");
    CHECK(inst.actors[0].id_code         == "X_m");
    CHECK(inst.actors[0].country_id_code == "X");
}

// =====================================================================
// save_system: round-trip faction actor in event_history
// =====================================================================

TEST_CASE("M7.2 save round-trip: faction actor in event_history serialises + deserialises") {
    GameState before;
    before.countries.push_back(make_country(0, "X"));
    before.factions.push_back(make_faction(0, 0, "X_m", "X", "military", 0.95));
    before.events.push_back(make_event("frc",
        { make_trigger("faction.radicalism", "gt", 0.85) }));

    const auto matches = ee::match_events(before);
    REQUIRE(matches.size() == 1u);
    REQUIRE(ef::record_match(before, matches[0], GameDate(1930, 4, 1)));

    const std::string text = ss::serialize(before);
    CHECK(text.find("\"kind\": \"faction\"") != std::string::npos);

    const auto r = ss::deserialize(text);
    REQUIRE(r);
    const auto& after = r.value();
    REQUIRE(after.event_history.size() == 1u);
    REQUIRE(after.event_history[0].actors.size() == 1u);
    CHECK(after.event_history[0].actors[0].kind == "faction");
    CHECK(after.event_history[0].actors[0].id_code == "X_m");
    CHECK(after.event_history[0].actors[0].country_id_code == "X");
}

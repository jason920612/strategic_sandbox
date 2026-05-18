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
using leviathan::core::EventInstance;
using leviathan::core::EventInstanceActor;
using leviathan::core::EventTrigger;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::InterestGroupKind;
using leviathan::core::InterestGroupState;
using leviathan::core::PolicyEffect;
namespace ee = leviathan::systems::event_evaluator;
namespace ef = leviathan::systems::event_firer;
namespace ss = leviathan::systems::save_system;

// =====================================================================
// helpers
// =====================================================================

namespace {

CountryState make_country(int id, const std::string& code,
                          double stability = 0.5,
                          double legitimacy = 0.5) {
    CountryState c;
    c.id         = CountryId{id};
    c.id_code    = code;
    c.name       = code;
    c.stability  = stability;
    c.legitimacy = legitimacy;
    return c;
}

InterestGroupState make_ig(const std::string& code,
                           int country_id,
                           double radicalism = 0.0,
                           double loyalty = 0.5) {
    InterestGroupState g;
    g.id_code    = code;
    g.name       = code;
    g.kind       = InterestGroupKind::Bureaucracy;
    g.country    = CountryId{country_id};
    g.radicalism = radicalism;
    g.loyalty    = loyalty;
    return g;
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
                           std::vector<EventTrigger> triggers,
                           std::vector<PolicyEffect> effects = {}) {
    EventDefinition d;
    d.id_code    = id_code;
    d.name       = id_code;
    d.true_cause = "test true cause";   // M6.1: required non-empty
    d.triggers   = std::move(triggers);
    d.effects  = std::move(effects);
    return d;
}

}  // namespace

// =====================================================================
// record_match: per-kind actor conversion
// =====================================================================

TEST_CASE("M5.5 record_match: country actor becomes kind=country EventInstanceActor with id_code=country_id_code") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.events.push_back(make_event("unrest", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);

    ef::record_match(s, matches[0], GameDate(1930, 3, 15));
    REQUIRE(s.event_history.size() == 1u);
    const auto& inst = s.event_history[0];
    CHECK(inst.event_id_code == "unrest");
    CHECK(inst.fired_on      == GameDate(1930, 3, 15));
    REQUIRE(inst.actors.size() == 1u);
    CHECK(inst.actors[0].kind            == "country");
    CHECK(inst.actors[0].id_code         == "GER");
    CHECK(inst.actors[0].country_id_code == "GER");
    CHECK(inst.actors[0].index           == 0u);
}

TEST_CASE("M5.5 record_match: interest_group actor resolves owning country's id_code via state lookup") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    s.countries.push_back(make_country(1, "FRA"));
    s.interest_groups.push_back(make_ig("ger_bureau",  0, /*rad*/0.10));
    s.interest_groups.push_back(make_ig("fra_workers", 1, /*rad*/0.85));
    s.events.push_back(make_event("radical_ig", {
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);

    ef::record_match(s, matches[0], GameDate(1930, 4, 1));
    REQUIRE(s.event_history.size() == 1u);
    const auto& inst = s.event_history[0];
    CHECK(inst.event_id_code == "radical_ig");
    REQUIRE(inst.actors.size() == 1u);
    CHECK(inst.actors[0].kind            == "interest_group");
    CHECK(inst.actors[0].id_code         == "fra_workers");
    CHECK(inst.actors[0].country_id_code == "FRA");
    CHECK(inst.actors[0].index           == 1u);
}

TEST_CASE("M5.5 record_match: cross-scope match records both actors in def order") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.interest_groups.push_back(make_ig("ger_bureau", 0, /*rad*/0.85));
    s.events.push_back(make_event("compound", {
        make_trigger("country.stability",         "lt", 0.30),
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);

    ef::record_match(s, matches[0], GameDate(1930, 5, 1));
    REQUIRE(s.event_history.size() == 1u);
    const auto& inst = s.event_history[0];
    REQUIRE(inst.actors.size() == 2u);
    CHECK(inst.actors[0].kind            == "country");
    CHECK(inst.actors[0].id_code         == "GER");
    CHECK(inst.actors[0].country_id_code == "GER");
    CHECK(inst.actors[1].kind            == "interest_group");
    CHECK(inst.actors[1].id_code         == "ger_bureau");
    CHECK(inst.actors[1].country_id_code == "GER");
}

TEST_CASE("M5.5 record_match: fired_on is whatever caller passes — firer does not consult state.current_date") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.20));
    s.current_date = GameDate(1932, 11, 22);
    s.events.push_back(make_event("x", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);

    // Caller passes a date totally unrelated to state.current_date.
    ef::record_match(s, matches[0], GameDate(1942, 7, 4));
    REQUIRE(s.event_history.size() == 1u);
    CHECK(s.event_history[0].fired_on == GameDate(1942, 7, 4));
    // state.current_date is NOT modified.
    CHECK(s.current_date == GameDate(1932, 11, 22));
}

TEST_CASE("M5.5 record_match: empty-triggers match becomes EventInstance with empty actors") {
    // Vacuously-true case (M5.1 loader rejects empty triggers; this
    // is reachable only via hand-built defs / synthesised matches,
    // but the firer is still total over it).
    GameState s;
    ee::EventMatch m;
    m.event_index   = 0;
    m.event_id_code = "vacuous";
    // m.triggers stays empty.
    ef::record_match(s, m, GameDate(1930, 1, 1));
    REQUIRE(s.event_history.size() == 1u);
    CHECK(s.event_history[0].event_id_code == "vacuous");
    CHECK(s.event_history[0].actors.empty());
}

TEST_CASE("M5.5 record_match: IG actor with broken owning-country handle leaves country_id_code empty (save will reject)") {
    // Build an EventMatch by hand whose IG actor references a
    // CountryId that does NOT exist in state.countries. The firer
    // is documented as "always succeeds; leaves country_id_code
    // empty on lookup failure; save layer rejects on next
    // round-trip" — pin that contract.
    //
    // Note: the state has a country (so its OWN deserialize side
    // passes) but no matching IG record, since the hand-built
    // EventMatch bypasses the evaluator's invariant. We only need
    // the country list to be deserialize-clean; the firer takes
    // its actor data straight from the EventMatch we pass in.
    GameState s;
    s.countries.push_back(make_country(0, "GER"));

    ee::EventMatch m;
    m.event_index   = 0;
    m.event_id_code = "x";
    ee::TriggerEvaluation te;
    te.trigger_index    = 0;
    te.actor.kind       = ee::TriggerActorKind::InterestGroup;
    te.actor.id_code    = "orphan_ig";
    te.actor.country    = CountryId{99};   // not in state.countries
    te.actor.index      = 0;
    m.triggers.push_back(te);

    ef::record_match(s, m, GameDate(1930, 1, 1));
    REQUIRE(s.event_history.size() == 1u);
    REQUIRE(s.event_history[0].actors.size() == 1u);
    CHECK(s.event_history[0].actors[0].kind            == "interest_group");
    CHECK(s.event_history[0].actors[0].id_code         == "orphan_ig");
    CHECK(s.event_history[0].actors[0].country_id_code == "");

    // And the save layer rejects it loudly on the next round-trip.
    const std::string text = ss::serialize(s);
    const auto r = ss::deserialize(text);
    REQUIRE(r.failed());
    CHECK(r.error().find("country_id_code") != std::string::npos);
}

// =====================================================================
// record_matches: batch, ordering, accumulation
// =====================================================================

TEST_CASE("M5.5 record_matches: empty input is a no-op") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    const std::vector<ee::EventMatch> none;
    const auto out = ef::record_matches(s, none, GameDate(1930, 1, 1));
    CHECK(out.recorded == 0u);
    CHECK(s.event_history.empty());
}

TEST_CASE("M5.5 record_matches: preserves input order") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.interest_groups.push_back(make_ig("ger_bureau", 0, /*rad*/0.85));
    s.events.push_back(make_event("a_low_stab", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    s.events.push_back(make_event("b_radical", {
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 2u);

    const auto out = ef::record_matches(s, matches, GameDate(1930, 6, 1));
    CHECK(out.recorded == 2u);
    REQUIRE(s.event_history.size() == 2u);
    CHECK(s.event_history[0].event_id_code == "a_low_stab");
    CHECK(s.event_history[1].event_id_code == "b_radical");
}

TEST_CASE("M5.5 record_matches: appends — does NOT replace; multiple calls accumulate") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.20));
    s.events.push_back(make_event("x", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);

    ef::record_matches(s, matches, GameDate(1930, 1, 1));
    ef::record_matches(s, matches, GameDate(1930, 2, 1));
    ef::record_matches(s, matches, GameDate(1930, 3, 1));
    REQUIRE(s.event_history.size() == 3u);
    CHECK(s.event_history[0].fired_on == GameDate(1930, 1, 1));
    CHECK(s.event_history[1].fired_on == GameDate(1930, 2, 1));
    CHECK(s.event_history[2].fired_on == GameDate(1930, 3, 1));
    // M5.5 deliberately does NOT dedup. If the caller wants to
    // skip already-fired events (historical-once semantics), it
    // gates BEFORE calling record_matches.
}

TEST_CASE("M5.5 record_matches: end-to-end with match_events on canonical-shape state") {
    // Canonical-shape state PLUS drifted stability so something
    // actually fires. Verifies the full evaluator -> firer
    // composition; this is the brick the future M5.x runner
    // integration will glue into the monthly pipeline.
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.interest_groups.push_back(make_ig("ger_bureaucracy", 0, /*rad*/0.85));
    s.events.push_back(make_event("low_stability_unrest", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    s.events.push_back(make_event("radical_interest_group_warning", {
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 2u);

    const auto out =
        ef::record_matches(s, matches, GameDate(1930, 4, 1));
    CHECK(out.recorded == 2u);
    REQUIRE(s.event_history.size() == 2u);
    CHECK(s.event_history[0].event_id_code == "low_stability_unrest");
    CHECK(s.event_history[0].actors[0].kind == "country");
    CHECK(s.event_history[0].actors[0].country_id_code == "GER");
    CHECK(s.event_history[1].event_id_code == "radical_interest_group_warning");
    CHECK(s.event_history[1].actors[0].kind == "interest_group");
    CHECK(s.event_history[1].actors[0].country_id_code == "GER");
}

// =====================================================================
// no side effects: firer touches only state.event_history
// =====================================================================

TEST_CASE("M5.5 record_matches: does NOT modify countries / IGs / events / logs / applied_commands / current_date") {
    GameState s;
    s.current_date = GameDate(1932, 5, 5);
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20, /*legit*/0.30));
    s.interest_groups.push_back(make_ig("ger_bureau", 0, /*rad*/0.85));
    s.events.push_back(make_event("compound", {
        make_trigger("country.stability",         "lt", 0.30),
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);

    const auto c_stab_before  = s.countries[0].stability;
    const auto c_legit_before = s.countries[0].legitimacy;
    const auto ig_rad_before  = s.interest_groups[0].radicalism;
    const auto events_size_before = s.events.size();
    const auto logs_size_before   = s.logs.size();
    const auto cmds_size_before   = s.applied_commands.size();
    const auto date_before        = s.current_date;

    ef::record_matches(s, matches, GameDate(1930, 1, 1));

    CHECK(s.countries[0].stability         == c_stab_before);
    CHECK(s.countries[0].legitimacy        == c_legit_before);
    CHECK(s.interest_groups[0].radicalism  == ig_rad_before);
    CHECK(s.events.size()                  == events_size_before);
    CHECK(s.logs.size()                    == logs_size_before);
    CHECK(s.applied_commands.size()        == cmds_size_before);
    CHECK(s.current_date                   == date_before);
}

TEST_CASE("M5.5 record_match: effects vector on the EventDefinition is NOT consulted") {
    // M5.5 is purely the EventMatch -> EventInstance conversion.
    // The matched EventDefinition's `effects` vector is irrelevant
    // at fire time — that's the future effects-applicator's job.
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    PolicyEffect bogus;
    bogus.target = "country.this_does_not_exist";
    bogus.op     = "not_an_op";
    bogus.value  = 99.0;
    s.events.push_back(make_event(
        "with_bogus_effect",
        { make_trigger("country.stability", "lt", 0.30) },
        { bogus }));
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);

    ef::record_match(s, matches[0], GameDate(1930, 1, 1));
    // Fire succeeded — effects vector contents had no influence.
    REQUIRE(s.event_history.size() == 1u);
    CHECK(s.event_history[0].event_id_code == "with_bogus_effect");
    // And the country was NOT mutated (no effect applied).
    CHECK(s.countries[0].stability == 0.20);
}

// =====================================================================
// save round-trip: recorded entry survives a save / load cycle
// =====================================================================

TEST_CASE("M5.5 record_matches: a recorded entry round-trips through save v14") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.interest_groups.push_back(make_ig("ger_bureau", 0, /*rad*/0.85));
    s.events.push_back(make_event("compound", {
        make_trigger("country.stability",         "lt", 0.30),
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    ef::record_matches(s, matches, GameDate(1930, 3, 15));
    REQUIRE(s.event_history.size() == 1u);

    const std::string text = ss::serialize(s);
    const auto r = ss::deserialize(text);
    REQUIRE(r);
    REQUIRE(r.value().event_history.size() == 1u);
    const auto& reloaded = r.value().event_history[0];
    CHECK(reloaded.event_id_code == "compound");
    CHECK(reloaded.fired_on      == GameDate(1930, 3, 15));
    REQUIRE(reloaded.actors.size() == 2u);
    CHECK(reloaded.actors[0].kind            == "country");
    CHECK(reloaded.actors[0].country_id_code == "GER");
    CHECK(reloaded.actors[1].kind            == "interest_group");
    CHECK(reloaded.actors[1].country_id_code == "GER");
}

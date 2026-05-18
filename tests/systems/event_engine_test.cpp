#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/event_engine.hpp"
#include "leviathan/systems/save_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventInstance;
using leviathan::core::EventTrigger;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::InterestGroupKind;
using leviathan::core::InterestGroupState;
using leviathan::core::PolicyEffect;
namespace eng = leviathan::systems::event_engine;
namespace ss  = leviathan::systems::save_system;

// =====================================================================
// helpers
// =====================================================================

namespace {

CountryState make_country(int id, const std::string& code,
                          double stability  = 0.5,
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
    return EventTrigger{target, op, value};
}

PolicyEffect make_effect(const std::string& target,
                         const std::string& op,
                         double value) {
    return PolicyEffect{target, op, value};
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
// tick_events: empty / no-match no-op
// =====================================================================

TEST_CASE("M5.7 tick_events: empty state succeeds with all counts == 0") {
    GameState s;
    const auto r = eng::tick_events(s);
    REQUIRE(r);
    CHECK(r.value().events_matched        == 0);
    CHECK(r.value().events_recorded       == 0);
    CHECK(r.value().events_applied        == 0);
    CHECK(r.value().total_effects_applied == 0);
    CHECK(s.event_history.empty());
}

TEST_CASE("M5.7 tick_events: state with countries but no events.events succeeds with 0 / 0 / 0") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    // state.events stays empty.
    const auto r = eng::tick_events(s);
    REQUIRE(r);
    CHECK(r.value().events_matched == 0);
    CHECK(s.event_history.empty());
}

TEST_CASE("M5.7 tick_events: state where no events match emits no history entry") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.80));  // above threshold
    s.events.push_back(make_event("unrest",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) }));
    const auto r = eng::tick_events(s);
    REQUIRE(r);
    CHECK(r.value().events_matched        == 0);
    CHECK(r.value().events_recorded       == 0);
    CHECK(r.value().events_applied        == 0);
    CHECK(r.value().total_effects_applied == 0);
    CHECK(s.event_history.empty());
    CHECK(s.countries[0].stability == doctest::Approx(0.80));  // untouched
}

// =====================================================================
// tick_events: one match — records + applies
// =====================================================================

TEST_CASE("M5.7 tick_events: one matching event is recorded AND its effects apply") {
    GameState s;
    s.current_date = GameDate(1930, 3, 15);
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.events.push_back(make_event("unrest",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) }));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    CHECK(r.value().events_matched         == 1);
    CHECK(r.value().events_recorded        == 1);
    CHECK(r.value().events_applied         == 1);
    CHECK(r.value().total_effects_applied  == 1);
    REQUIRE(s.event_history.size()         == 1u);
    CHECK(s.event_history[0].event_id_code == "unrest");
    CHECK(s.event_history[0].fired_on      == GameDate(1930, 3, 15));
    CHECK(s.countries[0].stability         == doctest::Approx(0.18));
}

TEST_CASE("M5.7 tick_events: fired_on for every recorded instance is state.current_date") {
    GameState s;
    s.current_date = GameDate(1942, 7, 4);
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.events.push_back(make_event("a",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) }));
    REQUIRE(eng::tick_events(s));
    REQUIRE(s.event_history.size() == 1u);
    CHECK(s.event_history[0].fired_on == GameDate(1942, 7, 4));
}

// =====================================================================
// tick_events: two matches — canonical order preserved
// =====================================================================

TEST_CASE("M5.7 tick_events: two matches are recorded + applied in vector order") {
    GameState s;
    s.current_date = GameDate(1930, 6, 1);
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.interest_groups.push_back(make_ig("ger_bureau", 0, /*rad*/0.85));

    s.events.push_back(make_event("low_stab",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) }));
    s.events.push_back(make_event("radical_ig",
        { make_trigger("interest_group.radicalism", "gt", 0.75) },
        { make_effect("country.legitimacy", "add", -0.01) }));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    CHECK(r.value().events_matched         == 2);
    CHECK(r.value().events_recorded        == 2);
    CHECK(r.value().events_applied         == 2);
    CHECK(r.value().total_effects_applied  == 2);
    REQUIRE(s.event_history.size() == 2u);
    CHECK(s.event_history[0].event_id_code == "low_stab");
    CHECK(s.event_history[1].event_id_code == "radical_ig");
    CHECK(s.countries[0].stability  == doctest::Approx(0.18));  // 0.20 - 0.02
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.49));  // 0.50 - 0.01
}

// =====================================================================
// tick_events: idempotency — repeated calls fire repeatedly (no dedup)
// =====================================================================

TEST_CASE("M5.7 tick_events: calling twice fires twice (M5.7 has no dedup; cooldown is caller policy)") {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.events.push_back(make_event("x",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) }));

    REQUIRE(eng::tick_events(s));
    REQUIRE(eng::tick_events(s));
    REQUIRE(s.event_history.size() == 2u);
    // Stability dropped twice: 0.20 -> 0.18 -> 0.16.
    CHECK(s.countries[0].stability == doctest::Approx(0.16));
}

// =====================================================================
// tick_events: state.current_date / countries-not-yet-matched semantics
// =====================================================================

TEST_CASE("M5.7 tick_events: an event that fires drops state past another event's threshold — second event STILL evaluated") {
    // Pin the "evaluate snapshot first, then fire/apply" ordering.
    // The evaluator runs ONCE at the top of tick_events; the apply
    // pass that mutates state afterwards does NOT re-trigger
    // evaluation. So a second event whose trigger only becomes
    // true *because* the first event's effect dropped a value
    // does NOT fire in the same tick.
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.40));

    s.events.push_back(make_event("first",
        { make_trigger("country.stability", "lt", 0.50) },
        { make_effect("country.stability", "add", -0.20) }));   // 0.40 -> 0.20
    // Second event would match AFTER first's apply (0.20 < 0.30)
    // but tick_events evaluates once: at entry, stability is 0.40,
    // so "second" did NOT match the initial snapshot.
    s.events.push_back(make_event("second",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.legitimacy", "add", -0.10) }));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    CHECK(r.value().events_matched == 1);  // only "first" matched the snapshot
    REQUIRE(s.event_history.size() == 1u);
    CHECK(s.event_history[0].event_id_code == "first");
    CHECK(s.countries[0].stability  == doctest::Approx(0.20));
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.50));  // untouched
}

// =====================================================================
// tick_events: failure path — partial state pinned
// =====================================================================

TEST_CASE("M5.7 tick_events: failed apply_event_effects bubbles up; partial state pinned") {
    // Construct a state where one event matches but the
    // applicator will fail. The simplest way is an IG-scoped
    // trigger whose IG references a CountryId that doesn't exist
    // in state.countries — match_events binds country=Invalid;
    // record_match writes empty country_id_code; apply_event_effects
    // rejects on "empty country_id_code".
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(make_country(0, "GER"));
    // Construct an interest group whose `country` doesn't exist
    // anywhere in state.countries. The evaluator's actor binding
    // will set TriggerActor.country = CountryId{99}, the firer
    // will fail the country lookup and leave country_id_code
    // empty, and apply_event_effects will reject on first-actor
    // country_id_code empty.
    s.interest_groups.push_back(make_ig("orphan_ig", /*country*/99,
                                        /*rad*/0.85));
    s.events.push_back(make_event("orphan_event",
        { make_trigger("interest_group.radicalism", "gt", 0.75) },
        { make_effect("country.stability", "add", -0.02) }));

    const auto r = eng::tick_events(s);
    REQUIRE(r.failed());
    // The match was recorded BEFORE the apply failed, so
    // event_history holds the entry; the effect did NOT land
    // (M5.6 pre-flight atomicity).
    REQUIRE(s.event_history.size() == 1u);
    CHECK(s.event_history[0].event_id_code == "orphan_event");
    CHECK(s.countries[0].stability == doctest::Approx(0.5));    // untouched
}

// =====================================================================
// tick_events: integration with existing save round-trip
// =====================================================================

TEST_CASE("M5.7 tick_events: after-tick state round-trips through save v14") {
    GameState s;
    s.current_date = GameDate(1930, 5, 1);
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.events.push_back(make_event("x",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) }));

    REQUIRE(eng::tick_events(s));
    const std::string text = ss::serialize(s);
    const auto r = ss::deserialize(text);
    REQUIRE(r);
    REQUIRE(r.value().event_history.size() == 1u);
    CHECK(r.value().event_history[0].event_id_code == "x");
    CHECK(r.value().event_history[0].fired_on      == GameDate(1930, 5, 1));
    CHECK(r.value().countries[0].stability         == doctest::Approx(0.18));
}

// =====================================================================
// tick_events: M5.6 regression — does NOT mutate active_policies
// =====================================================================

TEST_CASE("M5.7 tick_events: does NOT append to country.active_policies (events aren't policies)") {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.events.push_back(make_event("x",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) }));

    REQUIRE(eng::tick_events(s));
    CHECK(s.countries[0].active_policies.empty());
}

// =====================================================================
// tick_events: M5.7 does NOT touch state.logs or state.applied_commands
// =====================================================================

TEST_CASE("M5.7 tick_events: does NOT append to state.logs or state.applied_commands") {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.events.push_back(make_event("x",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) }));

    const auto logs_before = s.logs.size();
    const auto cmds_before = s.applied_commands.size();
    REQUIRE(eng::tick_events(s));
    CHECK(s.logs.size()             == logs_before);
    CHECK(s.applied_commands.size() == cmds_before);
}

// =====================================================================
// tick_events: M5.7 does NOT change canonical 1-day run output
// (no auto-wiring into monthly pipeline; pinned indirectly by the
// fact this test never calls runner::run + canonical scenario)
// =====================================================================

TEST_CASE("M5.7 tick_events: empty state.events keeps state byte-identical") {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(make_country(0, "GER", 0.50));
    s.countries.push_back(make_country(1, "FRA", 0.60));
    s.interest_groups.push_back(make_ig("ger_bureau", 0, 0.10));

    const std::string before = ss::serialize(s);
    REQUIRE(eng::tick_events(s));
    const std::string after = ss::serialize(s);
    CHECK(before == after);
}

// =====================================================================
// M6.1 runtime regression: tick_events does NOT consult true_cause
// =====================================================================

TEST_CASE("M6.1 tick_events: events with different true_cause fire identically") {
    // Two states differing ONLY in true_cause should produce
    // identical event firing semantics (same matched count, same
    // recorded count, same applied count, same field mutation).
    // Pins that no part of the engine (evaluator, firer, effects
    // applicator, composition) reads true_cause.
    GameState s_a;
    s_a.current_date = GameDate(1930, 1, 1);
    s_a.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    auto def_a = make_event("unrest",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) });
    def_a.true_cause = "narrative version A";
    s_a.events.push_back(def_a);

    GameState s_b;
    s_b.current_date = GameDate(1930, 1, 1);
    s_b.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    auto def_b = make_event("unrest",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) });
    def_b.true_cause = "completely different narrative version B";
    s_b.events.push_back(def_b);

    const auto r_a = eng::tick_events(s_a);
    const auto r_b = eng::tick_events(s_b);
    REQUIRE(r_a);
    REQUIRE(r_b);
    CHECK(r_a.value().events_matched         == r_b.value().events_matched);
    CHECK(r_a.value().events_recorded        == r_b.value().events_recorded);
    CHECK(r_a.value().events_applied         == r_b.value().events_applied);
    CHECK(r_a.value().total_effects_applied  == r_b.value().total_effects_applied);
    CHECK(s_a.countries[0].stability         == doctest::Approx(s_b.countries[0].stability));
    CHECK(s_a.event_history.size()           == s_b.event_history.size());
    REQUIRE(s_a.event_history.size() == 1u);
    // event_history records event_id_code, NOT true_cause.
    CHECK(s_a.event_history[0].event_id_code == s_b.event_history[0].event_id_code);
}

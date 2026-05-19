// RCR-1: unit tests for the event-engine extensions and annual stats.
//
// Covers the corrective batch's new helpers:
//   - event_evaluator::rank_weighted_events (RFC-090 §5.3 / §5.6 / §5.7)
//   - event_effects::select_default_option  (RFC-090 §5.4 / §5.8)
//   - event_effects::resolve_followup_ids   (RFC-090 §5.12)
//   - annual_stats::snapshot / CSV writers  (RFC-090 §3.9 / RFC-010 §5)
//   - event_firer log emission              (RFC-090 §5.9 — additional pin)

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/annual_stats.hpp"
#include "leviathan/systems/event_effects.hpp"
#include "leviathan/systems/event_evaluator.hpp"
#include "leviathan/systems/event_firer.hpp"
#include "leviathan/systems/save_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventOption;
using leviathan::core::EventTrigger;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::PolicyEffect;
using leviathan::core::WeightModifier;
namespace ev_ev = leviathan::systems::event_evaluator;
namespace ev_ef = leviathan::systems::event_effects;
namespace ev_fi = leviathan::systems::event_firer;
namespace ann   = leviathan::systems::annual_stats;

namespace {

CountryState make_country(std::string id_code, double stability = 0.50) {
    CountryState c;
    c.id_code   = std::move(id_code);
    c.name      = c.id_code;
    c.stability = stability;
    return c;
}

EventDefinition make_event(std::string id_code,
                           std::vector<EventTrigger>    triggers,
                           std::vector<WeightModifier>  weight_modifiers = {},
                           std::vector<EventOption>     options          = {},
                           std::vector<std::string>     followup_ids     = {}) {
    EventDefinition d;
    d.id_code            = std::move(id_code);
    d.name               = d.id_code;
    d.visible_report     = "vr";
    d.true_cause         = "tc";
    d.category           = "test";   // issue #112: required non-empty
    d.triggers           = std::move(triggers);
    d.weight_modifiers   = std::move(weight_modifiers);
    d.options            = std::move(options);
    d.followup_event_ids = std::move(followup_ids);
    return d;
}

}  // namespace

// =====================================================================
// RCR-1 event_evaluator::rank_weighted_events (RFC-090 §5.3 / §5.6 / §5.7)
// =====================================================================

TEST_CASE("RCR-1 rank_weighted_events: empty state.events returns empty vector") {
    GameState state;
    const auto r = ev_ev::rank_weighted_events(state);
    REQUIRE(r);
    CHECK(r.value().empty());
}

TEST_CASE("RCR-1 rank_weighted_events: every event ends at kBaseWeight when no modifiers") {
    GameState state;
    state.events.push_back(make_event("a",
        { EventTrigger{"country.stability", "lt", 0.30} }));
    state.events.push_back(make_event("b",
        { EventTrigger{"country.stability", "gt", 0.70} }));

    const auto r = ev_ev::rank_weighted_events(state);
    REQUIRE(r);
    const auto& out = r.value();
    REQUIRE(out.size() == 2u);
    CHECK(out[0].weight == doctest::Approx(ev_ev::kBaseWeight));
    CHECK(out[1].weight == doctest::Approx(ev_ev::kBaseWeight));
}

TEST_CASE("RCR-1 rank_weighted_events: matching modifier raises weight; ties use vector order") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));

    // event a has a modifier that matches (stability < 0.30 → +0.5)
    state.events.push_back(make_event("a",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "lt", 0.30, 0.5} }));
    // event b: base weight, no modifier
    state.events.push_back(make_event("b",
        { EventTrigger{"country.stability", "lt", 0.30} }));

    const auto r = ev_ev::rank_weighted_events(state);
    REQUIRE(r);
    const auto& out = r.value();
    REQUIRE(out.size() == 2u);
    CHECK(out[0].event_id_code == "a");
    CHECK(out[0].weight == doctest::Approx(1.5));
    CHECK(out[1].event_id_code == "b");
    CHECK(out[1].weight == doctest::Approx(1.0));
}

TEST_CASE("RCR-1 rank_weighted_events: non-matching modifier contributes zero") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.60));  // > 0.30, modifier won't match
    state.events.push_back(make_event("a",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "lt", 0.30, 100.0} }));

    const auto r = ev_ev::rank_weighted_events(state);
    REQUIRE(r);
    const auto& out = r.value();
    REQUIRE(out.size() == 1u);
    CHECK(out[0].weight == doctest::Approx(1.0));
}

TEST_CASE("RCR-1 rank_weighted_events: negative weight_delta lowers priority") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.10));
    state.events.push_back(make_event("low",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "lt", 0.30, -0.4} }));
    state.events.push_back(make_event("base",
        { EventTrigger{"country.stability", "lt", 0.30} }));

    const auto r = ev_ev::rank_weighted_events(state);
    REQUIRE(r);
    const auto& out = r.value();
    REQUIRE(out.size() == 2u);
    CHECK(out[0].event_id_code == "base");  // 1.0
    CHECK(out[1].event_id_code == "low");   // 0.6
}

TEST_CASE("RCR-1 rank_weighted_events: deterministic — repeated calls byte-stable") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.10));
    for (int i = 0; i < 5; ++i) {
        state.events.push_back(make_event(
            "e" + std::to_string(i),
            { EventTrigger{"country.stability", "lt", 0.30} },
            { WeightModifier{"country.stability", "lt", 0.30,
                             static_cast<double>(i) * 0.1} }));
    }
    const auto r1 = ev_ev::rank_weighted_events(state);
    const auto r2 = ev_ev::rank_weighted_events(state);
    REQUIRE(r1);
    REQUIRE(r2);
    REQUIRE(r1.value().size() == r2.value().size());
    for (std::size_t i = 0; i < r1.value().size(); ++i) {
        CHECK(r1.value()[i].event_index   == r2.value()[i].event_index);
        CHECK(r1.value()[i].event_id_code == r2.value()[i].event_id_code);
        CHECK(r1.value()[i].weight        == r2.value()[i].weight);
    }
}

// =====================================================================
// Post-M6.7 hardening: rank_weighted_events strict modifier validation
// =====================================================================

TEST_CASE("Hardening: rank_weighted_events REJECTS NaN weight_delta") {
    // Post-M6.7 hardening (`feedback_no_silent_degradation`): a
    // malformed weight modifier no longer silently contributes
    // zero. NaN / ±Inf weight_delta now Result::failure.
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    state.events.push_back(make_event("bad",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "lt", 0.30,
                         std::numeric_limits<double>::quiet_NaN()} }));

    // RNG counter must NOT advance on the failure path. rank_weighted_events
    // is itself RNG-free, but we still pin that the failure surfaces and
    // the state.rng.counter is whatever the caller had.
    const std::uint64_t before = state.rng.counter;
    const auto r = ev_ev::rank_weighted_events(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("weight_delta is not finite") != std::string::npos);
    CHECK(state.rng.counter == before);
}

TEST_CASE("Hardening: rank_weighted_events REJECTS +Inf / -Inf weight_delta") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    const double pinf = std::numeric_limits<double>::infinity();
    state.events.push_back(make_event("bad",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "lt", 0.30, pinf} }));

    auto r = ev_ev::rank_weighted_events(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("weight_delta is not finite") != std::string::npos);

    state.events.clear();
    state.events.push_back(make_event("bad",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "lt", 0.30, -pinf} }));
    r = ev_ev::rank_weighted_events(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("weight_delta is not finite") != std::string::npos);
}

TEST_CASE("Hardening: rank_weighted_events REJECTS non-finite modifier value") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    state.events.push_back(make_event("bad",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "lt",
                         std::numeric_limits<double>::quiet_NaN(), 0.5} }));

    const auto r = ev_ev::rank_weighted_events(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("value is not finite") != std::string::npos);
}

TEST_CASE("Hardening: rank_weighted_events REJECTS unrecognised modifier target") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    state.events.push_back(make_event("bad",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.notarealfield", "lt", 0.30, 0.5} }));

    const auto r = ev_ev::rank_weighted_events(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("country.notarealfield") != std::string::npos);
    CHECK(r.error().find("not in the country/interest_group allowlist")
          != std::string::npos);
}

TEST_CASE("Hardening: rank_weighted_events REJECTS unrecognised modifier op") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    state.events.push_back(make_event("bad",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "eq" /* not in allowlist */,
                         0.30, 0.5} }));

    const auto r = ev_ev::rank_weighted_events(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("op = 'eq'") != std::string::npos);
}

// =====================================================================
// RCR-1 event_effects::select_default_option (RFC-090 §5.4 / §5.8)
// =====================================================================

TEST_CASE("RCR-1 select_default_option: empty options returns nullptr") {
    EventDefinition d = make_event("e",
        { EventTrigger{"country.stability", "lt", 0.30} });
    CHECK(ev_ef::select_default_option(d) == nullptr);
}

TEST_CASE("RCR-1 select_default_option: returns pointer to options[0]") {
    EventDefinition d = make_event("e",
        { EventTrigger{"country.stability", "lt", 0.30} },
        {},
        {
            EventOption{"opt_a", "Option A", {{"country.stability", "add", 0.05}}},
            EventOption{"opt_b", "Option B", {{"country.stability", "add", -0.05}}},
        });
    const auto* p = ev_ef::select_default_option(d);
    REQUIRE(p != nullptr);
    CHECK(p->id_code == "opt_a");
    CHECK(p->label   == "Option A");
    REQUIRE(p->effects.size() == 1u);
    CHECK(p->effects[0].target == "country.stability");
}

// =====================================================================
// RCR-1 event_effects::resolve_followup_ids (RFC-090 §5.12)
// =====================================================================

TEST_CASE("Hardening: resolve_followup_ids empty followup list returns empty vector") {
    GameState state;
    state.events.push_back(make_event("a",
        { EventTrigger{"country.stability", "lt", 0.30} }));
    EventDefinition d = make_event("trig",
        { EventTrigger{"country.stability", "lt", 0.30} });
    const auto r = ev_ef::resolve_followup_ids(state, d);
    REQUIRE(r);
    CHECK(r.value().empty());
}

TEST_CASE("Hardening: resolve_followup_ids resolves matching ids to state.events indices") {
    GameState state;
    state.events.push_back(make_event("alpha",
        { EventTrigger{"country.stability", "lt", 0.30} }));
    state.events.push_back(make_event("beta",
        { EventTrigger{"country.stability", "lt", 0.30} }));
    state.events.push_back(make_event("gamma",
        { EventTrigger{"country.stability", "lt", 0.30} }));

    EventDefinition d = make_event("origin",
        { EventTrigger{"country.stability", "lt", 0.30} },
        {}, {}, {"gamma", "alpha"});

    const auto r = ev_ef::resolve_followup_ids(state, d);
    REQUIRE(r);
    REQUIRE(r.value().size() == 2u);
    CHECK(r.value()[0] == 2u);  // gamma
    CHECK(r.value()[1] == 0u);  // alpha
}

TEST_CASE("Hardening: resolve_followup_ids rejects unresolvable id_code") {
    // Post-M6.7 strict-fallback hardening: an unknown followup
    // id_code is now Result::failure (was silently skipped).
    GameState state;
    state.events.push_back(make_event("alpha",
        { EventTrigger{"country.stability", "lt", 0.30} }));
    EventDefinition d = make_event("origin",
        { EventTrigger{"country.stability", "lt", 0.30} },
        {}, {}, {"ghost", "alpha"});

    const auto r = ev_ef::resolve_followup_ids(state, d);
    REQUIRE(r.failed());
    CHECK(r.error().find("ghost") != std::string::npos);
    CHECK(r.error().find("origin") != std::string::npos);
    CHECK(r.error().find("does not resolve") != std::string::npos);
}

// =====================================================================
// RCR-1 annual_stats (RFC-090 §3.9 / RFC-010 §5)
// =====================================================================

TEST_CASE("Hardening: annual_stats::snapshot rejects empty state.countries") {
    // Post-M6.7 strict-fallback hardening: the previous "empty
    // state produces zeroed averages" success is now a hard
    // failure. The runner gates the call so empty-world
    // simulations skip emission entirely.
    GameState state;
    const auto r = ann::snapshot(state, 1930);
    REQUIRE(r.failed());
    CHECK(r.error().find("state.countries is empty")
          != std::string::npos);
}

TEST_CASE("Hardening: annual_stats::snapshot averages and total over populated state") {
    GameState state;
    auto a = make_country("A"); a.stability = 0.4; a.legitimacy = 0.5; a.gdp = 100.0; a.corruption = 0.2;
    auto b = make_country("B"); b.stability = 0.6; b.legitimacy = 0.7; b.gdp = 200.0; b.corruption = 0.4;
    state.countries.push_back(a);
    state.countries.push_back(b);

    const auto r = ann::snapshot(state, 1940);
    REQUIRE(r);
    const auto& row = r.value();
    CHECK(row.year             == 1940);
    CHECK(row.country_count    == 2u);
    CHECK(row.avg_stability    == doctest::Approx(0.5));
    CHECK(row.avg_legitimacy   == doctest::Approx(0.6));
    CHECK(row.avg_gdp          == doctest::Approx(150.0));
    CHECK(row.avg_corruption   == doctest::Approx(0.3));
    CHECK(row.total_gdp        == doctest::Approx(300.0));
}

TEST_CASE("RCR-1 annual_stats::write_csv_header: stable header text") {
    const auto h = ann::write_csv_header();
    CHECK(h == "date,year,country_count,avg_stability,avg_legitimacy,"
               "avg_gdp,avg_corruption,total_gdp,event_history_count\n");
}

TEST_CASE("RCR-1 annual_stats::write_csv_row: deterministic format") {
    ann::AnnualRow row;
    row.year                = 1930;
    row.country_count       = 3u;
    row.avg_stability       = 0.55;
    row.avg_legitimacy      = 0.50;
    row.avg_gdp             = 80.0;
    row.avg_corruption      = 0.20;
    row.total_gdp           = 240.0;
    row.event_history_count = 1u;

    const auto s = ann::write_csv_row(GameDate(1930, 12, 31), row);
    // First field is the date in ISO-8601, then comma, then the rest.
    CHECK(s.substr(0, 11) == "1930-12-31,");
    // Ends with a newline.
    CHECK(s.back() == '\n');
    // Contains scientific notation for the doubles.
    CHECK(s.find('e') != std::string::npos);
    // Contains the integer fields.
    CHECK(s.find(",3,")   != std::string::npos);  // country_count
    CHECK(s.find(",1\n")  != std::string::npos);  // event_history_count
}

// =====================================================================
// RCR-1 event_firer per-fire log emission (RFC-090 §5.9 — extra pin)
// =====================================================================

TEST_CASE("RCR-1 event_firer: record_match appends one LogEntry with event_fired category") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.10));
    state.events.push_back(make_event("low_stab",
        { EventTrigger{"country.stability", "lt", 0.30} }));

    const auto matches = ev_ev::match_events(state);
    REQUIRE(matches.size() == 1u);

    const auto before = state.logs.size();
    ev_fi::record_match(state, matches.front(), GameDate(1930, 3, 15));
    CHECK(state.logs.size() == before + 1);

    const auto& last = state.logs.back();
    CHECK(last.category == "event_fired");
    CHECK(last.date     == GameDate(1930, 3, 15));
    CHECK(last.message.find("low_stab") != std::string::npos);
    bool found_event_id_meta = false;
    for (const auto& kv : last.metadata) {
        if (kv.first == "event_id_code" && kv.second == "low_stab") {
            found_event_id_meta = true;
            break;
        }
    }
    CHECK(found_event_id_meta);
}

// =====================================================================
// RCR-1 event_evaluator::select_weighted_event (RFC-090 §5.7)
// =====================================================================

TEST_CASE("RCR-1 select_weighted_event: empty events returns success(nullopt)") {
    GameState state;
    const auto r = ev_ev::select_weighted_event(state);
    REQUIRE(r);
    CHECK_FALSE(r.value().has_value());
}

TEST_CASE("RCR-1 select_weighted_event: no triggers currently match -> success(nullopt)") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.80));
    state.events.push_back(make_event("low_stab",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "gt", 0.50, 5.0} }));
    const auto r = ev_ev::select_weighted_event(state);
    REQUIRE(r);
    CHECK_FALSE(r.value().has_value());
}

TEST_CASE("RCR-1 select_weighted_event: returns matched candidate with highest weight") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    // a: matches, base weight 1.0
    state.events.push_back(make_event("a",
        { EventTrigger{"country.stability", "lt", 0.30} }));
    // b: matches, +1.5 weight via modifier (total 2.5)
    state.events.push_back(make_event("b",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "lt", 0.30, 1.5} }));
    // c: does NOT currently match -- excluded from selection
    state.events.push_back(make_event("c",
        { EventTrigger{"country.stability", "gt", 0.90} },
        { WeightModifier{"country.stability", "lt", 0.30, 10.0} }));

    const auto r = ev_ev::select_weighted_event(state);
    REQUIRE(r);
    REQUIRE(r.value().has_value());
    const auto& pick = r.value().value();
    CHECK(pick.event_id_code == "b");
    CHECK(pick.weight        == doctest::Approx(2.5));
}

TEST_CASE("RCR-1 select_weighted_event: ties resolve by event vector order") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    state.events.push_back(make_event("first",
        { EventTrigger{"country.stability", "lt", 0.30} }));
    state.events.push_back(make_event("second",
        { EventTrigger{"country.stability", "lt", 0.30} }));

    const auto r = ev_ev::select_weighted_event(state);
    REQUIRE(r);
    REQUIRE(r.value().has_value());
    CHECK(r.value().value().event_id_code == "first");
}

// =====================================================================
// Post-M6.7 hardening: select_weighted_event must PROPAGATE malformed
// weight failures, NOT silently convert them to nullopt. Previously
// the signature returned `std::optional<...>` and ate the
// rank_weighted_events failure as "no pick"; that was the last silent
// degradation site in the event-evaluator surface.
// =====================================================================

TEST_CASE("Hardening: select_weighted_event REJECTS NaN modifier weight_delta") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    state.events.push_back(make_event("bad",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "lt", 0.30,
                         std::numeric_limits<double>::quiet_NaN()} }));

    const auto r = ev_ev::select_weighted_event(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("weight_delta is not finite") != std::string::npos);
    CHECK(r.error().find("bad") != std::string::npos);
}

TEST_CASE("Hardening: select_weighted_event REJECTS +Inf modifier weight_delta") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    state.events.push_back(make_event("bad",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "lt", 0.30,
                         std::numeric_limits<double>::infinity()} }));

    const auto r = ev_ev::select_weighted_event(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("weight_delta is not finite") != std::string::npos);
}

TEST_CASE("Hardening: select_weighted_event REJECTS unrecognised modifier target") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    state.events.push_back(make_event("bad",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.notarealfield", "lt", 0.30, 0.5} }));

    const auto r = ev_ev::select_weighted_event(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("not in the country/interest_group allowlist")
          != std::string::npos);
}

TEST_CASE("Hardening: select_weighted_event REJECTS unrecognised modifier op") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));
    state.events.push_back(make_event("bad",
        { EventTrigger{"country.stability", "lt", 0.30} },
        { WeightModifier{"country.stability", "eq" /* not allowed */,
                         0.30, 0.5} }));

    const auto r = ev_ev::select_weighted_event(state);
    REQUIRE(r.failed());
    CHECK(r.error().find("op = 'eq'") != std::string::npos);
}

// =====================================================================
// RCR-1 event_effects::apply_default_option_effects (RFC-090 §5.4 / §5.8)
// =====================================================================

TEST_CASE("RCR-1 apply_default_option_effects: empty options -> success, effects_applied=0, state unchanged") {
    GameState state;
    auto c = make_country("GER", /*stab*/0.50);
    c.id = CountryId{0};
    state.countries.push_back(c);

    leviathan::core::EventInstance inst;
    inst.event_id_code = "no_options";
    inst.fired_on      = GameDate(1930, 1, 1);
    leviathan::core::EventInstanceActor a;
    a.kind            = "country";
    a.id_code         = "GER";
    a.country_id_code = "GER";
    a.index           = 0;
    inst.actors.push_back(a);

    EventDefinition d = make_event("no_options",
        { EventTrigger{"country.stability", "lt", 0.30} });

    const auto r = ev_ef::apply_default_option_effects(state, inst, d);
    REQUIRE(r);
    CHECK(r.value().effects_applied == 0);
    CHECK(state.countries[0].stability == doctest::Approx(0.50));
}

TEST_CASE("RCR-1 apply_default_option_effects: first option's effects land on the actor's country") {
    GameState state;
    auto c = make_country("GER", /*stab*/0.50);
    c.id        = CountryId{0};
    c.legitimacy = 0.50;
    state.countries.push_back(c);

    leviathan::core::EventInstance inst;
    inst.event_id_code = "with_options";
    inst.fired_on      = GameDate(1930, 1, 1);
    leviathan::core::EventInstanceActor a;
    a.kind            = "country";
    a.id_code         = "GER";
    a.country_id_code = "GER";
    a.index           = 0;
    inst.actors.push_back(a);

    EventDefinition d = make_event("with_options",
        { EventTrigger{"country.stability", "lt", 0.30} },
        {},
        {
            EventOption{"opt_a", "Option A",
                {{"country.stability", "add", 0.05},
                 {"country.legitimacy", "add", 0.10}}},
            EventOption{"opt_b", "Option B",
                {{"country.stability", "add", -0.05}}},
        });

    const auto r = ev_ef::apply_default_option_effects(state, inst, d);
    REQUIRE(r);
    CHECK(r.value().effects_applied == 2);
    // opt_a applied (deterministic first-option selector). Mechanical
    // asymptotic-add:
    //   stability  0.50 + 0.05 * (1 - 0.50) = 0.525
    //   legitimacy 0.50 + 0.10 * (1 - 0.50) = 0.55
    CHECK(state.countries[0].stability  == doctest::Approx(0.525));
    CHECK(state.countries[0].legitimacy == doctest::Approx(0.55));
}

TEST_CASE("Hardening: apply_default_option_effects REJECTS empty actors") {
    // Post-M6.7 strict-fallback hardening (mirrors
    // apply_event_effects): a structurally inconsistent
    // EventInstance with no actors returns Result::failure.
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.50));

    leviathan::core::EventInstance inst;
    inst.event_id_code = "no_actor";
    // actors empty

    EventDefinition d = make_event("no_actor",
        { EventTrigger{"country.stability", "lt", 0.30} },
        {},
        { EventOption{"opt", "Opt",
            {{"country.stability", "add", 0.5}}} });

    const auto r = ev_ef::apply_default_option_effects(state, inst, d);
    REQUIRE(r.failed());
    CHECK(r.error().find("no actors") != std::string::npos);
    CHECK(state.countries[0].stability == doctest::Approx(0.50));
}

TEST_CASE("RCR-1 apply_default_option_effects: bogus effect target -> failure, atomic state") {
    GameState state;
    auto c = make_country("GER", /*stab*/0.50);
    c.id = CountryId{0};
    state.countries.push_back(c);

    leviathan::core::EventInstance inst;
    inst.event_id_code = "bad_opt";
    inst.fired_on      = GameDate(1930, 1, 1);
    leviathan::core::EventInstanceActor a;
    a.kind            = "country";
    a.id_code         = "GER";
    a.country_id_code = "GER";
    a.index           = 0;
    inst.actors.push_back(a);

    EventDefinition d = make_event("bad_opt",
        { EventTrigger{"country.stability", "lt", 0.30} },
        {},
        { EventOption{"opt", "Opt",
            {{"country.does_not_exist", "add", 0.1}}} });

    const auto r = ev_ef::apply_default_option_effects(state, inst, d);
    CHECK_FALSE(r);
    // M1.5 pre-flight atomicity inherited.
    CHECK(state.countries[0].stability == doctest::Approx(0.50));
}

// =====================================================================
// RCR-1 event_firer::record_followup (RFC-090 §5.12)
// =====================================================================

TEST_CASE("RCR-1 record_followup: appends EventInstance + LogEntry, actors inherited from parent") {
    GameState state;
    state.countries.push_back(make_country("GER", /*stab*/0.20));

    leviathan::core::EventInstance parent;
    parent.event_id_code = "parent_event";
    parent.fired_on      = GameDate(1930, 3, 15);
    leviathan::core::EventInstanceActor a;
    a.kind            = "country";
    a.id_code         = "GER";
    a.country_id_code = "GER";
    a.index           = 0;
    parent.actors.push_back(a);

    EventDefinition followup_def = make_event("followup_event",
        { EventTrigger{"country.stability", "lt", 0.30} });

    const auto history_before = state.event_history.size();
    const auto logs_before    = state.logs.size();
    ev_fi::record_followup(state, parent, followup_def, GameDate(1930, 4, 15));

    REQUIRE(state.event_history.size() == history_before + 1);
    const auto& inst = state.event_history.back();
    CHECK(inst.event_id_code == "followup_event");
    CHECK(inst.fired_on      == GameDate(1930, 4, 15));
    REQUIRE(inst.actors.size() == 1u);
    CHECK(inst.actors[0].country_id_code == "GER");

    REQUIRE(state.logs.size() == logs_before + 1);
    const auto& last = state.logs.back();
    CHECK(last.category == "event_fired");
    CHECK(last.message.find("followup of parent_event") != std::string::npos);
    bool found_followup_of_meta = false;
    for (const auto& kv : last.metadata) {
        if (kv.first == "followup_of" && kv.second == "parent_event") {
            found_followup_of_meta = true;
            break;
        }
    }
    CHECK(found_followup_of_meta);
}

// =====================================================================
// RCR-1 fixture round-trip: weight_modifiers / options /
// followup_event_ids survive scenario_loader -> save -> deserialize
// =====================================================================

TEST_CASE("RCR-1 fixture round-trip: non-empty weight_modifiers / options / followup_event_ids survive save round-trip") {
    // Build a state with a hand-built event carrying all three new
    // fields populated. Round-trip via save_system::serialize +
    // deserialize and verify every byte of the new schema survives.
    GameState state;
    EventDefinition d;
    d.id_code        = "rich_event";
    d.name           = "Rich Event";
    d.description    = "An event exercising every RCR-1 schema field.";
    d.visible_report = "public report";
    d.true_cause     = "private cause";
    d.category       = "political";   // issue #112: required non-empty
    d.triggers.push_back({"country.stability", "lt", 0.30});
    d.effects.push_back({"country.legitimacy", "add", -0.01});
    d.weight_modifiers.push_back({"country.stability", "lt", 0.20, 1.5});
    d.weight_modifiers.push_back({"country.legitimacy", "lt", 0.30, 0.5});
    d.options.push_back({"calm", "Calm response",
                         {{"country.legitimacy", "add", 0.02}}});
    d.options.push_back({"crackdown", "Crackdown",
                         {{"country.central_control", "add", 0.03},
                          {"country.legitimacy", "add", -0.02}}});
    d.option_effect_mode =
        leviathan::core::EventOptionEffectMode::OptionOnly;  // issue #112
    d.followup_event_ids = {"escalation", "diplomatic_fallout"};
    state.events.push_back(d);

    const std::string serialised =
        leviathan::systems::save_system::serialize(state);
    const auto re_r =
        leviathan::systems::save_system::deserialize(serialised);
    REQUIRE(re_r);
    const auto& re = re_r.value();
    REQUIRE(re.events.size() == 1u);
    const auto& e = re.events.front();

    REQUIRE(e.weight_modifiers.size() == 2u);
    CHECK(e.weight_modifiers[0].target       == "country.stability");
    CHECK(e.weight_modifiers[0].op           == "lt");
    CHECK(e.weight_modifiers[0].value        == doctest::Approx(0.20));
    CHECK(e.weight_modifiers[0].weight_delta == doctest::Approx(1.5));
    CHECK(e.weight_modifiers[1].target       == "country.legitimacy");

    REQUIRE(e.options.size() == 2u);
    CHECK(e.options[0].id_code == "calm");
    CHECK(e.options[0].label   == "Calm response");
    REQUIRE(e.options[0].effects.size() == 1u);
    CHECK(e.options[0].effects[0].target == "country.legitimacy");
    CHECK(e.options[1].id_code == "crackdown");
    REQUIRE(e.options[1].effects.size() == 2u);

    REQUIRE(e.followup_event_ids.size() == 2u);
    CHECK(e.followup_event_ids[0] == "escalation");
    CHECK(e.followup_event_ids[1] == "diplomatic_fallout");
}

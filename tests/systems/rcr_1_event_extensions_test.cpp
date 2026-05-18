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
    const auto out = ev_ev::rank_weighted_events(state);
    CHECK(out.empty());
}

TEST_CASE("RCR-1 rank_weighted_events: every event ends at kBaseWeight when no modifiers") {
    GameState state;
    state.events.push_back(make_event("a",
        { EventTrigger{"country.stability", "lt", 0.30} }));
    state.events.push_back(make_event("b",
        { EventTrigger{"country.stability", "gt", 0.70} }));

    const auto out = ev_ev::rank_weighted_events(state);
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

    const auto out = ev_ev::rank_weighted_events(state);
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

    const auto out = ev_ev::rank_weighted_events(state);
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

    const auto out = ev_ev::rank_weighted_events(state);
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
    REQUIRE(r1.size() == r2.size());
    for (std::size_t i = 0; i < r1.size(); ++i) {
        CHECK(r1[i].event_index   == r2[i].event_index);
        CHECK(r1[i].event_id_code == r2[i].event_id_code);
        CHECK(r1[i].weight        == r2[i].weight);
    }
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

TEST_CASE("RCR-1 resolve_followup_ids: empty followup list returns empty vector") {
    GameState state;
    state.events.push_back(make_event("a",
        { EventTrigger{"country.stability", "lt", 0.30} }));
    EventDefinition d = make_event("trig",
        { EventTrigger{"country.stability", "lt", 0.30} });
    const auto out = ev_ef::resolve_followup_ids(state, d);
    CHECK(out.empty());
}

TEST_CASE("RCR-1 resolve_followup_ids: resolves matching ids to state.events indices") {
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

    const auto out = ev_ef::resolve_followup_ids(state, d);
    REQUIRE(out.size() == 2u);
    CHECK(out[0] == 2u);  // gamma
    CHECK(out[1] == 0u);  // alpha
}

TEST_CASE("RCR-1 resolve_followup_ids: skips unresolvable ids without failing") {
    GameState state;
    state.events.push_back(make_event("alpha",
        { EventTrigger{"country.stability", "lt", 0.30} }));
    EventDefinition d = make_event("origin",
        { EventTrigger{"country.stability", "lt", 0.30} },
        {}, {}, {"ghost", "alpha"});

    const auto out = ev_ef::resolve_followup_ids(state, d);
    REQUIRE(out.size() == 1u);
    CHECK(out[0] == 0u);
}

// =====================================================================
// RCR-1 annual_stats (RFC-090 §3.9 / RFC-010 §5)
// =====================================================================

TEST_CASE("RCR-1 annual_stats::snapshot: empty state produces zeroed averages and zero counts") {
    GameState state;
    const auto row = ann::snapshot(state, 1930);
    CHECK(row.year                == 1930);
    CHECK(row.country_count       == 0u);
    CHECK(row.avg_stability       == doctest::Approx(0.0));
    CHECK(row.avg_legitimacy      == doctest::Approx(0.0));
    CHECK(row.avg_gdp             == doctest::Approx(0.0));
    CHECK(row.avg_corruption      == doctest::Approx(0.0));
    CHECK(row.total_gdp           == doctest::Approx(0.0));
    CHECK(row.event_history_count == 0u);
}

TEST_CASE("RCR-1 annual_stats::snapshot: averages and total over populated state") {
    GameState state;
    auto a = make_country("A"); a.stability = 0.4; a.legitimacy = 0.5; a.gdp = 100.0; a.corruption = 0.2;
    auto b = make_country("B"); b.stability = 0.6; b.legitimacy = 0.7; b.gdp = 200.0; b.corruption = 0.4;
    state.countries.push_back(a);
    state.countries.push_back(b);

    const auto row = ann::snapshot(state, 1940);
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

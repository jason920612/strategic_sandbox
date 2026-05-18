// Issue #110 event-engine wiring tests.
//
// These tests prove `event_engine::tick_events` actually uses the
// helpers that were tests-only on main:
//   - rank_weighted_events for descending-weight fire order
//   - apply_default_option_effects for events with options
//   - resolve_followup_ids + record_followup for depth-1 chains
// Each case drives `tick_events` directly (no monthly pipeline) so
// the engine surface is the single thing under test.

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/event_engine.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventOption;
using leviathan::core::EventTrigger;
using leviathan::core::GameState;
using leviathan::core::PolicyEffect;
using leviathan::core::WeightModifier;
namespace eng = leviathan::systems::event_engine;

namespace {

CountryState make_country(int id, std::string code,
                          double stability  = 0.50,
                          double legitimacy = 0.50) {
    CountryState c;
    c.id         = CountryId{id};
    c.id_code    = std::move(code);
    c.name       = c.id_code;
    c.stability  = stability;
    c.legitimacy = legitimacy;
    return c;
}

EventTrigger trig(std::string target, std::string op, double value) {
    return EventTrigger{std::move(target), std::move(op), value};
}

PolicyEffect peff(std::string target, std::string op, double value) {
    return PolicyEffect{std::move(target), std::move(op), value};
}

WeightModifier wmod(std::string target, std::string op,
                    double value, double delta) {
    WeightModifier m;
    m.target       = std::move(target);
    m.op           = std::move(op);
    m.value        = value;
    m.weight_delta = delta;
    return m;
}

EventDefinition make_event(std::string id_code,
                           std::vector<EventTrigger>   triggers,
                           std::vector<PolicyEffect>   effects = {},
                           std::vector<WeightModifier> weights = {},
                           std::vector<EventOption>    options = {},
                           std::vector<std::string>    followups = {}) {
    EventDefinition d;
    d.id_code           = std::move(id_code);
    d.name              = d.id_code;
    d.visible_report    = "test report";
    d.true_cause        = "test true cause";
    d.triggers          = std::move(triggers);
    d.effects           = std::move(effects);
    d.weight_modifiers  = std::move(weights);
    d.options           = std::move(options);
    d.followup_event_ids = std::move(followups);
    return d;
}

}  // namespace

// =====================================================================
// Issue #110 §4: weight-desc firing order
// =====================================================================

TEST_CASE("Issue #110: tick_events fires matched events in descending-weight order") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10, /*leg*/0.10));

    // Two events that BOTH match the same country state. Event A
    // is in index 0 with a *light* weight modifier (delta +0.10);
    // event B is in index 1 with a *heavy* weight modifier
    // (delta +0.50). B must fire FIRST (insertion order in
    // event_history reflects fire order).
    s.events.push_back(make_event(
        "event_a",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.01) },
        { wmod("country.stability", "lt", 0.50, 0.10) }));
    s.events.push_back(make_event(
        "event_b",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.01) },
        { wmod("country.stability", "lt", 0.50, 0.50) }));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    REQUIRE(s.event_history.size() == 2u);
    CHECK(s.event_history[0].event_id_code == "event_b");
    CHECK(s.event_history[1].event_id_code == "event_a");
    CHECK(r.value().events_recorded == 2);
    CHECK(r.value().events_applied  == 2);
}

TEST_CASE("Issue #110: tick_events ties on weight resolve via stable event vector index") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10));

    // Two events both at base weight (no modifier) → tie. Stable
    // tie-break puts the lower index first.
    s.events.push_back(make_event(
        "first_in_order",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.005) }));
    s.events.push_back(make_event(
        "second_in_order",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.005) }));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    REQUIRE(s.event_history.size() == 2u);
    CHECK(s.event_history[0].event_id_code == "first_in_order");
    CHECK(s.event_history[1].event_id_code == "second_in_order");
}

// =====================================================================
// Issue #110 §5: option-default effects applied in tick flow
// =====================================================================

TEST_CASE("Issue #110: tick_events fires options[0].effects for events with options") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50, /*leg*/0.10));

    EventOption opt_default;
    opt_default.id_code = "concede";
    opt_default.label   = "Concede";
    opt_default.effects = { peff("country.legitimacy", "add", 0.05) };
    EventOption opt_alt;
    opt_alt.id_code = "crackdown";
    opt_alt.label   = "Crackdown";
    opt_alt.effects = { peff("country.legitimacy", "add", -0.05) };

    s.events.push_back(make_event(
        "legit_crisis",
        { trig("country.legitimacy", "lt", 0.20) },
        // Base effects deliberately distinct from option effects so
        // we can tell which path ran.
        { peff("country.legitimacy", "add", -0.99) },
        /*weights=*/{},
        /*options=*/{opt_default, opt_alt}));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    REQUIRE(s.event_history.size() == 1u);
    // The DEFAULT option ran (opt[0]: +0.05), not the base effects
    // (-0.99 which would land at clamp-zero). 0.10 + 0.05 = 0.15.
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.15));
}

TEST_CASE("Issue #110: tick_events falls through to base effects when options is empty") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10));
    s.events.push_back(make_event(
        "no_options",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.04) }));  // base effects only

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    REQUIRE(s.event_history.size() == 1u);
    CHECK(s.countries[0].stability == doctest::Approx(0.06));  // 0.10 - 0.04
    CHECK(r.value().events_with_options == 0);
}

// =====================================================================
// Issue #110 §6: depth-1 followup chain executes in tick flow
// =====================================================================

TEST_CASE("Issue #110: tick_events records and applies a depth-1 followup chain") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10, /*leg*/0.50));

    // Followup event: not directly triggered, only reached via
    // parent's `followup_event_ids`. Its triggers do not need to
    // hold — depth-1 cascade is independent of trigger matching.
    s.events.push_back(make_event(
        "followup_event",
        { trig("country.stability", "lt", 0.05) },   // does NOT match
        { peff("country.legitimacy", "add", 0.03) }));

    // Parent event matches and lists the followup.
    s.events.push_back(make_event(
        "parent_event",
        { trig("country.stability", "lt", 0.20) },   // matches
        { peff("country.stability", "add", -0.01) },
        /*weights=*/{},
        /*options=*/{},
        /*followups=*/{"followup_event"}));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    // Two entries in event_history: parent first (record_match), then
    // followup (record_followup).
    REQUIRE(s.event_history.size() == 2u);
    CHECK(s.event_history[0].event_id_code == "parent_event");
    CHECK(s.event_history[1].event_id_code == "followup_event");
    // Followup's effects (legitimacy +0.03) applied even though its
    // own trigger does not match → 0.50 + 0.03 = 0.53.
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.53));
    CHECK(r.value().events_with_followups          == 1);
    CHECK(r.value().followups_recorded             == 1);
    CHECK(r.value().total_followup_effects_applied == 1);
}

TEST_CASE("Issue #110: tick_events DOES NOT recurse beyond depth-1") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10));

    // Chain: parent -> child -> grandchild. Only `parent` matches
    // its trigger; child + grandchild are reached purely via
    // followup_event_ids. The depth-1 cap means child is recorded
    // and applied, but grandchild MUST NOT appear.
    s.events.push_back(make_event(
        "grandchild_event",
        { trig("country.stability", "lt", 0.05) },
        { peff("country.legitimacy", "add", 0.99) }));   // would be obvious

    s.events.push_back(make_event(
        "child_event",
        { trig("country.stability", "lt", 0.05) },
        { peff("country.legitimacy", "add", 0.02) },
        /*weights=*/{},
        /*options=*/{},
        /*followups=*/{"grandchild_event"}));

    s.events.push_back(make_event(
        "parent_event",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.01) },
        /*weights=*/{},
        /*options=*/{},
        /*followups=*/{"child_event"}));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    REQUIRE(s.event_history.size() == 2u);
    CHECK(s.event_history[0].event_id_code == "parent_event");
    CHECK(s.event_history[1].event_id_code == "child_event");
    // grandchild's giant +0.99 legitimacy effect must NOT have
    // applied. Legitimacy ends at 0.50 + 0.02 (child) = 0.52.
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.52));
    CHECK(r.value().followups_recorded == 1);   // ONLY the child
}

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/event_evaluator.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventTrigger;
using leviathan::core::GameState;
using leviathan::core::InterestGroupKind;
using leviathan::core::InterestGroupState;
using leviathan::core::PolicyEffect;
namespace ee = leviathan::systems::event_evaluator;

// =====================================================================
// helpers
// =====================================================================

namespace {

CountryState make_country(int id,
                          const std::string& code,
                          double stability,
                          double legitimacy = 0.5,
                          double compliance = 0.5) {
    CountryState c;
    c.id         = CountryId{id};
    c.id_code    = code;
    c.name       = code;
    c.stability  = stability;
    c.legitimacy = legitimacy;
    c.government_authority.bureaucratic_compliance = compliance;
    return c;
}

InterestGroupState make_ig(const std::string& code,
                           int                country_id,
                           double             radicalism,
                           double             loyalty = 0.5) {
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

EventDefinition make_event(const std::string&             id_code,
                           std::vector<EventTrigger>      triggers,
                           std::vector<PolicyEffect>      effects = {}) {
    EventDefinition d;
    d.id_code        = id_code;
    d.name           = id_code;
    d.visible_report = "test visible report";   // M6.2: required non-empty
    d.true_cause     = "test true cause";       // M6.1: required non-empty
    d.triggers       = std::move(triggers);
    d.effects  = std::move(effects);
    return d;
}

}  // namespace

// =====================================================================
// trigger_matches: per-op semantics on country.stability
// =====================================================================

TEST_CASE("M5.2 trigger_matches: country.stability lt matches when any country < value") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.55));
    s.countries.push_back(make_country(2, "FRA", 0.20));
    CHECK(ee::trigger_matches(s, make_trigger("country.stability", "lt", 0.30)));
}

TEST_CASE("M5.2 trigger_matches: country.stability lt is FALSE when every country >= value") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.55));
    s.countries.push_back(make_country(2, "FRA", 0.60));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "lt", 0.30)));
}

TEST_CASE("M5.2 trigger_matches: country.stability lte includes equality") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.30));
    CHECK(ee::trigger_matches(s, make_trigger("country.stability", "lte", 0.30)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "lt",  0.30)));
}

TEST_CASE("M5.2 trigger_matches: country.stability gt + gte semantics") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.80));
    CHECK(ee::trigger_matches(s, make_trigger("country.stability", "gt",  0.75)));
    CHECK(ee::trigger_matches(s, make_trigger("country.stability", "gte", 0.80)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "gt", 0.80)));
}

// =====================================================================
// trigger_matches: every country.* target leaf
// =====================================================================

TEST_CASE("M5.2 trigger_matches: country.legitimacy") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", /*stab*/0.50, /*legit*/0.40));
    CHECK(ee::trigger_matches(s, make_trigger("country.legitimacy", "lt", 0.50)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.legitimacy", "gt", 0.50)));
}

TEST_CASE("M5.2 trigger_matches: country.government_authority.bureaucratic_compliance") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.50, 0.50, /*compliance*/0.20));
    CHECK(ee::trigger_matches(
        s,
        make_trigger("country.government_authority.bureaucratic_compliance",
                     "lt", 0.30)));
    CHECK_FALSE(ee::trigger_matches(
        s,
        make_trigger("country.government_authority.bureaucratic_compliance",
                     "gt", 0.30)));
}

// =====================================================================
// trigger_matches: interest_group.* targets, any-IG semantics
// =====================================================================

TEST_CASE("M5.2 trigger_matches: interest_group.radicalism gt matches when any IG > value") {
    GameState s;
    s.interest_groups.push_back(make_ig("ger_bureau", 1, /*rad*/0.10));
    s.interest_groups.push_back(make_ig("fra_bureau", 2, /*rad*/0.80));
    CHECK(ee::trigger_matches(s, make_trigger("interest_group.radicalism", "gt", 0.75)));
}

TEST_CASE("M5.2 trigger_matches: interest_group.radicalism gt FALSE when no IG exceeds") {
    GameState s;
    s.interest_groups.push_back(make_ig("ger_bureau", 1, 0.10));
    s.interest_groups.push_back(make_ig("fra_bureau", 2, 0.20));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("interest_group.radicalism", "gt", 0.75)));
}

TEST_CASE("M5.2 trigger_matches: interest_group.loyalty") {
    GameState s;
    s.interest_groups.push_back(make_ig("ger_bureau", 1, 0.10, /*loyalty*/0.20));
    CHECK(ee::trigger_matches(s, make_trigger("interest_group.loyalty", "lt", 0.30)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("interest_group.loyalty", "gt", 0.30)));
}

// =====================================================================
// trigger_matches: empty entity list = FALSE (existential over empty)
// =====================================================================

TEST_CASE("M5.2 trigger_matches: country.* on empty countries returns FALSE") {
    GameState s;
    // No countries.
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "lt", 1.0)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.legitimacy", "lte", 1.0)));
    CHECK_FALSE(ee::trigger_matches(
        s,
        make_trigger("country.government_authority.bureaucratic_compliance",
                     "gte", 0.0)));
}

TEST_CASE("M5.2 trigger_matches: interest_group.* on empty IGs returns FALSE") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.5));
    // No interest_groups.
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("interest_group.radicalism", "gt", 0.0)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("interest_group.loyalty",    "lt", 1.0)));
}

// =====================================================================
// trigger_matches: unknown target / op / non-finite value = FALSE
// =====================================================================

TEST_CASE("M5.2 trigger_matches: unknown target returns FALSE (defensive — loader is the gate)") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.55));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.gdp", "lt", 1000.0)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.budget.military", "gt", 0.0)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("not_a_target", "lt", 0.0)));
}

TEST_CASE("M5.2 trigger_matches: unknown op returns FALSE") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.55));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "eq", 0.55)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "ne", 0.55)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "",   0.55)));
}

TEST_CASE("M5.2 trigger_matches: non-finite trigger value returns FALSE") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.55));
    EventTrigger nan_t = make_trigger("country.stability", "lt", std::nan(""));
    EventTrigger inf_t = make_trigger("country.stability", "lt",
                                      std::numeric_limits<double>::infinity());
    CHECK_FALSE(ee::trigger_matches(s, nan_t));
    CHECK_FALSE(ee::trigger_matches(s, inf_t));
}

TEST_CASE("M5.2 trigger_matches: non-finite state value never matches") {
    GameState s;
    CountryState c = make_country(1, "GER", std::nan(""));
    s.countries.push_back(c);
    // Even though NaN < 0.30 is false in IEEE-754, the evaluator
    // pins the explicit semantics: NaN never matches any comparison.
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "lt",  1.0)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "gt",  0.0)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "lte", 1.0)));
    CHECK_FALSE(ee::trigger_matches(s, make_trigger("country.stability", "gte", 0.0)));
}

// =====================================================================
// evaluate: AND across def.triggers
// =====================================================================

TEST_CASE("M5.2 evaluate: AND across triggers — all must match") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20, /*legit*/0.30));
    EventDefinition def = make_event("compound", {
        make_trigger("country.stability",  "lt", 0.30),
        make_trigger("country.legitimacy", "lt", 0.40),
    });
    CHECK(ee::evaluate(s, def));
}

TEST_CASE("M5.2 evaluate: AND fails when any trigger fails") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20, /*legit*/0.80));
    EventDefinition def = make_event("compound", {
        make_trigger("country.stability",  "lt", 0.30),  // pass
        make_trigger("country.legitimacy", "lt", 0.40),  // fail
    });
    CHECK_FALSE(ee::evaluate(s, def));
}

TEST_CASE("M5.2 evaluate: AND across country.* and interest_group.* triggers") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20));
    s.interest_groups.push_back(make_ig("ger_bureau", 1, 0.80));
    EventDefinition def = make_event("cross_scope", {
        make_trigger("country.stability",         "lt", 0.30),
        make_trigger("interest_group.radicalism", "gt", 0.75),
    });
    CHECK(ee::evaluate(s, def));
}

TEST_CASE("M5.2 evaluate: empty triggers is vacuously TRUE (loader prevents this)") {
    // M5.1 loader rejects empty triggers; this test pins the
    // hand-built / synthesized-def semantics so a defensive
    // reader knows the contract.
    GameState s;
    EventDefinition def = make_event("empty", {});
    CHECK(ee::evaluate(s, def));
}

// =====================================================================
// match_events: canonical ordering + zero-matches + multi-matches
// =====================================================================

TEST_CASE("M5.2 match_events: returns nothing when state.events is empty") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20));
    const auto matches = ee::match_events(s);
    CHECK(matches.empty());
}

TEST_CASE("M5.2 match_events: returns nothing when no event triggers match") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.55));
    s.events.push_back(make_event("unrest", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    const auto matches = ee::match_events(s);
    CHECK(matches.empty());
}

TEST_CASE("M5.2 match_events: returns single match with index + id_code") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20));
    s.events.push_back(make_event("unrest", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    CHECK(matches[0].event_index   == 0u);
    CHECK(matches[0].event_id_code == "unrest");
}

TEST_CASE("M5.2 match_events: preserves canonical (vector) order") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20, /*legit*/0.30));
    s.interest_groups.push_back(make_ig("ger_bureau", 1, /*rad*/0.85));

    // Three events: 0 matches, 1 matches, 2 matches. Result must
    // be [1, 2] in order — even though 0 doesn't match, 1 and 2's
    // relative order is preserved.
    s.events.push_back(make_event("no_match", {
        make_trigger("country.stability", "gt", 0.90),
    }));
    s.events.push_back(make_event("low_stab", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    s.events.push_back(make_event("radical_ig", {
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 2u);
    CHECK(matches[0].event_index   == 1u);
    CHECK(matches[0].event_id_code == "low_stab");
    CHECK(matches[1].event_index   == 2u);
    CHECK(matches[1].event_id_code == "radical_ig");
}

// =====================================================================
// no-mutate regression: evaluator never touches state
// =====================================================================

TEST_CASE("M5.2 match_events: never mutates GameState") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20, /*legit*/0.30));
    s.interest_groups.push_back(make_ig("ger_bureau", 1, 0.85));
    s.events.push_back(make_event("low_stab", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    s.events.push_back(make_event("radical_ig", {
        make_trigger("interest_group.radicalism", "gt", 0.75),
        make_trigger("interest_group.loyalty",    "gte", 0.0),
    }));

    // Snapshot the mutation-visible fields.
    const auto before_country_stability = s.countries[0].stability;
    const auto before_country_legitimacy = s.countries[0].legitimacy;
    const auto before_ig_radicalism = s.interest_groups[0].radicalism;
    const auto before_ig_loyalty    = s.interest_groups[0].loyalty;
    const auto before_events_size   = s.events.size();
    const auto before_event0_id     = s.events[0].id_code;
    const auto before_event1_id     = s.events[1].id_code;
    const auto before_applied_cmds  = s.applied_commands.size();

    (void)ee::match_events(s);
    (void)ee::evaluate(s, s.events[0]);
    (void)ee::trigger_matches(s, s.events[0].triggers[0]);

    CHECK(s.countries[0].stability  == before_country_stability);
    CHECK(s.countries[0].legitimacy == before_country_legitimacy);
    CHECK(s.interest_groups[0].radicalism == before_ig_radicalism);
    CHECK(s.interest_groups[0].loyalty    == before_ig_loyalty);
    CHECK(s.events.size()        == before_events_size);
    CHECK(s.events[0].id_code    == before_event0_id);
    CHECK(s.events[1].id_code    == before_event1_id);
    CHECK(s.applied_commands.size() == before_applied_cmds);
}

// =====================================================================
// canonical event semantics: at canonical scenario boot, nothing fires
// =====================================================================

TEST_CASE("M5.2 match_events: canonical-shape events do NOT match canonical-shape state") {
    // Pin the M5.1 "fixture values chosen so neither event fires
    // on the canonical scenario" property at the evaluator level.
    // (M5.1's runner regression test pinned the same property via
    // the runner; this pins it at the evaluator level so a future
    // refactor of the evaluator can't silently break it.)
    GameState s;
    // Canonical GER stability is 0.55, > 0.30 threshold.
    s.countries.push_back(make_country(1, "GER", 0.55, /*legit*/0.55));
    // Canonical interest-group radicalism is 0.10, < 0.75 threshold.
    s.interest_groups.push_back(make_ig("ger_bureaucracy", 1, /*rad*/0.10));

    s.events.push_back(make_event("low_stability_unrest", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    s.events.push_back(make_event("radical_interest_group_warning", {
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));

    CHECK(ee::match_events(s).empty());
}

TEST_CASE("M5.2 match_events: canonical events DO match when state drifts past thresholds") {
    GameState s;
    // Now GER stability falls below 0.30 and the IG radicalises.
    s.countries.push_back(make_country(1, "GER", 0.25));
    s.interest_groups.push_back(make_ig("ger_bureaucracy", 1, 0.80));

    s.events.push_back(make_event("low_stability_unrest", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    s.events.push_back(make_event("radical_interest_group_warning", {
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 2u);
    CHECK(matches[0].event_id_code == "low_stability_unrest");
    CHECK(matches[1].event_id_code == "radical_interest_group_warning");
}

// =====================================================================
// M5.2 deliberate non-goals: evaluator does NOT emit logs / events /
// applied_commands; does NOT touch effects (effects vector contents
// are ignored at evaluation time).
// =====================================================================

TEST_CASE("M5.2 evaluate: effects vector contents are NOT consulted") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20));

    PolicyEffect bogus;
    bogus.target = "country.this_does_not_exist";
    bogus.op     = "this_is_not_an_op_either";
    bogus.value  = std::nan("");

    EventDefinition def = make_event("only_trigger_matters", {
        make_trigger("country.stability", "lt", 0.30),
    }, {bogus});

    CHECK(ee::evaluate(s, def));
}

TEST_CASE("M5.2 match_events: never appends to state.logs or state.applied_commands") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20));
    s.events.push_back(make_event("unrest", {
        make_trigger("country.stability", "lt", 0.30),
    }, {{"country.stability", "add", -0.02}}));

    const auto logs_before = s.logs.size();
    const auto cmds_before = s.applied_commands.size();

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);

    CHECK(s.logs.size()             == logs_before);
    CHECK(s.applied_commands.size() == cmds_before);
}

// =====================================================================
// M5.3 - actor binding: which entity satisfied each trigger
// =====================================================================

using leviathan::systems::event_evaluator::EventMatch;
using leviathan::systems::event_evaluator::TriggerActor;
using leviathan::systems::event_evaluator::TriggerActorKind;
using leviathan::systems::event_evaluator::TriggerEvaluation;

// ---- trigger_actor: per-target binding ------------------------------

TEST_CASE("M5.3 trigger_actor: country.* returns the first satisfying country") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.55));
    s.countries.push_back(make_country(2, "FRA", 0.20));
    s.countries.push_back(make_country(3, "JPN", 0.10));
    const auto a = ee::trigger_actor(
        s, make_trigger("country.stability", "lt", 0.30));
    REQUIRE(a.has_value());
    CHECK(a->kind    == TriggerActorKind::Country);
    CHECK(a->id_code == "FRA");
    CHECK(a->country == CountryId{2});
    CHECK(a->index   == 1u);
}

TEST_CASE("M5.3 trigger_actor: interest_group.* returns the first satisfying IG") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.55));
    s.interest_groups.push_back(make_ig("ger_bureau", 1, /*rad*/0.10));
    s.interest_groups.push_back(make_ig("ger_workers", 1, /*rad*/0.85));
    s.interest_groups.push_back(make_ig("ger_military", 1, /*rad*/0.90));
    const auto a = ee::trigger_actor(
        s, make_trigger("interest_group.radicalism", "gt", 0.75));
    REQUIRE(a.has_value());
    CHECK(a->kind    == TriggerActorKind::InterestGroup);
    CHECK(a->id_code == "ger_workers");
    CHECK(a->country == CountryId{1});
    CHECK(a->index   == 1u);
}

TEST_CASE("M5.3 trigger_actor: returns nullopt when no entity satisfies") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.90));
    s.interest_groups.push_back(make_ig("ger_bureau", 1, 0.10));
    CHECK_FALSE(
        ee::trigger_actor(s, make_trigger("country.stability", "lt", 0.30))
            .has_value());
    CHECK_FALSE(
        ee::trigger_actor(s, make_trigger("interest_group.radicalism", "gt", 0.75))
            .has_value());
}

TEST_CASE("M5.3 trigger_actor: returns nullopt on unknown target / op / non-finite") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20));
    CHECK_FALSE(ee::trigger_actor(s,
        make_trigger("country.gdp", "lt", 1000.0)).has_value());
    CHECK_FALSE(ee::trigger_actor(s,
        make_trigger("country.stability", "eq", 0.20)).has_value());
    CHECK_FALSE(ee::trigger_actor(s,
        make_trigger("country.stability", "lt", std::nan(""))).has_value());
}

TEST_CASE("M5.3 trigger_actor: returns nullopt on empty entity list") {
    GameState s;
    // No countries, no IGs.
    CHECK_FALSE(ee::trigger_actor(s,
        make_trigger("country.stability", "lt", 1.0)).has_value());
    CHECK_FALSE(ee::trigger_actor(s,
        make_trigger("interest_group.radicalism", "gt", 0.0)).has_value());
}

TEST_CASE("M5.3 trigger_actor: country.id_code mismatch tolerated — index is the canonical handle") {
    // Two countries with the same id_code (would be rejected at
    // load by the M5.1 duplicate check, but pin the contract:
    // the evaluator picks by predicate, not by id_code).
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.50));
    s.countries.push_back(make_country(2, "GER", 0.10));   // same code
    const auto a = ee::trigger_actor(
        s, make_trigger("country.stability", "lt", 0.30));
    REQUIRE(a.has_value());
    CHECK(a->id_code == "GER");
    CHECK(a->index   == 1u);    // second country, the satisfying one
    CHECK(a->country == CountryId{2});
}

// ---- evaluate_match: full per-trigger binding for one event ---------

TEST_CASE("M5.3 evaluate_match: nullopt when any trigger fails") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20, /*legit*/0.80));
    EventDefinition def = make_event("compound", {
        make_trigger("country.stability",  "lt", 0.30),   // pass on GER
        make_trigger("country.legitimacy", "lt", 0.40),   // fail on GER
    });
    CHECK_FALSE(ee::evaluate_match(s, def).has_value());
}

TEST_CASE("M5.3 evaluate_match: populated when ALL triggers match") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20, /*legit*/0.30));
    EventDefinition def = make_event("compound", {
        make_trigger("country.stability",  "lt", 0.30),
        make_trigger("country.legitimacy", "lt", 0.40),
    });
    const auto m = ee::evaluate_match(s, def);
    REQUIRE(m.has_value());
    CHECK(m->event_id_code == "compound");
    REQUIRE(m->triggers.size() == 2u);
    CHECK(m->triggers[0].trigger_index == 0u);
    CHECK(m->triggers[0].actor.kind    == TriggerActorKind::Country);
    CHECK(m->triggers[0].actor.id_code == "GER");
    CHECK(m->triggers[1].trigger_index == 1u);
    CHECK(m->triggers[1].actor.kind    == TriggerActorKind::Country);
    CHECK(m->triggers[1].actor.id_code == "GER");
}

TEST_CASE("M5.3 evaluate_match: cross-scope binding — country trigger + IG trigger pick different actors") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20));
    s.interest_groups.push_back(make_ig("ger_bureau", 1, /*rad*/0.10));
    s.interest_groups.push_back(make_ig("fra_workers", 2, /*rad*/0.85));
    EventDefinition def = make_event("cross_scope", {
        make_trigger("country.stability",         "lt", 0.30),
        make_trigger("interest_group.radicalism", "gt", 0.75),
    });
    const auto m = ee::evaluate_match(s, def);
    REQUIRE(m.has_value());
    REQUIRE(m->triggers.size() == 2u);
    CHECK(m->triggers[0].actor.kind    == TriggerActorKind::Country);
    CHECK(m->triggers[0].actor.id_code == "GER");
    CHECK(m->triggers[1].actor.kind    == TriggerActorKind::InterestGroup);
    CHECK(m->triggers[1].actor.id_code == "fra_workers");
    CHECK(m->triggers[1].actor.country == CountryId{2});
}

TEST_CASE("M5.3 evaluate_match: empty triggers vector returns an EventMatch with empty actors") {
    // Vacuously true (mirrors evaluate semantics). M5.1 loader
    // rejects empty triggers; this pins the API contract for
    // hand-built defs.
    GameState s;
    EventDefinition def = make_event("empty", {});
    const auto m = ee::evaluate_match(s, def);
    REQUIRE(m.has_value());
    CHECK(m->event_id_code == "empty");
    CHECK(m->triggers.empty());
}

TEST_CASE("M5.3 evaluate_match: triggers vector mirrors def order even when several triggers share an actor") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20, /*legit*/0.30, /*compliance*/0.10));
    EventDefinition def = make_event("triple_on_GER", {
        make_trigger("country.stability",  "lt", 0.30),
        make_trigger("country.legitimacy", "lt", 0.40),
        make_trigger("country.government_authority.bureaucratic_compliance",
                     "lt", 0.20),
    });
    const auto m = ee::evaluate_match(s, def);
    REQUIRE(m.has_value());
    REQUIRE(m->triggers.size() == 3u);
    CHECK(m->triggers[0].trigger_index == 0u);
    CHECK(m->triggers[1].trigger_index == 1u);
    CHECK(m->triggers[2].trigger_index == 2u);
    for (const auto& te : m->triggers) {
        CHECK(te.actor.id_code == "GER");
        CHECK(te.actor.index   == 0u);
        CHECK(te.actor.country == CountryId{1});
    }
}

// ---- match_events: EventMatch carries the per-trigger binding -------

TEST_CASE("M5.3 match_events: each EventMatch carries triggers vector with actor binding") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20));
    s.interest_groups.push_back(make_ig("ger_bureau", 1, 0.85));

    s.events.push_back(make_event("low_stab", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    s.events.push_back(make_event("radical_ig", {
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 2u);

    CHECK(matches[0].event_index   == 0u);
    CHECK(matches[0].event_id_code == "low_stab");
    REQUIRE(matches[0].triggers.size() == 1u);
    CHECK(matches[0].triggers[0].trigger_index == 0u);
    CHECK(matches[0].triggers[0].actor.kind    == TriggerActorKind::Country);
    CHECK(matches[0].triggers[0].actor.id_code == "GER");

    CHECK(matches[1].event_index   == 1u);
    CHECK(matches[1].event_id_code == "radical_ig");
    REQUIRE(matches[1].triggers.size() == 1u);
    CHECK(matches[1].triggers[0].trigger_index == 0u);
    CHECK(matches[1].triggers[0].actor.kind    == TriggerActorKind::InterestGroup);
    CHECK(matches[1].triggers[0].actor.id_code == "ger_bureau");
}

TEST_CASE("M5.3 match_events: canonical-shape state -> zero matches still (M5.3 doesn't change the gate)") {
    // M5.3 only enriches the return shape; whether a trigger
    // matches is unchanged from M5.2. The canonical-shape
    // no-fire regression must still hold.
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.55));
    s.interest_groups.push_back(make_ig("ger_bureaucracy", 1, 0.10));
    s.events.push_back(make_event("low_stability_unrest", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    s.events.push_back(make_event("radical_interest_group_warning", {
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));
    CHECK(ee::match_events(s).empty());
}

// ---- no-mutate regression: actor binding does not touch state ------

TEST_CASE("M5.3 evaluate_match + match_events: never mutate GameState") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20, /*legit*/0.30));
    s.interest_groups.push_back(make_ig("ger_bureau", 1, 0.85));
    s.events.push_back(make_event("compound", {
        make_trigger("country.stability",         "lt", 0.30),
        make_trigger("interest_group.radicalism", "gt", 0.75),
    }));

    const auto c_stab_before = s.countries[0].stability;
    const auto c_legit_before = s.countries[0].legitimacy;
    const auto ig_rad_before = s.interest_groups[0].radicalism;
    const auto events_size_before = s.events.size();
    const auto logs_before = s.logs.size();
    const auto cmds_before = s.applied_commands.size();

    (void)ee::trigger_actor(s, s.events[0].triggers[0]);
    (void)ee::evaluate_match(s, s.events[0]);
    (void)ee::match_events(s);

    CHECK(s.countries[0].stability         == c_stab_before);
    CHECK(s.countries[0].legitimacy        == c_legit_before);
    CHECK(s.interest_groups[0].radicalism  == ig_rad_before);
    CHECK(s.events.size()                  == events_size_before);
    CHECK(s.logs.size()                    == logs_before);
    CHECK(s.applied_commands.size()        == cmds_before);
}

// ---- M5.3 deliberate non-goals --------------------------------------

TEST_CASE("M5.3 evaluate_match: still does NOT consult effects vector") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20));
    PolicyEffect bogus;
    bogus.target = "country.this_does_not_exist";
    bogus.op     = "neither_does_this";
    bogus.value  = std::nan("");
    EventDefinition def = make_event("only_trigger_matters", {
        make_trigger("country.stability", "lt", 0.30),
    }, {bogus});
    const auto m = ee::evaluate_match(s, def);
    REQUIRE(m.has_value());
    REQUIRE(m->triggers.size() == 1u);
    CHECK(m->triggers[0].actor.id_code == "GER");
}

TEST_CASE("M5.3 match_events: still does NOT append to state.logs / state.applied_commands") {
    GameState s;
    s.countries.push_back(make_country(1, "GER", 0.20));
    s.events.push_back(make_event("unrest", {
        make_trigger("country.stability", "lt", 0.30),
    }, {{"country.stability", "add", -0.02}}));

    const auto logs_before = s.logs.size();
    const auto cmds_before = s.applied_commands.size();
    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    REQUIRE(matches[0].triggers.size() == 1u);
    CHECK(s.logs.size()             == logs_before);
    CHECK(s.applied_commands.size() == cmds_before);
}


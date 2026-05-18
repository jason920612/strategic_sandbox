#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/event_effects.hpp"
#include "leviathan/systems/event_evaluator.hpp"
#include "leviathan/systems/event_firer.hpp"
#include "leviathan/systems/policy_system.hpp"

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
namespace ee  = leviathan::systems::event_evaluator;
namespace ef  = leviathan::systems::event_firer;
namespace eex = leviathan::systems::event_effects;
namespace pol = leviathan::systems::policy;

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

EventDefinition make_event_def(const std::string& id_code,
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

EventInstance hand_instance(const std::string& event_id_code,
                            const std::string& actor_kind,
                            const std::string& actor_id_code,
                            const std::string& country_id_code,
                            std::size_t actor_index = 0) {
    EventInstance inst;
    inst.event_id_code = event_id_code;
    inst.fired_on      = GameDate(1930, 1, 1);
    EventInstanceActor a;
    a.kind            = actor_kind;
    a.id_code         = actor_id_code;
    a.country_id_code = country_id_code;
    a.index           = actor_index;
    inst.actors.push_back(a);
    return inst;
}

}  // namespace

// =====================================================================
// apply_event_effects: happy path
// =====================================================================

TEST_CASE("M5.6 apply_event_effects: country.* effect lands on the first actor's country") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.55));
    EventDefinition def = make_event_def("unrest",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) });
    EventInstance inst =
        hand_instance("unrest", "country", "GER", "GER");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r);
    CHECK(r.value().effects_applied == 1);
    CHECK(s.countries[0].stability == doctest::Approx(0.53));
}

TEST_CASE("M5.6 apply_event_effects: IG-actor's owning country gets the effect") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.40, /*legit*/0.45));
    EventDefinition def = make_event_def(
        "radical_warning",
        { make_trigger("interest_group.radicalism", "gt", 0.75) },
        { make_effect("country.legitimacy", "add", -0.01) });
    EventInstance inst =
        hand_instance("radical_warning", "interest_group",
                      "ger_bureau", "GER");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r);
    CHECK(r.value().effects_applied == 1);
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.44));
}

TEST_CASE("M5.6 apply_event_effects: multiple effects all land on the first actor's country in def order") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50, /*legit*/0.50));
    EventDefinition def = make_event_def(
        "compound",
        { make_trigger("country.stability", "lt", 0.60) },
        {
            make_effect("country.stability",  "add", -0.05),
            make_effect("country.legitimacy", "set",  0.30),
        });
    EventInstance inst =
        hand_instance("compound", "country", "GER", "GER");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r);
    CHECK(r.value().effects_applied == 2);
    CHECK(s.countries[0].stability  == doctest::Approx(0.45));
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.30));
}

TEST_CASE("M5.6 apply_event_effects: budget category effect routes through country.budget.* grammar") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    s.countries[0].budget.military = 0.20;
    EventDefinition def = make_event_def(
        "milboost",
        { make_trigger("country.stability", "lt", 1.00) },
        { make_effect("country.budget.military", "add", 0.10) });
    EventInstance inst =
        hand_instance("milboost", "country", "GER", "GER");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r);
    CHECK(r.value().effects_applied == 1);
    CHECK(s.countries[0].budget.military == doctest::Approx(0.30));
}

TEST_CASE("M5.6 apply_event_effects: ratio fields clamp to [0, 1] post-op (inherits M1.5 clamping)") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.05));
    EventDefinition def = make_event_def(
        "crash",
        { make_trigger("country.stability", "lt", 0.10) },
        { make_effect("country.stability", "add", -0.50) });
    EventInstance inst =
        hand_instance("crash", "country", "GER", "GER");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r);
    CHECK(s.countries[0].stability == doctest::Approx(0.0));
}

// =====================================================================
// pre-flight atomicity: M1.5 contract carries through
// =====================================================================

TEST_CASE("M5.6 apply_event_effects: non-finite effect value rejected; state untouched (atomic)") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50));
    EventDefinition def = make_event_def(
        "bad_value",
        { make_trigger("country.stability", "lt", 1.0) },
        {
            make_effect("country.stability", "add", -0.05),
            make_effect("country.legitimacy", "add",
                        std::numeric_limits<double>::infinity()),
        });
    EventInstance inst =
        hand_instance("bad_value", "country", "GER", "GER");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r.failed());
    // M1.5 pre-flight atomicity: stability stays at 0.50.
    CHECK(s.countries[0].stability == doctest::Approx(0.50));
}

TEST_CASE("M5.6 apply_event_effects: unknown target rejected; state untouched") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50));
    EventDefinition def = make_event_def(
        "bad_target",
        { make_trigger("country.stability", "lt", 1.0) },
        {
            make_effect("country.stability",  "add", -0.05),
            make_effect("country.no_such_field", "add", 0.10),
        });
    EventInstance inst =
        hand_instance("bad_target", "country", "GER", "GER");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r.failed());
    CHECK(s.countries[0].stability == doctest::Approx(0.50));
}

TEST_CASE("M5.6 apply_event_effects: unknown op rejected; state untouched") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50));
    EventDefinition def = make_event_def(
        "bad_op",
        { make_trigger("country.stability", "lt", 1.0) },
        { make_effect("country.stability", "mul", -0.05) });
    EventInstance inst =
        hand_instance("bad_op", "country", "GER", "GER");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r.failed());
    CHECK(s.countries[0].stability == doctest::Approx(0.50));
}

// =====================================================================
// edge cases: empty actors / unresolvable country / empty effects
// =====================================================================

TEST_CASE("M5.6 apply_event_effects: empty actors -> no-op success") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.50));
    EventDefinition def = make_event_def(
        "vacuous", {},
        { make_effect("country.stability", "add", -0.05) });
    EventInstance inst;
    inst.event_id_code = "vacuous";
    inst.fired_on      = GameDate(1930, 1, 1);

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r);
    CHECK(r.value().effects_applied == 0);
    // Country unchanged.
    CHECK(s.countries[0].stability == doctest::Approx(0.50));
}

TEST_CASE("M5.6 apply_event_effects: first-actor country_id_code that does NOT resolve -> Result::failure") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.50));
    EventDefinition def = make_event_def(
        "x",
        { make_trigger("country.stability", "lt", 1.0) },
        { make_effect("country.stability", "add", -0.05) });
    EventInstance inst =
        hand_instance("x", "country", "ATLANTIS", "ATLANTIS");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r.failed());
    CHECK(r.error().find("ATLANTIS") != std::string::npos);
    CHECK(s.countries[0].stability == doctest::Approx(0.50));
}

TEST_CASE("M5.6 apply_event_effects: empty country_id_code on the first actor rejected") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.50));
    EventDefinition def = make_event_def(
        "x",
        { make_trigger("country.stability", "lt", 1.0) },
        { make_effect("country.stability", "add", -0.05) });
    EventInstance inst =
        hand_instance("x", "country", "GER", /*country_id_code*/"");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r.failed());
    CHECK(r.error().find("empty country_id_code") != std::string::npos);
    CHECK(s.countries[0].stability == doctest::Approx(0.50));
}

TEST_CASE("M5.6 apply_event_effects: empty effects -> success with 0 applied") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.50));
    EventDefinition def = make_event_def(
        "x",
        { make_trigger("country.stability", "lt", 1.0) },
        {});
    EventInstance inst =
        hand_instance("x", "country", "GER", "GER");

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r);
    CHECK(r.value().effects_applied == 0);
    CHECK(s.countries[0].stability == doctest::Approx(0.50));
}

// =====================================================================
// "first actor wins" policy: when actors[] has multiple entries
// =====================================================================

TEST_CASE("M5.6 apply_event_effects: with multiple actors, first actor's country gets ALL effects") {
    // GER + FRA both in state; instance carries actors GER first
    // then FRA. The applicator must direct all effects to GER and
    // leave FRA untouched.
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50));
    s.countries.push_back(make_country(1, "FRA", /*stab*/0.70));

    EventDefinition def = make_event_def(
        "compound",
        {
            make_trigger("country.stability", "lt", 0.60),
            make_trigger("country.stability", "lt", 0.80),
        },
        { make_effect("country.stability", "add", -0.10) });

    EventInstance inst;
    inst.event_id_code = "compound";
    inst.fired_on      = GameDate(1930, 1, 1);
    EventInstanceActor a1;
    a1.kind = "country"; a1.id_code = "GER"; a1.country_id_code = "GER"; a1.index = 0;
    EventInstanceActor a2;
    a2.kind = "country"; a2.id_code = "FRA"; a2.country_id_code = "FRA"; a2.index = 1;
    inst.actors.push_back(a1);
    inst.actors.push_back(a2);

    const auto r = eex::apply_event_effects(s, inst, def);
    REQUIRE(r);
    CHECK(r.value().effects_applied == 1);
    CHECK(s.countries[0].stability == doctest::Approx(0.40));   // GER -0.10
    CHECK(s.countries[1].stability == doctest::Approx(0.70));   // FRA untouched
}

// =====================================================================
// no side effects beyond the M1.5 / M5.6 effect path
// =====================================================================

TEST_CASE("M5.6 apply_event_effects: does NOT touch state.event_history / state.logs / state.applied_commands") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.50));
    EventDefinition def = make_event_def(
        "x",
        { make_trigger("country.stability", "lt", 1.0) },
        { make_effect("country.stability", "add", -0.05) });
    EventInstance inst =
        hand_instance("x", "country", "GER", "GER");

    const auto eh_before  = s.event_history.size();
    const auto logs_before = s.logs.size();
    const auto cmds_before = s.applied_commands.size();

    REQUIRE(eex::apply_event_effects(s, inst, def));

    CHECK(s.event_history.size()    == eh_before);
    CHECK(s.logs.size()             == logs_before);
    CHECK(s.applied_commands.size() == cmds_before);
}

TEST_CASE("M5.6 apply_event_effects: does NOT append to country.active_policies (events aren't policies)") {
    // The M1.15 ActivePolicy bookkeeping is policy-specific.
    // M5.6 reaches into the SHARED apply_effects_to_actor helper
    // that intentionally skips that step. Pin the contract so a
    // future refactor can't quietly re-merge the paths and
    // start polluting active_policies with synthesized events.
    GameState s;
    s.countries.push_back(make_country(0, "GER", 0.50));
    EventDefinition def = make_event_def(
        "x",
        { make_trigger("country.stability", "lt", 1.0) },
        { make_effect("country.stability", "add", -0.05) });
    EventInstance inst =
        hand_instance("x", "country", "GER", "GER");

    const auto ap_before = s.countries[0].active_policies.size();
    REQUIRE(eex::apply_event_effects(s, inst, def));
    CHECK(s.countries[0].active_policies.size() == ap_before);
}

// =====================================================================
// end-to-end composition: evaluator -> firer -> effects applicator
// =====================================================================

TEST_CASE("M5.6 end-to-end: match_events -> record_matches -> apply_event_effects produces consistent mutation") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.interest_groups.push_back(make_ig("ger_bureau", 0, /*rad*/0.85));

    EventDefinition def_a = make_event_def(
        "low_stability_unrest",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.stability", "add", -0.02) });
    EventDefinition def_b = make_event_def(
        "radical_interest_group_warning",
        { make_trigger("interest_group.radicalism", "gt", 0.75) },
        { make_effect("country.legitimacy", "add", -0.01) });
    s.events.push_back(def_a);
    s.events.push_back(def_b);

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 2u);
    ef::record_matches(s, matches, GameDate(1930, 3, 15));
    REQUIRE(s.event_history.size() == 2u);

    // Apply each event's effects through M5.6. The runner
    // integration (M5.7+) will own the WHEN of this composition.
    REQUIRE(eex::apply_event_effects(s, s.event_history[0], def_a));
    REQUIRE(eex::apply_event_effects(s, s.event_history[1], def_b));

    CHECK(s.countries[0].stability  == doctest::Approx(0.18));  // 0.20 - 0.02
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.49));  // 0.50 - 0.01
}

// =====================================================================
// M1.5 regression: existing apply_policy_effects still works after
// the M5.6 refactor (the actor-validation + duration cap path)
// =====================================================================

TEST_CASE("M5.6 regression: apply_policy_effects still produces ActivePolicy entry (post-refactor)") {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50));
    leviathan::core::PolicyData policy;
    policy.id_code        = "raise_taxes";
    policy.name           = "Raise Taxes";
    policy.duration_days  = 30;
    policy.effects.push_back(make_effect("country.legal_tax_burden", "set", 0.40));

    REQUIRE(pol::apply_policy_effects(s, CountryId{0}, policy));
    CHECK(s.countries[0].legal_tax_burden == doctest::Approx(0.40));
    REQUIRE(s.countries[0].active_policies.size() == 1u);
    CHECK(s.countries[0].active_policies[0].policy_id_code == "raise_taxes");
}

TEST_CASE("M5.6 regression: apply_policy_effects still rejects bad duration without touching state") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50));
    leviathan::core::PolicyData policy;
    policy.id_code        = "x";
    policy.duration_days  = -1;
    policy.effects.push_back(make_effect("country.stability", "add", -0.05));
    const auto r = pol::apply_policy_effects(s, CountryId{0}, policy);
    REQUIRE(r.failed());
    CHECK(s.countries[0].stability == doctest::Approx(0.50));
    CHECK(s.countries[0].active_policies.empty());
}

// =====================================================================
// apply_effects_to_actor (new M5.6 public helper) directly
// =====================================================================

TEST_CASE("M5.6 policy::apply_effects_to_actor: does NOT append ActivePolicy entry") {
    // The whole point of extracting this helper is to give the
    // event-effects path a way to apply effects WITHOUT the
    // M1.15 active_policies bookkeeping. Pin that contract.
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50));
    std::vector<PolicyEffect> effects = {
        make_effect("country.stability", "add", -0.05),
    };
    REQUIRE(pol::apply_effects_to_actor(s, CountryId{0}, effects));
    CHECK(s.countries[0].stability == doctest::Approx(0.45));
    CHECK(s.countries[0].active_policies.empty());
}

TEST_CASE("M5.6 policy::apply_effects_to_actor: invalid actor index rejected; state untouched") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50));
    std::vector<PolicyEffect> effects = {
        make_effect("country.stability", "add", -0.05),
    };
    const auto r = pol::apply_effects_to_actor(s, CountryId{99}, effects);
    REQUIRE(r.failed());
    CHECK(s.countries[0].stability == doctest::Approx(0.50));
}

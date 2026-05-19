#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <limits>
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
    d.id_code        = id_code;
    d.name           = id_code;
    d.visible_report = "test visible report";   // M6.2: required non-empty
    d.true_cause     = "test true cause";       // M6.1: required non-empty
    d.category       = "test";                  // issue #112: required non-empty
    d.triggers       = std::move(triggers);
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
    // but the firer is still total over the actor-list side).
    //
    // M6.8 / M6.9 update: `record_match` now requires
    // `match.event_index` to be a valid index into `state.events`
    // (so it can look up the M6.1 true_cause and the M6.2
    // visible_report). The vacuously-true ACTOR side is preserved;
    // we just need a corresponding EventDefinition in state.events.
    GameState s;
    s.events.push_back(make_event("vacuous", {
        make_trigger("country.stability", "lt", 0.30),
    }));
    ee::EventMatch m;
    m.event_index   = 0;
    m.event_id_code = "vacuous";
    // m.triggers stays empty.
    const auto r = ef::record_match(s, m, GameDate(1930, 1, 1));
    REQUIRE(r);
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
    // M6.8 / M6.9 update: record_match needs the EventDefinition
    // to source true_cause + visible_report; supply one.
    s.events.push_back(make_event("x", {
        make_trigger("country.stability", "lt", 0.30),
    }));

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

    const auto fired = ef::record_match(s, m, GameDate(1930, 1, 1));
    REQUIRE(fired);
    REQUIRE(s.event_history.size() == 1u);
    REQUIRE(s.event_history[0].actors.size() == 1u);
    CHECK(s.event_history[0].actors[0].kind            == "interest_group");
    CHECK(s.event_history[0].actors[0].id_code         == "orphan_ig");
    CHECK(s.event_history[0].actors[0].country_id_code == "");

    // And the save layer rejects it loudly on the next round-trip.
    const std::string text = ss::serialize(s);
    const auto rt = ss::deserialize(text);
    REQUIRE(rt.failed());
    CHECK(rt.error().find("country_id_code") != std::string::npos);
}

// =====================================================================
// record_matches: batch, ordering, accumulation
// =====================================================================

TEST_CASE("M5.5 record_matches: empty input is a no-op") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    const std::vector<ee::EventMatch> none;
    const auto r = ef::record_matches(s, none, GameDate(1930, 1, 1));
    REQUIRE(r);
    CHECK(r.value().recorded == 0u);
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

    const auto r = ef::record_matches(s, matches, GameDate(1930, 6, 1));
    REQUIRE(r);
    CHECK(r.value().recorded == 2u);
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

    const auto r =
        ef::record_matches(s, matches, GameDate(1930, 4, 1));
    REQUIRE(r);
    CHECK(r.value().recorded == 2u);
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

TEST_CASE("M5.5 record_matches: does NOT modify countries / IGs / events / applied_commands / current_date (logs migrated to RCR-1)") {
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
    // RCR-1 (RFC-090 §5.9): record_matches now appends ONE
    // LogEntry per fired event into state.logs. The original
    // M5.5 "no logs append" invariant is deliberately
    // superseded by RCR-1 — events.jsonl is the visible record
    // surface for fired events. The remaining invariants
    // (countries / IGs / events / applied_commands /
    // current_date all unchanged) still hold.
    CHECK(s.logs.size()                    == logs_size_before + 1);
    CHECK(s.logs.back().category           == "event_fired");
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

// =====================================================================
// M6.8 (RFC-090 §6.8 "debug 模式顯示真相"): event_firer attaches the
// matching event's `EventDefinition.true_cause` (M6.1) as a
// `true_cause` metadata key on every `event_fired` LogEntry it
// records. The attachment is UNCONDITIONAL at the event_firer
// layer — the events.jsonl artefact filters the key out unless
// `RunnerOptions::debug_mode == true` (see logging_system_test).
// =====================================================================

namespace {

const leviathan::core::LogEntry* find_event_fired_log(const GameState& s) {
    for (const auto& e : s.logs) {
        if (e.category == "event_fired") return &e;
    }
    return nullptr;
}

const std::string* metadata_value(const leviathan::core::LogEntry& e,
                                  const std::string& key) {
    for (const auto& kv : e.metadata) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

}  // namespace

TEST_CASE("M6.8 record_match: appends `true_cause` metadata key sourced from EventDefinition") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.events.push_back(make_event("low_stab",
        { make_trigger("country.stability", "lt", 0.30) }));
    // Author a specific true_cause so the test can pin the value.
    s.events.back().true_cause =
        "Local interest groups instigated unrest to derail tax reform.";

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    ef::record_match(s, matches[0], GameDate(1930, 4, 1));

    const auto* entry = find_event_fired_log(s);
    REQUIRE(entry != nullptr);
    const auto* tc = metadata_value(*entry, "true_cause");
    REQUIRE(tc != nullptr);
    CHECK(*tc ==
          "Local interest groups instigated unrest to derail tax reform.");
    // M6.8 invariant: the truth IS recorded in state.logs even
    // though events.jsonl will hide it without --debug.
}

TEST_CASE("M6.8 record_followup: appends `true_cause` metadata key sourced from followup EventDefinition") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.events.push_back(make_event("parent",
        { make_trigger("country.stability", "lt", 0.30) }));
    s.events.push_back(make_event("child",
        { make_trigger("country.stability", "lt", 0.30) }));
    s.events.back().true_cause =
        "Censorship law triggered media backlash; ministry knew.";

    // Build a synthetic parent EventInstance directly so we can
    // exercise record_followup without driving the engine.
    EventInstance parent;
    parent.event_id_code = "parent";
    parent.fired_on      = GameDate(1930, 4, 1);
    EventInstanceActor a;
    a.kind            = "country";
    a.id_code         = "GER";
    a.country_id_code = "GER";
    a.index           = 0;
    parent.actors.push_back(a);

    ef::record_followup(s, parent, s.events[1], GameDate(1930, 5, 1));

    const auto* entry = find_event_fired_log(s);
    REQUIRE(entry != nullptr);
    const auto* tc = metadata_value(*entry, "true_cause");
    REQUIRE(tc != nullptr);
    CHECK(*tc ==
          "Censorship law triggered media backlash; ministry knew.");
    const auto* fu = metadata_value(*entry, "followup_of");
    REQUIRE(fu != nullptr);
    CHECK(*fu == "parent");
}

TEST_CASE("M6.8 + M6.9 record_match: metadata keys land in the documented insertion order") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.20));
    s.events.push_back(make_event("low_stab",
        { make_trigger("country.stability", "lt", 0.30) }));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    REQUIRE(ef::record_match(s, matches[0], GameDate(1930, 4, 1)));

    const auto* entry = find_event_fired_log(s);
    REQUIRE(entry != nullptr);
    // Stable insertion order (post-M6.9):
    //   event_id_code, actor_kind, actor_id_code, country_id_code,
    //   true_cause (M6.8),
    //   publicText, information_accuracy, reported_intensity,
    //   noise_sample (M6.9). The publicText key uses the
    //   RFC-060 §3 vocabulary; the source field is M6.2's
    //   EventDefinition.visible_report, which keeps its
    //   schema-level name.
    REQUIRE(entry->metadata.size() == 9u);
    CHECK(entry->metadata[0].first == "event_id_code");
    CHECK(entry->metadata[1].first == "actor_kind");
    CHECK(entry->metadata[2].first == "actor_id_code");
    CHECK(entry->metadata[3].first == "country_id_code");
    CHECK(entry->metadata[4].first == "true_cause");
    CHECK(entry->metadata[5].first == "publicText");
    CHECK(entry->metadata[6].first == "information_accuracy");
    CHECK(entry->metadata[7].first == "reported_intensity");
    CHECK(entry->metadata[8].first == "noise_sample");
}

// =====================================================================
// M6.9 (RFC-090 §6.9 "非 debug 模式隱藏真相") — record_match emits
// the player-facing distortion fields sourced from the M6.3 / M6.6
// / M6.7 information_accuracy + M6.4 reported_value + M6.5
// bias_noise composition. `publicText` is emitted on EVERY
// `event_fired` LogEntry (sourced verbatim from M6.2
// `EventDefinition.visible_report` — the metadata key follows
// the RFC-060 §3 `EventLogEntry.publicText` vocabulary; the
// schema field keeps its M6.2 name). The three numeric
// distortion keys (`information_accuracy`,
// `reported_intensity`, `noise_sample`) appear ONLY when the
// event has a first-actor country (country-anchored events,
// which is every event the M5.1 schema accepts at load
// time). Vacuous-actor hand-built matches still emit
// `publicText` but skip the numeric distortion fields —
// there is no country anchor for
// `information_accuracy::compute_for_country` in that
// degenerate case.
// `logging::write_jsonl_line` filters only the M6.8
// `true_cause` key out of the events.jsonl artefact in
// non-debug mode; the M6.9 keys are not filtered.
// RFC-080 §8 `ReportedValue = TrueValue + Bias + Noise`
// lives in (information_accuracy, reported_intensity,
// noise_sample).
// =====================================================================

namespace {

CountryState make_m69_country(int id, const std::string& code,
                              double intelligence_capability,
                              double budget_intelligence,
                              double corruption) {
    CountryState c;
    c.id      = CountryId{id};
    c.id_code = code;
    c.name    = code;
    c.stability  = 0.20;   // below threshold so the event fires
    c.legitimacy = 0.50;
    c.corruption = corruption;
    c.government_authority.intelligence_capability = intelligence_capability;
    c.budget.intelligence                          = budget_intelligence;
    return c;
}

}  // namespace

TEST_CASE("M6.9 record_match: publicText metadata mirrors EventDefinition.visible_report verbatim") {
    GameState s;
    s.countries.push_back(
        make_m69_country(0, "GER", /*cap*/0.5, /*bud*/0.0, /*corr*/0.0));
    auto def = make_event("ev_v",
        { make_trigger("country.stability", "lt", 0.30) });
    def.visible_report =
        "Reports describe a stability concern in the capital district.";
    s.events.push_back(std::move(def));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    REQUIRE(ef::record_match(s, matches[0], GameDate(1930, 4, 1)));

    const auto* entry = find_event_fired_log(s);
    REQUIRE(entry != nullptr);
    // RFC-060 §3 EventLogEntry.publicText: the events.jsonl
    // metadata key uses the RFC vocabulary; the schema field
    // keeps its M6.2 name (EventDefinition.visible_report).
    const auto* pt = metadata_value(*entry, "publicText");
    REQUIRE(pt != nullptr);
    CHECK(*pt ==
          "Reports describe a stability concern in the capital district.");
    // No `visible_report` key on the emitted entry — only
    // `publicText` (the RFC-060 name).
    CHECK(metadata_value(*entry, "visible_report") == nullptr);
}

TEST_CASE("M6.9 record_match: high accuracy -> reported_intensity close to 1.0 and small noise envelope") {
    // intelligence_capability = 1.0, budget.intelligence = 1.0,
    // corruption = 0.0 -> accuracy = 1.0 (maxed). Reported intensity
    // = 1.0 * accuracy = 1.0. amplitude = 1 - 1 = 0; bias_noise
    // returns 0 verbatim at amplitude 0. Player sees the truth.
    GameState s;
    s.countries.push_back(
        make_m69_country(0, "MAX", /*cap*/1.0, /*bud*/1.0, /*corr*/0.0));
    s.events.push_back(make_event("ev_hi",
        { make_trigger("country.stability", "lt", 0.30) }));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    REQUIRE(ef::record_match(s, matches[0], GameDate(1930, 4, 1)));

    const auto* entry = find_event_fired_log(s);
    REQUIRE(entry != nullptr);
    const auto* acc = metadata_value(*entry, "information_accuracy");
    const auto* ri  = metadata_value(*entry, "reported_intensity");
    const auto* ns  = metadata_value(*entry, "noise_sample");
    REQUIRE(acc != nullptr);
    REQUIRE(ri  != nullptr);
    REQUIRE(ns  != nullptr);
    CHECK(std::stod(*acc) == doctest::Approx(1.0));
    CHECK(std::stod(*ri)  == doctest::Approx(1.0));
    // amplitude = 0 -> bias_noise returns 0 verbatim.
    CHECK(std::stod(*ns)  == doctest::Approx(0.0));
}

TEST_CASE("M6.9 record_match: low accuracy + high corruption -> larger distortion envelope (noise amplitude rises)") {
    // intelligence_capability = 0.0, budget.intelligence = 0.0,
    // corruption = 1.0 -> accuracy = 0.4 - 0.4 = 0.0 (full
    // blackout). amplitude = 1 - 0 = 1.0; bias_noise spans
    // [-1, +1]; |noise| can be large.
    GameState s_lo;
    s_lo.countries.push_back(
        make_m69_country(0, "LOW", /*cap*/0.0, /*bud*/0.0, /*corr*/1.0));
    s_lo.events.push_back(make_event("ev_lo",
        { make_trigger("country.stability", "lt", 0.30) }));

    const auto matches_lo = ee::match_events(s_lo);
    REQUIRE(matches_lo.size() == 1u);
    REQUIRE(ef::record_match(s_lo, matches_lo[0], GameDate(1930, 4, 1)));

    const auto* entry_lo = find_event_fired_log(s_lo);
    REQUIRE(entry_lo != nullptr);
    const double acc_lo =
        std::stod(*metadata_value(*entry_lo, "information_accuracy"));
    const double ns_lo  =
        std::stod(*metadata_value(*entry_lo, "noise_sample"));
    CHECK(acc_lo == doctest::Approx(0.0));
    // noise envelope = 1.0; pin |noise| <= 1.0 mechanically.
    CHECK(std::abs(ns_lo) <= 1.0);

    // Now compare against a high-accuracy country firing the same
    // event id_code / country_id_code. The noise amplitudes differ
    // (1.0 vs 0.0), so the noise SAMPLES MUST differ as well.
    GameState s_hi;
    s_hi.countries.push_back(
        make_m69_country(0, "LOW", /*cap*/1.0, /*bud*/1.0, /*corr*/0.0));
    s_hi.events.push_back(make_event("ev_lo",
        { make_trigger("country.stability", "lt", 0.30) }));
    const auto matches_hi = ee::match_events(s_hi);
    REQUIRE(matches_hi.size() == 1u);
    REQUIRE(ef::record_match(s_hi, matches_hi[0], GameDate(1930, 4, 1)));

    const auto* entry_hi = find_event_fired_log(s_hi);
    REQUIRE(entry_hi != nullptr);
    const double ns_hi  =
        std::stod(*metadata_value(*entry_hi, "noise_sample"));
    // Distortion shape: high accuracy -> noise = 0; low accuracy ->
    // |noise| can be non-zero. Pin that the low-accuracy noise
    // envelope is wider than the high-accuracy one.
    CHECK(std::abs(ns_lo) >= std::abs(ns_hi));
    CHECK(std::abs(ns_hi) == doctest::Approx(0.0));
}

TEST_CASE("M6.9 record_match: same inputs -> deterministic distortion sample") {
    // Two independent runs with same event_id_code +
    // country_id_code + fired_on + accuracy MUST produce
    // byte-identical noise (bias_noise is hash-deterministic per
    // its M6.5 contract; no state.rng consumption).
    GameState s_a;
    s_a.countries.push_back(
        make_m69_country(0, "DET", /*cap*/0.3, /*bud*/0.2, /*corr*/0.4));
    s_a.events.push_back(make_event("ev_det",
        { make_trigger("country.stability", "lt", 0.30) }));
    const auto matches_a = ee::match_events(s_a);
    REQUIRE(matches_a.size() == 1u);
    REQUIRE(ef::record_match(s_a, matches_a[0], GameDate(1930, 4, 1)));

    GameState s_b;
    s_b.countries.push_back(
        make_m69_country(0, "DET", /*cap*/0.3, /*bud*/0.2, /*corr*/0.4));
    s_b.events.push_back(make_event("ev_det",
        { make_trigger("country.stability", "lt", 0.30) }));
    const auto matches_b = ee::match_events(s_b);
    REQUIRE(matches_b.size() == 1u);
    REQUIRE(ef::record_match(s_b, matches_b[0], GameDate(1930, 4, 1)));

    const auto* entry_a = find_event_fired_log(s_a);
    const auto* entry_b = find_event_fired_log(s_b);
    REQUIRE(entry_a != nullptr);
    REQUIRE(entry_b != nullptr);
    CHECK(*metadata_value(*entry_a, "information_accuracy") ==
          *metadata_value(*entry_b, "information_accuracy"));
    CHECK(*metadata_value(*entry_a, "reported_intensity") ==
          *metadata_value(*entry_b, "reported_intensity"));
    CHECK(*metadata_value(*entry_a, "noise_sample") ==
          *metadata_value(*entry_b, "noise_sample"));
}

TEST_CASE("M6.9 record_match: non-finite country.intelligence_capability FAILS LOUDLY (no LogEntry / EventInstance appended)") {
    GameState s;
    auto c = make_m69_country(0, "BAD", /*cap*/0.5, /*bud*/0.0, /*corr*/0.0);
    c.government_authority.intelligence_capability =
        std::numeric_limits<double>::quiet_NaN();
    s.countries.push_back(c);
    s.events.push_back(make_event("ev_bad",
        { make_trigger("country.stability", "lt", 0.30) }));

    const auto matches = ee::match_events(s);
    REQUIRE(matches.size() == 1u);
    const auto r = ef::record_match(s, matches[0], GameDate(1930, 4, 1));
    REQUIRE(r.failed());
    CHECK(r.error().find("information_accuracy") != std::string::npos);
    // Per-event atomicity: nothing was appended.
    CHECK(s.event_history.empty());
    bool found_fired = false;
    for (const auto& e : s.logs) {
        if (e.category == "event_fired") {
            found_fired = true;
            break;
        }
    }
    CHECK_FALSE(found_fired);
}

TEST_CASE("M6.9 record_followup: distortion uses parent's first-actor country") {
    GameState s;
    s.countries.push_back(
        make_m69_country(0, "GER", /*cap*/0.5, /*bud*/0.0, /*corr*/0.0));
    s.events.push_back(make_event("p", {
        make_trigger("country.stability", "lt", 0.30)}));
    s.events.push_back(make_event("c", {
        make_trigger("country.stability", "lt", 0.30)}));
    s.events.back().visible_report =
        "Follow-up report on the original disturbance.";

    EventInstance parent;
    parent.event_id_code = "p";
    parent.fired_on      = GameDate(1930, 4, 1);
    EventInstanceActor a;
    a.kind            = "country";
    a.id_code         = "GER";
    a.country_id_code = "GER";
    a.index           = 0;
    parent.actors.push_back(a);

    REQUIRE(ef::record_followup(s, parent, s.events[1], GameDate(1930, 5, 1)));

    const auto* entry = find_event_fired_log(s);
    REQUIRE(entry != nullptr);
    const auto* pt  = metadata_value(*entry, "publicText");
    const auto* acc = metadata_value(*entry, "information_accuracy");
    REQUIRE(pt  != nullptr);
    REQUIRE(acc != nullptr);
    CHECK(*pt == "Follow-up report on the original disturbance.");
    // Same accuracy as the parent country (GER's cap=0.5 / bud=0
    // / corr=0 -> 0.4 + 0.6×0.35 = 0.61).
    CHECK(std::stod(*acc) == doctest::Approx(0.61));
}

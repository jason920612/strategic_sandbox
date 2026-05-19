// M5.5: EventFirer implementation.
//
// See include/leviathan/systems/event_firer.hpp for the public
// contract (semantics, conversion rules, deliberate non-goals).
// This file is the small bridge that turns M5.3 actor binding
// into M5.4 EventInstance records.

#include "leviathan/systems/event_firer.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "leviathan/core/log_entry.hpp"

namespace leviathan::systems::event_firer {
namespace {

// Resolve a CountryId to its on-disk id_code by linear scan of
// state.countries. Linear scan is fine here: state.countries is
// canonically small (scenario-load order; canonical 1930 has 3),
// and this is not on a per-tick hot path (a future M5.x runner
// integration will fire at most once per month-boundary
// evaluation, not once per day).
//
// Returns empty string if no country has matching id. Caller
// (M5.5 record_match below) treats empty country_id_code as a
// signal that state is internally inconsistent — the save
// layer's M5.4 validation will reject the resulting save round-
// trip, surfacing the bug loudly instead of silently corrupting
// history.
std::string country_id_code_for(const core::GameState& state,
                                core::CountryId        cid) {
    for (const auto& c : state.countries) {
        if (c.id == cid) {
            return c.id_code;
        }
    }
    return std::string{};
}

// Convert one TriggerEvaluation into one EventInstanceActor.
// kind string mapping: Country -> "country", InterestGroup ->
// "interest_group" (matches the M5.4 save-layer allowlist).
core::EventInstanceActor to_actor(
        const core::GameState&                          state,
        const event_evaluator::TriggerEvaluation&       te) {
    core::EventInstanceActor a;
    a.id_code = te.actor.id_code;
    a.index   = te.actor.index;
    switch (te.actor.kind) {
        case event_evaluator::TriggerActorKind::Country: {
            a.kind            = "country";
            // A country IS its own owning country: id_code is
            // both the actor's identity and its
            // country_id_code. Using id_code directly avoids
            // a redundant lookup of state.countries[idx]
            // that would produce the same string.
            a.country_id_code = te.actor.id_code;
            break;
        }
        case event_evaluator::TriggerActorKind::InterestGroup: {
            a.kind            = "interest_group";
            // The IG's owning country is referenced by
            // CountryId — resolve to id_code via the state
            // lookup. If lookup fails (state internally
            // inconsistent), country_id_code stays empty and
            // the save layer rejects it on next round-trip.
            a.country_id_code = country_id_code_for(state,
                                                    te.actor.country);
            break;
        }
    }
    return a;
}

}  // namespace

void record_match(core::GameState&                                state,
                  const event_evaluator::EventMatch&              match,
                  const core::GameDate&                           fired_on) {
    core::EventInstance inst;
    inst.event_id_code = match.event_id_code;
    inst.fired_on      = fired_on;
    inst.actors.reserve(match.triggers.size());
    for (const auto& te : match.triggers) {
        inst.actors.push_back(to_actor(state, te));
    }

    // RCR-1: RFC-090 §5.9 — emit one per-fire LogEntry into
    // `state.logs` so the M0.6 events.jsonl artefact surfaces
    // event firings. Canonical scenarios at M5 deliberately
    // tuned their events to NOT fire, so canonical events.jsonl
    // bytes stay byte-identical with the M5 close-out (the
    // canonical-non-fire property is the load-bearing invariant
    // that keeps M1.17 / M2 / M3 / M4 / M5 determinism baselines
    // green). Only scenarios where events fire receive these
    // new entries.
    //
    // kind="event_fired"; metadata is a stable insertion-order
    // sequence of:
    //   event_id_code
    //   actor_kind         (kind of actors[0], or "<none>" when
    //                       actors is empty)
    //   actor_id_code      (id_code of actors[0], or "")
    //   country_id_code    (country_id_code of actors[0], or "")
    //   true_cause         (M6.8: verbatim
    //                       EventDefinition.true_cause string)
    //
    // M6.8 (RFC-090 §6.8 "debug 模式顯示真相"): the
    // `true_cause` metadata key is appended here UNCONDITIONALLY
    // at fire time so the truth is always present in
    // `state.logs` (and therefore in the save.json `logs` array
    // — debuggable / inspectable / replay-stable). The
    // events.jsonl artefact filters this key out unless
    // `RunnerOptions::debug_mode == true`; see
    // `logging::export_jsonl` and runner::end_tick. The
    // EventDefinition.true_cause field has been required non-
    // empty on `state.events` since M6.1, so this lookup never
    // produces an empty string.
    //
    // Multi-actor events emit ONE log entry with the first
    // actor's surface attached; the EventInstance in
    // state.event_history records the full actor list. This
    // keeps events.jsonl per-fire records compact while leaving
    // the full audit trail in the save artefact.
    core::LogEntry entry;
    entry.date     = fired_on;
    entry.category = "event_fired";
    entry.source   = "event_firer";
    entry.message  = std::string("event ") + match.event_id_code +
                     " fired";
    entry.metadata.push_back({"event_id_code", match.event_id_code});
    if (!inst.actors.empty()) {
        const auto& a0 = inst.actors.front();
        entry.metadata.push_back({"actor_kind",      a0.kind});
        entry.metadata.push_back({"actor_id_code",   a0.id_code});
        entry.metadata.push_back({"country_id_code", a0.country_id_code});
    } else {
        entry.metadata.push_back({"actor_kind",      std::string("<none>")});
        entry.metadata.push_back({"actor_id_code",   std::string()});
        entry.metadata.push_back({"country_id_code", std::string()});
    }
    // M6.8: append the M6.1 true_cause string sourced from the
    // EventDefinition that owns this match. event_index is
    // populated by the M5.2 / M5.3 match-construction surface and
    // is always in range when record_match is called on a real
    // match (event_engine::tick_events / event_evaluator never
    // emit a match with an out-of-range index).
    if (match.event_index < state.events.size()) {
        entry.metadata.push_back(
            {"true_cause", state.events[match.event_index].true_cause});
    }
    state.logs.push_back(std::move(entry));

    state.event_history.push_back(std::move(inst));
}

FireOutcome record_matches(
        core::GameState&                                  state,
        const std::vector<event_evaluator::EventMatch>&   matches,
        const core::GameDate&                             fired_on) {
    FireOutcome out;
    for (const auto& m : matches) {
        record_match(state, m, fired_on);
        out.recorded += 1;
    }
    return out;
}

void record_followup(core::GameState&             state,
                     const core::EventInstance&   parent_instance,
                     const core::EventDefinition& followup_definition,
                     const core::GameDate&        fired_on) {
    core::EventInstance inst;
    inst.event_id_code = followup_definition.id_code;
    inst.fired_on      = fired_on;
    inst.actors        = parent_instance.actors;   // inherit from parent

    // Same log emission shape as record_match. Use the parent's
    // first-actor surface so events.jsonl can be filtered by
    // country_id_code consistently across base + followup fires.
    // M6.8 (RFC-090 §6.8): the followup's own `true_cause`
    // string is recorded as the last metadata key, mirroring the
    // record_match path; `logging::export_jsonl` filters it out
    // unless `RunnerOptions::debug_mode == true`.
    core::LogEntry entry;
    entry.date     = fired_on;
    entry.category = "event_fired";
    entry.source   = "event_firer";
    entry.message  = std::string("event ") + followup_definition.id_code +
                     " fired (followup of " +
                     parent_instance.event_id_code + ")";
    entry.metadata.push_back({"event_id_code", followup_definition.id_code});
    entry.metadata.push_back({"followup_of",   parent_instance.event_id_code});
    if (!inst.actors.empty()) {
        const auto& a0 = inst.actors.front();
        entry.metadata.push_back({"actor_kind",      a0.kind});
        entry.metadata.push_back({"actor_id_code",   a0.id_code});
        entry.metadata.push_back({"country_id_code", a0.country_id_code});
    } else {
        entry.metadata.push_back({"actor_kind",      std::string("<none>")});
        entry.metadata.push_back({"actor_id_code",   std::string()});
        entry.metadata.push_back({"country_id_code", std::string()});
    }
    entry.metadata.push_back(
        {"true_cause", followup_definition.true_cause});
    state.logs.push_back(std::move(entry));

    state.event_history.push_back(std::move(inst));
}

}  // namespace leviathan::systems::event_firer

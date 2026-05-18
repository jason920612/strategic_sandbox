// M5.5: EventFirer implementation.
//
// See include/leviathan/systems/event_firer.hpp for the public
// contract (semantics, conversion rules, deliberate non-goals).
// This file is the small bridge that turns M5.3 actor binding
// into M5.4 EventInstance records.

#include "leviathan/systems/event_firer.hpp"

#include <cstddef>
#include <utility>
#include <vector>

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

}  // namespace leviathan::systems::event_firer

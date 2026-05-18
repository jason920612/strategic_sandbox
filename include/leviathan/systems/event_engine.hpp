// EventEngine — one-tick composition that wires every M5.x
// helper (evaluator + weight ranker + firer + effects applicator
// + option-default + followup resolver + followup firer) into a
// single `tick_events(state)` call.
//
// Issue #110 (RFC strict compliance) replaced the prior "M5.7
// composition brick" semantics — which only iterated matches and
// applied base effects — with the wired behaviour RFC-090 §5.6 /
// §5.7 / §5.8 / §5.12 describe:
//
//   1. matches  = event_evaluator::match_events(state)
//        - vector<EventMatch>, M5.3 actor binding
//
//   2. ranked   = event_evaluator::rank_weighted_events(state)
//        - vector<WeightedEventCandidate>, sorted DESC by weight,
//          stable tie-break on event vector index
//
//   3. For each candidate in `ranked` that is ALSO in `matches`
//      (intersection by event_index), in weight-desc order:
//      a. event_firer::record_match(state, match, current_date)
//      b. if definition.options non-empty:
//            event_effects::apply_default_option_effects(...)
//         else:
//            event_effects::apply_event_effects(...)
//         (the two surfaces are independent — never both)
//      c. for each id in event_effects::resolve_followup_ids(...):
//            event_firer::record_followup(state, parent_instance,
//                                          followup_definition,
//                                          current_date)
//            apply followup base / default-option effects
//            (DEPTH-1: no recursion into followup's own
//             followup_event_ids; cap preserves M5.7 snapshot
//             semantics)
//
// fired_on = state.current_date for every recorded instance —
// parent or followup — in this tick.
//
// Failure semantics: any per-event apply failure (parent or
// followup) returns Result::failure. State at that point has the
// parent + all prior-iteration events recorded in event_history
// and their effects applied; the failing event is recorded but
// effects-failed. The caller decides whether to roll forward.
//
// What `tick_events` does NOT do:
//   - recurse into followup-of-followup chains (depth-1 only)
//   - consume state.rng (still RNG-free; weighted draw stays
//     deterministic via the weight-desc + stable-index rule)
//   - emit new artefacts beyond what record_match /
//     record_followup already write to state.event_history +
//     state.logs (one `event_fired` LogEntry per fired instance)

#ifndef LEVIATHAN_SYSTEMS_EVENT_ENGINE_HPP
#define LEVIATHAN_SYSTEMS_EVENT_ENGINE_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::event_engine {

// One tick_events call's summary. Counts include only events
// that finished successfully — on a mid-round failure, the
// counts reflect the (matched, recorded, applied) progress at
// the time of failure.
struct TickOutcome {
    int events_matched          = 0;  // == match_events(state).size()
    int events_recorded         = 0;  // appended to state.event_history (parents only)
    int events_applied          = 0;  // parent effects-applicator succeeded
    int total_effects_applied   = 0;  // sum of per-parent effects_applied
    int events_with_options     = 0;  // parents whose options vector was non-empty
    int events_with_followups   = 0;  // parents with at least one resolved followup
    int followups_recorded      = 0;  // depth-1 followups recorded in event_history
    int total_followup_effects_applied = 0;  // sum of per-followup effects_applied
};

// One round of event-engine processing on `state`. See header
// doc for the per-match semantics, weight-ordered firing, option-
// default-vs-base dispatch, depth-1 followup chain, and failure
// mode.
//
// fired_on = state.current_date for every recorded instance
// (parent + followups) in this round.
core::Result<TickOutcome> tick_events(core::GameState& state);

}  // namespace leviathan::systems::event_engine

#endif  // LEVIATHAN_SYSTEMS_EVENT_ENGINE_HPP

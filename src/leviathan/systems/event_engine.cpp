// Issue #110: event engine wiring.
//
// `tick_events` now drives:
//   - WEIGHT-ordered firing of every matched event (RFC-090 §5.6 +
//     §5.7), in descending `rank_weighted_events` order with stable
//     tie-break on event vector index.
//   - OPTION-default effect application (RFC-090 §5.4 / §5.8): when
//     an event has a non-empty `options` vector, fire `options[0]`'s
//     effects via `event_effects::apply_default_option_effects`
//     instead of the base `apply_event_effects`. Events without
//     options fall through to the M5.6 base-effect path.
//   - DEPTH-1 followup chains (RFC-090 §5.12): after firing the
//     parent, each resolved id in `followup_event_ids` is appended
//     to `event_history` via `record_followup` and its own effects
//     applied (option-default or base). No recursion into the
//     followup's own followup_event_ids — depth is capped at 1 to
//     preserve M5.7's snapshot-evaluation property.
//
// These three behaviours collectively close issue #110 §4, §5, §6:
// "weights must affect selection", "options must be applied in
// flow", "followups must execute in flow".

#include "leviathan/systems/event_engine.hpp"

#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <utility>

#include "leviathan/core/entities.hpp"
#include "leviathan/systems/event_effects.hpp"
#include "leviathan/systems/event_evaluator.hpp"
#include "leviathan/systems/event_firer.hpp"

namespace leviathan::systems::event_engine {
namespace {

// Dispatch base-vs-option effects for a single fired event.
// Returns the apply outcome (or propagates failure).
core::Result<event_effects::ApplyOutcome>
fire_effects_for(core::GameState&             state,
                 const core::EventInstance&   instance,
                 const core::EventDefinition& definition) {
    if (definition.options.empty()) {
        return event_effects::apply_event_effects(
            state, instance, definition);
    }
    // Options present → default-option effects only. The two surfaces
    // are independent (see event_effects.hpp): tick fires base OR
    // options[0], never both.
    return event_effects::apply_default_option_effects(
        state, instance, definition);
}

}  // namespace

core::Result<TickOutcome> tick_events(core::GameState& state) {
    TickOutcome out;

    const auto matches = event_evaluator::match_events(state);
    out.events_matched = static_cast<int>(matches.size());
    if (matches.empty()) {
        return core::Result<TickOutcome>::success(std::move(out));
    }

    // Build a lookup from event_index -> position in `matches` so we
    // can iterate `rank_weighted_events`'s already-sorted output and
    // intersect with the match set in O(1) per candidate.
    std::unordered_map<std::size_t, std::size_t> match_by_event_index;
    match_by_event_index.reserve(matches.size());
    for (std::size_t i = 0; i < matches.size(); ++i) {
        match_by_event_index.emplace(matches[i].event_index, i);
    }

    const auto ranked = event_evaluator::rank_weighted_events(state);

    // Walk ranked candidates in descending-weight order. Stable
    // index tie-break is already provided by `rank_weighted_events`
    // (per its doc-comment: stable_sort on event vector index for
    // equal weights).
    for (const auto& cand : ranked) {
        auto it = match_by_event_index.find(cand.event_index);
        if (it == match_by_event_index.end()) {
            continue;   // ranked event didn't match this tick
        }
        const auto& match = matches[it->second];
        const auto& def   = state.events[cand.event_index];

        // 1. Record the parent event.
        event_firer::record_match(state, match, state.current_date);
        out.events_recorded += 1;
        const auto parent_instance = state.event_history.back();

        // 2. Apply parent effects (base OR default-option).
        if (!def.options.empty()) {
            out.events_with_options += 1;
        }
        auto eff_r = fire_effects_for(state, parent_instance, def);
        if (!eff_r) {
            return core::Result<TickOutcome>::failure(
                "tick_events: parent " + cand.event_id_code +
                ": " + eff_r.error());
        }
        out.events_applied        += 1;
        out.total_effects_applied += eff_r.value().effects_applied;

        // 3. Depth-1 followup chain.
        const auto followup_indices =
            event_effects::resolve_followup_ids(state, def);
        if (!followup_indices.empty()) {
            out.events_with_followups += 1;
        }
        for (std::size_t fidx : followup_indices) {
            const auto& fdef = state.events[fidx];

            event_firer::record_followup(
                state, parent_instance, fdef, state.current_date);
            out.followups_recorded += 1;
            const auto followup_instance = state.event_history.back();

            // Apply followup effects (depth-1: NO recursion into
            // fdef.followup_event_ids).
            auto fr = fire_effects_for(state, followup_instance, fdef);
            if (!fr) {
                return core::Result<TickOutcome>::failure(
                    "tick_events: followup " + fdef.id_code +
                    " of " + cand.event_id_code + ": " + fr.error());
            }
            out.total_followup_effects_applied += fr.value().effects_applied;
        }
    }

    return core::Result<TickOutcome>::success(std::move(out));
}

}  // namespace leviathan::systems::event_engine

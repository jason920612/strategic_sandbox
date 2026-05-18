// M5.7: EventEngine implementation.
//
// See include/leviathan/systems/event_engine.hpp for the public
// contract (per-match semantics, failure mode, caller
// responsibilities, deliberate non-goals).
//
// This file is the small composition that glues the M5.2
// evaluator, M5.5 firer, and M5.6 effects applicator into a
// single per-round call. No new behaviour ships here beyond the
// composition itself — every step delegates to a shipped M5.x
// module.

#include "leviathan/systems/event_engine.hpp"

#include <cstddef>
#include <utility>

#include "leviathan/core/entities.hpp"
#include "leviathan/systems/event_effects.hpp"
#include "leviathan/systems/event_evaluator.hpp"
#include "leviathan/systems/event_firer.hpp"

namespace leviathan::systems::event_engine {

core::Result<TickOutcome> tick_events(core::GameState& state) {
    TickOutcome out;

    const auto matches = event_evaluator::match_events(state);
    out.events_matched = static_cast<int>(matches.size());

    for (const auto& match : matches) {
        // Resolve the EventDefinition by index (M5.2 guarantees
        // event_index is a valid offset into state.events at
        // the time match_events was called; we hold state by
        // reference and no concurrent mutator exists).
        const auto& def = state.events[match.event_index];

        // M5.5: record the match in state.event_history. The
        // firer is documented as always succeeds; an invariant
        // breach would surface on the next save round-trip via
        // M5.4's save-layer validation, not here.
        event_firer::record_match(state, match, state.current_date);
        out.events_recorded += 1;

        // M5.6: apply effects to the freshly-appended
        // EventInstance. The instance's first-actor
        // country_id_code drives where effects land.
        const auto& instance = state.event_history.back();
        auto r = event_effects::apply_event_effects(state, instance, def);
        if (!r) {
            return core::Result<TickOutcome>::failure(
                "tick_events: " + r.error());
        }
        out.events_applied        += 1;
        out.total_effects_applied += r.value().effects_applied;
    }

    return core::Result<TickOutcome>::success(std::move(out));
}

}  // namespace leviathan::systems::event_engine

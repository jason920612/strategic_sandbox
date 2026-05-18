// M5.6: EventEffects implementation.
//
// See include/leviathan/systems/event_effects.hpp for the public
// contract (actor-selection policy, pre-flight atomicity,
// deliberate non-goals). This file is the small bridge that turns
// "fire this event instance" into "apply these PolicyEffects to
// this country" by delegating to the M1.5 / M5.6-extracted
// `policy::apply_effects_to_actor` helper.

#include "leviathan/systems/event_effects.hpp"

#include <cstddef>
#include <string>
#include <utility>

#include "leviathan/core/ids.hpp"
#include "leviathan/systems/policy_system.hpp"

namespace leviathan::systems::event_effects {
namespace {

// Resolve a country id_code to a CountryId by linear scan of
// state.countries. Returns CountryId::invalid() if no match.
// Linear scan is fine: state.countries is canonically small and
// this is not a per-tick hot path.
core::CountryId find_country_by_id_code(const core::GameState& state,
                                        const std::string&     id_code) {
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        if (state.countries[i].id_code == id_code) {
            return state.countries[i].id;
        }
    }
    return core::CountryId::invalid();
}

}  // namespace

core::Result<ApplyOutcome> apply_event_effects(
        core::GameState&             state,
        const core::EventInstance&   instance,
        const core::EventDefinition& definition) {
    // Vacuous-actor case: a hand-built EventInstance with no
    // actors (mirrors the M5.5 empty-triggers vacuous fire). No
    // actor means no country to direct effects at; return
    // success with 0 applied. The caller can detect this via
    // outcome.effects_applied == 0.
    if (instance.actors.empty()) {
        return core::Result<ApplyOutcome>::success(ApplyOutcome{});
    }

    // Resolve the first actor's owning country to a CountryId.
    // M5.6 uses the FIRST actor's country for ALL effects; see
    // the header doc for the rationale.
    const auto& head_country_id_code = instance.actors.front().country_id_code;
    if (head_country_id_code.empty()) {
        return core::Result<ApplyOutcome>::failure(
            "apply_event_effects: '" + instance.event_id_code +
            "': first actor has empty country_id_code"
            " (state was internally inconsistent at fire time)");
    }
    const core::CountryId actor =
        find_country_by_id_code(state, head_country_id_code);
    if (!actor.valid()) {
        return core::Result<ApplyOutcome>::failure(
            "apply_event_effects: '" + instance.event_id_code +
            "': first actor's country_id_code '" + head_country_id_code +
            "' does not resolve to any country in state.countries");
    }

    // Delegate to the shared M1.5 / M5.6-extracted helper. M1.5
    // pre-flight atomicity is inherited: any per-effect failure
    // leaves state untouched.
    auto inner = policy::apply_effects_to_actor(state, actor,
                                                definition.effects);
    if (!inner) {
        return core::Result<ApplyOutcome>::failure(std::move(inner.error()));
    }
    const auto& v = inner.value();
    ApplyOutcome out;
    out.effects_applied         = v.effects_applied;
    out.faction_targets_updated = v.faction_targets_updated;
    return core::Result<ApplyOutcome>::success(std::move(out));
}

}  // namespace leviathan::systems::event_effects

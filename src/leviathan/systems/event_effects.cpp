// EventEffects implementation. Post-M6.7 hardening sweep applies
// the project-wide `feedback_no_silent_degradation` rule: vacuous-
// actor branches return `Result::failure` rather than silently
// returning success-with-0-applied; `resolve_followup_ids` rejects
// unresolvable id_codes rather than silently skipping;
// `select_best_option_for_country` returns `Result<const EventOption*>`
// so `effect_desire::for_country` failures propagate.

#include "leviathan/systems/event_effects.hpp"

#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "leviathan/core/ids.hpp"
#include "leviathan/systems/effect_desire.hpp"
#include "leviathan/systems/policy_system.hpp"

namespace leviathan::systems::event_effects {
namespace {

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
    if (instance.actors.empty()) {
        // Post-M6.7 strict-fallback hardening: a structurally
        // inconsistent EventInstance (no actors) is now a hard
        // error rather than a silent success-with-0-applied.
        return core::Result<ApplyOutcome>::failure(
            "apply_event_effects: '" + instance.event_id_code +
            "': EventInstance has no actors — structurally"
            " inconsistent input (engine path always produces"
            " at least one actor)");
    }

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

const core::EventOption*
select_default_option(const core::EventDefinition& definition) {
    if (definition.options.empty()) {
        return nullptr;
    }
    return &definition.options.front();
}

core::Result<ApplyOutcome> apply_default_option_effects(
        core::GameState&             state,
        const core::EventInstance&   instance,
        const core::EventDefinition& definition) {
    const auto* option = select_default_option(definition);
    if (option == nullptr) {
        // No options to apply: structural "nothing to do" — not a
        // numeric / runtime abnormality. Success with 0 is the
        // correct semantic.
        return core::Result<ApplyOutcome>::success(ApplyOutcome{});
    }
    if (instance.actors.empty()) {
        // Post-M6.7 strict-fallback hardening (mirrors
        // apply_event_effects): vacuous actors → failure.
        return core::Result<ApplyOutcome>::failure(
            "apply_default_option_effects: '" + instance.event_id_code +
            "': EventInstance has no actors — structurally"
            " inconsistent input");
    }

    const auto& head_country_id_code = instance.actors.front().country_id_code;
    if (head_country_id_code.empty()) {
        return core::Result<ApplyOutcome>::failure(
            "apply_default_option_effects: '" + instance.event_id_code +
            "': first actor has empty country_id_code");
    }
    const core::CountryId actor =
        find_country_by_id_code(state, head_country_id_code);
    if (!actor.valid()) {
        return core::Result<ApplyOutcome>::failure(
            "apply_default_option_effects: '" + instance.event_id_code +
            "': first actor's country_id_code '" + head_country_id_code +
            "' does not resolve to any country in state.countries");
    }

    auto inner = policy::apply_effects_to_actor(state, actor,
                                                option->effects);
    if (!inner) {
        return core::Result<ApplyOutcome>::failure(std::move(inner.error()));
    }
    const auto& v = inner.value();
    ApplyOutcome out;
    out.effects_applied         = v.effects_applied;
    out.faction_targets_updated = v.faction_targets_updated;
    return core::Result<ApplyOutcome>::success(std::move(out));
}

core::Result<const core::EventOption*>
select_best_option_for_country(const core::GameState&       state,
                               const core::CountryState&    country,
                               const core::EventDefinition& definition) {
    if (definition.options.empty()) {
        // "No options to score" is a structural input; success-
        // with-nullptr lets the caller switch to the fallback
        // path (base-effects-only). Not a numeric abnormality.
        return core::Result<const core::EventOption*>::success(nullptr);
    }
    double      best_score = -std::numeric_limits<double>::infinity();
    std::size_t best_index = 0;
    for (std::size_t i = 0; i < definition.options.size(); ++i) {
        const auto& opt = definition.options[i];
        double score = 0.0;
        for (const auto& eff : opt.effects) {
            if (eff.op != "add") { continue; }
            if (eff.target.rfind("country.", 0) == 0) {
                auto desire_r = effect_desire::for_country(
                    country, eff.target, state);
                if (!desire_r) {
                    return core::Result<const core::EventOption*>::failure(
                        std::move(desire_r.error()));
                }
                score += eff.value * desire_r.value();
            }
        }
        if (i == 0 || score > best_score) {
            best_score = score;
            best_index = i;
        }
        // Strict `>` preserves the lower-index winner on ties
        // (deterministic vector-order tie-break).
    }
    return core::Result<const core::EventOption*>::success(
        &definition.options[best_index]);
}

namespace {
core::Result<core::CountryId>
resolve_head_actor_country(const core::GameState&     state,
                           const core::EventInstance& instance,
                           const char*                tag) {
    if (instance.actors.empty()) {
        // Post-M6.7 strict-fallback hardening: vacuous actors are
        // now an error here too (mirrors apply_event_effects).
        return core::Result<core::CountryId>::failure(
            std::string(tag) + ": '" + instance.event_id_code +
            "': EventInstance has no actors — structurally"
            " inconsistent input");
    }
    const auto& head_country_id_code = instance.actors.front().country_id_code;
    if (head_country_id_code.empty()) {
        return core::Result<core::CountryId>::failure(
            std::string(tag) + ": '" + instance.event_id_code +
            "': first actor has empty country_id_code");
    }
    const core::CountryId actor =
        find_country_by_id_code(state, head_country_id_code);
    if (!actor.valid()) {
        return core::Result<core::CountryId>::failure(
            std::string(tag) + ": '" + instance.event_id_code +
            "': first actor's country_id_code '" + head_country_id_code +
            "' does not resolve to any country in state.countries");
    }
    return core::Result<core::CountryId>::success(actor);
}
}  // namespace

core::Result<ApplyOutcome>
apply_option_effects_with_mode(
        core::GameState&                state,
        const core::EventInstance&      instance,
        const core::EventDefinition&    definition,
        const core::EventOption&        option,
        core::EventOptionEffectMode     mode) {
    auto actor_r = resolve_head_actor_country(
        state, instance, "apply_option_effects_with_mode");
    if (!actor_r) {
        return core::Result<ApplyOutcome>::failure(
            std::move(actor_r.error()));
    }
    const core::CountryId actor = actor_r.value();

    ApplyOutcome out;
    std::string apply_err;
    auto apply_one = [&](const std::vector<core::PolicyEffect>& effs) -> bool {
        auto inner =
            policy::apply_effects_to_actor(state, actor, effs);
        if (!inner) {
            apply_err = std::move(inner.error());
            return false;
        }
        out.effects_applied += inner.value().effects_applied;
        out.faction_targets_updated +=
            inner.value().faction_targets_updated;
        return true;
    };

    switch (mode) {
        case core::EventOptionEffectMode::OptionOnly:
            if (!apply_one(option.effects)) {
                return core::Result<ApplyOutcome>::failure(std::move(apply_err));
            }
            break;
        case core::EventOptionEffectMode::BaseThenOption:
            if (!apply_one(definition.effects) ||
                !apply_one(option.effects)) {
                return core::Result<ApplyOutcome>::failure(std::move(apply_err));
            }
            break;
        case core::EventOptionEffectMode::OptionThenBase:
            if (!apply_one(option.effects) ||
                !apply_one(definition.effects)) {
                return core::Result<ApplyOutcome>::failure(std::move(apply_err));
            }
            break;
    }

    return core::Result<ApplyOutcome>::success(std::move(out));
}

core::Result<std::vector<std::size_t>>
resolve_followup_ids(const core::GameState&       state,
                     const core::EventDefinition& definition) {
    std::vector<std::size_t> out;
    out.reserve(definition.followup_event_ids.size());
    for (const auto& id_code : definition.followup_event_ids) {
        std::size_t found_index = state.events.size();
        for (std::size_t i = 0; i < state.events.size(); ++i) {
            if (state.events[i].id_code == id_code) {
                found_index = i;
                break;
            }
        }
        if (found_index >= state.events.size()) {
            // Post-M6.7 strict-fallback hardening: unresolvable
            // followup id_codes now surface as a hard error rather
            // than being silently skipped. Authoring typos are
            // caught here.
            return core::Result<std::vector<std::size_t>>::failure(
                "resolve_followup_ids: definition '" + definition.id_code +
                "' references followup event id_code '" + id_code +
                "' which does not resolve to any entry in state.events");
        }
        out.push_back(found_index);
    }
    return core::Result<std::vector<std::size_t>>::success(std::move(out));
}

}  // namespace leviathan::systems::event_effects

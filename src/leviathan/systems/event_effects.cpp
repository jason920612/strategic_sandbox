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
#include <limits>
#include <string>
#include <utility>

#include "leviathan/core/ids.hpp"
#include "leviathan/systems/effect_desire.hpp"
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
        return core::Result<ApplyOutcome>::success(ApplyOutcome{});
    }
    if (instance.actors.empty()) {
        return core::Result<ApplyOutcome>::success(ApplyOutcome{});
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

// Issue #112: state-based option chooser. For non-player countries
// (or any deterministic headless caller), score each option by
// summing `effect.value * effect_desire::for_country(country, target)`
// over its country.* effects. Highest-scored wins; tie on equal
// score breaks by lower option vector index. Mirrors the
// score_policy pattern in ai_policy.cpp.
const core::EventOption*
select_best_option_for_country(const core::GameState&       state,
                               const core::CountryState&    country,
                               const core::EventDefinition& definition) {
    if (definition.options.empty()) {
        return nullptr;
    }
    double      best_score = -std::numeric_limits<double>::infinity();
    std::size_t best_index = 0;
    for (std::size_t i = 0; i < definition.options.size(); ++i) {
        const auto& opt = definition.options[i];
        double score = 0.0;
        for (const auto& eff : opt.effects) {
            if (eff.op != "add") { continue; }
            if (eff.target.rfind("country.", 0) == 0) {
                score += eff.value
                       * effect_desire::for_country(country, eff.target, state);
            }
        }
        if (i == 0 || score > best_score) {
            best_score = score;
            best_index = i;
        }
        // Strict `>` preserves the lower-index winner on ties
        // (deterministic vector-order tie-break).
    }
    return &definition.options[best_index];
}

namespace {
// Resolve the head actor's owning country to a CountryId for
// mode-aware apply. Mirrors apply_event_effects' guard logic.
core::Result<core::CountryId>
resolve_head_actor_country(const core::GameState&     state,
                           const core::EventInstance& instance,
                           const char*                tag) {
    if (instance.actors.empty()) {
        return core::Result<core::CountryId>::success(
            core::CountryId::invalid());
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
    if (!actor.valid()) {
        // No actors → success with 0 applied (mirrors the M5.6
        // vacuous-actor case for the base apply path).
        return core::Result<ApplyOutcome>::success(ApplyOutcome{});
    }

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

std::vector<std::size_t>
resolve_followup_ids(const core::GameState&       state,
                     const core::EventDefinition& definition) {
    std::vector<std::size_t> out;
    out.reserve(definition.followup_event_ids.size());
    for (const auto& id_code : definition.followup_event_ids) {
        for (std::size_t i = 0; i < state.events.size(); ++i) {
            if (state.events[i].id_code == id_code) {
                out.push_back(i);
                break;
            }
        }
    }
    return out;
}

}  // namespace leviathan::systems::event_effects

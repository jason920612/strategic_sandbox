// M5.2: EventEvaluator implementation.
// M5.3: actor-binding extension — the existing per-op / per-target
// dispatch grows a "which entity satisfied" return path so the
// future firer / effects-applicator knows where to direct an
// effect. See include/leviathan/systems/event_evaluator.hpp for
// the public contract.

#include "leviathan/systems/event_evaluator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace leviathan::systems::event_evaluator {
namespace {

// Numeric comparison for the M5.1 op allowlist. Returns false on
// any unknown op string and on any non-finite operand — keeps
// NaN out of the gate.
bool op_compare(const std::string& op, double lhs, double rhs) {
    if (!std::isfinite(lhs) || !std::isfinite(rhs)) {
        return false;
    }
    if (op == "lt")  { return lhs <  rhs; }
    if (op == "lte") { return lhs <= rhs; }
    if (op == "gt")  { return lhs >  rhs; }
    if (op == "gte") { return lhs >= rhs; }
    return false;
}

// Project a target string + a country into the country-side
// double it names. Returns nan for non-country-side leaves.
double pick_country_value(const std::string&        target,
                          const core::CountryState& c) {
    if (target == "country.stability")  { return c.stability;  }
    if (target == "country.legitimacy") { return c.legitimacy; }
    if (target == "country.government_authority.bureaucratic_compliance") {
        return c.government_authority.bureaucratic_compliance;
    }
    return std::nan("");
}

// Project a target string + an interest group into the IG-side
// double it names. Returns nan for non-IG targets.
double pick_interest_group_value(const std::string&              target,
                                 const core::InterestGroupState& g) {
    if (target == "interest_group.radicalism") { return g.radicalism; }
    if (target == "interest_group.loyalty")    { return g.loyalty;    }
    return std::nan("");
}

bool target_is_country_scope(const std::string& target) {
    return target == "country.stability"
        || target == "country.legitimacy"
        || target == "country.government_authority.bureaucratic_compliance";
}

bool target_is_interest_group_scope(const std::string& target) {
    return target == "interest_group.radicalism"
        || target == "interest_group.loyalty";
}

// M5.3: find the index of the first country (in vector order)
// that satisfies the trigger; returns std::nullopt if none do
// or if the target is not country-scoped.
std::optional<std::size_t>
first_country_index_satisfying(const core::GameState&    state,
                               const core::EventTrigger& trig) {
    if (!target_is_country_scope(trig.target)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const double v = pick_country_value(trig.target,
                                            state.countries[i]);
        if (op_compare(trig.op, v, trig.value)) {
            return i;
        }
    }
    return std::nullopt;
}

// M5.3: find the index of the first interest group (in vector
// order) that satisfies the trigger; returns std::nullopt if
// none do or if the target is not IG-scoped.
std::optional<std::size_t>
first_interest_group_index_satisfying(const core::GameState&    state,
                                      const core::EventTrigger& trig) {
    if (!target_is_interest_group_scope(trig.target)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < state.interest_groups.size(); ++i) {
        const double v = pick_interest_group_value(
            trig.target, state.interest_groups[i]);
        if (op_compare(trig.op, v, trig.value)) {
            return i;
        }
    }
    return std::nullopt;
}

}  // namespace

bool trigger_matches(const core::GameState&    state,
                     const core::EventTrigger& trig) {
    if (!std::isfinite(trig.value)) {
        return false;
    }
    if (target_is_country_scope(trig.target)) {
        return first_country_index_satisfying(state, trig).has_value();
    }
    if (target_is_interest_group_scope(trig.target)) {
        return first_interest_group_index_satisfying(state, trig).has_value();
    }
    return false;
}

std::optional<TriggerActor>
trigger_actor(const core::GameState&    state,
              const core::EventTrigger& trig) {
    if (!std::isfinite(trig.value)) {
        return std::nullopt;
    }
    if (target_is_country_scope(trig.target)) {
        const auto idx = first_country_index_satisfying(state, trig);
        if (!idx.has_value()) {
            return std::nullopt;
        }
        const auto& c = state.countries[*idx];
        TriggerActor a;
        a.kind    = TriggerActorKind::Country;
        a.id_code = c.id_code;
        a.country = c.id;
        a.index   = *idx;
        return a;
    }
    if (target_is_interest_group_scope(trig.target)) {
        const auto idx =
            first_interest_group_index_satisfying(state, trig);
        if (!idx.has_value()) {
            return std::nullopt;
        }
        const auto& g = state.interest_groups[*idx];
        TriggerActor a;
        a.kind    = TriggerActorKind::InterestGroup;
        a.id_code = g.id_code;
        a.country = g.country;
        a.index   = *idx;
        return a;
    }
    return std::nullopt;
}

bool evaluate(const core::GameState&        state,
              const core::EventDefinition&  def) {
    for (const auto& t : def.triggers) {
        if (!trigger_matches(state, t)) {
            return false;
        }
    }
    return true;
}

std::optional<EventMatch>
evaluate_match(const core::GameState&       state,
               const core::EventDefinition& def) {
    EventMatch m;
    m.event_index   = 0;             // caller fills if needed
    m.event_id_code = def.id_code;
    m.triggers.reserve(def.triggers.size());
    for (std::size_t ti = 0; ti < def.triggers.size(); ++ti) {
        auto actor_opt = trigger_actor(state, def.triggers[ti]);
        if (!actor_opt.has_value()) {
            return std::nullopt;
        }
        TriggerEvaluation te;
        te.trigger_index = ti;
        te.actor         = std::move(*actor_opt);
        m.triggers.push_back(std::move(te));
    }
    return m;
}

std::vector<EventMatch> match_events(const core::GameState& state) {
    std::vector<EventMatch> out;
    out.reserve(state.events.size());
    for (std::size_t i = 0; i < state.events.size(); ++i) {
        const auto& def = state.events[i];
        auto m = evaluate_match(state, def);
        if (m.has_value()) {
            m->event_index = i;
            out.push_back(std::move(*m));
        }
    }
    return out;
}

std::vector<WeightedEventCandidate>
rank_weighted_events(const core::GameState& state) {
    std::vector<WeightedEventCandidate> out;
    out.reserve(state.events.size());

    for (std::size_t i = 0; i < state.events.size(); ++i) {
        const auto& def = state.events[i];
        double weight = kBaseWeight;
        for (const auto& wm : def.weight_modifiers) {
            // Reuse the trigger_matches machinery: treat the
            // modifier as a transient EventTrigger with the same
            // target / op / value. trigger_matches returns false
            // for unrecognised target / op / non-finite value, so
            // those modifiers contribute zero — author typos are
            // silent here (same defensive-false convention as
            // M5.2; the M5.1 / RCR-1 save layer rejects malformed
            // fixtures at load time).
            core::EventTrigger probe;
            probe.target = wm.target;
            probe.op     = wm.op;
            probe.value  = wm.value;
            if (trigger_matches(state, probe)) {
                weight += wm.weight_delta;
            }
        }
        WeightedEventCandidate c;
        c.event_index   = i;
        c.event_id_code = def.id_code;
        c.weight        = weight;
        out.push_back(std::move(c));
    }

    // Stable sort by descending weight; ties retain original
    // vector order (std::stable_sort with a `>` comparator
    // achieves that without an explicit secondary key).
    std::stable_sort(out.begin(), out.end(),
                     [](const WeightedEventCandidate& a,
                        const WeightedEventCandidate& b) {
                         return a.weight > b.weight;
                     });
    return out;
}

}  // namespace leviathan::systems::event_evaluator

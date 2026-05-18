// M5.2: EventEvaluator implementation.
//
// See include/leviathan/systems/event_evaluator.hpp for the public
// contract (semantics, allowlists, aggregation rules, deliberate
// non-goals). This file is the small dispatcher behind it: a per-op
// numeric compare, a per-target state-slice selector with "any
// entity satisfies" semantics, and the AND-across-triggers fold.

#include "leviathan/systems/event_evaluator.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace leviathan::systems::event_evaluator {
namespace {

// Numeric comparison for the M5.1 op allowlist. Returns false on
// any unknown op string and on any non-finite operand — keeps NaN
// out of the gate (per the header comment, NaN never matches).
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

// Project a target string + a country into the country-side double
// it names. Returns std::nan if the target is not a recognised
// country-side leaf; the caller treats nan as "skip this entity".
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

}  // namespace

bool trigger_matches(const core::GameState&    state,
                     const core::EventTrigger& trig) {
    if (!std::isfinite(trig.value)) {
        return false;
    }

    if (target_is_country_scope(trig.target)) {
        for (const auto& c : state.countries) {
            const double v = pick_country_value(trig.target, c);
            if (op_compare(trig.op, v, trig.value)) {
                return true;
            }
        }
        return false;
    }

    if (target_is_interest_group_scope(trig.target)) {
        for (const auto& g : state.interest_groups) {
            const double v = pick_interest_group_value(trig.target, g);
            if (op_compare(trig.op, v, trig.value)) {
                return true;
            }
        }
        return false;
    }

    // Target not in the M5.1 allowlist — defensive false. The
    // M5.1 loader is the gate; the evaluator does not duplicate
    // the allowlist error messaging.
    return false;
}

bool evaluate(const core::GameState&        state,
              const core::EventDefinition&  def) {
    // Empty triggers: vacuously true (the M5.1 loader rejects
    // empty triggers, so this case is unreachable through canonical
    // load paths — pinned by a test for defensive readers).
    for (const auto& t : def.triggers) {
        if (!trigger_matches(state, t)) {
            return false;
        }
    }
    return true;
}

std::vector<TriggerMatch> match_events(const core::GameState& state) {
    std::vector<TriggerMatch> out;
    out.reserve(state.events.size());
    for (std::size_t i = 0; i < state.events.size(); ++i) {
        const auto& def = state.events[i];
        if (evaluate(state, def)) {
            TriggerMatch m;
            m.event_index   = i;
            m.event_id_code = def.id_code;
            out.push_back(std::move(m));
        }
    }
    return out;
}

}  // namespace leviathan::systems::event_evaluator

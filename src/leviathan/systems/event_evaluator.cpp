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

// M7.2 (RFC-090 §7.2): project a target string + a faction
// into the faction-side double it names. Returns nan for
// non-faction targets. Only `faction.radicalism` is in the
// M7.2 allowlist; `faction.loyalty` / `faction.support` /
// `faction.influence` remain unreachable until a future
// sub-milestone widens the allowlist.
double pick_faction_value(const std::string&        target,
                          const core::FactionState& f) {
    if (target == "faction.radicalism") { return f.radicalism; }
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

// M7.2 (RFC-090 §7.2): faction.* targets bind to
// `state.factions`. Only one field is in the M7.2 allowlist;
// the predicate is small but documented as a separate function
// for parity with the country / interest_group scope predicates.
bool target_is_faction_scope(const std::string& target) {
    return target == "faction.radicalism";
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

// M7.2: find the index of the first faction (in vector order)
// that satisfies the trigger; returns std::nullopt if none do
// or if the target is not faction-scoped. Mirrors the M5.3
// IG variant in shape so the existing ANY-entity-satisfies
// aggregation stays consistent.
std::optional<std::size_t>
first_faction_index_satisfying(const core::GameState&    state,
                               const core::EventTrigger& trig) {
    if (!target_is_faction_scope(trig.target)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < state.factions.size(); ++i) {
        const double v = pick_faction_value(
            trig.target, state.factions[i]);
        if (op_compare(trig.op, v, trig.value)) {
            return i;
        }
    }
    return std::nullopt;
}

// Issue #112: per-country scoped trigger satisfaction. Returns the
// SAME country's index when target is country-scoped (provided that
// country actually satisfies the predicate); returns the first
// matching IG index restricted to IGs whose owning country == cid
// for IG-scoped triggers. Used by `match_events_for_country` so an
// IG in FRA doesn't make GER's event pool match.
std::optional<std::size_t>
country_index_satisfying_for(const core::GameState&    state,
                             core::CountryId           cid,
                             const core::EventTrigger& trig) {
    if (!target_is_country_scope(trig.target)) {
        return std::nullopt;
    }
    if (!cid.valid()) { return std::nullopt; }
    const auto idx = static_cast<std::size_t>(cid.value());
    if (idx >= state.countries.size()) {
        return std::nullopt;
    }
    const double v = pick_country_value(trig.target, state.countries[idx]);
    if (op_compare(trig.op, v, trig.value)) {
        return idx;
    }
    return std::nullopt;
}

std::optional<std::size_t>
ig_index_satisfying_for(const core::GameState&    state,
                        core::CountryId           cid,
                        const core::EventTrigger& trig) {
    if (!target_is_interest_group_scope(trig.target)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < state.interest_groups.size(); ++i) {
        const auto& g = state.interest_groups[i];
        if (g.country != cid) { continue; }
        const double v = pick_interest_group_value(trig.target, g);
        if (op_compare(trig.op, v, trig.value)) {
            return i;
        }
    }
    return std::nullopt;
}

// M7.2 (RFC-090 §7.2): per-country scoped faction satisfaction.
// Mirrors `ig_index_satisfying_for` shape so issue #112's
// "one country's match cannot bleed into another's pool" rule
// continues to hold for the new faction-scoped trigger.
std::optional<std::size_t>
faction_index_satisfying_for(const core::GameState&    state,
                             core::CountryId           cid,
                             const core::EventTrigger& trig) {
    if (!target_is_faction_scope(trig.target)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < state.factions.size(); ++i) {
        const auto& f = state.factions[i];
        if (f.country != cid) { continue; }
        const double v = pick_faction_value(trig.target, f);
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
    if (target_is_faction_scope(trig.target)) {
        return first_faction_index_satisfying(state, trig).has_value();
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
    if (target_is_faction_scope(trig.target)) {
        const auto idx =
            first_faction_index_satisfying(state, trig);
        if (!idx.has_value()) {
            return std::nullopt;
        }
        const auto& f = state.factions[*idx];
        TriggerActor a;
        a.kind    = TriggerActorKind::Faction;
        a.id_code = f.id_code;
        a.country = f.country;
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

// Issue #112: per-country event-match scoping. Each event evaluates
// against ONE country only:
//   * `country.*` triggers must hold for state.countries[country_id]
//     specifically (NOT any other country).
//   * `interest_group.*` triggers must hold for some IG whose owning
//     `country == country_id` (NOT any IG in any country).
//   * Mixed-trigger events require BOTH sides to bind within the
//     same country.
// One country's match does NOT suppress another's — each country
// gets its own draw pool. Same event template may match for
// multiple countries independently.
std::optional<EventMatch>
evaluate_match_for_country(const core::GameState&       state,
                           core::CountryId              country_id,
                           const core::EventDefinition& def) {
    EventMatch m;
    m.event_index   = 0;
    m.event_id_code = def.id_code;
    m.triggers.reserve(def.triggers.size());
    for (std::size_t ti = 0; ti < def.triggers.size(); ++ti) {
        const auto& trig = def.triggers[ti];
        if (!std::isfinite(trig.value)) {
            return std::nullopt;
        }
        TriggerEvaluation te;
        te.trigger_index = ti;
        if (target_is_country_scope(trig.target)) {
            const auto idx = country_index_satisfying_for(state, country_id, trig);
            if (!idx.has_value()) {
                return std::nullopt;
            }
            const auto& c = state.countries[*idx];
            te.actor.kind    = TriggerActorKind::Country;
            te.actor.id_code = c.id_code;
            te.actor.country = c.id;
            te.actor.index   = *idx;
        } else if (target_is_interest_group_scope(trig.target)) {
            const auto idx = ig_index_satisfying_for(state, country_id, trig);
            if (!idx.has_value()) {
                return std::nullopt;
            }
            const auto& g = state.interest_groups[*idx];
            te.actor.kind    = TriggerActorKind::InterestGroup;
            te.actor.id_code = g.id_code;
            te.actor.country = g.country;
            te.actor.index   = *idx;
        } else if (target_is_faction_scope(trig.target)) {
            // M7.2 (RFC-090 §7.2): faction.* triggers bind to
            // factions whose owning country matches `country_id`.
            // Per-country scoping mirrors the interest_group.*
            // path so issue #112's "one country's match cannot
            // bleed into another's pool" rule continues to hold.
            const auto idx =
                faction_index_satisfying_for(state, country_id, trig);
            if (!idx.has_value()) {
                return std::nullopt;
            }
            const auto& f = state.factions[*idx];
            te.actor.kind    = TriggerActorKind::Faction;
            te.actor.id_code = f.id_code;
            te.actor.country = f.country;
            te.actor.index   = *idx;
        } else {
            return std::nullopt;
        }
        m.triggers.push_back(std::move(te));
    }
    return m;
}

std::vector<EventMatch>
match_events_for_country(const core::GameState& state,
                         core::CountryId        country_id) {
    std::vector<EventMatch> out;
    if (!country_id.valid()
        || static_cast<std::size_t>(country_id.value())
               >= state.countries.size()) {
        return out;
    }
    out.reserve(state.events.size());
    for (std::size_t i = 0; i < state.events.size(); ++i) {
        const auto& def = state.events[i];
        auto m = evaluate_match_for_country(state, country_id, def);
        if (m.has_value()) {
            m->event_index = i;
            out.push_back(std::move(*m));
        }
    }
    return out;
}

core::Result<std::vector<WeightedEventCandidate>>
rank_weighted_events(const core::GameState& state) {
    using R = core::Result<std::vector<WeightedEventCandidate>>;
    std::vector<WeightedEventCandidate> out;
    out.reserve(state.events.size());

    // Post-M6.7 hardening (`feedback_no_silent_degradation`):
    // every modifier is explicitly validated. The previous
    // "trigger_matches returns false for unrecognised target
    // / op / non-finite value, so the modifier contributes
    // zero" silent-skip path is gone — author typos and
    // runtime non-finite weight_delta now `Result::failure`
    // loudly. The M5.1 / RCR-1 save layer still rejects
    // malformed fixtures at load time; this is the runtime
    // line of defence.
    const auto valid_op = [](const std::string& op) {
        return op == "lt" || op == "lte" || op == "gt" || op == "gte";
    };
    const auto valid_target = [](const std::string& target) {
        return target_is_country_scope(target)
            || target_is_interest_group_scope(target)
            || target_is_faction_scope(target);  // M7.2 (RFC-090 §7.2)
    };

    for (std::size_t i = 0; i < state.events.size(); ++i) {
        const auto& def = state.events[i];
        double weight = kBaseWeight;
        for (std::size_t mi = 0; mi < def.weight_modifiers.size(); ++mi) {
            const auto& wm = def.weight_modifiers[mi];
            if (!std::isfinite(wm.weight_delta)) {
                return R::failure(
                    "rank_weighted_events: event '" + def.id_code +
                    "' weight_modifiers[" + std::to_string(mi) +
                    "].weight_delta is not finite");
            }
            if (!std::isfinite(wm.value)) {
                return R::failure(
                    "rank_weighted_events: event '" + def.id_code +
                    "' weight_modifiers[" + std::to_string(mi) +
                    "].value is not finite");
            }
            if (!valid_target(wm.target)) {
                return R::failure(
                    "rank_weighted_events: event '" + def.id_code +
                    "' weight_modifiers[" + std::to_string(mi) +
                    "].target = '" + wm.target +
                    "' is not in the country / interest_group /"
                    " faction allowlist");
            }
            if (!valid_op(wm.op)) {
                return R::failure(
                    "rank_weighted_events: event '" + def.id_code +
                    "' weight_modifiers[" + std::to_string(mi) +
                    "].op = '" + wm.op +
                    "' is not in {lt, lte, gt, gte}");
            }
            core::EventTrigger probe;
            probe.target = wm.target;
            probe.op     = wm.op;
            probe.value  = wm.value;
            if (trigger_matches(state, probe)) {
                weight += wm.weight_delta;
            }
        }
        if (!std::isfinite(weight)) {
            return R::failure(
                "rank_weighted_events: event '" + def.id_code +
                "' accumulated weight is not finite (" +
                std::to_string(weight) + ")");
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
    return R::success(std::move(out));
}

core::Result<std::optional<WeightedEventCandidate>>
select_weighted_event(const core::GameState& state) {
    using R = core::Result<std::optional<WeightedEventCandidate>>;
    const auto matches = match_events(state);
    if (matches.empty()) {
        return R::success(std::nullopt);
    }

    // Build a small lookup of currently-matched event indices.
    // state.events is canonically small (~10 in RCR-1 fixtures),
    // so a linear scan per ranked candidate is cheaper than a
    // hash set allocation here.
    //
    // Post-M6.7 hardening: a `rank_weighted_events` failure
    // (malformed weight modifier, non-finite weight, etc.)
    // MUST propagate as a Result failure — the previous
    // `return std::nullopt` shape silently swallowed the
    // problem and is forbidden by
    // `feedback_no_silent_degradation`.
    auto ranked_r = rank_weighted_events(state);
    if (!ranked_r) {
        return R::failure(std::move(ranked_r.error()));
    }
    for (const auto& cand : ranked_r.value()) {
        for (const auto& m : matches) {
            if (m.event_index == cand.event_index) {
                return R::success(cand);
            }
        }
    }
    return R::success(std::nullopt);
}

}  // namespace leviathan::systems::event_evaluator

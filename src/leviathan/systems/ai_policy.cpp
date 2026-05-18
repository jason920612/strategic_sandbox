#include "leviathan/systems/ai_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>

#include "leviathan/systems/policy_system.hpp"

namespace leviathan::systems::ai_policy {
namespace {

// "Desire to move target up" for a country at the given effect target.
// Positive = country benefits when the value rises; negative = country
// benefits when the value falls; 0 = neutral.
//
// Reads CountryState fields (RFC-090 §3.5/§3.8) plus
// GameState::relationships (RFC-090 §3.6/§3.7) for the military-power
// term — the load-bearing place where relationships and military_strength
// feed AI behaviour rather than sitting as inert data.
double target_desire(const core::CountryState& c,
                     const std::string&        target,
                     const core::GameState&    state) {
    if (target == "country.stability")  { return 1.0 - c.stability;  }
    if (target == "country.legitimacy") { return 1.0 - c.legitimacy; }
    if (target == "country.administrative_efficiency") {
        return 1.0 - c.administrative_efficiency;
    }
    if (target == "country.fiscal_capacity") {
        return 1.0 - c.fiscal_capacity;
    }
    if (target == "country.central_control") {
        // Moderate desire: countries want central_control in a healthy
        // mid-range, not maximised. Saturates at zero once already
        // above 0.6 so high-control authoritarian states don't
        // over-centralise.
        return std::max(0.0, 0.6 - c.central_control);
    }
    if (target == "country.corruption") {
        // Bad-axis: country wants this DOWN. Negative desire means a
        // negative-value effect on corruption (i.e. cutting it)
        // scores positively.
        return -c.corruption;
    }
    if (target == "country.budget_balance") {
        // Normalise against a GDP-tied scale so the term doesn't
        // dominate, then clamp to [0, 1] so a severe deficit doesn't
        // blow up the score relative to other in-range pressures.
        const double scale = std::max(std::abs(c.gdp) * 0.01, 1.0);
        const double raw   = -c.budget_balance / scale;
        return std::min(1.0, std::max(0.0, raw));
    }
    if (target == "country.legal_tax_burden") {
        // Mixed knob: deficits create desire to raise; an already-
        // high tax burden creates desire to lower. Budget pressure
        // clamped to [0, 1] for the same reason as country.budget_balance.
        const double scale = std::max(std::abs(c.gdp) * 0.01, 1.0);
        const double raw_budget = -c.budget_balance / scale;
        const double budget_pressure =
            std::min(1.0, std::max(0.0, raw_budget));
        return budget_pressure - c.legal_tax_burden * 0.5;
    }
    if (target == "country.military_power") {
        // Effective threat = max(threat_perception, max incoming
        // relationship threat). This is where the scorer reads
        // GameState::relationships (RFC-090 §3.6 / §3.7) — the
        // RFC-required wiring that issue #110 §1 demanded.
        double effective_threat = c.threat_perception;
        for (const auto& rel : state.relationships) {
            if (rel.to == c.id && rel.threat > effective_threat) {
                effective_threat = rel.threat;
            }
        }
        // Plus military_strength disparity: hostile neighbour
        // stronger than us → desire to militarise rises (RFC-090
        // §3.8 — the load-bearing read of military_strength).
        double max_neighbour_str = 0.0;
        for (const auto& rel : state.relationships) {
            if (rel.to == c.id && rel.from.valid()) {
                const auto fidx =
                    static_cast<std::size_t>(rel.from.value());
                if (fidx < state.countries.size()) {
                    const double s = state.countries[fidx].military_strength;
                    if (s > max_neighbour_str) {
                        max_neighbour_str = s;
                    }
                }
            }
        }
        const double military_gap =
            std::max(0.0, max_neighbour_str - c.military_strength);
        // Normalise gap by 100 (typical compliance-fixture scale
        // is 10..90); clamp to [0, 1].
        const double military_gap_norm = std::min(1.0, military_gap / 100.0);
        return effective_threat + 0.5 * military_gap_norm;
    }
    // Unknown country target (e.g. country.gdp, country.tax_revenue
    // — not directly addressed by policies in the current fixtures):
    // neutral.
    return 0.0;
}

struct InterestGroupAggregate {
    double mean_radicalism_inf_weighted = 0.0;  // 0..1
    double mean_loyalty                 = 0.5;  // 0..1
    int    count                        = 0;
};

InterestGroupAggregate
aggregate_country_igs(const core::CountryState& c,
                      const core::GameState&    state) {
    InterestGroupAggregate out;
    double total_influence    = 0.0;
    double weighted_radicalism = 0.0;
    double total_loyalty      = 0.0;
    for (const auto& ig : state.interest_groups) {
        if (ig.country == c.id) {
            ++out.count;
            total_influence    += ig.influence;
            weighted_radicalism += ig.influence * ig.radicalism;
            total_loyalty      += ig.loyalty;
        }
    }
    if (total_influence > 0.0) {
        out.mean_radicalism_inf_weighted = weighted_radicalism / total_influence;
    }
    if (out.count > 0) {
        out.mean_loyalty = total_loyalty / out.count;
    }
    return out;
}

// Compute a policy's score for a given country. Higher is better; on
// equal scores the caller breaks the tie by lower vector index.
double score_policy(const core::CountryState& c,
                    const core::PolicyData&   p,
                    const core::GameState&    state) {
    double score = 0.0;

    // Effect-driven scoring: each country-targeted effect contributes
    // `value × desire`. Faction-side effects (legacy M1.2 surface) are
    // ignored — M3.1 InterestGroupState is the modern actor model.
    for (const auto& eff : p.effects) {
        if (eff.op != "add") {
            continue;
        }
        if (eff.target.rfind("country.", 0) == 0) {
            score += eff.value * target_desire(c, eff.target, state);
        }
    }

    // Interest-group pressure: RFC-040 §4 / RFC-090 §3.5 expect AI
    // behaviour to reflect "派系利益". Aggregate radicalism (influence-
    // weighted) boosts concession-flavoured policies and penalises
    // crackdown ids; low loyalty penalises centralisation.
    const auto ig = aggregate_country_igs(c, state);
    if (p.category == "welfare" || p.category == "labor") {
        score += ig.mean_radicalism_inf_weighted * 0.7;
    }
    if (p.id_code == "press_censorship" ||
        p.id_code == "security_tightening") {
        score -= ig.mean_radicalism_inf_weighted * 0.5;
    }
    if (p.id_code == "centralize_authority") {
        score -= (1.0 - ig.mean_loyalty) * 0.5;
    }

    // Category baselines for policies whose effects don't directly
    // touch a country.* target but whose category names a pressure.
    if (p.category == "intelligence") {
        score += c.threat_perception * 0.20;
    }
    if (p.category == "media") {
        score += (1.0 - c.legitimacy) * 0.10;
    }
    if (p.id_code == "corruption_crackdown") {
        score += c.corruption * 0.30;
    }

    return score;
}

bool has_unexpired_policy(const core::CountryState& c,
                          const std::string&        id_code,
                          const core::GameDate&     current_date) {
    for (const auto& ap : c.active_policies) {
        if (ap.policy_id_code == id_code && current_date < ap.expires_on) {
            return true;
        }
    }
    return false;
}

// Pick the highest-scoring policy that isn't already active+unexpired
// on the country. Returns nullopt if no eligible policy exists.
std::optional<std::string>
pick_policy_for_country(const core::CountryState& c,
                        const core::GameState&    state) {
    if (state.policies.empty()) {
        return std::nullopt;
    }
    double      best_score = -std::numeric_limits<double>::infinity();
    std::size_t best_index = state.policies.size();  // sentinel: none
    for (std::size_t i = 0; i < state.policies.size(); ++i) {
        const auto& p = state.policies[i];
        if (has_unexpired_policy(c, p.id_code, state.current_date)) {
            continue;
        }
        const double s = score_policy(c, p, state);
        if (s > best_score) {
            best_score = s;
            best_index = i;
        }
        // Tie-break: strict `>` keeps the FIRST policy (lower vector
        // index) on equal scores — deterministic vector-order rule.
    }
    if (best_index >= state.policies.size()) {
        return std::nullopt;
    }
    return state.policies[best_index].id_code;
}

}  // namespace

core::Result<std::vector<Selection>>
select_policies(const core::GameState& state) {
    std::vector<Selection> out;

    if (state.countries.empty() || state.policies.empty()) {
        return core::Result<std::vector<Selection>>::success(std::move(out));
    }

    const core::CountryId player = state.player_country;
    const bool player_valid = player.valid()
        && static_cast<std::size_t>(player.value()) < state.countries.size();

    out.reserve(state.countries.size());
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const auto& country = state.countries[i];
        const core::CountryId cid{
            static_cast<core::CountryId::underlying_type>(i)};
        if (player_valid && cid == player) {
            continue;
        }
        auto pick = pick_policy_for_country(country, state);
        if (!pick.has_value()) {
            // No eligible candidate — counted as `skipped` by the
            // apply path; no Selection emitted.
            continue;
        }
        out.push_back(Selection{cid, std::move(*pick)});
    }

    return core::Result<std::vector<Selection>>::success(std::move(out));
}

core::Result<ApplyOutcome>
apply_selected_policies(core::GameState& state) {
    ApplyOutcome outcome;

    // `considered` counts every non-player country we scanned, not
    // every Selection we emit — so skipped-everything-active still
    // appears in `skipped`.
    const core::CountryId player = state.player_country;
    const bool player_valid = player.valid()
        && static_cast<std::size_t>(player.value()) < state.countries.size();
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const core::CountryId cid{
            static_cast<core::CountryId::underlying_type>(i)};
        if (player_valid && cid == player) {
            continue;
        }
        outcome.considered += 1;
    }

    auto sel_r = select_policies(state);
    if (!sel_r) {
        return core::Result<ApplyOutcome>::failure(std::move(sel_r.error()));
    }
    const auto& selections = sel_r.value();

    // Anyone considered but absent from the selections list was
    // skipped (no eligible policy).
    outcome.skipped = outcome.considered - selections.size();

    for (const auto& sel : selections) {
        const core::PolicyData* policy = nullptr;
        for (const auto& p : state.policies) {
            if (p.id_code == sel.policy_id_code) {
                policy = &p;
                break;
            }
        }
        if (policy == nullptr) {
            outcome.skipped += 1;
            continue;
        }
        auto apply_r = policy::apply_policy_effects(
            state, sel.country, *policy);
        if (!apply_r) {
            outcome.failed_countries.push_back(sel.country);
            continue;
        }
        outcome.applied += 1;
    }

    return core::Result<ApplyOutcome>::success(std::move(outcome));
}

}  // namespace leviathan::systems::ai_policy

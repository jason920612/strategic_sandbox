#include "leviathan/systems/ai_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>

#include "leviathan/systems/effect_desire.hpp"
#include "leviathan/systems/policy_system.hpp"

namespace leviathan::systems::ai_policy {
namespace {

// Issue #112: pressure threshold for AI policy selection. Below
// this total pressure (sum of normalised [0, 1] terms), a country
// emits ZERO selections this tick. Tuned so the compliance
// scenario (1930_rfc_compliance.json) at month 1 still exercises
// AI policy auto-apply for at least 25% of its 20 countries —
// see the calibration note in docs/rfc-090-010-compliance-audit.md
// §6.1 RFC-090 §3.5 entry.
constexpr double kPressureThreshold = 0.80;

constexpr double kCapacityLowMax    = 0.30;   // [0, 0.30)   → 1 policy
constexpr double kCapacityMediumMax = 0.60;   // [0.30, 0.60)→ 2 policies
                                              // [0.60, ∞)   → 3 policies

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

// Issue #112: compute the country's total pressure as a sum of six
// normalised [0, 1] terms (RFC-040 §4 factors). Above
// kPressureThreshold → AI emits selections; below → emits 0.
double compute_total_pressure(const core::CountryState& c,
                              const core::GameState&    state) {
    double p = 0.0;
    p += (1.0 - c.stability);      // [0, 1]
    p += (1.0 - c.legitimacy);     // [0, 1]
    p += c.corruption;             // [0, 1]

    // Budget deficit pressure, normalised by a GDP-tied scale,
    // clamped to [0, 1].
    const double scale = std::max(std::abs(c.gdp) * 0.01, 1.0);
    const double raw   = -c.budget_balance / scale;
    p += std::min(1.0, std::max(0.0, raw));

    // Threat pressure: max(threat_perception, max inbound
    // relationship threat). The military-disparity term is also
    // a threat pressure (a weak country next to a stronger
    // potential adversary feels pressure even with relationship
    // threat = 0). Mirrors the policy scorer's military_power
    // desire term so the gate and the picker stay aligned.
    double effective_threat = c.threat_perception;
    for (const auto& rel : state.relationships) {
        if (rel.to == c.id && rel.threat > effective_threat) {
            effective_threat = rel.threat;
        }
    }
    double max_neighbour_str = 0.0;
    for (const auto& rel : state.relationships) {
        if (rel.to == c.id && rel.from.valid()) {
            const auto fidx =
                static_cast<std::size_t>(rel.from.value());
            if (fidx < state.countries.size()) {
                const double sval = state.countries[fidx].military_strength;
                if (sval > max_neighbour_str) {
                    max_neighbour_str = sval;
                }
            }
        }
    }
    const double military_gap =
        std::max(0.0, max_neighbour_str - c.military_strength);
    const double military_gap_norm =
        std::min(1.0, military_gap / 100.0);
    p += std::min(1.0,
                  std::max(0.0,
                           effective_threat + 0.5 * military_gap_norm));

    // IG aggregate pressure: influence-weighted radicalism.
    const auto ig = aggregate_country_igs(c, state);
    p += std::min(1.0, std::max(0.0, ig.mean_radicalism_inf_weighted));

    return p;
}

// Issue #112: how many policies the country can enact this tick,
// gated by administrative_efficiency + bureaucratic_compliance +
// budget headroom. Returns 1 / 2 / 3.
std::size_t capacity_to_count(const core::CountryState& c) {
    const double scale = std::max(std::abs(c.gdp) * 0.01, 1.0);
    const double raw   = -c.budget_balance / scale;
    const double budget_pressure =
        std::min(1.0, std::max(0.0, raw));
    const double cap =
        0.5 * c.administrative_efficiency +
        0.3 * c.government_authority.bureaucratic_compliance +
        0.2 * std::max(0.0, 1.0 - budget_pressure);
    if (cap < kCapacityLowMax)    { return 1; }
    if (cap < kCapacityMediumMax) { return 2; }
    return 3;
}

// Score one policy for one country. Issue #112 keeps the same
// scorer the issue-#110 work shipped; capacity / threshold gates
// are layered on top in pick_top_k.
double score_policy(const core::CountryState& c,
                    const core::PolicyData&   p,
                    const core::GameState&    state) {
    double score = 0.0;
    for (const auto& eff : p.effects) {
        if (eff.op != "add") { continue; }
        if (eff.target.rfind("country.", 0) == 0) {
            score += eff.value
                   * effect_desire::for_country(c, eff.target, state);
        }
    }
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

// Pick the top-k highest-scoring policies that aren't already
// active+unexpired on the country AND aren't already in the
// picked set this tick (no-stack rule). Returns up to k id_codes
// in descending-score order; tie-break by lower vector index.
std::vector<std::string>
pick_top_k_for_country(const core::CountryState& c,
                       const core::GameState&    state,
                       std::size_t               k) {
    std::vector<std::string> out;
    if (state.policies.empty() || k == 0) {
        return out;
    }
    std::unordered_set<std::string> picked_this_tick;

    for (std::size_t round = 0; round < k; ++round) {
        double      best_score = -std::numeric_limits<double>::infinity();
        std::size_t best_index = state.policies.size();   // sentinel
        for (std::size_t i = 0; i < state.policies.size(); ++i) {
            const auto& p = state.policies[i];
            if (has_unexpired_policy(c, p.id_code, state.current_date)) {
                continue;
            }
            if (picked_this_tick.count(p.id_code) > 0) {
                continue;
            }
            const double s = score_policy(c, p, state);
            if (s > best_score) {
                best_score = s;
                best_index = i;
            }
            // Strict `>` keeps lower vector index on ties.
        }
        if (best_index >= state.policies.size()) {
            break;   // no eligible candidate left
        }
        out.push_back(state.policies[best_index].id_code);
        picked_this_tick.insert(state.policies[best_index].id_code);
    }
    return out;
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

    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const auto& country = state.countries[i];
        const core::CountryId cid{
            static_cast<core::CountryId::underlying_type>(i)};
        if (player_valid && cid == player) {
            continue;
        }

        // Issue #112: pressure gate. Below threshold → 0 selections.
        const double pressure = compute_total_pressure(country, state);
        if (pressure < kPressureThreshold) {
            continue;
        }

        // Capacity bound: 1 / 2 / 3 picks based on admin / bcomp / budget.
        const std::size_t k = capacity_to_count(country);
        auto picks = pick_top_k_for_country(country, state, k);
        for (auto& id_code : picks) {
            out.push_back(Selection{cid, std::move(id_code)});
        }
    }

    return core::Result<std::vector<Selection>>::success(std::move(out));
}

core::Result<ApplyOutcome>
apply_selected_policies(core::GameState& state) {
    ApplyOutcome outcome;

    const core::CountryId player = state.player_country;
    const bool player_valid = player.valid()
        && static_cast<std::size_t>(player.value()) < state.countries.size();

    // Count `considered` plus the pressure-gate skip-count and
    // the no-eligible-candidate skip-count. Walk the countries
    // ourselves so we can categorise each one before calling
    // select_policies — necessary because select_policies emits
    // a per-tick selection vector that doesn't distinguish "no
    // pressure" from "all candidates stacked".
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const core::CountryId cid{
            static_cast<core::CountryId::underlying_type>(i)};
        if (player_valid && cid == player) {
            continue;
        }
        outcome.considered += 1;

        const auto& country = state.countries[i];
        const double pressure = compute_total_pressure(country, state);
        if (pressure < kPressureThreshold) {
            outcome.pressure_below_threshold_skipped += 1;
            continue;
        }
        const std::size_t k = capacity_to_count(country);
        const auto picks = pick_top_k_for_country(country, state, k);
        if (picks.empty()) {
            outcome.skipped += 1;
        }
    }

    auto sel_r = select_policies(state);
    if (!sel_r) {
        return core::Result<ApplyOutcome>::failure(std::move(sel_r.error()));
    }
    const auto& selections = sel_r.value();

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

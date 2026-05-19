#include "leviathan/systems/ai_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

#include "internal/numeric_guards.hpp"
#include "leviathan/systems/effect_desire.hpp"
#include "leviathan/systems/policy_system.hpp"

namespace leviathan::systems::ai_policy {

namespace ng = leviathan::systems::detail;

namespace {

constexpr const char* kModule = "ai_policy";

constexpr double kPressureThreshold = 0.80;

constexpr double kCapacityLowMax    = 0.30;
constexpr double kCapacityMediumMax = 0.60;

struct InterestGroupAggregate {
    double mean_radicalism_inf_weighted = 0.0;
    double mean_loyalty                 = 0.5;
    int    count                        = 0;
};

// Aggregate IG state for one country. Validates every IG's influence /
// radicalism / loyalty as finite ratios in [0, 1] before reading.
core::Result<InterestGroupAggregate>
aggregate_country_igs(const core::CountryState& c,
                      const core::GameState&    state) {
    InterestGroupAggregate out;
    double total_influence    = 0.0;
    double weighted_radicalism = 0.0;
    double total_loyalty      = 0.0;
    for (const auto& ig : state.interest_groups) {
        if (ig.country != c.id) continue;
        if (auto err = ng::require_unit_ratio<InterestGroupAggregate>(
                kModule, "interest_group", ig.id_code,
                "interest_group.influence", ig.influence)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<InterestGroupAggregate>(
                kModule, "interest_group", ig.id_code,
                "interest_group.radicalism", ig.radicalism)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<InterestGroupAggregate>(
                kModule, "interest_group", ig.id_code,
                "interest_group.loyalty", ig.loyalty)) {
            return *err;
        }
        ++out.count;
        total_influence    += ig.influence;
        weighted_radicalism += ig.influence * ig.radicalism;
        total_loyalty      += ig.loyalty;
    }
    if (total_influence > 0.0) {
        out.mean_radicalism_inf_weighted =
            weighted_radicalism / total_influence;
    }
    if (out.count > 0) {
        out.mean_loyalty = total_loyalty / out.count;
    }
    return core::Result<InterestGroupAggregate>::success(out);
}

// Validate every input compute_total_pressure reads. Failure surfaces
// the offending entity / field / value.
core::Result<double> compute_total_pressure(const core::CountryState& c,
                                            const core::GameState&    state) {
    if (auto err = ng::require_unit_ratio<double>(
            kModule, "country", c.id_code, "country.stability", c.stability)) {
        return *err;
    }
    if (auto err = ng::require_unit_ratio<double>(
            kModule, "country", c.id_code, "country.legitimacy",
            c.legitimacy)) {
        return *err;
    }
    if (auto err = ng::require_unit_ratio<double>(
            kModule, "country", c.id_code, "country.corruption",
            c.corruption)) {
        return *err;
    }
    if (auto err = ng::require_finite_double<double>(
            kModule, "country", c.id_code, "country.gdp", c.gdp)) {
        return *err;
    }
    if (auto err = ng::require_finite_double<double>(
            kModule, "country", c.id_code, "country.budget_balance",
            c.budget_balance)) {
        return *err;
    }
    if (auto err = ng::require_unit_ratio<double>(
            kModule, "country", c.id_code, "country.threat_perception",
            c.threat_perception)) {
        return *err;
    }
    if (auto err = ng::require_nonneg_finite<double>(
            kModule, "country", c.id_code, "country.military_strength",
            c.military_strength)) {
        return *err;
    }

    double p = 0.0;
    p += (1.0 - c.stability);
    p += (1.0 - c.legitimacy);
    p += c.corruption;

    // Budget deficit pressure, normalised by a GDP-tied formula
    // denominator floor (scale >= 1.0 by construction; gdp validated
    // finite above so std::abs(gdp) is finite).
    const double scale = std::max(std::abs(c.gdp) * 0.01, 1.0);
    const double raw   = -c.budget_balance / scale;
    // Saturate raw to `[0, 1]`: documented game-mechanic bound on the
    // pressure contribution; inputs are validated finite above.
    p += std::min(1.0, std::max(0.0, raw));

    // Threat pressure (validate inbound relationship doubles).
    double effective_threat = c.threat_perception;
    for (std::size_t ri = 0; ri < state.relationships.size(); ++ri) {
        const auto& rel = state.relationships[ri];
        if (rel.to != c.id) continue;
        if (!std::isfinite(rel.threat)) {
            return core::Result<double>::failure(
                std::string(kModule) +
                ": state.relationships[" + std::to_string(ri) +
                "].threat = " + std::to_string(rel.threat) +
                " is not finite");
        }
        if (rel.threat > effective_threat) effective_threat = rel.threat;
    }
    double max_neighbour_str = 0.0;
    for (std::size_t ri = 0; ri < state.relationships.size(); ++ri) {
        const auto& rel = state.relationships[ri];
        if (rel.to != c.id || !rel.from.valid()) continue;
        if (!std::isfinite(rel.threat) ||
            !std::isfinite(rel.relationship)) {
            return core::Result<double>::failure(
                std::string(kModule) +
                ": state.relationships[" + std::to_string(ri) +
                "] threat / relationship is not finite");
        }
        const bool hostile =
            rel.threat > 0.0 || rel.relationship < 0.0;
        if (!hostile) continue;
        const auto fidx = static_cast<std::size_t>(rel.from.value());
        if (fidx < state.countries.size()) {
            const double sval = state.countries[fidx].military_strength;
            if (!std::isfinite(sval) || sval < 0.0) {
                return core::Result<double>::failure(
                    std::string(kModule) +
                    ": state.countries[" + std::to_string(fidx) +
                    "].military_strength = " + std::to_string(sval) +
                    " is not a finite non-negative value");
            }
            if (sval > max_neighbour_str) max_neighbour_str = sval;
        }
    }
    const double military_gap =
        std::max(0.0, max_neighbour_str - c.military_strength);
    const double military_gap_norm =
        std::min(1.0, military_gap / 100.0);
    p += std::min(1.0,
                  std::max(0.0,
                           effective_threat + 0.5 * military_gap_norm));

    auto ig_r = aggregate_country_igs(c, state);
    if (!ig_r) {
        return core::Result<double>::failure(std::move(ig_r.error()));
    }
    p += std::min(1.0,
                  std::max(0.0,
                           ig_r.value().mean_radicalism_inf_weighted));

    return core::Result<double>::success(p);
}

core::Result<std::size_t> capacity_to_count(const core::CountryState& c) {
    if (auto err = ng::require_finite_double<std::size_t>(
            kModule, "country", c.id_code, "country.gdp", c.gdp)) {
        return *err;
    }
    if (auto err = ng::require_finite_double<std::size_t>(
            kModule, "country", c.id_code, "country.budget_balance",
            c.budget_balance)) {
        return *err;
    }
    if (auto err = ng::require_unit_ratio<std::size_t>(
            kModule, "country", c.id_code,
            "country.administrative_efficiency",
            c.administrative_efficiency)) {
        return *err;
    }
    if (auto err = ng::require_unit_ratio<std::size_t>(
            kModule, "country", c.id_code,
            "country.government_authority.bureaucratic_compliance",
            c.government_authority.bureaucratic_compliance)) {
        return *err;
    }
    const double scale = std::max(std::abs(c.gdp) * 0.01, 1.0);
    const double raw   = -c.budget_balance / scale;
    const double budget_pressure =
        std::min(1.0, std::max(0.0, raw));
    const double cap =
        0.5 * c.administrative_efficiency +
        0.3 * c.government_authority.bureaucratic_compliance +
        0.2 * std::max(0.0, 1.0 - budget_pressure);
    if (cap < kCapacityLowMax)    return core::Result<std::size_t>::success(1);
    if (cap < kCapacityMediumMax) return core::Result<std::size_t>::success(2);
    return core::Result<std::size_t>::success(3);
}

core::Result<double> score_policy(const core::CountryState& c,
                                  const core::PolicyData&   p,
                                  const core::GameState&    state) {
    double score = 0.0;
    for (const auto& eff : p.effects) {
        if (eff.op != "add") continue;
        if (eff.target.rfind("country.", 0) != 0) continue;
        auto desire_r = effect_desire::for_country(c, eff.target, state);
        if (!desire_r) {
            return core::Result<double>::failure(std::move(desire_r.error()));
        }
        score += eff.value * desire_r.value();
    }
    auto ig_r = aggregate_country_igs(c, state);
    if (!ig_r) {
        return core::Result<double>::failure(std::move(ig_r.error()));
    }
    const auto& ig = ig_r.value();
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
        if (auto err = ng::require_unit_ratio<double>(
                kModule, "country", c.id_code,
                "country.threat_perception", c.threat_perception)) {
            return *err;
        }
        score += c.threat_perception * 0.20;
    }
    if (p.category == "media") {
        if (auto err = ng::require_unit_ratio<double>(
                kModule, "country", c.id_code,
                "country.legitimacy", c.legitimacy)) {
            return *err;
        }
        score += (1.0 - c.legitimacy) * 0.10;
    }
    if (p.id_code == "corruption_crackdown") {
        if (auto err = ng::require_unit_ratio<double>(
                kModule, "country", c.id_code,
                "country.corruption", c.corruption)) {
            return *err;
        }
        score += c.corruption * 0.30;
    }
    return core::Result<double>::success(score);
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

core::Result<std::vector<std::string>>
pick_top_k_for_country(const core::CountryState& c,
                       const core::GameState&    state,
                       std::size_t               k) {
    std::vector<std::string> out;
    if (state.policies.empty() || k == 0) {
        return core::Result<std::vector<std::string>>::success(std::move(out));
    }
    std::unordered_set<std::string> picked_this_tick;

    for (std::size_t round = 0; round < k; ++round) {
        double      best_score = -std::numeric_limits<double>::infinity();
        std::size_t best_index = state.policies.size();
        for (std::size_t i = 0; i < state.policies.size(); ++i) {
            const auto& p = state.policies[i];
            if (has_unexpired_policy(c, p.id_code, state.current_date)) {
                continue;
            }
            if (picked_this_tick.count(p.id_code) > 0) {
                continue;
            }
            auto s_r = score_policy(c, p, state);
            if (!s_r) {
                return core::Result<std::vector<std::string>>::failure(
                    std::move(s_r.error()));
            }
            const double s = s_r.value();
            if (s > best_score) {
                best_score = s;
                best_index = i;
            }
        }
        if (best_index >= state.policies.size()) break;
        out.push_back(state.policies[best_index].id_code);
        picked_this_tick.insert(state.policies[best_index].id_code);
    }
    return core::Result<std::vector<std::string>>::success(std::move(out));
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
        if (player_valid && cid == player) continue;

        auto pressure_r = compute_total_pressure(country, state);
        if (!pressure_r) {
            return core::Result<std::vector<Selection>>::failure(
                std::move(pressure_r.error()));
        }
        if (pressure_r.value() < kPressureThreshold) continue;

        auto k_r = capacity_to_count(country);
        if (!k_r) {
            return core::Result<std::vector<Selection>>::failure(
                std::move(k_r.error()));
        }
        auto picks_r = pick_top_k_for_country(country, state, k_r.value());
        if (!picks_r) {
            return core::Result<std::vector<Selection>>::failure(
                std::move(picks_r.error()));
        }
        for (auto& id_code : picks_r.value()) {
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

    // Per-country categorisation walk. Surfaces any pressure /
    // capacity / scorer failure as a hard Result::failure.
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const core::CountryId cid{
            static_cast<core::CountryId::underlying_type>(i)};
        if (player_valid && cid == player) continue;
        outcome.considered += 1;

        const auto& country = state.countries[i];
        auto pressure_r = compute_total_pressure(country, state);
        if (!pressure_r) {
            return core::Result<ApplyOutcome>::failure(
                std::move(pressure_r.error()));
        }
        if (pressure_r.value() < kPressureThreshold) {
            outcome.pressure_below_threshold_skipped += 1;
            continue;
        }
        auto k_r = capacity_to_count(country);
        if (!k_r) {
            return core::Result<ApplyOutcome>::failure(std::move(k_r.error()));
        }
        auto picks_r =
            pick_top_k_for_country(country, state, k_r.value());
        if (!picks_r) {
            return core::Result<ApplyOutcome>::failure(
                std::move(picks_r.error()));
        }
        if (picks_r.value().empty()) outcome.skipped += 1;
    }

    auto sel_r = select_policies(state);
    if (!sel_r) {
        return core::Result<ApplyOutcome>::failure(std::move(sel_r.error()));
    }
    const auto& selections = sel_r.value();

    // Apply phase. Post-M6.7 hardening: a per-country apply failure
    // now surfaces as a hard Result::failure (no fail-continue, no
    // failed_countries accumulation). `ApplyOutcome::failed_countries`
    // is retained as a vestigial field that is always empty under the
    // strict validation regime.
    for (const auto& sel : selections) {
        const core::PolicyData* policy = nullptr;
        for (const auto& p : state.policies) {
            if (p.id_code == sel.policy_id_code) {
                policy = &p;
                break;
            }
        }
        if (policy == nullptr) {
            return core::Result<ApplyOutcome>::failure(
                std::string(kModule) +
                "::apply_selected_policies: selected policy id_code '" +
                sel.policy_id_code + "' was not found in state.policies");
        }
        auto apply_r = policy::apply_policy_effects(
            state, sel.country, *policy);
        if (!apply_r) {
            return core::Result<ApplyOutcome>::failure(
                std::string(kModule) +
                "::apply_selected_policies: country " +
                std::to_string(sel.country.value()) +
                " policy '" + sel.policy_id_code + "': " +
                std::move(apply_r.error()));
        }
        outcome.applied += 1;
    }

    return core::Result<ApplyOutcome>::success(std::move(outcome));
}

}  // namespace leviathan::systems::ai_policy

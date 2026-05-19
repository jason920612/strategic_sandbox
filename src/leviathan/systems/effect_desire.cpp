#include "leviathan/systems/effect_desire.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include "internal/numeric_guards.hpp"

namespace leviathan::systems::effect_desire {

namespace ng = leviathan::systems::detail;

namespace {

constexpr const char* kModule = "effect_desire::for_country";

// Local helpers so each branch can do a single-line input check.
std::optional<core::Result<double>> check_ratio(
        const core::CountryState& c, const char* field_name, double v) {
    return ng::require_unit_ratio<double>(
        kModule, "country", c.id_code, field_name, v);
}
std::optional<core::Result<double>> check_finite(
        const core::CountryState& c, const char* field_name, double v) {
    return ng::require_finite_double<double>(
        kModule, "country", c.id_code, field_name, v);
}
std::optional<core::Result<double>> check_nonneg_finite(
        const core::CountryState& c, const char* field_name, double v) {
    return ng::require_nonneg_finite<double>(
        kModule, "country", c.id_code, field_name, v);
}

}  // namespace

core::Result<double> for_country(const core::CountryState& c,
                                 const std::string&        target,
                                 const core::GameState&    state) {
    if (target == "country.stability") {
        if (auto err = check_ratio(c, "country.stability", c.stability)) {
            return *err;
        }
        return core::Result<double>::success(1.0 - c.stability);
    }
    if (target == "country.legitimacy") {
        if (auto err = check_ratio(c, "country.legitimacy", c.legitimacy)) {
            return *err;
        }
        return core::Result<double>::success(1.0 - c.legitimacy);
    }
    if (target == "country.administrative_efficiency") {
        if (auto err = check_ratio(c,
                "country.administrative_efficiency",
                c.administrative_efficiency)) {
            return *err;
        }
        return core::Result<double>::success(
            1.0 - c.administrative_efficiency);
    }
    if (target == "country.fiscal_capacity") {
        if (auto err = check_ratio(c, "country.fiscal_capacity",
                                   c.fiscal_capacity)) {
            return *err;
        }
        return core::Result<double>::success(1.0 - c.fiscal_capacity);
    }
    if (target == "country.central_control") {
        // Moderate desire: countries want central_control in a healthy
        // mid-range, not maximised. The `std::max(0.0, 0.6 - x)` shape
        // is a documented game-mechanic floor (max with literal 0.0 is
        // not silent degradation of an upstream value — `x` itself
        // is validated finite + in `[0, 1]` above, so the result is
        // provably in `[0.0, 0.6]` by construction).
        if (auto err = check_ratio(c, "country.central_control",
                                   c.central_control)) {
            return *err;
        }
        return core::Result<double>::success(
            std::max(0.0, 0.6 - c.central_control));
    }
    if (target == "country.corruption") {
        if (auto err = check_ratio(c, "country.corruption", c.corruption)) {
            return *err;
        }
        return core::Result<double>::success(-c.corruption);
    }
    if (target == "country.budget_balance") {
        // Validate the two inputs the scale + ratio computation reads.
        // `c.gdp` must be finite (the `std::abs(gdp) * 0.01` floor of
        // 1.0 alone won't rescue a NaN gdp — it'd produce NaN scale).
        if (auto err = check_finite(c, "country.gdp", c.gdp)) {
            return *err;
        }
        if (auto err = check_finite(c, "country.budget_balance",
                                    c.budget_balance)) {
            return *err;
        }
        // Formula denominator floor — guarantees scale >= 1.0 by
        // construction; NOT a fallback for non-finite gdp (handled
        // above). With gdp validated finite, `std::abs(gdp)` is
        // finite, so `std::max(..., 1.0)` is finite and >= 1.0.
        const double scale = std::max(std::abs(c.gdp) * 0.01, 1.0);
        const double raw   = -c.budget_balance / scale;
        // Saturate raw to `[0, 1]`: this is a documented game-mechanic
        // bound on the desire-score contribution (deficit pressure
        // can't dominate other in-range desires). All inputs validated
        // above; this clamp documents the formula range, not a fallback.
        return core::Result<double>::success(
            std::min(1.0, std::max(0.0, raw)));
    }
    if (target == "country.legal_tax_burden") {
        if (auto err = check_finite(c, "country.gdp", c.gdp)) {
            return *err;
        }
        if (auto err = check_finite(c, "country.budget_balance",
                                    c.budget_balance)) {
            return *err;
        }
        if (auto err = check_ratio(c, "country.legal_tax_burden",
                                   c.legal_tax_burden)) {
            return *err;
        }
        const double scale = std::max(std::abs(c.gdp) * 0.01, 1.0);
        const double raw_budget = -c.budget_balance / scale;
        const double budget_pressure =
            std::min(1.0, std::max(0.0, raw_budget));
        return core::Result<double>::success(
            budget_pressure - c.legal_tax_burden * 0.5);
    }
    if (target == "country.military_power") {
        // Effective threat = max(threat_perception, max incoming
        // relationship threat). Reads `state.relationships` (RFC-090
        // §3.6 / §3.7).
        if (auto err = check_ratio(c, "country.threat_perception",
                                   c.threat_perception)) {
            return *err;
        }
        if (auto err = check_nonneg_finite(c, "country.military_strength",
                                           c.military_strength)) {
            return *err;
        }
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
            if (rel.threat > effective_threat) {
                effective_threat = rel.threat;
            }
        }
        // Plus military_strength disparity from HOSTILE neighbours
        // only (per RFC-090 §3.6 / §3.7 / §3.8 — only adversaries
        // drive defence spending).
        double max_neighbour_str = 0.0;
        for (std::size_t ri = 0; ri < state.relationships.size(); ++ri) {
            const auto& rel = state.relationships[ri];
            if (rel.to != c.id || !rel.from.valid()) {
                continue;
            }
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
            const auto fidx =
                static_cast<std::size_t>(rel.from.value());
            if (fidx < state.countries.size()) {
                const double s = state.countries[fidx].military_strength;
                if (!std::isfinite(s) || s < 0.0) {
                    return core::Result<double>::failure(
                        std::string(kModule) +
                        ": state.countries[" + std::to_string(fidx) +
                        "].military_strength = " + std::to_string(s) +
                        " is not a finite non-negative value");
                }
                if (s > max_neighbour_str) {
                    max_neighbour_str = s;
                }
            }
        }
        const double military_gap =
            std::max(0.0, max_neighbour_str - c.military_strength);
        // Normalise gap by 100 (typical compliance-fixture scale
        // is 10..90); clamp to `[0, 1]` as a documented formula
        // range (denominator is a constant 100.0; inputs validated
        // finite + non-negative above; no silent degradation).
        const double military_gap_norm =
            std::min(1.0, military_gap / 100.0);
        return core::Result<double>::success(
            effective_threat + 0.5 * military_gap_norm);
    }
    // Unknown country target: neutral (e.g. country.gdp,
    // country.tax_revenue not directly addressed by the current
    // policy / option fixtures). No input read; nothing to validate.
    return core::Result<double>::success(0.0);
}

}  // namespace leviathan::systems::effect_desire

#include "leviathan/systems/effect_desire.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace leviathan::systems::effect_desire {

double for_country(const core::CountryState& c,
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
        // RFC-required wiring that issue #110 §1 demanded and that
        // issue #112 leaves intact (both policy scorer and option
        // chooser now consume it through this shared helper).
        double effective_threat = c.threat_perception;
        for (const auto& rel : state.relationships) {
            if (rel.to == c.id && rel.threat > effective_threat) {
                effective_threat = rel.threat;
            }
        }
        // Plus military_strength disparity from HOSTILE neighbours
        // only: a stronger neighbour we share a positive
        // relationship and zero threat with (an ally) does NOT
        // produce militarisation pressure. A neighbour counts as
        // hostile if `rel.threat > 0` OR `rel.relationship < 0`
        // (RFC-090 §3.6 / §3.7 / §3.8 read together — only
        // adversaries drive defence spending).
        double max_neighbour_str = 0.0;
        for (const auto& rel : state.relationships) {
            if (rel.to != c.id || !rel.from.valid()) {
                continue;
            }
            const bool hostile =
                rel.threat > 0.0 || rel.relationship < 0.0;
            if (!hostile) {
                continue;
            }
            const auto fidx =
                static_cast<std::size_t>(rel.from.value());
            if (fidx < state.countries.size()) {
                const double s = state.countries[fidx].military_strength;
                if (s > max_neighbour_str) {
                    max_neighbour_str = s;
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
    // — not directly addressed by policies / event options in the
    // current fixtures): neutral.
    return 0.0;
}

}  // namespace leviathan::systems::effect_desire

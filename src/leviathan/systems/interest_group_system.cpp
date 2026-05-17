#include "leviathan/systems/interest_group_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace leviathan::systems::interest_group {

core::Result<ReactionOutcome> react(core::GameState& state) {
    // ---- Preflight: every group.country must index into state.countries.
    // M3.2 documents atomicity across the list: if any single
    // group has a bad country, NO group's loyalty / radicalism
    // is mutated. The preflight pass is cheap (size N walk) and
    // makes the test surface obvious.
    for (std::size_t i = 0; i < state.interest_groups.size(); ++i) {
        const auto& g = state.interest_groups[i];
        if (!g.country.valid() ||
            g.country.value() < 0 ||
            static_cast<std::size_t>(g.country.value()) >=
                state.countries.size()) {
            return core::Result<ReactionOutcome>::failure(
                "interest_group::react: interest_groups[" +
                std::to_string(i) +
                "] country is not a valid index into state.countries");
        }
    }

    // ---- Apply drift -----------------------------------------------------
    ReactionOutcome outcome;
    for (auto& g : state.interest_groups) {
        const auto& country = state.countries[
            static_cast<std::size_t>(g.country.value())];
        const double target_loyalty    = country.stability;
        const double target_radicalism = 1.0 - country.stability;

        g.loyalty    += (target_loyalty    - g.loyalty)
                        * kInterestGroupReactionRate;
        g.radicalism += (target_radicalism - g.radicalism)
                        * kInterestGroupReactionRate;

        g.loyalty    = std::clamp(g.loyalty,    0.0, 1.0);
        g.radicalism = std::clamp(g.radicalism, 0.0, 1.0);

        ++outcome.groups_updated;
    }

    return core::Result<ReactionOutcome>::success(outcome);
}

// ===========================================================================
// M3.3 - interest-group -> country feedback
// ===========================================================================

core::Result<CountryFeedbackOutcome> country_feedback(
        core::GameState& state) {
    // ---- Preflight: validate every input before any mutation ---------
    // M3.3 is the first reverse-direction system. The preflight
    // pass is stricter than M3.2's because a NaN in influence or
    // radicalism would otherwise propagate into country.stability
    // and silently poison the simulation. Cost: one extra walk
    // through state.interest_groups + state.countries; tiny
    // relative to the per-month per-country systems.
    for (std::size_t i = 0; i < state.interest_groups.size(); ++i) {
        const auto& g = state.interest_groups[i];
        if (!g.country.valid() ||
            g.country.value() < 0 ||
            static_cast<std::size_t>(g.country.value()) >=
                state.countries.size()) {
            return core::Result<CountryFeedbackOutcome>::failure(
                "interest_group::country_feedback: interest_groups[" +
                std::to_string(i) +
                "] country is not a valid index into state.countries");
        }
        if (!std::isfinite(g.influence) ||
            g.influence < 0.0 || g.influence > 1.0) {
            return core::Result<CountryFeedbackOutcome>::failure(
                "interest_group::country_feedback: interest_groups[" +
                std::to_string(i) +
                "] influence is not a finite ratio in [0, 1]");
        }
        if (!std::isfinite(g.radicalism) ||
            g.radicalism < 0.0 || g.radicalism > 1.0) {
            return core::Result<CountryFeedbackOutcome>::failure(
                "interest_group::country_feedback: interest_groups[" +
                std::to_string(i) +
                "] radicalism is not a finite ratio in [0, 1]");
        }
    }
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const auto& c = state.countries[i];
        if (!std::isfinite(c.stability) ||
            c.stability < 0.0 || c.stability > 1.0) {
            return core::Result<CountryFeedbackOutcome>::failure(
                "interest_group::country_feedback: countries[" +
                std::to_string(i) +
                "] stability is not a finite ratio in [0, 1]");
        }
    }

    // ---- Apply feedback ----------------------------------------------
    CountryFeedbackOutcome outcome;
    for (std::size_t ci = 0; ci < state.countries.size(); ++ci) {
        double weighted_sum = 0.0;
        double weight_sum   = 0.0;
        for (const auto& g : state.interest_groups) {
            if (static_cast<std::size_t>(g.country.value()) != ci) {
                continue;
            }
            if (g.influence <= 0.0) {
                continue;
            }
            weighted_sum += g.influence * g.radicalism;
            weight_sum   += g.influence;
        }
        if (weight_sum <= 0.0) {
            // No matching groups (or all zero-influence) -> skip.
            continue;
        }

        const double weighted_radicalism = weighted_sum / weight_sum;
        const double target_stability    = 1.0 - weighted_radicalism;

        auto& country = state.countries[ci];
        country.stability +=
            (target_stability - country.stability) *
            kInterestGroupCountryFeedbackRate;
        country.stability = std::clamp(country.stability, 0.0, 1.0);
        ++outcome.countries_updated;
    }

    return core::Result<CountryFeedbackOutcome>::success(outcome);
}

// ===========================================================================
// M3.4 - interest-group -> government_authority pressure
// ===========================================================================

core::Result<AuthorityPressureOutcome> authority_pressure(
        core::GameState& state) {
    // ---- Preflight: validate inputs that this step actually reads -----
    // M3.4 reads ONLY:
    //   - group.country (every group, regardless of kind, because
    //     a bad country index would crash the per-country loop
    //     below before the kind filter even runs)
    //   - group.influence (every group)
    //   - group.loyalty   (every group)
    //   - country.government_authority.bureaucratic_compliance
    // It does NOT read radicalism or country.stability, so those
    // are not preflighted here. The strict-preflight pattern
    // mirrors M3.3 (`country_feedback`) to keep the M3 reverse-
    // direction surface consistent.
    for (std::size_t i = 0; i < state.interest_groups.size(); ++i) {
        const auto& g = state.interest_groups[i];
        if (!g.country.valid() ||
            g.country.value() < 0 ||
            static_cast<std::size_t>(g.country.value()) >=
                state.countries.size()) {
            return core::Result<AuthorityPressureOutcome>::failure(
                "interest_group::authority_pressure: interest_groups[" +
                std::to_string(i) +
                "] country is not a valid index into state.countries");
        }
        if (!std::isfinite(g.influence) ||
            g.influence < 0.0 || g.influence > 1.0) {
            return core::Result<AuthorityPressureOutcome>::failure(
                "interest_group::authority_pressure: interest_groups[" +
                std::to_string(i) +
                "] influence is not a finite ratio in [0, 1]");
        }
        if (!std::isfinite(g.loyalty) ||
            g.loyalty < 0.0 || g.loyalty > 1.0) {
            return core::Result<AuthorityPressureOutcome>::failure(
                "interest_group::authority_pressure: interest_groups[" +
                std::to_string(i) +
                "] loyalty is not a finite ratio in [0, 1]");
        }
    }
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const double bc =
            state.countries[i].government_authority.bureaucratic_compliance;
        if (!std::isfinite(bc) || bc < 0.0 || bc > 1.0) {
            return core::Result<AuthorityPressureOutcome>::failure(
                "interest_group::authority_pressure: countries[" +
                std::to_string(i) +
                "] government_authority.bureaucratic_compliance is"
                " not a finite ratio in [0, 1]");
        }
    }

    // ---- Apply pressure ----------------------------------------------
    AuthorityPressureOutcome outcome;
    for (std::size_t ci = 0; ci < state.countries.size(); ++ci) {
        double weighted_sum = 0.0;
        double weight_sum   = 0.0;
        for (const auto& g : state.interest_groups) {
            if (static_cast<std::size_t>(g.country.value()) != ci) {
                continue;
            }
            if (g.kind != core::InterestGroupKind::Bureaucracy) {
                continue;
            }
            if (g.influence <= 0.0) {
                continue;
            }
            weighted_sum += g.influence * g.loyalty;
            weight_sum   += g.influence;
        }
        if (weight_sum <= 0.0) {
            continue;
        }

        const double target_compliance = weighted_sum / weight_sum;
        auto& compliance =
            state.countries[ci].government_authority.bureaucratic_compliance;
        compliance +=
            (target_compliance - compliance) *
            kInterestGroupAuthorityPressureRate;
        compliance = std::clamp(compliance, 0.0, 1.0);
        ++outcome.countries_updated;
    }

    return core::Result<AuthorityPressureOutcome>::success(outcome);
}

}  // namespace leviathan::systems::interest_group

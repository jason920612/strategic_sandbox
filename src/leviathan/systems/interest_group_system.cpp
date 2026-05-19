#include "leviathan/systems/interest_group_system.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "internal/numeric_guards.hpp"

namespace leviathan::systems::interest_group {

namespace ng = leviathan::systems::detail;

namespace {

// One interest group's prepared drift candidates (used by react()).
struct PreparedReactionWrite {
    core::InterestGroupState* group           = nullptr;
    double                    new_loyalty     = 0.0;
    double                    new_radicalism  = 0.0;
};

// One country's prepared stability write (used by country_feedback()).
struct PreparedFeedbackWrite {
    std::size_t country_index = 0;
    double      new_stability = 0.0;
    // Trace metadata captured during candidate computation.
    double      before                = 0.0;
    double      target_stability      = 0.0;
    double      weighted_radicalism   = 0.0;
    double      weight_sum            = 0.0;
    int         matched_groups        = 0;
};

// One country's prepared compliance write (used by authority_pressure()).
struct PreparedAuthorityWrite {
    std::size_t country_index             = 0;
    double      new_compliance            = 0.0;
    double      before                    = 0.0;
    double      target_compliance         = 0.0;
    double      weighted_bureau_loyalty   = 0.0;
    double      weight_sum                = 0.0;
    int         matched_groups            = 0;
};

}  // namespace

core::Result<ReactionOutcome> react(core::GameState& state) {
    // ---- Preflight: every group.country must index into state.countries.
    // M3.2 documents atomicity across the list: if any single group
    // has a bad country, NO group's loyalty / radicalism is mutated.
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

    // Validate every input the drift formula reads (post-M6.7
    // hardening: `feedback_no_silent_degradation`).
    for (auto& g : state.interest_groups) {
        if (auto err = ng::require_unit_ratio<ReactionOutcome>(
                "interest_group::react", "interest_group", g.id_code,
                "interest_group.loyalty", g.loyalty)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<ReactionOutcome>(
                "interest_group::react", "interest_group", g.id_code,
                "interest_group.radicalism", g.radicalism)) {
            return *err;
        }
    }
    for (const auto& c : state.countries) {
        if (auto err = ng::require_unit_ratio<ReactionOutcome>(
                "interest_group::react", "country", c.id_code,
                "country.stability", c.stability)) {
            return *err;
        }
    }

    // ---- Pre-flight: compute drift candidates and validate ---------
    std::vector<PreparedReactionWrite> prepared;
    prepared.reserve(state.interest_groups.size());
    for (auto& g : state.interest_groups) {
        const auto& country = state.countries[
            static_cast<std::size_t>(g.country.value())];
        const double target_loyalty    = country.stability;
        const double target_radicalism = 1.0 - country.stability;
        const double new_loyalty =
            g.loyalty + (target_loyalty - g.loyalty)
                        * kInterestGroupReactionRate;
        const double new_radicalism =
            g.radicalism + (target_radicalism - g.radicalism)
                           * kInterestGroupReactionRate;

        if (auto err = ng::require_unit_ratio<ReactionOutcome>(
                "interest_group::react", "interest_group", g.id_code,
                "interest_group.loyalty (post-drift candidate)",
                new_loyalty)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<ReactionOutcome>(
                "interest_group::react", "interest_group", g.id_code,
                "interest_group.radicalism (post-drift candidate)",
                new_radicalism)) {
            return *err;
        }
        prepared.push_back({&g, new_loyalty, new_radicalism});
    }

    // ---- Commit ----------------------------------------------------
    ReactionOutcome outcome;
    for (auto& p : prepared) {
        p.group->loyalty    = p.new_loyalty;
        p.group->radicalism = p.new_radicalism;
        ++outcome.groups_updated;
    }

    return core::Result<ReactionOutcome>::success(outcome);
}

// ===========================================================================
// M3.3 - interest-group -> country feedback
// ===========================================================================

core::Result<CountryFeedbackOutcome> country_feedback(
        core::GameState& state,
        std::vector<CountryFeedbackTraceRow>* trace_out) {
    // ---- Preflight: validate every input before any mutation ---------
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
        if (auto err = ng::require_unit_ratio<CountryFeedbackOutcome>(
                "interest_group::country_feedback", "interest_group",
                g.id_code, "interest_group.influence", g.influence)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<CountryFeedbackOutcome>(
                "interest_group::country_feedback", "interest_group",
                g.id_code, "interest_group.radicalism", g.radicalism)) {
            return *err;
        }
    }
    for (const auto& c : state.countries) {
        if (auto err = ng::require_unit_ratio<CountryFeedbackOutcome>(
                "interest_group::country_feedback", "country", c.id_code,
                "country.stability", c.stability)) {
            return *err;
        }
    }

    // ---- Pre-flight: compute every country's candidate stability ---
    std::vector<PreparedFeedbackWrite> prepared;
    for (std::size_t ci = 0; ci < state.countries.size(); ++ci) {
        double weighted_sum = 0.0;
        double weight_sum   = 0.0;
        int    matched      = 0;
        for (const auto& g : state.interest_groups) {
            if (static_cast<std::size_t>(g.country.value()) != ci) {
                continue;
            }
            if (g.influence <= 0.0) {
                continue;
            }
            weighted_sum += g.influence * g.radicalism;
            weight_sum   += g.influence;
            ++matched;
        }
        if (weight_sum <= 0.0) {
            // No matching groups (or all zero-influence) -> skip.
            continue;
        }

        const auto& country = state.countries[ci];
        const double before                = country.stability;
        const double weighted_radicalism   = weighted_sum / weight_sum;
        const double target_stability      = 1.0 - weighted_radicalism;
        const double new_stability =
            before + (target_stability - before) *
                     kInterestGroupCountryFeedbackRate;

        // Candidate-validate-commit (post-M6.7 hardening): the previous
        // `std::clamp(country.stability, 0.0, 1.0)` saturation is gone.
        if (auto err = ng::require_unit_ratio<CountryFeedbackOutcome>(
                "interest_group::country_feedback", "country",
                country.id_code,
                "country.stability (post-feedback candidate)",
                new_stability)) {
            return *err;
        }

        prepared.push_back({ci, new_stability, before, target_stability,
                            weighted_radicalism, weight_sum, matched});
    }

    // ---- Commit + emit trace ---------------------------------------
    CountryFeedbackOutcome outcome;
    for (const auto& p : prepared) {
        auto& country = state.countries[p.country_index];
        country.stability = p.new_stability;
        ++outcome.countries_updated;

        if (trace_out != nullptr) {
            CountryFeedbackTraceRow row;
            row.date                = state.current_date;
            row.country_id          = static_cast<int>(p.country_index);
            row.country_id_code     = country.id_code;
            row.matched_groups      = p.matched_groups;
            row.weight_sum          = p.weight_sum;
            row.weighted_radicalism = p.weighted_radicalism;
            row.target_stability    = p.target_stability;
            row.stability_before    = p.before;
            row.stability_after     = country.stability;
            row.stability_delta     = country.stability - p.before;
            trace_out->push_back(std::move(row));
        }
    }

    return core::Result<CountryFeedbackOutcome>::success(outcome);
}

// ===========================================================================
// M3.4 - interest-group -> government_authority pressure
// ===========================================================================

core::Result<AuthorityPressureOutcome> authority_pressure(
        core::GameState& state,
        std::vector<AuthorityPressureTraceRow>* trace_out) {
    // ---- Preflight: validate inputs that this step actually reads -----
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
        if (auto err = ng::require_unit_ratio<AuthorityPressureOutcome>(
                "interest_group::authority_pressure", "interest_group",
                g.id_code, "interest_group.influence", g.influence)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<AuthorityPressureOutcome>(
                "interest_group::authority_pressure", "interest_group",
                g.id_code, "interest_group.loyalty", g.loyalty)) {
            return *err;
        }
    }
    for (const auto& c : state.countries) {
        if (auto err = ng::require_unit_ratio<AuthorityPressureOutcome>(
                "interest_group::authority_pressure", "country",
                c.id_code,
                "country.government_authority.bureaucratic_compliance",
                c.government_authority.bureaucratic_compliance)) {
            return *err;
        }
    }

    // ---- Pre-flight: compute every country's candidate compliance ---
    std::vector<PreparedAuthorityWrite> prepared;
    for (std::size_t ci = 0; ci < state.countries.size(); ++ci) {
        double weighted_sum = 0.0;
        double weight_sum   = 0.0;
        int    matched      = 0;
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
            ++matched;
        }
        if (weight_sum <= 0.0) {
            continue;
        }

        const double target_compliance = weighted_sum / weight_sum;
        const double before =
            state.countries[ci].government_authority.bureaucratic_compliance;
        const double new_compliance =
            before + (target_compliance - before) *
                     kInterestGroupAuthorityPressureRate;

        if (auto err = ng::require_unit_ratio<AuthorityPressureOutcome>(
                "interest_group::authority_pressure", "country",
                state.countries[ci].id_code,
                "country.government_authority.bureaucratic_compliance"
                " (post-pressure candidate)",
                new_compliance)) {
            return *err;
        }

        prepared.push_back({ci, new_compliance, before, target_compliance,
                            target_compliance, weight_sum, matched});
    }

    // ---- Commit + emit trace ---------------------------------------
    AuthorityPressureOutcome outcome;
    for (const auto& p : prepared) {
        auto& compliance =
            state.countries[p.country_index]
                .government_authority.bureaucratic_compliance;
        compliance = p.new_compliance;
        ++outcome.countries_updated;

        if (trace_out != nullptr) {
            AuthorityPressureTraceRow row;
            row.date                            = state.current_date;
            row.country_id                      =
                static_cast<int>(p.country_index);
            row.country_id_code                 =
                state.countries[p.country_index].id_code;
            row.matched_groups                  = p.matched_groups;
            row.weight_sum                      = p.weight_sum;
            row.weighted_bureaucracy_loyalty    = p.weighted_bureau_loyalty;
            row.target_bureaucratic_compliance  = p.target_compliance;
            row.bureaucratic_compliance_before  = p.before;
            row.bureaucratic_compliance_after   = compliance;
            row.bureaucratic_compliance_delta   = compliance - p.before;
            trace_out->push_back(std::move(row));
        }
    }

    return core::Result<AuthorityPressureOutcome>::success(outcome);
}

}  // namespace leviathan::systems::interest_group

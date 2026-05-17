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
        core::GameState& state,
        std::vector<CountryFeedbackTraceRow>* trace_out) {
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
            // M3.6: skipped countries produce no trace row by design.
            continue;
        }

        const double weighted_radicalism = weighted_sum / weight_sum;
        const double target_stability    = 1.0 - weighted_radicalism;

        auto& country = state.countries[ci];
        const double before = country.stability;
        country.stability +=
            (target_stability - country.stability) *
            kInterestGroupCountryFeedbackRate;
        country.stability = std::clamp(country.stability, 0.0, 1.0);
        ++outcome.countries_updated;

        // M3.6: emit a trace row reflecting the mutation that just
        // landed. Only countries actually updated produce a row,
        // and the row is emitted AFTER the clamp so `stability_after`
        // matches what the rest of the simulation will read this
        // month. `trace_out == nullptr` is the default M3.3 / M3.4 /
        // M3.5 behaviour.
        if (trace_out != nullptr) {
            CountryFeedbackTraceRow row;
            row.date                = state.current_date;
            row.country_id          = static_cast<int>(ci);
            row.country_id_code     = country.id_code;
            row.matched_groups      = matched;
            row.weight_sum          = weight_sum;
            row.weighted_radicalism = weighted_radicalism;
            row.target_stability    = target_stability;
            row.stability_before    = before;
            row.stability_after     = country.stability;
            row.stability_delta     = country.stability - before;
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
            // M3.6: skipped countries produce no trace row by design.
            continue;
        }

        const double target_compliance = weighted_sum / weight_sum;
        auto& compliance =
            state.countries[ci].government_authority.bureaucratic_compliance;
        const double before = compliance;
        compliance +=
            (target_compliance - compliance) *
            kInterestGroupAuthorityPressureRate;
        compliance = std::clamp(compliance, 0.0, 1.0);
        ++outcome.countries_updated;

        // M3.6: emit a trace row reflecting the mutation that just
        // landed. `target_compliance == weighted_bureaucracy_loyalty`
        // by definition; we surface both names so the CSV column
        // matches the formula reader's mental model.
        if (trace_out != nullptr) {
            AuthorityPressureTraceRow row;
            row.date                            = state.current_date;
            row.country_id                      = static_cast<int>(ci);
            row.country_id_code                 =
                state.countries[ci].id_code;
            row.matched_groups                  = matched;
            row.weight_sum                      = weight_sum;
            row.weighted_bureaucracy_loyalty    = target_compliance;
            row.target_bureaucratic_compliance  = target_compliance;
            row.bureaucratic_compliance_before  = before;
            row.bureaucratic_compliance_after   = compliance;
            row.bureaucratic_compliance_delta   = compliance - before;
            trace_out->push_back(std::move(row));
        }
    }

    return core::Result<AuthorityPressureOutcome>::success(outcome);
}

// ===========================================================================
// M3.9 - interest-group -> government_authority.military_loyalty pressure
// ===========================================================================
//
// Mirrors M3.4 `authority_pressure` line for line, except:
//   * filters interest groups on `kind == Military` (vs Bureaucracy);
//   * reads / writes `military_loyalty` (vs bureaucratic_compliance);
//   * uses `kInterestGroupMilitaryPressureRate` (same numerical value
//     as M3.4's rate; the constants stay separate so a future rate
//     re-tune of one channel doesn't accidentally re-tune the other).
//
// The two functions are deliberately not merged into a single
// kind-parameterised helper. M3.9 is a sibling-by-extension; further
// authority sub-fields (intelligence_capability, media_control) will
// likely take their own kinds, weights, or aggregations and the
// boilerplate is small enough that duplication is cheaper than a
// premature abstraction.

core::Result<MilitaryPressureOutcome> military_pressure(
        core::GameState& state,
        std::vector<MilitaryPressureTraceRow>* trace_out) {
    // ---- Preflight: validate inputs that this step actually reads -----
    for (std::size_t i = 0; i < state.interest_groups.size(); ++i) {
        const auto& g = state.interest_groups[i];
        if (!g.country.valid() ||
            g.country.value() < 0 ||
            static_cast<std::size_t>(g.country.value()) >=
                state.countries.size()) {
            return core::Result<MilitaryPressureOutcome>::failure(
                "interest_group::military_pressure: interest_groups[" +
                std::to_string(i) +
                "] country is not a valid index into state.countries");
        }
        if (!std::isfinite(g.influence) ||
            g.influence < 0.0 || g.influence > 1.0) {
            return core::Result<MilitaryPressureOutcome>::failure(
                "interest_group::military_pressure: interest_groups[" +
                std::to_string(i) +
                "] influence is not a finite ratio in [0, 1]");
        }
        if (!std::isfinite(g.loyalty) ||
            g.loyalty < 0.0 || g.loyalty > 1.0) {
            return core::Result<MilitaryPressureOutcome>::failure(
                "interest_group::military_pressure: interest_groups[" +
                std::to_string(i) +
                "] loyalty is not a finite ratio in [0, 1]");
        }
    }
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const double ml =
            state.countries[i].government_authority.military_loyalty;
        if (!std::isfinite(ml) || ml < 0.0 || ml > 1.0) {
            return core::Result<MilitaryPressureOutcome>::failure(
                "interest_group::military_pressure: countries[" +
                std::to_string(i) +
                "] government_authority.military_loyalty is not a"
                " finite ratio in [0, 1]");
        }
    }

    // ---- Apply pressure ----------------------------------------------
    MilitaryPressureOutcome outcome;
    for (std::size_t ci = 0; ci < state.countries.size(); ++ci) {
        double weighted_sum = 0.0;
        double weight_sum   = 0.0;
        int    matched      = 0;
        for (const auto& g : state.interest_groups) {
            if (static_cast<std::size_t>(g.country.value()) != ci) {
                continue;
            }
            if (g.kind != core::InterestGroupKind::Military) {
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

        const double target_loyalty = weighted_sum / weight_sum;
        auto& military_loyalty =
            state.countries[ci].government_authority.military_loyalty;
        const double before = military_loyalty;
        military_loyalty +=
            (target_loyalty - military_loyalty) *
            kInterestGroupMilitaryPressureRate;
        military_loyalty = std::clamp(military_loyalty, 0.0, 1.0);
        ++outcome.countries_updated;

        if (trace_out != nullptr) {
            MilitaryPressureTraceRow row;
            row.date                       = state.current_date;
            row.country_id                 = static_cast<int>(ci);
            row.country_id_code            =
                state.countries[ci].id_code;
            row.matched_groups             = matched;
            row.weight_sum                 = weight_sum;
            row.weighted_military_loyalty  = target_loyalty;
            row.target_military_loyalty    = target_loyalty;
            row.military_loyalty_before    = before;
            row.military_loyalty_after     = military_loyalty;
            row.military_loyalty_delta     = military_loyalty - before;
            trace_out->push_back(std::move(row));
        }
    }

    return core::Result<MilitaryPressureOutcome>::success(outcome);
}

}  // namespace leviathan::systems::interest_group

#include "leviathan/systems/monthly_pipeline.hpp"

#include <cstddef>
#include <string>

namespace leviathan::systems::monthly {

core::Result<CountryMonthlyOutcome> tick_country(core::GameState& state,
                                                 core::CountryId  country) {
    CountryMonthlyOutcome out;
    out.country = country;

    auto react = faction::react(state, country);
    if (!react.ok()) {
        return core::Result<CountryMonthlyOutcome>::failure(
            "monthly::tick_country: faction::react failed: " + react.error());
    }
    out.faction = react.value();

    auto stab = stability::tick(state, country);
    if (!stab.ok()) {
        return core::Result<CountryMonthlyOutcome>::failure(
            "monthly::tick_country: stability::tick failed: " + stab.error());
    }
    out.stability = stab.value();

    auto econ = economy::tick(state, country);
    if (!econ.ok()) {
        return core::Result<CountryMonthlyOutcome>::failure(
            "monthly::tick_country: economy::tick failed: " + econ.error());
    }
    out.economy = econ.value();

    return core::Result<CountryMonthlyOutcome>::success(out);
}

core::Result<MonthlyOutcome> tick_all_countries(core::GameState& state) {
    MonthlyOutcome out;
    out.countries.reserve(state.countries.size());

    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const core::CountryId id{static_cast<int>(i)};
        auto r = tick_country(state, id);
        if (!r.ok()) {
            return core::Result<MonthlyOutcome>::failure(
                "monthly::tick_all_countries: country index " +
                std::to_string(i) + " failed: " + r.error());
        }
        out.countries.push_back(r.value());
        ++out.countries_processed;
    }

    // M3.2: after every per-country tick has settled, drift each
    // interest group toward its country's post-tick stability.
    // The step is global (one call processes every group across
    // every country) and runs LAST so it reads the freshly
    // updated stability values.
    auto ig = interest_group::react(state);
    if (!ig.ok()) {
        return core::Result<MonthlyOutcome>::failure(
            "monthly::tick_all_countries: interest_group::react"
            " failed: " + ig.error());
    }
    out.interest_groups_updated = ig.value().groups_updated;

    // M3.3: close the loop in the opposite direction. Each
    // country's stability drifts toward (1 - influence-weighted
    // radicalism) of its own interest groups, at the slower
    // rate kInterestGroupCountryFeedbackRate (0.02). Runs AFTER
    // M3.2 so it sees the just-updated radicalism values.
    //
    // M3.6: pass the trace vector so the runner can dump one row
    // per actually-updated country to interest_group_country_feedback.csv.
    auto fb = interest_group::country_feedback(
        state, &out.interest_group_country_feedback_trace_rows);
    if (!fb.ok()) {
        return core::Result<MonthlyOutcome>::failure(
            "monthly::tick_all_countries: interest_group::"
            "country_feedback failed: " + fb.error());
    }
    out.interest_group_countries_updated =
        fb.value().countries_updated;

    // M3.4: drift each country's
    // government_authority.bureaucratic_compliance toward the
    // influence-weighted loyalty of its Bureaucracy-kind
    // interest groups, at the even slower rate
    // kInterestGroupAuthorityPressureRate (0.01). Runs AFTER
    // M3.3 so it sees the just-updated group loyalty values.
    //
    // M3.6: pass the trace vector so the runner can dump one row
    // per actually-updated country to interest_group_authority_pressure.csv.
    auto ap = interest_group::authority_pressure(
        state, &out.interest_group_authority_pressure_trace_rows);
    if (!ap.ok()) {
        return core::Result<MonthlyOutcome>::failure(
            "monthly::tick_all_countries: interest_group::"
            "authority_pressure failed: " + ap.error());
    }
    out.interest_group_authority_countries_updated =
        ap.value().countries_updated;

    // ---- 7. M5.8: event_engine::tick_events --------------------------
    // Final global step: evaluate every state.events trigger
    // against the post-M3.4 state snapshot, fire every matching
    // event, and apply each fired event's effects. Runs LAST so
    // events about "low stability / high radicalism" check the
    // values as they stand at month-end, not pre-month. M5.7's
    // tick_events ships the snapshot-evaluation contract
    // (evaluator runs once at the top; cascade events wait for
    // the next monthly tick).
    auto ev = event_engine::tick_events(state);
    if (!ev.ok()) {
        return core::Result<MonthlyOutcome>::failure(
            "monthly::tick_all_countries: event_engine::"
            "tick_events failed: " + ev.error());
    }
    out.event_tick = ev.value();

    return core::Result<MonthlyOutcome>::success(std::move(out));
}

}  // namespace leviathan::systems::monthly

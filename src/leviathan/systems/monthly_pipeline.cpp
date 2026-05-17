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
    auto fb = interest_group::country_feedback(state);
    if (!fb.ok()) {
        return core::Result<MonthlyOutcome>::failure(
            "monthly::tick_all_countries: interest_group::"
            "country_feedback failed: " + fb.error());
    }
    out.interest_group_countries_updated =
        fb.value().countries_updated;

    return core::Result<MonthlyOutcome>::success(std::move(out));
}

}  // namespace leviathan::systems::monthly

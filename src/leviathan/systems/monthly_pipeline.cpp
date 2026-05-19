#include "leviathan/systems/monthly_pipeline.hpp"

#include <cstddef>
#include <string>

#include "leviathan/systems/faction_conflict.hpp"

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

    // ---- 7. Issue #108 fix: ai_policy::apply_selected_policies ------
    // RFC-010 §2.2 ("AI 國家可自動選政策") and RFC-090 §3.5 expect
    // AI auto-selection to be part of normal simulation, not a
    // helper that exists but is never called. Wire it here, after
    // the per-country + M3 state-wide steps (so AI decisions see
    // freshly-drifted faction / interest-group state) but BEFORE
    // event_engine::tick_events (so events can react to the
    // AI-applied state in the same monthly tick). RCR-1 shipped
    // the helper as RNG-free + mutation-via-existing-policy-path;
    // wiring it into the monthly pipeline does NOT change the
    // helper's semantics. Fail-continue: a per-country apply
    // failure is captured in ApplyOutcome.failed_countries and
    // does NOT abort the monthly pipeline (mirrors the M5.6
    // event-effects convention: a broken event for country X
    // doesn't block country Y).
    auto ai = ai_policy::apply_selected_policies(state);
    if (!ai.ok()) {
        return core::Result<MonthlyOutcome>::failure(
            "monthly::tick_all_countries: ai_policy::"
            "apply_selected_policies failed: " + ai.error());
    }
    out.ai_policies_considered =
        static_cast<int>(ai.value().considered);
    out.ai_policies_applied =
        static_cast<int>(ai.value().applied);
    out.ai_policies_skipped =
        static_cast<int>(ai.value().skipped);
    out.ai_policies_failed =
        static_cast<int>(ai.value().failed_countries.size());

    // ---- 8. M7.4 faction_conflict::tick_apply_pressure ---------------
    // RFC-090 §7.4 + RFC-020 §8 `派系鬥爭`. Walks state.factions
    // per country; for each (country, RFC-020 §8 rivalry-pair)
    // where BOTH sides of the pair have at least one faction in
    // the country with influence > threshold, applies
    // asymptotic radicalism+ on every participating faction.
    // Runs AFTER ai_policy::apply so AI-driven influence
    // mutations are visible; runs BEFORE event_engine::tick_events
    // so M5 events keyed off faction radicalism (M7.2
    // `faction.radicalism` trigger) observe the drift.
    auto fc = faction_conflict::tick_apply_pressure(state);
    if (!fc.ok()) {
        return core::Result<MonthlyOutcome>::failure(
            "monthly::tick_all_countries: faction_conflict::"
            "tick_apply_pressure failed: " + fc.error());
    }
    out.faction_conflict_pairs_active =
        fc.value().pairs_active;
    out.faction_conflict_factions_drifted =
        fc.value().factions_drifted;

    // ---- 9. M5.8: event_engine::tick_events --------------------------
    // Final global step: evaluate every state.events trigger
    // against the post-M3.4 + post-AI-apply state snapshot, fire
    // every matching event, and apply each fired event's effects.
    // Runs LAST so events about "low stability / high radicalism"
    // check the values as they stand at month-end, not pre-month.
    // M5.7's tick_events ships the snapshot-evaluation contract
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

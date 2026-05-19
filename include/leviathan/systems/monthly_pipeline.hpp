// MonthlyPipeline - explicit-call composition of the M1.6 / M1.7 / M1.8
// per-country systems into a single caller.
//
// M1.9 wires the existing free-function systems together; it adds NO
// new gameplay. Each call to `tick_country` invokes the three
// sub-systems in the canonical order:
//
//   1.  faction::react   (state, country)
//   2.  stability::tick  (state, country)
//   3.  economy::tick    (state, country)
//
// M3.2 extends `tick_all_countries` with a global step that runs
// AFTER every per-country tick:
//
//   4.  interest_group::react (state)
//
// The global step runs last so it reads post-tick stability from
// every country; it never mutates country / faction / policy
// state and never feeds back into the per-country pipeline in
// the same month.
//
// M3.3 adds the reverse direction as a second global step run
// AFTER M3.2:
//
//   5.  interest_group::country_feedback (state)
//
// `country_feedback` reads the freshly-drifted `group.radicalism`
// (post-M3.2) and applies a small influence-weighted feedback to
// each country's `stability`. The closed loop (4 then 5) is
// deliberately tuned so 5 runs at a slower rate than 4 to avoid
// oscillation. `country_feedback` mutates ONLY `country.stability`
// — no other country / faction / authority / interest-group
// field is touched in this step.
//
// M3.4 adds a third global step run AFTER M3.3:
//
//   6.  interest_group::authority_pressure (state)
//
// `authority_pressure` reads the freshly-drifted Bureaucracy-kind
// `group.loyalty` (post-M3.2) and applies a small influence-
// weighted aggregate to each country's
// `government_authority.bureaucratic_compliance`. Slower than
// `country_feedback` so the rate ladder mood (0.05) -> stability
// (0.02) -> authority (0.01) keeps the outermost leg well-damped.
// `authority_pressure` mutates ONLY
// `government_authority.bureaucratic_compliance` — no other
// authority sub-field, no country state field beyond compliance,
// and no interest-group field is touched.
//
// The global steps AFTER M3.4 (issue #112 semantics + M7.1
// faction-demands insertion):
//
//   7.  ai_policy::apply_selected_policies      (state)
//   8.  faction_demands::tick_generate          (state, current_date)
//   9.  faction_demands::tick_expire_and_apply  (state, current_date)
//  10.  event_engine::tick_events               (state)
//
// Step 8 — M7.1 demand generation (RFC-090 §7.1, RFC-020 §7):
//   - For each faction whose `type` matches an RFC-020 §7
//     demand kind AND whose `radicalism > kFactionDemand-
//     GenerateRadicalismThreshold` AND which holds no Pending
//     demand of that kind yet, append a new Pending
//     FactionDemand with the canonical id_code shape and
//     `expires_on = current_date + kFactionDemandLifetimeDays`.
//   - Deterministic, RNG-free; appends to
//     `state.faction_demands`.
//   - Runs AFTER ai_policy::apply so AI policy mutations
//     (which can move budget shares / authority fields) are
//     observable to a future demand kind that reads them; runs
//     BEFORE the expire step so a freshly-generated demand
//     cannot also expire in the same tick (lifetime > 0).
//   - Runs BEFORE event_engine::tick_events so events that
//     read faction state see the post-generate radicalism /
//     loyalty drift produced by step 9 (the expire/apply step
//     drifts the faction whose demand expired).
//
// Step 9 — M7.1 demand expire + apply (RFC-090 §7.1):
//   - For each Pending demand whose `expires_on <=
//     current_date`, flip status to Expired and apply
//     asymptotic radicalism / loyalty drift on the issuing
//     faction (asymptotic-add radicalism +
//     kFactionDemandExpireRadicalismAsymptoticDelta;
//     asymptotic-subtract loyalty -
//     kFactionDemandExpireLoyaltyAsymptoticDelta). Expired
//     demands stay in `state.faction_demands` as an audit
//     trail.
//   - Deterministic, RNG-free.
//   - Runs BEFORE event_engine::tick_events so M5 events
//     that key off faction radicalism observe the drift.
//
// Step 7 — AI policy selection (RFC-090 §3.5 / RFC-040 §4):
//   - PRESSURE-gated: countries whose `compute_total_pressure`
//     (sum of normalised stability / legitimacy / corruption /
//     budget / threat / IG-radicalism terms) falls below
//     `ai_policy::kPressureThreshold = 0.80` emit ZERO selections
//     that tick. `MonthlyOutcome.ai_policies_*` counters expose
//     the gate result.
//   - CAPACITY-bounded: countries above the gate emit 1 / 2 / 3
//     picks based on
//     `0.5×admin_efficiency + 0.3×bcomp + 0.2×budget_headroom`.
//   - The scorer reads CountryState pressures, `state.relationships`
//     inbound threat + hostile-neighbour military_strength gap,
//     `state.interest_groups` influence-weighted radicalism /
//     loyalty, and PolicyData category + effects.
//   - No-stacking rule: candidates already active+unexpired are
//     excluded; chooser picks next-best.
//   - Player country always skipped.
//
// Step 8 — `tick_events` (RFC-090 §5.6 / §5.7 / §5.8 / §5.12,
// RFC-050 §3):
//   - PER-COUNTRY / PER-CATEGORY weighted-random draw via
//     `random::weighted_choice(state.rng, …)`. ONE event fires
//     per (country, category) per tick. Same template MAY fire
//     for multiple countries in the same tick — each country
//     rolls independently.
//   - Trigger scoping is strictly per-country (`country.*` and
//     `interest_group.*` both bind within the named country).
//   - For NON-player countries with options, the state-based
//     option chooser (`select_best_option_for_country`) picks
//     and `apply_option_effects_with_mode` applies per the
//     event's author-controlled `option_effect_mode` (OptionOnly
//     / BaseThenOption / OptionThenBase). Events without options
//     fall through to `apply_event_effects`.
//   - For PLAYER country with options, the parent EventInstance
//     is recorded but effects are deferred to a
//     `PlayerCommandKind::ChooseEventOption` command — NO
//     effects and NO followups during this tick.
//   - Followups are CONDITIONAL (RFC-050 §3 "條件連鎖"): each
//     followup must satisfy its own triggers AFTER the parent
//     applies. Multiple matching followups → weighted draw one.
//     Recursion runs depth-N up to
//     `event_engine::kMaxFollowupDepth = 5` with a visited-
//     id_code cycle guard.
//   - `record_match` and `record_followup` append one
//     `LogEntry{category="event_fired", source="event_firer"}`
//     to `state.logs` per fire — that's what surfaces in
//     `events.jsonl`.
//
// The order is observable: `stability::tick` reads the faction
// support / radicalism that `faction::react` just wrote, and
// `economy::tick`'s political-instability drag reads the stability
// `stability::tick` just produced. Tests in
// `tests/systems/monthly_pipeline_test.cpp` pin a state where the
// reversed order would produce a different result, so a future
// refactor cannot silently flip the order.
//
// `tick_all_countries` deliberately does NOT do:
//   * Runner / TimeSystem auto-invocation. The caller decides when a
//     month boundary occurred.
//   * Logging, RNG use, date mutation, save-schema change. Tests
//     verify each of these invariants.
//   * Partial atomicity. If one sub-system fails mid-pipeline, the
//     country is left in whatever partial state the earlier
//     sub-systems wrote. The first sub-system (`faction::react`)
//     does its own pre-flight invalid-id check, so a fully invalid
//     CountryId fails before any mutation.
//
// `tick_all_countries` iterates `state.countries` in vector order and
// fails fast on the first sub-failure. It does not silently skip and
// it does not roll back. M1.9's only role is "compose, don't decide
// recovery policy".
//
// RFC anchors:
//   * RFC-090 §M1 task 1.15 (monthly tick composition).
//   * RFC-060 §2 (system / data separation - the pipeline still
//     mutates only via the three existing sub-systems).
//
// Future M1.x candidates that intentionally do NOT belong here:
//   * Wiring to `TickResult.month_changed` from M0.9 runner.
//   * Storing `last_gdp_growth_rate` on CountryState so a future
//     stability tick can read it as RFC-080 §5 EconomicGrowth.
//   * Policy enactment scheduling.

#ifndef LEVIATHAN_SYSTEMS_MONTHLY_PIPELINE_HPP
#define LEVIATHAN_SYSTEMS_MONTHLY_PIPELINE_HPP

#include <vector>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"
#include "leviathan/systems/ai_policy.hpp"
#include "leviathan/systems/economy_system.hpp"
#include "leviathan/systems/event_engine.hpp"
#include "leviathan/systems/faction_demands.hpp"
#include "leviathan/systems/faction_system.hpp"
#include "leviathan/systems/interest_group_system.hpp"
#include "leviathan/systems/stability_system.hpp"

namespace leviathan::systems::monthly {

// Per-country outcome of a single `tick_country` call. Carries the
// outcome of each sub-system so callers (diagnostics, future logging,
// future UI) can inspect the deltas without re-querying GameState.
struct CountryMonthlyOutcome {
    core::CountryId             country;
    faction::ReactionOutcome    faction;
    stability::StabilityOutcome stability;
    economy::EconomyOutcome     economy;
};

// Aggregate outcome for a `tick_all_countries` call. The `countries`
// vector is filled in the same order as `state.countries`; size is
// always `countries_processed`.
//
// M3.2: after the per-country loop finishes, `tick_all_countries`
// invokes `interest_group::react(state)` once. The
// `interest_groups_updated` counter mirrors that call's
// `groups_updated` field.
//
// M3.3: immediately after M3.2, `interest_group::country_feedback`
// runs to close the reaction loop. The
// `interest_group_countries_updated` counter mirrors that call's
// `countries_updated` field — countries with no matching
// interest groups (or only zero-influence ones) are skipped and
// do not count toward this total.
//
// M3.4: immediately after M3.3,
// `interest_group::authority_pressure` runs to drift each
// country's `bureaucratic_compliance` toward its Bureaucracy-
// kind groups' weighted loyalty. The
// `interest_group_authority_countries_updated` counter mirrors
// that call's `countries_updated` — countries with no
// Bureaucracy-kind interest group (or only zero-influence ones)
// are skipped.
//
// M3.6: `tick_all_countries` also populates the two formula-
// trace vectors below with one row per actually-updated country
// per system. Skipped countries produce no row, so on most
// monthly ticks the row count equals the corresponding
// `*_countries_updated` field. The trace vectors are NOT
// persisted: they are a runtime observability surface consumed
// by the runner's CSV writers and discarded once `end_tick`
// has emitted them.
struct MonthlyOutcome {
    int countries_processed = 0;
    std::vector<CountryMonthlyOutcome> countries;
    int interest_groups_updated                    = 0;
    int interest_group_countries_updated           = 0;
    int interest_group_authority_countries_updated = 0;
    std::vector<interest_group::CountryFeedbackTraceRow>
        interest_group_country_feedback_trace_rows;
    std::vector<interest_group::AuthorityPressureTraceRow>
        interest_group_authority_pressure_trace_rows;

    // Issue #108 fix: AI policy auto-apply counters, populated by
    // the new global step that runs between M3.4 authority_pressure
    // and M5.8 event_engine::tick_events. RFC-010 §2.2 / RFC-090
    // §3.5 expect AI countries to *automatically* select policies
    // every tick; the helper-only ai_policy::apply_selected_policies
    // shipped in RCR-1 is now wired into the monthly pipeline so
    // the simulation actually exercises the AI behavior the RFC
    // describes. Counters mirror ai_policy::ApplyOutcome:
    //   ai_policies_considered  = non-player countries scanned
    //   ai_policies_applied     = successful per-country apply
    //   ai_policies_skipped     = no policy / no selection
    //   ai_policies_failed      = per-country apply returned failure
    //                             (fail-continue: monthly pipeline
    //                              does NOT abort on a single
    //                              country's failure)
    int ai_policies_considered = 0;
    int ai_policies_applied    = 0;
    int ai_policies_skipped    = 0;
    int ai_policies_failed     = 0;

    // M7.1 (RFC-090 §7.1, RFC-020 §7) faction-demand counters.
    // Populated by the new step 8 (generation) and step 9
    // (expire+apply) of `tick_all_countries`. Zero on a month
    // where no faction qualifies for a new demand and no Pending
    // demand has reached its expiry date.
    int faction_demands_factions_considered = 0;
    int faction_demands_generated           = 0;
    int faction_demands_expired             = 0;
    int faction_demands_factions_affected   = 0;

    // M5.8: final global step's outcome. Mirrors the fields of
    // `event_engine::TickOutcome` (events_matched / events_recorded
    // / events_applied / total_effects_applied). Zero on a month
    // where `state.events` is empty or nothing matched. The same
    // counts as `event_engine::tick_events(state)` returned —
    // the monthly pipeline just delegates and forwards.
    event_engine::TickOutcome event_tick;
};

// Run one month-boundary pipeline for a single country in the
// canonical order documented above.
//
// Failure cases (each preserves the documented stop point):
//   * `country` is not a valid index into `state.countries`
//     - detected by `faction::react`, no state mutation.
//   * `faction::react` fails for any other reason
//     - no state mutation (the underlying check is the only failure).
//   * `stability::tick` fails
//     - faction state already mutated; stability / economy untouched.
//   * `economy::tick` fails
//     - faction + stability already mutated; economy untouched.
//
// On success, the returned outcome carries all three sub-outcomes.
core::Result<CountryMonthlyOutcome> tick_country(core::GameState& state,
                                                 core::CountryId  country);

// Run `tick_country` for every entry in `state.countries`, in vector
// order, using each country's vector index as its CountryId. Fails
// fast on the first sub-failure (the partial outcomes captured so far
// are discarded; callers that need partial results should call
// `tick_country` individually).
//
// On an empty `state.countries`, returns success with
// `countries_processed = 0` and an empty `countries` vector.
core::Result<MonthlyOutcome> tick_all_countries(core::GameState& state);

}  // namespace leviathan::systems::monthly

#endif  // LEVIATHAN_SYSTEMS_MONTHLY_PIPELINE_HPP

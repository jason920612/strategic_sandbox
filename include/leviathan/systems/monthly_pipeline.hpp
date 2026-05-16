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
// The order is observable: `stability::tick` reads the faction
// support / radicalism that `faction::react` just wrote, and
// `economy::tick`'s political-instability drag reads the stability
// `stability::tick` just produced. Tests in
// `tests/systems/monthly_pipeline_test.cpp` pin a state where the
// reversed order would produce a different result, so a future
// refactor cannot silently flip the order.
//
// M1.9 deliberately does NOT do:
//   * Policy enactment / duration tracking / active-policy container.
//     A policy fires explicitly via `policy::apply_policy_effects`
//     from outside the pipeline; M1.9 has no opinion on when.
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
#include "leviathan/systems/economy_system.hpp"
#include "leviathan/systems/faction_system.hpp"
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
struct MonthlyOutcome {
    int countries_processed = 0;
    std::vector<CountryMonthlyOutcome> countries;
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

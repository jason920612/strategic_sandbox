// InterestGroupSystem - interest-group reactions and reverse-
// direction feedback (M3.2 + M3.3 + M3.4).
//
// M3.2 ships the country -> interest-group direction: each
// interest group's `loyalty` drifts linearly toward its country's
// `stability`, and its `radicalism` drifts linearly toward
// `1.0 - stability`. Both are clamped to `[0, 1]` after the step.
// Nothing else moves at that step.
//
// M3.3 closes the loop in the opposite direction with a small
// `country_feedback` step: each country's `stability` drifts
// toward `1.0 - influence_weighted_radicalism` of its own
// interest groups, at the slower rate
// `kInterestGroupCountryFeedbackRate` (0.02). Only `stability`
// changes — no other country / faction / authority field is
// touched. The feedback step is global (one call processes
// every country), runs AFTER the M3.2 `react` step inside
// `tick_all_countries`, and skips countries with no matching
// groups or zero total influence.
//
// M3.4 extends the reverse direction with a slower second
// channel that feeds the M2.16 government-authority block. The
// `authority_pressure` step drifts each country's
// `government_authority.bureaucratic_compliance` toward the
// influence-weighted loyalty of its **Bureaucracy-kind**
// interest groups at rate
// `kInterestGroupAuthorityPressureRate` (0.01). Only the
// bureaucratic_compliance sub-field of authority is touched —
// `military_loyalty`, `intelligence_capability`, and
// `media_control` stay where they are. The step is global,
// runs AFTER M3.3's `country_feedback` inside
// `tick_all_countries`, and skips countries with no matching
// Bureaucracy groups or zero total influence among them.
//
// Rate ladder (deliberately decreasing so the closed loop
// stays well-damped):
//   group mood        (M3.2)  0.05
//   country stability (M3.3)  0.02
//   authority         (M3.4)  0.01
//
// Reaction rules (M3.2):
//
//   loyalty    += (country.stability         - loyalty)    * 0.05
//   radicalism += ((1.0 - country.stability) - radicalism) * 0.05
//
// `influence`, `kind`, `country`, `id_code`, and `name` are all
// UNCHANGED. M3.2 only updates the two ratios that future
// reaction-driven gameplay (events, command resistance, faction
// reactions, eventually coup / strike / civil-war risk) is most
// likely to consume.
//
// What this system DOES (M3.2):
//   * Preflight-validates every `group.country` against
//     `state.countries` BEFORE mutating any group. If any group
//     has an out-of-range or invalid country index, `react`
//     returns failure and no group's `loyalty` / `radicalism`
//     has been touched. Atomicity across the list is documented
//     and tested.
//   * Iterates `state.interest_groups` in vector order and
//     applies the two-line formula above.
//   * Returns a `ReactionOutcome` whose `groups_updated` matches
//     `state.interest_groups.size()` on success.
//   * Pure read of every input field; no logs, no RNG, no time
//     advancement, no event emission.
//
// What this system does NOT do (deliberate non-goals for M3.2):
//   * No `influence` drift. Influence is structural-political
//     weight; M3.2 doesn't model how it moves.
//   * No `corruption` / `government_authority` / `legitimacy`
//     input — only `country.stability` drives the drift.
//   * No per-`InterestGroupKind` rate. The 0.05 constant is the
//     same for every kind.
//   * No weighted multi-input formula.
//   * No country aggregate effect — interest groups react to the
//     country but do NOT push back on `country.stability` etc.
//     The reverse direction is reserved for M3.3+.
//   * No event triggers, no `state.logs` entry, no AI, no UI,
//     no command-system integration, no new
//     `PlayerCommandKind`, no scenario hook.
//   * No save schema change. The M3.1 v11 shape is sufficient.
//   * No coup, strike, protest, civil war, cross-border
//     behaviour.

#ifndef LEVIATHAN_SYSTEMS_INTEREST_GROUP_SYSTEM_HPP
#define LEVIATHAN_SYSTEMS_INTEREST_GROUP_SYSTEM_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::interest_group {

// Drift rate applied per monthly step. All `InterestGroupKind`
// variants share the same rate in M3.2; per-kind tuning is
// deferred until gameplay evidence asks for it.
inline constexpr double kInterestGroupReactionRate = 0.05;

struct ReactionOutcome {
    // Count of interest groups whose `loyalty` / `radicalism`
    // were updated. On a successful `react` call, equal to
    // `state.interest_groups.size()` (every group gets the
    // formula applied unconditionally; deltas may be zero when
    // already at the target). Zero when `state.interest_groups`
    // is empty.
    int groups_updated = 0;
};

// Apply one step of M3.2 interest-group reaction to every group
// in `state.interest_groups`.
//
// Preconditions:
//   * Every `group.country` in `state.interest_groups` must be
//     a valid index into `state.countries`. The function pre-
//     flight-validates the whole list BEFORE mutating any
//     group, so a single bad entry leaves every other group
//     untouched.
//
// On success the function:
//   * Drifts each group's `loyalty` toward
//     `state.countries[group.country].stability` at
//     `kInterestGroupReactionRate`.
//   * Drifts each group's `radicalism` toward
//     `1.0 - state.countries[group.country].stability` at
//     the same rate.
//   * Clamps both fields to `[0, 1]` after the step.
//   * Returns a `ReactionOutcome` with
//     `groups_updated == state.interest_groups.size()`.
//
// `react` is a pure read of every input field except the two
// it mutates. It does NOT touch `influence`, `state.logs`,
// `state.rng`, `state.current_date`, country state, faction
// state, policy state, or `state.applied_commands`.
core::Result<ReactionOutcome> react(core::GameState& state);

// ===========================================================================
// M3.3 - interest-group -> country feedback
// ===========================================================================

// Drift rate applied to `country.stability` per monthly step,
// chosen slower than `kInterestGroupReactionRate` so the
// closed loop (M3.2 + M3.3) cannot oscillate quickly. Uniform
// across countries and `InterestGroupKind` variants.
inline constexpr double kInterestGroupCountryFeedbackRate = 0.02;

struct CountryFeedbackOutcome {
    // Count of countries whose `stability` was updated. A country
    // with zero matching interest groups, or whose matching groups
    // all have `influence <= 0.0`, is skipped (its stability stays
    // byte-identical) and does NOT count toward this total.
    int countries_updated = 0;
};

// Apply one step of M3.3 interest-group -> country feedback.
//
// For each country index `ci`:
//
//   weighted_radicalism =
//     sum(g.influence * g.radicalism)  /  sum(g.influence)
//     over every g in state.interest_groups with
//     g.country.value() == ci and g.influence > 0.0
//
//   if weight_sum > 0:
//       target = 1.0 - weighted_radicalism
//       country.stability += (target - country.stability) *
//                            kInterestGroupCountryFeedbackRate
//       country.stability = clamp(country.stability, 0.0, 1.0)
//
//   otherwise the country is skipped (no mutation).
//
// Preconditions (preflight-validated BEFORE any mutation; a
// single bad entry leaves every country untouched):
//   * Every `g.country` in `state.interest_groups` indexes into
//     `state.countries`.
//   * Every `g.influence` and `g.radicalism` is finite and in
//     `[0, 1]`.
//   * Every `country.stability` is finite and in `[0, 1]`.
//
// Failure cases:
//   * Any precondition above violated.
//
// Pure read of every other input field: `country_feedback` does
// NOT touch interest groups, factions, policies, government
// authority, legitimacy, corruption, central_control,
// administrative_efficiency, RNG, logs, the date, or applied
// commands. The single mutation surface is `country.stability`.
core::Result<CountryFeedbackOutcome> country_feedback(
    core::GameState& state);

// ===========================================================================
// M3.4 - interest-group -> government_authority pressure
// ===========================================================================

// Drift rate applied to
// `country.government_authority.bureaucratic_compliance` per
// monthly step. Slower than `kInterestGroupCountryFeedbackRate`
// (0.02) so the closed loop's outermost leg (group mood ->
// stability -> authority) stays well-damped. Uniform across
// countries; no per-kind, per-country, or per-output variant.
inline constexpr double kInterestGroupAuthorityPressureRate = 0.01;

struct AuthorityPressureOutcome {
    // Count of countries whose
    // `government_authority.bureaucratic_compliance` was updated.
    // A country with no `Bureaucracy`-kind interest group, or
    // whose matching Bureaucracy groups all have `influence <=
    // 0.0`, is skipped (its compliance stays byte-identical) and
    // does NOT count toward this total.
    int countries_updated = 0;
};

// Apply one step of M3.4 interest-group -> government-authority
// pressure.
//
// For each country index `ci`:
//
//   bureaucracy_loyalty =
//     sum(g.influence * g.loyalty)  /  sum(g.influence)
//     over every g in state.interest_groups with
//         g.country.value() == ci
//         g.kind == InterestGroupKind::Bureaucracy
//         g.influence > 0.0
//
//   if weight_sum > 0:
//       target  = bureaucracy_loyalty
//       country.government_authority.bureaucratic_compliance
//           += (target - bureaucratic_compliance) *
//              kInterestGroupAuthorityPressureRate
//       country.government_authority.bureaucratic_compliance =
//           clamp(..., 0.0, 1.0)
//
//   otherwise the country is skipped (no mutation).
//
// Preconditions (preflight-validated BEFORE any mutation; a
// single bad entry leaves every country untouched):
//   * Every `g.country` in `state.interest_groups` indexes into
//     `state.countries`.
//   * Every `g.influence` is finite and in `[0, 1]`.
//   * Every `g.loyalty` is finite and in `[0, 1]`.
//   * Every country's
//     `government_authority.bureaucratic_compliance` is finite
//     and in `[0, 1]`.
//
// `radicalism` and `country.stability` are NOT consulted (M3.4
// reads only the Bureaucracy-kind loyalty aggregate), so they
// are not preflighted here. Same for the other three
// `government_authority` sub-fields (military_loyalty /
// intelligence_capability / media_control) — `authority_pressure`
// neither reads nor writes them.
//
// Failure cases:
//   * Any precondition above violated.
//
// Pure read of every other input field: `authority_pressure`
// does NOT touch interest groups, factions, policies, country
// stability / legitimacy / corruption / central_control /
// administrative_efficiency, the other three authority sub-
// fields, RNG, logs, the date, or applied commands. The
// single mutation surface is
// `country.government_authority.bureaucratic_compliance`.
core::Result<AuthorityPressureOutcome> authority_pressure(
    core::GameState& state);

}  // namespace leviathan::systems::interest_group

#endif  // LEVIATHAN_SYSTEMS_INTEREST_GROUP_SYSTEM_HPP

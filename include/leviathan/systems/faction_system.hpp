// FactionSystem - one step of faction-state reaction.
//
// M1.6 introduces the FIRST faction-side dynamics. Per the M1.5
// review, scope is deliberately tiny: an explicit-call free function
// that applies two linear-toward-equilibrium reaction rules and
// returns. There is no monthly tick, no AI, no event integration,
// no type-specific behaviour, and no faction-vs-faction interaction
// yet.
//
// Reaction rules (M1.6):
//
//   loyalty += (country.stability  - loyalty) * 0.10
//   support += (country.legitimacy - support) * 0.05
//
// Both ratios are clamped to [0, 1] after the step.
//
// `influence`, `radicalism`, `resources`, `country_id_code`, `type`,
// and `preferred_policies` are all UNCHANGED. Their dynamics belong
// in later sub-milestones once balance / test cases demand them.
//
// Why these two rules first?
//   * `loyalty -> stability` is the most defensible single link in
//     RFC-080 §5 (stability sustains regime loyalty).
//   * `support -> legitimacy` lets a faction's popular backing track
//     the regime's perceived right to rule.
//   * Both are pure linear convergence with documented rates, so the
//     test suite can verify them with exact arithmetic.

#ifndef LEVIATHAN_SYSTEMS_FACTION_SYSTEM_HPP
#define LEVIATHAN_SYSTEMS_FACTION_SYSTEM_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::faction {

struct ReactionOutcome {
    // Count of factions whose state was touched (any faction whose
    // `country` matches the argument). Equal to the number of
    // factions in `country` whether or not the deltas were zero.
    int factions_updated = 0;
};

// Reaction-rule rate constants. Exposed in the header so tests can
// reference them rather than re-hard-coding the literals.
inline constexpr double kLoyaltyDriftRate = 0.10;
inline constexpr double kSupportDriftRate = 0.05;

// Apply one step of faction reactions to every faction belonging to
// `country` in `state`. See the rule list at the top of this header.
//
// Failure cases:
//   - `country` is not a valid index into `state.countries`
// On failure, no faction is modified.
core::Result<ReactionOutcome> react(core::GameState& state,
                                    core::CountryId  country);

}  // namespace leviathan::systems::faction

#endif  // LEVIATHAN_SYSTEMS_FACTION_SYSTEM_HPP

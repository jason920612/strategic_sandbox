// Issue #112: per-country / per-category weighted-random event
// draw + recursive conditional followup chain + author-controlled
// base/option effect mode.
//
// New semantics for `event_engine::tick_events(state)`:
//
//   for each country in state.countries (vector order):
//     matches = match_events_for_country(state, country.id)
//     if matches empty: continue            // no rng consumption
//     bucket matches by EventDefinition.category
//       (first-matched-event vector-order; NOT lexicographic; NOT
//        unordered_map iteration)
//     for each bucket (in first-matched-event vector-order):
//       candidates = filter bucket to weight > 0
//       if empty: continue
//       weights = [weight(c) for c in candidates]
//       choice = random::weighted_choice(state.rng, weights, tag)
//       fire selected event:
//         - record parent EventInstance + LogEntry
//         - if player country AND options non-empty:
//             append PendingPlayerEvent, apply NO effects,
//             SKIP followup recursion (resumes on
//             PlayerCommandKind::ChooseEventOption command)
//         - else if options non-empty:
//             headless AI chooser picks the best-scored option;
//             apply_option_effects_with_mode per
//             definition.option_effect_mode
//         - else:
//             apply_event_effects (base path)
//         - then recurse depth-1+ conditional followup chain (A3)
//
// Conditional followup recursion (post-parent-apply, depth-N
// single-path):
//
//   visited = { parent.event_id_code }
//   depth   = 0
//   current = parent_node
//   while depth < kMaxFollowupDepth:
//     fids = resolve_followup_ids(state, current.definition)
//     pool = []
//     for fidx in fids:
//       fdef = state.events[fidx]
//       if fdef.id_code in visited: continue         // cycle guard
//       if !evaluate(state, fdef): continue          // F1+G1
//       w = weight_for_event(state, fidx)
//       if w > 0: pool.append((fidx, fdef, w))
//     if pool empty: break
//     chosen = random::weighted_choice(state.rng, weights, tag)
//     record_followup(state, current.instance, fchosen, current_date)
//       // immediate-predecessor: `followup_of` metadata points
//       // to current.event_id_code, NOT to the original root
//     apply_event_effects OR apply_option_effects_with_mode based
//       on fchosen.options + option_effect_mode (followups follow
//       the same author-controlled mode as parents)
//     visited.add(fchosen.id_code)
//     current = (fidx, child_instance, fchosen.id_code)
//     depth += 1
//
// Both guards run simultaneously:
//   - visited-set guard (event_id_code-keyed) — stops A→B→A and
//     any revisit. Faithful cycle detection because the loader
//     rejects duplicate id_codes.
//   - depth guard (kMaxFollowupDepth = 5) — independent stop for
//     long non-cyclic chains.
//
// state.rng consumption (one draw per event_engine::tick_events
// internal call):
//   - no matched events for a country → no draw, no RNG advance.
//   - one selectable candidate in a (country, category) bucket →
//     ONE draw (random::weighted_choice always consumes one draw,
//     even for singleton pools; per random_service.hpp:64-66).
//   - N selectable candidates → ONE draw per (country, category)
//     bucket fire + ONE draw per followup chain step.
//
// Canonical 1930_minimal preserves zero-fire under this semantics
// (its 2 events deliberately never match canonical authored state)
// — so M1.17 / M2 / M3 / M4 / M5 byte-identical determinism
// baselines stay green. Compliance 1930_rfc_compliance scenario
// fires events and consumes RNG; its baselines move (called out
// in the PR description).

#ifndef LEVIATHAN_SYSTEMS_EVENT_ENGINE_HPP
#define LEVIATHAN_SYSTEMS_EVENT_ENGINE_HPP

#include <cstddef>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::event_engine {

// Max followup chain depth before recursion stops independent of
// the visited-set cycle guard. Exported so tests can drive a
// MAX_DEPTH-length chain and assert termination.
inline constexpr std::size_t kMaxFollowupDepth = 5;

// One tick_events call's summary. Counts include only events
// that finished successfully; mid-tick apply failure short-
// circuits the call and these reflect partial progress.
struct TickOutcome {
    int countries_processed              = 0;
    int countries_with_matches           = 0;
    int categories_processed             = 0;
    // Total trigger-matched events found across all (country,
    // category) buckets BEFORE the weighted draw filters down to
    // one per bucket. Provided so tests can verify "this state
    // produces N matches" independent of how the draw decided.
    int events_matched                   = 0;
    int events_drawn                     = 0;
    int events_recorded                  = 0;
    int events_applied                   = 0;
    int events_pending_player_choice     = 0;
    int total_effects_applied            = 0;
    int events_with_options              = 0;
    int events_with_followups            = 0;
    int followups_recorded               = 0;
    int total_followup_effects_applied   = 0;
};

// One round of event-engine processing on `state`. Drives per-
// country / per-category weighted draw + conditional followup
// recursion + author-controlled option mode + player-choice
// deferral per the header doc above.
//
// fired_on = state.current_date for every recorded instance.
//
// On a per-event-apply failure the call returns Result::failure
// after appending the failed event's parent EventInstance + its
// `event_fired` LogEntry; the caller decides whether to roll
// forward. Subsequent draws / followups for the failing event
// are skipped.
core::Result<TickOutcome> tick_events(core::GameState& state);

// Issue #112: Public followup recursion entry-point used by both
// `tick_events` (when a non-deferred parent fires) and
// `commands::dispatch_one` (after a player resolves a pending
// option choice with `ChooseEventOption`).
//
// Parameters:
//   * parent_instance_index — index into `state.event_history` for
//     the just-fired parent EventInstance. Its `event_id_code` and
//     the corresponding `state.events[parent_definition_index]`
//     entry seed the chain.
//   * parent_definition_index — index into `state.events` for the
//     definition whose `followup_event_ids` will be resolved.
//   * country_for_option — country to score followup options
//     against (mirrors the parent's actor country).
//   * tag_prefix — `random::weighted_choice` tag prefix (so player-
//     command-driven recursion gets a distinguishable RNG-tag
//     namespace from the engine-driven one).
//
// Returns a partial TickOutcome containing only the followups_*
// counters. State is updated in place; on per-followup-effect
// failure the chain stops and returns Result::failure.
struct FollowupOutcome {
    int followups_recorded             = 0;
    int total_followup_effects_applied = 0;
};

core::Result<FollowupOutcome>
recurse_followups_from_event(core::GameState&             state,
                             std::size_t                  parent_instance_index,
                             std::size_t                  parent_definition_index,
                             const core::CountryState&    country_for_option,
                             const std::string&           tag_prefix);

}  // namespace leviathan::systems::event_engine

#endif  // LEVIATHAN_SYSTEMS_EVENT_ENGINE_HPP

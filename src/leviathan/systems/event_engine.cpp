// Issue #112: event engine rewrite — per-country / per-category
// weighted-random draw + conditional followup recursion +
// author-controlled option effect mode + player-choice deferral.
//
// See include/leviathan/systems/event_engine.hpp for the full
// semantics. This file is the binding of all the M5.x / RCR-1 /
// issue-110 helpers plus the new random::weighted_choice path.

#include "leviathan/systems/event_engine.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/systems/event_effects.hpp"
#include "leviathan/systems/event_evaluator.hpp"
#include "leviathan/systems/event_firer.hpp"
#include "leviathan/systems/random_service.hpp"

namespace leviathan::systems::event_engine {
namespace {

// Compute one event's current weight using `rank_weighted_events`
// semantics, but for a SINGLE event index (avoids re-ranking the
// whole vector inside the conditional-followup loop). Reuses the
// existing `rank_weighted_events` for the country-loop bucket
// build; followup recursion calls this per-followup.
double event_weight_for_index(const core::GameState& state,
                              std::size_t            event_index) {
    if (event_index >= state.events.size()) {
        return 0.0;
    }
    // We rebuild via rank_weighted_events and pick. rank_weighted_events
    // is O(E * M) per call; for the small E we have, this is cheap
    // and avoids duplicating modifier-evaluation logic.
    const auto ranked = event_evaluator::rank_weighted_events(state);
    for (const auto& c : ranked) {
        if (c.event_index == event_index) {
            return c.weight;
        }
    }
    return 0.0;
}

// Helper: apply a fired event's effects according to its option /
// option_effect_mode contract. Used by both the parent fire path
// and the followup recursion.
//
//   - definition.options EMPTY → apply_event_effects (base)
//   - definition.options NON-EMPTY → state-based option chooser
//     + apply_option_effects_with_mode per
//       definition.option_effect_mode
//
// Returns the per-call ApplyOutcome (effects_applied count) or
// the failure error.
core::Result<event_effects::ApplyOutcome>
apply_for_ai(core::GameState&                state,
             const core::EventInstance&      instance,
             const core::EventDefinition&    def,
             const core::CountryState*       country_for_option_score) {
    if (def.options.empty()) {
        return event_effects::apply_event_effects(state, instance, def);
    }
    // Pick option deterministically from country state. country
    // is required when options non-empty for the chooser.
    if (country_for_option_score == nullptr) {
        return core::Result<event_effects::ApplyOutcome>::failure(
            "event_engine: apply_for_ai called with non-empty "
            "options but no country to score against (" +
            instance.event_id_code + ")");
    }
    auto opt_r = event_effects::select_best_option_for_country(
        state, *country_for_option_score, def);
    if (!opt_r) {
        return core::Result<event_effects::ApplyOutcome>::failure(
            std::move(opt_r.error()));
    }
    const core::EventOption* opt = opt_r.value();
    if (opt == nullptr) {
        // Should be unreachable: options non-empty ⇒ chooser returns
        // a pointer. Defensive guard preserved as success-with-0 so
        // caller can detect "scorer agreed there's nothing to apply".
        return core::Result<event_effects::ApplyOutcome>::success(
            event_effects::ApplyOutcome{});
    }
    return event_effects::apply_option_effects_with_mode(
        state, instance, def, *opt, def.option_effect_mode);
}

// Bucket key for first-matched-event vector-order grouping. We
// use a vector-of-pairs so iteration order is deterministic and
// reviewable (NOT std::unordered_map).
struct CategoryBucket {
    std::string                            category;
    std::vector<event_evaluator::EventMatch> matches;
};

std::vector<CategoryBucket>
bucket_by_category_in_first_seen_order(
        const std::vector<event_evaluator::EventMatch>& matches,
        const core::GameState&                          state) {
    std::vector<CategoryBucket> out;
    for (const auto& m : matches) {
        const auto& def = state.events[m.event_index];
        const std::string& cat = def.category;
        // Find existing bucket for this category, else create.
        auto it = std::find_if(
            out.begin(), out.end(),
            [&](const CategoryBucket& b) { return b.category == cat; });
        if (it == out.end()) {
            CategoryBucket b;
            b.category = cat;
            b.matches.push_back(m);
            out.push_back(std::move(b));
        } else {
            it->matches.push_back(m);
        }
    }
    return out;
}

struct FollowupNode {
    std::size_t definition_index;
    std::size_t instance_index;
    std::string event_id_code;
};

// Recurse the conditional followup chain starting from `parent_node`.
// Updates `out` counters as work proceeds. Returns first error
// encountered (in which case the chain stops; partial effects so
// far stay applied). Internal helper used by the public
// `tick_events` and `recurse_followups_from_event` entry-points.
core::Result<FollowupOutcome>
recurse_followups_impl(core::GameState&                  state,
                       FollowupNode                      parent_node,
                       const core::CountryState&         country_for_option,
                       const std::string&                tag_prefix,
                       std::unordered_set<std::string>*  tick_fired_in /* may be nullptr */,
                       int*                              pending_player_followups_out /* may be nullptr */) {
    FollowupOutcome out;
    std::unordered_set<std::string> visited;
    visited.insert(parent_node.event_id_code);

    // Issue #112 §3 (player-followup deferral): when a followup
    // event has options AND fires for the player's country, the
    // chain pauses — record the followup, append a
    // PendingPlayerEvent, apply NO effects, return out. The
    // player must resolve via ChooseEventOption before the chain
    // continues. AI/headless followups (non-player country, OR
    // followup without options) still auto-apply via apply_for_ai
    // as before.
    const bool actor_is_player =
        state.player_country.valid()
        && state.player_country == country_for_option.id;

    FollowupNode current = std::move(parent_node);

    for (std::size_t depth = 0; depth < kMaxFollowupDepth; ++depth) {
        const auto& current_def = state.events[current.definition_index];
        auto fids_r =
            event_effects::resolve_followup_ids(state, current_def);
        if (!fids_r) {
            return core::Result<FollowupOutcome>::failure(
                std::move(fids_r.error()));
        }
        const auto& fids = fids_r.value();
        if (fids.empty()) {
            break;
        }

        struct PoolEntry {
            std::size_t definition_index;
            double      weight;
        };
        std::vector<PoolEntry> pool;
        for (std::size_t fidx : fids) {
            const auto& fdef = state.events[fidx];
            if (visited.count(fdef.id_code) > 0) {
                continue;   // cycle guard
            }
            // Issue #112 tick-level dedup: an event that's
            // already fired in this tick (via any prior bucket
            // draw OR chain) is not eligible to fire again.
            if (tick_fired_in != nullptr &&
                tick_fired_in->count(fdef.id_code) > 0) {
                continue;
            }
            if (!event_evaluator::evaluate(state, fdef)) {
                continue;   // F1+G1: conditional chain — followup
                            // must match its own triggers AFTER
                            // parent effects applied.
            }
            const double w = event_weight_for_index(state, fidx);
            if (w > 0.0) {
                pool.push_back(PoolEntry{fidx, w});
            }
        }
        if (pool.empty()) {
            break;
        }

        std::vector<double> weights;
        weights.reserve(pool.size());
        for (const auto& p : pool) { weights.push_back(p.weight); }
        const std::string tag = tag_prefix + ".followup." +
                                current.event_id_code;
        const std::size_t chosen_idx =
            random::weighted_choice(state.rng, weights, tag);
        if (chosen_idx >= pool.size()) {
            // Defensive: weighted_choice contract guarantees
            // [0, weights.size()); never reached in practice.
            break;
        }
        const std::size_t fidx = pool[chosen_idx].definition_index;
        const auto& fdef = state.events[fidx];

        // Record the followup. record_followup uses the IMMEDIATE
        // PREDECESSOR (current.instance_index) so events.jsonl
        // `followup_of` metadata points to the direct chain
        // parent — chain A → B → C records C.followup_of = B,
        // not C.followup_of = A.
        const auto& current_instance =
            state.event_history[current.instance_index];
        event_firer::record_followup(
            state, current_instance, fdef, state.current_date);
        const std::size_t child_instance_index =
            state.event_history.size() - 1;
        out.followups_recorded += 1;
        visited.insert(fdef.id_code);
        if (tick_fired_in != nullptr) {
            tick_fired_in->insert(fdef.id_code);
        }

        // Issue #112 §3: if the followup has options AND fires
        // for the player country, defer effects + halt the
        // chain. A PendingPlayerEvent is appended; the player
        // must resolve via ChooseEventOption before the chain
        // can resume. Without this guard, AI/headless option
        // selection would auto-pick options[best] and bypass
        // the player's choice for downstream followups.
        if (actor_is_player && !fdef.options.empty()) {
            core::PendingPlayerEvent pending;
            pending.event_history_index = child_instance_index;
            pending.event_id_code       = fdef.id_code;
            pending.country_id_code     = country_for_option.id_code;
            state.pending_player_events.push_back(std::move(pending));
            if (pending_player_followups_out != nullptr) {
                *pending_player_followups_out += 1;
            }
            break;   // chain pauses; ChooseEventOption resumes it
        }

        // Apply the followup's effects (mode-aware just like a
        // parent fire). Non-player country, OR player country
        // with an options-empty followup (base-effects path).
        const auto& child_instance =
            state.event_history[child_instance_index];
        auto eff_r = apply_for_ai(state, child_instance, fdef,
                                  &country_for_option);
        if (!eff_r) {
            return core::Result<FollowupOutcome>::failure(
                std::move(eff_r.error()));
        }
        out.total_followup_effects_applied +=
            eff_r.value().effects_applied;

        current = FollowupNode{fidx, child_instance_index, fdef.id_code};
    }

    return core::Result<FollowupOutcome>::success(std::move(out));
}

}  // namespace

core::Result<TickOutcome> tick_events(core::GameState& state) {
    TickOutcome out;

    for (std::size_t ci = 0; ci < state.countries.size(); ++ci) {
        // Issue #112 §3 (E1): each COUNTRY rolls independently;
        // the same event template MAY fire for multiple countries
        // in the same tick. The dedup set below is per-country
        // scope: it prevents an event from firing twice for the
        // SAME country (e.g. once as a category-direct match and
        // once as a followup-chain result) but does NOT block
        // OTHER countries from firing the same template.
        std::unordered_set<std::string> fired_in_tick;
        out.countries_processed += 1;
        const core::CountryState& country = state.countries[ci];
        const core::CountryId cid = country.id;

        // Per-country match pool — strictly scoped to this country.
        const auto matches =
            event_evaluator::match_events_for_country(state, cid);
        out.events_matched += static_cast<int>(matches.size());
        if (matches.empty()) {
            continue;   // no rng consumption for this country
        }
        out.countries_with_matches += 1;

        // Bucket matches by category in first-matched-event
        // vector-order (deterministic, reviewable iteration).
        const auto buckets =
            bucket_by_category_in_first_seen_order(matches, state);

        const bool is_player_country =
            state.player_country.valid() && state.player_country == cid;

        for (const auto& bucket : buckets) {
            out.categories_processed += 1;

            // Compute weights for matched events in this bucket.
            // Weight filter: > 0 only. Negative or zero weight is
            // excluded from the draw pool (documented behaviour).
            struct DrawEntry {
                std::size_t event_index;
                double      weight;
            };
            std::vector<DrawEntry> draw_pool;
            for (const auto& m : bucket.matches) {
                // Skip events that have already fired for THIS
                // country this tick — either via a different
                // category bucket OR via this country's followup
                // recursion. The `fired_in_tick` set is per-
                // country (declared at the top of the country
                // loop), so it does NOT block another country
                // from firing the same template (RFC E1: same
                // template may fire for multiple countries
                // independently in the same tick).
                if (fired_in_tick.count(
                        state.events[m.event_index].id_code) > 0) {
                    continue;
                }
                const double w = event_weight_for_index(state, m.event_index);
                if (w > 0.0) {
                    draw_pool.push_back(DrawEntry{m.event_index, w});
                }
            }
            if (draw_pool.empty()) {
                continue;
            }

            std::vector<double> weights;
            weights.reserve(draw_pool.size());
            for (const auto& d : draw_pool) {
                weights.push_back(d.weight);
            }
            const std::string tag =
                "event_engine.tick_events." + country.id_code +
                "." + bucket.category;
            const std::size_t chosen_idx =
                random::weighted_choice(state.rng, weights, tag);
            if (chosen_idx >= draw_pool.size()) {
                continue;   // defensive; weighted_choice guarantees in-range
            }
            const std::size_t event_index = draw_pool[chosen_idx].event_index;
            out.events_drawn += 1;

            // Find the EventMatch for the chosen event (need its
            // triggers / actors for record_match).
            const event_evaluator::EventMatch* match_ptr = nullptr;
            for (const auto& m : bucket.matches) {
                if (m.event_index == event_index) {
                    match_ptr = &m;
                    break;
                }
            }
            if (match_ptr == nullptr) {
                continue;   // defensive; bucket built from matches
            }
            const auto& match = *match_ptr;
            const auto& def   = state.events[event_index];

            // Record the parent event (always — including the
            // player-country pending case, so events.jsonl shows
            // what was offered).
            event_firer::record_match(state, match, state.current_date);
            const std::size_t parent_instance_index =
                state.event_history.size() - 1;
            out.events_recorded += 1;
            fired_in_tick.insert(def.id_code);

            if (!def.options.empty()) {
                out.events_with_options += 1;
            }

            // Player-country deferral: if this country is the
            // player AND the event has options, append a pending
            // entry and skip effect-apply + followup recursion.
            if (is_player_country && !def.options.empty()) {
                core::PendingPlayerEvent pending;
                pending.event_history_index = parent_instance_index;
                pending.event_id_code       = def.id_code;
                pending.country_id_code     = country.id_code;
                state.pending_player_events.push_back(std::move(pending));
                out.events_pending_player_choice += 1;
                continue;   // no effects, no followups
            }

            // Apply parent effects (option-aware for non-player
            // countries; base for events without options).
            const auto& parent_instance =
                state.event_history[parent_instance_index];
            auto eff_r = apply_for_ai(state, parent_instance, def, &country);
            if (!eff_r) {
                return core::Result<TickOutcome>::failure(
                    "tick_events: " + def.id_code + ": " +
                    eff_r.error());
            }
            out.events_applied        += 1;
            out.total_effects_applied += eff_r.value().effects_applied;

            // Conditional followup recursion (depth-N single-path).
            if (!def.followup_event_ids.empty()) {
                out.events_with_followups += 1;
            }
            // Re-fetch country reference because earlier effect
            // application may have mutated state (and any address
            // we held could be invalidated by vector reallocation —
            // though state.countries doesn't grow here, this is
            // belt-and-braces).
            const core::CountryState& refreshed_country =
                state.countries[ci];
            FollowupNode parent_node{
                event_index, parent_instance_index, def.id_code};
            int pending_player_followups = 0;
            auto follow_r = recurse_followups_impl(
                state, parent_node, refreshed_country, tag,
                &fired_in_tick, &pending_player_followups);
            if (!follow_r) {
                return core::Result<TickOutcome>::failure(
                    "tick_events: " + def.id_code + " followup: " +
                    follow_r.error());
            }
            out.followups_recorded +=
                follow_r.value().followups_recorded;
            out.total_followup_effects_applied +=
                follow_r.value().total_followup_effects_applied;
            out.events_pending_player_choice += pending_player_followups;
        }
    }

    return core::Result<TickOutcome>::success(std::move(out));
}

core::Result<FollowupOutcome>
recurse_followups_from_event(core::GameState&             state,
                             std::size_t                  parent_instance_index,
                             std::size_t                  parent_definition_index,
                             const core::CountryState&    country_for_option,
                             const std::string&           tag_prefix) {
    if (parent_instance_index >= state.event_history.size()) {
        return core::Result<FollowupOutcome>::failure(
            "recurse_followups_from_event: parent_instance_index "
            "out of range for state.event_history.size()");
    }
    if (parent_definition_index >= state.events.size()) {
        return core::Result<FollowupOutcome>::failure(
            "recurse_followups_from_event: parent_definition_index "
            "out of range for state.events.size()");
    }
    FollowupNode node{
        parent_definition_index,
        parent_instance_index,
        state.events[parent_definition_index].id_code,
    };
    // Public entry-point (called from commands::dispatch_one after
    // ChooseEventOption) doesn't have a tick-scope dedup set —
    // the player command happens between ticks, so per-tick
    // semantics don't apply. Pass nullptr for the dedup set.
    // The pending_player_followups counter is plumbed through so
    // a player-country followup chain that hits another options-
    // event correctly pauses again (the player gets another
    // ChooseEventOption prompt for the downstream chain).
    int pending_player_followups = 0;
    return recurse_followups_impl(
        state, node, country_for_option, tag_prefix, nullptr,
        &pending_player_followups);
}

}  // namespace leviathan::systems::event_engine

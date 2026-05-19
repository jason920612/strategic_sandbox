// EventEffects - apply an EventDefinition's effects against an
// EventInstance's matched actors.
//
// M5.6 introduced the base effects applicator. The current event
// engine calls this module from `event_engine::tick_events` and from
// `commands::dispatch_one` when a deferred player option resolves.
//
// Actor-selection policy (M5.6-era):
//
//   ALL effects in `definition.effects` apply to ONE country —
//   the country resolved from `instance.actors.front()`'s
//   `country_id_code`. This is the simplest meaningful policy
//   the user described as "let event instance + matched actors
//   apply small PolicyEffect" — the matched actor list tells the
//   applicator "fire FOR this country". Multi-actor cross-product
//   semantics, per-effect actor selection, and weighted-actor
//   selection are all deferred to a dedicated selection-policy
//   sub-milestone (per the PR #92 review note: don't sneak
//   selection-policy variants into the firing/effects PR).
//
//   Empty `instance.actors` → Result::failure (post-M6.7
//     strict-fallback hardening; the previous "vacuous no-op
//     success" silently tolerated a structurally inconsistent
//     EventInstance and is no longer accepted).
//   Cannot resolve `country_id_code` to a country in
//     `state.countries` → Result::failure (state unchanged thanks
//     to the M1.5 pre-flight atomicity).
//
// Effect resolution:
//
//   Effects use the existing M1.5 target / op grammar:
//     country.<field>          - direct CountryState field
//     country.budget.<cat>     - one BudgetState category
//     faction:<type>.<field>   - all factions in the actor whose
//                                type matches; 0 matches is a
//                                silent no-op
//
//   Ops: "add", "set". Ratio fields clamped to [0, 1] post-op.
//   Non-finite values rejected at pre-flight. Pre-flight
//   atomicity from M1.5 is inherited: any failure leaves state
//   untouched.
//
// This module still does not decide which events fire, which option
// is presented to the player, or which followup continues a chain.
// Those policies live in `event_engine` / `commands`; this module
// applies already-chosen effects with the M1.5 pre-flight contract.

#ifndef LEVIATHAN_SYSTEMS_EVENT_EFFECTS_HPP
#define LEVIATHAN_SYSTEMS_EVENT_EFFECTS_HPP

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"
#include "leviathan/systems/policy_system.hpp"

namespace leviathan::systems::event_effects {

// One apply_event_effects call's summary.
struct ApplyOutcome {
    int effects_applied         = 0;  // matches definition.effects.size() on success
    int faction_targets_updated = 0;  // mirrors policy::ApplyOutcome semantics
};

// Apply the effects of `definition` to the actor resolved from
// `instance.actors.front().country_id_code`. See header doc for
// the actor-selection policy and deliberate non-goals.
//
// Preconditions (none checked — caller responsibility):
//   - `instance.event_id_code == definition.id_code`
//   - `definition` is a legitimate match for `instance`'s state
//     (the M5.2/M5.3 evaluator + M5.5 firer produced both)
//
// Returns Result::failure if:
//   - `instance.actors` is non-empty but the first actor's
//     `country_id_code` does not resolve to any country in
//     `state.countries`
//   - any effect has a non-finite value (per M1.5 pre-flight)
//   - any effect has an unrecognised op or target (per M1.5)
//
// State is unchanged on failure (inherits M1.5 pre-flight
// atomicity).
//
// Post-M6.7 strict-fallback hardening: `instance.actors` empty
// returns `Result::failure` (was previously a silent vacuous-
// no-op success). Per `feedback_no_silent_degradation`, a
// hand-built EventInstance with no actors is a structurally
// inconsistent input; the engine path never produces one (every
// fired event has at least one actor), so this branch is now
// reserved for surfacing the inconsistency.
core::Result<ApplyOutcome> apply_event_effects(
    core::GameState&             state,
    const core::EventInstance&   instance,
    const core::EventDefinition& definition);

// Legacy deterministic option selector retained for tests and
// diagnostics. Returns a pointer into `definition.options[0]` if the
// options vector is non-empty, or `nullptr` if the definition has no
// options.
//
// Pure read; never mutates `definition`. The returned pointer is
// non-owning and stable across the call but invalidated by any
// subsequent mutation of `definition.options`.
//
// Production event firing does NOT use this helper: non-player
// countries use `select_best_option_for_country`, and player-country
// events are resolved by `PlayerCommandKind::ChooseEventOption`.
const core::EventOption*
select_default_option(const core::EventDefinition& definition);

// Legacy helper: apply the deterministic first option's effects to
// the actor's country.
//
// Composes `select_default_option(definition)` with the M1.5 /
// M5.6 `policy::apply_effects_to_actor` machinery. The actor is
// resolved from `instance.actors.front().country_id_code`
// (same first-actor-wins convention as `apply_event_effects`).
//
// Semantics:
//   - definition.options empty -> success with effects_applied=0
//     (no option to choose); `state` unchanged.
//   - instance.actors empty -> success with effects_applied=0;
//     `state` unchanged.
//   - first option's effects[] applied via
//     policy::apply_effects_to_actor, inheriting M1.5 pre-flight
//     atomicity. On failure the whole call returns Result::failure
//     with `state` unchanged.
//
// The production engine uses `apply_option_effects_with_mode`
// instead so authors can choose OptionOnly / BaseThenOption /
// OptionThenBase. Keep this helper out of the main event path.
core::Result<ApplyOutcome> apply_default_option_effects(
    core::GameState&             state,
    const core::EventInstance&   instance,
    const core::EventDefinition& definition);

// Issue #112: state-based deterministic option chooser. Scores each
// `definition.options` entry using
//   sum over option.effects e: e.value * effect_desire::for_country(country, e.target, state)
// and returns a pointer to the highest-scored option. Ties resolve
// to the lower vector index.
//
// Post-M6.7 hardening (`feedback_api_signature_expresses_failure`):
// signature is now `Result<const core::EventOption*>`. Success
// with `nullptr` indicates `definition.options` is empty (structurally
// "no options to choose"). Failure propagates any
// `effect_desire::for_country` failure (non-finite / out-of-range
// CountryState inputs) so a corrupted state surfaces here rather
// than silently scoring as 0 and picking arbitrary options.
//
// Used by `event_engine::tick_events` for non-player-country events
// (and for non-player-country followups with options). Player
// countries route through the `ChooseEventOption` command surface
// instead — see PendingPlayerEvent + commands.cpp.
//
// Pure read; never mutates state or definition.
core::Result<const core::EventOption*>
select_best_option_for_country(const core::GameState&       state,
                               const core::CountryState&    country,
                               const core::EventDefinition& definition);

// Issue #112: apply the chosen option's effects (and optionally
// definition.effects) according to the author-controlled
// `EventOptionEffectMode`.
//
//   OptionOnly      — only option.effects apply
//   BaseThenOption  — definition.effects first, then option.effects
//   OptionThenBase  — option.effects first, then definition.effects
//
// Same actor-resolution semantics as `apply_event_effects`: the
// first actor's `country_id_code` is resolved to a CountryId; that
// country receives all effects.
//
// Returns failure if actor resolution fails OR if any effect
// rejects at the M1.5 pre-flight. On failure the M1.5 pre-flight
// atomicity per the call leaves that pass's state untouched, but
// in BaseThenOption / OptionThenBase a successful first pass is
// NOT rolled back — callers must treat partial mid-mode failure
// as a soft state.
core::Result<ApplyOutcome>
apply_option_effects_with_mode(
    core::GameState&             state,
    const core::EventInstance&   instance,
    const core::EventDefinition& definition,
    const core::EventOption&     option,
    core::EventOptionEffectMode  mode);

// RFC-090 §5.12 — followup-event-id resolver. Given a
// definition's `followup_event_ids` vector, returns the matching
// `state.events` indices for every id_code.
//
// Post-M6.7 strict-fallback hardening: an unresolvable
// `id_code` (not present in `state.events`) now returns
// `Result::failure` naming the offending id_code. The previous
// silent-skip-and-continue behavior tolerated cross-scenario
// save reloads but masked authoring errors where a followup
// chain references a typoed event id. If a scenario reload
// legitimately needs to tolerate missing followups, the caller
// must do its own pre-resolution check before invoking the
// resolver.
//
// Returns a vector of size == `definition.followup_event_ids.size()`
// on success; the result preserves the order of
// `definition.followup_event_ids`. Empty input → empty success.
// Pure read.
//
// Scope split between resolver + firer:
//   - `resolve_followup_ids` (THIS function) only resolves
//     followup id_code strings to `state.events` indices.
//   - `event_firer::record_followup` is the matching firing
//     primitive: takes one resolved followup definition + the
//     parent instance + a date, and appends one EventInstance
//     to event_history plus one `event_fired` LogEntry to state.logs.
//   - `event_engine::tick_events` and
//     `event_engine::recurse_followups_from_event` own recursive,
//     conditional followup selection and depth/cycle guards.
core::Result<std::vector<std::size_t>>
resolve_followup_ids(const core::GameState&       state,
                     const core::EventDefinition& definition);

}  // namespace leviathan::systems::event_effects

#endif  // LEVIATHAN_SYSTEMS_EVENT_EFFECTS_HPP

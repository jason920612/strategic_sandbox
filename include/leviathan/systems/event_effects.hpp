// EventEffects - apply an EventDefinition's effects against an
// EventInstance's matched actors.
//
// M5.6 ships the effects-applicator skeleton: a free-function
// bridge that takes a fired M5.4 `core::EventInstance` (produced
// by the M5.5 firer) plus the matching M5.1 `core::EventDefinition`
// and runs the definition's `effects` through the shared M1.5
// policy machinery (`policy::apply_effects_to_actor`).
//
// M5.6 still does NOT integrate with the runner or monthly
// pipeline, NOT change `events.jsonl` semantics, NOT add any new
// artefact, NOT bump the save format. It is the missing
// effects-application brick — the runner-integration sub-milestone
// will compose evaluator (M5.2) + actor binding (M5.3) + firer
// (M5.5) + this applicator into a `tick_events(state)` style call
// in its own dedicated PR.
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
//   Empty `instance.actors` → no-op success (0 effects applied).
//   Cannot resolve `country_id_code` to a country in
//   `state.countries` → Result::failure (state unchanged thanks
//   to the M1.5 pre-flight atomicity).
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
// What M5.6 does NOT do:
//
//   no runner / monthly integration
//   no events.jsonl change
//   no log entry on apply (no state.logs append)
//   no state.applied_commands append
//   no new artefact (still 10)
//   no save format bump (still v14)
//   no per-effect actor selection (single-actor policy)
//   no multi-actor cross-product / weighted / random selection
//   no broader trigger ops / targets in the effect grammar
//     (effects inherit the M1.5 grammar; that's the contract)
//   no cooldown / historical-once gating (caller policy)
//   no auto-firing — the caller must have already invoked
//     event_firer::record_match (or hand-built the
//     EventInstance) before calling apply_event_effects
//   no new state field
//   no event_evaluator / event_firer / scenario_loader changes

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
// Special case: `instance.actors` empty → success with
// `effects_applied=0`. This models the vacuous-true M5.1 case
// where a definition has no triggers — no actor to direct
// effects at, no effects applied. The caller can detect this
// via `outcome.effects_applied == 0`.
core::Result<ApplyOutcome> apply_event_effects(
    core::GameState&             state,
    const core::EventInstance&   instance,
    const core::EventDefinition& definition);

// RCR-1: RFC-090 §5.4 / §5.8 — deterministic default option
// selector. Returns a pointer into `definition.options[0]` if the
// options vector is non-empty, or `nullptr` if the definition has
// no options (which mirrors the M5.6-era contract where
// `definition.effects` was the only effect path).
//
// Pure read; never mutates `definition`. The returned pointer is
// non-owning and stable across the call but invalidated by any
// subsequent mutation of `definition.options`.
//
// Future expansion: a smarter selector could take a player choice
// or AI heuristic. RCR-1 ships the deterministic first-option
// rule per the user's instruction "deterministic default option
// selection helper".
const core::EventOption*
select_default_option(const core::EventDefinition& definition);

// RCR-1: RFC-090 §5.4 / §5.8 — apply the default option's
// effects to the actor's country.
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
// The applicator deliberately does NOT call `apply_event_effects`
// in addition — `definition.effects` (the base effect list) and
// `options[i].effects` are independent surfaces. A future runner-
// policy that wants to fire both base + option-default effects
// would call this helper alongside `apply_event_effects`. RCR-1
// ships the deterministic primitive; runner wiring is intentionally
// out of scope (consistent with the rest of the event-engine
// surface in this corrective batch).
core::Result<ApplyOutcome> apply_default_option_effects(
    core::GameState&             state,
    const core::EventInstance&   instance,
    const core::EventDefinition& definition);

// RCR-1: RFC-090 §5.12 — followup-event-id resolver. Given a
// definition's `followup_event_ids` vector, returns the matching
// `state.events` indices for every id_code that resolves
// successfully. Unresolvable id_codes are skipped (the M5.4-era
// "EventInstance.event_id_code is NOT cross-checked against
// state.events on load" tolerance applies here too — followup
// chains across scenario reloads are legitimate).
//
// Returns a vector of size <= `definition.followup_event_ids.size()`;
// the result preserves the order of `definition.followup_event_ids`.
// Empty input → empty output. Pure read.
//
// Scope split between resolver + firer:
//   - `resolve_followup_ids` (THIS function) only resolves
//     followup id_code strings to `state.events` indices.
//   - `event_firer::record_followup` is the matching firing
//     primitive: takes one resolved followup definition + the
//     parent instance + a date, and appends one EventInstance
//     to event_history plus one `event_fired` LogEntry to
//     state.logs (RCR-1 deterministic chain firing).
//   - Automatic recursive runner cascade (looping
//     record_followup over resolved chains every tick) remains
//     out of scope for RCR-1 — wiring it into
//     `event_engine::tick_events` would change M5.7
//     snapshot-evaluation semantics and is intentionally
//     deferred.
std::vector<std::size_t>
resolve_followup_ids(const core::GameState&       state,
                     const core::EventDefinition& definition);

}  // namespace leviathan::systems::event_effects

#endif  // LEVIATHAN_SYSTEMS_EVENT_EFFECTS_HPP

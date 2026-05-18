// EventEngine - one-round composition of the M5.2 evaluator,
// M5.5 firer, and M5.6 effects applicator.
//
// M5.7 ships the runner-integration *skeleton*: a single free
// function `tick_events(state)` that composes the three M5
// surfaces into a usable "evaluate -> record -> apply" round.
// The caller decides WHEN to invoke it (per-day, per-month,
// player-trigger only, etc.); M5.7 itself does NOT wire into
// any pipeline yet.
//
// Deliberate scope split (matches M1's M1.9 -> M1.10 pacing
// where M1.9 shipped the monthly composition function and M1.10
// wired it into the runner): M5.7 ships the composition; a
// future M5.x will wire it into the runner / monthly pipeline.
// Splitting these surfaces keeps each PR a single decoupled
// change, and lets the M5.7 PR avoid touching M1.17's
// byte-identical determinism baselines — those would shift the
// moment `tick_events` ran inside `tick_all_countries`, and
// rebaking them is its own surface change.
//
// One round of `tick_events`:
//
//   1. matches = event_evaluator::match_events(state)
//        - read-only; never mutates state
//        - returns vector<EventMatch> with M5.3 actor binding
//   2. for each match m in matches (in canonical order):
//        a. event_firer::record_match(state, m, state.current_date)
//             - appends one EventInstance to state.event_history
//             - fired_on = state.current_date (the ONLY place
//               `tick_events` reads state.current_date; the
//               firer itself is still date-neutral per M5.5)
//        b. instance = state.event_history.back()
//        c. apply_event_effects(state, instance, state.events[m.event_index])
//             - applies the matched EventDefinition's effects
//               via the M5.6 / M1.5 shared helper
//             - on failure, `tick_events` bails out and returns
//               Result::failure (caller sees partial state:
//               event_history has entries for matches [0..i],
//               but only [0..i-1] had their effects applied)
//
// Caller responsibilities (M5.7-era contract):
//
//   * Idempotency / dedup is the caller's call. Two consecutive
//     calls to `tick_events` on the same state fire the same
//     events twice. Cooldown / historical-once gating belongs
//     to the future M5.x runner-integration PR that decides
//     WHEN to call this — not to this composition brick.
//   * Selection policy is M5.6's "first actor wins" (no
//     all-actors / weighted / per-effect actor scoping). Don't
//     change that here; it belongs in its own dedicated
//     selection-policy sub-milestone per PR #92 review.
//   * No `state.logs` append, no `events.jsonl` emission, no
//     new artefact, no new RunnerOptions / CLI flag. M5.7 is
//     the composition brick only; the future runner-integration
//     PR adds those.
//
// What M5.7 explicitly does NOT do (for symmetry with the other
// M5 notes):
//
//   no auto-wire into runner / monthly pipeline
//   no events.jsonl change
//   no log entry on tick (no state.logs append)
//   no state.applied_commands append
//   no new artefact (still 10)
//   no save format bump (still v14)
//   no new RunnerOptions field / CLI flag
//   no new PlayerCommandKind
//   no new state field
//   no cooldown / historical-once gating (caller policy)
//   no selection-policy variants (M5.6 first-actor-wins stays)
//   no chained events / choices / RNG outcomes
//   no broader trigger ops / targets / actor kinds
//   no balance pass
//   no event author tooling
//   no UI surface
//   no changes to event_evaluator / event_firer /
//     event_effects / policy_system module APIs
//   no changes to scenario_loader / canonical fixtures
//   no docs/milestone-5-checkpoint.md (still deferred)

#ifndef LEVIATHAN_SYSTEMS_EVENT_ENGINE_HPP
#define LEVIATHAN_SYSTEMS_EVENT_ENGINE_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::event_engine {

// One tick_events call's summary. Counts include only events
// that finished successfully — on a mid-round failure, the
// counts reflect the (matched, recorded, applied) progress at
// the time of failure.
struct TickOutcome {
    int events_matched        = 0;  // == match_events(state).size()
    int events_recorded       = 0;  // appended to state.event_history
    int events_applied        = 0;  // effects-applicator succeeded
    int total_effects_applied = 0;  // sum of per-event effects_applied
};

// One round of event-engine processing on `state`. See header
// doc for the per-match semantics, failure mode, and caller
// responsibilities.
//
// fired_on = state.current_date for every recorded instance in
// this round.
//
// Returns Result::failure if any matched event's
// `apply_event_effects` call fails. State at failure: matches
// [0..failed_index] are recorded in event_history, but only
// matches [0..failed_index-1] have had their effects applied.
// The caller (a future M5.x runner-integration PR or a test)
// is responsible for deciding whether to roll forward or back.
core::Result<TickOutcome> tick_events(core::GameState& state);

}  // namespace leviathan::systems::event_engine

#endif  // LEVIATHAN_SYSTEMS_EVENT_ENGINE_HPP
